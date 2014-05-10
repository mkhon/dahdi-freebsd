/*
 * Dynamic Span Interface for DAHDI (Multi-Span Ethernet Interface)
 *
 * Written by Joseph Benden <joe@thrallingpenguin.com>
 *
 * Copyright (C) 2007-2010, Thralling Penguin LLC.
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#if !defined(__FreeBSD__)
#include <linux/notifier.h>
#endif /* !__FreeBSD__ */
#include <linux/crc32.h>

/**
 * Undefine USE_PROC_FS, if you do not want the /proc/dahdi/dynamic-ethmf
 * support. Undefining this would give a slight performance increase.
 */
#undef USE_PROC_FS

#ifdef USE_PROC_FS
# include <linux/proc_fs.h>
# include <asm/atomic.h>
#endif

#ifdef CONFIG_DEVFS_FS
# include <linux/devfs_fs_kernel.h>
#endif

#include <dahdi/kernel.h>
#include <dahdi/user.h>

#if defined(__FreeBSD__)
#include "ng_dahdi_netdev.h"
#endif

#define ETH_P_ZTDETH			0xd00d
#define ETHMF_MAX_PER_SPAN_GROUP	8
#define ETHMF_MAX_GROUPS		16
#define ETHMF_FLAG_IGNORE_CHAN0	(1 << 3)
#define ETHMF_MAX_SPANS			4
#define ETHMF_MAX_CHANNELS		31

struct ztdeth_header {
	unsigned short subaddr;
};

/* Timer for enabling spans - used to combat a lock problem */
static struct timer_list timer;

/* Whether or not the timer has been deleted */
static atomic_t timer_deleted = ATOMIC_INIT(0);

#ifdef USE_PROC_FS
/* Global error counter */
static atomic_t errcount = ATOMIC_INIT(0);
#endif

/* Whether or not we are in shutdown */
static atomic_t shutdown = ATOMIC_INIT(0);

#if defined(__FreeBSD__)
static char g_padding[ETHMF_MAX_CHANNELS * 8];
static char g_padding_ignore[8];
#else
static struct sk_buff_head skbs;
#endif

#ifdef USE_PROC_FS
struct ethmf_group {
	unsigned int hash_addr;
	atomic_t spans;
	atomic_t rxframecount;
	atomic_t txframecount;
	atomic_t rxbytecount;
	atomic_t txbytecount;
	atomic_t devupcount;
	atomic_t devdowncount;
};
static struct ethmf_group ethmf_groups[ETHMF_MAX_GROUPS];
#endif

struct ztdeth {
	/* Destination MAC address */
	unsigned char addr[ETH_ALEN];
	/* Destination MAC address hash value */
	unsigned int addr_hash;
	/* span sub-address, in network byte order */
	unsigned short subaddr;
	/* DAHDI span associated with this TDMoE-mf span */
	struct dahdi_span *span;
	/* Ethernet interface name */
	char ethdev[IFNAMSIZ];
	/* Ethernet device reference */
	struct net_device *dev;
	/* trx buffer */
	unsigned char *msgbuf;
	/* trx buffer length */
	int msgbuf_len;
	/* wether or not this frame is ready for trx */
	atomic_t ready;
	/* delay counter, to ensure all spans are added, prior to usage */
	atomic_t delay;
	/* rvc buffer */
	unsigned char *rcvbuf;
	/* the number of channels in this span */
	int real_channels;
	/* use padding if 1, else no padding */
	atomic_t no_front_padding;
	/* counter to pseudo lock the rcvbuf */
	atomic_t refcnt;

	struct list_head list;
};

/**
 * Lock for adding and removing items in ethmf_list
 */
static DEFINE_SPINLOCK(ethmf_lock);

#if defined(__FreeBSD__)
static inline void
rcu_read_lock(void)
{
	spin_lock(&ethmf_lock);
}

static inline void
rcu_read_unlock(void)
{
	spin_unlock(&ethmf_lock);
}

#define synchronize_rcu()

#define list_add_rcu(e, l)			list_add(e, l)
#define list_del_rcu(e)				list_del(e)
#define _list_temp(e)				__CONCAT(e##_tmp_, __LINE__)
#define list_for_each_entry_rcu(e, l, elem)	__typeof__(e) _list_temp(e); list_for_each_entry_safe(e, _list_temp(e), l, elem)
#endif /* __FreeBSD__ */

/**
 * The active list of all running spans
 */
static _LIST_HEAD(ethmf_list);

static inline void ethmf_errors_inc(void)
{
#ifdef USE_PROC_FS
	atomic_inc(&errcount);
#endif
}

#ifdef USE_PROC_FS
static inline int hashaddr_to_index(unsigned int hash_addr)
{
	int i, z = -1;
	for (i = 0; i < ETHMF_MAX_GROUPS; ++i) {
		if (z == -1 && ethmf_groups[i].hash_addr == 0)
			z = i;
		if (ethmf_groups[i].hash_addr == hash_addr)
			return i;
	}
	if (z != -1) {
		ethmf_groups[z].hash_addr = hash_addr;
	}
	return z;
}
#endif

/**
 * Find the Ztdeth Struct and DAHDI span for a given MAC address and subaddr.
 *
 * NOTE: RCU read lock must already be held.
 */
static inline void find_ethmf(const unsigned char *addr,
	const unsigned short subaddr, struct ztdeth **ze,
	struct dahdi_span **span)
{
	struct ztdeth *z;

	list_for_each_entry_rcu(z, &ethmf_list, list) {
		if (!atomic_read(&z->delay)) {
			if (!memcmp(addr, z->addr, ETH_ALEN)
					&& z->subaddr == subaddr) {
				*ze = z;
				*span = z->span;
				return;
			}
		}
	}

	/* no results */
	*ze = NULL;
	*span = NULL;
}

/**
 * Determines if all spans are ready for transmit. If all spans are ready,
 * we return the number of spans which indeed are ready and populate the
 * array of pointers to those spans..
 *
 * NOTE: RCU read lock must already be held.
 */
static inline int ethmf_trx_spans_ready(unsigned int addr_hash, struct ztdeth *(*ready_spans)[ETHMF_MAX_PER_SPAN_GROUP])
{
	struct ztdeth *t;
	int span_count = 0, spans_ready = 0;

	list_for_each_entry_rcu(t, &ethmf_list, list) {
		if (!atomic_read(&t->delay) && t->addr_hash == addr_hash) {
			++span_count;
			if (atomic_read(&t->ready)) {
				short subaddr = ntohs(t->subaddr);
				if (subaddr < ETHMF_MAX_PER_SPAN_GROUP) {
					(*ready_spans)[subaddr] = t;
					++spans_ready;
				} else {
					printk(KERN_ERR "More than %d spans per multi-frame group are not currently supported.",
						ETHMF_MAX_PER_SPAN_GROUP);
				}
			}
		}
	}

	if (span_count && spans_ready && span_count == spans_ready) {
		return spans_ready;
	}
	return 0;
}

/**
 * Ethernet receiving side processing function.
 */
#if defined(__FreeBSD__)
static int ztdethmf_rcv(struct net_device *dev, struct ether_header *eh,
			unsigned char *msg, int msglen)
#else
static int ztdethmf_rcv(struct sk_buff *skb, struct net_device *dev,
		struct packet_type *pt, struct net_device *orig_dev)
#endif /* !__FreeBSD__ */
{
	int num_spans = 0, span_index = 0;
	unsigned char *src_addr;
	unsigned char *data;
	struct dahdi_span *span;
	struct ztdeth *z = NULL;
	struct ztdeth_header *zh;
	unsigned int samples, channels, rbslen, flags;
	unsigned int skip = 0;

#if defined(__FreeBSD__)
	if (msglen < sizeof(*zh))
		return 0;
	zh = (struct ztdeth_header *) msg;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
	zh = (struct ztdeth_header *) skb_network_header(skb);
#else
	zh = (struct ztdeth_header *) skb->nh.raw;
#endif /* !__FreeBSD__ */
	if (ntohs(zh->subaddr) & 0x8000) {
		/* got a multi-span frame */
		num_spans = ntohs(zh->subaddr) & 0xFF;

		/* Currently max of 4 spans supported */
		if (unlikely(num_spans > ETHMF_MAX_SPANS)) {
			goto out;
		}

#if defined(__FreeBSD__)
		data = msg + sizeof(*zh);
		src_addr = eh->ether_shost;
#else /* !__FreeBSD__ */
		skb_pull(skb, sizeof(struct ztdeth_header));
#ifdef NEW_SKB_LINEARIZE
		if (skb_is_nonlinear(skb))
			skb_linearize(skb);
#else
		if (skb_is_nonlinear(skb))
			skb_linearize(skb, GFP_KERNEL);
#endif
		data = (unsigned char *) skb->data;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
		src_addr = eth_hdr(skb)->h_source;
#else
		src_addr = skb->mac.ethernet->h_source;
#endif
#endif /* !__FreeBSD__ */

		rcu_read_lock();
		do {
			find_ethmf(src_addr, htons(span_index), &z, &span);
			if (unlikely(!z || !span)) {
				/* The recv'd span does not belong to us */
				/* ethmf_errors_inc(); */
				++span_index;
				continue;
			}

			samples = data[(span_index * 6)] & 0xFF;
			flags = data[((span_index * 6) + 1)] & 0xFF;
			channels = data[((span_index * 6) + 5)] & 0xFF;
			rbslen = ((channels + 3) / 4) * 2;

			if (unlikely(samples != 8 || channels > ETHMF_MAX_CHANNELS || channels == 0)) {
				ethmf_errors_inc();
				++span_index;
				continue;
			}

			if (atomic_dec_and_test(&z->refcnt) == 0) {
				memcpy(z->rcvbuf, data + 6*span_index, 6); /* TDM Header */
				/*
				 * If we ignore channel zero we must skip the first eight bytes and
				 * ensure that ztdynamic doesn't get confused by this new flag
				 */
				if (flags & ETHMF_FLAG_IGNORE_CHAN0) {
					skip = 8;

					/* Remove this flag since ztdynamic may not understand it */
					z->rcvbuf[1] = flags & ~(ETHMF_FLAG_IGNORE_CHAN0);

					/* Additionally, now we will transmit with front padding */
					atomic_set(&z->no_front_padding, 0);
				} else {
					/* Disable front padding if we recv'd a packet without it */
					atomic_set(&z->no_front_padding, 1);
				}
				memcpy(z->rcvbuf + 6, data + 6*num_spans + 16
					*span_index, rbslen); /* RBS Header */

				/* 256 == 32*8; if padding lengths change, this must be modified */
				memcpy(z->rcvbuf + 6 + rbslen, data + 6*num_spans + 16
					*num_spans + (256)*span_index + skip, channels
					* 8); /* Payload */

				dahdi_dynamic_receive(span, z->rcvbuf, 6 + rbslen
					+ channels*8);
			} else {
				ethmf_errors_inc();
				printk(KERN_INFO "TDMoE span overflow detected. Span %d was dropped.", span_index);
			}
			atomic_inc(&z->refcnt);

#ifdef USE_PROC_FS
			if (span_index == 0) {
				atomic_inc(&(ethmf_groups[hashaddr_to_index(z->addr_hash)].rxframecount));
				atomic_add(skb->len + z->dev->hard_header_len +
					sizeof(struct ztdeth_header),
					&(ethmf_groups[hashaddr_to_index(z->addr_hash)].rxbytecount));
			}
#endif
			++span_index;
		} while (!atomic_read(&shutdown) && span_index < num_spans);
		rcu_read_unlock();
	}

out:
#if !defined(__FreeBSD__)
	kfree_skb(skb);
#endif
	return 0;
}

static int ztdethmf_notifier(struct notifier_block *block, unsigned long event,
		void *ptr)
{
	struct net_device *dev = ptr;
	struct ztdeth *z;

	switch (event) {
	case NETDEV_GOING_DOWN:
	case NETDEV_DOWN:
		rcu_read_lock();
		list_for_each_entry_rcu(z, &ethmf_list, list) {
			/* Note that the device no longer exists */
			if (z->dev == dev) {
				z->dev = NULL;
#ifdef USE_PROC_FS
				atomic_inc(&(ethmf_groups[hashaddr_to_index(z->addr_hash)].devdowncount));
#endif
			}
		}
		rcu_read_unlock();
		break;
	case NETDEV_UP:
		rcu_read_lock();
		list_for_each_entry_rcu(z, &ethmf_list, list) {
			/* Now that the device exists again, use it */
			if (!strcmp(z->ethdev, dev->name)) {
				z->dev = dev;
#ifdef USE_PROC_FS
				atomic_inc(&(ethmf_groups[hashaddr_to_index(z->addr_hash)].devupcount));
#endif
			}
		}
		rcu_read_unlock();
		break;
	}
	return 0;
}

static void ztdethmf_transmit(struct dahdi_dynamic *dyn, u8 *msg, size_t msglen)
{
	struct ztdeth *z = dyn->pvt, *ready_spans[ETHMF_MAX_PER_SPAN_GROUP];
#if defined(__FreeBSD__)
	struct mbuf *m;
	struct ether_header eh;
	struct ztdeth_header zh;
#else
	struct sk_buff *skb;
	struct ztdeth_header *zh;
	unsigned char addr[ETH_ALEN];
#endif
	unsigned short subaddr;
	struct net_device *dev;
	int spans_ready = 0, index = 0;

	if (atomic_read(&shutdown))
		return;

	rcu_read_lock();

	if (unlikely(!z || !z->dev)) {
		rcu_read_unlock();
		return;
	}

#if defined(__FreeBSD__)
	if (atomic_cmpset_int(&z->ready, 0, 1)) {
		memcpy(z->msgbuf, msg, msglen);
		z->msgbuf_len = msglen;
	}
#else /* !__FreeBSD__ */
	if (!atomic_read(&z->ready)) {
		if (atomic_inc_return(&z->ready) == 1) {
			memcpy(z->msgbuf, msg, msglen);
			z->msgbuf_len = msglen;
		}
	}
#endif /* !__FreeBSD__ */

	spans_ready = ethmf_trx_spans_ready(z->addr_hash, &ready_spans);
	if (spans_ready) {
		int pad[ETHMF_MAX_SPANS], rbs[ETHMF_MAX_SPANS];

		dev = z->dev;
#if defined(__FreeBSD__)
		bcopy(z->addr, &eh.ether_dhost, sizeof(eh.ether_dhost));
#else /* !__FreeBSD__ */
		memcpy(addr, z->addr, sizeof(z->addr));
#endif /* !__FreeBSD__ */

		for (index = 0; index < spans_ready; index++) {
			int chan = ready_spans[index]->real_channels;
			/* By default we pad to 32 channels, but if
			 * no_front_padding is false then we have a pad
			 * in the front of 8 bytes, so this implies one
			 * less channel
			 */
			if (atomic_read(&(ready_spans[index]->no_front_padding)))
				pad[index] = (32 - chan)*8;
			else
				pad[index] = (ETHMF_MAX_CHANNELS - chan)*8;

			rbs[index] = ((chan + 3) / 4) * 2;
		}

#if defined(__FreeBSD__)
		MGETHDR(m, M_NOWAIT, MT_DATA);
		if (m == NULL) {
			rcu_read_unlock();
			ethmf_errors_inc();
			return;
		}
		MCLGET(m, M_NOWAIT);

		/* copy ethernet header and reserve space for ztdeth header */
		bcopy(dev->dev_addr, &eh.ether_shost, sizeof(eh.ether_shost));
		eh.ether_type = __constant_htons(ETH_P_ZTDETH);
		m_copyback(m, 0, sizeof(eh), (caddr_t) &eh);
		m->m_pkthdr.len = m->m_len = sizeof(eh) + sizeof(zh);
#else /* !__FreeBSD__ */
		/* Allocate the standard size for a 32-chan frame */
		skb = dev_alloc_skb(1112 + dev->hard_header_len
			+ sizeof(struct ztdeth_header) + 32);
		if (unlikely(!skb)) {
			rcu_read_unlock();
			ethmf_errors_inc();
			return;
		}

		/* Reserve header space */
		skb_reserve(skb, dev->hard_header_len
				+ sizeof(struct ztdeth_header));
#endif /* !__FreeBSD__ */

		/* copy each spans header */
		for (index = 0; index < spans_ready; index++) {
			if (!atomic_read(&(ready_spans[index]->no_front_padding))) {
				ready_spans[index]->msgbuf[1]
					|= ETHMF_FLAG_IGNORE_CHAN0;
			}
#if defined(__FreeBSD__)
			m_append(m, 6, ready_spans[index]->msgbuf);
#else
			memcpy(skb_put(skb, 6), ready_spans[index]->msgbuf, 6);
#endif
		}

		/* copy each spans RBS payload */
		for (index = 0; index < spans_ready; index++) {
#if defined(__FreeBSD__)
			m_append(m, 16, ready_spans[index]->msgbuf + 6);
#else
			memcpy(skb_put(skb, 16), ready_spans[index]->msgbuf + 6,
				rbs[index]);
#endif
		}

		/* copy each spans data/voice payload */
		for (index = 0; index < spans_ready; index++) {
			int chan = ready_spans[index]->real_channels;
			if (!atomic_read(&(ready_spans[index]->no_front_padding))) {
				/* This adds an additional (padded) channel to our total */
#if defined(__FreeBSD__)
				m_append(m, sizeof(g_padding_ignore), g_padding_ignore);
#else
				memset(skb_put(skb, 8), 0xA5, 8); /* ETHMF_IGNORE_CHAN0 */
#endif
			}
#if defined(__FreeBSD__)
			m_append(m, chan * 8, ready_spans[index]->msgbuf + (6 + rbs[index]));
#else
			memcpy(skb_put(skb, chan*8), ready_spans[index]->msgbuf
					+ (6 + rbs[index]), chan*8);
#endif
			if (pad[index] > 0) {
#if defined(__FreeBSD__)
				m_append(m, pad[index], g_padding);
#else
				memset(skb_put(skb, pad[index]), 0xDD, pad[index]);
#endif
			}

			/* mark span as ready for new data/voice */
			atomic_set(&(ready_spans[index]->ready), 0);
		}

		subaddr = htons((unsigned short)(0x8000 | (unsigned char)(spans_ready & 0xFF)));
#if defined(__FreeBSD__)
		/* copy ztdeth header */
		zh.subaddr = subaddr;
		m_copyback(m, sizeof(eh), sizeof(zh), (caddr_t) &zh);

		/* send raw ethernet frame */
		dev_xmit(dev, m);
#else /* !__FreeBSD__ */
		/* Throw on header */
		zh = (struct ztdeth_header *)skb_push(skb,
				sizeof(struct ztdeth_header));
		zh->subaddr = subaddr;

		/* Setup protocol type */
		skb->protocol = __constant_htons(ETH_P_ZTDETH);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
		skb_set_network_header(skb, 0);
#else
		skb->nh.raw = skb->data;
#endif
		skb->dev = dev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
		dev_hard_header(skb, dev, ETH_P_ZTDETH, addr, dev->dev_addr, skb->len);
#else
		if (dev->hard_header)
			dev->hard_header(skb, dev, ETH_P_ZTDETH, addr,
					dev->dev_addr, skb->len);
#endif
		/* queue frame for delivery */
		if (dev) {
			skb_queue_tail(&skbs, skb);
		}
#endif /* !__FreeBSD__ */
#ifdef USE_PROC_FS
		atomic_inc(&(ethmf_groups[hashaddr_to_index(z->addr_hash)].txframecount));
		atomic_add(skb->len, &(ethmf_groups[hashaddr_to_index(z->addr_hash)].txbytecount));
#endif
	}

	rcu_read_unlock();

	return;
}

static int ztdethmf_flush(void)
{
#if !defined(__FreeBSD__)
	struct sk_buff *skb;

	/* Handle all transmissions now */
	while ((skb = skb_dequeue(&skbs))) {
		dev_queue_xmit(skb);
	}
#endif /* !__FreeBSD__ */
	return 0;
}

static struct packet_type ztdethmf_ptype = {
	.type = __constant_htons(ETH_P_ZTDETH),	/* Protocol */
	.dev  = NULL,				/* Device (NULL = wildcard) */
	.func = ztdethmf_rcv,			/* Receiver */
};

static void ztdethmf_destroy(struct dahdi_dynamic *dyn)
{
	struct ztdeth *z = dyn->pvt;
	unsigned long flags;

	atomic_set(&shutdown, 1);
	synchronize_rcu();

	spin_lock_irqsave(&ethmf_lock, flags);
	list_del_rcu(&z->list);
	spin_unlock_irqrestore(&ethmf_lock, flags);
	synchronize_rcu();
#if defined(USE_PROC_FS)
	atomic_dec(&(ethmf_groups[hashaddr_to_index(z->addr_hash)].spans));
#endif

	if (z) { /* Successfully removed */
		printk(KERN_INFO "Removed interface for %s\n",
			z->span->name);
		kfree(z->msgbuf);
		kfree(z);
	} else {
		if (z && z->span && z->span->name) {
			printk(KERN_ERR "Cannot find interface for %s\n",
				z->span->name);
		}
	}
}

static int ztdethmf_create(struct dahdi_dynamic *dyn, const char *addr)
{
	struct ztdeth *z;
	char src[256];
	char *src_ptr;
	int x, bufsize, num_matched;
	unsigned long flags;
	struct dahdi_span *const span = &dyn->span;

	BUG_ON(!span);
	BUG_ON(!addr);

	if (span->channels > ETHMF_MAX_CHANNELS) {
		printk(KERN_ERR "span %s, %d channels, but max %d channels supported\n",
		    span->name, span->channels, ETHMF_MAX_CHANNELS);
	}
	z = kmalloc(sizeof(struct ztdeth), GFP_KERNEL);
	if (!z)
		return -ENOMEM;

	/* Zero it out */
	memset(z, 0, sizeof(struct ztdeth));

	/* set a delay for xmit/recv to workaround Zaptel problems */
	atomic_set(&z->delay, 4);

	/* create a msg buffer. MAX OF 31 CHANNELS!!!! */
	bufsize = ETHMF_MAX_CHANNELS * DAHDI_CHUNKSIZE + ETHMF_MAX_CHANNELS / 4 + 48;
	z->msgbuf = kmalloc(bufsize, GFP_KERNEL);
	z->rcvbuf = kmalloc(bufsize, GFP_KERNEL);

	/* Address should be <dev>/<macaddr>/subaddr */
	strlcpy(src, addr, sizeof(src));
	/* replace all / with space; otherwise kernel sscanf does not work */
	src_ptr = src;
	while (*src_ptr) {
		if (*src_ptr == '/')
			*src_ptr = ' ';
		++src_ptr;
	}
	num_matched = sscanf(src,
			"%16s %hhx:%hhx:%hhx:%hhx:%hhx:%hhx %hu",
			z->ethdev, &z->addr[0], &z->addr[1],
			&z->addr[2], &z->addr[3], &z->addr[4],
			&z->addr[5], &z->subaddr);
	if (8 != num_matched) {
		printk(KERN_ERR "Only matched %d entries in '%s'\n", num_matched, src);
		printk(KERN_ERR "Invalid TDMoE Multiframe address: %s\n", addr);
		kfree(z);
		return -EINVAL;
	}
	z->dev = dev_get_by_name(
#if !defined(__FreeBSD__) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
		&init_net,
#endif
		z->ethdev);
	if (!z->dev) {
		printk(KERN_ERR "TDMoE Multiframe: Invalid device '%s'\n", z->ethdev);
		kfree(z);
		return -EINVAL;
	}
	z->span = span;
	z->subaddr = htons(z->subaddr);
	z->addr_hash = crc32_le(0, z->addr, ETH_ALEN);
	z->real_channels = span->channels;

	src[0] = '\0';
	for (x = 0; x < 5; x++)
		sprintf(src + strlen(src), "%02x:", z->dev->dev_addr[x]);
	sprintf(src + strlen(src), "%02x", z->dev->dev_addr[5]);

	printk(KERN_INFO "TDMoEmf: Added new interface for %s at %s "
		"(addr=%s, src=%s, subaddr=%d)\n", span->name, z->dev->name,
		addr, src, ntohs(z->subaddr));

	atomic_set(&z->ready, 0);
	atomic_set(&z->refcnt, 0);

	spin_lock_irqsave(&ethmf_lock, flags);
	list_add_rcu(&z->list, &ethmf_list);
	spin_unlock_irqrestore(&ethmf_lock, flags);
#if defined(USE_PROC_FS)
	atomic_inc(&(ethmf_groups[hashaddr_to_index(z->addr_hash)].spans));
#endif

	/* enable the timer for enabling the spans */
	mod_timer(&timer, jiffies + HZ);
	atomic_set(&shutdown, 0);
	dyn->pvt = z;
	return 0;
}

static struct dahdi_dynamic_driver ztd_ethmf = {
	.owner = THIS_MODULE,
	.name = "ethmf",
	.desc = "Ethernet",
	.create = ztdethmf_create,
	.destroy = ztdethmf_destroy,
	.transmit = ztdethmf_transmit,
	.flush = ztdethmf_flush,
};

static struct notifier_block ztdethmf_nblock = {
	.notifier_call = ztdethmf_notifier,
};

/**
 * Decrements each delay counter in the ethmf_list and returns the number of
 * delay counters that are not equal to zero.
 */
static int ethmf_delay_dec(void)
{
	struct ztdeth *z;
	int count_nonzero = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(z, &ethmf_list, list) {
		if (atomic_read(&z->delay)) {
			atomic_dec(&z->delay);
			++count_nonzero;
		} else
			atomic_set(&z->delay, 0);
	}
	rcu_read_unlock();
	return count_nonzero;
}

/**
 * Timer callback function to allow all spans to be added, prior to any of
 * them being used.
 */
static void timer_callback(unsigned long param)
{
	if (ethmf_delay_dec()) {
		if (!atomic_read(&timer_deleted)) {
			timer.expires = jiffies + HZ;
			add_timer(&timer);
		}
	} else {
		printk(KERN_INFO "All TDMoE multiframe span groups are active.\n");
	}
}

#ifdef USE_PROC_FS
static struct proc_dir_entry *proc_entry;
static const char *ztdethmf_procname = "dahdi/dynamic-ethmf";
static int ztdethmf_proc_read(char *page, char **start, off_t off, int count,
		int *eof, void *data)
{
	struct ztdeth *z = NULL;
	int len = 0, i = 0;
	unsigned int group = 0, c = 0;

	rcu_read_lock();

	len += sprintf(page + len, "Errors: %d\n\n", atomic_read(&errcount));

	for (group = 0; group < ETHMF_MAX_GROUPS; ++group) {
		if (atomic_read(&(ethmf_groups[group].spans))) {
			len += sprintf(page + len, "Group #%d (0x%x)\n", i++, ethmf_groups[group].hash_addr);
			len += sprintf(page + len, "  Spans: %d\n",
				atomic_read(&(ethmf_groups[group].spans)));

			c = 1;
			list_for_each_entry_rcu(z, &ethmf_list, list) {
				if (z->addr_hash == ethmf_groups[group].hash_addr) {
					if (c == 1) {
						len += sprintf(page + len,
							"  Device: %s (MAC: %02x:%02x:%02x:%02x:%02x:%02x)\n",
							z->ethdev,
							z->addr[0], z->addr[1], z->addr[2],
							z->addr[3], z->addr[4], z->addr[5]);
					}
					len += sprintf(page + len, "    Span %d: subaddr=%u ready=%d delay=%d real_channels=%d no_front_padding=%d\n",
						c++, ntohs(z->subaddr),
						atomic_read(&z->ready), atomic_read(&z->delay),
						z->real_channels, atomic_read(&z->no_front_padding));
				}
			}
			len += sprintf(page + len, "  Device UPs: %u\n",
				atomic_read(&(ethmf_groups[group].devupcount)));
			len += sprintf(page + len, "  Device DOWNs: %u\n",
				atomic_read(&(ethmf_groups[group].devdowncount)));
			len += sprintf(page + len, "  Rx Frames: %u\n",
				atomic_read(&(ethmf_groups[group].rxframecount)));
			len += sprintf(page + len, "  Tx Frames: %u\n",
				atomic_read(&(ethmf_groups[group].txframecount)));
			len += sprintf(page + len, "  Rx Bytes: %u\n",
				atomic_read(&(ethmf_groups[group].rxbytecount)));
			len += sprintf(page + len, "  Tx Bytes: %u\n",
				atomic_read(&(ethmf_groups[group].txbytecount)));
			if (len <= off) {
				off -= len;
				len = 0;
			}
			if (len > off+count)
				break;
		}
	}
	rcu_read_unlock();

	if (len <= off) {
		off -= len;
		len = 0;
	}
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	return len;
}
#endif

static int __init ztdethmf_init(void)
{
	init_timer(&timer);
	timer.expires = jiffies + HZ;
	timer.function = &timer_callback;
	add_timer(&timer);

	dev_add_pack(&ztdethmf_ptype);
	register_netdevice_notifier(&ztdethmf_nblock);
	dahdi_dynamic_register_driver(&ztd_ethmf);

#if defined(__FreeBSD__)
	memset(g_padding, 0xdd, sizeof(g_padding));
	memset(g_padding_ignore, 0xa5, sizeof(g_padding_ignore));
#else
	skb_queue_head_init(&skbs);
#endif

#ifdef USE_PROC_FS
	proc_entry = create_proc_read_entry(ztdethmf_procname, 0444, NULL,
		ztdethmf_proc_read, NULL);
	if (!proc_entry) {
		printk(KERN_ALERT "create_proc_read_entry failed.\n");
	}
#endif

	return 0;
}

static void __exit ztdethmf_exit(void)
{
	atomic_set(&timer_deleted, 1);
	del_timer_sync(&timer);

	dev_remove_pack(&ztdethmf_ptype);
	unregister_netdevice_notifier(&ztdethmf_nblock);
	dahdi_dynamic_unregister_driver(&ztd_ethmf);

#ifdef USE_PROC_FS
	if (proc_entry)
		remove_proc_entry(ztdethmf_procname, NULL);
#endif
}

#if defined(__FreeBSD__)
LINUX_DEV_MODULE(dahdi_dynamic_ethmf);
MODULE_VERSION(dahdi_dynamic_ethmf, 1);
MODULE_DEPEND(dahdi_dynamic_ethmf, dahdi, 1, 1, 1);
MODULE_DEPEND(dahdi_dynamic_ethmf, dahdi_dynamic, 1, 1, 1);
MODULE_DEPEND(dahdi_dynamic_ethmf, ng_dahdi_netdev, 1, 1, 1);
#endif /* __FreeBSD__ */

MODULE_DESCRIPTION("DAHDI Dynamic TDMoEmf Support");
MODULE_AUTHOR("Joseph Benden <joe@thrallingpenguin.com>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

module_init(ztdethmf_init);
module_exit(ztdethmf_exit);

/*
 * Copyright (c) 2010 Max Khon <fjoe@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#ifndef _NG_DAHDI_NETDEV_H_
#define _NG_DAHDI_NETDEV_H_

#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/ethernet.h>

#include <linux/netdevice.h>

/**
 * Notifier network device events
 */
#define NETDEV_UP		0x0001
#define NETDEV_DOWN		0x0002
#define NETDEV_GOING_DOWN	0x0009

/**
 * Notifier descriptor
 */
struct notifier_block {
	int (*notifier_call)(struct notifier_block *block, unsigned long event, void *ptr);

	SLIST_ENTRY(notifier_block) next;
};

/**
 * Register notifier
 */
int register_netdevice_notifier(struct notifier_block *block);

/**
 * Unregister notifier
 */
int unregister_netdevice_notifier(struct notifier_block *block);

/**
 * Packet type descriptor
 */
struct packet_type {
	uint16_t type;			// ethernet packet type
	void *dev;			// unused
	int (*func)(struct net_device *netdev, struct ether_header *eh,
		    unsigned char *msg, int msglen);

	SLIST_ENTRY(packet_type) next;
};

/**
 * Add packet type handler
 */
void dev_add_pack(struct packet_type *ptype);

/**
 * Remove packet type handler
 */
void dev_remove_pack(struct packet_type *ptype);

/**
 * Get network device by name
 */
struct net_device *dev_get_by_name(const char *devname);

/**
 * Release network device
 */
void dev_put(struct net_device *netdev);

/**
 * Transmit raw ethernet frame
 *
 * Takes ownership of passed mbuf.
 */
void dev_xmit(struct net_device *netdev, struct mbuf *m);

#endif /* _NG_DAHDI_NETDEV_H_ */

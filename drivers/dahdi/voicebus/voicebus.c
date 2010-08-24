/*
 * VoiceBus(tm) Interface Library.
 *
 * Written by Shaun Ruffell <sruffell@digium.com>
 * and based on previous work by Mark Spencer <markster@digium.com>,
 * Matthew Fredrickson <creslin@digium.com>, and
 * Michael Spiceland <mspiceland@digium.com>
 *
 * Copyright (C) 2007-2010 Digium, Inc.
 *
 * All rights reserved.

 * VoiceBus is a registered trademark of Digium.
 *
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/libkern.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/resource.h>

#ifdef wmb
#undef wmb
#endif
#define wmb()
#else /* !__FreeBSD__ */
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/sched.h>
#endif /* !__FreeBSD__ */

#include <dahdi/kernel.h>
#include "voicebus.h"
#include "voicebus_net.h"
#include "vpmadtreg.h"
#include "GpakCust.h"

#if 0 && VOICEBUS_DEFERRED == TIMER
#if HZ < 1000
/* \todo Put an error message here. */
#endif
#endif

/* Interrupt status' reported in SR_CSR5 */
#define TX_COMPLETE_INTERRUPT 		0x00000001
#define TX_STOPPED_INTERRUPT 		0x00000002
#define TX_UNAVAILABLE_INTERRUPT	0x00000004
#define TX_JABBER_TIMEOUT_INTERRUPT	0x00000008
#define TX_UNDERFLOW_INTERRUPT		0x00000020
#define RX_COMPLETE_INTERRUPT		0x00000040
#define RX_UNAVAILABLE_INTERRUPT	0x00000080
#define RX_STOPPED_INTERRUPT		0x00000100
#define RX_WATCHDOG_TIMEOUT_INTERRUPT	0x00000200
#define TIMER_INTERRUPT			0x00000800
#define FATAL_BUS_ERROR_INTERRUPT	0x00002000
#define ABNORMAL_INTERRUPT_SUMMARY	0x00008000
#define NORMAL_INTERRUPT_SUMMARY	0x00010000

#define SR_CSR5				0x0028
#define NAR_CSR6			0x0030

#define IER_CSR7			0x0038
#define		CSR7_TCIE		0x00000001 /* tx complete */
#define		CSR7_TPSIE		0x00000002 /* tx processor stopped */
#define		CSR7_TDUIE		0x00000004 /* tx desc unavailable */
#define 	CSR7_TUIE		0x00000020 /* tx underflow */
#define		CSR7_RCIE		0x00000040 /* rx complete */
#define 	CSR7_RUIE		0x00000080 /* rx desc unavailable */
#define		CSR7_RSIE		0x00000100 /* rx processor stopped */
#define 	CSR7_FBEIE		0x00002000 /* fatal bus error */
#define		CSR7_AIE		0x00008000 /* abnormal enable */
#define 	CSR7_NIE		0x00010000 /* normal enable */

#define DEFAULT_COMMON_INTERRUPTS (CSR7_TCIE|CSR7_TPSIE|CSR7_RUIE|CSR7_RSIE|\
				   CSR7_FBEIE|CSR7_AIE|CSR7_NIE)

#define DEFAULT_NORMAL_INTERRUPTS (DEFAULT_COMMON_INTERRUPTS|CSR7_TDUIE)

#define DEFAULT_NO_IDLE_INTERRUPTS (DEFAULT_COMMON_INTERRUPTS|CSR7_RCIE)

#define CSR9				0x0048
#define 	CSR9_MDC		0x00010000
#define 	CSR9_MDO		0x00020000
#define 	CSR9_MMC		0x00040000
#define 	CSR9_MDI		0x00080000

#define OWN_BIT (1 << 31)

#ifdef CONFIG_VOICEBUS_ECREFERENCE

/*
 * These dahdi_fifo_xxx functions are currently only used by the voicebus
 * drivers, but are named more generally to facilitate moving out in the
 * future.  They probably also could stand to be changed in order to use a
 * kfifo implementation from the kernel if one is available.
 *
 */

struct dahdi_fifo {
	size_t total_size;
	u32 start;
	u32 end;
	u8 data[0];
};

static unsigned int dahdi_fifo_used_space(struct dahdi_fifo *fifo)
{
	return (fifo->end >= fifo->start) ? fifo->end - fifo->start :
			    fifo->total_size - fifo->start + fifo->end;
}

unsigned int __dahdi_fifo_put(struct dahdi_fifo *fifo, u8 *data, size_t size)
{
	int newinsertposition;
	int cpy_one_len, cpy_two_len;

	if ((size + dahdi_fifo_used_space(fifo)) > (fifo->total_size - 1))
		return -1;

	if ((fifo->end + size) >= fifo->total_size) {
		cpy_one_len = fifo->total_size - fifo->end;
		cpy_two_len = fifo->end + size - fifo->total_size;
		newinsertposition = cpy_two_len;
	} else {
		cpy_one_len = size;
		cpy_two_len = 0;
		newinsertposition = fifo->end + size;
	}

	memcpy(&fifo->data[fifo->end], data, cpy_one_len);

	if (cpy_two_len)
		memcpy(&fifo->data[0], &data[cpy_one_len], cpy_two_len);

	fifo->end = newinsertposition;

	return size;
}
EXPORT_SYMBOL(__dahdi_fifo_put);

unsigned int __dahdi_fifo_get(struct dahdi_fifo *fifo, u8 *data, size_t size)
{
	int newbegin;
	int cpy_one_len, cpy_two_len;

	if (size > dahdi_fifo_used_space(fifo))
		return 0;

	if ((fifo->start + size) >= fifo->total_size) {
		cpy_one_len = fifo->total_size - fifo->start;
		cpy_two_len = fifo->start + size - fifo->total_size;
		newbegin = cpy_two_len;
	} else {
		cpy_one_len = size;
		cpy_two_len = 0;
		newbegin = fifo->start + size;
	}

	memcpy(&data[0], &fifo->data[fifo->start], cpy_one_len);

	if (cpy_two_len)
		memcpy(&data[cpy_one_len], &fifo->data[0], cpy_two_len);

	fifo->start = newbegin;

	return size;
}
EXPORT_SYMBOL(__dahdi_fifo_get);

void dahdi_fifo_free(struct dahdi_fifo *fifo)
{
	kfree(fifo);
}
EXPORT_SYMBOL(dahdi_fifo_free);

struct dahdi_fifo *dahdi_fifo_alloc(u32 maxsize, gfp_t alloc_flags)
{
	struct dahdi_fifo *fifo;

	fifo = kmalloc(maxsize + sizeof(*fifo) + 1, alloc_flags);

	if (!fifo)
		return NULL;

	fifo->start = fifo->end = 0;
	fifo->total_size = maxsize + 1;

	return fifo;
}
EXPORT_SYMBOL(dahdi_fifo_alloc);
#endif /* CONFIG_VOICEBUS_ECREFERENCE */

#if defined(__FreeBSD__)
#define in_interrupt()	0
#define barrier()

#define DMA_FROM_DEVICE	0
#define DMA_TO_DEVICE	1

struct vbb *
voicebus_alloc(struct voicebus *vb, int malloc_flags)
{
	struct vbb *vbb;
	int res;
	bus_dmamap_t dma_map;

	res = bus_dmamem_alloc(vb->dma_tag, (void **) &vbb, BUS_DMA_NOWAIT | BUS_DMA_ZERO, &dma_map);
	if (res)
		return NULL;
	vbb->dma_map = dma_map;

	return vbb;
}

void
voicebus_free(struct voicebus *vb, struct vbb *vbb)
{
	bus_dmamap_t dma_map = vbb->dma_map;

	bus_dmamem_free(vb->dma_tag, vbb, dma_map);
	bus_dmamap_destroy(vb->dma_tag, dma_map);
}

static __le32
voicebus_map(struct voicebus *vb, struct vbb *vbb, int direction)
{
	int res;

	res = bus_dmamap_load(vb->dma_tag, vbb->dma_map, vbb->data, sizeof(vbb->data),
	    dahdi_dma_map_addr, &vbb->paddr, 0);
	if (res) {
		if (printk_ratelimit())
			printf("voicebus: Can't load DMA map\n");
		return 0;
	}
	return vbb->paddr;
}

static void
voicebus_unmap(struct voicebus *vb, struct vbb *vbb, int direction)
{
	bus_dmamap_unload(vb->dma_tag, vbb->dma_map);
}
#else /* !__FreeBSD__ */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
kmem_cache_t *voicebus_vbb_cache;
#else
struct kmem_cache *voicebus_vbb_cache;
#endif

struct vbb *
voicebus_alloc(struct voicebus *vb, int malloc_flags)
{
	return kmem_cache_alloc(voicebus_vbb_cache, malloc_flags);
}
EXPORT_SYMBOL(voicebus_alloc);

void
voicebus_free(struct voicebus *vb, struct vbb *vbb)
{
	kmem_cache_free(voicebus_vbb_cache, vbb);
}
EXPORT_SYMBOL(voicebus_free);

static __le32
voicebus_map(struct voicebus *vb, struct vbb *vbb, int direction)
{
	vbb->paddr = dma_map_single(&vb->pdev->dev, vbb->data, sizeof(vbb->data), direction);
	return vbb->paddr;
}

static void
voicebus_unmap(struct voicebus *vb, struct vbb *vbb, int direction)
{
	dma_unmap_single(&vb->pdev->dev, vbb->paddr, VOICEBUS_SFRAME_SIZE, direction);
}
#endif /* !__FreeBSD__ */

/* In memory structure shared by the host and the adapter. */
struct voicebus_descriptor {
	volatile __le32 des0;
	volatile __le32 des1;
	volatile __le32 buffer1;
	volatile __le32 container; /* Unused */
} __attribute__((packed));

static inline void
handle_transmit(struct voicebus *vb, struct list_head *buffers)
{
	vb->ops->handle_transmit(vb, buffers);
}

static inline void
handle_receive(struct voicebus *vb, struct list_head *buffers)
{
	vb->ops->handle_receive(vb, buffers);
}

static inline struct voicebus_descriptor *
vb_descriptor(const struct voicebus_descriptor_list *dl,
	      const unsigned int index)
{
	struct voicebus_descriptor *d;
	d = (struct voicebus_descriptor *)((u8*)dl->desc +
		((sizeof(*d) + dl->padding) * index));
	return d;
}

static int
vb_initialize_descriptors(struct voicebus *vb, struct voicebus_descriptor_list *dl, u32 des1)
{
	int i;
	struct voicebus_descriptor *d;
	const u32 END_OF_RING = 0x02000000;
	u8 cache_line_size;

	BUG_ON(!dl);

	/*
	 * Add some padding to each descriptor to ensure that they are
	 * aligned on host system cache-line boundaries, but only for the
	 * cache-line sizes that we support.
	 *
	 */
#if defined(__FreeBSD__)
	if ((cache_line_size = pci_read_config(vb->pdev->dev, 0x0c, 1)) == 0) {
		dev_err(&vb->pdev->dev, "Failed read of cache line "
			"size from PCI configuration space.\n");
		return -EIO;
	}
#else /* !__FreeBSD__ */
	if (pci_read_config_byte(vb->pdev, 0x0c, &cache_line_size)) {
		dev_err(&vb->pdev->dev, "Failed read of cache line "
			"size from PCI configuration space.\n");
		return -EIO;
	}
#endif

	if ((0x08 == cache_line_size) || (0x10 == cache_line_size) ||
	    (0x20 == cache_line_size)) {
		dl->padding = (cache_line_size*sizeof(u32)) - sizeof(*d);
	} else {
		dl->padding = 0;
	}

#if defined(__FreeBSD__)
	i = dahdi_dma_allocate(vb->pdev->dev, (sizeof(*d) + dl->padding) * DRING_SIZE,
	    &dl->dma_tag, &dl->dma_map, (void **) &dl->desc, &dl->desc_dma);
	if (i) {
		return i;
	}
#else /* !__FreeBSD__ */
	dl->desc = pci_alloc_consistent(vb->pdev,
		(sizeof(*d) + dl->padding) * DRING_SIZE, &dl->desc_dma);
	if (!dl->desc)
		return -ENOMEM;
#endif /* !__FreeBSD__ */

	memset(dl->desc, 0, (sizeof(*d) + dl->padding) * DRING_SIZE);
	for (i = 0; i < DRING_SIZE; ++i) {
		d = vb_descriptor(dl, i);
		d->des1 = des1;
	}
	d->des1 |= cpu_to_le32(END_OF_RING);
#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_PREWRITE);
#endif
	atomic_set(&dl->count, 0);
	return 0;
}

#define OWNED(_d_) (((_d_)->des0)&OWN_BIT)
#define SET_OWNED(_d_) do { wmb(); (_d_)->des0 |= OWN_BIT; wmb(); } while (0)

static int
vb_initialize_tx_descriptors(struct voicebus *vb)
{
	int i;
	int des1 = 0xe4800000 | VOICEBUS_SFRAME_SIZE;
	struct voicebus_descriptor *d;
	struct voicebus_descriptor_list *dl = &vb->txd;
	const u32 END_OF_RING = 0x02000000;
	u8 cache_line_size;

	WARN_ON(!dl);
	WARN_ON((NULL == vb->idle_vbb) || (0 == vb->idle_vbb_dma_addr));

	/*
	 * Add some padding to each descriptor to ensure that they are
	 * aligned on host system cache-line boundaries, but only for the
	 * cache-line sizes that we support.
	 *
	 */
#if defined(__FreeBSD__)
	if ((cache_line_size = pci_read_config(vb->pdev->dev, 0x0c, 1)) == 0) {
		dev_err(&vb->pdev->dev, "Failed read of cache line "
			"size from PCI configuration space.\n");
		return EIO;
	}
#else /* !__FreeBSD__ */
	if (pci_read_config_byte(vb->pdev, 0x0c, &cache_line_size)) {
		dev_err(&vb->pdev->dev, "Failed read of cache line "
			"size from PCI configuration space.\n");
		return -EIO;
	}
#endif /* !__FreeBSD__ */

	if ((0x08 == cache_line_size) || (0x10 == cache_line_size) ||
	    (0x20 == cache_line_size)) {
		dl->padding = (cache_line_size*sizeof(u32)) - sizeof(*d);
	} else {
		dl->padding = 0;
	}

#if defined(__FreeBSD__)
	i = dahdi_dma_allocate(vb->pdev->dev, (sizeof(*d) + dl->padding) * DRING_SIZE,
	    &dl->dma_tag, &dl->dma_map, (void **) &dl->desc, &dl->desc_dma);
	if (i) {
		return i;
	}
#else /* !__FreeBSD__ */
	dl->desc = pci_alloc_consistent(vb->pdev,
					(sizeof(*d) + dl->padding) *
					DRING_SIZE, &dl->desc_dma);
	if (!dl->desc)
		return -ENOMEM;
#endif /* !__FreeBSD__ */

	memset(dl->desc, 0, (sizeof(*d) + dl->padding) * DRING_SIZE);

	for (i = 0; i < DRING_SIZE; ++i) {
		d = vb_descriptor(dl, i);
		d->des1 = des1;
		dl->pending[i] = NULL;
		d->buffer1 = 0;
	}
	d->des1 |= cpu_to_le32(END_OF_RING);
#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_PREWRITE);
#endif
	atomic_set(&dl->count, 0);
	return 0;
}

static int
vb_initialize_rx_descriptors(struct voicebus *vb)
{
	return vb_initialize_descriptors(
		vb, &vb->rxd, VOICEBUS_SFRAME_SIZE);
}

/*! \brief  Use to set the minimum number of buffers queued to the hardware
 * before enabling interrupts.
 */
int
voicebus_set_minlatency(struct voicebus *vb, unsigned int ms)
{
	unsigned long flags;
	/*
	 * One millisecond of latency means that we have 3 buffers pending,
	 * since two are always going to be waiting in the TX fifo on the
	 * interface chip.
	 *
	 */
#define MESSAGE "%d ms is an invalid value for minumum latency.  Setting to %d ms.\n"
	if (DRING_SIZE < ms) {
		dev_warn(&vb->pdev->dev, MESSAGE, ms, DRING_SIZE);
		return -EINVAL;
	} else if (VOICEBUS_DEFAULT_LATENCY > ms) {
		dev_warn(&vb->pdev->dev, MESSAGE, ms, VOICEBUS_DEFAULT_LATENCY);
		return -EINVAL;
	}
	spin_lock_irqsave(&vb->lock, flags);
	vb->min_tx_buffer_count = ms;
	spin_unlock_irqrestore(&vb->lock, flags);
	return 0;
}
EXPORT_SYMBOL(voicebus_set_minlatency);

/*! \brief Returns the number of buffers currently on the transmit queue. */
int
voicebus_current_latency(struct voicebus *vb)
{
	int latency;
	unsigned long flags;
	spin_lock_irqsave(&vb->lock, flags);
	latency = vb->min_tx_buffer_count;
	spin_unlock_irqrestore(&vb->lock, flags);
	return latency;
}
EXPORT_SYMBOL(voicebus_current_latency);


/*!
 * \brief Read one of the hardware control registers without acquiring locks.
 */
static inline u32
__vb_getctl(struct voicebus *vb, u32 addr)
{
	u32 ret;
#if defined(__FreeBSD__)
	ret = bus_space_read_4(rman_get_bustag(vb->mem_res), rman_get_bushandle(vb->mem_res), addr);
#else
	ret = readl(vb->iobase + addr);
	rmb();
#endif
	return ret;
}

/*!
 * \brief Read one of the hardware control registers with locks held.
 */
static inline u32
vb_getctl(struct voicebus *vb, u32 addr)
{
	unsigned long flags;
	u32 val;
	spin_lock_irqsave(&vb->lock, flags);
	val = __vb_getctl(vb, addr);
	spin_unlock_irqrestore(&vb->lock, flags);
	return val;
}

static int
__vb_is_stopped(struct voicebus *vb)
{
	u32 reg;
	reg = __vb_getctl(vb, SR_CSR5);
	reg = (reg >> 17) & 0x3f;
	return ((0 == reg) || (3 == reg)) ? 1 : 0;
}
/*!
 * \brief Returns whether or not the interface is running.
 *
 * NOTE:  Running in this case means whether or not the hardware reports the
 *        transmit processor in any state but stopped.
 *
 * \return 1 of the process is stopped, 0 if running.
 */
static int
vb_is_stopped(struct voicebus *vb)
{
	int ret;
	unsigned long flags;
	spin_lock_irqsave(&vb->lock, flags);
	ret = __vb_is_stopped(vb);
	spin_unlock_irqrestore(&vb->lock, flags);
	return ret;
}

#if defined(CONFIG_VOICEBUS_INTERRUPT)

static inline void vb_disable_deferred(struct voicebus *vb)
{
#if defined(__FreeBSD__)
	atomic_inc(&vb->deferred_disabled_count);
#else
	if (atomic_inc_return(&vb->deferred_disabled_count) == 1)
		disable_irq(vb->pdev->irq);
#endif
}

static inline void vb_enable_deferred(struct voicebus *vb)
{
#if defined(__FreeBSD__)
	atomic_dec(&vb->deferred_disabled_count);
#else
	if (atomic_dec_return(&vb->deferred_disabled_count) == 0)
		enable_irq(vb->pdev->irq);
#endif
}

#else

static inline void vb_disable_deferred(struct voicebus *vb)
{
	tasklet_disable(&vb->tasklet);
}

static inline void vb_enable_deferred(struct voicebus *vb)
{
	tasklet_enable(&vb->tasklet);
}

#endif

static void
vb_cleanup_tx_descriptors(struct voicebus *vb)
{
	unsigned int i;
	struct voicebus_descriptor_list *dl = &vb->txd;
	struct voicebus_descriptor *d;
	struct vbb *vbb;

	vb_disable_deferred(vb);

	while (!list_empty(&vb->tx_complete)) {
		vbb = list_entry(vb->tx_complete.next, struct vbb, entry);
		list_del(&vbb->entry);
		voicebus_free(vb, vbb);
	}

#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_POSTREAD);
#endif
	for (i = 0; i < DRING_SIZE; ++i) {
		d = vb_descriptor(dl, i);
		if (d->buffer1 && (d->buffer1 != vb->idle_vbb_dma_addr)) {
			WARN_ON(!dl->pending[i]);
			voicebus_unmap(vb, dl->pending[i], DMA_TO_DEVICE);
			voicebus_free(vb, dl->pending[i]);
		}
		if (NORMAL == vb->mode) {
			d->des1 |= 0x80000000;
			d->buffer1 = vb->idle_vbb_dma_addr;
			dl->pending[i] = vb->idle_vbb;
			SET_OWNED(d);
		} else {
			d->buffer1 = 0;
			dl->pending[i] = NULL;
			d->des0 &= ~OWN_BIT;
		}
	}
#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_PREWRITE);
#endif

	dl->head = dl->tail = 0;
	atomic_set(&dl->count, 0);
	vb_enable_deferred(vb);
}

static void vb_cleanup_rx_descriptors(struct voicebus *vb)
{
	unsigned int i;
	struct voicebus_descriptor_list *dl = &vb->rxd;
	struct voicebus_descriptor *d;
	struct vbb *vbb;

	vb_disable_deferred(vb);
#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_POSTREAD);
#endif
	for (i = 0; i < DRING_SIZE; ++i) {
		d = vb_descriptor(dl, i);
		if (d->buffer1) {
			voicebus_unmap(vb, dl->pending[i], DMA_FROM_DEVICE);
			d->buffer1 = 0;
			BUG_ON(!dl->pending[i]);
			vbb = dl->pending[i];
			list_add_tail(&vbb->entry, &vb->free_rx);
			dl->pending[i] = NULL;
		}
		d->des0 &= ~OWN_BIT;
	}
#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_PREWRITE);
#endif
	dl->head = 0;
	dl->tail = 0;
	atomic_set(&dl->count, 0);
	vb_enable_deferred(vb);
}

static void vb_cleanup_descriptors(struct voicebus *vb,
				   struct voicebus_descriptor_list *dl)
{
	if (dl == &vb->txd)
		vb_cleanup_tx_descriptors(vb);
	else
		vb_cleanup_rx_descriptors(vb);
}

static void
vb_free_descriptors(struct voicebus *vb, struct voicebus_descriptor_list *dl)
{
	struct vbb *vbb;
	if (NULL == dl->desc) {
		WARN_ON(1);
		return;
	}
	vb_cleanup_descriptors(vb, dl);
#if defined(__FreeBSD__)
	dahdi_dma_free(&dl->dma_tag, &dl->dma_map, (void **) &dl->desc, &dl->desc_dma);
#else /* !__FreeBSD__ */
	pci_free_consistent(
		vb->pdev,
		(sizeof(struct voicebus_descriptor)+dl->padding)*DRING_SIZE,
		dl->desc, dl->desc_dma);
#endif /* !__FreeBSD__ */
	while (!list_empty(&vb->free_rx)) {
		vbb = list_entry(vb->free_rx.next, struct vbb, entry);
		list_del(&vbb->entry);
		voicebus_free(vb, vbb);
	}
}

/*!
 * \brief Write one of the hardware control registers without acquiring locks.
 */
static inline void
__vb_setctl(struct voicebus *vb, u32 addr, u32 val)
{
#if defined(__FreeBSD__)
	bus_space_write_4(rman_get_bustag(vb->mem_res), rman_get_bushandle(vb->mem_res),
	    addr, val);
#else
	wmb();
	writel(val, vb->iobase + addr);
	readl(vb->iobase + addr);
#endif
}

/*!
 * \brief Write one of the hardware control registers with locks held.
 */
static inline void
vb_setctl(struct voicebus *vb, u32 addr, u32 val)
{
	unsigned long flags;
	spin_lock_irqsave(&vb->lock, flags);
	__vb_setctl(vb, addr, val);
	spin_unlock_irqrestore(&vb->lock, flags);
}

static int
__vb_sdi_clk(struct voicebus *vb, u32 *sdi)
{
	unsigned int ret;
	*sdi &= ~CSR9_MDC;
	__vb_setctl(vb, 0x0048, *sdi);
	ret = __vb_getctl(vb, 0x0048);
	*sdi |= CSR9_MDC;
	__vb_setctl(vb, 0x0048, *sdi);
	return (ret & CSR9_MDI) ? 1 : 0;
}

static void
__vb_sdi_sendbits(struct voicebus *vb, u32 bits, int count, u32 *sdi)
{
	*sdi &= ~CSR9_MMC;
	__vb_setctl(vb, 0x0048, *sdi);
	while (count--) {

		if (bits & (1 << count))
			*sdi |= CSR9_MDO;
		else
			*sdi &= ~CSR9_MDO;

		__vb_sdi_clk(vb, sdi);
	}
}

static void
vb_setsdi(struct voicebus *vb, int addr, u16 val)
{
	u32 bits;
	u32 sdi = 0;
	unsigned long flags;
	/* Send preamble */
	bits = 0xffffffff;
	spin_lock_irqsave(&vb->lock, flags);
	__vb_sdi_sendbits(vb, bits, 32, &sdi);
	bits = (0x5 << 12) | (1 << 7) | (addr << 2) | 0x2;
	__vb_sdi_sendbits(vb, bits, 16, &sdi);
	__vb_sdi_sendbits(vb, val, 16, &sdi);
	spin_unlock_irqrestore(&vb->lock, flags);
}

static void
vb_enable_io_access(struct voicebus *vb)
{
#if !defined(__FreeBSD__)
	u32 reg;
#endif
	unsigned long flags;
	BUG_ON(!vb->pdev);
	spin_lock_irqsave(&vb->lock, flags);
#if defined(__FreeBSD__)
	pci_enable_io(vb->pdev->dev, SYS_RES_IOPORT);
	pci_enable_io(vb->pdev->dev, SYS_RES_MEMORY);
	pci_enable_busmaster(vb->pdev->dev);
#else /* !__FreeBSD__ */
	pci_read_config_dword(vb->pdev, 0x0004, &reg);
	reg |= 0x00000007;
	pci_write_config_dword(vb->pdev, 0x0004, reg);
#endif /* !__FreeBSD__ */
	spin_unlock_irqrestore(&vb->lock, flags);
}

/*! \brief Resets the voicebus hardware interface. */
static int
vb_reset_interface(struct voicebus *vb)
{
	unsigned long timeout;
	u32 reg;
	u32 pci_access;
	const u32 DEFAULT_PCI_ACCESS = 0xfffc0002;
	u8 cache_line_size;
	BUG_ON(in_interrupt());

#if defined(__FreeBSD__)
	if ((cache_line_size = pci_read_config(vb->pdev->dev, 0x0c, 1)) == 0) {
		dev_err(&vb->pdev->dev, "Failed read of cache line "
			"size from PCI configuration space.\n");
		return EIO;
	}
#else /* !__FreeBSD__ */
	if (pci_read_config_byte(vb->pdev, 0x0c, &cache_line_size)) {
		dev_err(&vb->pdev->dev, "Failed read of cache line "
			"size from PCI configuration space.\n");
		return -EIO;
	}
#endif /* !__FreeBSD__ */

	switch (cache_line_size) {
	case 0x08:
		pci_access = DEFAULT_PCI_ACCESS | (0x1 << 14);
		break;
	case 0x10:
		pci_access = DEFAULT_PCI_ACCESS | (0x2 << 14);
		break;
	case 0x20:
		pci_access = DEFAULT_PCI_ACCESS | (0x3 << 14);
		break;
	default:
		if (*vb->debug) {
			dev_warn(&vb->pdev->dev, "Host system set a cache "
				 "size of %d which is not supported. "
				 "Disabling memory write line and memory "
				 "read line.\n", cache_line_size);
		}
		pci_access = 0xfe584202;
		break;
	}

	/* The transmit and receive descriptors will have the same padding. */
	pci_access |= ((vb->txd.padding / sizeof(u32)) << 2) & 0x7c;

	vb_setctl(vb, 0x0000, pci_access | 1);

	timeout = jiffies + HZ/10; /* 100ms interval */
	do {
		reg = vb_getctl(vb, 0x0000);
	} while ((reg & 0x00000001) && time_before(jiffies, timeout));

	if (reg & 0x00000001) {
		dev_warn(&vb->pdev->dev, "Did not come out of reset "
			 "within 100ms\n");
		return -EIO;
	}

	vb_setctl(vb, 0x0000, pci_access);

	return 0;
}

/*!
 * \brief Give a frame to the hardware to use for receiving.
 *
 */
static inline int
vb_submit_rxb(struct voicebus *vb, struct vbb *vbb)
{
	struct voicebus_descriptor *d;
	struct voicebus_descriptor_list *dl = &vb->rxd;
	unsigned int tail = dl->tail;

	d = vb_descriptor(dl, tail);

#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_POSTREAD);
#endif
	if (unlikely(d->buffer1)) {
		/* Do not overwrite a buffer that is still in progress. */
		WARN_ON(1);
		list_add_tail(&vbb->entry, &vb->free_rx);
		return -EBUSY;
	}

	dl->pending[tail] = vbb;
	dl->tail = (++tail) & DRING_MASK;
	d->buffer1 = voicebus_map(vb, vbb, DMA_FROM_DEVICE);
	SET_OWNED(d); /* That's it until the hardware is done with it. */
#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_PREWRITE);
#endif
	atomic_inc(&dl->count);
	return 0;
}

/**
 * voicebus_transmit - Queue a buffer on the hardware descriptor ring.
 *
 */
int voicebus_transmit(struct voicebus *vb, struct vbb *vbb)
{
	struct voicebus_descriptor *d;
	struct voicebus_descriptor_list *dl = &vb->txd;

	d = vb_descriptor(dl, dl->tail);

#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(vb->dma_tag, vbb->dma_map, BUS_DMASYNC_PREWRITE);
#endif
	if (unlikely((d->buffer1 != vb->idle_vbb_dma_addr) && d->buffer1)) {
		if (printk_ratelimit())
			dev_warn(&vb->pdev->dev, "Dropping tx buffer buffer\n");
		voicebus_free(vb, vbb);
		/* Schedule the underrun handler to run here, since we'll need
		 * to cleanup as best we can. */
		schedule_work(&vb->underrun_work);
		return -EFAULT;
	}

	dl->pending[dl->tail] = vbb;
	d->buffer1 = voicebus_map(vb, vbb, DMA_TO_DEVICE);
	dl->tail = (dl->tail + 1) & DRING_MASK;
	SET_OWNED(d); /* That's it until the hardware is done with it. */
#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_PREWRITE);
#endif
	atomic_inc(&dl->count);
	return 0;
}
EXPORT_SYMBOL(voicebus_transmit);


/*!
 * \brief Instruct the hardware to check for a new tx descriptor.
 */
static inline void
__vb_tx_demand_poll(struct voicebus *vb)
{
	u32 status = __vb_getctl(vb, 0x0028);
	if ((status & 0x00700000) == 0x00600000)
		__vb_setctl(vb, 0x0008, 0x00000000);
}

static void setup_descriptors(struct voicebus *vb)
{
	int i;
	struct vbb *vbb;
	_LIST_HEAD(buffers);

	might_sleep();

	vb_cleanup_tx_descriptors(vb);
	vb_cleanup_rx_descriptors(vb);

	/* Tell the card where the descriptors are in host memory. */
	vb_setctl(vb, 0x0020, (u32)vb->txd.desc_dma);
	vb_setctl(vb, 0x0018, (u32)vb->rxd.desc_dma);

	for (i = 0; i < DRING_SIZE; ++i) {
		if (list_empty(&vb->free_rx)) {
			vbb = voicebus_alloc(vb, GFP_KERNEL);
		} else {
			vbb = list_entry(vb->free_rx.next, struct vbb, entry);
			list_del(&vbb->entry);
		}
		if (unlikely(NULL == vbb))
			BUG_ON(1);
		list_add_tail(&vbb->entry, &buffers);
	}

	vb_disable_deferred(vb);
	while (!list_empty(&buffers)) {
		vbb = list_entry(buffers.next, struct vbb, entry);
		list_del(&vbb->entry);
		vb_submit_rxb(vb, vbb);
	}
	vb_enable_deferred(vb);

	if (BOOT != vb->mode) {
		for (i = 0; i < vb->min_tx_buffer_count; ++i) {
			vbb = voicebus_alloc(vb, GFP_KERNEL);
			if (unlikely(NULL == vbb))
				BUG_ON(1);
			else
				list_add_tail(&vbb->entry, &buffers);
		}

		handle_transmit(vb, &buffers);

		vb_disable_deferred(vb);
		while (!list_empty(&buffers)) {
			vbb = list_entry(buffers.next, struct vbb, entry);
			list_del_init(&vbb->entry);
			voicebus_transmit(vb, vbb);
		}
		vb_enable_deferred(vb);
	}

}

static void
__vb_set_control_defaults(struct voicebus *vb)
{
	/* Pass bad packets, runt packets, disable SQE function,
	 * store-and-forward */
	vb_setctl(vb, 0x0030, 0x00280048);
	/* ...disable jabber and the receive watchdog. */
	vb_setctl(vb, 0x0078, 0x00000013);
	vb_getctl(vb, 0x0078);
}

static void __vb_set_mac_only_mode(struct voicebus *vb)
{
	u32 reg;
	reg = __vb_getctl(vb, 0x00fc);
	__vb_setctl(vb, 0x00fc, (reg & ~0x7) | 0x4);
	__vb_getctl(vb, 0x00fc);
}

static int
vb_initialize_interface(struct voicebus *vb)
{
	u32 reg;

	setup_descriptors(vb);

	__vb_set_control_defaults(vb);

	reg = vb_getctl(vb, 0x00fc);
	vb_setctl(vb, 0x00fc, (reg & ~0x7) | 0x7);
	vb_setsdi(vb, 0x00, 0x0100);
	vb_setsdi(vb, 0x16, 0x2100);

	__vb_set_mac_only_mode(vb);
	vb_setsdi(vb, 0x00, 0x0100);
	vb_setsdi(vb, 0x16, 0x2100);
	reg = vb_getctl(vb, 0x00fc);

	/*
	 * The calls to setsdi above toggle the reset line of the CPLD.  Wait
	 * here to give the CPLD time to stabilize after reset.
	 */
	msleep(10);

	return ((reg&0x7) == 0x4) ? 0 : -EIO;
}

#ifdef DBG
static void
dump_descriptor(struct voicebus *vb, struct voicebus_descriptor *d)
{
	VB_PRINTK(vb, DEBUG, "Displaying descriptor at address %08x\n", (unsigned int)d);
	VB_PRINTK(vb, DEBUG, "   des0:      %08x\n", d->des0);
	VB_PRINTK(vb, DEBUG, "   des1:      %08x\n", d->des1);
	VB_PRINTK(vb, DEBUG, "   buffer1:   %08x\n", d->buffer1);
	VB_PRINTK(vb, DEBUG, "   container: %08x\n", d->container);
}

static void
show_buffer(struct voicebus *vb, struct vbb *vbb)
{
	int x;
	unsigned char *c;
	c = vbb;
	printk(KERN_DEBUG "Packet %d\n", count);
	printk(KERN_DEBUG "");
	for (x = 1; x <= VOICEBUS_SFRAME_SIZE; ++x) {
		printk("%02x ", c[x]);
		if (x % 16 == 0)
			printk("\n");
	}
	printk(KERN_DEBUG "\n\n");
}
#endif

/*!
 * \brief Remove the next completed transmit buffer (txb) from the tx
 * descriptor ring.
 *
 * NOTE:  This function doesn't need any locking because only one instance is
 * 	  ever running on the deferred processing routine and it only looks at
 * 	  the head pointer. The deferred routine should only ever be running
 * 	  on one processor at a time (no multithreaded workqueues allowed!)
 *
 * Context: Must be called from the voicebus deferred workqueue.
 *
 * \return Pointer to buffer, or NULL if not available.
 */
static void *
vb_get_completed_txb(struct voicebus *vb)
{
	struct voicebus_descriptor_list *dl = &vb->txd;
	struct voicebus_descriptor *d;
	struct vbb *vbb;
	unsigned int head = dl->head;

	d = vb_descriptor(dl, head);

#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_POSTREAD);
#endif
	if (OWNED(d) || !d->buffer1 || (d->buffer1 == vb->idle_vbb_dma_addr))
		return NULL;

	vbb = dl->pending[head];
	voicebus_unmap(vb, vbb, DMA_TO_DEVICE);
	if (NORMAL == vb->mode) {
		d->buffer1 = vb->idle_vbb_dma_addr;
		dl->pending[head] = vb->idle_vbb;
		SET_OWNED(d);
	} else {
		d->buffer1 = 0;
		dl->pending[head] = NULL;
	}
	dl->head = (++head) & DRING_MASK;
	atomic_dec(&dl->count);
	vb_net_capture_vbb(vb, vbb, 1, d->des0, d->container);
#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_PREWRITE);
#endif
	return vbb;
}

static void *
vb_get_completed_rxb(struct voicebus *vb, u32 *des0)
{
	struct voicebus_descriptor *d;
	struct voicebus_descriptor_list *dl = &vb->rxd;
	unsigned int head = dl->head;
	struct vbb *vbb;

	d = vb_descriptor(dl, head);

#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_POSTREAD);
#endif
	if ((0 == d->buffer1) || OWNED(d))
		return NULL;

	vbb = dl->pending[head];
	voicebus_unmap(vb, vbb, DMA_FROM_DEVICE);
	dl->head = (++head) & DRING_MASK;
	d->buffer1 = 0;
	atomic_dec(&dl->count);
#	ifdef VOICEBUS_NET_DEBUG
	vb_net_capture_vbb(vb, vbb, 0, d->des0, d->container);
#	endif
	*des0 = le32_to_cpu(d->des0);
#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(vb->dma_tag, vbb->dma_map, BUS_DMASYNC_POSTREAD);
#endif
	return vbb;
}

/*!
 * \brief Command the hardware to check if it owns the next receive
 * descriptor.
 */
static inline void
__vb_rx_demand_poll(struct voicebus *vb)
{
	if (((__vb_getctl(vb, 0x0028) >> 17) & 0x7) == 0x4)
		__vb_setctl(vb, 0x0010, 0x00000000);
}

static void
__vb_enable_interrupts(struct voicebus *vb)
{
	if (BOOT == vb->mode)
		__vb_setctl(vb, IER_CSR7, DEFAULT_NO_IDLE_INTERRUPTS);
	else
		__vb_setctl(vb, IER_CSR7, DEFAULT_NORMAL_INTERRUPTS);
}

static void
__vb_disable_interrupts(struct voicebus *vb)
{
	__vb_setctl(vb, IER_CSR7, 0);
}

static void
vb_disable_interrupts(struct voicebus *vb)
{
	unsigned long flags;
	spin_lock_irqsave(&vb->lock, flags);
	__vb_disable_interrupts(vb);
	spin_unlock_irqrestore(&vb->lock, flags);
}

static void start_packet_processing(struct voicebus *vb)
{
	u32 reg;
	unsigned long flags;

	spin_lock_irqsave(&vb->lock, flags);
	clear_bit(VOICEBUS_STOP, &vb->flags);
	clear_bit(VOICEBUS_STOPPED, &vb->flags);
#if defined(CONFIG_VOICEBUS_TIMER)
	vb->timer.expires = jiffies + HZ/1000;
	add_timer(&vb->timer);
#else
	/* Clear the interrupt status register. */
	__vb_setctl(vb, SR_CSR5, 0xffffffff);
	__vb_enable_interrupts(vb);
#endif
	/* Start the transmit and receive processors. */
	reg = __vb_getctl(vb, 0x0030);
	__vb_setctl(vb, 0x0030, reg|0x00002002);
	__vb_getctl(vb, 0x0030);
	__vb_rx_demand_poll(vb);
	__vb_tx_demand_poll(vb);
	__vb_getctl(vb, 0x0030);
	spin_unlock_irqrestore(&vb->lock, flags);
}

static void vb_tasklet_boot(unsigned long data);
static void vb_tasklet_hx8(unsigned long data);
static void vb_tasklet_normal(unsigned long data);

/*!
 * \brief Starts the VoiceBus interface.
 *
 * When the VoiceBus interface is started, it is actively transferring
 * frames to and from the backend of the card.  This means the card will
 * generate interrupts.
 *
 * This function should only be called from process context, with interrupts
 * enabled, since it can sleep while running the self checks.
 *
 * \return zero on success. -EBUSY if device is already running.
 */
int
voicebus_start(struct voicebus *vb)
{
	int ret;

	if (!vb_is_stopped(vb))
		return -EBUSY;

	if (NORMAL == vb->mode) {
		tasklet_init(&vb->tasklet, vb_tasklet_normal,
			     (unsigned long)vb);
	} else if (BOOT == vb->mode) {
		tasklet_init(&vb->tasklet, vb_tasklet_boot,
			     (unsigned long)vb);
	} else if (HX8 == vb->mode) {
		tasklet_init(&vb->tasklet, vb_tasklet_hx8,
			     (unsigned long)vb);
	} else {
		return -EINVAL;
	}

	ret = vb_reset_interface(vb);
	if (ret)
		return ret;
	ret = vb_initialize_interface(vb);
	if (ret)
		return ret;

	start_packet_processing(vb);

	BUG_ON(vb_is_stopped(vb));

	return 0;
}
EXPORT_SYMBOL(voicebus_start);

static void vb_stop_txrx_processors(struct voicebus *vb)
{
	unsigned long flags;
	u32 reg;
	int i;

	spin_lock_irqsave(&vb->lock, flags);
	reg = __vb_getctl(vb, NAR_CSR6);
	reg &= ~(0x2002);
	__vb_setctl(vb, NAR_CSR6, reg);
	spin_unlock_irqrestore(&vb->lock, flags);

	barrier();
	i = 150;
	while (--i && (__vb_getctl(vb, SR_CSR5) & (0x007e0000)))
		udelay(100);
}

/*!
 * \brief Stops the VoiceBus interface.
 *
 * Stops the VoiceBus interface and waits for any outstanding DMA transactions
 * to complete.  When this functions returns the VoiceBus interface tx and rx
 * states will both be suspended.
 *
 * Only call this function from process context, with interrupt enabled,
 * without any locks held since it sleeps.
 *
 * \return zero on success, -1 on error.
 */
void voicebus_stop(struct voicebus *vb)
{
	static DEFINE_SPINLOCK(stop);

	spin_lock(&stop);

	if (test_bit(VOICEBUS_STOP, &vb->flags) || vb_is_stopped(vb)) {
		spin_unlock(&stop);
		return;
	}

	set_bit(VOICEBUS_STOP, &vb->flags);
	vb_stop_txrx_processors(vb);

	WARN_ON(!vb_is_stopped(vb));
	set_bit(VOICEBUS_STOPPED, &vb->flags);

#if defined(CONFIG_VOICEBUS_TIMER)
	del_timer_sync(&vb->timer);
#endif
	vb_disable_interrupts(vb);
	spin_unlock(&stop);
}
EXPORT_SYMBOL(voicebus_stop);

/*!
 * \brief Prepare the interface for module unload.
 *
 * Stop the interface and free all the resources allocated by the driver.  The
 * caller should have returned all VoiceBus buffers to the VoiceBus layer
 * before calling this function.
 *
 * context: !in_interrupt()
 */
void
voicebus_release(struct voicebus *vb)
{
	set_bit(VOICEBUS_SHUTDOWN, &vb->flags);

#ifdef VOICEBUS_NET_DEBUG
	vb_net_unregister(vb);
#endif

	/* Make sure the underrun_work isn't running or going to run. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22)
	flush_scheduled_work();
#else
	cancel_work_sync(&vb->underrun_work);
#endif
	/* quiesce the hardware */
	voicebus_stop(vb);

	vb_reset_interface(vb);

	tasklet_kill(&vb->tasklet);

#if !defined(CONFIG_VOICEBUS_TIMER)
#if defined(__FreeBSD__)
	if (vb->irq_handle != NULL) {
		bus_teardown_intr(vb->pdev->dev, vb->irq_res, vb->irq_handle);
		vb->irq_handle = NULL;
	}
	if (vb->irq_res != NULL) {
		bus_release_resource(vb->pdev->dev, SYS_RES_IRQ, vb->irq_rid, vb->irq_res);
		vb->irq_res = NULL;
	}
#else /* !__FreeBSD__ */
	free_irq(vb->pdev->irq, vb);
#endif /* !__FreeBSD__ */
#endif

	/* Cleanup memory and software resources. */
	vb_free_descriptors(vb, &vb->txd);
	vb_free_descriptors(vb, &vb->rxd);
	spin_lock_destroy(&vb->lock);
	if (vb->idle_vbb_dma_addr) {
#if defined(__FreeBSD__)
		dahdi_dma_free(&vb->idle_vbb_dma_tag, &vb->idle_vbb_dma_map,
		    (void **) &vb->idle_vbb, &vb->idle_vbb_dma_addr);
#else /* !__FreeBSD__ */
		dma_free_coherent(&vb->pdev->dev, VOICEBUS_SFRAME_SIZE,
				  vb->idle_vbb, vb->idle_vbb_dma_addr);
#endif
	}

#if defined(__FreeBSD__)
	if (vb->mem_res != NULL) {
		bus_release_resource(vb->pdev->dev, SYS_RES_MEMORY, vb->mem_rid, vb->mem_res);
		vb->mem_res = NULL;
	}

	if (vb->dma_tag != NULL) {
		bus_dma_tag_destroy(vb->dma_tag);
		vb->dma_tag = NULL;
	}
#else /* !__FreeBSD__ */
	release_mem_region(pci_resource_start(vb->pdev, 1),
		pci_resource_len(vb->pdev, 1));

	pci_iounmap(vb->pdev, vb->iobase);
	pci_disable_device(vb->pdev);
#endif /* !__FreeBSD__ */
}
EXPORT_SYMBOL(voicebus_release);

static void
vb_increase_latency(struct voicebus *vb, unsigned int increase,
		    struct list_head *buffers)
{
	struct vbb *vbb;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
	struct vbb *n;
#endif
	int i;
	_LIST_HEAD(local);

	if (0 == increase)
		return;

	if (test_bit(VOICEBUS_LATENCY_LOCKED, &vb->flags))
		return;

	if (unlikely(increase > VOICEBUS_MAXLATENCY_BUMP))
		increase = VOICEBUS_MAXLATENCY_BUMP;

	if ((increase + vb->min_tx_buffer_count) > vb->max_latency)
		increase = vb->max_latency - vb->min_tx_buffer_count;

	/* Because there are 2 buffers in the transmit FIFO on the hardware,
	 * setting 3 ms of latency means that the host needs to be able to
	 * service the cards within 1ms.  This is because the interface will
	 * load up 2 buffers into the TX FIFO then attempt to read the 3rd
	 * descriptor.  If the OWN bit isn't set, then the hardware will set the
	 * TX descriptor not available interrupt. */

	/* Set the minimum latency in case we're restarted...we don't want to
	 * wait for the buffer to grow to this depth again in that case. */
	for (i = 0; i < increase; ++i) {
		vbb = voicebus_alloc(vb, GFP_ATOMIC);
		WARN_ON(NULL == vbb);
		if (likely(NULL != vbb))
			list_add_tail(&vbb->entry, &local);
	}

	handle_transmit(vb, &local);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
	list_for_each_entry_safe(vbb, n, &local, entry)
		list_move_tail(&vbb->entry, buffers);
#else
	list_splice_tail(&local, buffers);
#endif

	/* Set the new latency (but we want to ensure that there aren't any
	 * printks to the console, so we don't call the function) */
	vb->min_tx_buffer_count += increase;
}

static void vb_schedule_deferred(struct voicebus *vb)
{
#if !defined(CONFIG_VOICEBUS_INTERRUPT)
	tasklet_hi_schedule(&vb->tasklet);
#else
	vb->tasklet.func(vb->tasklet.data);
#endif
}

/**
 * vb_tasklet_boot() - When vb->mode == BOOT
 *
 * This deferred processing routine is for hx8 boards during initialization.  It
 * simply services any completed tx / rx packets without any concerns about what
 * the current latency is.
 */
static void vb_tasklet_boot(unsigned long data)
{
	struct voicebus *vb = (struct voicebus *)data;
	_LIST_HEAD(buffers);
	struct vbb *vbb;
	const int DEFAULT_COUNT = 5;
	int count = DEFAULT_COUNT;
	u32 des0 = 0;

	/* First, temporarily store any non-idle buffers that the hardware has
	 * indicated it's finished transmitting.  Non idle buffers are those
	 * buffers that contain actual data and was filled out by the client
	 * driver (as of this writing, the wcte12xp or wctdm24xxp drivers) when
	 * passed up through the handle_transmit callback.
	 *
	 * On the other hand, idle buffers are "dummy" buffers that solely exist
	 * to in order to prevent the transmit descriptor ring from ever
	 * completely draining. */
	while ((vbb = vb_get_completed_txb(vb)))
		list_add_tail(&vbb->entry, &vb->tx_complete);

	while (--count && !list_empty(&vb->tx_complete))
		list_move_tail(vb->tx_complete.next, &buffers);

	/* Prep all the new buffers for transmit before actually sending any
	 * of them. */
	handle_transmit(vb, &buffers);

	while (!list_empty(&buffers)) {
		vbb = list_entry(buffers.next, struct vbb, entry);
		list_del(&vbb->entry);
		voicebus_transmit(vb, vbb);
	}

	/* If there may still be buffers in the descriptor rings, reschedule
	 * ourself to run again.  We essentially yield here to allow any other
	 * cards a chance to run. */
#if !defined(CONFIG_VOICEBUS_INTERRUPT)
	if (unlikely(!count && !test_bit(VOICEBUS_STOP, &vb->flags)))
		vb_schedule_deferred(vb);
#endif

	/* And finally, pass up any receive buffers. */
	count = DEFAULT_COUNT;
	while (--count && (vbb = vb_get_completed_rxb(vb, &des0))) {
		if (likely((des0 & (0x7fff << 16)) ==
		    (VOICEBUS_SFRAME_SIZE << 16)))
			list_add_tail(&vbb->entry, &buffers);
		else
			vb_submit_rxb(vb, vbb);
	}

	handle_receive(vb, &buffers);
	while (!list_empty(&buffers)) {
		vbb = list_entry(buffers.next, struct vbb, entry);
		list_del(&vbb->entry);
		vb_submit_rxb(vb, vbb);
	}
	return;
}

/**
 * vb_tasklet_hx8() - When vb->mode == HX8
 *
 * The normal deferred processing routine for the Hx8 boards.  This deferred
 * processing routine doesn't configure any idle buffers and increases the
 * latency when there is a hard underrun.  There are not any softunderruns here,
 * unlike in vb_tasklet_normal.
 */
static void vb_tasklet_hx8(unsigned long data)
{
	struct voicebus *vb = (struct voicebus *)data;
	int hardunderrun;
	_LIST_HEAD(buffers);
	struct vbb *vbb;
	const int DEFAULT_COUNT = 5;
	int count = DEFAULT_COUNT;
	u32 des0 = 0;

	hardunderrun = test_and_clear_bit(VOICEBUS_HARD_UNDERRUN, &vb->flags);
	/* First, temporarily store any non-idle buffers that the hardware has
	 * indicated it's finished transmitting.  Non idle buffers are those
	 * buffers that contain actual data and was filled out by the client
	 * driver (as of this writing, the wcte12xp or wctdm24xxp drivers) when
	 * passed up through the handle_transmit callback.
	 *
	 * On the other hand, idle buffers are "dummy" buffers that solely exist
	 * to in order to prevent the transmit descriptor ring from ever
	 * completely draining. */
	while ((vbb = vb_get_completed_txb(vb)))
		list_add_tail(&vbb->entry, &vb->tx_complete);

	while (--count && !list_empty(&vb->tx_complete))
		list_move_tail(vb->tx_complete.next, &buffers);

	/* Prep all the new buffers for transmit before actually sending any
	 * of them. */
	handle_transmit(vb, &buffers);

	if (unlikely(hardunderrun))
		vb_increase_latency(vb, 1, &buffers);

	/* Now we can send all our buffers together in a group. */
	while (!list_empty(&buffers)) {
		vbb = list_entry(buffers.next, struct vbb, entry);
		list_del(&vbb->entry);
		voicebus_transmit(vb, vbb);
	}

	/* Print any messages about soft latency bumps after we fix the transmit
	 * descriptor ring. Otherwise it's possible to take so much time
	 * printing the dmesg output that we lose the lead that we got on the
	 * hardware, resulting in a hard underrun condition. */
	if (unlikely(hardunderrun)) {
#if !defined(CONFIG_VOICEBUS_SYSFS)
		if (!test_bit(VOICEBUS_LATENCY_LOCKED, &vb->flags) &&
		    printk_ratelimit()) {
			if (vb->max_latency != vb->min_tx_buffer_count) {
				dev_info(&vb->pdev->dev, "Missed interrupt. "
					 "Increasing latency to %d ms in "
					 "order to compensate.\n",
					 vb->min_tx_buffer_count);
			} else {
				dev_info(&vb->pdev->dev, "ERROR: Unable to "
					 "service card within %d ms and "
					 "unable to further increase "
					 "latency.\n", vb->max_latency);
			}
		}
#endif
	}

#if !defined(CONFIG_VOICEBUS_INTERRUPT)
	/* If there may still be buffers in the descriptor rings, reschedule
	 * ourself to run again.  We essentially yield here to allow any other
	 * cards a chance to run. */
	if (unlikely(!count && !test_bit(VOICEBUS_STOP, &vb->flags)))
		vb_schedule_deferred(vb);
#endif

	/* And finally, pass up any receive buffers. */
	count = DEFAULT_COUNT;
	while (--count && (vbb = vb_get_completed_rxb(vb, &des0))) {
		if (((des0 >> 16) & 0x7fff) == VOICEBUS_SFRAME_SIZE)
			list_add_tail(&vbb->entry, &buffers);
		else
			vb_submit_rxb(vb, vbb);
	}

	handle_receive(vb, &buffers);

	while (!list_empty(&buffers)) {
		vbb = list_entry(buffers.next, struct vbb, entry);
		list_del(&vbb->entry);
		vb_submit_rxb(vb, vbb);
	}

	return;
}

/**
 * vb_tasklet_relaxed() - When vb->mode == NORMAL
 *
 * This is the standard deferred processing routine for CPLD based cards
 * (essentially the non-hx8 cards).
 */
static void vb_tasklet_normal(unsigned long data)
{
	struct voicebus *vb = (struct voicebus *)data;
	int softunderrun;
	_LIST_HEAD(buffers);
	struct vbb *vbb;
	struct voicebus_descriptor_list *const dl = &vb->txd;
	struct voicebus_descriptor *d;
	int behind = 0;
	const int DEFAULT_COUNT = 5;
	int count = DEFAULT_COUNT;
	u32 des0 = 0;

	BUG_ON(NORMAL != vb->mode);
	/* First, temporarily store any non-idle buffers that the hardware has
	 * indicated it's finished transmitting.  Non idle buffers are those
	 * buffers that contain actual data and was filled out by the client
	 * driver (as of this writing, the wcte12xp or wctdm24xxp drivers) when
	 * passed up through the handle_transmit callback.
	 *
	 * On the other hand, idle buffers are "dummy" buffers that solely exist
	 * to in order to prevent the transmit descriptor ring from ever
	 * completely draining. */
	while ((vbb = vb_get_completed_txb(vb)))
		list_add_tail(&vbb->entry, &vb->tx_complete);

	if (unlikely(atomic_read(&dl->count) < 2)) {
		softunderrun = 1;
		d = vb_descriptor(dl, dl->head);
		if (1 == atomic_read(&dl->count))
			return;

		behind = 2;
		while (1) {
#if defined(__FreeBSD__)
			bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_POSTREAD);
#endif
			if (OWNED(d))
				break;
			if (d->buffer1 != vb->idle_vbb_dma_addr)
				goto tx_error_exit;
			SET_OWNED(d);
#if defined(__FreeBSD__)
			bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_PREWRITE);
#endif
			dl->head = (dl->head + 1) & DRING_MASK;
			d = vb_descriptor(dl, dl->head);
			++behind;
		}

	} else {
		softunderrun = 0;
	}

	while (--count && !list_empty(&vb->tx_complete))
		list_move_tail(vb->tx_complete.next, &buffers);

	/* Prep all the new buffers for transmit before actually sending any
	 * of them. */
	handle_transmit(vb, &buffers);

#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_POSTREAD);
#endif
	if (unlikely(softunderrun)) {
		int i;
		vb_increase_latency(vb, behind, &buffers);

		d = vb_descriptor(dl, dl->head);
		while (!OWNED(d)) {
			if (d->buffer1 != vb->idle_vbb_dma_addr)
				goto tx_error_exit;
			SET_OWNED(d);
			dl->head = (dl->head + 1) & DRING_MASK;
			d = vb_descriptor(dl, dl->head);
			++behind;
		}
		/* Now we'll get a little further ahead of the hardware. */
		for (i = 0; i < 5; ++i) {
			d = vb_descriptor(dl, dl->head);
			d->buffer1 = vb->idle_vbb_dma_addr;
			dl->pending[dl->head] = vb->idle_vbb;
			d->des0 |= OWN_BIT;
			dl->head = (dl->head + 1) & DRING_MASK;
		}
		dl->tail = dl->head;
	}

	d = vb_descriptor(dl, dl->tail);
	if (d->buffer1 != vb->idle_vbb_dma_addr)
		goto tx_error_exit;

#if defined(__FreeBSD__)
	bus_dmamap_sync(dl->dma_tag, dl->dma_map, BUS_DMASYNC_PREWRITE);
#endif

	/* Now we can send all our buffers together in a group. */
	while (!list_empty(&buffers)) {
		vbb = list_entry(buffers.next, struct vbb, entry);
		list_del(&vbb->entry);
		voicebus_transmit(vb, vbb);
	}

	/* Print any messages about soft latency bumps after we fix the transmit
	 * descriptor ring. Otherwise it's possible to take so much time
	 * printing the dmesg output that we lose the lead that we got on the
	 * hardware, resulting in a hard underrun condition. */
	if (unlikely(softunderrun)) {
#if !defined(CONFIG_VOICEBUS_SYSFS)
		if (!test_bit(VOICEBUS_LATENCY_LOCKED, &vb->flags) &&
		    printk_ratelimit()) {
			if (vb->max_latency != vb->min_tx_buffer_count) {
				dev_info(&vb->pdev->dev, "Missed interrupt. "
					 "Increasing latency to %d ms in "
					 "order to compensate.\n",
					 vb->min_tx_buffer_count);
			} else {
				dev_info(&vb->pdev->dev, "ERROR: Unable to "
					 "service card within %d ms and "
					 "unable to further increase "
					 "latency.\n", vb->max_latency);
			}
		}
#endif
	}

#if !defined(CONFIG_VOICEBUS_INTERRUPT)
	/* If there may still be buffers in the descriptor rings, reschedule
	 * ourself to run again.  We essentially yield here to allow any other
	 * cards a chance to run. */
	if (unlikely(!count && !test_bit(VOICEBUS_STOP, &vb->flags)))
		vb_schedule_deferred(vb);
#endif

	/* And finally, pass up any receive buffers. */
	count = DEFAULT_COUNT;
	while (--count && (vbb = vb_get_completed_rxb(vb, &des0))) {
		if (((des0 >> 16) & 0x7fff) == VOICEBUS_SFRAME_SIZE)
			list_add_tail(&vbb->entry, &buffers);
		else
			vb_submit_rxb(vb, vbb);
	}

	handle_receive(vb, &buffers);

	while (!list_empty(&buffers)) {
		vbb = list_entry(buffers.next, struct vbb, entry);
		list_del(&vbb->entry);
		vb_submit_rxb(vb, vbb);
	}

	return;
tx_error_exit:
	vb_disable_interrupts(vb);
	schedule_work(&vb->underrun_work);
	while (!list_empty(&buffers)) {
		vbb = list_entry(buffers.next, struct vbb, entry);
		list_del(&vbb->entry);
		voicebus_free(vb, vbb);
	}
	return;
}

/**
 * handle_hardunderrun() - reset the AN983 after experiencing a hardunderrun.
 * @work: 	The work_struct used to queue this function.
 *
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void handle_hardunderrun(void *data)
{
	struct voicebus *vb = data;
#else
static void handle_hardunderrun(struct work_struct *work)
{
	struct voicebus *vb = container_of(work, struct voicebus,
					   underrun_work);
#endif
	if (test_bit(VOICEBUS_STOP, &vb->flags) ||
	    test_bit(VOICEBUS_STOPPED, &vb->flags))
		return;

	voicebus_stop(vb);

	if (!test_bit(VOICEBUS_SHUTDOWN, &vb->flags)) {

		if (printk_ratelimit()) {
			dev_info(&vb->pdev->dev, "Host failed to service "
				 "card interrupt within %d ms which is a "
				 "hardunderun.\n", DRING_SIZE);
		}

		if (vb->ops->handle_error)
			vb->ops->handle_error(vb);

		vb_disable_deferred(vb);
		setup_descriptors(vb);
		start_packet_processing(vb);
		vb_enable_deferred(vb);
	}
}

/*!
 * \brief Interrupt handler for VoiceBus interface.
 *
 * NOTE: This handler is optimized for the case where only a single interrupt
 * condition will be generated at a time.
 *
 * ALSO NOTE:  Only access the interrupt status register from this function
 * since it doesn't employ any locking on the voicebus interface.
 */
DAHDI_IRQ_HANDLER(vb_isr)
{
	struct voicebus *vb = dev_id;
	u32 int_status;

#if defined(CONFIG_VOICEBUS_INTERRUPT) && defined(__FreeBSD__)
	if (atomic_read(&vb->deferred_disabled_count))
		return IRQ_NONE;
#endif

	int_status = __vb_getctl(vb, SR_CSR5);
	/* Mask out the reserved bits. */
	int_status &= ~(0xfc004010);
	int_status &= 0x7fff;

	if (!int_status)
		return IRQ_NONE;

	if (unlikely((int_status &
	    (TX_UNAVAILABLE_INTERRUPT|RX_UNAVAILABLE_INTERRUPT)) &&
	    !test_bit(VOICEBUS_STOP, &vb->flags) &&
	    (BOOT != vb->mode))) {
		if (NORMAL == vb->mode) {
			__vb_disable_interrupts(vb);
			__vb_setctl(vb, SR_CSR5, int_status);
			schedule_work(&vb->underrun_work);
		} else if (HX8 == vb->mode) {
			set_bit(VOICEBUS_HARD_UNDERRUN, &vb->flags);
			vb_schedule_deferred(vb);
			__vb_setctl(vb, SR_CSR5, int_status);
		}
	} else if (likely(int_status &
		   (TX_COMPLETE_INTERRUPT|RX_COMPLETE_INTERRUPT))) {
		/* ******************************************************** */
		/* NORMAL INTERRUPT CASE				    */
		/* ******************************************************** */
		vb_schedule_deferred(vb);
		__vb_setctl(vb, SR_CSR5, TX_COMPLETE_INTERRUPT|RX_COMPLETE_INTERRUPT);
	} else {
		if (int_status & FATAL_BUS_ERROR_INTERRUPT)
			dev_err(&vb->pdev->dev, "Fatal Bus Error detected!\n");

		if (int_status & TX_STOPPED_INTERRUPT) {
			BUG_ON(!test_bit(VOICEBUS_STOP, &vb->flags));
			if (__vb_is_stopped(vb)) {
				__vb_disable_interrupts(vb);
			}
		}
		if (int_status & RX_STOPPED_INTERRUPT) {
			BUG_ON(!test_bit(VOICEBUS_STOP, &vb->flags));
			if (__vb_is_stopped(vb)) {
				__vb_disable_interrupts(vb);
			}
		}

		/* Clear the interrupt(s) */
		__vb_setctl(vb, SR_CSR5, int_status);
	}

	return IRQ_HANDLED;
}

#if defined(CONFIG_VOICEBUS_TIMER)
/*! \brief Called if the deferred processing is to happen in the context of
 * the timer.
 */
static void
vb_timer(unsigned long data)
{
	unsigned long start = jiffies;
	struct voicebus *vb = (struct voicebus *)data;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	vb_isr(0, vb, 0);
#else
	vb_isr(0, vb);
#endif
	if (!test_bit(VOICEBUS_STOPPED, &vb->flags)) {
		vb->timer.expires = start + HZ/1000;
		add_timer(&vb->timer);
	}
}
#endif

/*!
 * \brief Initalize the voicebus interface.
 *
 * This function must be called in process context since it may sleep.
 * \todo Complete this description.
 */
int
__voicebus_init(struct voicebus *vb, const char *board_name,
		enum voicebus_mode mode)
{
	int retval = 0;

	BUG_ON(NULL == vb);
	BUG_ON(NULL == board_name);
	BUG_ON(NULL == vb->ops);
	BUG_ON(NULL == vb->pdev);
	BUG_ON(NULL == vb->debug);

	/* ----------------------------------------------------------------
	   Initialize the pure software constructs.
	   ---------------------------------------------------------------- */
	vb->max_latency = VOICEBUS_DEFAULT_MAXLATENCY;

	spin_lock_init(&vb->lock);
	set_bit(VOICEBUS_STOP, &vb->flags);

	if ((NORMAL != mode) && (BOOT != mode) && (HX8 != mode))
		return -EINVAL;

	vb->mode = mode;

	vb->min_tx_buffer_count = VOICEBUS_DEFAULT_LATENCY;

	INIT_LIST_HEAD(&vb->tx_complete);
	INIT_LIST_HEAD(&vb->free_rx);

#if defined(CONFIG_VOICEBUS_TIMER)
	init_timer(&vb->timer);
	vb->timer.function = vb_timer;
	vb->timer.data = (unsigned long)vb;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&vb->underrun_work, handle_hardunderrun, vb);
#else
	INIT_WORK(&vb->underrun_work, handle_hardunderrun);
#endif

#if defined(__FreeBSD__)
	retval = bus_dma_tag_create(bus_get_dma_tag(vb->pdev->dev), 8, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct vbb), 1, sizeof(struct vbb), BUS_DMA_ALLOCNOW, NULL, NULL, &vb->dma_tag);
	if (retval) {
		device_printf(vb->pdev->dev, "Can't allocate DMA tag for voicebus frames\n");
		goto cleanup;
	}

	vb->mem_rid = PCIR_BAR(1);
	vb->mem_res = bus_alloc_resource_any(vb->pdev->dev, SYS_RES_MEMORY, &vb->mem_rid, RF_ACTIVE);
	if (vb->mem_res == NULL) {
		device_printf(vb->pdev->dev, "Can't allocate memory resource\n");
		retval = -ENXIO;
		goto cleanup;
	}
#else /* !__FreeBSD__ */
	/* ----------------------------------------------------------------
	   Configure the hardware / kernel module interfaces.
	   ---------------------------------------------------------------- */
	if (pci_set_dma_mask(vb->pdev, DMA_BIT_MASK(32))) {
		dev_err(&vb->pdev->dev, "No suitable DMA available.\n");
		goto cleanup;
	}

	if (pci_enable_device(vb->pdev)) {
		dev_err(&vb->pdev->dev, "Failed call to pci_enable_device.\n");
		retval = -EIO;
		goto cleanup;
	}

	/* \todo This driver should be modified to use the memory mapped I/O
	   as opposed to IO space for portability and performance. */
	if (0 == (pci_resource_flags(vb->pdev, 0)&IORESOURCE_IO)) {
		dev_err(&vb->pdev->dev, "BAR0 is not IO Memory.\n");
		retval = -EIO;
		goto cleanup;
	}
	vb->iobase = pci_iomap(vb->pdev, 1, 0);
	if (!request_mem_region(pci_resource_start(vb->pdev, 1),
	    pci_resource_len(vb->pdev, 1), board_name)) {
		dev_err(&vb->pdev->dev, "IO Registers are in use by another "
			"module.\n");
		retval = -EIO;
		goto cleanup;
	}
#endif

#if defined(__FreeBSD__)
	retval = dahdi_dma_allocate(vb->pdev->dev, VOICEBUS_SFRAME_SIZE,
	    &vb->idle_vbb_dma_tag, &vb->idle_vbb_dma_map, (void **) &vb->idle_vbb, &vb->idle_vbb_dma_addr);
	if (retval) {
		dev_err(&vb->pdev->dev, "Can't allocate DMA memory\n");
		goto cleanup;
	}
#else /* !__FreeBSD__ */
	vb->idle_vbb = dma_alloc_coherent(&vb->pdev->dev, VOICEBUS_SFRAME_SIZE,
					  &vb->idle_vbb_dma_addr, GFP_KERNEL);
#endif

#if !defined(__FreeBSD__)
	/* ----------------------------------------------------------------
	   Configure the hardware interface.
	   ---------------------------------------------------------------- */
	if (pci_set_dma_mask(vb->pdev, DMA_BIT_MASK(32))) {
		release_mem_region(pci_resource_start(vb->pdev, 1),
			pci_resource_len(vb->pdev, 1));
		dev_warn(&vb->pdev->dev, "No suitable DMA available.\n");
		goto cleanup;
	}

	pci_set_master(vb->pdev);
#endif /* !__FreeBSD__ */

	vb_enable_io_access(vb);

	if (vb_reset_interface(vb)) {
		retval = -EIO;
		dev_warn(&vb->pdev->dev, "Failed reset.\n");
		goto cleanup;
	}

	retval = vb_initialize_tx_descriptors(vb);
	if (retval)
		goto cleanup;

	retval = vb_initialize_rx_descriptors(vb);
	if (retval)
		goto cleanup;

#if !defined(CONFIG_VOICEBUS_TIMER)
#if defined(__FreeBSD__)
	vb->irq_res = bus_alloc_resource_any(
	    vb->pdev->dev, SYS_RES_IRQ, &vb->irq_rid, RF_SHAREABLE | RF_ACTIVE);
	if (vb->irq_res == NULL) {
		dev_err(&vb->pdev->dev, "Can't allocate IRQ resource\n");
		retval = -ENXIO;
		goto cleanup;
	}

	retval = bus_setup_intr(
	    vb->pdev->dev, vb->irq_res, INTR_TYPE_CLK | INTR_MPSAFE,
	    vb_isr, NULL, vb, &vb->irq_handle);
	if (retval) {
		dev_err(&vb->pdev->dev, "Can't setup interrupt handler (error %d)\n", retval);
		goto cleanup;
	}
#else /* !__FreeBSD__ */
	retval = request_irq(vb->pdev->irq, vb_isr, DAHDI_IRQ_SHARED,
			     board_name, vb);
	if (retval) {
		dev_warn(&vb->pdev->dev, "Failed to request interrupt line.\n");
		goto cleanup;
	}
#endif /* !__FreeBSD__ */
#endif

#ifdef VOICEBUS_NET_DEBUG
	vb_net_register(vb, board_name);
#endif
	return retval;
cleanup:

	tasklet_kill(&vb->tasklet);

#if defined(__FreeBSD__)
#if !defined(CONFIG_VOICEBUS_TIMER)
	if (vb->irq_handle != NULL) {
		bus_teardown_intr(vb->pdev->dev, vb->irq_res, vb->irq_handle);
		vb->irq_handle = NULL;
	}

	if (vb->irq_res != NULL) {
		bus_release_resource(vb->pdev->dev, SYS_RES_IRQ, vb->irq_rid, vb->irq_res);
		vb->irq_res = NULL;
	}
#endif

	/* Cleanup memory and software resources. */
	if (vb->txd.desc)
		vb_free_descriptors(vb, &vb->txd);

	if (vb->rxd.desc)
		vb_free_descriptors(vb, &vb->rxd);

	dahdi_dma_free(&vb->idle_vbb_dma_tag, &vb->idle_vbb_dma_map,
	    (void **) &vb->idle_vbb, &vb->idle_vbb_dma_addr);

	if (vb->mem_res != NULL) {
		bus_release_resource(vb->pdev->dev, SYS_RES_MEMORY, vb->mem_rid, vb->mem_res);
		vb->mem_res = NULL;
	}

	if (vb->dma_tag != NULL) {
		bus_dma_tag_destroy(vb->dma_tag);
		vb->dma_tag = NULL;
	}
#else /* !__FreeBSD__ */
	dma_free_coherent(&vb->pdev->dev, VOICEBUS_SFRAME_SIZE,
			  vb->idle_vbb, vb->idle_vbb_dma_addr);

	if (vb->iobase)
		pci_iounmap(vb->pdev, vb->iobase);

	if (vb->pdev)
		pci_disable_device(vb->pdev);
#endif /* !__FreeBSD__ */

	if (0 == retval)
		retval = -EIO;
	return retval;
}
EXPORT_SYMBOL(__voicebus_init);

static spinlock_t loader_list_lock;
static _LIST_HEAD(binary_loader_list);

/**
 * vpmadtreg_loadfirmware - Load the vpmadt032 firmware.
 * @vb: The voicebus device to load.
 */
int vpmadtreg_loadfirmware(struct voicebus *vb)
{
	struct vpmadt_loader *loader;
	int ret = 0;
	int loader_present = 0;
	unsigned long stop;
	might_sleep();

	/* First check to see if a loader is already loaded into memory. */
	spin_lock(&loader_list_lock);
	loader_present = !(list_empty(&binary_loader_list));
	spin_unlock(&loader_list_lock);

	if (!loader_present) {
#if defined(__FreeBSD__) || LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
		ret = request_module("dahdi_vpmadt032_loader");
#else
		/* If we use the blocking 'request_module' here and we are
		 * loading the client boards with async_schedule we will hang
		 * here. The module loader will wait for our asynchronous tasks
		 * to finish, but we can't because we're waiting for the load
		 * the finish. */
		ret = request_module_nowait("dahdi_vpmadt032_loader");
#endif
		if (ret)
			return ret;
		stop = jiffies + HZ;
		while (time_after(stop, jiffies)) {
			spin_lock(&loader_list_lock);
			loader_present = !(list_empty(&binary_loader_list));
			spin_unlock(&loader_list_lock);
			if (loader_present)
				break;
			msleep(10);
		}
	}

	spin_lock(&loader_list_lock);
	if (!list_empty(&binary_loader_list)) {
		loader = list_entry(binary_loader_list.next,
				struct vpmadt_loader, node);
		if (try_module_get(loader->owner)) {
			spin_unlock(&loader_list_lock);
			ret = loader->load(vb);
			module_put(loader->owner);
		} else {
			spin_unlock(&loader_list_lock);
			dev_info(&vb->pdev->dev, "Failed to find a "
				 "registered loader after loading module.\n");
			ret = -ENODEV;
		}
	} else {
		spin_unlock(&loader_list_lock);
		dev_info(&vb->pdev->dev, "Failed to find a registered "
			 "loader after loading module.\n");
		ret = -ENODEV;
	}
	return ret;
}

/* Called by the binary loader module when it is ready to start loading
 * firmware. */
int vpmadtreg_register(struct vpmadt_loader *loader)
{
	spin_lock(&loader_list_lock);
	list_add_tail(&loader->node, &binary_loader_list);
	spin_unlock(&loader_list_lock);
	return 0;
}
EXPORT_SYMBOL(vpmadtreg_register);

int vpmadtreg_unregister(struct vpmadt_loader *loader)
{
	int removed = 0;
	struct vpmadt_loader *cur, *temp;
	list_for_each_entry_safe(cur, temp, &binary_loader_list, node) {
		if (loader == cur) {
			list_del_init(&cur->node);
			removed = 1;
			break;
		}
	}
	WARN_ON(!removed);
	return 0;
}
EXPORT_SYMBOL(vpmadtreg_unregister);

static int __init voicebus_module_init(void)
{
	int res;

#if !defined(__FreeBSD__)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	voicebus_vbb_cache = kmem_cache_create(THIS_MODULE->name,
					 sizeof(struct vbb), 0,
#if defined(CONFIG_SLUB) && (LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 22))
					 SLAB_HWCACHE_ALIGN |
					 SLAB_STORE_USER, NULL,
					 NULL);
#else
					 SLAB_HWCACHE_ALIGN, NULL,
					 NULL);
#endif
#else
#if (defined(CONFIG_SLAB) && defined(CONFIG_SLAB_DEBUG)) || \
    (defined(CONFIG_SLUB) && defined(CONFIG_SLUB_DEBUG))
	voicebus_vbb_cache = kmem_cache_create(THIS_MODULE->name,
					 sizeof(struct vbb), 0,
					 SLAB_HWCACHE_ALIGN | SLAB_STORE_USER |
					 SLAB_POISON | SLAB_DEBUG_FREE, NULL);
#else
	voicebus_vbb_cache = kmem_cache_create(THIS_MODULE->name,
					 sizeof(struct vbb), 0,
					 SLAB_HWCACHE_ALIGN, NULL);
#endif
#endif

	if (NULL == voicebus_vbb_cache) {
		printk(KERN_ERR "%s: Failed to allocate buffer cache.\n",
		       THIS_MODULE->name);
		return -ENOMEM;
	}
#endif /* !__FreeBSD__ */

	/* This registration with dahdi.ko will fail since the span is not
	 * defined, but it will make sure that this module is a dependency of
	 * dahdi.ko, so that when it is being unloded, this module will be
	 * unloaded as well. */
	dahdi_register(NULL, 0);
	spin_lock_init(&loader_list_lock);
	res = vpmadt032_module_init();
	if (res)
		return res;
	return 0;
}

static void __exit voicebus_module_cleanup(void)
{
#if !defined(__FreeBSD__)
	kmem_cache_destroy(voicebus_vbb_cache);
#endif
	spin_lock_destroy(&loader_list_lock);
	WARN_ON(!list_empty(&binary_loader_list));
}

#if defined(__FreeBSD__)
static int
dahdi_voicebus_modevent(module_t mod __unused, int type, void *data __unused)
{
	int res;

	switch (type) {
	case MOD_LOAD:
		res = voicebus_module_init();
		return -res;
	case MOD_UNLOAD:
		voicebus_module_cleanup();
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

DAHDI_DEV_MODULE(dahdi_voicebus, dahdi_voicebus_modevent, NULL);
MODULE_VERSION(dahdi_voicebus, 1);
MODULE_DEPEND(dahdi_voicebus, dahdi, 1, 1, 1);
#else /* !__FreeBSD__ */
MODULE_DESCRIPTION("Voicebus Interface w/VPMADT032 support");
MODULE_AUTHOR("Digium Incorporated <support@digium.com>");
MODULE_LICENSE("GPL");

module_init(voicebus_module_init);
module_exit(voicebus_module_cleanup);
#endif /* !__FreeBSD__ */

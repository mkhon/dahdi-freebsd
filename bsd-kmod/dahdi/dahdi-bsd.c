/*-
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Max Khon under sponsorship from
 * the FreeBSD Foundation.
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

#include <dahdi/kernel.h>
#include <linux/mod_devicetable.h>

#include <sys/bus.h>
#include <sys/kernel.h>
#include <dev/pci/pcivar.h>

MALLOC_DEFINE(M_DAHDI, "dahdi", "DAHDI interface data structures");

SYSCTL_NODE(, OID_AUTO, dahdi, CTLFLAG_RW, 0, "DAHDI");
SYSCTL_NODE(_dahdi, OID_AUTO, echocan, CTLFLAG_RW, 0, "DAHDI Echo Cancelers");

void
rlprintf(int pps, const char *fmt, ...)
{
	va_list ap;
	static struct timeval last_printf;
	static int count;

	if (ppsratecheck(&last_printf, &count, pps)) {
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

void
device_rlprintf(int pps, device_t dev, const char *fmt, ...)
{
	va_list ap;
	static struct timeval last_printf;
	static int count;

	if (ppsratecheck(&last_printf, &count, pps)) {
		va_start(ap, fmt);
		device_print_prettyname(dev);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

void
dahdi_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *paddr = arg;
	*paddr = segs->ds_addr;
}

int
dahdi_dma_allocate(device_t dev, int size, bus_dma_tag_t *ptag, bus_dmamap_t *pmap, void **pvaddr, bus_addr_t *ppaddr)
{
	int res;

	res = bus_dma_tag_create(bus_get_dma_tag(dev), 8, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    size, 1, size, BUS_DMA_ALLOCNOW, NULL, NULL, ptag);
	if (res)
		return (-res);

	res = bus_dmamem_alloc(*ptag, pvaddr, BUS_DMA_NOWAIT | BUS_DMA_ZERO, pmap);
	if (res) {
		bus_dma_tag_destroy(*ptag);
		*ptag = NULL;
		return (-res);
	}

	res = bus_dmamap_load(*ptag, *pmap, *pvaddr, size, dahdi_dma_map_addr, ppaddr, 0);
	if (res) {
		bus_dmamem_free(*ptag, *pvaddr, *pmap);
		*pvaddr = NULL;

		bus_dmamap_destroy(*ptag, *pmap);
		*pmap = NULL;

		bus_dma_tag_destroy(*ptag);
		*ptag = NULL;
		return (-res);
	}

	return (0);
}

void
dahdi_dma_free(bus_dma_tag_t *ptag, bus_dmamap_t *pmap, void **pvaddr, bus_addr_t *ppaddr)
{
	if (*ppaddr != 0) {
		bus_dmamap_unload(*ptag, *pmap);
		*ppaddr = 0;
	}
	if (*pvaddr != NULL) {
		bus_dmamem_free(*ptag, *pvaddr, *pmap);
		*pvaddr = NULL;

		bus_dmamap_destroy(*ptag, *pmap);
		*pmap = NULL;
	}
	if (*ptag != NULL) {
		bus_dma_tag_destroy(*ptag);
		*ptag = NULL;
	}
}

const struct pci_device_id *
dahdi_pci_device_id_lookup(device_t dev, const struct pci_device_id *tbl)
{
	const struct pci_device_id *id;
	uint16_t vendor = pci_get_vendor(dev);
	uint16_t device = pci_get_device(dev);
	uint16_t subvendor = pci_get_subvendor(dev);
	uint16_t subdevice = pci_get_subdevice(dev);

	for (id = tbl; id->vendor != 0; id++) {
		if ((id->vendor == PCI_ANY_ID || id->vendor == vendor) &&
		    (id->device == PCI_ANY_ID || id->device == device) &&
		    (id->subvendor == PCI_ANY_ID || id->subvendor == subvendor) &&
		    (id->subdevice == PCI_ANY_ID || id->subdevice == subdevice))
			return id;
	}

	return NULL;
}

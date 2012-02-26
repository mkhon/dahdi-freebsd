#ifndef _LINUX_PCI_H_
#define _LINUX_PCI_H_

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/irqreturn.h>
#include <asm/atomic.h>

#include <linux/slab.h>		/* asm/pci.h */

#include <sys/bus.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

struct pci_dev {
	device_t dev;
};

#endif /* _LINUX_PCI_H_ */

#ifndef _LINUX_PCI_H_
#define _LINUX_PCI_H_

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/irqreturn.h>
#include <linux/pci_regs.h>
#include <asm/atomic.h>

#include <linux/slab.h>		/* asm/pci.h */

#include <sys/bus.h>
#include <dev/pci/pcivar.h>

struct pci_dev {
	struct device dev;
};

static inline int pci_read_config_byte(struct pci_dev *pci_dev, int where, u8 *val)
{
	*val = pci_read_config(pci_dev->dev.device, where, 1);
	return (0);
}

#endif /* _LINUX_PCI_H_ */

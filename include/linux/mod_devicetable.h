#ifndef _LINUX_MOD_DEVICETABLE_H_
#define _LINUX_MOD_DEVICETABLE_H_

#define PCI_ANY_ID (~0)

struct pci_device_id {
	uint32_t vendor, device;	/* Vendor and device ID or PCI_ANY_ID*/
	uint32_t subvendor, subdevice;	/* Subsystem ID's or PCI_ANY_ID */
	uint32_t class, class_mask;	/* (class,subclass,prog-if) triplet */
	unsigned long driver_data;	/* Data private to the driver */
};

#endif /* _LINUX_MOD_DEVICETABLE_H_ */

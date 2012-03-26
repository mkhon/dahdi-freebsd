#ifndef _LINUX_DEVICE_H_
#define _LINUX_DEVICE_H_

#include <linux/module.h>
#include <linux/list.h>		/* linux/kobject.h */
#include <linux/kref.h>		/* linux/klist.h */
#include <linux/semaphore.h>

struct device {
	device_t device;
};

#define dev_printk(level, dev, fmt, args...)	device_printf((dev)->device, level fmt, ##args)

#define dev_err(dev, fmt, args...)	dev_printk(KERN_ERR, dev, fmt, ##args)
#define dev_warn(dev, fmt, args...)	dev_printk(KERN_WARNING, dev, fmt, ##args)
#define dev_notice(dev, fmt, args...)	dev_printk(KERN_NOTICE, dev, fmt, ##args)
#define dev_info(dev, fmt, args...)	dev_printk(KERN_INFO, dev, fmt, ##args)
#define dev_dbg(dev, fmt, args...)	dev_printk(KERN_DEBUG, dev, fmt, ##args)

#endif /* _LINUX_DEVICE_H_ */

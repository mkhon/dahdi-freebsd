#ifndef _LINUX_DEVICE_H_
#define _LINUX_DEVICE_H_

#include <linux/module.h>
#include <linux/list.h>		/* linux/kobject.h */
#include <linux/semaphore.h>

#define dev_err(dev, fmt, args...)	device_printf(*(dev), fmt, ##args)
#define dev_warn(dev, fmt, args...)	device_printf(*(dev), fmt, ##args)
#define dev_notice(dev, fmt, args...)	device_printf(*(dev), fmt, ##args)
#define dev_info(dev, fmt, args...)	device_printf(*(dev), fmt, ##args)
#define dev_dbg(dev, fmt, args...)	device_printf(*(dev), fmt, ##args)

#endif /* _LINUX_DEVICE_H_ */

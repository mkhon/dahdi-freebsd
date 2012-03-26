/* dahdi-sysfs.c
 *
 * Copyright (C) 2011-2012, Xorcom
 * Copyright (C) 2011-2012, Digium, Inc.
 *
 * All rights reserved.
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

#include <linux/kernel.h>
#include <linux/version.h>
#define DAHDI_PRINK_MACROS_USE_debug
#include <dahdi/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>

#include "dahdi.h"
#include "dahdi-sysfs.h"


static char *initdir = "/usr/share/dahdi";
module_param(initdir, charp, 0644);

static int span_match(struct device *dev, struct device_driver *driver)
{
	return 1;
}

static inline struct dahdi_span *dev_to_span(struct device *dev)
{
	return dev_get_drvdata(dev);
}

#define	SPAN_VAR_BLOCK	\
	do {		\
		DAHDI_ADD_UEVENT_VAR("DAHDI_INIT_DIR=%s", initdir);	\
		DAHDI_ADD_UEVENT_VAR("SPAN_NUM=%d", span->spanno);	\
		DAHDI_ADD_UEVENT_VAR("SPAN_NAME=%s", span->name);	\
	} while (0)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
#define DAHDI_ADD_UEVENT_VAR(fmt, val...)			\
	do {							\
		int err = add_uevent_var(envp, num_envp, &i,	\
				buffer, buffer_size, &len,	\
				fmt, val);			\
		if (err)					\
			return err;				\
	} while (0)

static int span_uevent(struct device *dev, char **envp, int num_envp,
		char *buffer, int buffer_size)
{
	struct dahdi_span	*span;
	int			i = 0;
	int			len = 0;

	if (!dev)
		return -ENODEV;

	span = dev_to_span(dev);
	if (!span)
		return -ENODEV;

	dahdi_dbg(GENERAL, "SYFS dev_name=%s span=%s\n",
			dev_name(dev), span->name);
	SPAN_VAR_BLOCK;
	envp[i] = NULL;
	return 0;
}

#else
#define DAHDI_ADD_UEVENT_VAR(fmt, val...)			\
	do {							\
		int err = add_uevent_var(kenv, fmt, val);	\
		if (err)					\
			return err;				\
	} while (0)

static int span_uevent(struct device *dev, struct kobj_uevent_env *kenv)
{
	struct dahdi_span *span;

	if (!dev)
		return -ENODEV;
	span = dev_to_span(dev);
	if (!span)
		return -ENODEV;
	dahdi_dbg(GENERAL, "SYFS dev_name=%s span=%s\n",
			dev_name(dev), span->name);
	SPAN_VAR_BLOCK;
	return 0;
}

#endif

#define span_attr(field, format_string)				\
static BUS_ATTR_READER(field##_show, dev, buf)			\
{								\
	struct dahdi_span *span;				\
								\
	span = dev_to_span(dev);				\
	return sprintf(buf, format_string, span->field);	\
}

span_attr(name, "%s\n");
span_attr(desc, "%s\n");
span_attr(spantype, "%s\n");
span_attr(alarms, "0x%x\n");
span_attr(lbo, "%d\n");
span_attr(syncsrc, "%d\n");

static BUS_ATTR_READER(local_spanno_show, dev, buf)
{
	struct dahdi_span *span;

	span = dev_to_span(dev);
	return sprintf(buf, "%d\n", local_spanno(span));
}

static BUS_ATTR_READER(is_digital_show, dev, buf)
{
	struct dahdi_span *span;

	span = dev_to_span(dev);
	return sprintf(buf, "%d\n", dahdi_is_digital_span(span));
}

static BUS_ATTR_READER(is_sync_master_show, dev, buf)
{
	struct dahdi_span *span;

	span = dev_to_span(dev);
	return sprintf(buf, "%d\n", dahdi_is_sync_master(span));
}

static BUS_ATTR_READER(basechan_show, dev, buf)
{
	struct dahdi_span *span;

	span = dev_to_span(dev);
	if (!span->channels)
		return -ENODEV;
	return sprintf(buf, "%d\n", span->chans[0]->channo);
}

static BUS_ATTR_READER(channels_show, dev, buf)
{
	struct dahdi_span *span;

	span = dev_to_span(dev);
	return sprintf(buf, "%d\n", span->channels);
}

static BUS_ATTR_READER(lineconfig_show, dev, buf)
{
	struct dahdi_span *span;
	int len = 0;

	span = dev_to_span(dev);
	if (span->lineconfig) {
		/* framing first */
		if (span->lineconfig & DAHDI_CONFIG_B8ZS)
			len += sprintf(buf + len, "B8ZS/");
		else if (span->lineconfig & DAHDI_CONFIG_AMI)
			len += sprintf(buf + len, "AMI/");
		else if (span->lineconfig & DAHDI_CONFIG_HDB3)
			len += sprintf(buf + len, "HDB3/");
		/* then coding */
		if (span->lineconfig & DAHDI_CONFIG_ESF)
			len += sprintf(buf + len, "ESF");
		else if (span->lineconfig & DAHDI_CONFIG_D4)
			len += sprintf(buf + len, "D4");
		else if (span->lineconfig & DAHDI_CONFIG_CCS)
			len += sprintf(buf + len, "CCS");
		/* E1's can enable CRC checking */
		if (span->lineconfig & DAHDI_CONFIG_CRC4)
			len += sprintf(buf + len, "/CRC4");
	}
	len += sprintf(buf + len, "\n");
	return len;
}

static struct device_attribute span_dev_attrs[] = {
	__ATTR_RO(name),
	__ATTR_RO(desc),
	__ATTR_RO(spantype),
	__ATTR_RO(local_spanno),
	__ATTR_RO(alarms),
	__ATTR_RO(lbo),
	__ATTR_RO(syncsrc),
	__ATTR_RO(is_digital),
	__ATTR_RO(is_sync_master),
	__ATTR_RO(basechan),
	__ATTR_RO(channels),
	__ATTR_RO(lineconfig),
	__ATTR_NULL,
};

static struct driver_attribute dahdi_attrs[] = {
	__ATTR_NULL,
};

static struct bus_type spans_bus_type = {
	.name           = "dahdi_spans",
	.match          = span_match,
	.uevent         = span_uevent,
	.dev_attrs	= span_dev_attrs,
	.drv_attrs	= dahdi_attrs,
};

static int span_probe(struct device *dev)
{
	struct dahdi_span *span;

	span = dev_to_span(dev);
	span_dbg(DEVICES, span, "\n");
	return 0;
}

static int span_remove(struct device *dev)
{
	struct dahdi_span *span;

	span = dev_to_span(dev);
	span_dbg(DEVICES, span, "\n");
	return 0;
}

static struct device_driver dahdi_driver = {
	.name		= "generic_lowlevel",
	.bus		= &spans_bus_type,
	.probe		= span_probe,
	.remove		= span_remove,
	.owner		= THIS_MODULE
};

static void span_uevent_send(struct dahdi_span *span, enum kobject_action act)
{
	struct kobject	*kobj;

	kobj = &span->span_device->kobj;
	span_dbg(DEVICES, span, "SYFS dev_name=%s action=%d\n",
		dev_name(span->span_device), act);
	kobject_uevent(kobj, act);
}

static void span_release(struct device *dev)
{
	dahdi_dbg(DEVICES, "%s: %s\n", __func__, dev_name(dev));
}

void span_sysfs_remove(struct dahdi_span *span)
{
	struct device *span_device;
	int x;

	span_dbg(DEVICES, span, "\n");
	span_device = span->span_device;

	if (!span_device)
		return;

	for (x = 0; x < span->channels; x++)
		chan_sysfs_remove(span->chans[x]);
	if (!dev_get_drvdata(span_device))
		return;

	/* Grab an extra reference to the device since we'll still want it
	 * after we've unregistered it */

	get_device(span_device);
	span_uevent_send(span, KOBJ_OFFLINE);
	device_unregister(span->span_device);
	dev_set_drvdata(span_device, NULL);
	span_device->parent = NULL;
	put_device(span_device);
	memset(&span->span_device, 0, sizeof(span->span_device));
	kfree(span->span_device);
	span->span_device = NULL;
}

int span_sysfs_create(struct dahdi_span *span)
{
	struct device *span_device;
	int res = 0;
	int x;

	if (span->span_device) {
		WARN_ON(1);
		return -EEXIST;
	}

	span->span_device = kzalloc(sizeof(*span->span_device), GFP_KERNEL);
	if (!span->span_device)
		return -ENOMEM;

	span_device = span->span_device;
	span_dbg(DEVICES, span, "\n");

	span_device->bus = &spans_bus_type;
	span_device->parent = &span->parent->dev;
	dev_set_name(span_device, "span-%d", span->spanno);
	dev_set_drvdata(span_device, span);
	span_device->release = span_release;
	res = device_register(span_device);
	if (res) {
		span_err(span, "%s: device_register failed: %d\n", __func__,
				res);
		kfree(span->span_device);
		span->span_device = NULL;
		goto cleanup;
	}

	for (x = 0; x < span->channels; x++) {
		res = chan_sysfs_create(span->chans[x]);
		if (res)
			goto cleanup;
	}
	return 0;

cleanup:
	span_sysfs_remove(span);
	return res;
}

/* Only used to flag that the device exists: */
static struct {
	unsigned int clean_dahdi_driver:1;
	unsigned int clean_span_bus_type:1;
	unsigned int clean_device_bus:1;
	unsigned int clean_chardev:1;
} should_cleanup;

static inline struct dahdi_device *to_ddev(struct device *dev)
{
	return container_of(dev, struct dahdi_device, dev);
}

static ssize_t
dahdi_device_manufacturer_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct dahdi_device *ddev = to_ddev(dev);
	return sprintf(buf, "%s\n", ddev->manufacturer);
}

static ssize_t
dahdi_device_type_show(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	struct dahdi_device *ddev = to_ddev(dev);
	return sprintf(buf, "%s\n", ddev->devicetype);
}

static ssize_t
dahdi_device_span_count_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct dahdi_device *ddev = to_ddev(dev);
	unsigned int count = 0;
	struct list_head *pos;

	list_for_each(pos, &ddev->spans)
		++count;

	return sprintf(buf, "%d\n", count);
}

static ssize_t
dahdi_device_hardware_id_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct dahdi_device *ddev = to_ddev(dev);

	return sprintf(buf, "%s\n",
		(ddev->hardware_id) ? ddev->hardware_id : "");
}

static ssize_t
dahdi_device_auto_assign(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct dahdi_device *ddev = to_ddev(dev);
	dahdi_assign_device_spans(ddev);
	return count;
}

static ssize_t
dahdi_device_assign_span(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int ret;
	struct dahdi_span *span;
	unsigned int local_span_number;
	unsigned int desired_spanno;
	unsigned int desired_basechanno;
	struct dahdi_device *const ddev = to_ddev(dev);

	ret = sscanf(buf, "%u:%u:%u", &local_span_number, &desired_spanno,
		     &desired_basechanno);
	if (ret != 3) {
		dev_notice(dev, "bad input (should be <num>:<num>:<num>)\n");
		return -EINVAL;
	}

	if (desired_spanno && !desired_basechanno) {
		dev_notice(dev, "Must set span number AND base chan number\n");
		return -EINVAL;
	}

	list_for_each_entry(span, &ddev->spans, device_node) {
		if (local_span_number == local_spanno(span)) {
			ret = dahdi_assign_span(span, desired_spanno,
						desired_basechanno, 1);
			return (ret) ? ret : count;
		}
	}
	dev_notice(dev, "no match for local span number %d\n",
		local_span_number);
	return -EINVAL;
}

static ssize_t
dahdi_device_unassign_span(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int ret;
	unsigned int local_span_number;
	struct dahdi_span *span;
	struct dahdi_device *const ddev = to_ddev(dev);

	ret = sscanf(buf, "%u", &local_span_number);
	if (ret != 1)
		return -EINVAL;

	ret = -ENODEV;
	list_for_each_entry(span, &ddev->spans, device_node) {
		if (local_span_number == local_spanno(span))
			ret = dahdi_unassign_span(span);
	}
	if (-ENODEV == ret) {
		if (printk_ratelimit()) {
			dev_info(dev, "'%d' is an invalid local span number.\n",
				 local_span_number);
		}
		return -EINVAL;
	}
	return (ret < 0) ? ret : count;
}

static ssize_t
dahdi_spantype_show(struct device *dev,
		    struct device_attribute *attr, char *buf)
{
	struct dahdi_device *ddev = to_ddev(dev);
	int count = 0;
	ssize_t total = 0;
	struct dahdi_span *span;

	/* TODO: Make sure this doesn't overflow the page. */
	list_for_each_entry(span, &ddev->spans, device_node) {
		count = sprintf(buf, "%d:%s\n",
			local_spanno(span), span->spantype);
		buf += count;
		total += count;
	}

	return total;
}

static ssize_t
dahdi_spantype_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct dahdi_device *const ddev = to_ddev(dev);
	int ret;
	struct dahdi_span *span;
	unsigned int local_span_number;
	char desired_spantype[80];

	ret = sscanf(buf, "%u:%70s", &local_span_number, desired_spantype);
	if (ret != 2)
		return -EINVAL;

	list_for_each_entry(span, &ddev->spans, device_node) {
		if (local_spanno(span) == local_span_number)
			break;
	}

	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &span->flags)) {
		module_printk(KERN_WARNING, "Span %s is already assigned.\n",
			      span->name);
		return -EINVAL;
	}

	if (local_spanno(span) != local_span_number) {
		module_printk(KERN_WARNING,
				"%d is not a valid local span number "
				"for this device.\n", local_span_number);
		return -EINVAL;
	}

	if (!span->ops->set_spantype) {
		module_printk(KERN_WARNING, "Span %s does not support "
			      "setting type.\n", span->name);
		return -EINVAL;
	}

	ret = span->ops->set_spantype(span, &desired_spantype[0]);
	return (ret < 0) ? ret : count;
}

static struct device_attribute dahdi_device_attrs[] = {
	__ATTR(manufacturer, S_IRUGO, dahdi_device_manufacturer_show, NULL),
	__ATTR(type, S_IRUGO, dahdi_device_type_show, NULL),
	__ATTR(span_count, S_IRUGO, dahdi_device_span_count_show, NULL),
	__ATTR(hardware_id, S_IRUGO, dahdi_device_hardware_id_show, NULL),
	__ATTR(auto_assign, S_IWUSR, NULL, dahdi_device_auto_assign),
	__ATTR(assign_span, S_IWUSR, NULL, dahdi_device_assign_span),
	__ATTR(unassign_span, S_IWUSR, NULL, dahdi_device_unassign_span),
	__ATTR(spantype, S_IWUSR | S_IRUGO, dahdi_spantype_show,
	       dahdi_spantype_store),
	__ATTR_NULL,
};

static struct bus_type dahdi_device_bus = {
	.name = "dahdi_devices",
	.dev_attrs = dahdi_device_attrs,
};

static void dahdi_sysfs_cleanup(void)
{
	dahdi_dbg(DEVICES, "SYSFS\n");
	if (should_cleanup.clean_dahdi_driver) {
		dahdi_dbg(DEVICES, "Unregister driver\n");
		driver_unregister(&dahdi_driver);
		should_cleanup.clean_dahdi_driver = 0;
	}
	if (should_cleanup.clean_span_bus_type) {
		dahdi_dbg(DEVICES, "Unregister span bus type\n");
		bus_unregister(&spans_bus_type);
		should_cleanup.clean_span_bus_type = 0;
	}
	dahdi_sysfs_chan_exit();
	if (should_cleanup.clean_chardev) {
		dahdi_dbg(DEVICES, "Unregister character device\n");
		unregister_chrdev(DAHDI_MAJOR, "dahdi");
		should_cleanup.clean_chardev = 0;
	}

	if (should_cleanup.clean_device_bus) {
		dahdi_dbg(DEVICES, "Unregister DAHDI device bus\n");
		bus_unregister(&dahdi_device_bus);
		should_cleanup.clean_device_bus = 0;
	}
}

static void dahdi_device_release(struct device *dev)
{
	struct dahdi_device *ddev = container_of(dev, struct dahdi_device, dev);
	kfree(ddev);
}

/**
 * dahdi_sysfs_add_device - Add the dahdi_device into the sysfs hierarchy.
 * @ddev:	The device to add.
 * @parent:	The physical device that is implementing this device.
 *
 * By adding the dahdi_device to the sysfs hierarchy user space can control
 * how spans are numbered.
 *
 */
int dahdi_sysfs_add_device(struct dahdi_device *ddev, struct device *parent)
{
	int ret;
	struct device *const dev = &ddev->dev;
	const char *dn;

	dev->parent = parent;
	dev->bus = &dahdi_device_bus;
	dn = dev_name(dev);
	if (!dn || !*dn) {
		/* Invent default name based on parent */
		if (!parent)
			return -EINVAL;
		dev_set_name(dev, "%s:%s", parent->bus->name, dev_name(parent));
	}
	ret = device_add(dev);
	return ret;
}

void dahdi_sysfs_init_device(struct dahdi_device *ddev)
{
	device_initialize(&ddev->dev);
	ddev->dev.release = dahdi_device_release;
}

void dahdi_sysfs_unregister_device(struct dahdi_device *ddev)
{
	device_del(&ddev->dev);
}

int __init dahdi_sysfs_init(const struct file_operations *dahdi_fops)
{
	int res = 0;

	dahdi_dbg(DEVICES, "Registering DAHDI device bus\n");
	res = bus_register(&dahdi_device_bus);
	if (res)
		return res;
	should_cleanup.clean_device_bus = 1;

	dahdi_dbg(DEVICES,
		"Registering character device (major=%d)\n", DAHDI_MAJOR);
	res = register_chrdev(DAHDI_MAJOR, "dahdi", dahdi_fops);
	if (res) {
		module_printk(KERN_ERR,
			"Unable to register DAHDI character device "
			"handler on %d\n", DAHDI_MAJOR);
		return res;
	}
	should_cleanup.clean_chardev = 1;

	res = dahdi_sysfs_chan_init(dahdi_fops);
	if (res)
		goto cleanup;

	res = bus_register(&spans_bus_type);
	if (res) {
		dahdi_err("%s: bus_register(%s) failed. Error number %d",
			__func__, spans_bus_type.name, res);
		goto cleanup;
	}
	should_cleanup.clean_span_bus_type = 1;

	res = driver_register(&dahdi_driver);
	if (res) {
		dahdi_err("%s: driver_register(%s) failed. Error number %d",
			__func__, dahdi_driver.name, res);
		goto cleanup;
	}
	should_cleanup.clean_dahdi_driver = 1;

	module_printk(KERN_INFO, "Telephony Interface Registered on major %d\n",
			DAHDI_MAJOR);
	return 0;

cleanup:
	dahdi_sysfs_cleanup();
	return res;
}

void dahdi_sysfs_exit(void)
{
	dahdi_sysfs_cleanup();
}

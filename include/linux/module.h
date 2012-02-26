#ifndef _LINUX_MODULE_H_
#define _LINUX_MODULE_H_

#include <sys/param.h>
#include <sys/kernel.h>

#include <linux/kmod.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/moduleparam.h>
#include <linux/completion.h>	/* linux/tracepoint.h / linux/rcupdate.h */

struct module {
	const char *name;
	const char *description;
	const char *author;
	const char *license;
	atomic_t refcount;
	int (*init)(void);
	void (*exit)(void);
};

extern struct module _this_module;

#define THIS_MODULE (&_this_module)

struct module_ptr_args {
	const void **pfield;
	void *value;
};

int _linux_module_modevent(struct module *mod, int type, void *data);

#define _LINUX_MODULE(name)						\
	struct module _this_module = { #name }

#define LINUX_DEV_MODULE(name)						\
	_LINUX_MODULE(name);						\
	DEV_MODULE(name, _linux_module_modevent, THIS_MODULE)

#define LINUX_DRIVER_MODULE(name, busname, driver, devclass)		\
	_LINUX_MODULE(name);						\
	DRIVER_MODULE(name, busname, driver, devclass, _linux_module_modevent, THIS_MODULE);

void _linux_module_ptr_sysinit(void *arg);

#define _module_ptr_args	__CONCAT(_module_ptr_args_, __LINE__)
#define _module_ptr_init(field, val)					\
	static struct module_ptr_args _module_ptr_args = {		\
		(const void **) &(THIS_MODULE->field), val		\
	};								\
	SYSINIT(__CONCAT(_module_ptr_args, _init),			\
		SI_SUB_KLD, SI_ORDER_FIRST,				\
		_linux_module_ptr_sysinit, &_module_ptr_args)

#define module_init(f)		_module_ptr_init(init, f)
#define module_exit(f)		_module_ptr_init(exit, f)
#define MODULE_DESCRIPTION(s)	_module_ptr_init(description, s)
#define MODULE_AUTHOR(s)	_module_ptr_init(author, s)
#define MODULE_LICENSE(s)	_module_ptr_init(license, s)
#define MODULE_ALIAS(n)
#define MODULE_PARM_DESC(name, desc)
#define MODULE_DEVICE_TABLE(type, name)

int try_module_get(struct module *);
void module_put(struct module *);
int module_refcount(struct module *);

#define EXPORT_SYMBOL(s)
#define EXPORT_SYMBOL_GPL(s)

#endif /* _LINUX_MODULE_H_ */

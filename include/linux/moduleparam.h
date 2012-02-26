#ifndef _LINUX_MODULEPARAM_H_
#define _LINUX_MODULEPARAM_H_

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <linux/init.h>

#define _SYSCTL_FLAG(mode)	((mode) & 0200 ? CTLFLAG_RW : CTLFLAG_RD)
#define module_param(name, type, mode)	module_param_##type(MODULE_PARAM_PREFIX "." #name, name, mode)
#define module_param_int(name, var, mode)				\
	TUNABLE_INT((name), &(var));					\
	SYSCTL_INT(MODULE_PARAM_PARENT, OID_AUTO, var, _SYSCTL_FLAG(mode),\
		   &(var), 0, MODULE_PARAM_PREFIX "." #name)
#define module_param_uint(name, var, mode)				\
	TUNABLE_INT((name), &(var));					\
	SYSCTL_UINT(MODULE_PARAM_PARENT, OID_AUTO, var, _SYSCTL_FLAG(mode),\
		   &(var), 0, MODULE_PARAM_PREFIX "." #name)
#define module_param_charp(name, var, mode)				\
	TUNABLE_STR((name), (var), sizeof(var));			\
	SYSCTL_STRING(MODULE_PARAM_PARENT, OID_AUTO, var, _SYSCTL_FLAG(mode),\
		   var, sizeof(var), MODULE_PARAM_PREFIX "." #name)
#define module_param_array(name, type, nump, mode)	/* NOT IMPLEMENTED */

#endif /* _LINUX_MODULEPARAM_H_ */

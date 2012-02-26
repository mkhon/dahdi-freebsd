#ifndef _LINUX_KMOD_H_
#define _LINUX_KMOD_H_

#include <linux/gfp.h>

int request_module(const char *fmt, ...);

#endif /* _LINUX_KMOD_H_ */

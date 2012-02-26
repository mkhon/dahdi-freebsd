#ifndef _LINUX_VERSION_H_
#define _LINUX_VERSION_H_

#include <sys/types.h>
#include <sys/conf.h>		/* D_VERSION */

#define D_VERSION_LINEAR	(((D_VERSION & 0xffff) << 16) | (((D_VERSION >> 16) & 0xff) << 8) | ((D_VERSION >> 24) & 0xff))

#define LINUX_VERSION_CODE	KERNEL_VERSION(2, 6, 32)
#define KERNEL_VERSION(x, y, z)	(((x) << 16) + ((y) << 8) + (z))

#endif /* _LINUX_VERSION_H_ */

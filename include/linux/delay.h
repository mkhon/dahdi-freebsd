#ifndef _LINUX_DELAY_H_
#define _LINUX_DELAY_H_

#include <sys/types.h>
#include <sys/systm.h>

#define mdelay(msec)		DELAY((msec) * 1000)
#define udelay(usec)		DELAY(usec)

#if defined(msleep)
#undef msleep
#endif
#define msleep(msec)		mdelay(msec)
#define msleep_interruptible(msec)	msleep(msec)

#endif /* _LINUX_DELAY_H_ */

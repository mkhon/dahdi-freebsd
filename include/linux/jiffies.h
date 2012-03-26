#ifndef _LINUX_JIFFIES_H_
#define _LINUX_JIFFIES_H_

#include <sys/limits.h>

#if 1
/* emulate jiffies */
static inline unsigned long
_jiffies(void)
{
	struct timeval tv;

	microuptime(&tv);
	return tvtohz(&tv);
}

#define jiffies	_jiffies()
#else
#define jiffies	ticks
#endif

#define HZ hz

#define time_after(a, b)	((a) > (b))
#define time_after_eq(a, b)	((a) >= (b))
#define time_before(a, b)	time_after((b), (a))

#define msecs_to_jiffies(msec)	((msec) / 1000 / HZ)

#define MAX_JIFFY_OFFSET ((LONG_MAX >> 1)-1)

#endif /* _LINUX_JIFFIES_H_ */

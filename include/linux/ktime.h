#ifndef _LINUX_KTIME_H_
#define _LINUX_KTIME_H_

#include <linux/time.h>
#include <sys/time.h>

static inline
void ktime_get_ts(struct timespec *ts)
{
	getnanotime(ts);
}

#endif /* _LINUX_KTIME_H_ */

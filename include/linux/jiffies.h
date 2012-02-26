#ifndef _LINUX_JIFFIES_H_
#define _LINUX_JIFFIES_H_

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

#endif /* _LINUX_JIFFIES_H_ */

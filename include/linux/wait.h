#ifndef _LINUX_WAIT_H_
#define _LINUX_WAIT_H_

#include <linux/spinlock.h>

typedef void *wait_queue_head_t;
#define init_waitqueue_head(q)
#define wake_up(q)			wakeup_one(q)
#define wake_up_interruptible(q)	wakeup_one(q)
#define wake_up_interruptible_all(q)	wakeup(q)
#define wait_event_timeout(q, condition, timeout)			\
({									\
	int __ret = timeout;						\
	if (!(condition)) {						\
		for (;;) {						\
			if (tsleep(&q, 0, "wait_event", (timeout))) {	\
				__ret = 0;				\
				break;					\
			}						\
			if (condition)					\
				break;					\
		}							\
	}								\
	__ret;								\
})
#define wait_event_interruptible(q, condition) wait_event_timeout(q, condition, 0)
#define wait_event_interruptible_timeout(q, condition, timeout) wait_event_timeout(q, condition, timeout)

#endif /* _LINUX_WAIT_H_ */

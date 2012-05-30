#ifndef _LINUX_WAIT_H_
#define _LINUX_WAIT_H_

#include <linux/spinlock.h>

struct __wait_queue_head {
	void *dummy;
};
typedef struct __wait_queue_head wait_queue_head_t;

static inline void
init_waitqueue_head(wait_queue_head_t *q)
{
	// nothing to do
}

static inline void
wake_up(wait_queue_head_t *q)
{
	wakeup_one(q);
}

static inline void
wake_up_interruptible(wait_queue_head_t *q)
{
	wakeup_one(q);
}

static inline void
wake_up_interruptible_all(wait_queue_head_t *q)
{
	wakeup(q);
}

#define _wait_event(q, condition, prio, timeout)			\
({									\
	int __ret = (timeout);						\
	if (!(condition)) {						\
		for (;;) {						\
			if (tsleep(&(q), (prio), "lxwait", (timeout))) { \
				__ret = 0;				\
				break;					\
			}						\
			if (condition)					\
				break;					\
		}							\
	}								\
	__ret;								\
})
#define wait_event_timeout(q, condition, timeout) _wait_event(q, condition, 0, timeout)
#define wait_event_interruptible(q, condition) _wait_event(q, condition, PCATCH, 0)
#define wait_event_interruptible_timeout(q, condition, timeout) _wait_event(q, condition, PCATCH, timeout)

#endif /* _LINUX_WAIT_H_ */

#ifndef _LINUX_SCHED_H_
#define _LINUX_SCHED_H_

#include <asm/atomic.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uio.h>
#include <linux/workqueue.h>

#define schedule_timeout(jiffies)	pause("lnxslp", jiffies)

#endif /* _LINUX_SCHED_H_ */

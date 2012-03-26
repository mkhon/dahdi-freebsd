#ifndef _LINUX_SCHED_H_
#define _LINUX_SCHED_H_

#include <asm/atomic.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/uio.h>
#include <linux/workqueue.h>

#include <sys/proc.h>
#include <sys/sched.h>

#define schedule_timeout(jiffies)	pause("lnxslp", jiffies)
#define schedule()			sched_relinquish(curthread)

#endif /* _LINUX_SCHED_H_ */

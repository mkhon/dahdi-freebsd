#ifndef _LINUX_INTERRUPT_H_
#define _LINUX_INTERRUPT_H_

#include <sys/types.h>
#include <sys/taskqueue.h>
#include <linux/kernel.h>
#include <linux/irqreturn.h>

struct tasklet_struct {
	struct task task;

	void (*func)(unsigned long);
	unsigned long data;
	atomic_t disable_count;
};

#define DECLARE_TASKLET(name, _func, _data)			\
	struct tasklet_struct name = {				\
		func: _func,					\
		data: _data,					\
		disable_count: 0,				\
	};							\
	SYSINIT(name ## _init, SI_SUB_KLD, SI_ORDER_ANY, _tasklet_init, &name)

void _tasklet_init(struct tasklet_struct *t);
void tasklet_init(struct tasklet_struct *t, void (*func)(unsigned long), unsigned long data);
void tasklet_schedule(struct tasklet_struct *t);
void tasklet_hi_schedule(struct tasklet_struct *t);
void tasklet_disable(struct tasklet_struct *t);
void tasklet_enable(struct tasklet_struct *t);
void tasklet_kill(struct tasklet_struct *t);

#endif /* _LINUX_INTERRUPT_H_ */

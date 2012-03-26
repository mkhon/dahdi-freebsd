#ifndef _LINUX_WORKQUEUE_H_
#define _LINUX_WORKQUEUE_H_

#include <sys/taskqueue.h>
#include <linux/timer.h>

struct work_struct;

typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	struct task task;
	work_func_t func;
	struct taskqueue *tq;
};

struct _linux_work_init_args {
	struct work_struct *work;
	work_func_t func;
};

void _linux_work_init(struct _linux_work_init_args *a);

#define DECLARE_WORK(name, wf)						\
	struct work_struct name;					\
	static struct _linux_work_init_args name##_work_init_args =	\
	    { &name, wf };						\
	SYSINIT(name##_work_sysinit, SI_SUB_LOCK, SI_ORDER_MIDDLE,	\
	    _linux_work_init, &name##_work_init_args)

#define INIT_WORK(work, wf)						\
	do {								\
		struct _linux_work_init_args name##_work_init_args =	\
		    { work, wf };					\
		_linux_work_init(&name##_work_init_args);		\
	} while (0)

void schedule_work(struct work_struct *work);
void cancel_work_sync(struct work_struct *work);
void flush_work(struct work_struct *work);

struct workqueue_struct {
	struct taskqueue *tq;
};

struct workqueue_struct *create_singlethread_workqueue(const char *name);
void destroy_workqueue(struct workqueue_struct *wq);
void flush_workqueue(struct workqueue_struct *wq);
void queue_work(struct workqueue_struct *wq, struct work_struct *work);

#endif /* _LINUX_WORKQUEUE_H_ */

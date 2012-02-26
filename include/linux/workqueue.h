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

#define INIT_WORK(ws, wf)					\
	do {							\
		TASK_INIT(&(ws)->task, 0, _work_run, (ws));	\
		(ws)->func = (wf);				\
		(ws)->tq = taskqueue_fast;			\
	} while (0)
void _work_run(void *context, int pending);
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

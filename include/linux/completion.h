#ifndef _LINUX_COMPLETION_H_
#define _LINUX_COMPLETION_H_

#include <linux/wait.h>
#include <sys/condvar.h>
#include <sys/lock.h>
#include <sys/mutex.h>

struct completion {
	struct cv cv;
	struct mtx lock;
	int done;
};

void init_completion(struct completion *c);
void destroy_completion(struct completion *c);
void wait_for_completion(struct completion *c);
int wait_for_completion_timeout(struct completion *c, unsigned long timeout);
void complete(struct completion *c);

#endif /* _LINUX_COMPLETION_H_ */

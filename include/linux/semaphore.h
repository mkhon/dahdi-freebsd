#ifndef _LINUX_SEMAPHORE_H_
#define _LINUX_SEMAPHORE_H_

#include <sys/sema.h>

struct semaphore {
	struct sema sema;
};

void _sema_init(struct semaphore *s, int value);
void _sema_destroy(struct semaphore *s);
void down(struct semaphore *s);
int down_interruptible(struct semaphore *s);
int down_trylock(struct semaphore *s);
void up(struct semaphore *s);

#define init_MUTEX(s)		_sema_init((s), 1)
#define destroy_MUTEX(s)	_sema_destroy(s)

#endif /* _LINUX_SEMAPHORE_H_ */

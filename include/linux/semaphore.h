#ifndef _LINUX_SEMAPHORE_H_
#define _LINUX_SEMAPHORE_H_

#include <sys/sema.h>

struct semaphore {
	struct sema sema;
};

void _linux_sema_init(struct semaphore *s, int value);
void _linux_sema_destroy(struct semaphore *s);
void down(struct semaphore *s);
int down_interruptible(struct semaphore *s);
int down_trylock(struct semaphore *s);
void up(struct semaphore *s);

void _linux_sema_sysinit(struct semaphore *s);

#define DEFINE_SEMAPHORE(name)						\
	struct semaphore name;						\
	SYSINIT(name##_sema_sysinit, SI_SUB_LOCK, SI_ORDER_MIDDLE,	\
	    _linux_sema_sysinit, &name);				\
	SYSUNINIT(name##_sema_sysuninit, SI_SUB_LOCK, SI_ORDER_MIDDLE,	\
	    _linux_sema_destroy, &name)

#define init_MUTEX(s)		sema_init((s), 1)
#define destroy_MUTEX(s)	sema_destroy(s)

#define sema_init(s, value)	_linux_sema_init(s, value)
#define sema_destroy(s)		_linux_sema_destroy(s)

#endif /* _LINUX_SEMAPHORE_H_ */

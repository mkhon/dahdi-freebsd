#ifndef _LINUX_SPINLOCK_TYPES_H_
#define _LINUX_SPINLOCK_TYPES_H_

#include <sys/lock.h>
#include <sys/mutex.h>
#include <linux/rwlock_types.h>

typedef struct mtx spinlock_t;

#define DEFINE_SPINLOCK(name)				\
	struct mtx name;				\
	MTX_SYSINIT(name, &name, #name, MTX_SPIN)

#endif /* _LINUX_SPINLOCK_TYPES_H_ */

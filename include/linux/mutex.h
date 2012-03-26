#ifndef _LINUX_MUTEX_H_
#define _LINUX_MUTEX_H_

#include <linux/list.h>
#include <linux/spinlock_types.h>
#include <asm/atomic.h>

#include <linux/semaphore.h>

struct mutex {
	struct mtx mtx;
};

#define mutex_lock(m) mtx_lock(&(m)->mtx)
#define mutex_unlock(m) mtx_unlock(&(m)->mtx)
#define mutex_init(m) mtx_init(&(m)->mtx, "Linux mutex", NULL, MTX_DEF)

#define DEFINE_MUTEX(name)				\
	struct mutex name;				\
	MTX_SYSINIT(name, &name.mtx, #name, MTX_DEF)

#endif /* _LINUX_MUTEX_H_ */

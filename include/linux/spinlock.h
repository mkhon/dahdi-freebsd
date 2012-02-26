#ifndef _LINUX_SPINLOCK_H_
#define _LINUX_SPINLOCK_H_

#include <linux/compiler.h>
#include <linux/spinlock_types.h>
#include <linux/rwlock.h>
#include <linux/kernel.h>

#define spin_lock_init(lock)	mtx_init(lock, "DAHDI spinlock", NULL, MTX_SPIN)
#define spin_lock_destroy(lock)	mtx_destroy(lock)
#define spin_lock(lock)		mtx_lock_spin(lock)
#define spin_unlock(lock)	mtx_unlock_spin(lock)
#define spin_lock_bh(lock)	spin_lock(lock)
#define spin_unlock_bh(lock)	spin_unlock(lock)
#define spin_lock_irqsave(lock, flags)			\
	do {						\
		mtx_lock_spin(lock);			\
		(void) &(flags);			\
	} while (0)
#define spin_unlock_irqrestore(lock, flags)		\
	mtx_unlock_spin(lock)

#endif /* _LINUX_SPINLOCK_H_ */

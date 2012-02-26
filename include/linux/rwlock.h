#ifndef _LINUX_RWLOCK_H_
#define _LINUX_RWLOCK_H_

#ifndef _LINUX_SPINLOCK_H_
#error "Please do not include this file directly"
#endif

#if defined(SX_ADAPTIVESPIN) && !defined(SX_NOADAPTIVE)
#define SX_NOADAPTIVE SX_ADAPTIVESPIN
#endif

#define rwlock_init(rwlock)	sx_init_flags(rwlock, "DAHDI rwlock", SX_NOADAPTIVE)
#define read_lock(rwlock)	sx_slock(rwlock)
#define read_unlock(rwlock)	sx_sunlock(rwlock)

#define write_lock(rwlock)	sx_xlock(rwlock)
#define write_unlock(rwlock)	sx_xunlock(rwlock)
#define write_lock_irqsave(rwlock, flags)		\
	do {						\
		sx_xlock(rwlock);			\
		(void) &(flags);			\
	} while (0)
#define write_unlock_irqrestore(rwlock, flags)		\
	sx_xunlock(rwlock)

#endif /* _LINUX_RWLOCK_H_ */

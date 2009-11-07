#ifndef _DAHDI_COMPAT_BSD_H_
#define _DAHDI_COMPAT_BSD_H_

#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <machine/atomic.h>

struct module;

#define EXPORT_SYMBOL(s)

#define KERNEL_VERSION(x, y, z)	0

#define copy_from_user(to, from, n)	(bcopy((from), (to), (n)), 0)
#define copy_to_user(to, from, n)	(bcopy((from), (to), (n)), 0)

#define get_user(v, p)	copy_from_user(&(v), (void *) (p), sizeof(v))
#define put_user(v, p)	copy_to_user((void *) (p), &(v), sizeof(v))

typedef void *wait_queue_head_t;
#define init_waitqueue_head(q)
#define wake_up_interruptible(q)	wakeup(q)

#define test_bit(v, p)	((*(p)) & (1 << ((v) & 0x1f)))
#define set_bit(v, p)	atomic_set_long((p), (1 << ((v) & 0x1f)))
#define clear_bit(v, p)	atomic_clear_long((p), (1 << ((v) & 0x1f)))

#define ADDR (*(volatile long *) addr)

#ifdef SMP
#define	LOCK_PREFIX "lock ; "
#else
#define	LOCK_PREFIX ""
#endif /* SMP */

static __inline int test_and_set_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__ __volatile__( LOCK_PREFIX
		"btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"Ir" (nr) : "memory");
	return oldbit;
}

static __inline__ int test_and_clear_bit(int nr, volatile void * addr)
{
	int oldbit;

	__asm__ __volatile__( LOCK_PREFIX
		"btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"Ir" (nr) : "memory");
	return oldbit;
}

typedef u_int atomic_t;
#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		atomic_add_int(p, 1)
#define atomic_dec(p)		atomic_subtract_int(p, 1)
#define atomic_add(v, p)	atomic_add_int(p, v)
#define atomic_sub(v, p)	atomic_subtract_int(p, v)

typedef struct mtx spinlock_t;

#define DEFINE_SPINLOCK(name)				\
	struct mtx name;				\
	MTX_SYSINIT(name, &name, #name, MTX_SPIN)
#define spin_lock_init(lock)	mtx_init(lock, "dahdi lock", NULL, MTX_SPIN)
#define spin_lock_destroy(lock)	mtx_destroy(lock)
#define spin_lock(lock)		mtx_lock_spin(lock)
#define spin_unlock(lock)	mtx_unlock_spin(lock)
#define spin_lock_irqsave(lock, flags)			\
	do {						\
		mtx_lock_spin(lock);			\
		(void) &(flags);			\
	} while (0)
#define spin_unlock_irqrestore(lock, flags)		\
	mtx_unlock_spin(lock)

#define DEFINE_RWLOCK(name)				\
	struct sx name;					\
	SX_SYSINIT(name, &name, #name)
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

#define WARN_ON(cond)					\
	do {						\
		if (cond)				\
			printf("WARN_ON: " #cond "\n");	\
	} while (0)

#define KERN_EMERG	"<0>"	/* system is unusable			*/
#define KERN_ALERT	"<1>"	/* action must be taken immediately	*/
#define KERN_CRIT	"<2>"	/* critical conditions			*/
#define KERN_ERR	"<3>"	/* error conditions			*/
#define KERN_WARNING	"<4>"	/* warning conditions			*/
#define KERN_NOTICE	"<5>"	/* normal but significant condition	*/
#define KERN_INFO	"<6>"	/* informational			*/
#define KERN_DEBUG	"<7>"	/* debug-level messages			*/

#define dev_err(dev, fmt, args...)	device_printf(dev, fmt, ##args)
#define dev_warn(dev, fmt, args...)	device_printf(dev, fmt, ##args)
#define dev_notice(dev, fmt, args...)	device_printf(dev, fmt, ##args)
#define dev_info(dev, fmt, args...)	device_printf(dev, fmt, ##args)
#define dev_dbg(dev, fmt, args...)	device_printf(dev, fmt, ##args)

#define pr_info(fmt, args...)		printf(fmt, ##args)

#define printk(fmt, args...)		printf(fmt, ##args)

#define GFP_KERNEL	0
#define GFP_ATOMIC	0

MALLOC_DECLARE(M_DAHDI);

#define kmalloc(size, flags)	malloc((size), M_DAHDI, M_NOWAIT)
#define kcalloc(n, size, flags)	malloc((n) * (size), M_DAHDI, M_NOWAIT | M_ZERO)
#define kfree(p)		free(p, M_DAHDI)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define memmove(dst, src, size)	bcopy((src), (dst), (size))

#define might_sleep()

#define ENODATA EINVAL

#define __init
#define __exit
#define __devinit
#define __devexit
#define __devinitdata
#define __user

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

/* emulate jiffies */
static inline unsigned long _jiffies(void)
{
	struct timeval tv;

	microuptime(&tv);
	return tvtohz(&tv);
}

#define jiffies			_jiffies()
#define HZ			hz
#define udelay(usec)		DELAY(usec)
#define mdelay(msec)		DELAY((msec) * 1000)
#define time_after(a, b)	((a) > (b))
#define time_after_eq(a, b)	((a) >= (b))

#define DAHDI_IRQ_HANDLER(a)	static int a(void *dev_id)

#define THIS_MODULE		((struct module *) __FILE__)

#define PCI_ANY_ID (~0)

struct pci_device_id {
	uint32_t vendor, device;	/* Vendor and device ID or PCI_ANY_ID*/
	uint32_t subvendor, subdevice;	/* Subsystem ID's or PCI_ANY_ID */
	uint32_t class, class_mask;	/* (class,subclass,prog-if) triplet */
	unsigned long driver_data;	/* Data private to the driver */
};

#endif /* _DAHDI_COMPAT_BSD_H_ */

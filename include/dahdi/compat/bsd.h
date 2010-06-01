#ifndef _DAHDI_COMPAT_BSD_H_
#define _DAHDI_COMPAT_BSD_H_

#include <sys/callout.h>
#include <sys/condvar.h>
#include <sys/endian.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <machine/atomic.h>
#include <machine/bus.h>

#define LINUX_VERSION_CODE	KERNEL_VERSION(2, 6, 32)
#define KERNEL_VERSION(x, y, z)	(((x) << 16) + ((y) << 8) + (z))

/*
 * Byte order API
 */
#define cpu_to_le32(x)	htole32(x)
#define le32_to_cpu(x)	le32toh(x)
#define cpu_to_le16(x)	htole16(x)

#if _BYTE_ORDER == _LITTLE_ENDIAN
#define __constant_htons(x)	((uint16_t) (((uint16_t) (x)) << 8 | ((uint16_t) (x)) >> 8))
#else
#define __constant_htons(x)	(x)
#endif

/*
 * Copy from/to user API
 */
#define copy_from_user(to, from, n)	(bcopy((from), (to), (n)), 0)
#define copy_to_user(to, from, n)	(bcopy((from), (to), (n)), 0)

#define get_user(v, p)	copy_from_user(&(v), (void *) (p), sizeof(v))
#define put_user(v, p)	copy_to_user((void *) (p), &(v), sizeof(v))

/*
 * Waitqueue API
 */
typedef void *wait_queue_head_t;
#define init_waitqueue_head(q)
#define wake_up(q)			wakeup_one(q)
#define wake_up_interruptible(q)	wakeup_one(q)
#define wake_up_interruptible_all(q)	wakeup(q)
#define wait_event_timeout(q, condition, timeout)			\
({									\
	int __ret = timeout;						\
	if (!(condition)) {						\
		for (;;) {						\
			if (tsleep(&q, 0, "wait_event", (timeout))) {	\
				__ret = 0;				\
				break;					\
			}						\
			if (condition)					\
				break;					\
		}							\
	}								\
	__ret;								\
})

/*
 * Bit API
 */
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

/*
 * Atomic API
 */
typedef int atomic_t;
#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		atomic_add_int(p, 1)
#define atomic_dec(p)		atomic_subtract_int(p, 1)
#define atomic_dec_and_test(p)	(atomic_fetchadd_int(p, -1) == 1)
#define atomic_add(v, p)	atomic_add_int(p, v)
#define atomic_sub(v, p)	atomic_subtract_int(p, v)

#define ATOMIC_INIT(v)		(v)

/*
 * Spinlock API
 */
typedef struct mtx spinlock_t;

#define DEFINE_SPINLOCK(name)				\
	struct mtx name;				\
	MTX_SYSINIT(name, &name, #name, MTX_SPIN)
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

/*
 * Rwlock API
 */
typedef struct sx rwlock_t;

#define DEFINE_RWLOCK(name)				\
	struct sx name;					\
	SX_SYSINIT(name, &name, #name)
#define rwlock_init(rwlock)	sx_init(rwlock, "DAHDI rwlock")
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

/*
 * Tasklet API
 */
struct tasklet_struct {
	struct task task;

	void (*func)(unsigned long);
	unsigned long data;
	atomic_t disable_count;
};

void tasklet_init(struct tasklet_struct *t, void (*func)(unsigned long), unsigned long data);
void tasklet_hi_schedule(struct tasklet_struct *t);
void tasklet_disable(struct tasklet_struct *t);
void tasklet_enable(struct tasklet_struct *t);
void tasklet_kill(struct tasklet_struct *t);

/*
 * Timer API
 */
struct timer_list {
	struct mtx mtx;
	struct callout callout;

	unsigned long expires;
	void (*function)(unsigned long);
	unsigned long data;
};

void init_timer(struct timer_list *t);
void setup_timer(struct timer_list *t, void (*function)(unsigned long), unsigned long data);
void mod_timer(struct timer_list *t, unsigned long expires);
void add_timer(struct timer_list *t);
void del_timer(struct timer_list *t);
void del_timer_sync(struct timer_list *t);

/*
 * Completion API
 */
struct completion {
	struct cv cv;
	struct mtx lock;
};

#define INIT_COMPLETION(c)
void init_completion(struct completion *c);
void destroy_completion(struct completion *c);
int wait_for_completion_timeout(struct completion *c, unsigned long timeout);
void complete(struct completion *c);

/*
 * Semaphore API
 */
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

/*
 * Workqueue API
 */
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
void queue_work(struct workqueue_struct *wq, struct work_struct *work);

/*
 * Logging and assertions API
 */
void rlprintf(int pps, const char *fmt, ...)
	__printflike(2, 3);

void
device_rlprintf(int pps, device_t dev, const char *fmt, ...)
	__printflike(3, 4);

#define might_sleep()

#define WARN_ON(cond)					\
	do {						\
		if (cond)				\
			printf("WARN_ON: " #cond "\n");	\
	} while (0)
#define BUG_ON(cond)					\
	do {						\
		if (cond)				\
			panic("BUG_ON: " #cond);	\
	} while (0)

#define KERN_EMERG	"<0>"	/* system is unusable			*/
#define KERN_ALERT	"<1>"	/* action must be taken immediately	*/
#define KERN_CRIT	"<2>"	/* critical conditions			*/
#define KERN_ERR	"<3>"	/* error conditions			*/
#define KERN_WARNING	"<4>"	/* warning conditions			*/
#define KERN_NOTICE	"<5>"	/* normal but significant condition	*/
#define KERN_INFO	"<6>"	/* informational			*/
#define KERN_DEBUG	"<7>"	/* debug-level messages			*/
#define KERN_CONT	""

#define dev_err(dev, fmt, args...)	device_printf(*(dev), fmt, ##args)
#define dev_warn(dev, fmt, args...)	device_printf(*(dev), fmt, ##args)
#define dev_notice(dev, fmt, args...)	device_printf(*(dev), fmt, ##args)
#define dev_info(dev, fmt, args...)	device_printf(*(dev), fmt, ##args)
#define dev_dbg(dev, fmt, args...)	device_printf(*(dev), fmt, ##args)

#define pr_info(fmt, args...)		printf(fmt, ##args)

#define printk(fmt, args...)		printf(fmt, ##args)
#define vprintk(fmt, args)		vprintf(fmt, args)

int printk_ratelimit(void);

/*
 * Malloc API
 */
#define GFP_KERNEL	0
#define GFP_ATOMIC	0

MALLOC_DECLARE(M_DAHDI);

#define kmalloc(size, flags)	malloc((size), M_DAHDI, M_NOWAIT)
#define kcalloc(n, size, flags)	malloc((n) * (size), M_DAHDI, M_NOWAIT | M_ZERO)
#define kzalloc(a, b)		kcalloc(1, (a), (b))
#define kfree(p)		free(p, M_DAHDI)

/*
 * Kernel module API
 */
#define __init
#define __exit
#define __devinit
#define __devexit
#define __devinitdata

struct module;

#define try_module_get(m)	(1)
#define module_put(m)		((void) (m))
#define THIS_MODULE		((struct module *) __FILE__)
int request_module(const char *fmt, ...);

#define EXPORT_SYMBOL(s)

#define _SYSCTL_FLAG(mode)	((mode) & 0200 ? CTLFLAG_RW : CTLFLAG_RD)
#define module_param(name, type, mode)	module_param_##type(MODULE_PARAM_PREFIX "." #name, name, mode)
#define module_param_int(name, var, mode)				\
	TUNABLE_INT((name), &(var));					\
	SYSCTL_INT(MODULE_PARAM_PARENT, OID_AUTO, var, _SYSCTL_FLAG(mode),\
		   &(var), 0, MODULE_PARAM_PREFIX "." #name)
#define module_param_uint(name, var, mode)				\
	TUNABLE_INT((name), &(var));					\
	SYSCTL_UINT(MODULE_PARAM_PARENT, OID_AUTO, var, _SYSCTL_FLAG(mode),\
		   &(var), 0, MODULE_PARAM_PREFIX "." #name)
#define module_param_charp(name, var, mode)				\
	TUNABLE_STR((name), (var), sizeof(var));			\
	SYSCTL_STRING(MODULE_PARAM_PARENT, OID_AUTO, var, _SYSCTL_FLAG(mode),\
		   var, sizeof(var), MODULE_PARAM_PREFIX "." #name)
#define MODULE_PARM_DESC(name, desc)

#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

#define S_IRUGO         (S_IRUSR|S_IRGRP|S_IROTH)

SYSCTL_DECL(_dahdi);
SYSCTL_DECL(_dahdi_echocan);

/*
 * Firmware API
 */
struct firmware;

int request_firmware(const struct firmware **firmware_p, const char *name, device_t *device);
void release_firmware(const struct firmware *firmware);

/*
 * PCI device API
 */
#define PCI_ANY_ID (~0)

struct pci_device_id {
	uint32_t vendor, device;	/* Vendor and device ID or PCI_ANY_ID*/
	uint32_t subvendor, subdevice;	/* Subsystem ID's or PCI_ANY_ID */
	uint32_t class, class_mask;	/* (class,subclass,prog-if) triplet */
	unsigned long driver_data;	/* Data private to the driver */
};

struct pci_dev {
	device_t dev;
};

#define dahdi_pci_get_bus(pci_dev)	pci_get_bus((pci_dev)->dev)
#define dahdi_pci_get_slot(pci_dev)	pci_get_slot((pci_dev)->dev)
#define dahdi_pci_get_irq(pci_dev)	pci_get_irq((pci_dev)->dev)
#define dahdi_pci_get_device(pci_dev)	pci_get_device((pci_dev)->dev)
#define dahdi_pci_get_vendor(pci_dev)	pci_get_vendor((pci_dev)->dev)

struct pci_device_id *dahdi_pci_device_id_lookup(device_t dev, struct pci_device_id *tbl);

/*
 * Time API
 */
#if 1
/* emulate jiffies */
static inline unsigned long _jiffies(void)
{
	struct timeval tv;

	microuptime(&tv);
	return tvtohz(&tv);
}

#define jiffies			_jiffies()
#else
#define jiffies			ticks
#endif
#define HZ			hz

#define udelay(usec)		DELAY(usec)
#define mdelay(msec)		DELAY((msec) * 1000)

#define schedule_timeout(jiff)	pause("dhdslp", jiff)

#if defined(msleep)
#undef msleep
#endif
#define msleep(msec)		mdelay(msec)

#define time_after(a, b)	((a) > (b))
#define time_after_eq(a, b)	((a) >= (b))
#define time_before(a, b)	time_after((b), (a))

/*
 * Misc API
 */
char *
strncat(char * __restrict dst, const char * __restrict src, size_t n);

#define memmove(dst, src, size)	bcopy((src), (dst), (size))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define ENODATA EINVAL

#define __user

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define DAHDI_IRQ_HANDLER(a)	static int a(void *dev_id)

#define clamp(x, low, high) min(max(low, x), high)

extern u_short fcstab[256];

/*
 * DMA API
 */
void
dahdi_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error);

int
dahdi_dma_allocate(int size, bus_dma_tag_t *ptag, bus_dmamap_t *pmap, void **pvaddr, uint32_t *ppaddr);

void
dahdi_dma_free(bus_dma_tag_t *ptag, bus_dmamap_t *pmap, void **pvaddr, uint32_t *ppaddr);

#endif /* _DAHDI_COMPAT_BSD_H_ */

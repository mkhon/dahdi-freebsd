#ifndef _LINUX_BITOPS_H_
#define _LINUX_BITOPS_H_

#include <machine/atomic.h>

#define test_bit(v, p)	((*(p)) & (1 << ((v) & 0x1f)))
#define set_bit(v, p)	atomic_set_long((p), (1 << ((v) & 0x1f)))
#define clear_bit(v, p)	atomic_clear_long((p), (1 << ((v) & 0x1f)))

#if defined(__i386__) || defined(__x86_64__)
#define ADDR (*(volatile long *) addr)

#ifdef SMP
#define	LOCK_PREFIX "lock ; "
#else
#define	LOCK_PREFIX ""
#endif /* SMP */

static __inline int
test_and_set_bit(int nr, volatile void *addr)
{
	int oldbit;

	__asm__ __volatile__( LOCK_PREFIX
		"btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"Ir" (nr) : "memory");
	return oldbit;
}

static __inline__
int test_and_clear_bit(int nr, volatile void *addr)
{
	int oldbit;

	__asm__ __volatile__( LOCK_PREFIX
		"btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"Ir" (nr) : "memory");
	return oldbit;
}
#else
static __inline int
test_and_set_bit(int nr, volatile void *addr)
{
	int val;

	do {
		val = *(volatile int *) addr;
	} while (atomic_cmpset_int(addr, val, val | (1 << nr)) == 0);
	return (val & (1 << nr));
}

static __inline__
int test_and_clear_bit(int nr, volatile void *addr)
{
	int val;

	do {
		val = *(volatile int *) addr;
	} while (atomic_cmpset_int(addr, val, val & ~(1 << nr)) == 0);
	return (val & (1 << nr));
}
#endif

#endif /* _LINUX_BITOPS_H_ */

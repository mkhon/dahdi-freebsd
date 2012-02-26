#ifndef _ASM_ATOMIC_H_
#define _ASM_ATOMIC_H_

#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		atomic_add_int(p, 1)
#define atomic_dec(p)		atomic_subtract_int(p, 1)
#define atomic_dec_and_test(p)	(atomic_fetchadd_int(p, -1) == 1)
#define atomic_add(v, p)	atomic_add_int(p, v)
#define atomic_sub(v, p)	atomic_subtract_int(p, v)

#define ATOMIC_INIT(v)		(v)

#endif /* _ASM_ATOMIC_H_ */

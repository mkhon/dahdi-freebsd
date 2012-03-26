#ifndef _LINUX_UACCESS_H_
#define _LINUX_UACCESS_H_

#include <sys/systm.h>

#define __copy_from_user(to, from, n)	copyin((from), (to), (n))
#define copy_from_user(to, from, n)	__copy_from_user((to), (from), (n))
#define __copy_to_user(to, from, n)	copyout((from), (to), (n))
#define copy_to_user(to, from, n)	__copy_to_user((to), (from), (n))

#define get_user(v, p)	copy_from_user(&(v), (void *) (p), sizeof(v))
#define put_user(v, p)						\
	do {							\
		int j = (v);					\
		copy_to_user((void *) (p), &j, sizeof(j));	\
	} while (0)

#endif /* _LINUX_UACCESS_H_ */

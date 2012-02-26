#ifndef _LINUX_RWLOCK_TYPES_H_
#define _LINUX_RWLOCK_TYPES_H_

#include <sys/sx.h>

typedef struct sx rwlock_t;

#define DEFINE_RWLOCK(name)				\
	struct sx name;					\
	SX_SYSINIT(name, &name, #name)

#endif /* _LINUX_RWLOCK_TYPES_H_ */

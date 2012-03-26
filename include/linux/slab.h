#ifndef _LINUX_SLAB_H_
#define _LINUX_SLAB_H_

#include <sys/types.h>
#include <sys/malloc.h>

#include <linux/gfp.h>

#define kmalloc(size, flags)	malloc((size), M_LINUX, M_NOWAIT)
#define kcalloc(n, size, flags)	malloc((n) * (size), M_LINUX, M_NOWAIT | M_ZERO)
#define kzalloc(a, b)		kcalloc(1, (a), (b))
#define kfree(p)		free(__DECONST(void *, p), M_LINUX)

MALLOC_DECLARE(M_LINUX);

#endif /* !_LINUX_SLAB_H_ */

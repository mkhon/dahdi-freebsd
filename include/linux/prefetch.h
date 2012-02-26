#ifndef _LINUX_PREFETCH_H_
#define _LINUX_PREFETCH_H_

#define prefetch(x) __builtin_prefetch(x)

#endif /* _LINUX_PREFETCH_H_ */

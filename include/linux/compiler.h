#ifndef _LINUX_COMPILER_H_
#define _LINUX_COMPILER_H_

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define __attribute_const__ __attribute__((__const__))

#define __user

#endif /* _LINUX_COMPILER_H_ */

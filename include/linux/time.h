#ifndef _LINUX_TIME_H_
#define _LINUX_TIME_H_

#include <linux/kernel.h>

#include <sys/timespec.h>

struct timespec current_kernel_time(void);

#define NSEC_PER_SEC	1000000000L

#endif /* _LINUX_TIME_H_ */

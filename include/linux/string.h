#ifndef _LINUX_STRING_H_
#define _LINUX_STRING_H_

#include <sys/systm.h>

char *strncat(char * __restrict dst, const char * __restrict src, size_t n);

#endif /* _LINUX_STRING_H_ */

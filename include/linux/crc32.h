#ifndef _LINUX_CRC32_H_
#define _LINUX_CRC32_H_

#include <sys/libkern.h>

#define crc32_le(crc, data, len)	crc32_raw(data, len, crc)

#endif /* _LINUX_CRC32_H_ */

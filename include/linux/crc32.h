#ifndef _LINUX_CRC32_H_
#define _LINUX_CRC32_H_

#if defined(__FreeBSD__)
#include <sys/gsb_crc32.h>
#endif
#include <sys/libkern.h>

#define crc32_le(crc, data, len)	crc32_raw(data, len, crc)
#define crc32(crc, data, len)		crc32_le(crc, data, len)

#endif /* _LINUX_CRC32_H_ */

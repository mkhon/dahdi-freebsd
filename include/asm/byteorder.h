#ifndef _ASM_BYTEORDER_H_
#define _ASM_BYTEORDER_H_

#include <sys/endian.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN
#define __LITTLE_ENDIAN
#define __LITTLE_ENDIAN_BITFIELD
#define __constant_htons(x)	((uint16_t) (((uint16_t) (x)) << 8 | ((uint16_t) (x)) >> 8))
#elif _BYTE_ORDER == _BIG_ENDIAN
#define __BIG_ENDIAN
#define __BIG_ENDIAN_BITFIELD
#define __constant_htons(x)	(x)
#endif

#define cpu_to_le32(x)	htole32(x)
#define le32_to_cpu(x)	le32toh(x)
#define cpu_to_be32(x)	htobe32(x)

#define cpu_to_le16(x)	htole16(x)
#define le16_to_cpu(x)	le16toh(x)
#define cpu_to_be16(x)	htobe16(x)
#define be16_to_cpu(x)	be16toh(x)

#endif /* _ASM_BYTEORDER_H_ */

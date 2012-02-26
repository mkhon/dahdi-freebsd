#ifndef _LINUX_TYPES_H_
#define _LINUX_TYPES_H_

#include <sys/types.h>
#include <machine/bus.h>

typedef uint8_t __u8;
typedef uint8_t u8;

typedef uint16_t __u16;
typedef uint16_t u16;

typedef uint32_t __u32;
typedef uint32_t u32;

typedef int32_t __s32;
typedef int32_t s32;

typedef uint32_t __u64;
typedef uint64_t u64;

typedef __u16 __le16;
typedef __u16 __be16;

typedef __u32 __le32;
typedef __u32 __be32;

typedef volatile int atomic_t;

typedef bus_addr_t dma_addr_t;
typedef off_t loff_t;

#endif /* _LINUX_TYPES_H_ */

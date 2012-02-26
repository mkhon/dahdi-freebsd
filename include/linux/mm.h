#ifndef _LINUX_MM_H_
#define _LINUX_MM_H_

#include <linux/version.h>

struct vm_area_struct {
#if D_VERSION_LINEAR >= 0x20091217
	vm_ooffset_t offset;
#else
	vm_offset_t offset;
#endif
	vm_paddr_t *paddr;
	int nprot;
};

#endif /* _LINUX_MM_H_ */

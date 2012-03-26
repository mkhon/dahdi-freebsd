#ifndef _LINUX_KREF_H_
#define _LINUX_KREF_H_

#include <linux/types.h>

struct kref {
	volatile u_int refcount;
};

void kref_set(struct kref *kref, int num);
void kref_init(struct kref *kref);
void kref_get(struct kref *kref);
int kref_put(struct kref *kref, void (*release) (struct kref *kref));

#endif /* _LINUX_KREF_H_ */

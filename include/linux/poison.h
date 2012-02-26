#ifndef _LINUX_POISON_H_
#define _LINUX_POISON_H_

/*
 * These are non-NULL pointers that will result in page faults
 * under normal circumstances, used to verify that nobody uses
 * non-initialized list entries.
 */
#define LIST_POISON1  ((void *) 0x00100100)
#define LIST_POISON2  ((void *) 0x00200200)

#endif /* _LINUX_POISON_H_ */

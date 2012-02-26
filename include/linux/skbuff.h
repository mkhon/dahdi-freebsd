#ifndef _LINUX_SKBUFF_H_
#define _LINUX_SKBUFF_H_

#include <sys/mbuf.h>

#define sk_buff mbuf

struct sk_buff *dev_alloc_skb(unsigned int length);
void kfree_skb(struct sk_buff *skb);
void dev_kfree_skb(struct sk_buff *skb);

#endif /* _LINUX_SKBUFF_H_ */

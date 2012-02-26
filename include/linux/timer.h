#ifndef _LINUX_TIMER_H_
#define _LINUX_TIMER_H_

#include <sys/lock.h>
#include <sys/mutex.h>
#include <linux/jiffies.h>

struct timer_list {
	struct mtx mtx;
	struct callout callout;

	unsigned long expires;
	void (*function)(unsigned long);
	unsigned long data;
};

void init_timer(struct timer_list *t);
void setup_timer(struct timer_list *t, void (*function)(unsigned long), unsigned long data);
void mod_timer(struct timer_list *t, unsigned long expires);
void add_timer(struct timer_list *t);
int del_timer(struct timer_list *t);
int del_timer_sync(struct timer_list *t);

#endif /* _LINUX_TIMER_H_ */

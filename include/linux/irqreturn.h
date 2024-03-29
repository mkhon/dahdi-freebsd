#ifndef _LINUX_IRQRETURN_H_
#define _LINUX_IRQRETURN_H_

#include <sys/bus.h>

#define IRQ_NONE        FILTER_STRAY
#define IRQ_HANDLED     FILTER_HANDLED
#define IRQ_WAKE_THREAD	FILTER_SCHEDULE_THREAD
#define IRQ_RETVAL(x)   ((x) ? FILTER_HANDLED : FILTER_STRAY)

typedef int irqreturn_t;

#endif /* _LINUX_IRQRETURN_H_ */

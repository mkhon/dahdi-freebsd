#ifndef _ASM_BUG_H_
#define _ASM_BUG_H_

#include <sys/cdefs.h>

#define WARN_ON(condition)					\
({								\
	int __ret_warn_on = !!(condition);			\
	if (unlikely(__ret_warn_on))				\
		printf("WARN_ON: %s\n", #condition);		\
	unlikely(__ret_warn_on);				\
})

#define WARN_ON_ONCE(condition) ({				\
	static int __warned;					\
	int __ret_warn_once = !!(condition);			\
								\
	if (unlikely(__ret_warn_once))				\
		if (WARN_ON(!__warned))				\
			__warned = 1;				\
	unlikely(__ret_warn_once);				\
})

#define BUG_ON(condition)					\
	do {							\
		if (condition)					\
			panic("BUG_ON: %s", #condition);	\
	} while (0)

#endif /* _ASM_BUG_H_ */

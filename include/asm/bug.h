#ifndef _ASM_BUG_H_
#define _ASM_BUG_H_

#define WARN_ON(condition)				\
({							\
	int __ret_warn_on = !!(condition);		\
	if (unlikely(__ret_warn_on))			\
		printf("WARN_ON: " #condition "\n");	\
	unlikely(__ret_warn_on);			\
})

#define WARN_ON_ONCE(condition) ({			\
	static int __warned;				\
	int __ret_warn_once = !!(condition);		\
							\
	if (unlikely(__ret_warn_once))			\
		if (WARN_ON(!__warned))			\
			__warned = 1;			\
	unlikely(__ret_warn_once);			\
})

#define BUG_ON(cond)					\
	do {						\
		if (cond)				\
			panic("BUG_ON: " #cond);	\
	} while (0)

#endif /* _ASM_BUG_H_ */

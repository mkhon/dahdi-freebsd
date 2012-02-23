#ifndef _STDBOOL_H_
#define _STDBOOL_H_

#include <sys/types.h>

#ifndef __bool_true_false_are_defined
typedef int bool;

enum {
	false,
	true
};
#endif

#endif /* _STDBOOL_H_ */

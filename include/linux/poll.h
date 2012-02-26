#ifndef _LINUX_POLL_H_
#define _LINUX_POLL_H_

#include <sys/poll.h>
#include <linux/string.h>
#include <linux/uaccess.h>

typedef struct poll_table_struct {
	struct selinfo *selinfo;
} poll_table;

#endif /* _LINUX_POLL_H_ */

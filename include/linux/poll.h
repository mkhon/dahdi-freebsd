#ifndef _LINUX_POLL_H_
#define _LINUX_POLL_H_

#include <sys/poll.h>
#include <sys/selinfo.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>

typedef struct poll_table_struct {
	/* intentionally left empty */
} poll_table;

static inline void poll_wait(struct file *fp, wait_queue_head_t *wait_address, poll_table *p)
{
	selrecord(curthread, &fp->selinfo);
}

#endif /* _LINUX_POLL_H_ */

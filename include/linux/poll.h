#ifndef _LINUX_POLL_H_
#define _LINUX_POLL_H_

#include <sys/poll.h>
#include <sys/selinfo.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>

typedef struct poll_table_struct {
	struct selinfo *selinfo;
} poll_table;

#define poll_wait(fp, wait_address, poll_table) selrecord(curthread, &fp->selinfo)

#endif /* _LINUX_POLL_H_ */

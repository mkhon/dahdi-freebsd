#ifndef _LINUX_FS_H_
#define _LINUX_FS_H_

#include <linux/fcntl.h>
#include <linux/list.h>
#include <linux/wait.h>

#include <linux/bitops.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

#include <sys/selinfo.h>

struct inode;

struct poll_table_struct;

#define FOP_READ_ARGS_DECL	struct file *file, struct uio *uio, size_t count
#define FOP_READ_ARGS		file, uio, count
#define FOP_WRITE_ARGS_DECL	struct file *file, struct uio *uio, size_t count
#define FOP_WRITE_ARGS		file, uio, count

struct file_operations {
	struct module *owner;
	int (*open)(struct inode *inode, struct file *file);
	int (*release)(struct inode *inode, struct file *file);
	int (*ioctl)(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data);
	ssize_t (*read)(FOP_READ_ARGS_DECL);
	ssize_t (*write)(FOP_WRITE_ARGS_DECL);
	unsigned int (*poll)(struct file *file, struct poll_table_struct *wait_table);
	int (*mmap)(struct file *file, struct vm_area_struct *vma);
};

struct file {
	struct cdev *dev;
	int f_flags;
	void *private_data;
	struct selinfo selinfo;
	const struct file_operations *f_op;
};

#endif /* _LINUX_FS_H_ */

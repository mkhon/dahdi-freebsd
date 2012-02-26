#ifndef _LINUX_KERNEL_H_
#define _LINUX_KERNEL_H_

#include <asm/bug.h>
#include <asm/byteorder.h>
#include <linux/types.h>
#include <sys/cdefs.h>
#include <sys/systm.h>
#include <machine/stdarg.h>

#define printk(fmt, args...)		printf(fmt, ##args)
#define vprintk(fmt, args)		vprintf(fmt, args)

#define pr_info(fmt, args...)		printf(fmt, ##args)

int printk_ratelimit(void);

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define KERN_EMERG	"<0>"	/* system is unusable			*/
#define KERN_ALERT	"<1>"	/* action must be taken immediately	*/
#define KERN_CRIT	"<2>"	/* critical conditions			*/
#define KERN_ERR	"<3>"	/* error conditions			*/
#define KERN_WARNING	"<4>"	/* warning conditions			*/
#define KERN_NOTICE	"<5>"	/* normal but significant condition	*/
#define KERN_INFO	"<6>"	/* informational			*/
#define KERN_DEBUG	"<7>"	/* debug-level messages			*/
#define KERN_CONT	""

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	__typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define might_sleep()

#define clamp(x, low, high) min(max(low, x), high)

#endif /* _LINUX_KERNEL_H_ */

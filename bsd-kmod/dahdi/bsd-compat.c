/*-
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Max Khon under sponsorship from
 * the FreeBSD Foundation and Ethon Technologies GmbH.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#include <dahdi/kernel.h>
#include <asm/atomic.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/ppp_defs.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/workqueue.h>

#include <sys/syscallsubr.h>	/* kern_kldload() */
#include <sys/refcount.h>
#include <sys/sbuf.h>

SYSCTL_NODE(, OID_AUTO, dahdi, CTLFLAG_RW, 0, "DAHDI");
SYSCTL_NODE(_dahdi, OID_AUTO, echocan, CTLFLAG_RW, 0, "DAHDI Echo Cancelers");

/*
 * Tasklet API
 */
static void
tasklet_run(void *context, int pending)
{
	struct tasklet_struct *t = (struct tasklet_struct *) context;

	if (atomic_read(&t->disable_count))
		return;
	t->func(t->data);
}

void
tasklet_init(struct tasklet_struct *t, void (*func)(unsigned long), unsigned long data)
{
	TASK_INIT(&t->task, PI_REALTIME, tasklet_run, t);
	t->func = func;
	t->data = data;
	t->disable_count = 0;
}

void
tasklet_hi_schedule(struct tasklet_struct *t)
{
	taskqueue_enqueue_fast(taskqueue_fast, &t->task);
}

void
tasklet_disable(struct tasklet_struct *t)
{
	atomic_inc(&t->disable_count);
}

void
tasklet_enable(struct tasklet_struct *t)
{
	atomic_dec(&t->disable_count);
}

void
tasklet_kill(struct tasklet_struct *t)
{
	taskqueue_drain(taskqueue_fast, &t->task);
}

/*
 * Time API
 */
struct timespec
current_kernel_time(void)
{
	struct timespec ts;

	getnanotime(&ts);
	return ts;
}

/*
 * Timer API
 */
static void
run_timer(void *arg)
{
	struct timer_list *t = (struct timer_list *) arg;
	void (*function)(unsigned long);

	mtx_lock_spin(&t->mtx);
	if (callout_pending(&t->callout)) {
		/* callout was reset */
		mtx_unlock_spin(&t->mtx);
		return;
	}
	if (!callout_active(&t->callout)) {
		/* callout was stopped */
		mtx_unlock_spin(&t->mtx);
		return;
	}
	callout_deactivate(&t->callout);

	function = t->function;
	mtx_unlock_spin(&t->mtx);

	function(t->data);
}

void
init_timer(struct timer_list *t)
{
	mtx_init(&t->mtx, "dahdi timer lock", NULL, MTX_SPIN);
	callout_init(&t->callout, CALLOUT_MPSAFE);
	t->expires = 0;
	/*
	 * function and data are not initialized intentionally:
	 * they are not initialized by Linux implementation too
	 */
}

void
setup_timer(struct timer_list *t, void (*function)(unsigned long), unsigned long data)
{
	t->function = function;
	t->data = data;
	init_timer(t);
}

void
mod_timer(struct timer_list *t, unsigned long expires)
{
	mtx_lock_spin(&t->mtx);
	callout_reset(&t->callout, expires - jiffies, run_timer, t);
	mtx_unlock_spin(&t->mtx);
}

void
add_timer(struct timer_list *t)
{
	mod_timer(t, t->expires);
}

int
del_timer_sync(struct timer_list *t)
{
	mtx_lock_spin(&t->mtx);
	callout_stop(&t->callout);
	mtx_unlock_spin(&t->mtx);

	mtx_destroy(&t->mtx);
	return 0;
}

int
del_timer(struct timer_list *t)
{
	del_timer_sync(t);
	return 0;
}

/*
 * Completion API
 */
void
init_completion(struct completion *c)
{
	cv_init(&c->cv, "DAHDI completion cv");
	mtx_init(&c->lock, "DAHDI completion lock", "condvar", MTX_DEF);
	c->done = 0;
}

void
destroy_completion(struct completion *c)
{
	cv_destroy(&c->cv);
	mtx_destroy(&c->lock);
}

void
wait_for_completion(struct completion *c)
{
	mtx_lock(&c->lock);
	if (!c->done)
		cv_wait(&c->cv, &c->lock);
	c->done--;
	mtx_unlock(&c->lock);
}

int
wait_for_completion_timeout(struct completion *c, unsigned long timeout)
{
	int res = 0;

	mtx_lock(&c->lock);
	if (!c->done)
		res = cv_timedwait(&c->cv, &c->lock, timeout);
	if (res == 0)
		c->done--;
	mtx_unlock(&c->lock);
	return res == 0;
}

void
complete(struct completion *c)
{
	mtx_lock(&c->lock);
	c->done++;
	cv_signal(&c->cv);
	mtx_unlock(&c->lock);
}

/*
 * Semaphore API
 */
#undef sema_init
#undef sema_destroy

void
_linux_sema_init(struct semaphore *s, int value)
{
	sema_init(&s->sema, value, "DAHDI semaphore");
}

void
_linux_sema_destroy(struct semaphore *s)
{
	sema_destroy(&s->sema);
}

void
down(struct semaphore *s)
{
	sema_wait(&s->sema);
}

int
down_interruptible(struct semaphore *s)
{
	sema_wait(&s->sema);
	return 0;
}

int
down_trylock(struct semaphore *s)
{
	return sema_trywait(&s->sema) == 0;
}

void
up(struct semaphore *s)
{
	sema_post(&s->sema);
}

void
_linux_sema_sysinit(struct semaphore *s)
{
	_linux_sema_init(s, 1);
}

/*
 * Workqueue API
 */
static void
_work_run(void *context, int pending)
{
	struct work_struct *work = (struct work_struct *) context;
	work->func(work);
}

void
_linux_work_init(struct _linux_work_init_args *a)
{
	TASK_INIT(&a->work->task, 0, _work_run, a->work);
	a->work->func = a->func;
	a->work->tq = taskqueue_fast;
}

void
schedule_work(struct work_struct *work)
{
	work->tq = taskqueue_fast;
	taskqueue_enqueue_fast(taskqueue_fast, &work->task);
}

void
cancel_work_sync(struct work_struct *work)
{
	taskqueue_drain(work->tq, &work->task);
}

void
flush_work(struct work_struct *work)
{
	taskqueue_drain(work->tq, &work->task);
}

struct workqueue_struct *
create_singlethread_workqueue(const char *name)
{
	int res;
	struct workqueue_struct *wq;

	wq = malloc(sizeof(*wq), M_DAHDI, M_NOWAIT);
	if (wq == NULL)
		return NULL;

	wq->tq = taskqueue_create_fast(name, M_NOWAIT, taskqueue_thread_enqueue, &wq->tq);
	if (wq->tq == NULL) {
		free(wq, M_DAHDI);
		return NULL;
	}

	res = taskqueue_start_threads(&wq->tq, 1, PI_REALTIME, "%s taskq", name);
	if (res) {
		destroy_workqueue(wq);
		return NULL;
	}

	return wq;
}

void
destroy_workqueue(struct workqueue_struct *wq)
{
	taskqueue_free(wq->tq);
	free(wq, M_DAHDI);
}

static void
_flush_workqueue_fn(void *context, int pending)
{
	/* nothing to do */
}

static void
_flush_taskqueue(struct taskqueue **tq)
{
	struct task flushtask;

	PHOLD(curproc);
	TASK_INIT(&flushtask, 0, _flush_workqueue_fn, NULL);
	taskqueue_enqueue_fast(*tq, &flushtask);
	taskqueue_drain(*tq, &flushtask);
	PRELE(curproc);
}

void
flush_workqueue(struct workqueue_struct *wq)
{
	_flush_taskqueue(&wq->tq);
}

void
queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	work->tq = wq->tq;
	taskqueue_enqueue(wq->tq, &work->task);
}

/*
 * Drain taskqueue_fast (schedule_work() requests, including kref release requests)
 */
SYSUNINIT(dahdi_bsd_compat_sysuninit, SI_SUB_KLD, SI_ORDER_ANY,
    _flush_taskqueue, &taskqueue_fast);

/*
 * kref API
 */
void
kref_set(struct kref *kref, int num)
{
	refcount_init(&kref->refcount, 1);
}

void
kref_init(struct kref *kref)
{
	kref_set(kref, 1);
}

void
kref_get(struct kref *kref)
{
	refcount_acquire(&kref->refcount);
}

int
kref_put(struct kref *kref, void (*release) (struct kref *kref))
{
	if (refcount_release(&kref->refcount)) {
		INIT_WORK(&kref->release_work, (work_func_t) release);
		schedule_work(&kref->release_work);
		return 1;
	}

	return 0;
}

/*
 * k[v]asprintf
 */
char *
kvasprintf(gfp_t gfp, const char *fmt, va_list ap)
{
	struct sbuf *sb = sbuf_new_auto();
	char *res;
	int len;

	sbuf_vprintf(sb, fmt, ap);
#if __FreeBSD_version >= 802508
	if (sbuf_finish(sb)) {
		res = NULL;
	} else {
#else
	sbuf_finish(sb);
#endif
		len = sbuf_len(sb);
		res = kmalloc(len + 1, gfp);
		if (res != NULL)
			bcopy(sbuf_data(sb), res, len + 1);
#if __FreeBSD_version >= 802508
	}
#endif
	sbuf_delete(sb);
	return res;
}

char *
kasprintf(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *res;

	va_start(ap, fmt);
	res = kvasprintf(gfp, fmt, ap);
	va_end(ap);

	return res;
}

/*
 * Logging API
 */
int
printk_ratelimit(void)
{
	static struct timeval last_printk;
	static int count;

	return ppsratecheck(&last_printk, &count, 10);
}

/*
 * Kernel module API
 */
void
__module_get(struct module *m)
{
	atomic_inc(&m->refcount);
}

int
try_module_get(struct module *m)
{
	atomic_inc(&m->refcount);
	return (1);
}

void
module_put(struct module *m)
{
	atomic_dec(&m->refcount);
}

int
module_refcount(struct module *m)
{
	return atomic_read(&m->refcount);
}

int
_linux_module_modevent(module_t mod, int type, void *data)
{
	int res = 0;
	struct module *m = (struct module *) data;

	switch (type) {
	case MOD_LOAD:
		if (m->init)
			res = m->init();
		return (-res);
	case MOD_UNLOAD:
		if (m->exit)
			m->exit();
		return (0);
	case MOD_QUIESCE:
		if (module_refcount(m) > 0)
			return (EBUSY);
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

void
_linux_module_ptr_sysinit(void *arg)
{
	struct module_ptr_args *args = (struct module_ptr_args *) arg;
	*args->pfield = args->value;
}

int
request_module(const char *fmt, ...)
{
	va_list ap;
	char modname[128];
	int fileid;

	va_start(ap, fmt);
	vsnprintf(modname, sizeof(modname), fmt, ap);
	va_end(ap);

	return kern_kldload(curthread, modname, &fileid);
}

/*
 * Firmware API
 */
int
request_firmware(const struct firmware **firmware_p, const char *name, struct device *device)
{
	*firmware_p = firmware_get(name);
	return *firmware_p == NULL;
}

void
release_firmware(const struct firmware *firmware)
{
	if (firmware == NULL)
		return;

	firmware_put(firmware, FIRMWARE_UNLOAD);
}

/*
 * Misc API
 */

/*
 * Concatenate src on the end of dst.  At most strlen(dst)+n+1 bytes
 * are written at dst (at most n+1 bytes being appended).  Return dst.
 */
char *
strncat(char * __restrict dst, const char * __restrict src, size_t n)
{
	if (n != 0) {
		char *d = dst;
		const char *s = src;

		while (*d != 0)
			d++;
		do {
			if ((*d = *s++) == 0)
				break;
			d++;
		} while (--n != 0);
		*d = 0;
	}
	return (dst);
}

/*
 * FCS lookup table as calculated by genfcstab.
 */
u_short fcstab[256] = {
	0x0000,	0x1189,	0x2312,	0x329b,	0x4624,	0x57ad,	0x6536,	0x74bf,
	0x8c48,	0x9dc1,	0xaf5a,	0xbed3,	0xca6c,	0xdbe5,	0xe97e,	0xf8f7,
	0x1081,	0x0108,	0x3393,	0x221a,	0x56a5,	0x472c,	0x75b7,	0x643e,
	0x9cc9,	0x8d40,	0xbfdb,	0xae52,	0xdaed,	0xcb64,	0xf9ff,	0xe876,
	0x2102,	0x308b,	0x0210,	0x1399,	0x6726,	0x76af,	0x4434,	0x55bd,
	0xad4a,	0xbcc3,	0x8e58,	0x9fd1,	0xeb6e,	0xfae7,	0xc87c,	0xd9f5,
	0x3183,	0x200a,	0x1291,	0x0318,	0x77a7,	0x662e,	0x54b5,	0x453c,
	0xbdcb,	0xac42,	0x9ed9,	0x8f50,	0xfbef,	0xea66,	0xd8fd,	0xc974,
	0x4204,	0x538d,	0x6116,	0x709f,	0x0420,	0x15a9,	0x2732,	0x36bb,
	0xce4c,	0xdfc5,	0xed5e,	0xfcd7,	0x8868,	0x99e1,	0xab7a,	0xbaf3,
	0x5285,	0x430c,	0x7197,	0x601e,	0x14a1,	0x0528,	0x37b3,	0x263a,
	0xdecd,	0xcf44,	0xfddf,	0xec56,	0x98e9,	0x8960,	0xbbfb,	0xaa72,
	0x6306,	0x728f,	0x4014,	0x519d,	0x2522,	0x34ab,	0x0630,	0x17b9,
	0xef4e,	0xfec7,	0xcc5c,	0xddd5,	0xa96a,	0xb8e3,	0x8a78,	0x9bf1,
	0x7387,	0x620e,	0x5095,	0x411c,	0x35a3,	0x242a,	0x16b1,	0x0738,
	0xffcf,	0xee46,	0xdcdd,	0xcd54,	0xb9eb,	0xa862,	0x9af9,	0x8b70,
	0x8408,	0x9581,	0xa71a,	0xb693,	0xc22c,	0xd3a5,	0xe13e,	0xf0b7,
	0x0840,	0x19c9,	0x2b52,	0x3adb,	0x4e64,	0x5fed,	0x6d76,	0x7cff,
	0x9489,	0x8500,	0xb79b,	0xa612,	0xd2ad,	0xc324,	0xf1bf,	0xe036,
	0x18c1,	0x0948,	0x3bd3,	0x2a5a,	0x5ee5,	0x4f6c,	0x7df7,	0x6c7e,
	0xa50a,	0xb483,	0x8618,	0x9791,	0xe32e,	0xf2a7,	0xc03c,	0xd1b5,
	0x2942,	0x38cb,	0x0a50,	0x1bd9,	0x6f66,	0x7eef,	0x4c74,	0x5dfd,
	0xb58b,	0xa402,	0x9699,	0x8710,	0xf3af,	0xe226,	0xd0bd,	0xc134,
	0x39c3,	0x284a,	0x1ad1,	0x0b58,	0x7fe7,	0x6e6e,	0x5cf5,	0x4d7c,
	0xc60c,	0xd785,	0xe51e,	0xf497,	0x8028,	0x91a1,	0xa33a,	0xb2b3,
	0x4a44,	0x5bcd,	0x6956,	0x78df,	0x0c60,	0x1de9,	0x2f72,	0x3efb,
	0xd68d,	0xc704,	0xf59f,	0xe416,	0x90a9,	0x8120,	0xb3bb,	0xa232,
	0x5ac5,	0x4b4c,	0x79d7,	0x685e,	0x1ce1,	0x0d68,	0x3ff3,	0x2e7a,
	0xe70e,	0xf687,	0xc41c,	0xd595,	0xa12a,	0xb0a3,	0x8238,	0x93b1,
	0x6b46,	0x7acf,	0x4854,	0x59dd,	0x2d62,	0x3ceb,	0x0e70,	0x1ff9,
	0xf78f,	0xe606,	0xd49d,	0xc514,	0xb1ab,	0xa022,	0x92b9,	0x8330,
	0x7bc7,	0x6a4e,	0x58d5,	0x495c,	0x3de3,	0x2c6a,	0x1ef1,	0x0f78
};

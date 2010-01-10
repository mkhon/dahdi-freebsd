/*
 * Copyright (c) 2010 Max Khon <fjoe@FreeBSD.org>
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

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/syscallsubr.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <machine/stdarg.h>

#include <dahdi/compat/bsd.h>

static void
tasklet_run(void *context, int pending)
{
	struct tasklet_struct *t = (struct tasklet_struct *) context;
	t->func(t->data);
}

void
tasklet_init(struct tasklet_struct *t, void (*func)(unsigned long), unsigned long data)
{
	TASK_INIT(&t->task, 0, tasklet_run, t);
	t->func = func;
	t->data = data;
}

void
tasklet_hi_schedule(struct tasklet_struct *t)
{
	taskqueue_enqueue_fast(taskqueue_fast, &t->task);
}

void
tasklet_disable(struct tasklet_struct *t)
{
	// nothing to do
}

void
tasklet_kill(struct tasklet_struct *t)
{
	taskqueue_drain(taskqueue_fast, &t->task);
}

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
	t->function = 0;
	t->data = 0;
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

void
del_timer_sync(struct timer_list *t)
{
	mtx_lock_spin(&t->mtx);
	callout_stop(&t->callout);
	mtx_unlock_spin(&t->mtx);

	mtx_destroy(&t->mtx);
}

void
del_timer(struct timer_list *t)
{
	del_timer_sync(t);
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

void
rlprintf(int pps, const char *fmt, ...)
{
	va_list ap;
	static struct timeval last_printf;
	static int count;

	if (ppsratecheck(&last_printf, &count, pps)) {
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

void
device_rlprintf(int pps, device_t dev, const char *fmt, ...)
{
	va_list ap;
	static struct timeval last_printf;
	static int count;

	if (ppsratecheck(&last_printf, &count, pps)) {
		va_start(ap, fmt);
		device_print_prettyname(dev);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

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

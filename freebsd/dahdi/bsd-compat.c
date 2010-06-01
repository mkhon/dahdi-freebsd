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
#include <sys/firmware.h>
#include <sys/syscallsubr.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/stdarg.h>

#include <dahdi/compat/bsd.h>

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
	taskqueue_enqueue(taskqueue_fast, &t->task);
//	wakeup_one(taskqueue_fast);
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

/*
 * Completion API
 */
void
init_completion(struct completion *c)
{
	cv_init(&c->cv, "DAHDI completion cv");
	mtx_init(&c->lock, "DAHDI completion lock", "condvar", MTX_DEF);
}

void
destroy_completion(struct completion *c)
{
	cv_destroy(&c->cv);
	mtx_destroy(&c->lock);
}

int
wait_for_completion_timeout(struct completion *c, unsigned long timeout)
{
	int res;

	mtx_lock(&c->lock);
	res = cv_timedwait(&c->cv, &c->lock, timeout);
	mtx_unlock(&c->lock);
	return res == 0;
}

void
complete(struct completion *c)
{
	cv_signal(&c->cv);
}

/*
 * Semaphore API
 */
void
_sema_init(struct semaphore *s, int value)
{
	sema_init(&s->sema, value, "DAHDI semaphore");
}

void
_sema_destroy(struct semaphore *s)
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

/*
 * Workqueue API
 */
void
_work_run(void *context, int pending)
{
	struct work_struct *work = (struct work_struct *) context;
	work->func(work);
}

void
schedule_work(struct work_struct *work)
{
	work->tq = taskqueue_fast;
	taskqueue_enqueue(taskqueue_fast, &work->task);
//	wakeup_one(taskqueue_fast);
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

void
queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	work->tq = wq->tq;
	taskqueue_enqueue(wq->tq, &work->task);
//	wakeup_one(wq->tq);
}

/*
 * Logging API
 */
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
request_firmware(const struct firmware **firmware_p, const char *name, device_t *device)
{
	*firmware_p = firmware_get(name);
	return *firmware_p == NULL;
}

void
release_firmware(const struct firmware *firmware)
{
	firmware_put(firmware, FIRMWARE_UNLOAD);
}

/*
 * PCI device API
 */
struct pci_device_id *
dahdi_pci_device_id_lookup(device_t dev, struct pci_device_id *tbl)
{
	struct pci_device_id *id;
	uint16_t vendor = pci_get_vendor(dev);
	uint16_t device = pci_get_device(dev);
	uint16_t subvendor = pci_get_subvendor(dev);
	uint16_t subdevice = pci_get_subdevice(dev);

	for (id = tbl; id->vendor != 0; id++) {
		if ((id->vendor == PCI_ANY_ID || id->vendor == vendor) &&
		    (id->device == PCI_ANY_ID || id->device == device) &&
		    (id->subvendor == PCI_ANY_ID || id->subvendor == subvendor) &&
		    (id->subdevice == PCI_ANY_ID || id->subdevice == subdevice))
			return id;
	}

	return NULL;
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
 * DMA API
 */
void
dahdi_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	uint32_t *paddr = arg;
	*paddr = segs->ds_addr;
}

int
dahdi_dma_allocate(int size, bus_dma_tag_t *ptag, bus_dmamap_t *pmap, void **pvaddr, uint32_t *ppaddr)
{
	int res;

	res = bus_dma_tag_create(NULL, 8, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    size, 1, size, BUS_DMA_ALLOCNOW, NULL, NULL, ptag);
	if (res)
		return (res);

	res = bus_dmamem_alloc(*ptag, pvaddr, BUS_DMA_NOWAIT | BUS_DMA_ZERO, pmap);
	if (res) {
		bus_dma_tag_destroy(*ptag);
		*ptag = NULL;
		return (res);
	}

	res = bus_dmamap_load(*ptag, *pmap, *pvaddr, size, dahdi_dma_map_addr, ppaddr, 0);
	if (res) {
		bus_dmamem_free(*ptag, *pvaddr, *pmap);
		*pvaddr = NULL;

		bus_dmamap_destroy(*ptag, *pmap);
		*pmap = NULL;

		bus_dma_tag_destroy(*ptag);
		*ptag = NULL;
		return (res);
	}

	return (0);
}

void
dahdi_dma_free(bus_dma_tag_t *ptag, bus_dmamap_t *pmap, void **pvaddr, uint32_t *ppaddr)
{
	if (*ppaddr != 0) {
		bus_dmamap_unload(*ptag, *pmap);
		*ppaddr = 0;
	}
	if (*pvaddr != NULL) {
		bus_dmamem_free(*ptag, *pvaddr, *pmap);
		*pvaddr = NULL;

		bus_dmamap_destroy(*ptag, *pmap);
		*pmap = NULL;
	}
	if (*ptag != NULL) {
		bus_dma_tag_destroy(*ptag);
		*ptag = NULL;
	}
}

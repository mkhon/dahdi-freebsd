/*
 * Dummy DAHDI Driver for DAHDI Telephony interface
 *
 * Required:  kernel compiled with "options HZ=1000" 
 * Written by Chris Stenton <jacs@gnome.co.uk>
 * 
 * Copyright (C) 2004, Digium, Inc.
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Rewritten to use the time of day clock (which should be ntp synced
 * for this to work perfectly) by David G. Lawrence <dg@dglawrence.com>.
 * July 27th, 2007.
 *
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <sys/conf.h>
#include <sys/errno.h>

#include <dahdi/kernel.h>

struct dahdi_dummy {
	struct dahdi_span span;
	struct dahdi_chan _chan;
	struct dahdi_chan *chan;
};

MALLOC_DEFINE(M_DAHDIDUMMY, "dahdy_dummy", "DAHDI dummy interface data structures");

#ifndef timersub
#define timersub(tvp, uvp, vvp)                                         \
        do {                                                            \
                (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;          \
                (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;       \
                if ((vvp)->tv_usec < 0) {                               \
                        (vvp)->tv_sec--;                                \
                        (vvp)->tv_usec += 1000000;                      \
                }                                                       \
        } while (0)
#endif

static struct callout_handle dahdi_dummy_timer_handle = CALLOUT_HANDLE_INITIALIZER(&dahdi_dummy_timer_handle);

static struct dahdi_dummy *dahdi_d;

static int debug = 0;
static struct timeval basetime, curtime, sleeptime;

static __inline void
dahdi_dummy_timer(void *arg)
{
	int i, ticks;

loop:
	for (i = 0; i < hz / 100; i++) {
		dahdi_receive(&dahdi_d->span);
		dahdi_transmit(&dahdi_d->span);
	}

fixtime:
	microtime(&curtime);

	/*
	 * Sleep until the next 10ms boundry.
	 */
	basetime.tv_usec += 10000;
	if (basetime.tv_usec >= 1000000) {
		basetime.tv_sec++;
		basetime.tv_usec -= 1000000;
	}
	timersub(&basetime, &curtime, &sleeptime);

	/*
	 * Detect if we've gotten behind and need to start our processing
	 * immediately.
	 */
	if (sleeptime.tv_sec < 0 || sleeptime.tv_usec == 0) {
		/*
		 * Limit how far we can get behind to something reasonable (1 sec)
		 * so that we don't go nuts when something (ntp or admin) sets the
		 * clock forward by a large amount.
		 */
		if (sleeptime.tv_sec < -1) {
			basetime.tv_sec = curtime.tv_sec;
			basetime.tv_usec = curtime.tv_usec;
			goto fixtime;
		}
		goto loop;
	}
	/*
	 * Detect if something is messing with the system clock by
	 * checking that the sleep time is no more than 20ms and
	 * resetting our base time if it is. This case will occur if
	 * the system clock has been reset to an earlier time.
	 */
	if (sleeptime.tv_sec > 0 || sleeptime.tv_usec > 20000) {
		basetime.tv_sec = curtime.tv_sec;
		basetime.tv_usec = curtime.tv_usec;
		goto fixtime;
	}

	ticks = sleeptime.tv_usec * hz / 1000000;
	if (ticks == 0)
		goto loop;

	dahdi_dummy_timer_handle = timeout(dahdi_dummy_timer, NULL, ticks);
}

static int
dahdi_dummy_initialize(struct dahdi_dummy *dahdi_d)
{
	/* DAHDI stuff */
	dahdi_d->chan = &dahdi_d->_chan;
	sprintf(dahdi_d->span.name, "DAHDI_DUMMY/1");
	sprintf(dahdi_d->span.desc, "%s %d", dahdi_d->span.name, 1);
	sprintf(dahdi_d->chan->name, "DAHDI_DUMMY/%d/%d", 1, 0);
	dahdi_copy_string(dahdi_d->span.devicetype, "DAHDI Dummy Timing", sizeof(dahdi_d->span.devicetype));
	dahdi_d->chan->chanpos = 1;
	dahdi_d->span.chans = &dahdi_d->chan;
	dahdi_d->span.channels = 0;		/* no channels on our span */
	dahdi_d->span.deflaw = DAHDI_LAW_MULAW;
	dahdi_d->span.pvt = dahdi_d;
	dahdi_d->chan->pvt = dahdi_d;
	if (dahdi_register(&dahdi_d->span, 0)) {
		return -1;
	}
	return 0;
}

static int
dahdi_dummy_attach(void)
{
	dahdi_d = malloc(sizeof(struct dahdi_dummy), M_DAHDIDUMMY, M_NOWAIT | M_ZERO);
	if (dahdi_d == NULL) {
		printf("dahdi_dummy: Unable to allocate memory\n");
		return ENOMEM;
	}

	if (dahdi_dummy_initialize(dahdi_d)) {
		printf("dahdi_dummy: Unable to intialize zaptel driver\n");
		free(dahdi_d, M_DAHDIDUMMY);
		return ENODEV;
	}

	microtime(&basetime);
	dahdi_dummy_timer_handle = timeout(dahdi_dummy_timer, NULL, 1);

	if (debug)
		printf("dahdi_dummy: init() finished\n");
	return 0;
}

static void
cleanup_module(void)
{
	untimeout(dahdi_dummy_timer, NULL, dahdi_dummy_timer_handle);
	dahdi_unregister(&dahdi_d->span);
	free(dahdi_d, M_DAHDIDUMMY);

	if (debug)
		printf("dahdi_dummy: cleanup() finished\n");
}

static int
dahdi_dummy_modevent(module_t mod,int  type, void*  data)
{
	int ret;

	switch (type) {
	case MOD_LOAD:
		 ret = dahdi_dummy_attach();
		 if (ret)
			 return (ret);
		 printf("dahdi_dummy: loaded\n");
		 if (hz < 1000) {
			 printf("dahdi_dummy: WARNING Ticker rate only %d. Timer will not work well!!\nRecompile kernel with \"options HZ=1000\"\n", hz);
		 }
		break;

	case MOD_UNLOAD:
		cleanup_module();
		printf("dahdi_dummy: unloaded\n");
		break;

	default:
		return EOPNOTSUPP;
	}
	return 0;
}

MODULE_VERSION(dahdi_dummy, 1);
MODULE_DEPEND(dahdi_dummy, dahdi, 1, 1, 1);
DEV_MODULE(dahdi_dummy, dahdi_dummy_modevent, NULL);

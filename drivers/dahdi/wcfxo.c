/*
 * Wildcard X100P FXO Interface Driver for DAHDI Telephony interface
 *
 * Written by Mark Spencer <markster@digium.com>
 *            Matthew Fredrickson <creslin@digium.com>
 *
 * Copyright (C) 2001-2008, Digium, Inc.
 *
 * All rights reserved.
 *
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define set_current_state(x)
#else /* !__FreeBSD__ */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/moduleparam.h>
#endif /* !__FreeBSD__ */

#include <dahdi/kernel.h>

/* Uncomment to enable tasklet handling in the FXO driver.  Not recommended
   in general, but may improve interactive performance */

/* #define ENABLE_TASKLETS */

/* Un-comment the following for POTS line support for Japan */
/* #define	JAPAN */

/* Un-comment for lines (eg from and ISDN TA) that remove */
/* phone power during ringing                             */
/* #define ZERO_BATT_RING */

#define WC_MAX_IFACES 128

#define WC_CNTL    	0x00
#define WC_OPER		0x01
#define WC_AUXC    	0x02
#define WC_AUXD    	0x03
#define WC_MASK0   	0x04
#define WC_MASK1   	0x05
#define WC_INTSTAT 	0x06

#define WC_DMAWS	0x08
#define WC_DMAWI	0x0c
#define WC_DMAWE	0x10
#define WC_DMARS	0x18
#define WC_DMARI	0x1c
#define WC_DMARE	0x20

#define WC_AUXFUNC	0x2b
#define WC_SERCTL	0x2d
#define WC_FSCDELAY	0x2f


/* DAA registers */
#define WC_DAA_CTL1  		1
#define WC_DAA_CTL2  		2
#define WC_DAA_DCTL1 		5
#define WC_DAA_DCTL2		6
#define WC_DAA_PLL1_N1	 	7
#define WC_DAA_PLL1_M1	 	8
#define WC_DAA_PLL2_N2_M2 	9
#define WC_DAA_PLL_CTL   	10
#define WC_DAA_CHIPA_REV 	11
#define WC_DAA_LINE_STAT 	12
#define WC_DAA_CHIPB_REV 	13
#define WC_DAA_DAISY_CTL	14
#define WC_DAA_TXRX_GCTL	15
#define WC_DAA_INT_CTL1 	16
#define WC_DAA_INT_CTL2 	17
#define WC_DAA_INT_CTL3 	18
#define WC_DAA_INT_CTL4 	19


#define FLAG_EMPTY	0
#define FLAG_WRITE	1
#define FLAG_READ	2

#ifdef 	ZERO_BATT_RING			/* Need to debounce Off/On hook too */
#define	JAPAN
#endif

#define RING_DEBOUNCE	64		/* Ringer Debounce (in ms) */
#ifdef	JAPAN
#define BATT_DEBOUNCE	30		/* Battery debounce (in ms) */
#define OH_DEBOUNCE	350		/* Off/On hook debounce (in ms) */
#else
#define BATT_DEBOUNCE	80		/* Battery debounce (in ms) */
#endif

#define MINPEGTIME	10 * 8		/* 30 ms peak to peak gets us no more than 100 Hz */
#define PEGTIME		50 * 8		/* 50ms peak to peak gets us rings of 10 Hz or more */
#define PEGCOUNT	5		/* 5 cycles of pegging means RING */

#define	wcfxo_printk(level, span, fmt, ...)	\
	printk(KERN_ ## level "%s-%s: %s: " fmt, #level,	\
		THIS_MODULE->name, (span).name, ## __VA_ARGS__)

#define wcfxo_notice(span, fmt, ...) \
	wcfxo_printk(NOTICE, span, fmt, ## __VA_ARGS__)

#define wcfxo_dbg(span, fmt, ...) \
	((void)((debug) && wcfxo_printk(DEBUG, span, "%s: " fmt, \
			__FUNCTION__, ## __VA_ARGS__) ) )

struct reg {
	unsigned long flags;
	unsigned char index;
	unsigned char reg;
	unsigned char value;
};

static int wecareregs[] = 
{ 
	WC_DAA_DCTL1, WC_DAA_DCTL2, WC_DAA_PLL2_N2_M2, WC_DAA_CHIPA_REV, 
	WC_DAA_LINE_STAT, WC_DAA_CHIPB_REV, WC_DAA_INT_CTL2, WC_DAA_INT_CTL4, 
};

struct wcfxo {
#if defined(__FreeBSD__)
	struct pci_dev _dev;
#endif
	struct pci_dev *dev;
	char *variety;
	struct dahdi_span span;
	struct dahdi_chan _chan;
	struct dahdi_chan *chan;
	int usecount;
	int dead;
	int pos;
	unsigned long flags;
	int freeregion;
	int ring;
	int offhook;
	int battery;
	int wregcount;
	int readpos;
	int rreadpos;
	unsigned int pegtimer;
	int pegcount;
	int peg;
	int battdebounce;
	int nobatttimer;
	int ringdebounce;
#ifdef	JAPAN
	int ohdebounce;
#endif
	int allread;
	int regoffset;			/* How far off our registers are from what we expect */
	int alt;
	int ignoreread;
	int reset;
	/* Up to 6 register can be written at a time */
	struct reg regs[DAHDI_CHUNKSIZE];
	struct reg oldregs[DAHDI_CHUNKSIZE];
	unsigned char lasttx[DAHDI_CHUNKSIZE];
	/* Up to 32 registers of whatever we most recently read */
	unsigned char readregs[32];
#if defined(__FreeBSD__)
	struct resource *io_res;
	int io_rid;

	struct resource *irq_res;		/* resource for irq */
	int irq_rid;
	void *irq_handle;

	uint32_t readdma;
	bus_dma_tag_t   read_dma_tag;
	bus_dmamap_t    read_dma_map;

	uint32_t writedma;
	bus_dma_tag_t   write_dma_tag;
	bus_dmamap_t    write_dma_map;
#else /* !__FreeBSD__ */
	unsigned long ioaddr;
	dma_addr_t 	readdma;
	dma_addr_t	writedma;
#endif /* !__FreeBSD__ */
	volatile int *writechunk;					/* Double-word aligned write memory */
	volatile int *readchunk;					/* Double-word aligned read memory */
#ifdef ZERO_BATT_RING
	int onhook;
#endif
#ifdef ENABLE_TASKLETS
	int taskletrun;
	int taskletsched;
	int taskletpending;
	int taskletexec;
	int txerrors;
	int ints;
	struct tasklet_struct wcfxo_tlet;
#endif
};

#define FLAG_INVERTSER		(1 << 0)
#define FLAG_USE_XTAL		(1 << 1)
#define FLAG_DOUBLE_CLOCK	(1 << 2)
#define FLAG_RESET_ON_AUX5	(1 << 3)
#define FLAG_NO_I18N_REGS	(1 << 4) /*!< Uses si3035, rather si3034 */

struct wcfxo_desc {
	char *name;
	unsigned long flags;
};


static struct wcfxo_desc wcx100p = { "Wildcard X100P",
		FLAG_INVERTSER | FLAG_USE_XTAL | FLAG_DOUBLE_CLOCK };

static struct wcfxo_desc wcx101p = { "Wildcard X101P",
		FLAG_USE_XTAL | FLAG_DOUBLE_CLOCK };

static struct wcfxo_desc generic = { "Generic Clone",
		FLAG_USE_XTAL | FLAG_DOUBLE_CLOCK };

#if !defined(__FreeBSD__)
static struct wcfxo *ifaces[WC_MAX_IFACES];

static void wcfxo_release(struct wcfxo *wc);
#endif

static int debug = 0;

static int monitor = 0;

static int quiet = 0;

static int boost = 0;

static int opermode = 0;

static struct fxo_mode {
	char *name;
	int ohs;
	int act;
	int dct;
	int rz;
	int rt;
	int lim;
	int vol;
} fxo_modes[] =
{
	{ "FCC", 0, 0, 2, 0, 0, 0, 0 }, 	/* US */
	{ "CTR21", 0, 0, 3, 0, 0, 3, 0 },	/* Austria, Belgium, Denmark, Finland, France, Germany, 
										   Greece, Iceland, Ireland, Italy, Luxembourg, Netherlands,
										   Norway, Portugal, Spain, Sweden, Switzerland, and UK */
};

static inline u8
wcfxo_inb(struct wcfxo *wc, int reg)
{
#if defined(__FreeBSD__)
	return bus_space_read_1(rman_get_bustag(wc->io_res),
	    rman_get_bushandle(wc->io_res), reg);
#else
	return inb(wc->ioaddr + reg);
#endif
}

static inline void
wcfxo_outb(struct wcfxo *wc, int reg, u8 value)
{
#if defined(__FreeBSD__)
	bus_space_write_1(rman_get_bustag(wc->io_res),
	    rman_get_bushandle(wc->io_res), reg, value);
#else
	outb(value, wc->ioaddr + reg);
#endif
}

static inline void
wcfxo_outl(struct wcfxo *wc, int reg, u32 value)
{
#if defined(__FreeBSD__)
	bus_space_write_4(rman_get_bustag(wc->io_res),
	    rman_get_bushandle(wc->io_res), reg, value);
#else
	outl(value, wc->ioaddr + reg);
#endif
}

static inline void wcfxo_transmitprep(struct wcfxo *wc, unsigned char ints)
{
	volatile int *writechunk;
	int x;
	int written=0;
	unsigned short cmd;

	/* if nothing to transmit, have to do the dahdi_transmit() anyway */
	if (!(ints & 3)) {
		/* Calculate Transmission */
		dahdi_transmit(&wc->span);
		return;
	}

	/* Remember what it was we just sent */
	memcpy(wc->lasttx, wc->chan->writechunk, DAHDI_CHUNKSIZE);

	if (ints & 0x01)  {
		/* Write is at interrupt address.  Start writing from normal offset */
		writechunk = wc->writechunk;
	} else {
		writechunk = wc->writechunk + DAHDI_CHUNKSIZE * 2;
	}

	dahdi_transmit(&wc->span);

	for (x=0;x<DAHDI_CHUNKSIZE;x++) {
		/* Send a sample, as a 32-bit word, and be sure to indicate that a command follows */
		if (wc->flags & FLAG_INVERTSER)
			writechunk[x << 1] = cpu_to_le32(
				~((unsigned short)(DAHDI_XLAW(wc->chan->writechunk[x], wc->chan))| 0x1) << 16
				);
		else
			writechunk[x << 1] = cpu_to_le32(
				((unsigned short)(DAHDI_XLAW(wc->chan->writechunk[x], wc->chan))| 0x1) << 16
				);

		/* We always have a command to follow our signal */
		if (!wc->regs[x].flags) {
			/* Fill in an empty register command with a read for a potentially useful register  */
			wc->regs[x].flags = FLAG_READ;
			wc->regs[x].reg = wecareregs[wc->readpos];
			wc->regs[x].index = wc->readpos;
			wc->readpos++;
			if (wc->readpos >= (sizeof(wecareregs) / sizeof(wecareregs[0]))) {
				wc->allread = 1;
				wc->readpos = 0;
			}
		}

		/* Prepare the command to follow it */
		switch(wc->regs[x].flags) {
		case FLAG_READ:
			cmd = (wc->regs[x].reg | 0x20) << 8;
			break;
		case FLAG_WRITE:
			cmd = (wc->regs[x].reg << 8) | (wc->regs[x].value & 0xff);
			written = 1;
			/* Wait at least four samples before reading */
			wc->ignoreread = 4;
			break;
		default:
			printk(KERN_DEBUG "wcfxo: Huh?  No read or write??\n");
			cmd = 0;
		}
		/* Setup the write chunk */
		if (wc->flags & FLAG_INVERTSER)
			writechunk[(x << 1) + 1] = cpu_to_le32(~(cmd << 16));
		else
			writechunk[(x << 1) + 1] = cpu_to_le32(cmd << 16);
	}
	if (written)
		wc->readpos = 0;
	wc->wregcount = 0;

	for (x=0;x<DAHDI_CHUNKSIZE;x++) {
		/* Rotate through registers */
		wc->oldregs[x] = wc->regs[x];
		wc->regs[x].flags = FLAG_EMPTY;
	}

#if defined(__FreeBSD__)
	bus_dmamap_sync(wc->write_dma_tag, wc->write_dma_map, BUS_DMASYNC_PREWRITE);
#endif
}

static inline void wcfxo_receiveprep(struct wcfxo *wc, unsigned char ints)
{
	volatile int *readchunk;
	int x;
	int realreg;
	int realval;
	int sample;
	if (ints & 0x04)
		/* Read is at interrupt address.  Valid data is available at normal offset */
		readchunk = wc->readchunk;
	else
		readchunk = wc->readchunk + DAHDI_CHUNKSIZE * 2;
#if defined(__FreeBSD__)
	bus_dmamap_sync(wc->read_dma_tag, wc->read_dma_map, BUS_DMASYNC_POSTREAD);
#endif

	/* Keep track of how quickly our peg alternates */
	wc->pegtimer+=DAHDI_CHUNKSIZE;
	for (x=0;x<DAHDI_CHUNKSIZE;x++) {

		/* We always have a command to follow our signal.  */
		if (wc->oldregs[x].flags == FLAG_READ && !wc->ignoreread) {
			realreg = wecareregs[(wc->regs[x].index + wc->regoffset) %
							(sizeof(wecareregs) / sizeof(wecareregs[0]))];
			realval = (le32_to_cpu(readchunk[(x << 1) +wc->alt]) >> 16) & 0xff;
			if ((realval == 0x89) && (realreg != WC_DAA_PLL2_N2_M2)) {
				/* Some sort of slippage, correct for it */
				while(realreg != WC_DAA_PLL2_N2_M2) {
					/* Find register 9 */
					realreg = wecareregs[(wc->regs[x].index + ++wc->regoffset) %
										 (sizeof(wecareregs) / sizeof(wecareregs[0]))];
					wc->regoffset = wc->regoffset % (sizeof(wecareregs) / sizeof(wecareregs[0]));
				}
				if (debug)
					printk(KERN_DEBUG "New regoffset: %d\n", wc->regoffset);
			}
			/* Receive into the proper register */
			wc->readregs[realreg] = realval;
		}
		/* Look for pegging to indicate ringing */
		sample = (short)(le32_to_cpu(readchunk[(x << 1) + (1 - wc->alt)]) >> 16);
		if ((sample > 32000) && (wc->peg != 1)) {
			if ((wc->pegtimer < PEGTIME) && (wc->pegtimer > MINPEGTIME))
				wc->pegcount++;
			wc->pegtimer = 0;
			wc->peg = 1;
		} else if ((sample < -32000) && (wc->peg != -1)) {
			if ((wc->pegtimer < PEGTIME) && (wc->pegtimer > MINPEGTIME))
				wc->pegcount++;
			wc->pegtimer = 0;
			wc->peg = -1;
		}
		wc->chan->readchunk[x] = DAHDI_LIN2X((sample), (wc->chan));
	}
	if (wc->pegtimer > PEGTIME) {
		/* Reset pegcount if our timer expires */
		wc->pegcount = 0;
	}
	/* Decrement debouncer if appropriate */
	if (wc->ringdebounce)
		wc->ringdebounce--;
	if (!wc->offhook && !wc->ringdebounce) {
		if (!wc->ring && (wc->pegcount > PEGCOUNT)) {
			/* It's ringing */
			if (debug)
				printk(KERN_DEBUG "RING!\n");
			dahdi_hooksig(wc->chan, DAHDI_RXSIG_RING);
			wc->ring = 1;
		}
		if (wc->ring && !wc->pegcount) {
			/* No more ring */
			if (debug)
				printk(KERN_DEBUG "NO RING!\n");
			dahdi_hooksig(wc->chan, DAHDI_RXSIG_OFFHOOK);
			wc->ring = 0;
		}
	}
	if (wc->ignoreread)
		wc->ignoreread--;

	/* Do the echo cancellation...  We are echo cancelling against
	   what we sent two chunks ago*/
	dahdi_ec_chunk(wc->chan, wc->chan->readchunk, wc->lasttx);

	/* Receive the result */
	dahdi_receive(&wc->span);
}

#ifdef ENABLE_TASKLETS
static void wcfxo_tasklet(unsigned long data)
{
	struct wcfxo *wc = (struct wcfxo *)data;
	wc->taskletrun++;
	/* Run tasklet */
	if (wc->taskletpending) {
		wc->taskletexec++;
		wcfxo_receiveprep(wc, wc->ints);
		wcfxo_transmitprep(wc, wc->ints);
	}
	wc->taskletpending = 0;
}
#endif

static void wcfxo_stop_dma(struct wcfxo *wc);
static void wcfxo_restart_dma(struct wcfxo *wc);

DAHDI_IRQ_HANDLER(wcfxo_interrupt)
{
	struct wcfxo *wc = dev_id;
	unsigned char ints;
	unsigned char b;
#ifdef DEBUG_RING
	static int oldb = 0;
	static int oldcnt = 0;
#endif

	ints = wcfxo_inb(wc, WC_INTSTAT);


	if (!ints)
		return IRQ_NONE;

	wcfxo_outb(wc, WC_INTSTAT, ints);

	if (ints & 0x0c) {  /* if there is a rx interrupt pending */
#ifdef ENABLE_TASKLETS
		wc->ints = ints;
		if (!wc->taskletpending) {
			wc->taskletpending = 1;
			wc->taskletsched++;
			tasklet_hi_schedule(&wc->wcfxo_tlet);
		} else
			wc->txerrors++;
#else
		wcfxo_receiveprep(wc, ints);
		/* transmitprep looks to see if there is anything to transmit
		   and returns by itself if there is nothing */
		wcfxo_transmitprep(wc, ints);
#endif
	}

	if (ints & 0x10) {
		printk(KERN_INFO "FXO PCI Master abort\n");
		/* Stop DMA andlet the watchdog start it again */
		wcfxo_stop_dma(wc);
		return IRQ_RETVAL(1);
	}

	if (ints & 0x20) {
		printk(KERN_INFO "PCI Target abort\n");
		return IRQ_RETVAL(1);
	}
	if (1 /* !(wc->report % 0xf) */) {
		/* Check for BATTERY from register and debounce for 8 ms */
		b = wc->readregs[WC_DAA_LINE_STAT] & 0xf;
		if (!b) {
			wc->nobatttimer++;
#if 0
			if (wc->battery)
				printk(KERN_DEBUG "Battery loss: %d (%d debounce)\n", b, wc->battdebounce);
#endif
			if (wc->battery && !wc->battdebounce) {
				if (debug)
					printk(KERN_DEBUG "NO BATTERY!\n");
				wc->battery =  0;
#ifdef	JAPAN
				if ((!wc->ohdebounce) && wc->offhook) {
					dahdi_hooksig(wc->chan, DAHDI_RXSIG_ONHOOK);
					if (debug)
						printk(KERN_DEBUG "Signalled On Hook\n");
#ifdef	ZERO_BATT_RING
					wc->onhook++;
#endif
				}
#else
				dahdi_hooksig(wc->chan, DAHDI_RXSIG_ONHOOK);
#endif
				wc->battdebounce = BATT_DEBOUNCE;
			} else if (!wc->battery)
				wc->battdebounce = BATT_DEBOUNCE;
			if ((wc->nobatttimer > 5000) &&
#ifdef	ZERO_BATT_RING
			    !(wc->readregs[WC_DAA_DCTL1] & 0x04) &&
#endif
			    (!wc->span.alarms)) {
				wc->span.alarms = DAHDI_ALARM_RED;
				dahdi_alarm_notify(&wc->span);
			}
		} else if (b == 0xf) {
			if (!wc->battery && !wc->battdebounce) {
				if (debug)
					printk(KERN_DEBUG "BATTERY!\n");
#ifdef	ZERO_BATT_RING
				if (wc->onhook) {
					wc->onhook = 0;
					dahdi_hooksig(wc->chan, DAHDI_RXSIG_OFFHOOK);
					if (debug)
						printk(KERN_DEBUG "Signalled Off Hook\n");
				}
#else
				dahdi_hooksig(wc->chan, DAHDI_RXSIG_OFFHOOK);
#endif
				wc->battery = 1;
				wc->nobatttimer = 0;
				wc->battdebounce = BATT_DEBOUNCE;
				if (wc->span.alarms) {
					wc->span.alarms = 0;
					dahdi_alarm_notify(&wc->span);
				}
			} else if (wc->battery)
				wc->battdebounce = BATT_DEBOUNCE;
		} else {
			/* It's something else... */
				wc->battdebounce = BATT_DEBOUNCE;
		}

		if (wc->battdebounce)
			wc->battdebounce--;
#ifdef	JAPAN
		if (wc->ohdebounce)
			wc->ohdebounce--;
#endif

	}

	return IRQ_RETVAL(1);
}

static int wcfxo_setreg(struct wcfxo *wc, unsigned char reg, unsigned char value)
{
	int x;
	if (wc->wregcount < DAHDI_CHUNKSIZE) {
		x = wc->wregcount;
		wc->regs[x].reg = reg;
		wc->regs[x].value = value;
		wc->regs[x].flags = FLAG_WRITE;
		wc->wregcount++;
		return 0;
	}
	printk(KERN_NOTICE "wcfxo: Out of space to write register %02x with %02x\n", reg, value);
	return -1;
}

static int wcfxo_open(struct dahdi_chan *chan)
{
	struct wcfxo *wc = chan->pvt;
	if (wc->dead)
		return -ENODEV;
	wc->usecount++;
	return 0;
}

static int wcfxo_watchdog(struct dahdi_span *span, int event)
{
	printk(KERN_INFO "FXO: Restarting DMA\n");
	wcfxo_restart_dma(span->pvt);
	return 0;
}

static int wcfxo_close(struct dahdi_chan *chan)
{
	struct wcfxo *wc = chan->pvt;
	wc->usecount--;
#if !defined(__FreeBSD__)
	/* If we're dead, release us now */
	if (!wc->usecount && wc->dead)
		wcfxo_release(wc);
#endif /* !__FreeBSD__ */
	return 0;
}

static int wcfxo_hooksig(struct dahdi_chan *chan, enum dahdi_txsig txsig)
{
	struct wcfxo *wc = chan->pvt;
	int reg=0;
	switch(txsig) {
	case DAHDI_TXSIG_START:
	case DAHDI_TXSIG_OFFHOOK:
		/* Take off hook and enable normal mode reception.  This must
		   be done in two steps because of a hardware bug. */
		reg = wc->readregs[WC_DAA_DCTL1] & ~0x08;
		wcfxo_setreg(wc, WC_DAA_DCTL1, reg);

		reg = reg | 0x1;
		wcfxo_setreg(wc, WC_DAA_DCTL1, reg);
		wc->offhook = 1;
#ifdef	JAPAN
		wc->battery = 1;
		wc->battdebounce = BATT_DEBOUNCE;
		wc->ohdebounce = OH_DEBOUNCE;
#endif
		break;
	case DAHDI_TXSIG_ONHOOK:
		/* Put on hook and enable on hook line monitor */
		reg =  wc->readregs[WC_DAA_DCTL1] & 0xfe;
		wcfxo_setreg(wc, WC_DAA_DCTL1, reg);

		reg = reg | 0x08;
		wcfxo_setreg(wc, WC_DAA_DCTL1, reg);
		wc->offhook = 0;
		/* Don't accept a ring for another 1000 ms */
		wc->ringdebounce = 1000;
#ifdef	JAPAN
		wc->ohdebounce = OH_DEBOUNCE;
#endif
		break;
	default:
		printk(KERN_NOTICE "wcfxo: Can't set tx state to %d\n", txsig);
	}
	if (debug)
		printk(KERN_DEBUG "Setting hook state to %d (%02x)\n", txsig, reg);
	return 0;
}

static int wcfxo_initialize(struct wcfxo *wc)
{
	/* DAHDI stuff */
	wc->span.owner = THIS_MODULE;
	sprintf(wc->span.name, "WCFXO/%d", wc->pos);
	snprintf(wc->span.desc, sizeof(wc->span.desc) - 1, "%s Board %d", wc->variety, wc->pos + 1);
	sprintf(wc->chan->name, "WCFXO/%d/%d", wc->pos, 0);
	snprintf(wc->span.location, sizeof(wc->span.location) - 1,
		 "PCI Bus %02d Slot %02d", dahdi_pci_get_bus(wc->dev), dahdi_pci_get_slot(wc->dev) + 1);
	wc->span.manufacturer = "Digium";
	dahdi_copy_string(wc->span.devicetype, wc->variety, sizeof(wc->span.devicetype));
	wc->chan->sigcap = DAHDI_SIG_FXSKS | DAHDI_SIG_FXSLS | DAHDI_SIG_SF;
	wc->chan->chanpos = 1;
	wc->span.chans = &wc->chan;
	wc->span.channels = 1;
	wc->span.hooksig = wcfxo_hooksig;
	wc->span.irq = dahdi_pci_get_irq(wc->dev);
	wc->span.open = wcfxo_open;
	wc->span.close = wcfxo_close;
	wc->span.flags = DAHDI_FLAG_RBS;
	wc->span.deflaw = DAHDI_LAW_MULAW;
	wc->span.watchdog = wcfxo_watchdog;
#ifdef ENABLE_TASKLETS
	tasklet_init(&wc->wcfxo_tlet, wcfxo_tasklet, (unsigned long)wc);
#endif
	init_waitqueue_head(&wc->span.maintq);

	wc->span.pvt = wc;
	wc->chan->pvt = wc;
	if (dahdi_register(&wc->span, 0)) {
		printk(KERN_NOTICE "Unable to register span with DAHDI\n");
		return -1;
	}
	return 0;
}

static int wcfxo_hardware_init(struct wcfxo *wc)
{
	/* Hardware stuff */
	/* Reset PCI Interface chip and registers */
	wcfxo_outb(wc, WC_CNTL, 0x0e);

	/* Set all to outputs except AUX 4, which is an input */
	wcfxo_outb(wc, WC_AUXC, 0xef);

	/* Reset the DAA (DAA uses AUX5 for reset) */
	wcfxo_outb(wc, WC_AUXD, 0x00);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1 + HZ / 800);

	/* Set hook state to on hook & un-reset the DAA */
	if (wc->flags & FLAG_RESET_ON_AUX5) {
		/* Set hook state to on hook for when we switch.
		   Make sure reset is high */
		wcfxo_outb(wc, WC_AUXD, 0x34);
	} else {
		/* Set hook state to on hook for when we switch */
		wcfxo_outb(wc, WC_AUXD, 0x24);
	}

	/* Back to normal, with automatic DMA wrap around */
	wcfxo_outb(wc, WC_CNTL, 0x01);

	/* Make sure serial port and DMA are out of reset */
	wcfxo_outb(wc, WC_CNTL, wcfxo_inb(wc, WC_CNTL) & 0xf9);

	/* Configure serial port for MSB->LSB operation */
	if (wc->flags & FLAG_DOUBLE_CLOCK)
		wcfxo_outb(wc, WC_SERCTL, 0xc1);
	else
		wcfxo_outb(wc, WC_SERCTL, 0xc0);

	if (wc->flags & FLAG_USE_XTAL) {
		/* Use the crystal oscillator */
		wcfxo_outb(wc, WC_AUXFUNC, 0x04);
	}

	/* Delay FSC by 2 so it's properly aligned */
	wcfxo_outb(wc, WC_FSCDELAY, 0x2);

	/* Setup DMA Addresses */
	wcfxo_outl(wc, WC_DMAWS, wc->writedma);		/* Write start */
	wcfxo_outl(wc, WC_DMAWI, wc->writedma + DAHDI_CHUNKSIZE * 8 - 4);	/* Middle (interrupt) */
	wcfxo_outl(wc, WC_DMAWE, wc->writedma + DAHDI_CHUNKSIZE * 16 - 4);	/* End */

	wcfxo_outl(wc, WC_DMARS, wc->readdma);		/* Read start */
	wcfxo_outl(wc, WC_DMARI, wc->readdma + DAHDI_CHUNKSIZE * 8 - 4);	/* Middle (interrupt) */
	wcfxo_outl(wc, WC_DMARE, wc->readdma + DAHDI_CHUNKSIZE * 16 - 4);	/* End */

	/* Clear interrupts */
	wcfxo_outb(wc, WC_INTSTAT, 0xff);
	return 0;
}

static void wcfxo_enable_interrupts(struct wcfxo *wc)
{
	/* Enable interrupts (we care about all of them) */
	wcfxo_outb(wc, WC_MASK0, 0x3f);
	/* No external interrupts */
	wcfxo_outb(wc, WC_MASK1, 0x00);
}

static void wcfxo_start_dma(struct wcfxo *wc)
{
	/* Reset Master and TDM */
	wcfxo_outb(wc, WC_CNTL, 0x0f);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1);
	wcfxo_outb(wc, WC_CNTL, 0x01);
	wcfxo_outb(wc, WC_OPER, 0x01);
}

static void wcfxo_restart_dma(struct wcfxo *wc)
{
	/* Reset Master and TDM */
	wcfxo_outb(wc, WC_CNTL, 0x01);
	wcfxo_outb(wc, WC_OPER, 0x01);
}


static void wcfxo_stop_dma(struct wcfxo *wc)
{
	wcfxo_outb(wc, WC_OPER, 0x00);
}

static void wcfxo_reset_tdm(struct wcfxo *wc)
{
	/* Reset TDM */
	wcfxo_outb(wc, WC_CNTL, 0x0f);
}

static void wcfxo_disable_interrupts(struct wcfxo *wc)	
{
	wcfxo_outb(wc, WC_MASK0, 0x00);
	wcfxo_outb(wc, WC_MASK1, 0x00);
}

static void wcfxo_set_daa_mode(struct wcfxo *wc)
{
	/* Set country specific parameters (OHS, ACT, DCT, RZ, RT, LIM, VOL) */
	int reg16 = ((fxo_modes[opermode].ohs & 0x1) << 6) |
				((fxo_modes[opermode].act & 0x1) << 5) |
				((fxo_modes[opermode].dct & 0x3) << 2) |
				((fxo_modes[opermode].rz & 0x1) << 1) |
				((fxo_modes[opermode].rt & 0x1) << 0);
	int reg17 = ((fxo_modes[opermode].lim & 0x3) << 3);
	int reg18 = ((fxo_modes[opermode].vol & 0x3) << 3);

	if (wc->flags & FLAG_NO_I18N_REGS) {
		wcfxo_dbg(wc->span, "This card does not support international settings.\n");
		return;
	}

	wcfxo_setreg(wc, WC_DAA_INT_CTL1, reg16);
	wcfxo_setreg(wc, WC_DAA_INT_CTL2, reg17);
	wcfxo_setreg(wc, WC_DAA_INT_CTL3, reg18);


	/* Wait a couple of jiffies for our writes to finish */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1 + (DAHDI_CHUNKSIZE * HZ) / 800);

	printk(KERN_INFO "wcfxo: DAA mode is '%s'\n", fxo_modes[opermode].name);
}

static int wcfxo_init_daa(struct wcfxo *wc)
{
	/* This must not be called in an interrupt */
	/* We let things settle for a bit */
	unsigned char reg15;
	int chip_revb;
//	set_current_state(TASK_INTERRUPTIBLE);
//	schedule_timeout(10);

	/* Soft-reset it */
	wcfxo_setreg(wc, WC_DAA_CTL1, 0x80);

	/* Let the reset go */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(1 + (DAHDI_CHUNKSIZE * HZ) / 800);

	/* We have a clock at 18.432 Mhz, so N1=1, M1=2, CGM=0 */
	wcfxo_setreg(wc, WC_DAA_PLL1_N1, 0x0);	/* This value is N1 - 1 */
	wcfxo_setreg(wc, WC_DAA_PLL1_M1, 0x1);	/* This value is M1 - 1 */
	/* We want to sample at 8khz, so N2 = 9, M2 = 10 (N2-1, M2-1) */
	wcfxo_setreg(wc, WC_DAA_PLL2_N2_M2, 0x89);
	
	/* Wait until the PLL's are locked. Time is between 100 uSec and 1 mSec */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1 + HZ/1000 + (DAHDI_CHUNKSIZE * HZ) / 800);

	/* No additional ration is applied to the PLL and faster lock times
	 * are possible */
	wcfxo_setreg(wc, WC_DAA_PLL_CTL, 0x0);
	/* Enable off hook pin */
	wcfxo_setreg(wc, WC_DAA_DCTL1, 0x0a);
	if (monitor) {
		/* Enable ISOcap and external speaker and charge pump if present */
		wcfxo_setreg(wc, WC_DAA_DCTL2, 0x80);
	} else {
		/* Enable ISOcap and charge pump if present (leave speaker disabled) */
		wcfxo_setreg(wc, WC_DAA_DCTL2, 0xe0);
	}

	/* Wait a couple of jiffies for our writes to finish */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1 + (DAHDI_CHUNKSIZE * HZ) / 800);
	reg15 = 0x0;
	/* Go ahead and attenuate transmit signal by 6 db */
	if (quiet) {
		printk(KERN_INFO "wcfxo: Attenuating transmit signal for quiet operation\n");
		reg15 |= (quiet & 0x3) << 4;
	}
	if (boost) {
		printk(KERN_INFO "wcfxo: Boosting receive signal\n");
		reg15 |= (boost & 0x3);
	}
	wcfxo_setreg(wc, WC_DAA_TXRX_GCTL, reg15);

	/* REVB: reg. 13, bits 5:2 */ 
	chip_revb = (wc->readregs[WC_DAA_CHIPB_REV] >> 2) & 0xF; 
	wcfxo_dbg(wc->span, "DAA chip REVB is %x\n", chip_revb);
	switch(chip_revb) {
		case 1: case 2: case 3:
			/* This is a si3034. Nothing to do */
			break;
		case 4: case 5: case 7:
			/* This is 3035. Has no support for international registers */
			wc->flags |= FLAG_NO_I18N_REGS;
			break;
		default:
			wcfxo_notice(wc->span, "Unknown DAA chip revision: REVB=%d\n",
					chip_revb);
	}

	/* Didn't get it right.  Register 9 is still garbage */
	if (wc->readregs[WC_DAA_PLL2_N2_M2] != 0x89)
		return -1;
#if 0
	{ int x;
	int y;
	for (y=0;y<100;y++) {
		printk(KERN_DEBUG " reg dump ====== %d ======\n", y);
		for (x=0;x<sizeof(wecareregs) / sizeof(wecareregs[0]);x++) {
			printk(KERN_DEBUG "daa: Reg %d: %02x\n", wecareregs[x], wc->readregs[wecareregs[x]]);
		}
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(100);
	} }
#endif	
	return 0;
}

#if !defined(__FreeBSD__)
static int __devinit wcfxo_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct wcfxo *wc;
	struct wcfxo_desc *d = (struct wcfxo_desc *)ent->driver_data;
	int x;

	for (x=0;x<WC_MAX_IFACES;x++)
		if (!ifaces[x]) break;
	if (x >= WC_MAX_IFACES) {
		printk(KERN_ERR "Too many interfaces: Found %d, can only handle %d.\n",
				x, WC_MAX_IFACES - 1);
		return -EIO;
	}
	
	if (pci_enable_device(pdev))
		return -EIO;

	wc = kmalloc(sizeof(struct wcfxo), GFP_KERNEL);
	if (!wc) {
		printk(KERN_ERR "wcfxo: Failed initializinf card. Not enough memory.");
		return -ENOMEM;
	}

	ifaces[x] = wc;
	memset(wc, 0, sizeof(struct wcfxo));
	wc->chan = &wc->_chan;
	wc->ioaddr = pci_resource_start(pdev, 0);
	wc->dev = pdev;
	wc->pos = x;
	wc->variety = d->name;
	wc->flags = d->flags;
	/* Keep track of whether we need to free the region */
	if (request_region(wc->ioaddr, 0xff, "wcfxo")) 
		wc->freeregion = 1;

	/* Allocate enough memory for two zt chunks, receive and transmit.  Each sample uses
	   32 bits.  Allocate an extra set just for control too */
	wc->writechunk = (int *)pci_alloc_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, &wc->writedma);
	if (!wc->writechunk) {
		printk(KERN_NOTICE "wcfxo: Unable to allocate DMA-able memory\n");
		if (wc->freeregion)
			release_region(wc->ioaddr, 0xff);
		return -ENOMEM;
	}

	wc->readchunk = wc->writechunk + DAHDI_MAX_CHUNKSIZE * 4;	/* in doublewords */
	wc->readdma = wc->writedma + DAHDI_MAX_CHUNKSIZE * 16;		/* in bytes */

	if (wcfxo_initialize(wc)) {
		printk(KERN_NOTICE "wcfxo: Unable to intialize modem\n");
		if (wc->freeregion)
			release_region(wc->ioaddr, 0xff);
		kfree(wc);
		return -EIO;
	}

	/* Enable bus mastering */
	pci_set_master(pdev);

	/* Keep track of which device we are */
	pci_set_drvdata(pdev, wc);

	if (request_irq(pdev->irq, wcfxo_interrupt, DAHDI_IRQ_SHARED, "wcfxo", wc)) {
		printk(KERN_NOTICE "wcfxo: Unable to request IRQ %d\n", pdev->irq);
		if (wc->freeregion)
			release_region(wc->ioaddr, 0xff);
		kfree(wc);
		return -EIO;
	}


	wcfxo_hardware_init(wc);
	/* Enable interrupts */
	wcfxo_enable_interrupts(wc);
	/* Initialize Write/Buffers to all blank data */
	memset((void *)wc->writechunk,0,DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4);
	/* Start DMA */
	wcfxo_start_dma(wc);

	/* Initialize DAA (after it's started) */
	if (wcfxo_init_daa(wc)) {
		printk(KERN_NOTICE "Failed to initailize DAA, giving up...\n");
		wcfxo_stop_dma(wc);
		wcfxo_disable_interrupts(wc);
		dahdi_unregister(&wc->span);
		free_irq(pdev->irq, wc);

		/* Reset PCI chip and registers */
		wcfxo_outb(wc, WC_CNTL, 0x0e);

		if (wc->freeregion)
			release_region(wc->ioaddr, 0xff);
		kfree(wc);
		return -EIO;
	}
	wcfxo_set_daa_mode(wc);
	printk(KERN_INFO "Found a Wildcard FXO: %s\n", wc->variety);

	return 0;
}

static void wcfxo_release(struct wcfxo *wc)
{
	dahdi_unregister(&wc->span);
	if (wc->freeregion)
		release_region(wc->ioaddr, 0xff);
	kfree(wc);
	printk(KERN_INFO "Freed a Wildcard\n");
}

static void __devexit wcfxo_remove_one(struct pci_dev *pdev)
{
	struct wcfxo *wc = pci_get_drvdata(pdev);
	if (wc) {

		/* Stop any DMA */
		wcfxo_stop_dma(wc);
		wcfxo_reset_tdm(wc);

		/* In case hardware is still there */
		wcfxo_disable_interrupts(wc);
		
		/* Immediately free resources */
		pci_free_consistent(pdev, DAHDI_MAX_CHUNKSIZE * 2 * 2 * 2 * 4, (void *)wc->writechunk, wc->writedma);
		free_irq(pdev->irq, wc);

		/* Reset PCI chip and registers */
		wcfxo_outb(wc, WC_CNTL, 0x0e);

		/* Release span, possibly delayed */
		if (!wc->usecount)
			wcfxo_release(wc);
		else
			wc->dead = 1;
	}
}
#endif /* !__FreeBSD__ */

static int
wcfxo_validate_params(void)
{
	int x;

	if ((opermode >= sizeof(fxo_modes) / sizeof(fxo_modes[0])) || (opermode < 0)) {
		printk(KERN_NOTICE "Invalid/unknown operating mode specified.  Please choose one of:\n");
		for (x=0;x<sizeof(fxo_modes) / sizeof(fxo_modes[0]); x++)
			printk(KERN_INFO "%d: %s\n", x, fxo_modes[x].name);
		return -ENODEV;
	}

	return 0;
}

static struct pci_device_id wcfxo_pci_tbl[] = {
	{ 0xe159, 0x0001, 0x8084, PCI_ANY_ID, 0, 0, (unsigned long) &generic },
	{ 0xe159, 0x0001, 0x8085, PCI_ANY_ID, 0, 0, (unsigned long) &wcx101p },
	{ 0xe159, 0x0001, 0x8086, PCI_ANY_ID, 0, 0, (unsigned long) &generic },
	{ 0xe159, 0x0001, 0x8087, PCI_ANY_ID, 0, 0, (unsigned long) &generic },
	{ 0x1057, 0x5608, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long) &wcx100p },
	{ 0 }
};

#if defined(__FreeBSD__)
SYSCTL_NODE(_dahdi, OID_AUTO, wcfxo, CTLFLAG_RW, 0, "DAHDI wcfxo");
#define MODULE_PARAM_PREFIX "dahdi.wcfxo"
#define MODULE_PARAM_PARENT _dahdi_wcfxo

static void
wcfxo_release_resources(struct wcfxo *wc)
{
        /* disconnect the interrupt handler */
	if (wc->irq_handle != NULL) {
		bus_teardown_intr(wc->dev->dev, wc->irq_res, wc->irq_handle);
		wc->irq_handle = NULL;
	}

	if (wc->irq_res != NULL) {
		bus_release_resource(wc->dev->dev, SYS_RES_IRQ, wc->irq_rid, wc->irq_res);
		wc->irq_res = NULL;
	}

	/* release DMA resources */
	dahdi_dma_free(&wc->write_dma_tag, &wc->write_dma_map, __DECONST(void **, &wc->writechunk), &wc->writedma);
	dahdi_dma_free(&wc->read_dma_tag, &wc->read_dma_map, __DECONST(void **, &wc->readchunk), &wc->readdma);

	/* release memory window */
	if (wc->io_res != NULL) {
		bus_release_resource(wc->dev->dev, SYS_RES_IOPORT, wc->io_rid, wc->io_res);
		wc->io_res = NULL;
	}
}

static int
wcfxo_setup_intr(struct wcfxo *wc)
{
	int error;

	wc->irq_res = bus_alloc_resource_any(
	     wc->dev->dev, SYS_RES_IRQ, &wc->irq_rid, RF_SHAREABLE | RF_ACTIVE);
	if (wc->irq_res == NULL) {
		device_printf(wc->dev->dev, "Can't allocate irq resource\n");
		return (ENXIO);
	}

	error = bus_setup_intr(
	    wc->dev->dev, wc->irq_res, INTR_TYPE_CLK | INTR_MPSAFE,
	    wcfxo_interrupt, NULL, wc, &wc->irq_handle);
	if (error) {
		device_printf(wc->dev->dev, "Can't setup interrupt handler (error %d)\n", error);
		return (ENXIO);
	}

	return (0);
}

static int
wcfxo_device_probe(device_t dev)
{
	struct pci_device_id *id;
	struct wcfxo_desc *d;

	id = dahdi_pci_device_id_lookup(dev, wcfxo_pci_tbl);
	if (id == NULL)
		return (ENXIO);

	if (wcfxo_validate_params())
		return (ENXIO);

	/* found device */
	device_printf(dev, "vendor=%x device=%x subvendor=%x\n",
	    id->vendor, id->device, id->subvendor);
	d = (struct wcfxo_desc *) id->driver_data;
	device_set_desc(dev, d->name);
	return (0);
}

static int
wcfxo_device_attach(device_t dev)
{
	int res;
	struct pci_device_id *id;
	struct wcfxo_desc *d;
	struct wcfxo *wc;

	id = dahdi_pci_device_id_lookup(dev, wcfxo_pci_tbl);
	if (id == NULL)
		return (ENXIO);

	d = (struct wcfxo_desc *) id->driver_data;
	wc = device_get_softc(dev);
	wc->dev = &wc->_dev;
	wc->dev->dev = dev;
	wc->chan = &wc->_chan;
	wc->pos = device_get_unit(dev);
	wc->variety = d->name;
	wc->flags = d->flags;

        /* allocate IO resource */
	wc->io_rid = PCIR_BAR(0);
	wc->io_res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &wc->io_rid, RF_ACTIVE);
	if (wc->io_res == NULL) {
		device_printf(dev, "Can't allocate IO resource\n");
		return (ENXIO);
	}

	/* enable bus mastering */
	pci_enable_busmaster(dev);

	res = wcfxo_setup_intr(wc);
	if (res)
		goto err;

	/* allocate enough memory for two zt chunks.  Each sample uses
	   32 bits.  Allocate an extra set just for control too */
	res = dahdi_dma_allocate(DAHDI_MAX_CHUNKSIZE * 2 * 2 * 4, &wc->write_dma_tag, &wc->write_dma_map,
	    __DECONST(void **, &wc->writechunk), &wc->writedma);
	if (res)
		goto err;

	res = dahdi_dma_allocate(DAHDI_MAX_CHUNKSIZE * 2 * 2 * 4, &wc->read_dma_tag, &wc->read_dma_map,
	    __DECONST(void **, &wc->readchunk), &wc->readdma);
	if (res)
		goto err;

	if (wcfxo_initialize(wc) < 0) {
		res = ENXIO;
		goto err;
	}

	wcfxo_hardware_init(wc);

	/* enable interrupts */
	wcfxo_enable_interrupts(wc);

	/* start DMA */
	wcfxo_start_dma(wc);

	/* initialize DAA (after it's started) */
	if (wcfxo_init_daa(wc)) {
		printk(KERN_NOTICE "Failed to initailize DAA, giving up...\n");
		res = ENXIO;
		goto err;
	}
	wcfxo_set_daa_mode(wc);
	printk(KERN_INFO "Found a Wildcard FXO: %s\n", wc->variety);
	return (0);

err:
	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &wc->span.flags))
		dahdi_unregister(&wc->span);

	wcfxo_stop_dma(wc);
	wcfxo_disable_interrupts(wc);

	/* reset PCI chip and registers */
	wcfxo_outb(wc, WC_CNTL, 0x0e);

	/* release resources */
	wcfxo_release_resources(wc);
	return (res);
}

static int
wcfxo_device_detach(device_t dev)
{
	struct wcfxo *wc = device_get_softc(dev);

	/* Stop any DMA */
	wcfxo_stop_dma(wc);
	wcfxo_reset_tdm(wc);

	/* In case hardware is still there */
	wcfxo_disable_interrupts(wc);

	/* Reset PCI chip and registers */
	wcfxo_outb(wc, WC_CNTL, 0x0e);

	/* Unregister */
	dahdi_unregister(&wc->span);

	/* Release resources */
	wcfxo_release_resources(wc);

	return (0);
}

static device_method_t wcfxo_methods[] = {
	DEVMETHOD(device_probe,     wcfxo_device_probe),
	DEVMETHOD(device_attach,    wcfxo_device_attach),
	DEVMETHOD(device_detach,    wcfxo_device_detach),
	{ 0, 0 }
};

static driver_t wcfxo_pci_driver = {
	"wcfxo",
	wcfxo_methods,
	sizeof(struct wcfxo)
};

static devclass_t wcfxo_devclass;

DRIVER_MODULE(wcfxo, pci, wcfxo_pci_driver, wcfxo_devclass, 0, 0);
MODULE_DEPEND(wcfxo, pci, 1, 1, 1);
MODULE_DEPEND(wcfxo, dahdi, 1, 1, 1);
#else /* !__FreeBSD__ */
MODULE_DEVICE_TABLE (pci, wcfxo_pci_tbl);

static struct pci_driver wcfxo_driver = {
	.name = "wcfxo",
	.probe = wcfxo_init_one,
	.remove = __devexit_p(wcfxo_remove_one),
	.id_table = wcfxo_pci_tbl,
};

static int __init wcfxo_init(void)
{
	int res;

	res = wcfxo_validate_params();
	if (res)
		return -ENODEV;

	res = dahdi_pci_module(&wcfxo_driver);
	if (res)
		return -ENODEV;
	return 0;
}

static void __exit wcfxo_cleanup(void)
{
	pci_unregister_driver(&wcfxo_driver);
}
#endif /* !__FreeBSD__ */

module_param(debug, int, 0644);
module_param(quiet, int, 0444);
module_param(boost, int, 0444);
module_param(monitor, int, 0444);
module_param(opermode, int, 0444);

#if !defined(__FreeBSD__)
MODULE_DESCRIPTION("Wildcard X100P Driver");
MODULE_AUTHOR("Mark Spencer <markster@digium.com>");
MODULE_LICENSE("GPL v2");

module_init(wcfxo_init);
module_exit(wcfxo_cleanup);
#endif /* !__FreeBSD__ */

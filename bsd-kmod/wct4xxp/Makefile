# $Id$

KMOD=	wct4xxp
SRCS=	base.c vpm450m.c ${OCTASIC_SRCS}
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS=	-DHOTPLUG_FIRMWARE ${OCTASIC_CFLAGS}

# ${srcdir}/oct612x/octasic-helper objects ${srcdir}
OCTASIC_PATH+=	${srcdir}/oct612x/octdeviceapi/oct6100api/oct6100_api
OCTASIC_SRCS+=\
	oct6100_adpcm_chan.c\
	oct6100_channel.c\
	oct6100_chip_open.c\
	oct6100_chip_stats.c\
	oct6100_conf_bridge.c\
	oct6100_debug.c\
	oct6100_events.c\
	oct6100_interrupts.c\
	oct6100_memory.c\
	oct6100_miscellaneous.c\
	oct6100_mixer.c\
	oct6100_phasing_tsst.c\
	oct6100_playout_buf.c\
	oct6100_remote_debug.c\
	oct6100_tlv.c\
	oct6100_tone_detection.c\
	oct6100_tsi_cnct.c\
	oct6100_tsst.c
OCTASIC_PATH+=	${srcdir}/oct612x/apilib/bt
OCTASIC_SRCS+=	octapi_bt0.c
OCTASIC_PATH+=	${srcdir}/oct612x/apilib/largmath
OCTASIC_SRCS+=	octapi_largmath.c
OCTASIC_PATH+=	${srcdir}/oct612x/apilib/llman
OCTASIC_SRCS+=	octapi_llman.c

# ${srcdir}/oct612x/octasic-helper cflags ${srcdir}
OCTASIC_CFLAGS=\
	-I${srcdir}/oct612x/include\
	-I${srcdir}/oct612x/octdeviceapi\
	-I${srcdir}/oct612x/octdeviceapi/oct6100api

srcdir=	${.CURDIR}/../../drivers/dahdi
.PATH: ${srcdir}/wct4xxp ${OCTASIC_PATH}

.include <bsd.kmod.mk>

CFLAGS+=	-Wno-cast-qual
CFLAGS+=	-Wno-undef	# octasic

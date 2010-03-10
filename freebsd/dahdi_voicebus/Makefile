# $Id$

.PATH: ${.CURDIR}/../../drivers/dahdi/voicebus

KMOD=	dahdi_voicebus
SRCS=	voicebus.c voicebus_net.c GpakCust.c GpakApi.c
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS=	-I${.CURDIR}/../../drivers/dahdi

.include <bsd.kmod.mk>

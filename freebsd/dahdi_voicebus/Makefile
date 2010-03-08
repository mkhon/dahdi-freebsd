# $Id$

.PATH: ${.CURDIR}/../../drivers/dahdi/voicebus

KMOD=	dahdi_voicebus
SRCS=	voicebus.c voicebus_net.c GpakCust.c GpakApi.c
CFLAGS=	-I${.CURDIR}/../../drivers/dahdi
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>

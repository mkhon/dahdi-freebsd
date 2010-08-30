# $Id$

.PATH: ${.CURDIR}/../../drivers/dahdi/wctc4xxp

KMOD=	wctc4xxp
SRCS=	base.c
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS=	-DHOTPLUG_FIRMWARE -DWITHOUT_NETDEV -DDEFERRED_PROCESSING=ITHREAD

.include <bsd.kmod.mk>

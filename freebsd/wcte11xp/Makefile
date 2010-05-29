# $Id$

.PATH: ${.CURDIR}/../../drivers/dahdi

KMOD=	wcte11xp
SRCS=	wcte11xp.c
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS=	--param inline-unit-growth=100

.include <bsd.kmod.mk>

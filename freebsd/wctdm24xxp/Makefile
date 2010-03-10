# $Id$

.PATH: ${.CURDIR}/../../drivers/dahdi/wctdm24xxp

KMOD=	wctdm24xxp
SRCS=	base.c xhfc.c
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS=	-I${.CURDIR}/../../drivers/dahdi

.include <bsd.kmod.mk>

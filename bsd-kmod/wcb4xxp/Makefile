# $Id$

.PATH: ${.CURDIR}/../../drivers/dahdi/wcb4xxp

KMOD=	wcb4xxp
SRCS=	base.c
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS=	--param inline-unit-growth=100

.include <bsd.kmod.mk>

# $Id$

.PATH: ${.CURDIR}/../../drivers/dahdi/wcte12xp

KMOD=	wcte12xp
SRCS=	base.c
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS=	-I${.CURDIR}/../../drivers/dahdi

.include <bsd.kmod.mk>

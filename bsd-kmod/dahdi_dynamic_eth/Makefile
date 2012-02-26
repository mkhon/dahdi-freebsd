# $Id$

.PATH:	${.CURDIR}/../../drivers/dahdi

KMOD=	dahdi_dynamic_eth
SRCS=	dahdi_dynamic_eth.c
SRCS+=	device_if.h bus_if.h
CFLAGS=	-I${.CURDIR}/../ng_dahdi_netdev

.include <bsd.kmod.mk>

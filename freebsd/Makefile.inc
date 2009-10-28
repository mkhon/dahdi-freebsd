# $Id$

PREFIX?=	/usr/local
INCLUDEDIR=	${.CURDIR}/../../include
KMODDIR?=	${PREFIX}/lib/dahdi

CFLAGS+=	-I${INCLUDEDIR} -D__KERNEL__

FETCH?=		fetch
MKDIR?=		mkdir -p
INSTALL_DATA?=	${INSTALL} -m ${SHAREMODE} -o ${SHAREOWN} -g ${SHAREGRP} -c
TAR?=		tar

beforeinstall:
	${MKDIR} ${KMODDIR}

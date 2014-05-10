# $Id$

PREFIX?=	/usr/local
INCLUDEDIR=	${.CURDIR}/../../include
KMODDIR?=	${PREFIX}/lib/dahdi

CFLAGS+=	-I${INCLUDEDIR} -D__KERNEL__ -DM_LINUX=M_DAHDI
#CFLAGS+=	-DINVARIANT_SUPPORT -DINVARIANTS -DDIAGNOSTIC

CP?=		cp
.if defined(NO_FETCH)
FETCH?=		@echo Skipping fetch
.else
FETCH?=		fetch
.endif
MKDIR?=		mkdir -p
INSTALL_DATA?=	${INSTALL} -m ${SHAREMODE} -o ${SHAREOWN} -g ${SHAREGRP} -c
TAR?=		tar
TOUCH?=		touch

beforeinstall:
	${MKDIR} ${DESTDIR}${KMODDIR}

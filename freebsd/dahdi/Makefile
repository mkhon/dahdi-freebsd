# $Id$

.PATH: ${.CURDIR}/../../drivers/dahdi

KMOD=		dahdi
SRCS=		dahdi-base.c version.h
SRCS+=		device_if.h bus_if.h
CLEANFILES=	version.h
INCS=		user.h wctdm_user.h compat/types.h

DAHDIVERSION!=	${.CURDIR}/../../build_tools/make_version ${.CURDIR}/../.. dahdi/linux
version.h::
	@DAHDIVERSION="${DAHDIVERSION}" ${.CURDIR}/../../build_tools/make_version_h >${.TARGET}.tmp
	@if ! cmp -s ${.TARGET}.tmp ${.TARGET}; then mv ${.TARGET}.tmp ${.TARGET}; fi
	@rm -f ${.TARGET}.tmp

afterinstall:
	${MKDIR} ${PREFIX}/include/dahdi/compat
.for _h in ${INCS}
	${INSTALL_DATA} ${INCLUDEDIR}/dahdi/${_h} ${PREFIX}/include/dahdi/${_h:H}
.endfor

.include <bsd.kmod.mk>
.include <bsd.own.mk>

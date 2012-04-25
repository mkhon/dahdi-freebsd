# $Id$

.PATH: ${.CURDIR}/../../drivers/dahdi

KMOD=		dahdi
SRCS=		dahdi-base.c dahdi-version.c dahdi-bsd.c ng_dahdi_iface.c
SRCS+=		bsd-compat.c
SRCS+=		version.h
SRCS+=		device_if.h bus_if.h pci_if.h
CLEANFILES=	version.h dahdi
CFLAGS+=	-I${.CURDIR}
DAHDI_INCS=	dahdi_config.h user.h wctdm_user.h

DAHDIVERSION!=	${.CURDIR}/../../build_tools/make_version ${.CURDIR}/../.. dahdi/freebsd
version.h::
	@DAHDIVERSION="${DAHDIVERSION}" ${.CURDIR}/../../build_tools/make_version_h >${.TARGET}.tmp
	@if ! cmp -s ${.TARGET}.tmp ${.TARGET}; then echo "Updating ${.TARGET}"; mv ${.TARGET}.tmp ${.TARGET}; fi
	@rm -f ${.TARGET}.tmp
	@[ -e dahdi ] || ln -sf . dahdi

.if !defined(WITHOUT_HEADERS)
afterinstall: install-headers
.endif

install-headers:
	${MKDIR} ${PREFIX}/include/dahdi
.for _h in ${DAHDI_INCS}
	${INSTALL_DATA} ${INCLUDEDIR}/dahdi/${_h} ${PREFIX}/include/dahdi/${_h:H}
.endfor

.include <bsd.kmod.mk>
.include <bsd.own.mk>

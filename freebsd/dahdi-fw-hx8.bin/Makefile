# $Id$

KMOD=		dahdi-fw-hx8.bin
FIRMWS=		dahdi-fw-hx8.bin:dahdi-fw-hx8.bin
TARBALL=	dahdi-fw-hx8-${HX8_VERSION}.tar.gz
CLEANFILES=	dahdi-fw-hx8.bin ${TARBALL}

dahdi-fw-hx8.bin:
	${FETCH} -m ${FIRMWARE_URL}/${TARBALL}
	${TAR} xvfz ${TARBALL}

.include "../../firmware.mk"
.include <bsd.kmod.mk>

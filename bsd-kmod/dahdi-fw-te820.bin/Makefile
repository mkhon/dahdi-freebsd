# $Id$

KMOD=		dahdi-fw-te820.bin
FIRMWS=		dahdi-fw-te820.bin:dahdi-fw-te820.bin
TARBALL=	dahdi-fw-te820-${WCT820_VERSION}.tar.gz
CLEANFILES=	dahdi-fw-te820.bin ${TARBALL}

dahdi-fw-te820.bin:
	${FETCH} -m ${FIRMWARE_URL}/${TARBALL}
	${TAR} xvfz ${TARBALL}

.include "../../firmware.mk"
.include <bsd.kmod.mk>

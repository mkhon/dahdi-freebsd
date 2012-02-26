# $Id$

KMOD=		dahdi-fw-oct6114-064.bin
FIRMWS=		dahdi-fw-oct6114-064.bin:dahdi-fw-oct6114-064.bin
TARBALL=	dahdi-fw-oct6114-064-${OCT6114_064_VERSION}.tar.gz
CLEANFILES=	dahdi-fw-oct6114-064.bin ${TARBALL}

dahdi-fw-oct6114-064.bin:
	${FETCH} -m ${FIRMWARE_URL}/${TARBALL}
	${TAR} xvfz ${TARBALL}

.include "../../firmware.mk"
.include <bsd.kmod.mk>

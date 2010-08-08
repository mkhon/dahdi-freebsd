# $Id$

KMOD=		dahdi-fw-oct6114-128.bin
FIRMWS=		dahdi-fw-oct6114-128.bin:dahdi-fw-oct6114-128.bin
TARBALL=	dahdi-fw-oct6114-128-${OCT6114_128_VERSION}.tar.gz
CLEANFILES=	dahdi-fw-oct6114-128.bin ${TARBALL}

dahdi-fw-oct6114-128.bin:
	${FETCH} -m ${FIRMWARE_URL}/${TARBALL}
	${TAR} xvfz ${TARBALL}

.include "../../firmware.mk"
.include <bsd.kmod.mk>

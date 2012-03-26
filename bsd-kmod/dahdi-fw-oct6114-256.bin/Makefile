# $Id$

KMOD=		dahdi-fw-oct6114-256.bin
FIRMWS=		dahdi-fw-oct6114-256.bin:dahdi-fw-oct6114-256.bin
TARBALL=	dahdi-fw-oct6114-256-${OCT6114_256_VERSION}.tar.gz
CLEANFILES=	dahdi-fw-oct6114-256.bin ${TARBALL}

dahdi-fw-oct6114-256.bin:
	${FETCH} -m ${FIRMWARE_URL}/${TARBALL}
	${TAR} xvfz ${TARBALL}

.include "../../firmware.mk"
.include <bsd.kmod.mk>

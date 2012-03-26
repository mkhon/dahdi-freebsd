# $Id$

KMOD=		dahdi-fw-vpmoct032.bin
FIRMWS=		dahdi-fw-vpmoct032.bin:dahdi-fw-vpmoct032.bin
TARBALL=	dahdi-fw-vpmoct032-${VPMOCT032_VERSION}.tar.gz
CLEANFILES=	dahdi-fw-vpmoct032.bin ${TARBALL}

dahdi-fw-vpmoct032.bin:
	${FETCH} -m ${FIRMWARE_URL}/${TARBALL}
	${TAR} xvfz ${TARBALL}

.include "../../firmware.mk"
.include <bsd.kmod.mk>

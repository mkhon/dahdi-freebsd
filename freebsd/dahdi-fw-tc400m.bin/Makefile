# $Id$

KMOD=		dahdi-fw-tc400m.bin
FIRMWS=		dahdi-fw-tc400m.bin:dahdi-fw-tc400m.bin
TARBALL=	dahdi-fw-tc400m-${TC400M_VERSION}.tar.gz
CLEANFILES=	dahdi-fw-tc400m.bin ${TARBALL}

dahdi-fw-tc400m.bin:
	${FETCH} -m ${FIRMWARE_URL}/${TARBALL}
	${TAR} xvfz ${TARBALL}

.include "../../firmware.mk"
.include <bsd.kmod.mk>

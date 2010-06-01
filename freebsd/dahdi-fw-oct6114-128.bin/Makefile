# $Id$

KMOD=	dahdi-fw-oct6114-128.bin
FIRMWS=	dahdi-fw-oct6114-128.bin:dahdi-fw-oct6114-128.bin

.include "../../firmware.mk"
.include <bsd.kmod.mk>

dahdi-fw-oct6114-128.bin: dahdi-fw-oct6114-128-${OCT6114_128_VERSION}.tar.gz
	${TAR} xvfz ${.ALLSRC}

dahdi-fw-oct6114-128-${OCT6114_128_VERSION}.tar.gz:
	${FETCH} ${FIRMWARE_URL}/${.TARGET}

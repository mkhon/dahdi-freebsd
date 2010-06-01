# $Id$

KMOD=	dahdi-fw-hx8.bin
FIRMWS=	dahdi-fw-hx8.bin:dahdi-fw-hx8.bin

.include "../../firmware.mk"
.include <bsd.kmod.mk>

dahdi-fw-hx8.bin: dahdi-fw-hx8-${HX8_VERSION}.tar.gz
	${TAR} xvfz ${.ALLSRC}

dahdi-fw-hx8-${HX8_VERSION}.tar.gz:
	${FETCH} ${FIRMWARE_URL}/${.TARGET}

# $Id$

KMOD=	dahdi_fw_oct6114_128
FIRMWS=	dahdi-fw-oct6114-128.bin:dahdi_fw_oct6114_128

.include "../../firmware.mk"
.include <bsd.kmod.mk>

dahdi-fw-oct6114-128.bin: dahdi-fw-oct6114-128-${OCT6114_128_VERSION}.tar.gz
	${TAR} xvfz ${.ALLSRC}

dahdi-fw-oct6114-128-${OCT6114_128_VERSION}.tar.gz:
	${FETCH} ${FIRMWARE_URL}/${.TARGET}

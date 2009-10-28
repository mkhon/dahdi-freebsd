# $Id$

KMOD=	dahdi_fw_oct6114_064
FIRMWS=	dahdi-fw-oct6114-064.bin:dahdi_fw_oct6114_064

.include "../../firmware.mk"
.include <bsd.kmod.mk>

dahdi-fw-oct6114-064.bin: dahdi-fw-oct6114-064-${OCT6114_064_VERSION}.tar.gz
	${TAR} xvfz ${.ALLSRC}

dahdi-fw-oct6114-064-${OCT6114_064_VERSION}.tar.gz:
	${FETCH} ${FIRMWARE_URL}/${.TARGET}

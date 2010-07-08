# $Id$

# DAHDI
SUBDIR=\
	dahdi\
	dahdi_dynamic\
	dahdi_transcode\
	dahdi_voicebus

# dynamic drivers
SUBDIR+=\
	dahdi_dynamic_loc\
	dahdi_dynamic_eth\
	dahdi_dynamic_ethmf\
	ng_dahdi_netdev

# SW echo cancellation drivers
SUBDIR+=\
	dahdi_echocan_jpah\
	dahdi_echocan_kb1\
	dahdi_echocan_mg2\
	dahdi_echocan_sec\
	dahdi_echocan_sec2

# HW drivers
SUBDIR+=\
	wcb4xxp\
	wcfxo\
	wct4xxp\
	wctdm\
	wctdm24xxp\
	wcte11xp\
	wcte12xp

# firmware drivers
SUBDIR+=\
	dahdi-fw-hx8.bin\
	dahdi-fw-oct6114-064.bin\
	dahdi-fw-oct6114-128.bin\
	${_dahdi_vpmadt032_loader}

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
_dahdi_vpmadt032_loader=	dahdi_vpmadt032_loader
.endif

.include <bsd.subdir.mk>

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
	dahdi_echocan_oslec\
	dahdi_echocan_sec\
	dahdi_echocan_sec2

# FXO/FXS analog and hybrid cards
SUBDIR+=\
	wcfxo\
	wctdm\
	wctdm24xxp\
	dahdi-fw-hx8.bin\
	${_dahdi_vpmadt032_loader}

# E1/T1 cards
SUBDIR+=\
	wct4xxp\
	wcte11xp\
	wcte12xp\
	dahdi-fw-oct6114-064.bin\
	dahdi-fw-oct6114-128.bin

# ISDN cards
SUBDIR+=\
	wcb4xxp

# Transcoding cards
SUBDIR+=\
	wctc4xxp\
	dahdi-fw-tc400m.bin

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
_dahdi_vpmadt032_loader=	dahdi_vpmadt032_loader
.endif

.include <bsd.subdir.mk>

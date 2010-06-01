# $Id$

SUBDIR=\
	dahdi\
	dahdi_dynamic\
	dahdi_dynamic_loc\
	dahdi_dynamic_eth\
	dahdi_dynamic_ethmf\
	dahdi_echocan_jpah\
	dahdi_echocan_kb1\
	dahdi_echocan_mg2\
	dahdi_echocan_sec\
	dahdi_echocan_sec2\
	dahdi-fw-hx8.bin\
	dahdi-fw-oct6114-064.bin\
	dahdi-fw-oct6114-128.bin\
	dahdi_voicebus\
	ng_dahdi_netdev\
	wcb4xxp\
	wcfxo\
	wct4xxp\
	wctdm24xxp

# the following drivers are ported but not tested on real HW
.if defined(WITH_EXPERIMENTAL)
SUBDIR+=\
	${_dahdi_vpmadt032_loader}\
	wctdm\
	wcte11xp\
	wcte12xp
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
_dahdi_vpmadt032_loader=	dahdi_vpmadt032_loader
.endif

.include <bsd.subdir.mk>

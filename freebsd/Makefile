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
	${_dahdi_vpmadt032_loader}\
	dahdi_voicebus\
	ng_dahdi_netdev\
	wcb4xxp\
	wcfxo\
	wct4xxp\
	wctdm\
	wctdm24xxp\
	wcte11xp\
	wcte12xp

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
_dahdi_vpmadt032_loader=	dahdi_vpmadt032_loader
.endif

.include <bsd.subdir.mk>

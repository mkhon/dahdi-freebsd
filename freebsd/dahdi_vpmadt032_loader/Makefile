# $Id$

.PATH: ${.CURDIR}/../../drivers/dahdi/vpmadt032_loader

KMOD=		dahdi_vpmadt032_loader
SRCS=		dahdi_vpmadt032_loader.c
SRCS+=		device_if.h bus_if.h pci_if.h
SRCS+=		vpmadt032_loader.h
OBJS=		vpmadt032_${VPMADT032_ARCH}.o
CFLAGS=		-I${.CURDIR}/../../drivers/dahdi
TARBALL=	dahdi-fwload-vpmadt032-${VPMADT032_VERSION}.tar.gz
CLEANFILES=	vpmadt032_loader.h vpmadt032_${VPMADT032_ARCH}.o ${TARBALL}
CLEANDIRS=	drivers

.if ${MACHINE_ARCH} == "i386"
VPMADT032_ARCH=	x86_32
.elif ${MACHINE_ARCH} == "amd64"
VPMADT032_ARCH=	x86_64
.else
.error "vpmadt032 is not supported on this architecture (${MACHINE_ARCH})"
.endif

vpmadt032_loader.h: ${.OBJDIR}/drivers/dahdi/vpmadt032_loader/vpmadt032_loader.h
	${CP} ${.ALLSRC} ${.TARGET}

vpmadt032_${VPMADT032_ARCH}.o: ${.OBJDIR}/drivers/dahdi/vpmadt032_loader/vpmadt032_${VPMADT032_ARCH}.o_shipped
	${CP} ${.ALLSRC:M*.o_shipped} ${.TARGET}

${.OBJDIR}/drivers/dahdi/vpmadt032_loader/vpmadt032_loader.h: ${.OBJDIR}/drivers/dahdi/vpmadt032_loader/vpmadt032_${VPMADT032_ARCH}.o_shipped

${.OBJDIR}/drivers/dahdi/vpmadt032_loader/vpmadt032_${VPMADT032_ARCH}.o_shipped:
	${FETCH} -m ${FIRMWARE_URL}/${TARBALL}
	${TAR} xvfz ${TARBALL}

.include "../../firmware.mk"
.include <bsd.kmod.mk>

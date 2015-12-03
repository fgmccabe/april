#
# Build the April utilities
#
APRIL_SRCS = aplist.ap apshell.ap apdebugger.ap apping.ap
APRIL_UTILS = apdebugger 

-include ${top_builddir}/April/april.Make

EXTRA_DIST = syslib.ap dxping.ap ${APRIL_SRCS}

CLEANFILES = ${APRIL_UTILS}

all-local: ${APRIL_UTILS}

install-exec-local: ${APRIL_UTILS}
	-${top_srcdir}/mkinstalldirs ${prefix}/april
	-(cd ${srcdir}; $(INSTALL_SCRIPT) ${APRIL_SRCS} ${prefix}/april)
	-$(INSTALL_SCRIPT) ${APRIL_UTILS} ${prefix}/bin




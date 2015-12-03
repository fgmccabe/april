APRIL_FILES = arith.ap date.ap closures.ap dns.ap ip.ap strptn.ap
APRIL_CODE = arith.aam date.aam closures.aam dns.aam ip.aam strptn.aam

-include ${top_builddir}/April/april.Make

CLEANFILES = ${APRIL_CODE}

all-local: ${APRIL_CODE}

install-data-local: ${APRIL_CODE}
	-$(INSTALL_SCRIPT) ${APRIL_CODE} ${prefix}/april
	-(cd ${srcdir}; $(INSTALL_SCRIPT) ${APRIL_FILES} ${prefix}/april)

EXTRA_DIST = ${APRIL_FILES} 


#
# Make the april library functions
#
# We have to make the april files by hand, cos automake dont know about april

-include ${top_builddir}/April/april.Make

APRIL_FILES = boot.ap files.ap stdio.ap sio.ap xml.ap scomms.af
APRIL_CODE = boot.aam files.aam stdio.aam sio.aam xml.aam scomms.af
APRIL_AF = files.af stdio.af sio.af xml.af

SUFFIXES = .af .ah .ap

include_HEADERS=sio.af xml.af files.af

xml.aam: sio.af

boot.aam: files.af

stest: sio.af

xtest: sio.af xml.af

stdio.aam: stdio.ap

EXTRA_DIST = files.ah ${APRIL_FILES} filio.ap stest.ap xtest.ap

CLEANFILES = ${APRIL_CODE} ${APRIL_AF}

all-local: ${APRIL_CODE} stest xtest files.af sio.af xml.af stdio.af

install-data-local: ${APRIL_CODE}
	-${top_srcdir}/mkinstalldirs ${prefix}/april
	(for XX in ${APRIL_CODE} ${APRIL_AF}; do\
	  $(INSTALL_SCRIPT) $${XX} ${prefix}/april; done)
	-(cd ${srcdir}; $(INSTALL_SCRIPT) ${APRIL_FILES} ${prefix}/april)


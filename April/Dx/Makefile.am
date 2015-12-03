#
# Make the april library to access Dx
#

-include ${top_builddir}/April/april.Make

APRIL_FILES = xml.ap grab.ap dx.ap
APRIL_CODE = ${APRIL_FILES:.ap=.aam}
APRIL_AF = xml.af dx.af

SUFFIXES = .af .ah .ap

include_HEADERS=dx.af

xml.aam xml.af:

dx.aam dx.af: xml.af

dxtest.aam: dx.af xml.af

EXTRA_DIST = ${APRIL_FILES} hello.xml

CLEANFILES = ${APRIL_CODE} ${APRIL_AF}

all-local: ${APRIL_CODE} 

install-data-local: ${APRIL_CODE}
	-${top_srcdir}/mkinstalldirs ${prefix}/april/dx ${prefix}/include/dx
	(for XX in ${APRIL_CODE} ; do\
	  $(INSTALL_SCRIPT) $${XX} ${prefix}/april/dx; done)
	(for XX in ${APRIL_AF}; do\
	  $(INSTALL_SCRIPT) $${XX} ${prefix}/include/dx; done)
	-(cd ${srcdir}; $(INSTALL_SCRIPT) ${APRIL_FILES} ${prefix}/april/dx)


## Process with Automake to generate Makefile.in and eventually Makefile
## This constructs the simple communications server

-include ${top_builddir}/April/april.Make

APRIL_SRCS = scomms.ap scs.ap sping.ap spinger.ap
APRIL_CODE = scomms.aam scs.aam sping.aam spinger.aam
APRIL_AF = scomms.af
APRIL_EXEC_FLAGS= -h2048

SUFFIXES = .af .ah

scs.aam:	scomms.af

scomms.af scomms.aam: scomms.ap

include_HEADERS = scomms.af

CLEANFILES = ${APRIL_CODE} scs scomms.af

EXTRA_DIST = ${APRIL_SRCS}

all-local: scs.aam sping.aam spinger.aam

install-exec-local: scs
	-${top_srcdir}/mkinstalldirs @bindir@
	-$(INSTALL_SCRIPT) scs @bindir@
	-${top_srcdir}/mkinstalldirs ${prefix}/april
	-(for XX in ${APRIL_CODE} ; do\
	  $(INSTALL_SCRIPT) $${XX} ${prefix}/april; done)
	-(for XX in ${APRIL_SRCS} ; do\
	  $(INSTALL_SCRIPT) $${XX} ${prefix}/april; done)


## Process with Automake to generate Makefile.in and eventually Makefile
## This constructs the Alex lexical parser generator

SUBDIRS = Doc

-include ${top_builddir}/April/april.Make

APRIL_SRCS = gendfa.ap genapril.ap genregexp.ap alex.ap
APRIL_CODE = genapril.aam genregexp.aam gendfa.aam
APRIL_EXEC_FLAGS= -h2048

SUFFIXES = .af .ah

alex.aam:       ${APRIL_CODE} regexp.ah genregexp.af gendfa.af genapril.af

## Special rules to express dependencies
genregexp.af genregexp.aam: regexp.ah

gendfa.aam gendfa.af: regexp.ah genregexp.af

genapril.aam genapril.af: regexp.ah 

include_HEADERS = regexp.ah 

CLEANFILES = ${APRIL_CODE} alex.aam

EXTRA_DIST = tester.ap token.ah	token.alx test.alx ident.alx ${APRIL_SRCS}

all-local: alex.aam

install-exec-local: alex.aam
	-${top_srcdir}/mkinstalldirs @bindir@
	-$(INSTALL_SCRIPT) alex @bindir@
	-${top_srcdir}/mkinstalldirs ${prefix}/april
	-(for XX in ${APRIL_CODE} alex.aam ; do\
	  $(INSTALL_SCRIPT) $${XX} ${prefix}/april; done)
	-(for XX in ${APRIL_SRCS} ; do\
	  $(INSTALL_SCRIPT) $${XX} ${prefix}/april; done)


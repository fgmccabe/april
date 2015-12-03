## Process with Automake to generate Makefile.in and eventually Makefile
## This constructs the Apg parser generator

SUBDIRS = Doc

APRIL_SRCS = gensets.ap closure.ap lr0.ap lalr.ap genparser.ap apglex.ap\
	rules.ap genrules.ap genapgrules.ap streamer.ap apg.ap
APRIL_CODE = gensets.aam closure.aam lr0.aam lalr.aam genparser.aam apglex.aam\
	rules.aam genrules.aam genapgrules.aam streamer.aam 
APRIL_IFACES = gensets.af closure.af lr0.af genrules.af genparser.af lalr.af\
	rules.af streamer.af


-include ${top_builddir}/April/april.Make

APRIL_EXEC_FLAGS= -h2048
APCFLAGS = -M
APRILENGINE = @aprilexec@
APRILDIR = @aprildir@

SUFFIXES = .af .alx .apg

apg.aam:	${APRIL_IFACES} ${APRIL_CODE}

apglex.ap:	apglex.alx
	@echo Running Alex on Apg lexical rules
	APRIL_DIR=file:///${APRILDIR} ${APRILENGINE} ${top_builddir}/April/Alex/alex apglex.alx

rules.ap: gensets.aam closure.aam lr0.aam lalr.aam genparser.aam genapgrules.aam apglex.aam
	@echo Constructing ruleset
	APRIL_DIR=file:///${APRILDIR} ${APRILENGINE} genapgrules.aam

include_HEADERS = apg.ah apg_tok.ah


apg.aam apg.af: gensets.af lr0.af lalr.af genparser.af rules.af genrules.af

closure.aam closure.af: gensets.af

genapgrules.aam genapgrules.af: gensets.af lr0.af lalr.af genparser.af

lalr.aam lalr.af: gensets.af closure.af lr0.af

lr0.aam lr0.af: gensets.af

ulmain: ul.af ulex.af ulmain.ap

ul.ap: ul.apg
	$(SHELL) APRIL_DIR=file:///${APRILDIR} ${APRILENGINE} ${top_builddir}/April/Apg/apg ul.apg

%.ap %.af: ${srcdir}/%.alx
	$(SHELL) APRIL_DIR=file:///${APRILDIR} ${APRILENGINE} ${top_builddir}/April/Alex/alex +l$(basename $@)lex $@
        
%.ap %.af: ${srcdir}/%.apg
	$(SHELL) APRIL_DIR=file:///$(top_builddir)/April ${top_builddir}/April/Engine/april ${top_builddir}/April/Apg/apg $@

CLEANFILES = $(APRIL_CODE) ${APRIL_IFACES} apg.aam

EXTRA_DIST = lextok.ah\
	token.ah\
	apg.ap\
	apglex.alx\
	apg.apg\
	apg.y\
	ul.apg\
	ulex.alx\
	ulmain.ap\
	test.ul\
	${APRIL_SRCS}

all-local: apg.aam

install-exec-local: apg.aam
	-${top_srcdir}/mkinstalldirs @bindir@
	-$(INSTALL_SCRIPT) apg @bindir@
	-${top_srcdir}/mkinstalldirs ${prefix}/april
	-(for XX in ${APRIL_CODE} apg.aam ; do\
	  $(INSTALL_SCRIPT) $${XX} ${prefix}/april; done)
	-(for XX in ${APRIL_SRCS} ; do\
	  $(INSTALL_SCRIPT) $${XX} ${prefix}/april; done)



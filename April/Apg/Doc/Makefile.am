info_TEXINFOS = apg.texi
dvi_TEXINFOS = apg.texi

THFLAGS = 

apg_TEXINFOS = apg.texi\
	gpl.texi\
	intro.texi\
	using.texi\
	rules.texi\
	theory.texi\
	ul.texi\
	grammar.texi

CLEANFILES = *.info* *.html

EXTRA_DIST = psfig.tex psfig.sty texinfo.tex *.info* *.html

all-local: apg.html

apg.html: ${apg_TEXINFOS}

SUFFIXES = .texi .html

.texi.html:
	$(TEXI2HTML) $<

install-data-local:
	-${top_srcdir}/mkinstalldirs @htmldir@
	-$(INSTALL_SCRIPT) apg*.html @htmldir@

uninstall-local:
	-rm @htmldir@/apg*.html

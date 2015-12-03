info_TEXINFOS = alex.texi
dvi_TEXINFOS = alex.texi

THFLAGS = 

alex_TEXINFOS = alex.texi\
	gpl.texi\
	intro.texi\
	using.texi\
	theory.texi\
	token.texi\
	grammar.texi

CLEANFILES = *.info* *.html

EXTRA_DIST = psfig.sty psfig.tex nfa.fig nfa.eps texinfo.tex\
	*.info* *.html

all-local: alex.html

SUFFIXES = .texi .html

.texi.html:
	$(TEXI2HTML) $<

alex.html: ${alex_TEXINFOS}

install-data-local:
	-${top_srcdir}/mkinstalldirs @htmldir@
	-$(INSTALL_SCRIPT) alex*.html @htmldir@

uninstall-local:
	-rm @htmldir@/alex*.html


info_TEXINFOS = april.texi
dvi_TEXINFOS = april.texi

THFLAGS = 

april_TEXINFOS = april.texi gpl.texi debugging.texi grammar.texi\
	process.texi echo.texi invoke.texi communicate.texi program.texi\
	arith.texi io.texi operators.texi string.texi macro.texi\
	syntax.texi values.texi expressions.texi patterns.texi statement.texi\
	functions.texi\
	compiling.texi environ.texi macros.texi predicate.texi lists.texi\
	resource.texi v2notes.texi v4notes.texi dll.texi errors.texi encoded.texi

EXTRA_DIST = fmgr.eps fmgr.fig psfig.sty psfig.tex texinfo.tex\
	april.info april.info* *.html

all-local: april.html

SUFFIXES = .texi .html

.texi.html:
	$(TEXI2HTML) $<

.texi.pdf:
	$(TEXI2PDF) $<

april.html: ${april_TEXINFOS}

april.pdf:  ${april_TEXINFOS}

install-data-local: april.html
	-${top_srcdir}/mkinstalldirs @htmldir@
	-(for XX in april*.html; do\
	  $(INSTALL_SCRIPT) $${XX} @htmldir@; done)

uninstall-local:
	-rm @htmldir@/april*.html


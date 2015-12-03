info_TEXINFOS = machine.texi
dvi_TEXINFOS = machine.texi

machine_TEXINFOS = machine.texi\
	introduction.texi\
	iformats.texi\
	compiling.texi\
	finale.texi\
	memory.texi\
	processes.texi\
	gc.texi\
        registers.texi\
	resources.texi\
	security.texi\
	instructions.texi\
	escapes.texi\
	types.texi

CLEANFILES = machine.info* machine*.html

EXTRA_DIST = texinfo.tex machine.info machine.info* *.html

all-local: machine.html

SUFFIXES = .texi .html

.texi.html:
	$(TEXI2HTML) $<

machine.html: ${machine_TEXINFOS}

install-data-local:
	-${top_srcdir}/mkinstalldirs @htmldir@
	-(for XX in machine*.html; do\
	  $(INSTALL_SCRIPT) $${XX} @htmldir@; done)

uninstall-local:
	-rm @htmldir@/april*.html




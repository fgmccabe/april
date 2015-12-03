# 
# Rules for compiling april programs within the build tree
#
APRILDIR = ${top_builddir}/April
APC = ${top_builddir}/April/Compiler/apc
APRIL_PREAMBLE="\#!${exec_prefix}/bin/april $(APRIL_EXEC_FLAGS)"

%.aam: ${srcdir}/%.ap
	@echo Were compiling april program $? ":-)"
	APRIL_DIR=file:///$(APRILDIR) $(APC) $(APCFLAGS) -o $(basename $@).aam $(basename $@).ap

%: ${srcdir}/%.ap
	@echo Construct executable april program $?
	APRIL_DIR=file:///$(APRILDIR) APRIL_PREAMBLE=$(APRIL_PREAMBLE) $(APC) $(APCFLAGS) -o ${basename $@} -x $(basename $@).ap

%.af: ${srcdir}/%.ap
	@echo We are generating april $@ from $<
	APRIL_DIR=file:///$(APRILDIR) $(APC) $(APCFLAGS) -M -o $(basename $@).aam $(basename $@).ap

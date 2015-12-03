#
# Main makefile for Unix version of the compiler
#
SUBDIRS = Headers

bin_PROGRAMS = apc

apc_SOURCES = main.c compile.c dict.c escapes.c foldexp.c \
        grammar.c operator.c token.c macro.c display.c heap.c errors.c\
        cell.c symbols.c structure.c stack.c filename.c makesig.c types.c unify.c real.c generalize.c\
        chkexp.c chkptn.c chkstmt.c chktheta.c depends.c compdebug.c comptheta.c compexp.c comptype.c\
	compptn.c compList.c compstmt.c comptest.c assem.c gencode.c comphash.c compchoice.c

INCLUDES = -I@top_srcdir@/April/Compiler/Headers  -I@ooiodir@/include -I@top_srcdir@/April/Headers '-DAPRILDIR="@prefix@"'

# Special dependencies
escapes.o: $(top_srcdir)/April/Headers/escapes.h

EXTRA_DIST = grammar.y

apc_LDADD = @ooiodir@/lib/libooio.la


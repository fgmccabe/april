#
# Makefile.am to create the april engine
#
SUBDIRS = Headers Doc

bin_PROGRAMS = april

april_SOURCES = main.c dict.c evaluate.c debug.c remdebug.c process.c\
        schedule.c compact.c generation.c opaque.c\
        msg.c handle.c string.c arith.c error.c \
        escapes.c code.c verify.c types.c coerce.c\
	args.c clock.c misc.c utility.c \
        writef.c chars.c read.c labels.c encode.c decode.c\
        setops.c sort.c socket.c pipe.c fileio.c\
	dir.c signal.c load.c 

INCLUDES = -I@top_srcdir@/April/Engine/Headers -I@ooiodir@/include -I@top_srcdir@/April/Headers '-DAPRILDIR="@prefix@"'
april_LDFLAGS = @april_LDFLAGS@
april_LDADD = @ooiodir@/lib/libooio.la

lib_LTLIBRARIES = libdltest.la libieeestr.la 

libdltest_la_SOURCES = dltest.c

libieeestr_la_SOURCES = ieee.c

# Special dependency 
escapes.o: $(top_srcdir)/April/Headers/escapes.h

EXTRA_DIST = log2.c dltest.ap apdltst.ap 


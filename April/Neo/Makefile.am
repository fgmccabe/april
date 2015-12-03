## Process with Automake to generate Makefile.in and eventually Makefile
## This will eventually be the April compiler

-include ${top_builddir}/April/april.Make
APRIL_EXEC_FLAGS= -h2048
APCFLAGS = -M

APRIL_SRCS = operators.ap opgrammar.ap aptok.ap display.ap genmacro.ap\
	streamer.ap
APRIL_CODE = operators.aam opgrammar.aam aptok.aam display.aam streamer.aam\
	genmacro.aam
include_HEADERS = abstract.ah cps.ah ap_tok.ah op_tree.ah

SUFFIXES = .af

EXTRA_DIST = transform.ap\
	aptok.alx\
	aptoktest.ap\
	plustimes.ap tpl.ap gt.ap foo.ap ops.ah\
	${APRIL_SRCS} grtest.ap macro.ap macro.af

grtest: ${APRIL_CODE} aptok.af opgrammar.af macro.af operators.af display.af

genmacro: ${APRIL_CODE} opgrammar.af operators.af display.af streamer.af

streamer.aam streamer.af: aptok.af display.af

aptok.ap:	aptok.alx
	${top_builddir}/April/Alex/alex aptok.alx

macro.ap: genmacro
	APRIL_DIR=file:///$(top_builddir)/April ${top_builddir}/April/Engine/april genmacro foo.ap

grtest.aam:	opgrammar.af aptok.af display.af macro.af streamer.af

opgrammar.aam:	operators.af

display.aam: operators.af

CLEANFILES = ${APRIL_CODE} grtest aptok.af operators.af opgrammar.af

all-local: genmacro


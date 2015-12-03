SUBDIRS = Test Suite

APRIL_FILES = fact.ap bench.ap ball.ap base.ap naive.ap reverse.ap\
	sem.ap phil.ap surgery.ap plipplop.ap double.ap\
	echo.ap sieve.ap csieve.ap world.ap fibonacci.ap sort.ap tp.ap\
	tree.ap qsort.ap flt.ap\
	mozart-single.ap mozart-server.ap mozart-client.ap\
	mozart-prod.ap mozart-cons.ap

APRIL_CODE =  fact.aam bench.aam ball.aam base.aam naive.aam reverse.aam\
	sem.aam phil.aam surgery.aam plipplop.aam double.aam\
	echo.aam sieve.aam csieve.aam world.aam fibonacci.aam sort.aam tp.aam\
	tree.aam qsort.aam flt.aam

EXTRA_APRIL = ${APRIL_FILES}
-include ${top_builddir}/April/april.Make

EXTRA_DIST = april_survey.tmpl question.ap survey.ap\
	baseball.ah\
	calculator.ap\
	client.ap server.ap\
	echo.ap\
	world2.ap\
	world3.ap\
	world4.ap\
	wor.ap hel.ap\
	grapher.ap\
	host.ap\
	infile.ap\
	inline.ap\
	pusher.ap mobile.ap mobile.ah mover.ap mover.ah\
	queue.ap\
	read.ap reader.ap remplip.ap\
	hello.af hello.ap\
	test.objects.ap\
	shell.ap\
	${APRIL_FILES}

CLEANFILES = ${APRIL_CODE}

all-local: ${APRIL_CODE}

install-data-local: ${APRIL_CODE}
	-${top_srcdir}/mkinstalldirs ${prefix}/april
	-(for XX in ${APRIL_CODE}; do\
	  $(INSTALL_SCRIPT) $${XX} ${prefix}/april; done)
	-(cd ${srcdir}; $(INSTALL_SCRIPT) ${APRIL_FILES} ${prefix}/april)


#ifndef _FLAGS_H_
#define _FLAGS_H_

#ifdef ALLTRACE			/* are we enabling all tracing? */
#define MACROTRACE
#define COMPTRACE
#define TYPETRACE
#define MEMTRACE
#define TRACEPARSE
#endif

extern logical SymbolDebug;	/* symbolic debugging generation */
extern logical traceCompile;	/* Are we tracing compilation? */
extern logical traceMemory;	/* Trace compiler heap management */
extern logical traceMacro;	/* Trace macro processing */
extern logical traceType;	/* tracing of the type checker */

#endif




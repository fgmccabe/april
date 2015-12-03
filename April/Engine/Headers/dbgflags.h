#ifndef _FLAGS_H_
#define _FLAGS_H_

#include "logical.h"

extern logical debugging;	/* instruction tracing option */
extern logical interactive;	/* Whether it should be interactive */
extern logical SymbolDebug;	/* symbolic debugging generation */
extern logical traceEscapes;	/* tracing of escape calls  */
extern logical traceSuspend;	/* suspension tracing  */
extern logical stressSuspend;	/* process stress testing  */
extern logical traceClock;	/* Clock and timer tracing  */
extern logical traceVerify;	/* Tracing of code verification */
extern logical traceMessage;	/* message tracing  */
extern logical traceTCP;	/* tracing of TCP messages */
extern logical traceResource;	/* tracing of the resource manager */
extern logical traceBank;	/* tracing of cash accounting */

#ifdef ALLTRACE			/* are we enabling all tracing? */
#ifndef PROCTRACE
#define PROCTRACE
#endif
#ifndef MEMTRACE
#define MEMTRACE
#endif
#ifndef EXECTRACE
#define EXECTRACE
#endif
#ifndef MSGTRACE
#define MSGTRACE
#endif
#ifndef CLOCKTRACE
#define CLOCKTRACE
#endif
#ifndef RESTRACE
#define RESTRACE
#endif
#ifndef BANKTRACE
#define BANKTRACE
#endif
#ifndef VERIFYTRACE
#define VERIFYTRACE
#endif
#ifndef RESOURCETRACE
#define RESOURCETRACE
#endif
#endif

#ifdef MEMTRACE
extern logical traceMemory;	/* tracing of stack memory access */
extern logical stressMemory;	/* stress test GC */
#endif

#endif




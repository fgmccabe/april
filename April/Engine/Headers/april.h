/* 
 Public header file for accessing April's abstract syntax
*/
#ifndef _APRIL_H
#define _APRIL_H

#include <stdlib.h>
#include <assert.h>
#include "dbgflags.h"		/* standard debugging flags */
#include "local.h"		/* localization */
#include "ooio.h"

typedef struct process_ *processpo;
typedef unsigned WORD32 instruction; /* Each instruction is 32 bits long */
typedef instruction* insPo;	/* pointer to instructions */

#include "word.h"		/* Standard definition of a cell & access fns */
#include "handle.h"
#include "alloc.h"
#include "symbols.h"		/* standard symbols available to the engine */
#include "str.h"		/* include string utilities */
#include "pool.h"		/* include memory pool utilities */
#include "hosts.h"		/* standard host name functions */

#ifndef NULL
#define NULL            ((void*)0) /* The NULL pointer */
#endif

#define MAXNAME 1024		/* Maximum size of an identifier */

#undef NumberOf
#define NumberOf(a) (sizeof(a)/sizeof(a[0]))

typedef retCode (*funpo)(processpo p,objPo *args); /* Escape function pointer */

void emulate(register processpo P);

#include "stats.h"		/* Statistics functions */
#include "dict.h"
#include "signals.h"

void init_args(char **argv, int argc, int start);
retCode m_command_line(processpo p,objPo *args);
retCode m_shell(processpo p,objPo *args);
retCode m_envir(processpo p,objPo *args);
retCode m_setenv(processpo p,objPo *args);
retCode m_getenv(processpo p,objPo *args);
retCode m_exec(processpo p,objPo *args);
retCode m_waitpid(processpo p,objPo *args);

/*
   Error reporting functions
*/
void syserr(char *msg);
void raiseError(processpo P,objPo errval);
extern uniChar errorMsg[1024];
retCode liberror(char *name,int arity,char *error,objPo code);
retCode Uliberror(char *name, int arity, uniChar *errmsg, objPo code);
retCode SpaceErr(void);		/* Run out of space during execution */
void april_exit(int code);

#define EXIT_SUCCEED 0		/* Normal exit */
#define EXIT_FAIL 1		/* Failing exit */

retCode force(objPo type,objPo ag,objPo *forced);

#ifdef EXECTRACE
void showEscape(processpo P,long code,objPo *args);
#endif
#endif

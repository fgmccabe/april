/*
   April compiler header file
   (c) 1994-2000 Imperial College and F.G. McCabe

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
   
   Contact: Francis McCabe <fgm@fla.fujitsu.com>
 */

#ifndef _COMPILE_H_
#define _COMPILE_H_

#include "ooio.h"			/* access our file library */
#include "pool.h"                       /* include memory pool utilities */
#include "local.h"                      /* localization */
#include "logical.h"                    /* definition of boolean */
#include "cdbgflags.h"                /* pick up the relevant debugging flags */
#include "cell.h"                       /* Cell structure interface */
#include "filename.h"

typedef struct {
  uniChar *header;
  uniChar *include;		/* include path */
  uniChar *sysinclude;		/* System include path */
  uniChar *macJoin;
  uniChar *out_name;

  int dLvl;                     // debugging level
  logical noWarnings;
  logical preProcOnly;
  logical executable;
  logical verbose;		/* true for feedback from the compiler*/
  logical makeAf;		/* true if we want to generate an .af file */
} OptInfoRec, *optPo;

void initCompiler(void);	/* Initialize the compiler tables */
void initDisplay(void);		/* initalize display system */
void Process(ioPo inFile,ioPo outFile,optionPo options);
logical compile(ioPo inFile,ioPo outFile,optionPo options);
void assembleFile(ioPo inFile,ioPo outFile);

/* Various Maxima */
#define MAXARG 125		/* Maximum number of args to a function call */
#define MAXENV 127		/* Maximum number of local variables in code */
#define MAXSTR 512		/* Default maximum size of a string var */
#define MAXERRORS 16		/* Maximum number of errors reported */
#define MAX_SYMB_LEN 512	/* Maximum length of a symbol */


#ifndef NumberOf
#define NumberOf(a) (sizeof(a)/sizeof(a[0]))
#endif

typedef WORD64 int64;

/* General functions */
void comp_exit(int code); /* Exit the compiler system */

void syserr(char *msg);		/* Report a fatal system error -- should never happen */


/* Symbol types */
typedef enum{function=0,procedure} entrytype;

void init_dict(void);		/* initialize the dictionary */

typedef enum {continued, jumped} cont;

typedef struct _contin_ *continPo;
typedef int (*contin_fun)(cpo code,int cl);

typedef struct _contin_ {
  int cl;			/* Continuation private data */
  contin_fun fn;		/* Continuation function to call */
  continPo next;		/* Next continuation to invoke */
} ContinRec;

typedef struct _context_ *cxpo;

typedef struct _context_{
  enum {none,valof,collect,labelled,continuation} ct; /* value context type */
  int dest,depth;		/* destination, collection depth */
  cellpo type;			/* expected result type */
  lblpo brk;			/* where to go on a break  */
  continPo valCont;		/* Continuation for valis statement */
  symbpo label;			/* Break label */
  cxpo prev;			/* Previous context */
} context;			/* compiler context record */

/* Compiler's dictionary structure */
#include "dict.h"
#include "structure.h"		/* Abstract syntax definitions */

logical complexExp(cellpo exp);

cellpo argType(cellpo type);
cellpo resType(cellpo type);

logical IsVarDecPtn(cellpo input,cellpo type,dictpo dict);
cellpo VarDecPtn(cellpo input,cellpo type,cellpo *name,dictpo dict);

int IsEscapeFun(symbpo e,int *arity,cellpo *at,cellpo *t);/* escape fun */
int EscapeFun(symbpo name, cellpo *t);

int IsEscapeProc(symbpo name,int *arity,cellpo *at);
int EscapeProc(symbpo name, cellpo *t);

logical IsEscape(symbpo name,cellpo t);

logical IsProgramType(cellpo t, dictpo dict);

logical IsAtest(cellpo input,dictpo dict);

typedef void (*errProc)(char *error,uniChar *msg1,cellpo file,long no,void *cl);
void reportErr(int line,char *msg,...);
void reportWarning(int line,char *msg,...);
void reportParseErr(int line,char *msg,...);
void resetErrors(errProc handler,void *client);
extern long comperrflag;	/* Has there been a syntax error? */
extern long compwarnflag;	/* Has there been a warning? */
extern long synerrors;		/* has there been a parse error */


cellpo ArgTypes(cellpo args,dictpo dict);
cellpo ATypes(cellpo args,dictpo dict,dictpo *nd);

void TypeCopy(cellpo input, cellpo output);
cellpo NewTypeVar(cellpo tgt);

#include "stack.h"		/* Stack functions */
#include "heap.h"		/* Simple heap management functions */
#include "display.h"		/* Display functions */

void install_escapes(void);
void init_escapes(void);

#endif

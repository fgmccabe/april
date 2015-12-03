/*
 * Install standard escapes into April compiler
 * (c) 1994-1998 Imperial College and F.G.McCabe
 * All rights reserved
 *
 */

#include "config.h"
#include <string.h>		/* String functions */
#include <stdlib.h>
#include "compile.h"
#include "keywords.h"
#include "dict.h"
#include "makesig.h"		/* Compiler's accessto type signatures */
#include "types.h"
#include "hash.h"

static hashPo fescapes;	/* The hash table of escape functions */
static hashPo pescapes;	/* The hash table of escape procedures */

#define MAXESC 257		/* initial size of the escape hash table */

typedef struct {
  int code;			/* opcode used for this escape function */
  unsigned long arity;		/* Number of operands */
  cellpo type;			/* The type associated with this escape */
  cellpo types;			/* Vector of argument types */
  cellpo rtype;			/* Result type -- for function */
} escrec, *escpo;

static void install_pesc(symbpo escape_fn,int code,char *spec);
static void install_fesc(symbpo escape_fn,int code,char *spec);

#define fescape(name,cname,code,pr,spec) install_fesc(locateC(name),code,spec);
#define pescape(name,cname,code,pr,spec) install_pesc(locateC(name),code,spec);

void init_escapes(void)
{
  /* set up the escape tables */
  fescapes = NewHash(MAXESC,(hashFun)uniHash,(compFun)uniCmp,NULL);
  /* Build function escape table */
  pescapes = NewHash(MAXESC,(hashFun)uniHash,(compFun)uniCmp,NULL);
}

void install_escapes(void)
{
#include "escapes.h"
}


/* Install a symbol into the procedure escapes table */
static void install_pesc(symbpo escape_fn,int code,char *spec)
{
  if((escpo)Search(escape_fn,pescapes)==NULL){
    escpo e = (escpo)malloc(sizeof(escrec));

    Install(escape_fn,e,pescapes); /* Install opcode into table */

    regenType(spec,(e->type=allocSingle()),True);	/* parse the type signature */

    isProcType(e->type,&e->arity);	/* unpack the procedure type */

    e->code = code;		/* code used to invoke the escape */
    e->types = tupleOfArgs(e->type,allocSingle());
    e->rtype = voidcell;
  }
}

/* Install a symbol into the functions escapes table */
static void install_fesc(symbpo escape_fn,int code,char *spec)
{
  if((escpo)Search(escape_fn,fescapes)==NULL){
    escpo e = (escpo)malloc(sizeof(escrec));

    Install(escape_fn,e,fescapes); /* Install opcode into table */

    regenType(spec,(e->type=allocSingle()),False); /* regenerate a type from signature */
    
    isFunType(e->type,&e->types,&e->rtype);	
    e->code = code;		/* code used to invoke the escape */
    if(isTpl(e->types))
      e->arity = tplArity(e->types);	/* number of arguments */
    else
      e->arity = 1;
  }
}

/* Locate an escape function and return escape types etc */
int IsEscapeFun(symbpo name,int *arity,cellpo *at, cellpo *t)
{
  escpo e = (escpo)Search(name,fescapes);
 
  if(e!=NULL){
    if(at!=NULL)
      *at = e->types;
    if(t!=NULL)
      *t = e->rtype;
    if(arity!=NULL)
      *arity = e->arity;
    return e->code;
  }
  else
    return -1;
}

/* Locate an escape function and return its type etc */
int EscapeFun(symbpo name, cellpo *t)
{
  escpo e = (escpo)Search(name,fescapes);
 
  if(e!=NULL){
    if(t!=NULL)
      *t=e->type;
    return e->code;
  }
  else
    return -1;
}

/* Locate an escape procedure and return types of arguments */
int IsEscapeProc(symbpo name, int *arity,cellpo *at)
{
  escpo e = (escpo)Search(name,pescapes);
 
  if(e!=NULL){
    if(at!=NULL)
      *at = e->types;
    if(arity!=NULL)
      *arity = e->arity;
    return e->code;
  }
  else
    return -1;
}

int EscapeProc(symbpo name,cellpo *t)
{
  escpo e = (escpo)Search(name,pescapes);
 
  if(e!=NULL){
    if(t!=NULL)
      *t = e->type;
    return e->code;
  }
  else
    return -1;
}


/* General escape lookup */
logical IsEscape(symbpo name,cellpo t)
{
  cellpo type;
  if(EscapeFun(name,&type)>=0 || EscapeProc(name,&type)>=0){
    if(t!=NULL)
      copyCell(t,type);
    return True;
  }
  else
    return False;
}


#ifdef ALLTRACE
static retCode dumpEsc(void *n,void *r,void *c)
{
  escpo e = (escpo)r;
  
  outMsg(logFile,"Escape: %s -- %w\n",n,e->type);
  return Ok;
}

void dumpEscapes(void)
{
  ProcessTable(dumpEsc,fescapes,NULL);
  ProcessTable(dumpEsc,pescapes,NULL);
}
#endif

/*
 * Compiler dictionary maintenance functions 
 * (c) 1994-1998 Imperial College and F.G.McCabe
 * All rights reserved
 *
 */
#include "config.h"
#include <stdlib.h>		/* Standard functions */
#include <string.h>
#include <assert.h>
#include "compile.h"		/* Compiler structures */
#include "keywords.h"		/* Standard April keywords */
#include "assem.h"
#include "types.h"

#define MAXDICT 1024		/* initial size of compiler dictionary */
#define MINBOUND 3		/* first bound variable */

typedef struct _dict_ {
  centry ce;			/* the kind of dictionary entry this is */
  assMode rw;			/* is this a r/w variable */
  initStat inited;		/* true if variable has been initialized */
  logical reported;		/* true if we have reported on this variable */
  vartype vt;			/* is this local or in the environment */
  cellpo name;			/* the name of the entry */
  int arity;			/* any arity that is associated with entry */
  cellpo type;			/* the type associated with this variable */
  cellpo ft;			/* function resutl type */
  cellpo at;			/* argument type vector */
  int offset;			/* integer offset value */
  int index;			/* offset to index variable */
  dictTrail trail;		/* marker which indicates when this variable was bound */
  dictpo next;			/* next entry in the dictionary */
}dctrec;

static poolPo dictpool=NULL; /* pool of dictionary entries */

void init_compiler_dict(void)
{
  dictpool = newPool(sizeof(dctrec),MAXDICT);
}

static dictTrail nextTr(dictpo dict);

/* declare a variable*/
dictpo declVar(cellpo name,cellpo type,vartype vt,
	       assMode rw,initStat inited,logical reported,
	       int offset,int index,dictpo dct)
{

  dictpo d=(dictpo)allocPool(dictpool); /* grab a new dictionary entry */
  cellpo lhs,rhs;

  d->name = name;
  d->ce = cvar;
  d->rw = rw;			/* a read/write variable? */
  d->inited = inited;		/* has this variable been initialized? */
  d->reported = reported;
  d->vt = vt;			/* local or environment variable? */
  d->type = dupCell(deRef(type));
  d->offset = offset;
  d->next = dct;
  d->index = index;

  if(inited)
    d->trail = nextTr(dct);
  else
    d->trail = -1;		/* Not initialized */

  /* Check for function and procedure variables */
  if(isFunType(d->type,&lhs,&rhs)){ /* really a function? */
    d->ce = cfun;
    d->ft = deRef(rhs);
    d->at = deRef(lhs);
    d->arity = argArity(d->at);
  }
  else if(isProcType(d->type,NULL)){
    d->ce = cproc;		/* we have a procedure */
    d->at = tupleOfArgs(d->type,allocSingle());
    d->arity = tplArity(d->at);
  }
  return d;
}

/* Set a scope mark */
dictpo addMarker(dictpo dct)
{
  dictpo d=(dictpo)allocPool(dictpool); /* grab a new dictionary entry */

  d->ce = cmark;
  d->next = dct;
  d->name = NULL;		/* an unlikely name */
  d->type = NULL;
  d->trail = -1;
  return d;
}

dictpo stripMarks(dictpo dict)
{
  while(dict!=NULL && dict->ce==cmark)
    dict = dict->next;
  return dict;
}


dictpo outerDict(dictpo dict)
{
  while(dict!=NULL && dict->ce!=cmark)
    dict = dict->next;

  if(dict!=NULL)
    return dict->next;

  return NULL;
}

/* This is useful in computing the `lexical depth' of a bit of code */
int countMarkers(dictpo d)
{
  int count = 0;
  while(d!=NULL){
    if(d->ce==cmark)
      count++;
    d = d->next;
  }
  return count;
}

void InitVar(cellpo v,logical inited,dictpo dict)
{
  dictpo d = dict;
  while(dict!=NULL)
    if(dict->name!=NULL && sameCell(dict->name,v)){
      if(inited && !dict->inited)
	dict->trail = nextTr(d);
      dict->inited=inited;	/* mark the variable as initialised */
      break;
    }
    else
      dict=dict->next;		/* the next dictionary entry */
}

logical inittedVar(cellpo v,dictpo dict)
{
  while(dict!=NULL)
    if(dict->name!=NULL && sameCell(dict->name,v)){
      if(dict->inited==Inited)
	return True;
      else
	return False;
    }
    else
      dict=dict->next;		/* the next dictionary entry */
  return False;
}

void ReportVar(cellpo v,logical reported,dictpo dict)
{
  while(dict!=NULL)
    if(dict->name!=NULL && sameCell(dict->name,v)){
      dict->reported=reported;	/* mark the variable as reported */
      return;
    }
    else
      dict=dict->next;		/* the next dictionary entry */
}


static dictTrail nextTr(dictpo dict)
{
  long tr = -1;

  while(dict!=NULL){
    if(dict->trail>tr)
      tr = dict->trail;
    dict = dict->next;
  }
  return tr+1;
}

dictTrail currentTrail(dictpo dict)
{
  return nextTr(dict)-1;
}

void resetDict(dictTrail trail,dictpo dict)
{
  while(dict!=NULL){
    if(dict->trail>trail){
      dict->inited = False;	/* uninitialize */
      dict->trail = -1;
    }
    dict = dict->next;
  }
}
      

/* Clear down a dictionary down to the mark u */
dictpo clearDict(dictpo dict, dictpo u)
{
  dictpo d=dict;

  while(d!=NULL)
    if(d==u){
      d=dict;			/* restart from the beginning*/

      while(d!=u){
	dictpo d1 = d->next;
	
	freePool(dictpool,d); /* release the storage */

	d=d1;
      }
      return u;			/* we've cleared it */
    }
  else
    d=d->next;

  if(u!=NULL)
    outStr(logFile,"Something wrong in clearDict\n");
  return dict;			/* if we got here something went wrong */
}

/* Clear down a dictionary down to the mark u */
dictpo chopDict(dictpo top, dictpo chop, dictpo base)
{
  dictpo *d=&top;

  while(*d!=NULL && *d!=chop && *d!=base)
    d = &(*d)->next;

  if(*d==chop){
    *d = base;
    clearDict(chop,base);
    return top;
  }
  return top;
}

logical IsFunVar(cellpo fn,int *r,cellpo *ft,cellpo *at,int *off,int *index,
		 vartype *vt,dictpo dict)
{

  while(dict!=NULL)
    if(dict->ce==cfun && sameCell(dict->name,fn)){
      if(ft!=NULL)
	*ft=dict->ft;	/* return the type of the function */
      if(at!=NULL)
	*at=dict->at;
      if(index!=NULL)
	*index = dict->index;	/* index variable */
      if(off!=NULL)
	*off=dict->offset;	/* return the location of the function */
      if(vt!=NULL)
	*vt=dict->vt;		/* is function in the environment vector? */
      if(r!=NULL)
	 *r = dict->arity;
      return True;
    }
    else
      dict=dict->next;		/* step on though the dictionary */
  return False;
}

logical IsProcVar(cellpo fn,int *r,cellpo *at,int *off,int *index,
		  vartype *vt,dictpo dict)
{

  while(dict!=NULL)
    if(dict->ce==cproc && sameCell(dict->name,fn)){
      if(at!=NULL)
	*at=dict->at;
      if(index!=NULL)
	*index=dict->index;	/* return the index offset */
      if(off!=NULL)
	*off=dict->offset;	/* return the location of the procedure */
      if(vt!=NULL)
	*vt=dict->vt;		/* is procedure in the environment vector? */
      if(r!=NULL)
	 *r = dict->arity;
      return True;
    }
    else
      dict=dict->next;		/* step on though the dictionary */
  return False;
}

logical IsVar(cellpo v,centry *et,cellpo *t,int *off,int *index,
	      assMode *rw,initStat *inited,vartype *vt,dictpo dict)
{
  while(dict!=NULL)
    if(dict->ce!=ctype && dict->ce!=ccons && dict->ce!=cmark && 
       sameCell(dict->name,v)){
      if(et!=NULL)
	*et=dict->ce;		/* the kind of variable */
      if(t!=NULL)
	*t=dict->type;		/* return the type of the variable */
      if(index!=NULL)
	*index=dict->index;	/* return the index offset */
      if(off!=NULL)
	*off=dict->offset;	/* return the offset of the variable */
      if(rw!=NULL)
	*rw=dict->rw;		/* is the variable rw? */
      if(inited!=NULL)
	*inited=dict->inited;	/* is the variable initialised yet? */
      if(vt!=NULL)
	*vt=dict->vt;		/* return whether var is local or env */
      return True;
    }
    else
      dict=dict->next;	/* the next dictionary entry */
  return False;
}

logical IsLocalVar(cellpo v,dictpo dict)
{
  while(dict!=NULL && dict->ce != cmark)
    if(dict->ce!=ctype && dict->ce!=ccons && dict->ce!=cmark && 
       sameCell(dict->name,v))
      return True;
    else
      dict=dict->next;	/* the next dictionary entry */
  return False;
}

logical IsType(cellpo tp,cellpo *head,cellpo *body,dictpo dict)
{
  while(dict!=NULL)
    if(dict->ce == ctype && sameCell(dict->name,tp)){
      if(head!=NULL)
	*head = dict->type;
      if(body!=NULL)
	*body = dict->ft;
      return True;
    }
    else
      dict=dict->next;	/* the next dictionary entry */
  return False;
}

/* True if a symbol is from a type enumeration */
static logical InType(cellpo type, symbpo s)
{
  cellpo e,o;

  if(isBinaryCall(type,kchoice,&e,&o))
    return InType(e,s) || InType(o,s);
  else
    return isSymb(type) && symVal(type)==s;
}

cellpo dictName(dictpo entry)
{
  return entry->name;
}

centry dictCe(dictpo entry,cellpo name)
{
  assert(entry->name==name);
  return entry->ce;
}

vartype dictVt(dictpo entry,cellpo name)
{
  assert(entry->name==name);
  return entry->vt;
}

assMode dictRW(dictpo entry,cellpo name)
{
  assert(entry->name==name);
  return entry->rw;
}

initStat dictInited(dictpo entry,cellpo name)
{
  assert(entry->name==name);
  return entry->inited;
}

int dictOffset(dictpo entry,cellpo name)
{
  assert(entry->name==name);
  return entry->offset;
}

int dictIndex(dictpo entry,cellpo name)
{
  assert(entry->name==name);
  return entry->index;
}

cellpo dictType(dictpo entry,cellpo name)
{
  assert(entry->name==name);
  return entry->type;
}

logical dictReported(dictpo entry,cellpo name)
{
  assert(entry->name==name);
  return entry->reported;
}

void setDictReported(dictpo entry,cellpo name,logical reported)
{
  assert(entry->name==name);
  entry->reported = reported;
}

void processDict(dictpo top,dictpo bottom,dictProc proc,void *cl)
{
  while(top!=NULL && top!=bottom){
    proc(top,cl);
    top = top->next;
  }
}

/* Process dictionary in reverse order! */
void processRevDict(dictpo top,dictpo bottom,dictProc proc,void *cl)
{
  if(top!=NULL){
    if(top->next!=NULL && top->next!=bottom)
      processRevDict(top->next,bottom,proc,cl); 

    proc(top,cl);
  }
}

static void showVar(dictpo d,char *type)
{
  switch(d->vt){
  case localvar:
    outMsg(logFile,"%s%s%s %w @ FP[%d] -- %#10.1800w\n",
	   (d->rw==multiAss?"":"r/o "),
	   (d->inited==Inited?"":"not inited "),
	   type,
	   d->name,d->offset,d->type);
    break;
  case envvar:
    outMsg(logFile,"%s%s%s %w @ E[%d] -- %#10.1800w\n",
	   (d->rw==multiAss?"":"r/o "),
	   (d->inited==Inited?"":"not inited "),
	   type,
	   d->name,d->offset,d->type);
    break;
  case indexvar:
    outMsg(logFile,"%s%s%s %w @ FP[%d][%d] -- %#10.1800w\n",
	   (d->rw==multiAss?"":"r/o "),
	   (d->inited==Inited?"":"not inited "),
	   type,
	   d->name,d->offset,d->index,d->type);
    break;
  case thetavar:
    outMsg(logFile,"%s%s%s %w @ E[%d][%d] -- %#10.1800w\n",
	   (d->rw==multiAss?"":"r/o "),
	   (d->inited==Inited?"":"not inited "),
	   type,
	   d->name,d->offset,d->index,d->type);
  }
}

void dump_dict(dictpo d,char *msg,long depth)
{
  if(msg!=NULL)
    outMsg(logFile,"%s dictionary\n",msg);
  
  while(depth-->0 &&d!=NULL){
    switch(d->ce){
    case cvar:
      showVar(d,"variable");
      break;

    case cfun:
      showVar(d,"function");
      break;

    case cproc:
      showVar(d,"function");
      break;

    case cptn:
      showVar(d,"function");
      break;
 
    case ccons:
      outMsg(logFile,"constructor %s/%d `%w' -> `%10w'\n",
	      d->name,d->arity,symVal(d->ft),d->type);
      break;

    case ctype:
      outMsg(logFile,"type %w%10.1459w%s%10.1459w\n",d->name,d->type,ktype,d->ft);
      break;

    case cunknown:
      outMsg(logFile,"unknown variable %w -- `%5w'\n",d->name,d->type);
      break;

    case cmark:
      outMsg(logFile,"Mark\n");
    }
    d=d->next;
  }
}

void dumpDict(dictpo d)
{
  dump_dict(d,NULL,10000);
  flushFile(logFile);
}

void dD(dictpo d,long dp)
{
  dump_dict(d,NULL,dp);
  flushFile(logFile);
}


/*
  Encapsulation of the major compiler data structures of cells, strings and floats
  (c) 1994-2000 Imperial College and F.G.McCabe

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
 
#include "config.h"
#include "compile.h"
#include "cellP.h"
#include "operators.h"
#include "keywords.h"
#include "types.h"
#include "typesP.h"
#include "hash.h"		/* access the hash functions */
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/* accessing interface for a cell */
tag_tp tG(cellpo x)
{
  return x->tag;
}

cellpo Next(cellpo x)
{
  return x+1;
}

cellpo Prev(cellpo x)
{
  return x-1;
}

cellpo nthCell(cellpo x,long i)
{
  return x+i;
}

short int whenVisited(cellpo x)
{
  return x->visited;
}

void visitCell(cellpo x,short int i)
{
  x->visited = i;
}

logical IsInt(cellpo x)
{
  return x->tag==intger;
}

logical IsInteger(cellpo x)
{
  return x->tag==intger;
}

long intVal(cellpo x)
{
#ifdef MEMTRACE
  if(traceMemory && x->tag!=intger)
    syserr("tried to access non-integer as integer");
#endif
  return x->d.i;
}


long integerVal(cellpo x)
{
  return intVal(x);
}

void mkInt(register cellpo x,long numb)
{
  x->tag = intger;
  x->d.i = numb;
  x->l_info = -1;
  x->type = NULL;
  x->visited = 0;
}

cellpo allocInt(long num)
{
  cellpo I = allocSingle();
  mkInt(I,num);
  return I;
}

logical IsFloat(cellpo x)
{
  return x->tag==floating;
}

FLOAT fltVal(cellpo x)
{
#ifdef MEMTRACE
  if(traceMemory && x->tag!=floating)
    syserr("tried to access non-floating as floating");
#endif
  return *x->d.f;
}

void mkFlt(register cellpo c, FLOAT f)
{
  floatpo fp;

  if((fp=allocFloat(f))!=NULL){
    c->tag = floating;
    c->visited = 0;
    c->d.f = fp;
    c->l_info = -1;
    c->type = NULL;
  }
}

logical isSymb(cellpo x)
{
  return x->tag==symbolic;
}

logical IsSymbol(cellpo in,symbpo sym)
{
  return tG(in)==symbolic && in->d.s==sym;
}

symbpo symVal(cellpo x)
{
#ifdef MEMTRACE
  if(traceMemory && x->tag!=symbolic)
    syserr("tried to access non-symbol as symbol");
#endif
  return x->d.s;
}

void mkSymb(register cellpo x,symbpo sym)
{
  x->tag = symbolic;
  x->visited = 0;
  x->d.s = sym;
  x->l_info = -1;
  x->type = NULL;
}

void mkChar(register cellpo x,uniChar ch)
{
  x->tag = character;
  x->visited = 0;
  x->d.ch = ch;
  x->l_info = -1;
  x->type = NULL;
}

logical isChr(cellpo x)
{
  return x->tag==character;
}

uniChar CharVal(cellpo x)
{
  assert(isChr(x));
  
  return x->d.ch;
}

logical IsString(cellpo x)
{
  return x->tag==strng;
}

uniChar *strVal(cellpo x)
{
#ifdef MEMTRACE
  if(traceMemory && x->tag!=strng)
    syserr("tried to access non-string as string");
#endif
  return x->d.st;
}

void mkStr(register cellpo x,uniChar *str)
{
  x->tag = strng;
  x->visited = 0;
  x->d.st = str;
  x->l_info = -1;
  x->type = NULL;
}

logical IsIdent(cellpo x)
{
  return x->tag==identifier;
}

logical IsId(cellpo in,symbpo sym)
{
  return tG(in)==symbolic && in->d.s==sym;
}

symbpo idVal(cellpo x)
{
  assert(IsIdent(x));
  return x->d.s;
}

void mkIdent(register cellpo x,symbpo sym)
{
  x->tag = identifier;
  x->visited = 0;
  x->d.s = sym;
  x->l_info = -1;
  x->type = NULL;
}

logical IsList(cellpo x)
{
  return x->tag==list;
}

logical isEmptyList(cellpo x)
{
  return x->tag==list && x->d.t==NULL;
}

logical isNonEmptyList(cellpo x)
{
  return x->tag==list && x->d.t!=NULL;
}

cellpo listVal(cellpo x)
{
  assert(!traceMemory || x->tag==list);
  
  return x->d.t;
}

cellpo listHead(cellpo x)
{
  assert(!traceMemory || x->tag==list);
  
  return x->d.t;
}

cellpo listTail(cellpo x)
{
  assert(!traceMemory || x->tag==list);

  return x->d.t+1;
}

long listLength(cellpo x)
{
  long count = 0;
  while(isNonEmptyList(x)){
    count++;
    x = listTail(x);
  }
  if(isEmptyList(x))
    return count;
  else
    return -1;
}

void mkList(register cellpo x,register cellpo lst)
{
  x->tag = list;
  x->visited = 0;
  x->d.t = lst;
  x->l_info = -1;
  x->type = NULL;
}

logical isTpl(cellpo x)
{
  return x->tag==tuple;
}

cellpo tplVal(cellpo x)
{
#ifdef MEMTRACE
  if(traceMemory && x->tag!=tuple)
    syserr("tried to access non-tuple as tuple");
#endif
  return x->d.t;
}

unsigned long tplArity(cellpo x)
{
#ifdef MEMTRACE
  if(traceMemory && x->tag!=tuple)
    syserr("tried to access non-tuple as tuple");
#endif
  return intVal(x->d.t);
}

cellpo tplArg(cellpo x,unsigned long n)
{
#ifdef MEMTRACE
  if(traceMemory){
    if(x->tag!=tuple)
      syserr("tried to access non-tuple as tuple");
    else if(n<0 || n>=tplArity(x))
      syserr("bounds error in accessing tuple");
  }
#endif
  return x->d.t+1+n;
}

void updateTpl(cellpo x,unsigned long n,cellpo val)
{
  assert(isTpl(x) && tplArity(x)>n);

  x->d.t[n+1] = *val;
}

void mkTpl(register cellpo x,register cellpo tpl)
{
  x->tag = tuple;
  x->visited = 0;
  x->d.t = tpl;
  x->l_info = -1;
  x->type = NULL;
}

logical isCons(cellpo x)
{
  return x->tag==constructor;
}

unsigned long consArity(cellpo x)
{
  assert(isCons(x));

  return (unsigned)intVal(x->d.t);
}

cellpo consFn(cellpo x)
{
  assert(isCons(x));
  
  return x->d.t+1;
}

cellpo consEl(cellpo x,unsigned long i)
{
  assert(isCons(x) && consArity(x)>i);
  
  return x->d.t+i+2;
}


void updateConsEl(cellpo x,unsigned long n,cellpo val)
{
  assert(isCons(x) && consArity(x)>n);

  x->d.t[n+2] = *val;
}

void updateConsCons(cellpo x,cellpo fn)
{
  assert(isCons(x));
  
  x->d.t[1]=*fn;
}

void mkCons(register cellpo x,register cellpo tpl)
{
  x->tag = constructor;
  x->visited = 0;
  x->d.t = tpl;
  x->l_info = -1;
  x->type = NULL;
}

logical IsCode(cellpo x)
{
  return x->tag==codeseg;
}

codepo codeVal(cellpo x)
{
#ifdef MEMTRACE
  if(traceMemory && x->tag!=codeseg)
    syserr("tried to access non-code as code");
#endif
  return x->d.c;
}

void mkCode(register cellpo x,codepo pc)
{
  x->tag = codeseg;
  x->visited = 0;
  x->d.c = pc;
  x->l_info = -1;
  x->type = NULL;
}

logical IsTypeVar(cellpo x)
{
  return x->tag==tvar;
}

cellpo deRef(register cellpo t)
{
  while(tG(t)==tvar){
    cellpo value = t->d.t;

    if(value==NULL || value==t)
      return t;
    else
      t=value;				/* Follow the chain down to the end */
  }
  
  return t;
}

cellpo refVal(cellpo x)
{
#ifdef MEMTRACE
  if(traceMemory && x->tag!=tvar)
    syserr("tried to access non-type var");
#endif
  return x->d.t;
}

void mktvar(register cellpo x,register cellpo value)
{
  x->tag = tvar;
  x->visited = 0;
  x->d.t = value;
  x->l_info = -1;
  x->type = NULL;
}

cellpo dupCell(cellpo c)
{
  cellpo x = allocSingle();		/* a copy of the cell itself */
  
  *x = *c;
  x->visited = 0;
  return x;
}

cellpo copyCell(cellpo dst,cellpo src)
{
  if(src!=dst)
    *dst = *src;
  return dst;
}

/*
 * Compare two cells for equality -- this is able to handle fixpoint structures
 */

static logical sameCll(cellpo c1,cellpo c2,tracePo left,tracePo right)
{
  c1 = deRef(c1);
  c2 = deRef(c2);

  if(c1==c2)
    return True;
  else
    switch(c1->tag){
    case intger:
      switch(c2->tag){
      case intger:
	return intVal(c1)==intVal(c2);
      case floating:
	return intVal(c1)==fltVal(c2);
      default:
	return False;
      }
      
    case floating:
      switch(c2->tag){
      case intger:
	return fltVal(c1)==intVal(c2);
      case floating:
	return fltVal(c1)==fltVal(c2);
      default:
	return False;
      }

    case symbolic:
      return c2->tag==symbolic && (symVal(c1)==symVal(c2) || uniCmp(symVal(c1),symVal(c2))==0);

    case strng:
      return c2->tag==strng && uniCmp(strVal(c1),strVal(c2))==0;
      
    case character:
      return c2->tag==character && CharVal(c1)==CharVal(c2);

    case codeseg:
      return c2->tag==codeseg && codeVal(c1)==codeVal(c2);

    case list:
      if(c2->tag==list){
	if(isEmptyList(c1))
	  return isEmptyList(c2);
	else if(isNonEmptyList(c2))
	  return sameCll(c1=listVal(c1),c2=listVal(c2),left,right) &&
	    sameCll(c1+1,c2+1,left,right);
	else
	  return False;
      }
      else
	return False;

    case tuple:
      if(c2->tag==tuple){
	register long i= tplArity(c1);
	register long j = tplArity(c2);

	c1 = tplVal(c1);
	c2 = tplVal(c2);

	if(i!=j)
	  return False;
	else{
	  while(i--){
	    if(!sameCll(++c1,++c2,left,right))
	      return False;
	  }
	  return True;
	}
      }
      else
	return False;

    case constructor:
      if(c2->tag==constructor){
	register unsigned long ar = consArity(c1);

	if(ar!=consArity(c2) || !sameCll(consFn(c1),consFn(c2),left,right))
	  return False;
	else{
	  unsigned int i;
	  
	  for(i=0;i<ar;i++)
	    if(!sameCll(consEl(c1,i),consEl(c2,i),left,right))
	      return False;

	  return True;
	}
      }
      else
	return False;

    case tvar:
      return c2->tag==tvar && c1==c2;

    default:
      return False;
    }
}

logical sameCell(cellpo c1,cellpo c2)
{
  return sameCll(c1,c2,NULL,NULL);
}

/* construct line number information */
typedef struct _line_data {
  cellpo file;			/* Which file this fragment is in */
  int line;			/* Line number object is at */
} line_data;

static linepo line_table;	/* Table of line number information */
static int max_line;		/* Maximum size of the line number table */
static int cur_line = 0;	/* Current top of line info table */

static hashPo file_table = NULL;

static void delstr(void *n,void *r)
{
  free(n);
}

void initLineTable(void)
{
  if(line_table==NULL){
    line_table = (linepo)malloc(sizeof(line_data)*1024);
    max_line = 1024;
  }

  if(line_table==NULL)
    syserr("couldnt allocate line number table");

  if(file_table!=NULL)
    DelHash(file_table);

  file_table = NewHash(32,(hashFun)uniHash,(compFun)uniCmp,(destFun)delstr);
  cur_line=0;
}

int mkLineInfo(uniChar *file,int at)
{
  cellpo fn=(cellpo)Search(file,file_table);

  if(fn==NULL){
    fn = allocSingle();
    mkSymb(fn,locateU(file));
    Install(symVal(fn),fn,file_table);
  }

  if(cur_line>0 && fn==line_table[cur_line-1].file && 
     line_table[cur_line-1].line==at)
    return cur_line-1;
  else{
    if(cur_line>=max_line){
      max_line = max_line + max_line/2;
      line_table = (linepo)realloc(line_table,sizeof(line_data)*max_line);

      if(line_table==NULL)
	MemExit();		/* Ran out of space */
    }

    if(fn!=NULL)		/* the file exists elsewhere */
      line_table[cur_line].file = fn;
    else
      mkSymb(line_table[cur_line].file=allocSingle(),locateU(file));
    line_table[cur_line].line = at;
    return cur_line++;
  }
}

void setLineInfo(int at,uniChar *file,cellpo tgt)
{
  tgt->l_info = mkLineInfo(file,at);
}

void setCellLineInfo(int at,cellpo file,cellpo tgt)
{
  if(cur_line>0 && file==line_table[cur_line-1].file && 
     line_table[cur_line-1].line==at)
    tgt->l_info = cur_line-1;
  else{
    if(cur_line>=max_line){
      max_line = max_line + max_line/2;
      line_table = (linepo)realloc(line_table,sizeof(line_data)*max_line);

      if(line_table==NULL)
	MemExit();		/* Ran out of space */
    }

    line_table[cur_line].file = file;
    line_table[cur_line].line = at;
    tgt->l_info = cur_line++;
  }
}

void copyLineInfo(cellpo in,cellpo out)
{
  out->l_info = in->l_info;
}

int lineInfo(cellpo x)
{
  return x->l_info;
}

void stLineInfo(cellpo x,int l)
{
  x->l_info = l;
}

cellpo sourceFile(int line)
{
  if(line!=-1)
    return line_table[line].file;
  else
    return NULL;
}

int sourceLine(int line)
{
  if(line!=-1)
    return line_table[line].line;
  else
    return -1;
}

cellpo source_file(cellpo in)
{
  return sourceFile(in->l_info);
}

int source_line(cellpo in)
{
  return sourceLine(in->l_info);
}

uniChar *sourceLocator(char *prefix,int line)
{
  cellpo file = sourceFile(line);
  long line_no = sourceLine(line);
  static uniChar msg[1024]={0};

  if(file!=NULL)
    return strMsg(msg,NumberOf(msg),"%s%U/%d",prefix,symVal(file),line_no);
  else
    return msg;
}

/* Try to pick up type information about an expression
 * and sort out the variable being declared if necessary
 */

cellpo typeInfo(cellpo exp)
{
  return exp->type;
}

cellpo setTypeInfo(cellpo exp,cellpo type)
{
  exp->type = type;
  return exp;
}

cellpo copyTypeInfo(cellpo dest,cellpo in)
{
  dest->type = in->type;
  return dest;
}


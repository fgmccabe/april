/*
  Hash table generator
  (c) 1994-1999 Imperial College and F.G.McCabe

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
#include <stdlib.h>		/* Standard functions */
#include <string.h>		/* String processing standard library */
#include <assert.h>		/* Compile-time assertions */
#include <limits.h>		/* We need these */
#include "compile.h"		/* Compiler structures */
#include "generate.h"		/* Code generation interface */
#include "types.h"		/* Type cheking interface */
#include "dict.h"
#include "keywords.h"		/* Standard April keywords */
#include "opcodes.h"		/* April machine opcodes */
#include "assem.h"		/* Code generation*/
#include "makesig.h"
#include "hash.h"		/* access the hash functions */
#include "cdbgflags.h"		/* Debugging option flags */

static long countIntDisj(cellpo input,long *min,long *max);
static long countSymDisj(cellpo input,dictpo dict);
static long countCharDisj(cellpo input);
static long nxtPrime(long n);

/* 
 * compHash generates a hash table from a disjunction
 */
typedef struct _h_bucket_ *bkPo;

typedef struct _h_bucket_ {
  cellpo key;
  cellpo input;
  long offset;
  lblpo L;
  bkPo next;
} hEntry;

static logical bldTable(cellpo input,bkPo *table,bkPo *buckets,dictpo dict,
			long factor)
{
  cellpo lhs,rhs;
  if(isBinaryCall(input,kstmt,&lhs,&rhs)||isBinaryCall(input,kfn,&lhs,&rhs)){
    if(LiteralSymbPtn(lhs,&lhs,dict)){
      long ix = uniHash(symVal(lhs))%factor;
      bkPo bk = (*buckets)++;	/* grab a new bucket */
      bkPo *f = table+ix;
      
      bk->key = lhs;
      bk->input = input;
      bk->L = NULL;
      bk->next = NULL;

      while(*f!=NULL)
	f = &(*f)->next;

      *f = bk;
      return True;
    }
    else
      return False;
  }
  else if(isBinaryCall(input,kchoice,&lhs,&rhs))
    return bldTable(lhs,table,buckets,dict,factor) &&
      bldTable(rhs,table,buckets,dict,factor);
  else
    return False;
}

static logical bldIntTable(cellpo input,bkPo *table,bkPo *buckets,
			   long min,long factor)
{
  cellpo lhs,rhs;
  if(isBinaryCall(input,kstmt,&lhs,&rhs)||isBinaryCall(input,kfn,&lhs,&rhs)){
    if(IsInteger(lhs)){
      long ix = integerVal(lhs)-min;
      bkPo bk = (*buckets)++;	/* grab a new bucket */
      bkPo *f = table+ix;
      
      bk->key = lhs;
      bk->input = input;
      bk->L = NULL;
      bk->next = NULL;

      while(*f!=NULL)
	f = &(*f)->next;

      *f = bk;
      return True;
    }
    else
      return False;
  }
  else if(isBinaryCall(input,kchoice,&lhs,&rhs))
    return bldIntTable(lhs,table,buckets,min,factor) &&
      bldIntTable(rhs,table,buckets,min,factor);
  else
    return False;
}

static logical bldCharTable(cellpo input,bkPo *table,bkPo *buckets,long factor)
{
  cellpo lhs,rhs;
  if(isBinaryCall(input,kstmt,&lhs,&rhs)||isBinaryCall(input,kfn,&lhs,&rhs)){
    if(isChr(lhs)){
      long ix = CharVal(lhs);
      bkPo bk = (*buckets)++;	/* grab a new bucket */
      bkPo *f = table+ix;
      
      bk->key = lhs;
      bk->input = input;
      bk->L = NULL;
      bk->next = NULL;

      while(*f!=NULL)
	f = &(*f)->next;

      *f = bk;
      return True;
    }
    else
      return False;
  }
  else if(isBinaryCall(input,kchoice,&lhs,&rhs))
    return bldCharTable(lhs,table,buckets,factor) &&
      bldCharTable(rhs,table,buckets,factor);
  else
    return False;
}


#ifdef COMPTRACE
static void showTable(bkPo *table,long factor)
{
  int i;

  for(i=0;i<factor;i++){
    if(table[i]!=NULL){
      char *prefix = "[";
      bkPo entry = table[i];

      outMsg(logFile,"%d: ",i);

      while(entry!=NULL){
	outMsg(logFile,"%s%w -> %.900w",prefix,entry->key,entry->input);
	prefix = ",";
	entry = entry->next;
      }
      outStr(logFile,"]\n");
    }
  }
  flushFile(logFile);
}
#endif

logical buildHash(cellpo input,int arity,int srcvar,int depth,
		  lblpo LF,lblpo Next,dictpo dict,cpo code,
		  int dLvl,choiceFun comp,void *cl)
{
  long count = countSymDisj(input,dict);
  long min = LONG_MAX;
  long max = -LONG_MAX;

  if(count>1){
    long factor = nxtPrime(count);
    hEntry bckts[factor];
    bkPo buckets=&bckts[0];
    bkPo table[factor];
    long i;

    for(i=0;i<factor;i++)
      table[i]=NULL;		/* clear the hash table */

    bldTable(input,table,&buckets,dict,factor); /* construct the table */

#ifdef COMPTRACE
    if(traceCompile)
      showTable(table,factor);
#endif

    genIns(code,"Switch table",hjmp,srcvar,factor);
    genIns(code,"Failure jump",jmp,LF);

				/* construct the jump table */
    for(i=0;i<factor;i++){
      if(table[i]==NULL)	/* empty table entry */
	genIns(code,"Empty hash entry",jmp,LF);
      else{
	lblpo Lx = table[i]->L = NewLabel(code);
	genIns(code,"Try buckets",jmp,Lx);
      }
    }

    /* Construct the individual code segments */
    for(i=0;i<factor;i++){
      if(table[i]!=NULL){
	bkPo first = table[i];

	while(first!=NULL){
	  lblpo Lf = LF;

	  if(first->next!=NULL)	/* define the next label */
	    Lf = first->next->L = NewLabel(code);

	  DefLabel(code,first->L);
	  comp(first->input,arity,srcvar,depth,Next,Lf,dict,code,
	       False,dLvl,cl);
	  first = first->next;
	}
      }
    }

    return True;
  }
  /* check for a dense integer table */
  else if((count=countIntDisj(input,&min,&max)) && 
	  count >1 && count>((max-min)/2)){
    long factor = max-min+1;
    hEntry bckts[factor];
    bkPo buckets = &bckts[0];
    bkPo table[factor];
    long i;
    int D;

    for(i=0;i<factor;i++)
      table[i]=NULL;		/* clear the hash table */

    bldIntTable(input,table,&buckets,min,factor); /* construct the table */

#ifdef COMPTRACE
    if(traceCompile)
      showTable(table,factor);
#endif

    if(min!=0){
      D = depth-1;

      if(abs(min)<128)
	genIns(code,"adjust index",incr,srcvar,-min,D);
      else{
	cellpo tmp=allocSingle();
	mkInt(tmp,min);
	genIns(code,NULL,movl,tmp,D);
	genIns(code,"adjust index",minus,srcvar,D,D);
      }
    }
    else
      D = srcvar;

    genIns(code,"Switch table",ijmp,D,factor);
    genIns(code,"Failure jump",jmp,LF);

				/* construct the jump table */
    for(i=0;i<factor;i++){
      if(table[i]==NULL)	/* empty table entry */
	genIns(code,"Empty hash entry",jmp,LF);
      else{
	lblpo Lx = table[i]->L = NewLabel(code);
	genIns(code,"Try buckets",jmp,Lx);
      }
    }

    /* Construct the individual code segments */
    for(i=0;i<factor;i++){
      if(table[i]!=NULL){
	bkPo first = table[i];

	while(first!=NULL){
	  lblpo Lf = LF;

	  if(first->next!=NULL)	/* define the next label */
	    Lf = first->next->L = NewLabel(code);

	  DefLabel(code,first->L);
	  comp(first->input,arity,srcvar,depth,Next,Lf,dict,code,
	       False,dLvl,cl);

	  first = first->next;
	}
      }
    }
    return True;
  }
  /* check for a dense character table */
  else if((count=countCharDisj(input)) >1){
    long factor = nxtPrime(count);
    hEntry bckts[factor];
    bkPo buckets=&bckts[0];
    bkPo table[factor];
    long i;

    for(i=0;i<factor;i++)
      table[i]=NULL;		/* clear the hash table */

    bldCharTable(input,table,&buckets,factor); /* construct the table */

#ifdef COMPTRACE
    if(traceCompile)
      showTable(table,factor);
#endif

    genIns(code,"Switch table",cjmp,srcvar,factor);
    genIns(code,"Failure jump",jmp,LF);

				/* construct the jump table */
    for(i=0;i<factor;i++){
      if(table[i]==NULL)	/* empty table entry */
	genIns(code,"Empty hash entry",jmp,LF);
      else{
	lblpo Lx = table[i]->L = NewLabel(code);
	genIns(code,"Try buckets",jmp,Lx);
      }
    }

    /* Construct the individual code segments */
    for(i=0;i<factor;i++){
      if(table[i]!=NULL){
	bkPo first = table[i];

	while(first!=NULL){
	  lblpo Lf = LF;

	  if(first->next!=NULL)	/* define the next label */
	    Lf = first->next->L = NewLabel(code);

	  DefLabel(code,first->L);
	  comp(first->input,arity,srcvar,depth,Next,Lf,dict,code,
	       False,dLvl,cl);
	  first = first->next;
	}
      }
    }

    return True;
  }
  else
    return False;
}

logical LiteralSymbPtn(cellpo input,cellpo *sym,dictpo dict)
{
  if(IsQuote(input,&input) && isSymb(input)){
    if(sym!=NULL)
      *sym = input;
    return True;
  }
  else if(isSymb(input)){
    uniChar *sy = symVal(input);

    if(sym!=NULL)
      *sym = input;

    if(sy==kuscore)
      return False;
    else{			/* literal symbol or variable */
      if(IsVar(input,NULL,NULL,NULL,NULL,NULL,NULL,NULL,dict))
	return False;
      else
	return isConstructorType(typeInfo(input),NULL,NULL);
    }
  }
  else if(IsEnumStruct(input,&input)){
    if(sym!=NULL)
      *sym = input;
    return True;
  }
  else
    return False;
}

static long countSymDisj(cellpo input,dictpo dict)
{
  cellpo lhs,rhs;
  if(isBinaryCall(input,kstmt,&lhs,NULL)||isBinaryCall(input,kfn,&lhs,NULL)){
    if(LiteralSymbPtn(lhs,NULL,dict))
      return 1;
    else
      return -1;
  }
  else if(isBinaryCall(input,kchoice,&lhs,&rhs)){
    long lcount = countSymDisj(lhs,dict);
    long rcount = countSymDisj(rhs,dict);

    if(lcount==-1||rcount==-1)
      return -1;
    else
      return lcount+rcount;
  }
  else
    return -1;
}

static long countIntDisj(cellpo input,long *min,long *max)
{
  cellpo lhs,rhs;
  if(isBinaryCall(input,kstmt,&lhs,NULL)||isBinaryCall(input,kfn,&lhs,NULL)){
    if(IsInteger(lhs)){
      long i = integerVal(lhs);

      if(i<*min)
	*min=i;

      if(i>*max)
	*max = i;
      return 1;
    }
    else
      return -1;
  }
  else if(isBinaryCall(input,kchoice,&lhs,&rhs)){
    long lcount = countIntDisj(lhs,min,max);
    long rcount = countIntDisj(rhs,min,max);

    if(lcount==-1||rcount==-1)
      return -1;
    else
      return lcount+rcount;
  }
  else
    return -1;
}

static long countCharDisj(cellpo input)
{
  cellpo lhs,rhs;
  if(isBinaryCall(input,kstmt,&lhs,NULL)||isBinaryCall(input,kfn,&lhs,NULL)){
    if(isChr(lhs))
      return 1;
    else
      return -1;
  }
  else if(isBinaryCall(input,kchoice,&lhs,&rhs)){
    long lcount = countCharDisj(lhs);
    long rcount = countCharDisj(rhs);

    if(lcount==-1||rcount==-1)
      return -1;
    else
      return lcount+rcount;
  }
  else
    return -1;
}


/* Utility function to compute prime numbers */
 
typedef struct {
  long pr;
  long next;
} PrimeRec, *primePo;

static long nxtPrime(long n)
{
  long i;
  long len = n/2+1;
  PrimeRec table[len];
  long pos = 0;
  long pr = 3;

  while(pr<=n){
    assert(pos<len);

    table[pos].pr=pr;
    table[pos++].next=2*pr;

    pr+=2;

    i=0;
    while(i<pos){
      if(pr>table[i].next)
	table[i].next+=table[i].pr;

      if(pr==table[i].next){
	table[i].next+=table[i].pr;
	pr+=2;
	i=0;			/* start again */
      }
      else
	i++;
    }
  }

  return pr;
}

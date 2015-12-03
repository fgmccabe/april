/*
  Abstract syntax structure accessing functions for April compiler
  (c) 1994-1998 Imperial College and F.G.McCabe

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
#include <assert.h>		/* debugging assertions */
#include "compile.h"
#include "cellP.h"		/* We need to know what a cell looks like */
#include "keywords.h"		/* Standard April keywords */

unsigned long argArity(cellpo type)
{
  if(isTpl(type=deRef(type)))
    return tplArity(type);
  else
    return 1;
}

/*
 * Test a structure for a specific function 
 */

logical IsStruct(cellpo input, cellpo *fn,unsigned long *ar)
{
  if(isCons(input)){
    if(fn!=NULL)
      *fn = consFn(input);
    if(ar!=NULL)
      *ar = consArity(input);
    return True;
  }
  else
    return False;
}

logical isCall(cellpo input, symbpo fn,unsigned long *ar)
{
  cellpo f;
  
  if(isCons(input) && IsSymbol((f=consFn(input)),fn)){
    if(ar!=NULL)
      *ar = consArity(input);
    return True;
  }

  return False;
}

cellpo BuildStruct(cellpo fn,unsigned long ar,cellpo tgt)
{
  cellpo els = allocCons(ar);
  
  copyCell(&els[1],fn);
  
  mkCons(tgt,els);
  return tgt;
}

cellpo callArg(cellpo input,unsigned long n)
{
  assert(isCons(input));
  
  return consEl(input,n);
}

logical IsAnyStruct(cellpo input,cellpo *a1)
{
  return isUnaryCall(input,kany,a1);
}

cellpo BuildAnyStruct(cellpo a1,cellpo tgt)
{
  return BuildUnaryCall(kany,a1,tgt);
}

logical IsEnumStruct(cellpo input,cellpo *a1)
{
  return isUnaryCall(input,kenum,a1);
}

cellpo BuildEnumStruct(cellpo a1,cellpo tgt)
{
  return BuildUnaryCall(kenum,a1,tgt);
}

cellpo tupleOfArgs(cellpo input,cellpo tgt)
{
  assert(isCons(input));

  {
    unsigned long ar = consArity(input);
    unsigned long i;
    cellpo args = allocTuple(ar);
    
    mkTpl(tgt,args);

    for(i=0;i<ar;i++)
      copyCell(tplArg(tgt,i),consEl(input,i));
      
    return tgt;
  }
}

cellpo tupleIze(cellpo input)
{
  cellpo lhs,rhs;
  
  if(isTpl(input))
    return input;
  else if(isBinaryCall(input,kguard,&lhs,&rhs)){
    tupleIze(lhs);
    return BuildBinCall(kguard,lhs,rhs,input);
  }
  else{
    cellpo a = allocTuple(1);
    
    copyCell(&a[1],input);
    
    mkTpl(input,a);
    return input;
  }
}

/*
 * Check for a ternary application
 */
logical IsTernaryCall(cellpo input,cellpo *f,cellpo *a1,cellpo *a2,cellpo *a3)
{
  if(isCons(input) && consArity(input)==3){
    if(f!=NULL)
      *f = consFn(input);
    if(a1!=NULL)
      *a1 = consEl(input,0);
    if(a2!=NULL)
      *a2 = consEl(input,1);
    return True;
  }

  return False;
}

logical IsTernaryStruct(cellpo input,symbpo f,cellpo *a1,cellpo *a2,cellpo *a3)
{
  cellpo fn;
  
  if(isCons(input) && consArity(input)==3 && isSymb(fn=consFn(input)) && symVal(fn)==f){
    if(a1!=NULL)
      *a1 = consEl(input,0);
    if(a2!=NULL)
      *a2 = consEl(input,1);
    if(a3!=NULL)
      *a3 = consEl(input,2);
    return True;
  }

  return False;
}

/*
 * Check for a binary application
 */
logical IsBinaryStruct(cellpo input,cellpo *f,cellpo *a1,cellpo *a2)
{
  if(isCons(input) && consArity(input)==2){
    if(f!=NULL)
      *f = consFn(input);
    if(a1!=NULL)
      *a1 = consEl(input,0);
    if(a2!=NULL)
      *a2 = consEl(input,1);
    return True;
  }

  return False;
}

/*
 * Check for a particular binary function
 */
logical isBinaryCall(cellpo input,symbpo fn,cellpo *a1,cellpo *a2)
{
  if(isCons(input) && consArity(input)==2 && IsSymbol(consFn(input),fn)){
    if(a1!=NULL)
      *a1 = consEl(input,0);
    if(a2!=NULL)
      *a2 = consEl(input,1);
    return True;
  }

  return False;
}

/*
 * Build a binary function call structure -- using heap
 */
cellpo BuildBinCall(symbpo fn,cellpo a1,cellpo a2,cellpo tgt)
{
  cellpo call = allocCons(2);

  mkSymb(&call[1],fn);
  call[2]=*a1;
  call[3]=*a2;
  
  mkCons(tgt,call);
  return tgt;
}

cellpo BuildBinStruct(cellpo fn,cellpo a1,cellpo a2,cellpo tgt)
{
  cellpo call = allocCons(2);

  call[1]=*fn;
  call[2]=*a1;
  call[3]=*a2;
  
  mkCons(tgt,call);
  return tgt;
}

/*
 * Check for a particular unary function
 */
logical isUnaryCall(cellpo input, symbpo fn, cellpo *a1)
{
  if(isCons(input) && IsSymbol(consFn(input),fn) && consArity(input)==1){
    if(a1!=NULL)
      *a1 = consEl(input,0);
    return True;
  }
  else
    return False;
}

logical IsUnaryStruct(cellpo input, cellpo *fn, cellpo *a1)
{
  if(isCons(input) && consArity(input)==1){
    if(fn!=NULL)
      *fn = consFn(input);
    if(a1!=NULL)
      *a1 = consEl(input,0);
    return True;
  }
  else
    return False;
}

/*
 * Build a unary function call structure -- using heap
 */
cellpo BuildUnaryCall(symbpo fn,cellpo a1,cellpo tgt)
{
  cellpo els = allocCons(1);
  
  mkSymb(&els[1],fn);
  copyCell(&els[2],a1);
  
  mkCons(tgt,els);

  return tgt;
}

cellpo BuildUnaryStruct(cellpo fn,cellpo a1,cellpo tgt)
{
  cellpo call = allocCons(1);

  call[1]=*fn;
  call[2]=*a1;
  
  mkCons(tgt,call);
  return tgt;
}

/*
 * Test for a zero-arg function 
 */
logical IsZeroStruct(cellpo t,symbpo fn)
{  
  return isCons(t) && IsSymbol(consFn(t),fn);
}

cellpo BuildZeroStruct(cellpo fn,cellpo tgt)
{
  cellpo els = allocCons(0);
  
  copyCell(&els[1],fn);
  mkCons(tgt,els);

  return tgt;
}

logical isCodeClosure(cellpo input)
{
  return isCons(input) && IsCode(consFn(input));
}

logical IsMacroDef(cellpo input,cellpo *pt,cellpo *rp)
{
  cellpo lhs;

  if(IsHashStruct(input,kmacro,&lhs) &&
     isBinaryCall(lhs,kmacreplace,pt,rp))
    return True;
  else
    return False;
}

logical IsVoid(cellpo input)
{
  return IsSymbol(input,kvoid);
}

logical IsTheta(cellpo input,cellpo *body)
{
  if(isUnaryCall(input,ksemi,NULL)||isBinaryCall(input,ksemi,NULL,NULL)){
    if(body!=NULL)
      *body = input;
    return True;
  }
  else
    return False;
}

/*
 * Check for a quoted expression
 */
logical IsQuote(cellpo input, cellpo *a1)
{
  return isUnaryCall(input,kquote,a1);
}

logical IsMeta(cellpo input, cellpo *a1)
{
  return isUnaryCall(input,kmeta,a1);
}

logical IsHashStruct(cellpo input, symbpo fn, cellpo *a1)
{
  cellpo arg;
  
  return isUnaryCall(input,khash,&arg) && isUnaryCall(arg,fn,a1);
}

logical IsQuery(cellpo input,cellpo *lhs,cellpo *rhs)
{
  return isBinaryCall(input,kquery,lhs,rhs);
}

cellpo BuildQuery(cellpo lhs,cellpo rhs,cellpo tgt)
{
  return BuildBinCall(kquery,lhs,rhs,tgt);
}

logical IsShriek(cellpo input,cellpo *exp)
{
  return isUnaryCall(input,kshriek,exp);
}

logical IsForAll(cellpo input,cellpo *lhs,cellpo *rhs)
{
  return isBinaryCall(input,kallQ,lhs,rhs);
}

cellpo BuildForAll(cellpo lhs,cellpo rhs,cellpo tgt)
{
  return BuildBinStruct(allQ,lhs,rhs,tgt);
}

cellpo BuildFunType(cellpo lhs,cellpo rhs,cellpo tgt)
{
  return BuildBinStruct(funTp,lhs,rhs,tgt);
}

cellpo BuildProcType(unsigned long ar,cellpo tgt)
{
  return BuildStruct(procTp,ar,tgt);
}

cellpo BuildListType(cellpo lhs,cellpo tgt)
{
  if(lhs!=NULL)
    return BuildUnaryStruct(listTp,lhs,tgt);
  else{
    BuildUnaryStruct(listTp,voidcell,tgt);
    
    NewTypeVar(consEl(tgt,0));
    return tgt;
  }
}


logical IsTvar(cellpo input,cellpo *name)
{
  cellpo nme;
  if(isUnaryCall(input,ktvar,&nme) && isSymb(nme)){
    if(name!=NULL)
      *name=nme;
    return True;
  }
  else
    return False;
}


cellpo BuildHashStruct(symbpo fn,cellpo a1,cellpo tgt)
{
  BuildUnaryCall(fn,a1,tgt);
  BuildUnaryCall(khash,tgt,tgt);
  return tgt;
}


logical IsTuple(cellpo input,unsigned long *ari)
{
  assert(input!=NULL);

  if(isTpl(input)){
    if(ari!=NULL)
      *ari = tplArity(input);
    return True;
  }
  else
    return False;
}

cellpo BuildTuple(unsigned long arity,cellpo tgt)
{
  mkTpl(tgt,allocTuple(arity));

  return tgt;
}

/*
 * Convert a list into a tuple
 */

cellpo List2Tuple(cellpo input,cellpo tgt)
{
  long ar = 0;
  cellpo ptr=input;
  cellpo dst;
  int l_info = input->l_info;	/* We will copy this across */

  while(isNonEmptyList(ptr)){	/* Compute the length of the list */
    ar++;
    ptr=listVal(ptr)+1;
  }

  if(ar!=1){
    cellpo ptr = Next(dst=allocTuple(ar)); /* we want to allow tgt to override input */
  
    while(isNonEmptyList(input)){
      input = listVal(input);
      *ptr++=*input++;		/* copy the head of the list into the tuple element */
    }

    mkTpl(tgt,dst);
    tgt->l_info=l_info;		/* preserve line information */
    return tgt;
  }
  else{
    copyCell(tgt,listVal(input));
    tgt->l_info=l_info;		/* preserve line information */
    return tgt;
  }    
}

/*
 * Convert a tuple into a list
 */
cellpo Tuple2List(cellpo input,cellpo tgt)
{
  cellpo fn;
  unsigned long ar;
  
  if(isTpl(input)){
    cellpo dst=tgt;
    int l_info = input->l_info;	/* We will copy this across */
    
    ar = tplArity(input);
    
    if(ar>0){
      cellpo tpl=tplArg(input,0); /* We must go in to the input tuple to allow input=tgt */
      unsigned int i;

      for(i=0;i<ar;i++){
	cellpo pair = allocPair();
	mkList(dst,pair);
	dst->l_info=l_info;

	*pair++=*tpl++;
	dst = pair;
      }
    }

    mkList(dst,NULL);
    dst->l_info=l_info;
    return tgt;
  }
  else if(IsStruct(input,&fn,&ar)){
    int l_info = input->l_info;	/* We will copy this across */
    cellpo lst = allocSingle();
    cellpo pair = lst;
    unsigned long i;
    
    lst->l_info=l_info;
    
    for(i=0;i<ar;i++){
      cellpo pr = allocPair();
      
      mkList(pair,pr);
      pair->l_info=l_info;
      *pr++=*consEl(input,i);
      pair = pr;
    }
    
    mkList(pair,NULL);
    pair->l_info=l_info;
    
    *tgt = *lst;
    return tgt;
  }
  else{				/* create a single-element list */
    cellpo pair = allocPair();
    mkList(tgt,pair);
    copyLineInfo(input,tgt);
    *pair++=*input++;
    mkList(pair,NULL);
    return tgt;
  }
}

/* 
 * Join two tuples together 
 */
cellpo JoinTuples(cellpo a,cellpo b,cellpo tgt)
{
  unsigned long Aar = tplArity(a);
  unsigned long Bar = tplArity(b);
  cellpo tpl = allocTuple(Aar+Bar);
  unsigned long i;
    
  for(i=0;i<Aar;i++)
    tpl[i+1]=*tplArg(a,i);
  for(i=0;i<Bar;i++)
    tpl[i+Aar+1]=*tplArg(b,i);
  
  mkTpl(tgt,tpl);
  return tgt;
}

/* Strip off type information and debugg flags */
cellpo stripCell(cellpo input)
{
  if(IsHashStruct(input=deRef(input),knodebug,&input))
    return stripCell(input);
  else if(IsHashStruct(input,kdebug,&input))
    return stripCell(input);
  else
    return input;
}

/* Test a structure for being a literal function value */
logical IsFunLiteral(cellpo input)
{
  cellpo lhs,rhs;

  if(isBinaryCall(input=stripCell(input),kchoice,&lhs,&rhs))
    return IsFunLiteral(lhs) || IsFunLiteral(rhs);
  else
    return isBinaryCall(input,kfn,&lhs,&rhs);
}

logical IsProcLiteral(cellpo input)
{
  cellpo lhs,rhs;

  if(isBinaryCall(input=stripCell(input),kchoice,&lhs,&rhs))
    return IsProcLiteral(lhs) || IsProcLiteral(rhs);
  else
    return isBinaryCall(input=stripCell(input),kstmt,&lhs,NULL);
}

/*
  Compute the `generalization' of a type expression
  This means constructing a series of quantifiers for variables
  not referenced outside a type expression
  Compute the fresh version of a quantified type expression
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
#include <stdlib.h>		/* Standard functions */
#include <assert.h>
#include "compile.h"		/* Compiler structures */
#include "cellP.h"
#include "types.h"		/* Type cheking interface */
#include "typesP.h"		/* Private data structures for type checking */
#include "keywords.h"		/* Standard April keywords */

static logical alreadyQuantified(cellpo t,lstPo lst)
{
  while(lst!=NULL){
    if(occInType(t,lst->type))
      return True;
    else
      lst = lst->tail;
  }
  return False;
}

static logical freeInEnv(cellpo t,envPo env)
{
  while(env!=NULL){
    if(!isBinaryCall(env->type,ktype,NULL,NULL) && occInType(t,env->type)) /* ???? */
      return False;
    env = env->tail;
  }
  return True;
}

static logical oInTp(cellpo name,cellpo value,tracePo scope)
{
  cellpo lhs,rhs;

  if(sameCell(name,value=deRef(value)))
    return True;
  else if(isBinaryCall(value,kquery,&lhs,&rhs))
    return oInTp(name,lhs,scope);
  else if(isListType(value,&lhs))
    return oInTp(name,lhs,scope);
  else if(IsForAll(value,&lhs,&rhs)){
    if(sameCell(name,lhs=deRef(lhs)))
      return False;		/* locally bound quantifier */
    else
      return oInTp(value,rhs,scope);
  }
  else if(isTpl(value)){
    long ar = tplArity(value);

    while(ar--)
      if(oInTp(name,tplArg(value,ar),scope))
	return True;

    return False;
  }
  else if(isCons(value)){
    unsigned long ar = consArity(value);
    
    if(oInTp(name,consFn(value),scope))
      return True;

    while(ar--)
      if(oInTp(name,consEl(value,ar),scope))
	return True;

    return False;
  }
  else
    return False;
}

logical occInType(cellpo name,cellpo value)
{
  return oInTp(name,value,NULL);
}


static lstPo genQuants(cellpo t,lstPo copy,envPo outer,tracePo scope)
{
  cellpo lhs,rhs;
  unsigned long ar;

  if(IsTypeVar(t=deRef(t))){
    if(alreadyQuantified(t,copy) || !freeInEnv(t,outer))
      return copy;
    else
      return newLst(t,copy);
  }
  else if(IsQuery(t,&lhs,&rhs))
    return genQuants(lhs,copy,outer,scope);

  else if(isSymb(t))
    return copy;

  else if(isListType(t,&lhs)) 
    return genQuants(lhs,copy,outer,scope);
  else if(isBinaryCall(t,kchoice,&lhs,&rhs)) /* explicit disjunction... */
    return genQuants(rhs,genQuants(lhs,copy,outer,scope),outer,scope);
  else if(IsForAll(t,&lhs,&rhs)){
    envPo nouter = newEnv(lhs,lhs,outer);
    lstPo ncopy = genQuants(rhs,copy,nouter,scope);
    popEnv(nouter,outer);
    return ncopy;
  }
  else if(IsStruct(t,NULL,&ar)){
    int i;
    
    for(i=0;i<ar;i++)
      copy = genQuants(consEl(t,i),copy,outer,scope);
      
    return copy;
  }
  else if(IsTuple(t,&ar)){
    int i;

    for(i=0;i<ar;i++)
      copy = genQuants(tplArg(t,i),copy,outer,scope);
    return copy;
  }
  else
    return copy;		/* No other possiblities */
}

cellpo generalize(cellpo t,envPo env,cellpo tgt)
{
  if(isFunType(t,NULL,NULL)||isProcType(t,NULL) ||
     isBinaryCall(t,ktype,NULL,NULL)||
     isBinaryCall(t,ktpname,NULL,NULL)||
     isBinaryCall(t,kconstr,NULL,NULL)){
    lstPo quants = genQuants(t,NULL,env,NULL);
    lstPo q = quants;

    copyCell(tgt,t);

    while(q!=NULL){
      BuildForAll(q->type,tgt,tgt);
      q = q->tail;
    }

    popLst(quants,NULL);
    return tgt;
  }
  else
    return copyCell(tgt,t);
}

cellpo generalizeTypeExp(cellpo t,envPo env,cellpo tgt)
{
  lstPo quants = genQuants(t,NULL,env,NULL);
  lstPo q = quants;

  copyCell(tgt,t);

  while(q!=NULL){
    BuildForAll(q->type,tgt,tgt);
    q = q->tail;
  }

  popLst(quants,NULL);
  return tgt;
}

static cellpo frshn(cellpo t,envPo env,cellpo tgt,tracePo scope)
{
  cellpo fn,args;
  unsigned long ar;

  if(IsTypeVar(t=deRef(t))){	/* is this type variable being replaced? */
    cellpo type = pickupType(t,env);
    if(type!=NULL)
      return copyCell(tgt,type);
    else
      return copyCell(tgt,t);
  }
  else if(isListType(t,&args)){
    cellpo type = BuildListType(voidcell,tgt);
    frshn(args,env,consEl(type,0),scope);
    return type;
  }
  else if(IsStruct(t,&fn,&ar)){
    long i;
    
    BuildStruct(fn,ar,tgt);
    
    for(i=0;i<ar;i++)
      frshn(consEl(t,i),env,consEl(tgt,i),scope);
    return tgt;
  }
  else if(isTpl(t)){
    long ar = tplArity(t);
    long i;
  
    mkTpl(tgt,allocTuple(ar));

    for(i=0;i<ar;i++)
      frshn(tplArg(t,i),env,tplArg(tgt,i),scope);
    return tgt;
  }
  else if(isSymb(t))
    return copyCell(tgt,t);
  else{
    reportErr(lineInfo(t),"Problem in copying type %w",t);
    return copyCell(tgt,t);
  }
}

cellpo freshType(cellpo t,envPo env,cellpo tgt)
{
  envPo subE = env;
  cellpo var;

  while(IsForAll(t=deRef(t),&var,&t)){	/* pick up the leading quantifiers */
    cellpo xx = NewTypeVar(NULL);
    subE = newEnv(var,xx,subE);
  }

  if(subE==env)
    return copyCell(tgt,t);
  else{
    frshn(t,subE,tgt,NULL);
    popEnv(subE,env);
    return tgt;
  }
}

cellpo freshenType(cellpo t,envPo env,cellpo tgt,cellpo *vars,long *count)
{
  envPo subE = env;
  cellpo var;
  long max = *count;

  *count = 0;

  while(IsForAll(t=deRef(t),&var,&t)){	/* pick up the leading quantifiers */
    cellpo xx = NewTypeVar(NULL);
    if((*count)++<max)
      *vars++=xx;
    subE = newEnv(var,xx,subE);
  }

  if(subE==env)
    return copyCell(tgt,t);
  else{
    frshn(t,subE,tgt,NULL);
    popEnv(subE,env);
    return tgt;
  }
}

  
  

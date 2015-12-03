/*
  Dependency analysis for classifying groups of declarations into tightly
  recursive groups
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
#include "cellP.h"		/* we need this at the moment */
#include "types.h"		/* Type checking interface */
#include "typesP.h"		/* Private data structures for type checking */
#include "keywords.h"		/* Standard April keywords */

/* In order to have minimal types, it is nec. to split the theta body 
   into groups of tightly mutually recursive functions. Mutually recursive 
   functions must be type checked `together'.
   The simplest view of mutually recursive functions is to assume all functions
   in a theta are mutually recursive. This leads to unfortunate side-effects
   in the standard Hindley/Milner typechecking algorithm.

   This dependency analysis algorithm is modified and reverse engineered from
   the Gofer system.
*/

static poolPo dependPool = NULL;

static void pushDep(depStackPo stack,depPo v)
{
  stack->stack[stack->top++]=v;
}

static depPo findDep(cellpo name,depStackPo stack)
{
  int i=0;

  for(i=0;i<stack->top;i++)
    if(sameCell(name,stack->stack[i]->name))
      return stack->stack[i];

  return NULL;
}

static depPo popDep(depStackPo stack)
{
  if(stack->top<=0)
    return NULL;
  else
    return stack->stack[--stack->top];
}

void dumpDpStack(depStackPo stack)
{
  int i=0;
  for(i=0;i<stack->top;i++){
    depPo dep = stack->stack[i];

    while(dep!=NULL){
      if(dep->marker>0)
	outMsg(logFile,"%d: ",dep->marker);
      Display(logFile,dep->name);
      if(dep->next!=NULL)
	outStr(logFile,",");
      dep = dep->next;
    }
    outStr(logFile,"\n");
  }
  flushFile(logFile);
}

void clearDeps(depPo dep)
{
  if(dep!=NULL){
    if(dep->next!=NULL)
      clearDeps(dep->next);
    freePool(dependPool,dep);
  }
}

/* Build a list of dependency records and a new environment from a theta */
static envPo thDepend(cellpo body,envPo env,depStackPo defs)
{
  cellpo lhs,rhs,fn;

  if(IsHashStruct(body,knodebug,&body))
    return thDepend(body,env,defs);
  else if(IsHashStruct(body,kdebug,&body))
    return thDepend(body,env,defs);
  else if(isBinaryCall(body,ksemi,&lhs,&rhs))
    return thDepend(rhs,thDepend(lhs,env,defs),defs);
  else if(isUnaryCall(body,ksemi,&body))
    return thDepend(body,env,defs);
  else if(isBinaryCall(body,ktype,&lhs,&rhs)){
    depPo new = (depPo)allocPool(dependPool);
    cellpo ftype;

    if(IsStruct(lhs,&fn,NULL))
      ftype = typeFunSkel(fn,&env);
    else{
      fn = lhs;
      ftype = typeFunSkel(fn,&env);
    }

    new->name = fn;		/* fill out the dependency  */
    new->type = ftype;
    new->body = body;
    new->marker = -1;
    new->next = NULL;

    pushDep(defs,new);		/* add to the stack */

    return env;			/* we dont modify the environment for this */
  }

  else if(isBinaryCall(body,kconstr,&lhs,&rhs)){
    if(IsStruct(lhs,&fn,NULL)){
      depPo new = (depPo)allocPool(dependPool);
      cellpo ptype = NewTypeVar(NULL); /* New type variable */

      new->name = fn;		/* fill out the dependency  */
      new->type = ptype;	/* prototype of the procedure type */
      new->body = body;		/* constructor definition */
      new->marker = -1;		/* Not yet processed */
      new->next = NULL;

      pushDep(defs,new);	/* add to the stack */
      return newEnv(fn,ptype,env);
    }
    else if(isSymb(lhs)){
      depPo new = (depPo)allocPool(dependPool);
      cellpo ptype = NewTypeVar(NULL); /* New type variable */

      new->name = lhs;		/* fill out the dependency  */
      new->type = ptype;	/* prototype of the procedure type */
      new->body = body;		/* enumerated symbol definition */
      new->marker = -1;		/* Not yet processed */
      new->next = NULL;

      pushDep(defs,new);	/* add to the stack */
      return newEnv(lhs,ptype,env);
    }
    else{
      reportErr(lineInfo(body),"Invalid constructor definition - %w",body);
      return env;
    }
  }

  else if(isBinaryCall(body,ktpname,&lhs,&rhs)){
    depPo new = (depPo)allocPool(dependPool);
    cellpo ftype=BuildBinCall(ktpname,voidcell,voidcell,allocSingle());

    NewTypeVar(callArg(ftype,0));
    NewTypeVar(callArg(ftype,1));

    if(IsStruct(lhs,&fn,NULL))
      ;
    else{
      fn = lhs;
    }

    new->name = fn;		/* fill out the dependency  */
    new->type = ftype;
    new->body = body;
    new->marker = -1;
    new->next = NULL;

    pushDep(defs,new);		/* add to the stack */

    return newEnv(fn,ftype,env);
  }

  /* New-style function or procedure definition */
  else if(isBinaryCall(body,kfield,&lhs,NULL)){
    depPo new = (depPo)allocPool(dependPool);
    cellpo ftype = NewTypeVar(NULL);
    cellpo ft;

    if(IsQuery(lhs,&ft,&lhs))
      realType(ft,env,ftype);

    new->name = lhs;		/* fill out the dependency  */
    new->type = ftype;
    new->body = body;
    new->marker = -1;		/* Not yet processed */
    new->next = NULL;

    pushDep(defs,new);		/* add to the stack */

    return newEnv(lhs,ftype,env);
  }

  else if(isBinaryCall(body,kdefn,&lhs,NULL)){
    depPo new = (depPo)allocPool(dependPool);
    cellpo ptype = NewTypeVar(NULL); /* New type variable */
    cellpo ft;

    if(IsQuery(lhs,&ft,&lhs))
      realType(ft,env,ptype);

    new->name = lhs;		/* fill out the dependency  */
    new->type = ptype;		/* prototype of the procedure type */
    new->body = body;		/* procedure definition */
    new->marker = -1;		/* Not yet processed */
    new->next = NULL;

    pushDep(defs,new);		/* add to the stack */
    return newEnv(lhs,ptype,env);
  }

  else if(isBinaryCall(body,kfn,&lhs,&rhs) && IsStruct(lhs,&fn,NULL) &&
	  !IsSymbol(fn,kguard)){
    BuildBinCall(kdefn,fn,BuildBinCall(kfn,tupleOfArgs(lhs,allocSingle()),rhs,body),body);
    return thDepend(body,env,defs);
  }

  else if(IsUnaryStruct(body,&lhs,&rhs) && IsStruct(lhs,&fn,NULL) &&
	  isSymb(fn)){
    BuildBinCall(kdefn,fn,BuildBinCall(kstmt,tupleOfArgs(lhs,allocSingle()),rhs,body),body);
    return thDepend(body,env,defs);
  }
  else{
    reportErr(lineInfo(body),"Unrecognised theta component - %w",body);
    return env;
  }
}

static int analyseDef(cellpo body,depStackPo defs,depStackPo groups,
		      depStackPo stack,int *daCount);

static int minPoint(int x,int y)
{
  return (x<=y || y==0) ? x : y;
}

/* Attempt to construct a binding group for a program */
static int markDependency(depPo v,depStackPo defs,depStackPo groups,
			  depStackPo stack,int *daCount)
{
  int low = *daCount;		/* Where in the temp stack are we? */
  int base = *daCount;

  v->marker = (*daCount)++;

  pushDep(stack,v);		/* push entry on the temp stack */

  low = minPoint(low,analyseDef(v->body,defs,groups,stack,daCount));

  if(low==base){		/* We have a new group */
    depPo temp = NULL;
    depPo vv;

    do{
      vv = popDep(stack);	/* drop an entry from the stack */

      assert(vv!=NULL);		/* Verify condition */
      vv->marker = 0;		/* We can group this name now */
      vv->next = temp;
      temp = vv;
    } while(vv!=v);

    pushDep(groups,temp);	/* Add to the groups */
  }
  return low;
}

static logical isLocalVar(cellpo n,lstPo local);
static lstPo findLocalDefs(cellpo b,lstPo local);
static int analyseSequence(cellpo body,depStackPo defs,depStackPo groups,
			   depStackPo stack,int *daCount,lstPo local);

/*
 * go through the body of a definition looking for dependencies
 */
static int analyseBody(cellpo body,depStackPo defs,depStackPo groups,
		      depStackPo stack,int *daCount,lstPo local)
{
  if(IsHashStruct(body,knodebug,&body))
    return analyseBody(body,defs,groups,stack,daCount,local);
  else if(IsHashStruct(body,kdebug,&body))
    return analyseBody(body,defs,groups,stack,daCount,local);
  else if(isSymb(body) && !isLocalVar(body,local)){
    depPo v = findDep(body,defs);

    if(v!=NULL){
      if(v->marker==-1)
	return markDependency(v,defs,groups,stack,daCount);
      else
	return v->marker;
    }
    else
      return *daCount;		/* Nothing here */
  }
  else if(isBinaryCall(body,ksemi,NULL,NULL) || isUnaryCall(body,ksemi,NULL)){
    lstPo nlocal = findLocalDefs(body,local);
    int point = analyseSequence(body,defs,groups,stack,daCount,nlocal);    

    popLst(nlocal,local);
    return point;
  }
  else if(IsQuote(body,NULL))
    return *daCount;
  else if(isCons(body)){
    unsigned long ar = consArity(body);
    unsigned long i;
    int low = analyseBody(consFn(body),defs,groups,stack,daCount,local);
    
    for(i=0;i<ar;i++)
      low = minPoint(low,analyseBody(consEl(body,i),defs,groups,stack,daCount,local));

    return low;
  }
  else if(isTpl(body)){
    long ar = tplArity(body);
    long i;
    int low = *daCount;

    for(i=0;i<ar;i++)
      low = minPoint(low,analyseBody(tplArg(body,i),defs,groups,stack,daCount,local));

    return low;
  }
  else if(isNonEmptyList(body)){
    int low = *daCount;
    while(isNonEmptyList(body)){
      low = minPoint(low,analyseBody(listHead(body),defs,groups,stack,daCount,local));
      body = listTail(body);
    }
    return minPoint(low,analyseBody(body,defs,groups,stack,daCount,local));
  }
  return *daCount;		/* Shouldnt ever get here */
}

static int analyseSequence(cellpo body,depStackPo defs,depStackPo groups,
			   depStackPo stack,int *daCount,lstPo local)
{
  cellpo l;
  int low = *daCount;

  while(isBinaryCall(body,ksemi,&l,&body))
    low = minPoint(low,analyseBody(l,defs,groups,stack,daCount,local));

  if(isUnaryCall(body,ksemi,&l))
    return minPoint(low,analyseBody(l,defs,groups,stack,daCount,local));
  else
    return minPoint(low,analyseBody(body,defs,groups,stack,daCount,local));
}

static int analyseDef(cellpo body,depStackPo defs,depStackPo groups,
		      depStackPo stack,int *daCount)
{
  cellpo lhs,rhs,fn;

  if(IsHashStruct(body,knodebug,&body))
    return analyseDef(body,defs,groups,stack,daCount);
  else if(IsHashStruct(body,kdebug,&body))
    return analyseDef(body,defs,groups,stack,daCount);
  else if(isBinaryCall(body,kfn,NULL,&body))
    return analyseBody(body,defs,groups,stack,daCount,NULL);
  else if(isBinaryCall(body,kconstr,NULL,&body))
    return analyseBody(body,defs,groups,stack,daCount,NULL);
  else if(isBinaryCall(body,ktype,NULL,&body))
    return analyseBody(body,defs,groups,stack,daCount,NULL);
  else if(isBinaryCall(body,ktpname,NULL,&body))
    return analyseBody(body,defs,groups,stack,daCount,NULL);
  else if(isBinaryCall(body,kfield,NULL,&body))
    return analyseBody(body,defs,groups,stack,daCount,NULL);
  else if(isBinaryCall(body,kdefn,NULL,&body))
    return analyseBody(body,defs,groups,stack,daCount,NULL);
  else if(IsUnaryStruct(body,&lhs,&rhs) && IsStruct(lhs,&fn,NULL) &&
	  isSymb(fn))
    return analyseBody(rhs,defs,groups,stack,daCount,NULL);
  else if(isBinaryCall(body,kchoice,&lhs,&rhs))
    return minPoint(analyseDef(lhs,defs,groups,stack,daCount),
		    analyseDef(rhs,defs,groups,stack,daCount));
  else
    return -1;
}

static lstPo findLocalDefs(cellpo b,lstPo local)
{
  cellpo l;
  cellpo n,fn;

  while(isBinaryCall(b,ksemi,&l,&b)){

    if(isBinaryCall(l,kdefn,&n,NULL) || isBinaryCall(l,kfield,&n,NULL))
      local = newLst(n,local);
    else if(isBinaryCall(l,kfn,&n,NULL) && IsStruct(n,&fn,NULL) &&
	    !IsSymbol(fn,kguard))
      local = newLst(n,local);
    else if(IsStruct(l,&n,NULL) && IsStruct(n,&fn,NULL) &&
	    isSymb(fn))
      local = newLst(n,local);
  }

  if(isUnaryCall(b,ksemi,&l)){
    if(isBinaryCall(l,kdefn,&n,NULL) || isBinaryCall(l,kfield,&n,NULL))
      return newLst(n,local);
    else if(isBinaryCall(l,kfn,&n,NULL) && IsStruct(n,&fn,NULL) &&
	    !IsSymbol(fn,kguard))
      return newLst(n,local);
    else if(IsStruct(l,&n,NULL) && IsStruct(n,&fn,NULL) &&
	    isSymb(fn))
      return newLst(n,local);
  }
  return local;
}

static logical isLocalVar(cellpo n,lstPo local)
{
  return isOnLst(n,local);
}

static void etaDepend(depStackPo defs,depStackPo groups)
{
  DepStack stack;
  int daCount = 1;
  int i;

  stack.top = 0;
  groups->top = 0;		/* No groups initially */

  for(i=0;i<defs->top;i++){
    depPo v = defs->stack[i];

    if(v->marker==-1)
      markDependency(v,defs,groups,&stack,&daCount);
  }
}

envPo analyseDependencies(cellpo body,envPo env,depStackPo defs,
			  depStackPo groups)
{
  envPo nenv;

  defs->top = 0;
  groups->top = 0;

  if(dependPool==NULL)
    dependPool = newPool(sizeof(DependRec),256);

  nenv = thDepend(body,env,defs); /* Dependency analysis */
  etaDepend(defs,groups);	/* Compute the dependencies */

  return nenv;
}


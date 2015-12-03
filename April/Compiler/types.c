/*
  Type inference functions for April compiler
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
#include "makesig.h"		/* Access signature handling */

static poolPo envpool = NULL;
static poolPo listpool = NULL;

envPo standardTypes = NULL;
logical traceType = False;	/* True if tracing the type checker */

void initChecker(void)
{
  envpool = newPool(sizeof(TypeEnvRec),256);
  listpool = newPool(sizeof(typeListRec),64);

  standardTypes = resetChecker();
}

envPo resetChecker(void)
{
  standardTypes = decExtCons(ctrue,logicalTp,NULL);
  standardTypes = decExtCons(cfalse,logicalTp,standardTypes);
  standardTypes = decExtCons(cnullhandle,handleTp,standardTypes);
  {
    cellpo hdl = allocCons(2);
    cellpo h = allocSingle();
    
    mkSymb(&hdl[1],khdl);
    mkSymb(&hdl[2],ksymbolTp);
    mkSymb(&hdl[3],ksymbolTp);
    
    mkCons(h,hdl);
    
    standardTypes = decExtCons(h,handleTp,standardTypes);
  }
  return standardTypes;
}

/* Environment and type copying management */

logical knownType(cellpo s,envPo env,cellpo tgt)
{
  cellpo type = pickupType(s,env);

  if(type!=NULL){
    freshType(type,env,tgt);
    return True;
  }
  else{
    copyCell(tgt,s);
    return False;
  }
}

logical knownTypeName(cellpo s,envPo env)
{
  return pickupTypeDef(s,env)!=NULL;
}

static logical tpDefFilter(cellpo name,cellpo type,void *cl)
{
  if(isBinaryCall(type,ktype,NULL,NULL))
    return True;
  else if(IsForAll(type,NULL,&type))
    return tpDefFilter(name,type,cl);
  else
    return False;
}

cellpo pickupTypeDef(cellpo s,envPo env)
{
  return getType(s,env,tpDefFilter,NULL);
}


static logical tpNameFilter(cellpo name,cellpo type,void *cl)
{
  if(isBinaryCall(type,ktpname,NULL,NULL))
    return True;
  else if(IsForAll(type,NULL,&type))
    return tpNameFilter(name,type,cl);
  else
    return False;
}

/* Retrieve a type from the dictionary without duplication */
/* Cannot be used to retrieve types themselves */
cellpo pickupTypeRename(cellpo s,envPo env)
{
  return getType(s,env,tpNameFilter,NULL);
}

static logical tpRegFilter(cellpo name,cellpo type,void *cl)
{
  if(isBinaryCall(type,ktpname,NULL,NULL))
    return False;
  else if(isBinaryCall(type,ktype,NULL,NULL))
    return False;
  else if(IsForAll(type,NULL,&type))
    return tpRegFilter(name,type,cl);
  else
    return True;
}

/* Retrieve a type of an identifier from the dictionary */
/* Cannot be used to retrieve types themselves */
cellpo pickupType(cellpo s,envPo env)
{
  return getType(s,env,tpRegFilter,NULL);
}

cellpo getType(cellpo s,envPo env,envFilter fil,void *cl)
{
  cellpo type=NULL;

  if(isSymb(s)){
    symbpo fn = symVal(s);
    
    if(fn==kany && fil(s,s,cl))
      return anyTp;
    else if(fn==knumber && fil(s,s,cl))
      return numberTp;
    else if(fn==ksymbol && fil(s,s,cl))
      return symbolTp;
    else if(fn==kchar && fil(s,s,cl))
      return charTp;
    else if(fn==kstring && fil(s,s,cl))
      return stringTp;
    else if(fn==klogical && fil(s,s,cl))
      return logicalTp;
    else if(fn==khandle && fil(s,s,cl))
      return handleTp;
  }

  while(env!=NULL){
    if(sameCell(s,&env->typevar) && fil(s,env->type,cl)){
      return env->type;
    }
    else
      env = env->tail;
  }

  if(isSymb(s) && 
     (EscapeFun(symVal(s),&type)!=-1 || EscapeProc(symVal(s),&type)!=-1)
     && fil(s,type,cl))
    return type;

  return NULL;
}

/* Handle list of variables etc. */
lstPo newLst(cellpo t,lstPo lst)
{
  lstPo nw = (lstPo)allocPool(listpool);

  nw->type = t;
  nw->tail = lst;
  return nw;
}

void popLst(lstPo top,lstPo bot)
{
  while(top!=NULL && top!=bot){
    lstPo tail = top->tail;
    freePool(listpool,top);
    top = tail;
  }
}
    
lstPo extendNonG(cellpo t,lstPo nonG)
{
  return newLst(t,nonG);
}

logical isOnLst(cellpo t,lstPo lst)
{
  while(lst!=NULL){
    if(sameCell(t,lst->type))
      return True;
    lst = lst->tail;
  }
  return False;
}

cellpo NewTypeVar(cellpo tgt)
{
  cellpo var = allocSingle();
  mktvar(var,var);
  if(tgt!=NULL)
    mktvar(tgt,var);
  return var;
}

/* Return the repeated type in a closure */
cellpo elementType(cellpo type)
{
  cellpo arg;

  type = deRef(type);

  IsQuery(type,&type,NULL);

  if(isListType(type,&arg))
    return arg;
  else if(IsTypeVar(type)){	/* Bind the type var to a list structure */
    cell Tgt;
    cellpo tp = BuildListType(NULL,&Tgt);

    bindType(type,tp);		/* Bind existing type to this one */

    return consEl(tp,0);	/* The element type is also a type variable */
  }
  else
    return voidcell;		/* nothing -- should never happen */
}

logical IsConstructor(cellpo fn,envPo env,cellpo *type)
{
  cellpo tgt,lhs;

  if(isSymb(fn) && (tgt=pickupType(fn,env))!=NULL){
    cellpo tp = *type = freshType(tgt,env,allocSingle());

    return isConstructorType(tp,&lhs,NULL) && isTpl(deRef(lhs));
  }
  else
    return False;
}

logical IsEnumeration(cellpo fn,envPo env,cellpo *type)
{
  cellpo tgt,lhs;

  if(isSymb(fn) && (tgt=pickupType(fn,env))!=NULL){
    cellpo tp = *type = freshType(tgt,env,allocSingle());

    return isConstructorType(tp,&lhs,NULL) && isSymb(deRef(lhs));
  }
  else
    return False;
}

envPo extendRecordEnv(cellpo exp,cellpo type,envPo env)
{
  cellpo lhs,rhs;

  if(isTpl(type=deRef(type))){
    unsigned long arity = tplArity(type);
    unsigned long i;

    env = newEnv(cenvironment,type,env);

    for(i=0;i<arity;i++){
      if(IsQuery(deRef(tplArg(type,i)),&lhs,&rhs))
	env = newEnv(rhs,lhs,env);
    }
    return env;
  }
  else if(IsQuery(type,&lhs,&rhs))
    return newEnv(rhs,lhs,env);
  else if(isCons(type) && isSymb(consFn(type))){
    unsigned long arity = consArity(type);
    unsigned long i;

    env = newEnv(cenvironment,type,env);

    for(i=0;i<arity;i++){
      if(IsQuery(deRef(consEl(type,i)),&lhs,&rhs))
	env = newEnv(rhs,lhs,env);
    }
    return env;
  }
  else{
    if(IsTypeVar(type))
      reportErr(lineInfo(exp),"Cant determine type of %w",exp);
    else
      reportErr(lineInfo(exp),"%4w is not a record but a `%w'",exp,type);
    return env;
  }
}

envPo newEnv(cellpo name,cellpo type,envPo env)
{
  envPo e = (envPo)allocPool(envpool);

  e->typevar = *name;
  e->type = type;
  e->tail = env;
  return e;
}

void rebindEnv(cellpo s,cellpo type,envPo env)
{

  while(env!=NULL){
    if(sameCell(s,&env->typevar)){
      env->type = type;
      return;
    }
    else
      env = env->tail;
  }

  syserr("Attempted to redefine non-existant type");
}

void popEnv(envPo top,envPo last)
{
  while(top!=last){
    envPo tail = top->tail;
    freePool(envpool,top);
    top = tail;
  }
}

#ifdef TYPETRACE
void dumpEnv(envPo env)
{
  while(env!=NULL){
    outMsg(logFile,"`%#2w' : %10.1700t\n",&env->typevar,env->type);
    env = env->tail;
  }
  flushFile(logFile);
}

void dEnv(envPo env,long depth)
{
  while(depth>0 && env!=NULL){
    outMsg(logFile,"`%#2w' : %#10.1700t\n",&env->typevar,env->type);
    env = env->tail;
    depth--;
  }
  flushFile(logFile);
}

void showEnv(envPo env,char *name)
{
  while(env!=NULL){
    if(uniIsLit(symVal(&env->typevar),name)){
      outMsg(logFile,"`%#2w' : %#20t\n",&env->typevar,env->type);
      flushFile(logFile);
    }
    env = env->tail;
  }
  outMsg(logFile,"no more definitions of %s found\n",name);
  flushFile(logFile);
}

void dumpList(lstPo lst)
{
  while(lst!=NULL){
    outMsg(logFile,"%5w\n",lst->type);
    lst = lst->tail;
  }
  flushFile(logFile);
}
#endif

cellpo argType(cellpo type)
{
  cellpo lhs;

  assert(type!=NULL);

  while(IsForAll(type,NULL,&type))
    ;

  if(isFunType(type=deRef(type),&lhs,NULL))
    return lhs;
  else if(isProcType(type,NULL))
    return tupleOfArgs(type,allocSingle());
  else
    syserr("invalid call to argType");
  return NULL;
}

cellpo resType(cellpo type)
{
  cellpo rhs;

  assert(type!=NULL);

  while(IsForAll(type,NULL,&type))
    ;

  if(isFunType(type=deRef(type),NULL,&rhs))
    return rhs;
  else
    syserr("invalid call to resType");
  return NULL;
}

unsigned long arityOfType(cellpo type)
{
  cellpo lhs;
  unsigned long ar;
  
  if(isFunType(type=deRef(type),&lhs,NULL))
    return argArity(lhs);
  else if(isProcType(type,&ar))
    return ar;
  else
    return argArity(type);
}

logical isListType(cellpo type,cellpo *el)
{
   while(IsForAll(type=deRef(type),NULL,&type))
    ;

  if(isUnaryCall(type,klistTp,el))
    return True;
  else
    return False;
}
 
logical isFunType(cellpo type,cellpo *at,cellpo *rt)
{
  assert(type!=NULL);

  while(IsForAll(type=deRef(type),NULL,&type))
    ;

  if(isBinaryCall(type,kfunTp,at,rt)||isBinaryCall(type,kfn,at,rt))
    return True;
  else
    return False;
}

logical isProcType(cellpo type,unsigned long *ar)
{
  assert(type!=NULL);

  while(IsForAll(type=deRef(type),NULL,&type))
    ;

  return isCall(type,kprocTp,ar)||isCall(type,kblock,ar);
}

logical isConstructorType(cellpo type,cellpo *at,cellpo *rt)
{
  assert(type!=NULL);

  while(IsForAll(type=deRef(type),NULL,&type))
    ;

  return isBinaryCall(type,kconstr,at,rt);
}


static inline integer hashOne(int64 l,int64 r)
{
  integer A = l*37+r;
  if(A<0)
    return -A;
  else
    return A;
}

static integer hashName(uniChar *s)
{
  integer hash = 0;

  while(*s!='\0')
    hash = hashOne(hash,*s++);
  return hash;
}

static integer hashASCII(char *s)
{
  integer hash = 0;

  while(*s!='\0')
    hash = hashOne(hash,*s++);
  return hash;
}
static cellpo vars[MAXSTR];
static long vtop = 0;

integer tpHash(cellpo type)
{
  cellpo lhs,rhs;

  if(IsHashStruct(type,knodebug,&type)) /* Strip off the debugging stuff */
    return tpHash(type);
  else if(IsHashStruct(type,kdebug,&type))
    return tpHash(type);

  else if(isSymb(type))		        /* We have a symbol ... */
    return hashName(symVal(type));

  else if(isListType(type,&lhs) || isUnaryCall(type,kbkets,&lhs))
    return hashOne(tpHash(lhs),hashASCII("[]"));

  else if(IsTvar(type,&rhs)){
    int i=0;
    for(i=0;i<vtop;i++)
      if(sameCell(vars[i],rhs))
	return i+1;
    vars[vtop++]=rhs;
    return vtop;
  }

  else if(isUnaryCall(type,ktypeof,&type))
    return 0;			/* not legal, but let it pass */

  else if(isUnaryCall(type,ktof,&type))
    return 0;			/* not legal, but let it pass */

  else if(isBinaryCall(type,kquery,&lhs,&rhs))
    return hashOne(hashOne(tpHash(lhs),tpHash(rhs)),hashASCII("?"));

  else if(IsTuple(type,NULL)){	/* Regular tuple */
    long ar = tplArity(type);
    long i;
    int64 h = 0;

    for(i=0;i<ar;i++)
      h = hashOne(h,tpHash(tplArg(type,i)));

    return h;
  }

  else if(IsTypeVar(type)){
    int i=0;
    for(i=0;i<vtop;i++)
      if(sameCell(vars[i],type))
	return i+1;
    vars[vtop++]=type;
    return vtop;
  }

  else if(isBinaryCall(type,kchoice,&lhs,&rhs))
    return hashOne(hashOne(tpHash(lhs),tpHash(rhs)),hashASCII("|"));

  else if(isFunType(type,&lhs,&rhs)||isBinaryCall(type,kfn,&lhs,&rhs))
    return hashOne(hashOne(tpHash(lhs),tpHash(rhs)),hashASCII("=>"));
    
  else if(isProcType(type,NULL)){
    unsigned long arity = consArity(type);
    unsigned long i;
    int64 h = 0;

    for(i=0;i<arity;i++)
      h = hashOne(h,tpHash(tplArg(type,i)));

    return hashOne(h,hashASCII("{}"));
  }

  else if(IsForAll(type,&lhs,&rhs))
    return hashOne(hashOne(tpHash(lhs),tpHash(rhs)),hashASCII("-"));

  /* A user defined type ... ? */
  else if(isCons(type) && isSymb(consFn(type))){
    unsigned long ar = consArity(type);
    unsigned long i;
    int64 h = hashOne(tpHash(consFn(type)),hashASCII("@"));

    for(i=0;i<ar;i++)
      h = hashOne(h,tpHash(consEl(type,i)));

    return h;
  }

  else
    return 0;
}

symbpo typeHash(cellpo name,cellpo type)
{
  uniChar sym[1024];

  vtop = 0;

  strMsg(sym,NumberOf(sym),"#%U#%ld",symVal(name),tpHash(type));
  return locateU(sym);
}

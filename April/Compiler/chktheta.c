/*
  New type checker for April compiler
  Implements new typechecking algorithm -- based on pattern unification
  Type check theta expressions and handle dependencies
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
#include "types.h"		/* Type checking interface */
#include "typesP.h"		/* Private data structures for type checking */
#include "keywords.h"		/* Standard April keywords */

/* Compute the type of a theta expression's body */

/* A theta record is equivalent to the env part of a letrec expression
 */

/* Second pass compute the types of all the declared programs */
static void etaType(cellpo body,envPo env)
{
  cellpo lhs,rhs,fn;

  if(IsHashStruct(body,knodebug,&body))
    etaType(body,env);
  else if(IsHashStruct(body,kdebug,&body))
    etaType(body,env);
  else if(isBinaryCall(body,ksemi,&lhs,&rhs)){ /* shouldn't happen */
    etaType(lhs,env);
    etaType(rhs,env);
  }
  else if(isUnaryCall(body,ksemi,&lhs))
    etaType(lhs,env);

  else if(isBinaryCall(body,ktype,&lhs,&rhs)){
    if(isSymb(lhs)){		/* simple user type */
      cellpo form = allocSingle();
      cellpo tp = pickupTypeDef(lhs,env);
      cellpo f1,f2;

      mkSymb(form,typeHash(lhs,rhs));
#ifdef TYPETRACE
      if(traceType)
	outMsg(logFile,"Type hash of %w is %w\n",lhs,form);
#endif

      BuildBinCall(ktype,cblock,form,form); /* {}::=type#hash */

      if(!checkType(tp,form,&f1,&f2))
	reportErr(lineInfo(body),"unable to reconcile type definition %w"
		  "with other uses",tp);
    }
    else if(isCons(lhs)){               // we have name(a1,..,ak) ::= type
      unsigned long i,ar = consArity(lhs);
      cellpo form=allocSingle();
      cellpo args = tupleOfArgs(lhs,allocSingle());
      cellpo tp = pickupTypeDef(fn=consFn(lhs),env);
      cellpo f1,f2;

      mkSymb(form,typeHash(fn,rhs));

#ifdef TYPETRACE
      if(traceType)
	outMsg(logFile,"Type hash of %w is %w\n",lhs,form);
#endif

      BuildStruct(form,consArity(lhs),form);
      for(i=0;i<ar;i++){
        copyCell(consEl(form,i),tplArg(args,i));
      }
      
      BuildBinCall(ktype,args,form,form); /* (%t,...,%t)::=type#hash(%t,...,%t) */
      
      realType(form,env,form);

      if(!checkType(tp,form,&f1,&f2))
	reportErr(lineInfo(body),"unable to reconcile type definition %w"
		  "with other uses",tp);
    }
    else
      reportErr(lineInfo(body),"invalid lhs of type declaration %w\n",body);
  }

  else if(isBinaryCall(body,kconstr,&lhs,&rhs)){
    cellpo fn = isSymb(lhs)?lhs:consFn(lhs);
    cellpo args = isSymb(lhs)?lhs:tupleOfArgs(lhs,allocSingle());
    cellpo ftype = pickupType(fn,env); /* pick up the computed type */
    cellpo rtype = allocSingle();
    cellpo f1,f2;

    realType(BuildBinCall(kconstr,args,rhs,rtype),env,rtype);

    if(!checkType(ftype,rtype,&f1,&f2))
      reportErr(lineInfo(body),"conflict between computed type of %w %W`%t' "
		"and inferred type %W`%t'",fn,f2,rtype,f1,ftype);
  }

  else if(isBinaryCall(body,ktpname,&lhs,&rhs)){
    if(isSymb(lhs)){
      cellpo ftype = pickupTypeRename(lhs,env); /* pick up the existing type */
      cellpo rtype = allocSingle();
      cellpo f1,f2;

      assert(ftype!=NULL);

      realType(rhs,env,rtype);
      BuildBinCall(ktpname,cblock,rtype,rtype);

      if(!checkType(ftype,rtype,&f1,&f2))
	reportErr(lineInfo(body),"conflict between computed type of %w %W`%t' "
		  "and inferred type %W`%t'",fn,f2,rtype,f1,ftype);
    }
    else if(isCons(lhs)){
      cellpo ftype = pickupTypeRename(fn,env); /* pick up the computed type */
      cellpo rtype = allocSingle();
      cellpo f1,f2;

      assert(ftype!=NULL);

      BuildBinCall(ktpname,tupleOfArgs(lhs,allocSingle()),rhs,rtype);
      realType(rtype,env,rtype);

      if(!checkType(ftype,rtype,&f1,&f2))
	reportErr(lineInfo(body),"conflict between computed type of %w %W`%t' "
		  "and inferred type %W`%t'",consFn(lhs),f2,rtype,f1,ftype);
    }
  }

  else if(isBinaryCall(body,kdefn,&fn,&rhs) &&
	  (isSymb(fn)||IsQuery(fn,NULL,&fn))){
    cellpo ptype = pickupType(fn,env); /* pick up the computed type */
    cellpo type = whichType(rhs,&env,env,allocSingle(),True);
    cellpo f1,f2;

    if(!checkType(ptype,type,&f1,&f2))
      reportErr(lineInfo(rhs),"conflict between computed type of %w `%W%t'\n"
		"and inferred type `%W%t'",fn,f2,type,f1,ptype);

    setTypeInfo(rhs,ptype);

  }

  else if(isBinaryCall(body,kfield,&lhs,&rhs) &&
	  (isSymb(lhs)||IsQuery(lhs,NULL,&lhs))){
    cellpo ftype = pickupType(lhs,env); /* pick up type */
    cellpo rtype = whichType(rhs,&env,env,allocSingle(),True);
    cellpo f1,f2;

    if(!checkType(rtype,ftype,&f1,&f2))
      reportErr(lineInfo(body),"conflict between computed type of %w %W`%t'\n"
		"and inferred type `%W%t'",fn,rtype,f1,ftype,f2);

    setTypeInfo(rhs,ftype);

  }
  /* map fn(Ptn) => Exp to fn={Ptn=>Exp} */
  else if(isBinaryCall(body,kfn,&lhs,&rhs) && isCons(lhs) &&
	  !IsSymbol(consFn(lhs),kguard)){
    cellpo fn = consFn(lhs);
    cellpo args = tupleOfArgs(lhs,allocSingle());
    BuildBinCall(kdefn,fn,BuildBinCall(kfn,args,rhs,body),body);
    etaType(body,env);
  }

  /* map fn(Ptn){S} to fn={Ptn->Exp} */
  /* look for procedure style */
  else if(IsUnaryStruct(body,&lhs,&rhs) && isCons(lhs) && isSymb(fn=consFn(lhs))){
    cellpo args = tupleOfArgs(lhs,allocSingle());
    BuildBinCall(kdefn,fn,BuildBinCall(kstmt,args,rhs,body),body);
    etaType(body,env);
  }
  else
    reportErr(lineInfo(body),"unknown component %w in theta",body);
}


/* Process the functions and procedures declared in a theta */
static void generalizeTheta(cellpo body,envPo env,envPo outer)
{
  cellpo lhs,rhs,fn;

  if(IsHashStruct(body,knodebug,&body))
    generalizeTheta(body,env,outer);
  else if(IsHashStruct(body,kdebug,&body))
    generalizeTheta(body,env,outer);
  else if(isBinaryCall(body,ksemi,&lhs,&rhs)){ /* shouldn't happen */
    generalizeTheta(lhs,env,outer);
    generalizeTheta(rhs,env,outer);
  }
  else if(isUnaryCall(body,ksemi,&lhs))
    generalizeTheta(lhs,env,outer);

  else if(isBinaryCall(body,ktype,&lhs,&rhs)){
    cellpo fn = isSymb(lhs)?lhs:consFn(lhs);
    cellpo type = pickupTypeDef(fn,env);
    generalize(type,outer,type);
  }
  else if(isBinaryCall(body,kconstr,&lhs,&rhs)){
    cellpo fn = isSymb(lhs)?lhs:consFn(lhs);
    cellpo type = pickupType(fn,env);
    generalize(type,outer,type);
  }

  else if(isBinaryCall(body,ktpname,&lhs,&rhs)){
    cellpo fn = isSymb(lhs)?lhs:consFn(lhs);
    cellpo type = pickupTypeRename(fn,env);
    generalize(type,outer,type);
  }

  else if(isBinaryCall(body,kdefn,&fn,&rhs) &&
	  (isSymb(fn)||IsQuery(fn,NULL,&fn))){
    cellpo ptype = pickupType(fn,env); /* pick up the computed type */

    generalize(ptype,outer,ptype);

#ifdef TYPETRACE
    if(traceType)
      outMsg(logFile,"Type of %U is %t\n",symVal(fn),ptype);
#endif
  }

  else if(isBinaryCall(body,kfield,&lhs,&rhs) &&
	  (isSymb(lhs)||IsQuery(lhs,NULL,&lhs))){
    cellpo ftype = pickupType(lhs,env); /* pick up type */

    generalize(ftype,outer,ftype);

#ifdef TYPETRACE
    if(traceType)
      outMsg(logFile,"Type of variable %U is %5t\n",symVal(lhs),ftype);
#endif
  }
  else if(isBinaryCall(body,kfn,&lhs,&rhs) && IsStruct(lhs,&fn,NULL) &&
	  !IsSymbol(fn,kguard)){
    cellpo ftype = pickupType(fn,env); /* pick up the computed type */
    generalize(ftype,outer,ftype);

#ifdef TYPETRACE
    if(traceType)
      outMsg(logFile,"Type of %U is %t\n",symVal(fn),ftype);
#endif
  }

  /* look for procedure style */
  else if(IsUnaryStruct(body,&lhs,&rhs) && IsStruct(lhs,&fn,NULL) &&
	  isSymb(fn)){
    cellpo ftype = pickupType(fn,env); /* pick up the computed type */
    generalize(ftype,outer,ftype);

#ifdef TYPETRACE
    if(traceType)
      outMsg(logFile,"Type of %U is %t\n",symVal(fn),ftype);
#endif
  }

  else
    reportErr(lineInfo(body),"unknown component %w in theta",body);
}

/* Construct the real type of the theta expression */
static long buildThType(cellpo body,envPo env)
{
  cellpo lhs,rhs,fn;

  if(IsHashStruct(body,knodebug,&body))
    return buildThType(body,env);
  else if(IsHashStruct(body,kdebug,&body))
    return buildThType(body,env);
  else if(isBinaryCall(body,ksemi,&lhs,&rhs))
    return buildThType(lhs,env)+buildThType(rhs,env);
  else if(isUnaryCall(body,ksemi,&body))
    return buildThType(body,env);
  else if(isBinaryCall(body,ktype,&lhs,&rhs)){
    return 0;
  }
  else if(isBinaryCall(body,ktpname,NULL,NULL))
    return 0;
  else if(isBinaryCall(body,kconstr,NULL,NULL))
    return 0;
  else if(isBinaryCall(body,kfield,&lhs,&rhs) && 
	  (isSymb(lhs)||IsQuery(lhs,NULL,&lhs))){
    p_cell(pickupType(lhs,env));
    p_cell(lhs);
    pBinary(cquery);
    return 1;
  }
  else if(isBinaryCall(body,kdefn,&lhs,&rhs) && 
	  (isSymb(lhs)||IsQuery(lhs,NULL,&lhs))){
    p_cell(pickupType(lhs,env));
    p_cell(lhs);
    pBinary(cquery);
    return 1;
  }
  else if(isBinaryCall(body,kfn,&lhs,&rhs) && IsStruct(lhs,&fn,NULL) &&
	  !IsSymbol(fn,kguard)){
    p_cell(pickupType(fn,env));
    p_cell(fn);
    pBinary(cquery);
    return 1;
  }
  else if(IsUnaryStruct(body,&lhs,&rhs) && IsStruct(lhs,&fn,NULL) &&
	  isSymb(fn)){
    p_cell(pickupType(fn,env));
    p_cell(fn);
    pBinary(cquery);
    return 1;
  }
  else{
    p_sym(kvoid);
    return 1;
  }
} 

static void buildThetaType(cellpo body,envPo env,cellpo tgt)
{
  long count = buildThType(body,env);

  if(count!=1){
    cellpo tp = BuildTuple(count,tgt);
    long i=count-1;

    for(;i>=0;i--)
      popcell(tplArg(tp,i));
  }
  else
    popcell(tgt);
}


static envPo collectRW(cellpo body,envPo env,envPo nenv)
{
  cellpo lhs,rhs;

  if(IsHashStruct(body,knodebug,&body))
    return collectRW(body,env,nenv);
  else if(IsHashStruct(body,kdebug,&body))
    return collectRW(body,env,nenv);
  else if(isBinaryCall(body,ksemi,&lhs,&rhs))
    return collectRW(rhs,collectRW(lhs,env,nenv),nenv);
  else if(isUnaryCall(body,ksemi,&body))
    return collectRW(body,env,nenv);
  else if(isBinaryCall(body,kfield,&lhs,&rhs)){
    IsQuery(lhs,NULL,&lhs);
    return newEnv(lhs,pickupType(lhs,nenv),env);
  }
  else
    return env;
} 

static cellpo bldCons(cellpo type,cellpo choice)
{
  cellpo lhs,rhs;
  if(isBinaryCall(choice,kchoice,&lhs,&rhs))
    return BuildBinCall(ksemi,bldCons(type,lhs),bldCons(type,rhs),
			  allocSingle());
  else
    return BuildBinCall(kconstr,choice,type,allocSingle());
}

static logical typeIn(cellpo th,cellpo type)
{
  cellpo lhs,rhs;

  if(IsHashStruct(th,knodebug,&th))
    return typeIn(th,type);
  else if(IsHashStruct(th,kdebug,&th))
    return typeIn(th,type);
  else if(isBinaryCall(th,ksemi,&lhs,&rhs))
    return typeIn(lhs,type) || typeIn(rhs,type);
  else if(isUnaryCall(th,ksemi,&th))
    return typeIn(th,type);
  else if(isBinaryCall(th,ktype,&lhs,&rhs)){
    if(sameCell(lhs,type))
      return True;
    else if(IsStruct(lhs,&lhs,NULL) && sameCell(lhs,type))
      return True;
    else
      return False;
  }
  else
    return False;
}

static cellpo bldConFuns(cellpo body,cellpo th,envPo env)
{
  cellpo lhs,rhs;

  if(IsHashStruct(body,knodebug,&body))
    return bldConFuns(body,th,env);
  else if(IsHashStruct(body,kdebug,&body))
    return bldConFuns(body,th,env);
  else if(isBinaryCall(body,ksemi,&lhs,&rhs))
    return BuildBinCall(ksemi,bldConFuns(lhs,th,env),
			  bldConFuns(rhs,th,env),allocSingle());
  else if(isUnaryCall(body,ksemi,&body))
    return bldConFuns(body,th,env);
  else if(isBinaryCall(body,ktype,&lhs,&rhs)){
    cellpo lbl,Tp;
    cellpo tpname;

    if(isSymb(lhs))
      lbl = lhs;
    else if(IsStruct(lhs,&lbl,NULL) && isSymb(lbl))
      ;
    else{
      reportErr(lineInfo(lhs),"Type lhs %w of type definition invalid",lhs);
      return body;		/* we cant deal with this here */
    }

    /* We discriminate type renaming from new type definitions here */

    if(isSymb(rhs))
      tpname = rhs;
    else if(isUnaryCall(rhs,kbkets,NULL))
      tpname = cblock;
    else if(isTpl(rhs))
      tpname = cblock;
    else if(isCall(rhs,kblock,NULL))
      tpname = cblock;
    else if(isBinaryCall(rhs,kfn,NULL,NULL))
      tpname = cblock;
    else if(IsStruct(rhs,&Tp,NULL) && isSymb(Tp))
      tpname = Tp;
    else
      return body;

    if(symVal(tpname)!=kchoice && (tpname==cblock ||
				   (!sameCell(tpname,lbl)
				    && typeIn(th,tpname)) ||
				   pickupTypeDef(tpname,env)!=NULL))
      return BuildBinCall(ktpname,lhs,rhs,body);
    else
      return BuildBinCall(ksemi,body,bldCons(lhs,rhs),body);
  }
  else
    return body;
}  

static cellpo reshapeBody(cellpo body)
{
  cellpo lhs,rhs,fn;

  if(IsHashStruct(body,knodebug,&lhs))
    return BuildHashStruct(knodebug,reshapeBody(lhs),body);
  else if(IsHashStruct(body,kdebug,&body))
    return BuildHashStruct(kdebug,reshapeBody(lhs),body);
  else if(isBinaryCall(body,ksemi,&lhs,&rhs))
    return BuildBinCall(ksemi,reshapeBody(lhs),reshapeBody(rhs),body);
  else if(isUnaryCall(body,ksemi,&lhs))
    return BuildUnaryCall(ksemi,reshapeBody(lhs),body);
  else if(isBinaryCall(body,ktype,NULL,NULL))
    return body;
  else if(isBinaryCall(body,kconstr,NULL,NULL))
    return body;
  else if(isBinaryCall(body,ktpname,NULL,NULL))
    return body;
  else if(isBinaryCall(body,kfield,NULL,NULL))
    return body;
  else if(isBinaryCall(body,kdefn,NULL,NULL))
    return body;
  else if(isBinaryCall(body,kfn,&lhs,&rhs) && IsStruct(lhs,&fn,NULL) &&
	  !IsSymbol(fn,kguard)){
    BuildBinCall(kdefn,fn,BuildBinCall(kfn,tupleOfArgs(lhs,allocSingle()),rhs,body),body);
    return body;
  }
  else if(IsUnaryStruct(body,&lhs,&rhs) && IsStruct(lhs,&fn,NULL) &&
	  isSymb(fn)){
    BuildBinCall(kdefn,fn,BuildBinCall(kstmt,tupleOfArgs(lhs,allocSingle()),rhs,body),body);
    return body;
  }
  else
    return body;
}

cellpo thetaType(cellpo body,envPo env,cellpo tgt,logical inH)
{
  DepStack defs;
  DepStack groups;
  cellpo nbody = reshapeBody(body);
  envPo nenv = analyseDependencies(bldConFuns(nbody,nbody,env),
				   env,&defs,&groups);
  envPo oenv = collectRW(body,env,nenv);
  long i;

  for(i=0;i<groups.top;i++){
    depPo group = groups.stack[i];

    while(group!=NULL){
      etaType(group->body,nenv); /* Type check group*/
      group = group->next;
    }
    group = groups.stack[i];	/* construct the generalizations */

    while(group!=NULL){
      generalizeTheta(group->body,nenv,oenv);
      group = group->next;
    }
  }

  buildThetaType(body,nenv,tgt); /* Construct the theta type */

  popEnv(nenv,env);		/* release the environment */

  for(i=0;i<groups.top;i++)
    clearDeps(groups.stack[i]); /* release the dependency tree */

  return typeIs(body,tgt);
}


cellpo typeSkel(cellpo name,envPo *env)
{
  cellpo tp = pickupTypeDef(name,*env);

  if(tp==NULL){
    cellpo type = NewTypeVar(NULL);

    *env = newEnv(name,type,*env);
    return type;
  }

  return deRef(tp);
}

cellpo typeFunSkel(cellpo name,envPo *env)
{
  cellpo T=allocSingle();
  cellpo tp = pickupTypeDef(name,*env);

  if(tp==NULL){
    tp=BuildBinCall(ktype,voidcell,voidcell,T);

    NewTypeVar(callArg(tp,0));
    NewTypeVar(callArg(tp,1));

    *env = newEnv(name,tp,*env);
  }

  return deRef(tp);
}

envPo decExtCons(cellpo def,cellpo tp,envPo env)
{
  cellpo fn;
  
  if(IsStruct(def,&fn,NULL)){
    cellpo t = tupleOfArgs(def,allocSingle());
    cellpo a = allocSingle();
    
    realType(BuildBinCall(kconstr,t,tp,t),env,a);
        
    generalizeTypeExp(a,NULL,t);
    
    return newEnv(fn,t,env);
  }
  else if(isSymb(def)){
    cellpo at = allocSingle();
    
    realType(BuildBinCall(kconstr,def,tp,at),env,at);
    generalizeTypeExp(at,NULL,at);

    return newEnv(def,at,env);
  }
  else{
    reportErr(lineInfo(def),"Invalid type definition");
    return env;
  }
}

envPo defType(cellpo type,envPo env)
{
  cellpo lhs,rhs;

  if(isBinaryCall(type,ktype,&lhs,&rhs)){
    cellpo tpname;
    logical oldType=False;
    cellpo lbl,fn;

    /* We discriminate type renaming from new type definitions here */

    if(isSymb(rhs))
      tpname = rhs;
    else if(isListType(rhs,NULL))
      oldType = True;
    else if(isUnaryCall(rhs,kbkets,NULL))
      oldType = True;
    else if(isTpl(rhs))
      oldType = True;
    else if(isProcType(rhs,NULL))
      oldType = True;
    else if(isCall(rhs,kblock,NULL))
      oldType = True;
    else if(isFunType(rhs,NULL,NULL))
      oldType = True;
    else if(isBinaryCall(rhs,kfn,NULL,NULL))
      oldType = True;
    else if(IsQuery(rhs,NULL,NULL))
      oldType = True;
    else if(IsStruct(rhs,&tpname,NULL) && isSymb(tpname))
      ;
    else
      oldType = True;

    if(!oldType)
      if(symVal(tpname)!=kchoice && pickupTypeDef(tpname,env)!=NULL)
	oldType = True;		/* it is a redeclaration */

    if(isSymb(lhs))
      lbl = lhs;
    else if(IsStruct(lhs,&lbl,NULL) && isSymb(lbl))
      ;
    else{
      reportErr(lineInfo(lhs),"Type lhs %w of type definition invalid",lhs);
      return env;
    }

    if(oldType){
      if(isSymb(lhs)){
	cellpo def = allocSingle();
	cellpo tp = BuildBinCall(ktpname,cblock,rhs,allocSingle());

	realType(tp,env,def);
	generalizeTypeExp(def,env,def);

	return newEnv(lbl,def,env);
      }
      else if(IsStruct(lhs,&fn,NULL)){
	cellpo def = allocSingle();
	cellpo tp = BuildBinCall(ktpname,tupleOfArgs(lhs,allocSingle()),rhs,allocSingle());

	realType(tp,env,def);
	generalizeTypeExp(def,env,def);

	return newEnv(lbl,def,env);
      }
      else{
	reportErr(lineInfo(type),"lhs of type definition is not good");
	return env;
      }
    }
    else{			/* we have a new type definition here */
      cellpo def = allocSingle();
      
      if(isSymb(lhs)){		/* simple user type */
        cellpo form = allocSingle();
        cellpo tpHash = allocSingle();

        mkSymb(tpHash,typeHash(lhs,rhs));
#ifdef TYPETRACE
        if(traceType)
	  outMsg(logFile,"Type hash of %w is %w\n",lhs,tpHash);
#endif

        BuildBinCall(ktype,cblock,tpHash,form); /* {}::=type#hash */

        realType(form,env,def);
        generalizeTypeExp(def,env,def);
        env=newEnv(lbl,def,env);
       
        while(isBinaryCall(rhs,kchoice,&def,&rhs))
	  env = decExtCons(def,tpHash,env);
        return decExtCons(rhs,tpHash,env);
      }
      else if(isCons(lhs)){               // we have name(a1,..,ak) ::= type
        unsigned long i,ar = consArity(lhs);
        cellpo aa = allocSingle();
        cellpo form=allocSingle();
        cellpo args = tupleOfArgs(lhs,allocSingle());
        cellpo tpHash = allocSingle();

        realType(args,env,aa);

        mkSymb(tpHash,typeHash(consFn(lhs),rhs));

#ifdef TYPETRACE
        if(traceType)
	  outMsg(logFile,"Type hash of %w is %w\n",lhs,tpHash);
#endif

        BuildStruct(tpHash,consArity(lhs),form);
        for(i=0;i<ar;i++)
          copyCell(consEl(form,i),tplArg(aa,i));
      
        BuildBinCall(ktype,aa,form,def); /* (%t,...,%t)::=type#hash(%t,...,%t) */

        generalizeTypeExp(def,env,def);
        
        env=newEnv(lbl,def,env);
        
        while(isBinaryCall(rhs,kchoice,&def,&rhs))
	  env = decExtCons(def,lhs,env);
        return decExtCons(rhs,lhs,env);
      }
      else{
        reportErr(lineInfo(type),"invalid lhs of type declaration %w\n",type);
        return env;
      }
    }
  }
  else
    return env;
}

void generalizeType(cellpo type,envPo env,envPo outer)
{
  cellpo lhs,rhs,fn;

  if(isBinaryCall(type,ktype,&lhs,&rhs) && isSymb(lhs)){
    cellpo type = pickupTypeDef(lhs,env);
    generalizeTypeExp(type,outer,type);
    rebindEnv(lhs,type,env);

#ifdef TYPETRACE
    if(traceType)
      outMsg(logFile,"%w defined type is %w\n",lhs,type);
#endif

  }
  else if(isBinaryCall(type,ktype,&lhs,&rhs) && IsStruct(lhs,&fn,NULL)){
    cellpo type = pickupTypeDef(fn,env);

    generalizeTypeExp(type,outer,type);
    rebindEnv(fn,type,env);

#ifdef TYPETRACE
    if(traceType)
      outMsg(logFile,"%w defined type is %w\n",fn,type);
#endif
  }
}


/*
  Top level of the April compiler proper
  (c) 1994-1999 Imperial College and F.G. McCabe
 
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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "compile.h"
#include "cellP.h"
#include "types.h"		/* Type checking interface */
#include "typesP.h"
#include "generate.h"		/* Code generation interface */
#include "dict.h"
#include "assem.h"
#include "keywords.h"
#include "opcodes.h"
#include "encode.h"

#define MINBOUND 3		/* first bound variable */
#define MINENVVAR 0		/* Where the environment variables start */

/* Used in table of forward references constructed during a compTheta */
typedef struct{
  int Pi;			/* The program making the reference */
  int Pk;			/* Position in the referer's closure*/
  cellpo ref;			/* name of the reference */
} RefRecord;

/* Table structure of a theta environment */
typedef struct {
  int off;			/* Where is the closure to be generated */
  int depth;			/* What is the depth to use at this point? */
  cellpo name;			/* What is the name of the theta element? */
  int dLvl;			/* Are we debugging? */
  cellpo body;			/* The body of the program */
} ThetaItemRec, *thetaPo;

static dictpo FreeVars(cellpo exp,dictpo bound,dictpo free,dictpo outer,int *nfree);
static cellpo freeType(dictpo top,dictpo bottom,cellpo tgt);
static codepo compFunction(cellpo name,cellpo fun,dictpo dict,
			   dictpo *fdict,int dLvl);
static long thetaLength(cellpo input);
static logical singleArgClauses(cellpo clses);
static void writeAfFile(cellpo exp,uniChar *codeName);

static logical verbose;

/* A program should be a function */
void compileProg(cellpo prog,dictpo dict,ioPo outFile,optPo info)
{
  cellpo args,exp;
  cellpo type = typeInfo(prog);
  int dLvl = info->dLvl;

  verbose = info->verbose;

  if(type==NULL)
    reportErr(lineInfo(prog),"Top-level should be a function `%5w'",prog);
  else if(isBinaryCall(prog,kfn,&args,&exp)){
    dictpo free;
    codepo code = compFunction(ctoplevel,prog,dict,&free,dLvl);
    cellpo outer=allocSingle();	/* code() */

    /* construct the closure */
    mkCons(outer,allocCons(0));

    mkCode(consFn(outer),code);

    /* Dump the binary out to the file */
    if(!comperrflag){
      encodeICM(outFile,outer);
      if(info->makeAf)
	writeAfFile(exp,FileTail(fileName(outFile)));
    }
    clearDict(free,NULL);
  }    
  else
    reportErr(lineInfo(prog),"Top-level should be a function `%5.1800w'",prog);
}

/* Generate a new dictionary of bound variables for a function or procedure */
dictpo BoundVars(cellpo args,int srcvar,int ar,dictpo dict,
		 int depth,cpo code,int dLvl)
{
  if(isBinaryCall(stripCell(args),kguard,&args,NULL))
    return BoundVars(args,srcvar,ar,dict,depth,code,dLvl);
  else if(isTpl(args)){
    int i;
    long arity=tplArity(args);

    for(i=0;i<arity;i++){	/* declare bound variables */
      cellpo vr = tplArg(args,i);
      cellpo tp;

      isBinaryCall(vr,kguard,&vr,NULL); /* pattern might be guarded */

      tp = typeInfo(vr);

      assert(tp!=NULL);

      if(isBinaryCall(vr,kquery,NULL,&vr)||isUnaryCall(vr,kquery,&vr)){
	dict=declVar(vr,tp,localvar,singleAss,Inited,False,srcvar+arity-i-1,-1,dict);
      }
      else if(isSymb(vr) && !IsSymbol(vr,kuscore) && 
	      !isConstructorType(typeInfo(vr),NULL,NULL)){
	if(!IsLocalVar(vr,dict)){
	  dict=declVar(vr,tp,localvar,singleAss,Inited,False,srcvar+arity-i-1,-1,dict);
	}
	else
	  reportErr(lineInfo(vr),"duplicate formal parameter %w",vr);
      }
    }
    return dict;		/* a new dictionary */
  }
  else{
    cellpo vr = args;
    cellpo tp = typeInfo(args);

    assert(tp!=NULL);

    if(IsQuery(vr,NULL,&vr) || isUnaryCall(vr,kquery,&vr)){
      dict=declVar(vr,tp,localvar,singleAss,Inited,True,srcvar,-1,dict);

      if(dLvl>0)
	genVarDebug(code,depth,vr,tp,srcvar,dict,dict);
    } 
    else if(isSymb(vr)&&!IsSymbol(vr,kuscore) && 
	    !isConstructorType(typeInfo(vr),NULL,NULL)){
      if(!IsLocalVar(vr,dict) &&
	 !isConstructorType(typeInfo(vr),NULL,NULL)){
	dict=declVar(vr,tp,localvar,singleAss,Inited,True,srcvar,-1,dict);

	if(dLvl>0)
	  genVarDebug(code,depth,vr,tp,srcvar,dict,dict);
      }
      else
	reportErr(lineInfo(vr),"duplicate formal parameter %w",vr);
    }

    return dict;
  }
}

/* Find out all the free variables in an expression */
/* This algorithm is a bit simple minded -- it looks for occs in outer 
   which are not in bound */
static dictpo FreeVars(cellpo exp,dictpo bound,dictpo free,dictpo outer,int *nfree)
{
  vartype vt;

  if(isSymb(exp=stripCell(exp))){
    cellpo type;
    assMode rw;
    int index;

    if(IsVar(exp,NULL,&type,NULL,&index,&rw,NULL,&vt,outer) &&
       !IsVar(exp,NULL,NULL,NULL,NULL,NULL,NULL,NULL,bound) &&
       !IsVar(exp,NULL,NULL,NULL,NULL,NULL,NULL,NULL,free)){
      if(rw==multiAss && vt==indexvar){	/* this is a theta variable ... */
	int thetaVar;
	cellpo thetaType;

	if(IsVar(ctclosure,NULL,&thetaType,NULL,NULL,NULL,NULL,NULL,outer) &&
	   !IsVar(ctclosure,NULL,NULL,&thetaVar,NULL,NULL,NULL,NULL,free)){
	  thetaVar = (*nfree)++; /* We declare the theta environment as a whole */
	  free=declVar(ctclosure,thetaType,envvar,singleAss,notInited,True,thetaVar,-1,free);
	}
	free = declVar(exp,type,thetavar,multiAss,Inited,True,thetaVar,index,free);
      }
      else if(rw==multiAss && vt==thetavar){	/* this is a theta variable ... */
	int thetaVar;
	cellpo thetaType;

	if(IsVar(ctclosure,NULL,&thetaType,NULL,NULL,NULL,NULL,NULL,outer) &&
	   !IsVar(ctclosure,NULL,NULL,&thetaVar,NULL,NULL,NULL,NULL,free)){
	  thetaVar = (*nfree)++; /* We declare the theta environment as a whole */
	  free=declVar(ctclosure,thetaType,envvar,singleAss,notInited,True,
		       thetaVar,-1,free);
	}
	free = declVar(exp,type,thetavar,multiAss,Inited,True,thetaVar,index,free);
      }
      else			/* regular free variable */
	free = declVar(exp,type,envvar,rw,Inited,True,(*nfree)++,-1,free);
    }

    return free;
  }
  else if(IsString(exp) || IsInt(exp) || IsFloat(exp) || isChr(exp) || isEmptyList(exp))
    return free;
  else if(isNonEmptyList(exp)){
    exp = listVal(exp);

    if(exp!=NULL){
      free = FreeVars(exp,bound,free,outer,nfree);
      return FreeVars(Next(exp),bound,free,outer,nfree);
    }
    else
      return free;
  }
  else if(isCons(exp)){
    unsigned int ar=consArity(exp);
    unsigned int i;

    for(i=0;i<ar;i++)
      free=FreeVars(consEl(exp,i),bound,free,outer,nfree);
    return FreeVars(consFn(exp),bound,free,outer,nfree);
  }
  else if(isTpl(exp)){
    unsigned int ar=tplArity(exp);
    unsigned int i;

    for(i=0;i<ar;i++)
      free=FreeVars(tplArg(exp,i),bound,free,outer,nfree);
    return free;
  }
  else if(IsTypeVar(exp))
    return free;		/* ignore type variables */
  else
    return free;		/* shouldnt get here */
}

static dictpo FreeInDef(cellpo def,dictpo free,dictpo outer,int *nfree)
{
  cellpo lhs,rhs;

  if(isBinaryCall(def=stripCell(def),kchoice,&lhs,&rhs))
    return FreeInDef(rhs,FreeInDef(lhs,free,outer,nfree),outer,nfree);
  else if(isBinaryCall(def,kfn,&lhs,&rhs) ||
	  isBinaryCall(def,kstmt,&lhs,&rhs)){
    int DP=0;
    dictpo bound = EmbeddedVars(lhs,0,&DP,True,singleAss,False,NULL,NULL,NULL,NULL);

    free = FreeVars(def,bound,FreeVars(cdebugdisplay,bound,free,outer,nfree),outer,nfree);

    clearDict(bound,NULL);	/* this is wasteful ... */
    return free;
  }
  else
    return FreeVars(def,NULL,free,outer,nfree);
}

static void countTp(dictpo var,void *cl)
{
  if(dictVt(var,dictName(var))==envvar){
    long* count = (long*)cl;
    (*count)++;
  }
}

static void freeTp(dictpo var,void *cl)
{
  cellpo tgt = (cellpo)cl;
  cellpo name = dictName(var);

  if(dictVt(var,name)==envvar)
    copyCell(tplArg(tgt,dictOffset(var,name)-MINENVVAR),
		    dictType(var,name)); /* copy out the type data */
}

/* Construct a type vector of the free variables */
static cellpo freeType(dictpo top,dictpo bottom,cellpo tgt)
{
  long count = 0;
  
  processDict(top,bottom,countTp,(void*)&count);
    
  mkTpl(tgt,allocTuple(count));
  
  processDict(top,bottom,freeTp,(void*)tgt);

  return tgt;
}

/* generate references to free variables -- in reverse order to dictionary */
typedef struct _free_info_ {
  long nFree;			/* Count of the free variables */
  cpo code;
  int thisFn;
  int thisEnv;
  RefRecord *refs;
  int refTop;
  int depth;
  dictpo outer;			/* Outer dictionary */
} FreeRec;

static void genFr(dictpo entry,void *cl)
{
  FreeRec *info=(FreeRec*)cl;
  initStat Oinited;
  int Oind,Ooff;
  vartype Ovt;
  cellpo var = dictName(entry);
  vartype vt = dictVt(entry,var);
  int offset = dictOffset(entry,var);
  
  if(IsVar(var,NULL,NULL,&Ooff,&Oind,NULL,&Oinited,&Ovt,info->outer)){
    switch(Ovt){
    case localvar:
      info->nFree++;		/* We have a new free variable */
      if(vt==envvar){
	if(Oinited==notInited){
	  info->refs[info->refTop].Pi = info->thisFn;
	  info->refs[info->refTop].Pk = offset; /* Where the reference should go */
	  info->refs[info->refTop].ref = var;
	  info->refTop++;
	  genIns(info->code,"newvar",initv,--info->depth);
	}
	else
	  genIns(info->code,"old var",move,Ooff,--info->depth);
      }
      else{
	genIns(info->code,"old vr",move,Ooff,--info->depth);
      }
      return;
    case envvar:
      info->nFree++;		/* We have a new free variable */
      genIns(info->code,"env var",emove,Ooff,--info->depth);
      return;
    case indexvar:
      if(vt==envvar){
	info->nFree++;		/* We have a new free variable */
	if(Oinited==notInited){	/* We need a forward ref */
	  info->refs[info->refTop].Pi = info->thisFn;
	  info->refs[info->refTop].Pk = offset; /* Where the reference should go */
	  info->refs[info->refTop].ref = var;
	  info->refTop++;
	  genIns(info->code,"index var",initv,--info->depth);
	}
	else
	  genIns(info->code,NULL,indxfld,Ooff,Oind,--info->depth);
      }
      return;
    case thetavar:
#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"theta var %w\n",var);
#endif
      return;

    default:
      syserr("Problem in genFr");
    }
  }
}

static void genFree(dictpo dict,dictpo ed,dictpo outer,
		    int depth,int *dp,int *free,RefRecord *refs,int *refTop,
		    int thisFn,cpo code)
{
  FreeRec info;
  
  info.nFree = *free;
  info.code = code;
  info.thisFn=thisFn;
  info.refs=refs;
  info.refTop=*refTop;
  info.depth=depth;
  info.outer=outer;

  processRevDict(dict,ed,genFr,&info);
  
  if(dp!=NULL)
    *dp=info.depth;
  *free=info.nFree;
  *refTop=info.refTop;
}
  
/* Construct closure constructing code */
static void genClosure(char *name,int *dest,int depth,int *dp,
		       dictpo dict,dictpo ed,dictpo outer,cellpo tp,
		       codepo cd,cpo code,
		       RefRecord *refs,int *refTop,int thisFn)
{
  int free=MINENVVAR;		/* minimum length of closure tuple */
  cellpo ccd=allocSingle();

  mkCode(ccd,cd);		/* force a code structure */

  genDest(dest,depth,dp,code);	/* compute destination */

  genIns(code,name,movl,ccd,--depth); /* the code itself */

  genFree(dict,ed,outer,depth,&depth,&free,refs,refTop,thisFn,code);

  genIns(code,"closure",loc2cns,depth,free,*dest);
}


/* Compile a single equation ... invoked as part of choice compilation */

typedef struct {
  cellpo fname;
  cellpo rtype;
  int *dest;
} eqnInfo;

static logical compEqn(cellpo input,int arity,int srcvar,
			  int depth,lblpo Next,lblpo LF,dictpo dict,cpo code,
			  logical last,int dLvl,void *cl)
{
  cellpo lhs,rhs;
  eqnInfo *info = (eqnInfo*)cl;

  if(isBinaryCall(input,kfn,&lhs,&rhs)){
    dictpo ndict;
    logical mayfail = False;
    cellpo ptn = lhs;
    cellpo guard = ctrue;
    int i;
    long dL = dLvl;
    logical anyPresent = EmbeddedAny(lhs);
    lblpo Lf = LF;
    int edepth = depth;

    if(IsSymbol(info->fname,ktoplevel))
      dL = 0;

#ifdef COMPTRACE
    if(traceCompile)
      outMsg(logFile,"equation: %w=>%5w\n",lhs,rhs);
#endif

    if(anyPresent){
      Lf = NewLabel(code);
      genIns(code,"prepare for type unification",urst,edepth);
    }

    isBinaryCall(ptn,kguard,&ptn,&guard); /* remove the guard for a while... */

    assert(isTpl(ptn));

    {
      dictpo lo = BoundVars(lhs,srcvar,arity,dict,depth,code,dL);
      ndict = EmbeddedVars(ptn,depth,&depth,False,singleAss,False,lo,lo,dict,code); 

      for(i=0;i<arity;i++){
	cellpo arg = tplArg(ptn,i);
	mayfail |= compPttrn(arg,srcvar-1+arity-i,depth,&depth,Lf,ndict,&ndict,lo,dict,code,
			       singleAss,dL);
      }
    }
    
    compTest(guard,depth,&depth,Lf,ndict,&ndict,dict,code,dL);
    
    
    if(dLvl>0){
      lblpo lX = NewLabel(code);

      genIns(code,"Test for debugging",debug_d,lX);

      genPtnVarDebug(code,depth,ndict,dict,dict);
      genScopeDebug(code,depth,ndict);
      genLineDebugInfo(code,depth,rhs,False);
      DefLabel(code,lX);
    }

    compExp(rhs,info->rtype,info->dest,depth,&depth,ndict,dict,code,dLvl);

    if(!last||anyPresent)
      genIns(code,NULL,jmp,Next); /* we are done with the choice */

    if(anyPresent){
      DefLabel(code,Lf);
      genIns(code,"undo type unification",undo,edepth);
      genIns(code,"real failure",jmp,LF);
    }
    
    clearDict(ndict,dict);
    return mayfail;
  }
  else if(isBinaryCall(input,kchoice,&lhs,&rhs)){
    lblpo L0=NewLabel(code);

    compEqn(lhs,arity,srcvar,depth,Next,L0,dict,code,False,dLvl,cl);

    DefLabel(code,L0);
    return compEqn(rhs,arity,srcvar,depth,Next,L0,dict,code,last,dLvl,cl);
  }
  else{				/* non-standard `equation' */
    int i;

    if(!last){
      int EC = depth-=2;	/* place for the error handler */
      lblpo L0=NewLabel(code);

      genIns(code,NULL,errblk,EC,L0); /* start an error block */

      for(i=1;i<=arity;i++)
	genIns(code,NULL,move,srcvar+arity-i,depth-i);

      funCall(input,arity,depth-arity,dict,code,dLvl);

      genMove(code,depth-1,info->dest,"Collect result of equation");

      genIns(code,NULL,errend,EC); /* end the error block */
      genIns(code,NULL,jmp,Next); /* Exit to next statement */

      DefLabel(code,L0);
      genIns(code,"Pick up error value",moverr,++EC);
      if(dLvl>0)
	genIns(code,"report the error",error_d,depth,EC);

      genIns(code,"Test for failure",mlit,EC,cfailed);
      genIns(code,NULL,generr,EC); /* raise the error again */
      genIns(code,NULL,jmp,LF);	/* normal failure case */
      return True;		/* yes, it may fail... */
    }
    else{
      for(i=0;i<arity;i++)
	genIns(code,NULL,move,srcvar+arity-i,depth-i);

      funCall(input,arity,depth-arity,dict,code,dLvl);

      genMove(code,depth-1,info->dest,"Collect result of equation");
      return False;		/* no, it wont fail */
    }
  }
}

static codepo compFunction(cellpo name,cellpo fun,dictpo dict,dictpo *fdict,
			   int dLvl)
{
  cellpo type = typeInfo(fun);
  int nfree = MINENVVAR;
  dictpo free = FreeInDef(fun,NULL,dict,&nfree);
  dictpo ndict = addMarker(free);
  cpo code = NewCode(symVal(name));	/* start a new code segment */
  int ldest = -1;
  int ldepth = ldest;
  cellpo frTp = freeType(free,NULL,allocSingle());
  lblpo Lx = NewLabel(code);
  lblpo LF = NewLabel(code);
  cellpo atype = deRef(argType(deRef(type)));
  int arity = argArity(deRef(atype));
  int apc;
  eqnInfo info = {name,resType(deRef(type)),&ldest};
  int dbgLvl =  !IsSymbol(name,kdebugdisplay)?dLvl:0;

  if(verbose){
    outMsg(logFile,"Compiling %w\n",name);
    flushFile(logFile);
  }

  apc = genIns(code,NULL,allocv,-1); /* must have an alloc instruction */

  if(dbgLvl>0 && !IsSymbol(name,ktoplevel)){
    lblpo lX = NewLabel(code);

    genIns(code,"Test for debugging",debug_d,lX);

    genEntryDebug(code,0,name); /* entry debugging */
    genScopeDebug(code,0,ndict);
    
    DefLabel(code,lX);
  }

  genIns(code,"Clear result variable",initv,ldest);

  compChoice(fun,arity,MINBOUND,ldepth,Lx,LF,ndict,code,dbgLvl,compEqn,&info);

  if(ldepth>ldest)
    ldepth=ldest;

  DefLabel(code,Lx);

  if(dbgLvl>0 && !IsSymbol(name,ktoplevel)){
    lblpo LE = NewLabel(code);

    genIns(code,"Test for debugging",debug_d,LE);
  
    genRtnDebug(code,ldepth,name,info.rtype,ldest,ndict,ndict);
    genScopeDebug(code,ldepth,NULL);
    genExitDebug(code,ldepth,name); /* exit debugging */
    genLineDebugInfo(code,ldepth,name,False);

    DefLabel(code,LE);
  }

  genIns(code,NULL,result,arity,ldest);

  DefLabel(code,LF);

  genError(fun,"function failed",cfailed,ldepth,dict,code,dbgLvl);

  if(MaxDepth(code)<-MAXENV)	/* too many local variables */
    reportErr(lineInfo(fun),"function %w too complicated or too many variables",fun);
  else
    /* update the allocv instruction */
    upDateIns(code,apc,allocv,MaxDepth(code));

  if(fdict!=NULL){
    clearDict(ndict,free);
    *fdict = free;
  }
  else
    clearDict(ndict,NULL);

  return CloseCode(code,type,frTp,function);
}

void compLambda(cellpo fun,cellpo type,int *dest,int depth,int *dp,cpo code,dictpo dict,int dLvl)
{
  dictpo free;
  codepo sub = compFunction(cequation,fun,dict,&free,dLvl);
  int Top = 0;

  /* construct the closure */
  genClosure("lambda",dest,depth,dp,free,NULL,dict,cclosure,sub,code,NULL,&Top,-1);
  clearDict(free,NULL);
}

/* 
 * Generate a procedure ...
 */

/* Compile a single clause ... invoked as part of the choice compilation */

static logical compClause(cellpo input,int arity,int srcvar,
			  int depth,lblpo Next,lblpo LF,dictpo dict,cpo code,
			  logical last,int dLvl,void *cl)
{
  cellpo lhs,rhs;

  if(isBinaryCall(input,kstmt,&lhs,&rhs)){
    dictpo ndict;
    logical mayfail = False;
    context ctxt;		/* initial program context */
    cellpo guard = ctrue;
    cellpo ptn = lhs;
    cellpo t;
    int i;
    cont contin;
    int edepth = depth;
    logical anyPresent = EmbeddedAny(lhs);
    lblpo Lf = LF;

#ifdef COMPTRACE
    if(traceCompile)
      outMsg(logFile,"clause: %w->%w\n",lhs,rhs);
#endif

    ctxt.ct=none;		/* A procedure context */
    ctxt.brk=Next;		/* where to go on success */
    ctxt.valCont=NULL;
    ctxt.prev=NULL;

    if(anyPresent){
      Lf = NewLabel(code);
      genIns(code,"prepare for type unification",urst,edepth);
    }
    
    isBinaryCall(ptn,kguard,&ptn,&guard); /* remove the guard for a while... */

    assert(isTpl(ptn));

    {
      dictpo lo = BoundVars(lhs,srcvar,arity,dict,depth,code,dLvl);
      ndict = EmbeddedVars(ptn,depth,&depth,False,singleAss,False,lo,lo,dict,code); 

      for(i=0;i<arity;i++){
	cellpo arg = tplArg(ptn,i);
	mayfail |= compPttrn(arg,srcvar-1+arity-i,
			       depth,&depth,Lf,ndict,&ndict,lo,dict,
			       code,singleAss,dLvl);
      }
    }
    
    compTest(guard,depth,&depth,Lf,ndict,&ndict,dict,code,dLvl);
    
    if(!IsSymbol(guard,ktrue) && !(IsEnumStruct(guard,&t)&&IsSymbol(t,ktrue)))
      mayfail = True;

    if(dLvl>0)
      genPtnVarDebug(code,depth,ndict,dict,dict);

    if(dLvl>0){
      lblpo lX = NewLabel(code);

      genIns(code,"Test for debugging",debug_d,lX);
      genScopeDebug(code,depth,ndict);
      genLineDebugInfo(code,depth,rhs,False);
      
      DefLabel(code,lX);
    }
    
    compStmt(rhs,depth,&depth,&ctxt,Next,ndict,&ndict,dict,&contin,code,dLvl);

    if((!last||mayfail||anyPresent) && contin==continued)
      genIns(code,NULL,jmp,Next); /* we are done with the choice */

    if(anyPresent){
      DefLabel(code,Lf);
      genIns(code,"undo type unification",undo,edepth);
      genIns(code,"and fail",jmp,LF);
    }

    clearDict(ndict,dict);
    return mayfail;
  }
  else if(isBinaryCall(input,kchoice,&lhs,&rhs)){
    lblpo LE=NewLabel(code);

    compClause(lhs,arity,srcvar,depth,Next,LE,dict,code,False,dLvl,NULL);

    DefLabel(code,LE);
    return compClause(rhs,arity,srcvar,depth,Next,LF,dict,code,False,dLvl,cl);
  }
  else{
    reportErr(lineInfo(input),"invalid procedure body: %w",input);
    return True;
  }
}

static codepo compProcedure(cellpo name,cellpo proc,cellpo type,dictpo dict,
			    dictpo *fdict,int dLvl)
{
  int nfree = MINENVVAR;
  dictpo free = FreeInDef(proc,NULL,dict,&nfree);
  dictpo ndict = addMarker(free);
  cpo code = NewCode(symVal(name));	/* start a new code segment */
  int ldepth = 0;
  int apc;
  cellpo frTp = freeType(free,NULL,allocSingle());
  lblpo Lx = NewLabel(code);
  lblpo LF = NewLabel(code);
  int arity = argArity(deRef(argType(deRef(type))));

  if(verbose)
    outMsg(logFile,"Compiling %w\n",name);

  apc = genIns(code,NULL,allocv,-1); /* must have an alloc instruction */

#ifdef COMPTRACE
  if(traceCompile){
    outMsg(logFile,"procedure %s\n",symVal(name));
    dumpDict(ndict);
  }
#endif

  if(dLvl>0){
    lblpo lX = NewLabel(code);

    genIns(code,"Test for debugging",debug_d,lX);

    genEntryDebug(code,0,name); /* entry debugging */
    genScopeDebug(code,0,ndict);
    genLineDebugInfo(code,0,name,False);
    DefLabel(code,lX);
  }

  compChoice(proc,arity,MINBOUND,ldepth,Lx,LF,ndict,code,dLvl,compClause,NULL);

  DefLabel(code,Lx);

  if(dLvl>0){
    lblpo lX = NewLabel(code);

    genIns(code,"Test for debugging",debug_d,lX);

    genLineDebugInfo(code,ldepth,name,False);
    genScopeDebug(code,ldepth,NULL);
    genExitDebug(code,ldepth,name); /* exit debugging */
    DefLabel(code,lX);
  }

  genIns(code,symN(symVal(name)),ret); /* exit the procedure */

  DefLabel(code,LF);

  genError(proc,symN(symVal(name)),cfailed,ldepth,ndict,code,dLvl);

  //  genIns(code,"Failure point",movl,cfailed,--ldepth);
  //  genIns(code,NULL,generr,ldepth);

  if(MaxDepth(code)<-MAXENV)	/* too many local variables */
    reportErr(lineInfo(proc),"procedure %w too complicated or too many variables",proc);
  else
    /* update the allocv instruction */
    upDateIns(code,apc,allocv,MaxDepth(code));

  if(fdict!=NULL){
    clearDict(ndict,free);
    *fdict = free;
  }
  else
    clearDict(ndict,NULL);

  return CloseCode(code,type,frTp,procedure);
}

void compMu(cellpo proc,cellpo type,int *dest,int depth,int *dp,cpo code,
	    dictpo dict,int dLvl)
{
  dictpo free;
  codepo sub = compProcedure(cclause,proc,type,dict,&free,dLvl);
  int Top = 0;

  /* construct the closure */
  genClosure("clause",dest,depth,dp,free,NULL,dict,cclosure,sub,code,NULL,&Top,-1);
  clearDict(free,NULL);
}

/* Declare the functions, procedures and patterns in the theta body */
static dictpo declTheta(cellpo body,thetaPo items,int *indx,int var,cpo code,
			int *depth,dictpo dict,int dLvl)
{
  cellpo lhs,rhs;

  if(IsHashStruct(body,kdebug,&body))
    return declTheta(body,items,indx,var,code,depth,dict,dLvl+1);
  else if(IsHashStruct(body,knodebug,&body))
    return declTheta(body,items,indx,var,code,depth,dict,dLvl-1);
  else if(isUnaryCall(body,ksemi,&body))
    return declTheta(body,items,indx,var,code,depth,dict,dLvl);
  else if(isBinaryCall(body,ksemi,&lhs,&rhs))
    return declTheta(rhs,items,indx,var,code,depth,
		     declTheta(lhs,items,indx,var,code,depth,dict,dLvl),dLvl);
  else if(isBinaryCall(body,kfield,&lhs,&rhs)){
    cellpo type=typeInfo(rhs);
    int off = (*indx)++;

    IsQuery(lhs,NULL,&lhs);

    assert(type!=NULL && isSymb(lhs));

    items[off].name = lhs;
    items[off].body = body;
    items[off].off = off;
    items[off].dLvl = dLvl;

    genIns(code,symN(symVal(lhs)),initv,items[off].depth=--(*depth));
    return declVar(lhs,type,indexvar,multiAss,notInited,True,var,off,dict);
  }
  else if(isBinaryCall(body,kdefn,&lhs,&rhs)){
    cellpo type=typeInfo(rhs);
    int off = (*indx)++;

    IsQuery(lhs,NULL,&lhs);

    assert(type!=NULL && isSymb(lhs));

    items[off].name = lhs;
    items[off].body=body;
    items[off].off = off;
    items[off].dLvl = dLvl;

    genIns(code,symN(symVal(lhs)),initv,items[off].depth=--(*depth));
    return declVar(lhs,type,indexvar,singleAss,notInited,True,var,off,dict);
  }
  else
    return dict;		/* ignore other record elements */
}

static void updateRefs(thetaPo th,int thLen,RefRecord *refs,int refTop,int E,cpo code)
{
  int i,j;

  for(i=0;i<refTop;i++){	/* update the free environment */
    for(j=0;j<thLen;j++)
      if(sameCell(refs[i].ref,th[j].name)){
	genIns(code,symN(symVal(refs[i].ref)),cnupdte,th[j].depth,refs[i].Pk,refs[i].Pi);
	break;
      }
  }
}

static logical etaComp(cellpo body,int temp,dictpo dict,cpo code,
		       int depth,RefRecord *refs,int *refTop,int dLvl)
{
  cellpo fn,exp;
  int DP;

  if(IsHashStruct(body,kdebug,&body))
    return etaComp(body,temp,dict,code,depth,refs,refTop,dLvl+1);
  else if(IsHashStruct(body,knodebug,&body))
    return etaComp(body,temp,dict,code,depth,refs,refTop,dLvl-1);
  else if((isBinaryCall(body,kdefn,&fn,&exp)||
	   isBinaryCall(body,kfield,&fn,&exp)) && 
	  (isSymb(fn)||IsQuery(fn,NULL,&fn))){
    cellpo type;

    IsVar(fn,NULL,&type,NULL,NULL,NULL,NULL,NULL,dict);

    if(IsFunLiteral(exp)){
      dictpo free;
      codepo fun = compFunction(fn,exp,dict,&free,dLvl);

      /* construct the closure */
      genClosure(symN(symVal(fn)),&temp,depth,&DP,free,NULL,dict,cclosure,fun,code,
		 refs,refTop,temp);

      InitVar(fn,True,dict);
      clearDict(free,NULL);
      return True;
    }
    else if(IsProcLiteral(exp)){
      dictpo free;
      codepo proc = compProcedure(fn,exp,type,dict,&free,dLvl);

      /* construct the closure */
      genClosure(symN(symVal(fn)),&temp,depth,&DP,free,NULL,dict,cclosure,proc,code,
		 refs,refTop,temp);

      InitVar(fn,True,dict);
      clearDict(free,NULL);
      return True;
    }
    else
      return False;
  }
  else
    reportErr(lineInfo(body),"invalid element `%2w' of theta expression",body);
  return False;
}

/* Handle the non-function assignments */
static logical assignComp(cellpo body,int temp,dictpo dict,cpo code,
		       int depth,RefRecord *refs,int *refTop,int dLvl)
{
  cellpo fn,rhs;

  if(IsHashStruct(body,kdebug,&body))
    return assignComp(body,temp,dict,code,depth,refs,refTop,dLvl+1);
  else if(IsHashStruct(body,knodebug,&body))
    return assignComp(body,temp,dict,code,depth,refs,refTop,dLvl-1);
  else if((isBinaryCall(body,kdefn,&fn,&rhs)||
	   isBinaryCall(body,kfield,&fn,&rhs)) && 
	  (isSymb(fn) || (IsQuery(fn,NULL,&fn) && isSymb(fn)))){
    cellpo type;

    IsVar(fn,NULL,&type,NULL,NULL,NULL,NULL,NULL,dict);

    if(!IsFunLiteral(rhs) && !IsProcLiteral(rhs)){
      compExp(rhs,type,&temp,depth,&depth,dict,dict,code,dLvl);

      if(dLvl>0)
	genVarDebug(code,depth,fn,type,temp,dict,dict);

      InitVar(fn,True,dict);
      return True;
    }
  }
  return False;
}

void compTheta(cellpo body,cellpo type,int *dest,int depth,
	       int *dp,cpo code,dictpo dict,int dLvl)
{
  dictpo nd;			/* we are going to build a new dictionary */
  int D = genDest(dest,depth,dp,code); /* destination for the theta tuple */
  int E = depth-1;		/* temporary location for the tuple */
  int mark = depth-1;
  int items=thetaLength(body);	/* how many items are there? */
  int indx=0;
  int refTop=0;			/* initially no references */
  int i;
  RefRecord refs[1024];		/* Table of forward references */
  thetaPo th = (thetaPo)malloc(sizeof(ThetaItemRec)*(items+1));

#ifdef COMPTRACE_
  if(traceCompile)
    outMsg(logFile,"Theta expression %10.1800w\n",body);
#endif

  assert(type!=NULL);		/* make sure we transmitted the type info */

  genIns(code,"Clear mark variable",initv,mark);

  /* We pre-construct the theta object before we construct the els. */
  nd = declVar(ctclosure,type,localvar,singleAss,Inited,True,
	       E,0,addMarker(dict));

  nd = declTheta(body,th,&indx,E,code,&mark,nd,dLvl);
  if(items!=1)
    genIns(code,NULL,loc2tpl,mark,E-mark,E); /* build the outer theta tuple */

  /* The first pass constructs the functions and procedures ... */
  for(i=0;i<indx;i++){
    int temp = th[i].depth;
    cellpo fn;

    if(!((isBinaryCall(th[i].body,kdefn,&fn,NULL)||
	 isBinaryCall(th[i].body,kfield,&fn,NULL)) && 
	 (isSymb(fn) || IsQuery(fn,NULL,&fn))))
      fn = voidcell;

    if(etaComp(th[i].body,temp,nd,code,mark,refs,&refTop,th[i].dLvl) &&
       items!=1)
      genIns(code,symN(symVal(fn)),tpupdte,temp,th[i].off,E);
  }

  /* The second pass handles the other variables */
  for(i=0;i<indx;i++){
    int temp = th[i].depth;
    cellpo fn;

    if(!((isBinaryCall(th[i].body,kdefn,&fn,NULL)||
	 isBinaryCall(th[i].body,kfield,&fn,NULL)) && 
	 (isSymb(fn) || (IsQuery(fn,NULL,&fn) && isSymb(fn)))))
      fn = voidcell;

    if(assignComp(th[i].body,th[i].depth,nd,code,mark,refs,&refTop,
		  th[i].dLvl) &&
       items!=1)
      genIns(code,symN(symVal(fn)),tpupdte,temp,th[i].off,E);
  }

  updateRefs(th,indx,refs,refTop,E,code);
  if(items!=1)
    genMove(code,E,&D,"Theta expression");
  else
    genMove(code,th[0].depth,&D,"Theta expression");

  clearDict(nd,dict);		/* we are done with this dictionary */
  free(th);
}

static long thetaLen(cellpo input)
{
  cellpo lhs,rhs;

  if(isBinaryCall(input=deRef(input),ksemi,&lhs,&rhs))
    return thetaLen(lhs)+thetaLen(rhs);
  else if(isUnaryCall(input,ksemi,&lhs))
    return thetaLen(lhs);
  else if(IsQuery(input,&lhs,&rhs))
    return 1;
  else if(isBinaryCall(input,kfield,&lhs,&rhs))
    return 1;
  else if(isBinaryCall(input,kdefn,&lhs,&rhs))
    return 1;
  else
    return 0;
}

static long thetaLength(cellpo input)
{
  return thetaLen(input);
}

static logical singleArgClauses(cellpo clses)
{
  cellpo lhs,rhs;

  if(IsHashStruct(clses,knodebug,&clses))
    return singleArgClauses(clses);
  else if(IsHashStruct(clses,kdebug,&clses))
    return singleArgClauses(clses);
  else if(isBinaryCall(clses,kchoice,&lhs,&rhs))
    return singleArgClauses(lhs) && singleArgClauses(rhs);
  else if(isBinaryCall(clses,kstmt,&lhs,&rhs)||
	  isBinaryCall(clses,kfn,&lhs,&rhs)){
    if(isTpl(lhs))
      return False;
    else
      return True;
  }
  else{
    syserr("invalid program");
    return False;
  }
}

static void writeAfFile(cellpo exp,uniChar *codeName)
{
  cellpo type,arg;

  if(IsAnyStruct(exp,&arg) && (type=typeInfo(arg))!=NULL){
    uniChar afN[MAXSTR];
    uniChar *afName = fileNameSuffix(codeName,".af",afN,NumberOf(afN));
    ioPo afFile = newOutFile(afName,utf8Encoding);

    if(afFile == NULL){
      outMsg(logFile,"failed to create module file [%U]\n", afName);
      comp_exit(1);
    }

    outMsg(afFile,"/* automatically generated -- do not edit */\n");
    outMsg(afFile,"/* Module interface file for %U */\n",codeName);
    outMsg(afFile,"%0.749t from \"%U\";\n",type,codeName);

    closeFile(afFile);
  }
}

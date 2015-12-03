/*
  Statement code generator for April compiler
  New version for polymorphic based type system 
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
#include "compile.h"		/* Compiler structures */
#include "generate.h"		/* Code generation interface */
#include "types.h"		/* Type cheking interface */
#include "dict.h"
#include "keywords.h"		/* Standard April keywords */
#include "opcodes.h"		/* April machine opcodes */
#include "assem.h"		/* Code generation*/
#include "cdbgflags.h"		/* Debugging option flags */

static void InvokeContinuation(continPo cont,cpo code);
static int EndErrBlock(cpo code,int cl);
static int EndWithinBlock(cpo code,int cl);
static void assignStmt(cellpo lhs,cellpo rhs,int depth,
		       dictpo dict,dictpo outer,cpo code,lblpo Next,int dLvl);
static void compPrim(cellpo input,int depth,int *dp,cxpo ctxt,lblpo Next,
		     dictpo dict,dictpo *ndict,dictpo outer,
		     cont *cnt,cpo code,int dLvl);
static void compMatchStmt(cellpo lhs,cellpo rhs,int depth,int *dp,lblpo Next,
			  dictpo dict,dictpo *ndict,dictpo outer,cpo code,
			  assMode mode,int dLvl);

typedef struct {
  cxpo ctxt;			/* context structure */
  cont cnt;			/* continuation result */
} choiceStmtInfo;

static logical compStmtChoice(cellpo input,int arity,int srcvar,
			      int depth,lblpo Next,lblpo LF,
			      dictpo dict,cpo code,logical last,int dLvl,
			      void *cl);


static void compStmts(cellpo input,int depth,cxpo ctxt,lblpo Next,
		      dictpo dict,dictpo outer,cont *cnt,cpo code,int dLvl)
{
  cellpo st;
  dictpo ndict;

  if(IsHashStruct(input,knodebug,&input))
    compStmts(input,depth,ctxt,Next,dict,outer,cnt,code,dLvl-1);
  else if(isBinaryCall(input,ksemi,&st,&input)){
    lblpo L0 = NewLabel(code);
    cont contin;

    compStmt(st,depth,&depth,ctxt,L0,dict,&ndict,outer,&contin,code,dLvl);
    DefLabel(code,L0);
    compStmts(input,depth,ctxt,Next,ndict,outer,cnt,code,dLvl);
    clearDict(ndict,dict);
  }
  else if(isUnaryCall(input,ksemi,&st)){
    compStmt(st,depth,&depth,ctxt,Next,dict,&ndict,outer,cnt,code,dLvl);
    clearDict(ndict,dict);
  }
  else{
    compStmt(input,depth,&depth,ctxt,Next,dict,&ndict,outer,cnt,code,dLvl);
    clearDict(ndict,dict);
  }
}

static void compMatchStmt(cellpo lhs,cellpo rhs,int depth,int *dp,lblpo Next,
			  dictpo dict,dictpo *ndict,dictpo outer,cpo code,
			  assMode mode,int dLvl)
{
  cellpo exp=foldExp(rhs,rhs,dict);
  cellpo var;
  cellpo type=VarDecPtn(lhs,NULL,&var,dict);

  if(type!=NULL && isSymb(var) && /* a new variable? */
     (!IsLocalVar(var,dict) || IsLocalVar(var,outer))){
    int D=0;

    compExp(exp,type,&D,depth,dp,dict,outer,code,dLvl);

    if(ndict!=NULL)
      *ndict = declVar(var,type,localvar,mode,Inited,True,D,-1,dict);
    if(dLvl>0){
      lblpo LE = NewLabel(code);
  
      genVarDebug(code,*dp,var,type,D,dict,outer);
      DefLabel(code,LE);
    }
  }
  else{
    logical anyPresent = EmbeddedAny(lhs);
    lblpo LF=NewLabel(code);	/* grab a new failure label */

    if(anyPresent)
      genIns(code,"prepare for type unification",urst,depth);
  
    if(compExpPttrn(lhs,exp,depth,dp,LF,dict,ndict,dict,outer,code,mode,False,dLvl)){
      genIns(code,NULL,jmp,Next);	/* skip around failure code */
      DefLabel(code,LF);
      if(anyPresent)
        genIns(code,"undo type unification",undo,depth);

      matchError(lhs,depth,dict,code,dLvl);
    }
    else
      DefLabel(code,LF);		/* Just a dummy label */
  }
}
  
/* Generate code for a single statement */
void compStmt(cellpo input,int depth,int *dp,cxpo ctxt,lblpo Next,
	      dictpo dict,dictpo *ndict,dictpo outer,cont *cnt, cpo code,
	      int dLvl)
{
  cellpo s1,s2;

  assert(dp!=NULL);

  *dp=depth;

  if(ndict!=NULL)
    *ndict=dict;

  if(IsHashStruct(input,knodebug,&input))
    compStmt(input,depth,dp,ctxt,Next,dict,ndict,outer,cnt,code,dLvl-1);

  else if(isBinaryCall(input,ksemi,&s1,&s2)){
    dictpo ndict=dict;
    if(dLvl>0)
      genScopeDebug(code,depth,ndict);
    compStmts(input,depth,ctxt,Next,ndict,outer,cnt,code,dLvl);
    clearDict(ndict,dict);
    if(dLvl>0)
      genScopeDebug(code,depth,dict);
  }

  else if(isUnaryCall(input,ksemi,&s1))
    compStmt(s1,depth,dp,ctxt,Next,dict,ndict,outer,cnt,code,dLvl);

  else if(isBinaryCall(input,kelse,&s1,&s2)){
    cellpo test,then,t;

    if(isBinaryCall(s1,kthen,&test,&then) && isUnaryCall(test,kif,&test)){
      lblpo L0=NewLabel(code);
      dictpo nd;
      int DP;

      if(dLvl>0)
	genLineDebugInfo(code,depth,test,True); /* generate debugging info */

      test=foldExp(test,test,dict);

#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"if test %w (depth=%d)\n",test,depth);
#endif

      if(IsSymbol(test,ktrue) || (IsEnumStruct(test,&t) && IsSymbol(t,ktrue))){
	compStmt(then,depth,&DP,ctxt,Next,dict,&nd,outer,cnt,code,dLvl);
	clearDict(nd,dict);	
      }
      else if(IsSymbol(test,kfalse)
	      || (IsEnumStruct(test,&t) && IsSymbol(t,kfalse))){
	compStmt(s2,depth,&DP,ctxt,Next,dict,&nd,outer,cnt,code,dLvl);
	clearDict(nd,dict);	
      }
      else{
	cont tcont;

	compTest(test,depth,&DP,L0,dict,&nd,outer,code,dLvl); /* if test */
	if(dLvl>0)
	  genPtnVarDebug(code,DP,nd,dict,dict);

	compStmt(then,DP,&DP,ctxt,Next,nd,&nd,outer,&tcont,code,dLvl);
	if(tcont==continued)	/* Did we jump or drift? */
	  genIns(code,"exit from then",jmp,Next); /* we are done */
	clearDict(nd,dict);	
	DefLabel(code,L0);

	compStmt(s2,depth,&DP,ctxt,Next,dict,&nd,outer,cnt,code,dLvl);
	clearDict(nd,dict);	
      }
    }
    else
      reportErr(lineInfo(input),"Badly formed conditional: `%5.1800w'",input);
  }

  else if(isBinaryCall(input,kthen,&s1,&s2)){
    cellpo test,t;

    if(isUnaryCall(s1,kif,&test)){
      int DP;
      dictpo nd;

      if(dLvl>0)
	genLineDebugInfo(code,depth,test,True); /* generate debugging info */

      test=foldExp(test,test,dict);

#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"if test %w (depth=%d)\n",test,depth);
#endif

      if(IsSymbol(test,ktrue) || (IsEnumStruct(test,&t) && IsSymbol(t,ktrue))){
	compStmt(s2,depth,&DP,ctxt,Next,dict,&nd,outer,cnt,code,dLvl);
	clearDict(nd,dict);	
      }
      else{
	cont tcont;

	compTest(test,depth,&DP,Next,dict,&nd,outer,code,dLvl); /* if test */

	compStmt(s2,DP,&DP,ctxt,Next,nd,&nd,outer,&tcont,code,dLvl);
	*cnt = continued;
	clearDict(nd,dict);
      }
    }
    else
      reportErr(lineInfo(input),"Badly formed conditional: `%5.1800w'",input);
  }

  else if(isBinaryCall(input,kdo,&s1,&s2)){
    cellpo test;

    if(isUnaryCall(s1,kfor,&test)){
      cellpo el,set;
      if(isBinaryCall(test,kin,&el,&set)){
	lblpo L0=NewLabel(code);
	lblpo L1=NewLabel(code);
	dictpo ndict=dict;
	int Lst=--depth;
	int EL;		/* set element variable */
	int DP;
	cont scont;
	logical anyPresent = EmbeddedAny(el);

#ifdef COMPTRACE
	if(traceCompile)
	  outMsg(logFile,"for %w in %w test (depth=%d)\n",el,set,depth);
#endif

	if(dLvl>0)
	  genScopeDebug(code,depth+1,ndict);

	if(complexExp((set=foldExp(set,set,dict))))
	  genIns(code,"list copy",initv,Lst);

	/* Construct the set */
	compExp(set,NULL,&Lst,depth,&DP,dict,outer,code,dLvl); 

	EL = --DP;
	if(dLvl>0)
	  genIns(code,"initialize for debugging",initv,EL);
	
	genIns(code,"loop test",jmp,L1);
	DefLabel(code,L0);
	genIns(code,"head of list",ulst,Lst,EL,Lst); /* Pick up next el */

	  
	if(anyPresent)
	  genIns(code,"we have to account for type unification",urst,depth);

	compPttrn(el,EL,DP,&DP,L1,ndict,&ndict,ndict,outer,code,singleAss,dLvl);

	if(dLvl>0){
	  genPtnVarDebug(code,DP,ndict,dict,dict);
	  genLineDebugInfo(code,DP,test,True); /* generate debugging info */
        }

	compStmts(s2,DP,ctxt,L1,ndict,outer,&scont,code,dLvl); /* body */

	DefLabel(code,L1);
	if(anyPresent)
	  genIns(code,"undo any bound type vars",undo,depth);
	genIns(code,NULL,mnil,Lst,L0);

	*cnt=continued;	/* mark code as having a normal exit */
	
	clearDict(ndict,dict); /* remove excess dictionary */
	if(dLvl>0)
	  genScopeDebug(code,depth,dict);
      }
      else
	reportErr(lineInfo(input),"badly formed for loop: `%5.1800w'",input);
    }
    else if(isUnaryCall(s1,kwhile,&test)){
      lblpo L0=NewLabel(code);
      dictpo nd;
      cont scont;
      cellpo t;

      DefLabel(code,L0);
      if(dLvl>0)
	genLineDebugInfo(code,depth,test,True); /* generate debugging info */

      test=foldExp(test,test,dict);

#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"While test (depth=%d) `%w'\n",depth,test);
#endif

      if(!IsSymbol(test,kfalse) || 
	 (IsEnumStruct(test,&t) && IsSymbol(t,kfalse))){
	compTest(test,depth,&depth,Next,dict,&nd,outer,code,dLvl); 

	if(dLvl>0){
	  lblpo lX = NewLabel(code);

          genIns(code,"Test for debugging",debug_d,lX);

	  genPtnVarDebug(code,depth,nd,dict,dict);
	  genLineDebugInfo(code,depth,test,False); /* generate debugging info */
	  DefLabel(code,lX);
	}
      
	compStmts(s2,depth,ctxt,L0,nd,outer,&scont,code,dLvl);
	if(scont==continued)
	  genIns(code,"do loop test",jmp,L0);
	clearDict(nd,dict);
      }

      *cnt=continued;		/* we drift out of loops */

    }
    else
      reportErr(lineInfo(input),"badly formed `do' condition: %5.1800w",s1);
  }

  else if(isUnaryCall(input,ktry,&s1)) /* This is a noise word */
    compStmt(s1,depth,dp,ctxt,Next,dict,ndict,outer,cnt,code,dLvl);

  else if(isBinaryCall(input,kcatch,&s1,&s2)){
    int EC=depth-=2;		/* place for the error code */
    int DP;
    lblpo L0=NewLabel(code);
    lblpo L1=NewLabel(code);
    cont ecnt;
    context sctxt=*ctxt;
    ContinRec vc;
    dictpo ndict=dict;

    vc.cl = EC;
    vc.fn = EndErrBlock;
    vc.next = NULL;

    sctxt.valCont = &vc;
    sctxt.prev = ctxt;		/* Set up prior context */
    sctxt.ct = continuation;

    genIns(code,NULL,errblk,EC,L0); /* start an error block */
    compStmt(s1,depth,&DP,&sctxt,L1,dict,&ndict,outer,&ecnt,code,dLvl);
    DefLabel(code,L1);
    genIns(code,NULL,errend,EC);	/* end the error block */
    genIns(code,NULL,jmp,Next); /* Exit to next statement */
    clearDict(ndict,dict);

    DefLabel(code,L0);

    genIns(code,"Pick up error value",moverr,--depth);
    if(dLvl>0)
      genIns(code,"report the error",error_d,depth,depth);

    {
      choiceStmtInfo info = {ctxt,continued};
      lblpo LE = NewLabel(code);

      if(compChoice(s2,1,depth,depth,Next,LE,dict,code,dLvl,compStmtChoice,&info)){
	DefLabel(code,LE);
	genIns(code,"Re-raise error",generr,depth);
	if(cnt!=NULL)
	  *cnt=jumped;
      }
      else{
	DefLabel(code,LE);	/* dummy... */
	if(cnt!=NULL)
	  *cnt = info.cnt;	/* pick up continuation from choice */
      }
    }
  }

  else if(isBinaryCall(input,kwithin,&s1,&s2)){
    int DP;
    int CC2 = 0;

    compExp(foldExp(s2,s2,dict),NULL,&CC2,depth,&DP,dict,outer,code,dLvl);

    {
      lblpo Le = NewLabel(code);
      lblpo Lx = NewLabel(code);
      int EC = DP-=2;		/* place for error recovery*/
      int CC = --DP;		/* where is the click counter? */
      context sctxt;
      ContinRec vc,svc;
      cont scnt;

      vc.cl = CC;		/* set up to generate the use instruction */
      vc.fn = EndWithinBlock;
      vc.next = NULL;

      svc.cl = EC;		/* we also need to do the enderr stuff */
      svc.fn = EndErrBlock;
      svc.next = &vc;

      sctxt.valCont = &svc;	/* create a sub-context */
      sctxt.prev = ctxt;	/* Set up prior context */
      sctxt.ct = continuation;
      sctxt.dest=CC;
      sctxt.brk=Next;		/* where to go next */

      genIns(code,"use new click counter",use,CC2,CC);
      genIns(code,"wrap with error handler",errblk,EC,Le);

      if(dLvl>0)
	genScopeDebug(code,DP,dict);

      compStmt(s1,DP,&DP,&sctxt,Lx,dict,ndict,outer,&scnt,code,dLvl);

      DefLabel(code,Lx);

      if(dLvl>0)
	genScopeDebug(code,DP,*ndict);

      if(scnt==continued)	/* restore old click counter */
	genIns(code,"restore previous click counter",use,CC,CC);
      genIns(code,NULL,errend,EC);
      genIns(code,NULL,jmp,Next);

      DefLabel(code,Le);	/* we need to pick up errors here */
      genIns(code,"restore prior click counter",use,CC,CC);
      genIns(code,"Pick up error value",moverr,--depth);
      genIns(code,"Re-raise the error",generr,depth);
      *cnt = jumped;		/* we jumped, whatever */
    }
  }

  else if(isUnaryCall(input,kvalueof,&s1)){ /* allow valof as a statement */
    context ctxt;
	
    ctxt.ct=valof;		/* set up the right context */
    ctxt.dest=--depth;
    ctxt.type=anyTp;
    ctxt.brk=Next;
    ctxt.valCont=NULL;
    ctxt.prev = NULL;

    genIns(code,NULL,initv,ctxt.dest);

    compStmt(s1,depth,dp,&ctxt,Next,dict,ndict,outer,cnt,code,dLvl);
  }
  else if((isBinaryCall(input,klabel,&s1,&s2) || 
           isBinaryCall(input,kguard,&s1,&s2)) && isSymb(s1)){
    context sctxt=*ctxt;

    sctxt.brk=Next;
    sctxt.label = symVal(s1);
    sctxt.ct = labelled;
    sctxt.prev = ctxt;		/* Set up prior context */

    compStmt(s2,depth,dp,&sctxt,Next,dict,ndict,outer,cnt,code,dLvl);
  }
  else if(isUnaryCall(input,kleave,&s1) && isSymb(s1)){
    symbpo k = symVal(s1);

    if(dLvl>0 && source_file(input)!=NULL)
      genLineDebugInfo(code,depth,input,True);

    *cnt=jumped;		/* We jump out of this one */

    /* Invoke any continuations that are valid here */
    while(ctxt!=NULL && !(ctxt->ct==labelled && k==ctxt->label)){
      if(ctxt->ct==continuation)
	InvokeContinuation(ctxt->valCont,code); /* Invoke continuation */
      ctxt = ctxt->prev;
    }

    if(ctxt!=NULL && ctxt->brk!=NULL && ctxt->ct==labelled && k==ctxt->label)
      genIns(code,symN(k),jmp,ctxt->brk);
    else
      reportErr(lineInfo(input),"Cant find break label `%w' or label out of scope",s1);
  }

  else if(IsHashStruct(input,kdebug,&input)){
    if(dLvl>0)
      compStmt(input,depth,dp,ctxt,Next,dict,ndict,outer,cnt,code,dLvl);
    else{
      if(cnt!=NULL)
	*cnt=continued;
    }
  }

  else
    compPrim(input,depth,dp,ctxt,Next,dict,ndict,outer,cnt,code,dLvl);
}


static void compPrim(cellpo input,int depth,int *dp,cxpo ctxt,lblpo Next,
		     dictpo dict,dictpo *ndict,dictpo outer,
		     cont *cnt, cpo code,int dLvl)
{
  cellpo args,lhs,rhs;

  *dp=depth;			/* Default depth of result */

  if(ndict!=NULL)
    *ndict=dict;

  if(cnt!=NULL)
    *cnt=continued;

#ifdef COMPTRACE
  if(traceCompile)
    outMsg(logFile,"Statement (depth=%d) `%w'\n",depth,input);
#endif

  if(dLvl>0)
    genLineDebugInfo(code,depth,input,True); /* generate debugging info */

  if(isSymb(input)){		/* one of the standard keyword statements? */
    symbpo st=symVal(input);

    if(st==krelax || st==kblock)
      return;			/* No code generated here */
    else if(st==khalt){
      if(cnt!=NULL)
	*cnt=jumped;
      genIns(code,NULL,halt);
    }
    else if(st==kdie){
      if(cnt!=NULL)
	*cnt=jumped;
      if(dLvl>0 && source_file(input)!=NULL)
	genIns(code,NULL,die_d,depth);
      genIns(code,NULL,die);
    }
    else
      reportErr(lineInfo(input),"illegal statement: `%5.1800w'");
  }

  else if(isUnaryCall(input,kelement,&rhs)){
    while(ctxt!=NULL && ctxt->ct!=collect)
      ctxt = ctxt->prev;
    if(ctxt!=NULL && ctxt->ct==collect){
      int D=0; int DP;

      compExp(foldExp(rhs,rhs,dict),NULL,&D,depth,&DP,dict,outer,code,dLvl);
      genIns(code,"elemis",lsupdte,DP,D,ctxt->dest);
    }
    else
      reportErr(lineInfo(input),"statement `%5.1800w' not in collect or setof context",input);
  }

  else if(isUnaryCall(input,kvalis,&rhs)){
    cxpo ctext = ctxt;

    while(ctxt!=NULL && ctxt->ct!=valof)
      ctxt = ctxt->prev;

    if(ctxt!=NULL && ctxt->ct==valof){
      int DP;

      compExp(foldExp(rhs,rhs,dict),NULL,&ctxt->dest,depth,&DP,dict,outer,code,
	      dLvl);

      while(ctext!=NULL && ctext->ct!=valof){
	if(ctext->ct==continuation)
	  InvokeContinuation(ctext->valCont,code); /* Invoke continuation */
	ctext = ctext->prev;
      }

      if(ctxt->brk!=Next){
	genIns(code,"valis",jmp,ctxt->brk);
	*cnt=jumped;
      }
      else
	*cnt=continued;
    }
    else
      reportErr(lineInfo(input),"`%5.1800w' not in proper context",input);
  }

  else if(isBinaryCall(input,ksend,&lhs,&rhs)){
    int M = 0;
    int H = 0;

    anyExp(lhs,&M,depth,&depth,dict,outer,code,dLvl);
    compExp(rhs,typeInfo(rhs),&H,depth,&depth,dict,outer,code,dLvl);


    if(dLvl>0){
      genSendDebug(code,depth,H,M,typeInfo(lhs),dict,outer);
    }

    genIns(code,"Simple send message",snd,depth,H,M);
  }
  else if(isBinaryCall(input,kassign,&lhs,&rhs))
    assignStmt(lhs,rhs,depth,dict,outer,code,Next,dLvl);

  else if(isBinaryCall(input,kmatch,&lhs,&rhs))
    compMatchStmt(lhs,rhs,depth,dp,Next,dict,ndict,outer,code,singleAss,dLvl);

  else if(isBinaryCall(input,kfield,&lhs,&rhs)){
    cellpo exp = foldExp(rhs,rhs,dict);
    cellpo var = lhs;
    cellpo type = typeInfo(var);

    if(IsQuery(var,NULL,&var))
      type = typeInfo(var);

    if(type!=NULL && isSymb(var)){
      if(!IsLocalVar(var,dict)|| IsLocalVar(var,outer)){
	int D=0;
	compExp(exp,type,&D,depth,dp,dict,outer,code,dLvl);

	if(depth==*dp){		/* we must actually create a new copy of this */
	  *dp = --depth;
	  genMove(code,D,&depth,symN(symVal(var)));
	  D = depth;
	}

	if(ndict!=NULL)
	  *ndict =declVar(var,type,localvar,multiAss,Inited,True,D,-1,dict);

	if(dLvl>0){
	  genVarDebug(code,*dp,var,type,D,dict,outer);
	}
      }
      else
	reportErr(lineInfo(input),"%w attempt to re-declare variable",var);
    }
    else
      compMatchStmt(lhs,rhs,depth,dp,Next,dict,ndict,outer,code,multiAss,dLvl);
  }

  else if(isBinaryCall(input,kdefn,&lhs,&rhs)){	/* declare read-only var */
    cellpo exp = foldExp(rhs,rhs,dict);
    cellpo var = lhs;
    cellpo type = typeInfo(lhs);

    if(IsQuery(var,NULL,&var))
      type = typeInfo(var);

    if(type!=NULL && isSymb(var)){
      if(!IsLocalVar(var,dict) || IsLocalVar(var,outer)){
	int D=0;
	compExp(exp,type,&D,depth,dp,dict,outer,code,dLvl);

	if(depth==*dp){		/* we might need to create a copy of this */
	  assMode rw;

	  if(!(isSymb(exp) && 
	       IsVar(exp,NULL,NULL,NULL,NULL,&rw,NULL,NULL,dict) &&
	       rw==singleAss)){
	    *dp = --depth;
	    genMove(code,D,&depth,symN(symVal(var)));
	    D = depth;
	  }
	}

	if(!IsSymbol(var,kuscore)){
	  if(ndict!=NULL)
	    *ndict =declVar(var,type,localvar,singleAss,Inited,True,D,-1,dict);

	  if(dLvl>0){
	    genVarDebug(code,*dp,var,type,D,dict,outer);
	  }
	}
      }
      else
	reportErr(lineInfo(input),"%w attempt to re-declare variable",var);
    }
    else
     compMatchStmt(lhs,rhs,depth,dp,Next,dict,ndict,outer,code,singleAss,dLvl);
  }

  else if(isBinaryCall(input,kdot,&lhs,&rhs)){
    int D=0;
    dictpo ndict = dict;

    if(dLvl>0)
      genScopeDebug(code,depth,dict);

    compExp(foldExp(lhs,lhs,dict),NULL,&D,depth,&depth,dict,outer,code,dLvl);

    ndict = extendDict(typeInfo(lhs),D,code,depth,&depth,ndict);

    compStmt(rhs,depth,dp,ctxt,Next,ndict,&ndict,outer,cnt,code,dLvl);

    clearDict(ndict,dict);	/* pop the dictionary */

    if(dLvl>0)
      genScopeDebug(code,depth,dict);
  }

  else if(isUnaryCall(input,kraise,&args)){
    int D=0;

    compExp(foldExp(args,args,dict),NULL,&D,depth,&depth,dict,outer,code,dLvl);
    if(cnt!=NULL)
      *cnt=jumped;
    genIns(code,NULL,generr,D);
  }
  
  else if(isUnaryCall(input,kcase,&lhs) && isBinaryCall(lhs,kin,&lhs,&rhs)){
    int D=0;
    choiceStmtInfo info = {ctxt,continued};
    lblpo LE = NewLabel(code);
   
    compExp(foldExp(lhs,lhs,dict),NULL,&D,depth,&depth,dict,outer,code,dLvl);
    
    if(dLvl>0)
      genLineDebugInfo(code,depth,lhs,True); /* generate debugging info */

    if(compChoice(rhs,1,D,depth,Next,LE,dict,code,dLvl,compStmtChoice,&info)){
      DefLabel(code,LE);
      failError(lhs,depth,dict,code,dLvl);
      if(cnt!=NULL)
	*cnt = jumped;
    }
    else{
      DefLabel(code,LE);		/* dummy... */
      if(cnt!=NULL)
	*cnt = info.cnt;	/* pick up continuation from choice */
    }
  }

  else if(isCons(input)){
    long arity = compArguments(input,depth,&depth,dict,outer,code,dLvl);

    if(dLvl>0)
      genLineDebugInfo(code,depth,input,True); /* generate debugging info */
      
    procCall(consFn(input),arity,depth,dict,code,dLvl);
    if(cnt!=NULL)
      *cnt=continued;
  }
  else
    reportErr(lineInfo(input),"`5.1800w' illegal statement",input);
}

static logical compStmtChoice(cellpo input,int arity,int srcvar,
			      int depth,lblpo Next,lblpo LF,dictpo dict,
			      cpo code,logical last,int dLvl,
			      void *cl)
{
  choiceStmtInfo *info = (choiceStmtInfo*)cl;
  cellpo ptn,rhs;
  cellpo a1,a2;

  if(isBinaryCall(input,kstmt,&ptn,&rhs)){
    dictpo ndict,lo;
    cellpo guard=ctrue;
    int DP = depth;
    int src = srcvar;
    cont cnt;
    logical mayFail = False;	/* no possibility of failure...yet */
    logical anyPresent = EmbeddedAny(ptn);
    lblpo Lf = LF;
    
    if(anyPresent){
      Lf = NewLabel(code);
      genIns(code,"reset unification",urst,depth);
    }

    isBinaryCall(ptn,kguard,&ptn,&guard); /* remove the guard for a while */

    //    ndict=lo=BoundVars(ptn,src,arity,addMarker(dict),DP,code,dLvl);
    ndict=lo=addMarker(dict);
          
    mayFail = compPttrn(ptn,src,DP,&DP,Lf,ndict,&ndict,lo,dict,code,singleAss,dLvl);

    compTest(guard,DP,&DP,Lf,ndict,&ndict,dict,code,dLvl);

    if(!IsSymbol(guard,ktrue) || 
       (IsEnumStruct(guard,&guard) && !IsSymbol(guard,ktrue)))
      mayFail = True;
    
    if(dLvl>0 && source_file(rhs)!=NULL)
      genLineDebugInfo(code,DP,rhs,True); /* are we debugging? */

    compStmt(rhs,DP,&DP,info->ctxt,Next,ndict,&ndict,dict,&cnt,code,dLvl);
    if(cnt==continued && (!last||mayFail||anyPresent))
      genIns(code,NULL,jmp,Next); /* we are done with the choice */
      
    if(anyPresent){
      DefLabel(code,Lf);
      genIns(code,"undo type unification",undo,depth);
      genIns(code,"and fail",jmp,LF);
    }
    
    if(last)
      info->cnt = cnt;		/* record the result of the last choice */
    clearDict(ndict,dict);	/* remove excess dictionary */
    return mayFail;
  }
  else if(isBinaryCall(input,kchoice,&a1,&a2)){
    lblpo L0 = NewLabel(code);
    choiceStmtInfo iinfo = {info->ctxt,continued};

    compStmtChoice(a1,arity,srcvar,depth,Next,L0,dict,code,False,dLvl,&iinfo);

    if(iinfo.cnt==continued)
      genIns(code,"Skip around other choices",jmp,Next);

    DefLabel(code,L0);
    return compStmtChoice(a2,arity,srcvar,depth,Next,LF,dict,code,False,dLvl,cl);
  }
  else{
    reportErr(lineInfo(input),"`5.1800w' case clause",input);
    return True;
  }
}

void procCall(cellpo f,int arity,int depth,dictpo dict,cpo code,int dLvl)
{
  int index,off;
  vartype vt;
  int esc_code;

  if(isSymb(f)){
    int parity;

    if(IsProcVar(f,&parity,NULL,&off,&index,&vt,dict)){
      switch(vt){
      case localvar:
	genIns(code,symN(symVal(f)),call,depth,parity,off);
	break;
      case envvar:
	genIns(code,symN(symVal(f)),ecall,depth,parity,off);
	break;
      case indexvar:{
	int D = depth-1;
	genIns(code,"Access proc from record",indxfld,off,index,D);
	genIns(code,symN(symVal(f)),call,depth,parity,D);
	break;
      }
      case thetavar:{
	int D=depth-1;
	genIns(code,"Theta variable",emove,off,D);
	genIns(code,symN(symVal(f)),indxfld,D,index,D);
	genIns(code,symN(symVal(f)),call,depth,parity,D);
      }
      }
    }
    else if((esc_code=IsEscapeProc(symVal(f),&parity,NULL))!=-1)
      genIns(code,symN(symVal(f)),esc_fun,depth,esc_code);
    else
      reportErr(lineInfo(f),"non-procedure %4w applied",f);
  }
  else{
    int D=0;
    int DP;
    
    compExp(f,NULL,&D,depth,&DP,dict,dict,code,dLvl);
    genIns(code,NULL,call,depth,arity,D);
  }
}

static void assignStmt(cellpo lhs,cellpo rhs,int depth,
		       dictpo dict,dictpo outer,
		       cpo code,lblpo Next,int dLvl)
{
  rhs=foldExp(rhs,rhs,dict);

  if(isSymb(lhs)){		/* normal assignment statement */
    int index,off,DP;
    vartype vt;
    assMode rw;
    initStat inited;
    cellpo type;
    
    if(IsVar(lhs,NULL,&type,&off,&index,&rw,&inited,&vt,dict)){
      if(rw==singleAss)
	reportErr(lineInfo(lhs),"assignment to read-only variable -- %w",lhs);

      switch(vt){
      case localvar:
	if(inited==notInited){
	  reportWarning(lineInfo(lhs),"assignment to unitialized variable %w",lhs);
	  genIns(code,symN(symVal(lhs)),initv,off);
	}

	compExp(rhs,NULL,&off,depth,&DP,dict,outer,code,dLvl);

	if(dLvl>0){
	  genVarDebug(code,DP,lhs,type,off,dict,outer);
	}

	break;
      case envvar:{
	int D=0;
	compExp(rhs,NULL,&D,depth,&DP,dict,outer,code,dLvl);
	genIns(code,"We are going to update the environment",stoe,--DP);
	genIns(code,"Updating environment now",tpupdte,D,off,DP);

	if(dLvl>0)
	  genVarDebug(code,DP,lhs,type,D,dict,outer);

	break;
      }
      case indexvar:{
	int D=0;
	compExp(rhs,NULL,&D,depth,&DP,dict,outer,code,dLvl);
	genIns(code,NULL,tpupdte,D,off,index);


	if(dLvl>0)
	  genVarDebug(code,DP,lhs,type,D,dict,outer);

	break;
      }
      case thetavar:{
	int D=0;
	compExp(rhs,NULL,&D,depth,&DP,dict,outer,code,dLvl);

	genIns(code,"Update theta variable",emove,off,--DP);
	genIns(code,symN(symVal(lhs)),tpupdte,D,index,DP);


	if(dLvl>0)
	  genVarDebug(code,DP,lhs,type,D,dict,outer);

	break;
      }
      }
    }
    else if(IsSymbol(lhs,kuscore) || IsSymbol(lhs,kquery)){
      int D=0;
      compExp(rhs,NULL,&D,depth,&DP,dict,outer,code,dLvl);
    }
    else
      reportErr(lineInfo(lhs),"invalid lhs -- `%5.1800w' -- of assignment",lhs);
  }
  else{
    int DP;
    lblpo LF = NewLabel(code);
    dictpo ndict;
    logical anyPresent = EmbeddedAny(lhs);
    
    if(anyPresent)
      genIns(code,"prepare for type unification",urst,depth);

    compExpPttrn(lhs,rhs,depth,&DP,LF,dict,&ndict,dict,outer,code,multiAss,True,dLvl);

    genIns(code,NULL,jmp,Next);	/* skip around failure code */
    DefLabel(code,LF);
    
    if(anyPresent)
      genIns(code,"undo type unification",undo,depth);
    clearDict(ndict,dict);

    matchError(lhs,depth,dict,code,dLvl);
  }
}

/* Generate code for a match error */
void matchError(cellpo input,int depth,dictpo dict,cpo code,int dLvl)
{
  genError(input,"match error",cmatcherr,depth,dict,code,dLvl);
}

/* Generate code for a failed function application error */
void failError(cellpo input,int depth,dictpo dict,cpo code,int dLvl)
{

  genError(input,"failed",cfailed,depth,dict,code,dLvl);
}

/* Generate code for a general error */
void genError(cellpo input,char *msg,cellpo ecode,
	      int depth,dictpo dict,cpo code,int dLvl)
{
  int F = --depth;
  int A = --depth;
  long line=source_line(input);

  genIns(code,"Generate error exception",movl,cerror,F);
  
  if(line!=-1){
    uniChar em[MAX_SYMB_LEN];
    cellpo lno = allocSingle();

    strMsg(em,NumberOf(em),"%s - %U/%d",msg,symVal(source_file(input)),line);

    mkStr(lno,allocString(em));

    genIns(code,NULL,movl,lno,A);
  }
  else{
    uniChar em[MAX_SYMB_LEN];
    cellpo S = allocSingle();

    strMsg(em,NumberOf(em),"%s",msg);

    mkStr(S,allocString(em));

    genIns(code,NULL,movl,S,A);
  }

  genIns(code,NULL,movl,ecode,--depth);
  genIns(code,NULL,loc2cns,depth,2,F);
  genIns(code,NULL,generr,F);	/* generate an error */
}

/* Continuation call back for error blocks */
static int EndErrBlock(cpo code,int cl)
{
  return genIns(code,NULL,errend,cl);
}

/* Continuation call back for within statements */
static int EndWithinBlock(cpo code,int cl)
{
  return genIns(code,"return to prior CC",use,cl,cl);
}

static void InvokeContinuation(continPo cont,cpo code)
{
  while(cont!=NULL){
    contin_fun fn = cont->fn;	/* needed to work around OS X compiler */

    fn(code,cont->cl);
    cont=cont->next;
  }
}

dictpo extendDict(cellpo type,int var,cpo code,int depth,int *dp,dictpo dict)
{
  cellpo lhs,rhs;

  dict = declVar(cenvironment,type,localvar,singleAss,Inited,True,var,0,dict);

  if(isTpl(type=deRef(type))){
    int off=0;			/* The first offset in a record is 1 */
    long ar = tplArity(type);
    long i;

    for(i=0;i<ar;i++){
      cellpo at = deRef(tplArg(type,i));

      if(IsQuery(at,&lhs,&rhs) && isSymb(rhs=deRef(rhs)))
	dict=declVar(rhs,lhs,indexvar,singleAss,Inited,True,var,off++,dict);
    }

    return dict;
  }
  else if(IsQuery(type,&lhs,&rhs))
    return declVar(rhs,lhs,localvar,singleAss,Inited,True,var,0,dict);
  else{
    reportErr(lineInfo(type),"Record component %w inaccessible",type);
    return dict;
  }
}     


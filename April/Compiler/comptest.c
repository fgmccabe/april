/*
  Code generator for predicates for April compiler
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
#include <assert.h>
#include "compile.h"		/* Compiler structures */
#include "generate.h"		/* Code generation interface */
#include "types.h"		/* Type cheking interface */
#include "dict.h"
#include "keywords.h"		/* Standard April keywords */
#include "opcodes.h"		/* April machine opcodes */
#include "assem.h"		/* Code generation*/

/* Check out for a predicate expression */
logical IsAtest(cellpo input,dictpo dict)
{
  cellpo ft;
  cellpo fn;
  centry ce;
  
  if(IsHashStruct(input,knodebug,&input))
    return IsAtest(input,dict);
  else if(IsHashStruct(input,kdebug,&input))
    return IsAtest(input,dict);
  else if(isSymb(input)){
    if(IsVar(input,&ce,&ft,NULL,NULL,NULL,NULL,NULL,dict)&&ce==cvar)
      return isSymb(ft) && symVal(ft)==klogical;
    else
      return symVal(input)==ktrue || symVal(input)==kfalse;
  }
  else if(IsEnumStruct(input,&input))
    return symVal(input)==ktrue || symVal(input)==kfalse;
  else if(IsBinaryStruct(input,&fn,NULL,NULL) && isSymb(fn)){
    symbpo f=symVal(fn);
    if(f==kand || f==kor || f==kdand || f==kdor ||
	 f==kmatch || f==knomatch ||
	 f==kgreaterthan || f==kgreatereq ||
	 f==klessthan || f==klesseq ||
	 f==kin || f==kequal || f==knotequal)
      return True;
    else
      return IsFunVar(fn,NULL,&ft,NULL,NULL,NULL,NULL,dict)
        && IsSymbol(ft,klogical);
  }
  else if(IsUnaryStruct(input,&fn,NULL) && isSymb(fn)){      
    if(symVal(fn)==knot || symVal(fn)==kdnot)
      return True;
    else
      return IsFunVar(fn,NULL,&ft,NULL,NULL,NULL,NULL,dict)
        && IsSymbol(ft,klogical);
  }
  else
    return False;
}

static void posTst(cellpo input,int depth,int *dp,lblpo LF,
                   dictpo dict,dictpo *ndict,dictpo outer,cpo code,int dLvl);
static void negTst(cellpo input,int depth,lblpo LF,
                   dictpo dict,dictpo outer,cpo code,int dLvl);

void compTest(cellpo input,int depth,int *dp,lblpo LF,
	      dictpo dict,dictpo *ndict,dictpo outer,cpo code,int dLvl)
{
  int junk;
  dictpo jd;
  int *dd = (dp!=NULL?dp:&junk);
  dictpo *nd = (ndict!=NULL?ndict:&jd);
  
  posTst(input,depth,dd,LF,dict,nd,outer,code,dLvl);
}

/*
 * This test is looking for success to continue 
 */
static void posTst(cellpo input,int depth,int *dp,lblpo LF,
                   dictpo dict,dictpo *ndict,dictpo outer,cpo code,int dLvl)
{
  cellpo lhs,rhs,llhs,lrhs;

  *dp = depth;
  *ndict = dict;

#ifdef _COMPTRACE
  if(traceCompile)
    outMsg(aprilLogFile,"Compile test (positive) %5w\n",input);
#endif

  if(IsHashStruct(input,kdebug,&input))
    posTst(input,depth,dp,LF,dict,ndict,outer,code,dLvl+1);
  else if(IsHashStruct(input,knodebug,&input))
    posTst(input,depth,dp,LF,dict,ndict,outer,code,dLvl-1);
  else if(isSymb(input)){
    symbpo f=symVal(input);

    if(f==ktrue)
      return;			/* Nothing to do for a true test */
    else if(f==kfalse)
      genIns(code,NULL,jmp,LF);
    else{
      int E=0;
      input = foldExp(input,input,dict);
      compExp(input,NULL,&E,depth,&depth,dict,outer,code,dLvl);
      genIns(code,NULL,iffalse,E,LF);
    }
  }
  else if(IsEnumStruct(input,&input)){
    symbpo f=symVal(input);

    if(f==ktrue)
      return;			/* Nothing to do for a true test */
    else if(f==kfalse)
      genIns(code,NULL,jmp,LF);
    else
      syserr("problem in compiling test");
  }

  else if(isBinaryCall(input,kmatch,&lhs,&rhs)){
    int E=0;
    cellpo eType = typeInfo(rhs); /* get the type of the expression */

    assert(eType!=NULL);

    rhs = foldExp(rhs,rhs,dict); /* fold the expression ... */

    compExp(rhs,eType,&E,depth,&depth,dict,outer,code,dLvl);

    compPttrn(lhs,E,depth,dp,LF,dict,ndict,dict,outer,code,singleAss,dLvl);
    if(dLvl>0)
      genPtnVarDebug(code,*dp,*ndict,dict,dict);
  }

  else if(isBinaryCall(input,knomatch,&lhs,&rhs)){
    lblpo L0=NewLabel(code);
    cellpo eType = typeInfo(rhs); /* get the type of the expression */
    int E=0;
    dictpo jd;

    assert(eType!=NULL);

    rhs = foldExp(rhs,rhs,dict); /* fold the expression ... */

    compExp(rhs,eType,&E,depth,&depth,dict,outer,code,dLvl);
    compPttrn(lhs,E,depth,&depth,L0,dict,&jd,dict,outer,code,singleAss,dLvl);
    genIns(code,NULL,jmp,LF);
    DefLabel(code,L0);
  }

  else if(isBinaryCall(input,kequal,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);

    rhs = foldExp(stripCell(rhs),rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    if(SimpleLiteral(lhs,dict) && SimpleLiteral(rhs,dict)){
      if(!sameCell(lhs,rhs))
	genIns(code,"constant failure",jmp,LF);
    }
    else{
      int E1=0, E2=0;

      compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
      compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);
      if(IsSymbol(lType,knumberTp))
	genIns(code,NULL,fneq,E1,E2);
      else
	genIns(code,NULL,neq,E1,E2);
      genIns(code,NULL,jmp,LF);
    }
  }

  else if(isBinaryCall(input,knotequal,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);

    rhs = foldExp(rhs,rhs,dict);	/* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    if(SimpleLiteral(lhs,dict) && SimpleLiteral(rhs,dict)){
      if(sameCell(lhs,rhs))
	genIns(code,"constant failure",jmp,LF);
    }
    else{
      int E1=0, E2=0;

      compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
      compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

      if(IsSymbol(lType,knumberTp))
	genIns(code,NULL,feq,E1,E2);
      else
	genIns(code,NULL,eq,E1,E2);

      genIns(code,NULL,jmp,LF);
    }
  }
  else if(isBinaryCall(input,kgreaterthan,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);
    int E1=0, E2=0;

    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
    compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fle,E1,E2);
    else
      genIns(code,NULL,le,E1,E2);

    genIns(code,NULL,jmp,LF);
  }
  else if(isBinaryCall(input,klessthan,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);
    int E1=0, E2=0;

    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
    compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fle,E2,E1);
    else
      genIns(code,NULL,le,E2,E1);

    genIns(code,NULL,jmp,LF);
  }

  else if(isBinaryCall(input,klesseq,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);
    int E1=0, E2=0;

    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
    compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fgt,E1,E2);
    else
      genIns(code,NULL,gt,E1,E2);

    genIns(code,NULL,jmp,LF);
  }
  else if(isBinaryCall(input,kgreatereq,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);
    int E1=0, E2=0;

    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
    compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fgt,E2,E1);
    else
      genIns(code,NULL,gt,E2,E1);

    genIns(code,NULL,jmp,LF);
  }

  else if(isBinaryCall(input,kin,&lhs,&rhs) && /* ptn in low..high */
	  isBinaryCall(rhs,kdots,&llhs,&lrhs)){
    cellpo lType = typeInfo(llhs);
    cellpo rType = typeInfo(lrhs);
    int E1=0, E2=0, E=0;

    lrhs = foldExp(lrhs,lrhs,dict); /* fold the bounds expressions ... */
    llhs = foldExp(llhs,llhs,dict);

    compExp(lhs,NULL,&E,depth,&depth,dict,outer,code,dLvl);
    compExp(llhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fgt,E1,E);
    else
      genIns(code,NULL,gt,E1,E);

    genIns(code,NULL,jmp,LF);

    compExp(lrhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(rType,knumberTp))
      genIns(code,NULL,fgt,E,E2);
    else
      genIns(code,NULL,gt,E,E2);

    genIns(code,NULL,jmp,LF);
  }

  else if(isBinaryCall(input,kin,&lhs,&rhs)){ /* ptn in set */
    int Lst;
    int EL;
    int DP;
    lblpo L0=NewLabel(code);
    lblpo L1=NewLabel(code);
    lblpo L2=NewLabel(code);
    logical safe = False;

    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    dict=EmbeddedVars(lhs,depth,dp,safe,singleAss,False,dict,dict,outer,code);

    DP = *dp;
    Lst= --DP;

    if(!safe)
      genIns(code,NULL,initv,Lst);

    compExp(rhs,NULL,&Lst,DP,&DP,dict,outer,code,dLvl);

    EL=--DP;			/* element from tuple */
    if(!safe)
      genIns(code,NULL,initv,EL);
    genIns(code,"Start by testing end of list",jmp,L1);
    DefLabel(code,L0);
    genIns(code,"pick up element of list",ulst,Lst,EL,Lst);

    compPttrn(lhs,EL,DP,&DP,L1,dict,ndict,dict,outer,code,singleAss,dLvl);

    genIns(code,"we have success",jmp,L2);
    DefLabel(code,L1);
    genIns(code,"test of empty list",mnil,Lst,L0);
    genIns(code,"failed in test",jmp,LF);
    DefLabel(code,L2);
    if(dLvl>0)
      genPtnVarDebug(code,DP,*ndict,dict,dict);
  }

  else if(isBinaryCall(input,kand,&lhs,&rhs) || 
	  isBinaryCall(input,kdand,&lhs,&rhs)){
    posTst(lhs,depth,&depth,LF,dict,&dict,outer,code,dLvl);
    posTst(rhs,depth,dp,LF,dict,ndict,outer,code,dLvl);
  }
  else if(isBinaryCall(input,kor,&lhs,&rhs) ||
	  isBinaryCall(input,kdor,&lhs,&rhs)){
    lblpo L0 = NewLabel(code);
    lblpo Lx = NewLabel(code);
    dictpo idict=dict;
    int DP;

    posTst(lhs,depth,&DP,Lx,dict,&idict,outer,code,dLvl);
    genIns(code,"early sucess",jmp,L0);

    DefLabel(code,Lx);
    posTst(rhs,depth,&DP,LF,dict,&idict,outer,code,dLvl);

    clearDict(idict,dict);
    DefLabel(code,L0);
  }
  /* Negated test */
  else if(isUnaryCall(input,knot,&lhs) || isUnaryCall(input,kdnot,&lhs))
    negTst(lhs,depth,LF,dict,outer,code,dLvl);
  else{				/* compile the expression and test result */
    int E=0;
    compExp(input,NULL,&E,depth,&depth,dict,outer,code,dLvl);
    
    genIns(code,NULL,iffalse,E,LF);
  }
}

static void negTst(cellpo input,int depth,lblpo LF,
                   dictpo dict,dictpo outer,cpo code,int dLvl)
{
  cellpo lhs,rhs,llhs,lrhs;

  if(isSymb(input)){
    symbpo f=symVal(input);
      
    if(f==kfalse)
      return;
    else if(f==ktrue)
      genIns(code,NULL,jmp,LF);
    else{
      int E=0;
	
      compExp(input,NULL,&E,depth,&depth,dict,outer,code,dLvl);
      genIns(code,"test for truth of expression",iftrue,E,LF);
    }
  }
  else if(IsEnumStruct(input,&lhs)){ /* only true or false possible */
    symbpo f=symVal(input);
    
    if(f==kfalse)
      return;
    else if(f==ktrue)
      genIns(code,NULL,jmp,LF);
    else
      syserr("problem in compiling test");
  }
  else if(isBinaryCall(input,kmatch,&lhs,&rhs)){
    lblpo L0=NewLabel(code);
    cellpo eType = typeInfo(rhs); /* get the type of the expression */
    int E=0;
    dictpo dd=dict;

    assert(eType!=NULL);

    rhs = foldExp(rhs,rhs,dict); /* fold the expression ... */

    compExp(rhs,eType,&E,depth,&depth,dict,outer,code,dLvl);
    compPttrn(lhs,E,depth,&depth,L0,dict,&dd,dict,outer,code,singleAss,dLvl);

    clearDict(dd,dict);
    if(dLvl>0)
      genPtnVarDebug(code,depth,dd,dict,dict);

    genIns(code,NULL,jmp,LF);
    DefLabel(code,L0);
  }

  else if(isBinaryCall(input,knomatch,&lhs,&rhs)){
    int E=0;
    cellpo eType = typeInfo(rhs); /* get the type of the expression */
    int dp;
    dictpo ndict;

    assert(eType!=NULL);

    rhs = foldExp(rhs,rhs,dict); /* fold the expression ... */

    compExp(rhs,eType,&E,depth,&depth,dict,outer,code,dLvl);
      
    compPttrn(lhs,E,depth,&dp,LF,dict,&ndict,dict,outer,code,singleAss,dLvl);
    if(dLvl>0)
      genPtnVarDebug(code,dp,ndict,dict,dict);
    }

  else if(isBinaryCall(input,kequal,&lhs,&rhs)){
    cellpo test = BuildBinCall(knotequal,lhs,rhs,allocSingle()); /* =!=(lhs,rhs) */

    posTst(test,depth,&depth,LF,dict,&dict,outer,code,dLvl);
  }
  else if(isBinaryCall(input,knotequal,&lhs,&rhs)){
    cellpo test = BuildBinCall(kequal,lhs,rhs,allocSingle());

    posTst(test,depth,&depth,LF,dict,&dict,outer,code,dLvl);
  }
  else if(isBinaryCall(input,kgreaterthan,&lhs,&rhs)){
    cellpo test = BuildBinCall(klesseq,lhs,rhs,allocSingle()); /* <=(lhs,rhs) */

    posTst(test,depth,&depth,LF,dict,&dict,outer,code,dLvl);
  }
  else if(isBinaryCall(input,klessthan,&lhs,&rhs)){
    cellpo test = BuildBinCall(kgreatereq,lhs,rhs,allocSingle());	/* >=(lhs,rhs) */

    posTst(test,depth,&depth,LF,dict,&dict,outer,code,dLvl);
  }
  else if(isBinaryCall(input,klesseq,&lhs,&rhs)){
    cellpo test = BuildBinCall(kgreaterthan,lhs,rhs,allocSingle());	/* >(lhs,rhs) */

    posTst(test,depth,&depth,LF,dict,&dict,outer,code,dLvl);
  }
  else if(isBinaryCall(input,kgreatereq,&lhs,&rhs)){
    cellpo test = BuildBinCall(klessthan,lhs,rhs,allocSingle());	/* <(lhs,rhs) */

    posTst(test,depth,&depth,LF,dict,&dict,outer,code,dLvl);
  }

  else if(isBinaryCall(input,kin,&lhs,&rhs) && /* ptn in low..high */
          isBinaryCall(rhs,kdots,&llhs,&lrhs)){
    cellpo lType = typeInfo(llhs);
    cellpo rType = typeInfo(lrhs);
    int E1=0, E2=0, E=0;
    lblpo L0=NewLabel(code);

    lrhs = foldExp(lrhs,lrhs,dict); /* fold the bounds expressions ... */
    llhs = foldExp(llhs,llhs,dict);

    compExp(lhs,NULL,&E,depth,&depth,dict,outer,code,dLvl);
    compExp(llhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fgt,E1,E);
    else
      genIns(code,NULL,gt,E1,E);

    genIns(code,NULL,jmp,L0);

    compExp(lrhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);
      
    if(IsSymbol(rType,knumberTp))
      genIns(code,NULL,fle,E,E2);
    else
      genIns(code,NULL,le,E,E2);
    
    genIns(code,NULL,jmp,LF);
    DefLabel(code,L0);
  }

  else if(isBinaryCall(input,kin,&lhs,&rhs)){ /* remember, this fails if lhs in rhs! */
    int Lst;
    int EL;
    int DP;
    lblpo L0=NewLabel(code);
    lblpo L1=NewLabel(code);
    logical safe = False;
    dictpo ndict=EmbeddedVars(lhs,depth,&DP,safe,singleAss,False,dict,dict,outer,code);
    dictpo nd;
      
    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    Lst = --DP;
    genIns(code,NULL,initv,Lst);

    compExp(rhs,NULL,&Lst,DP,&DP,dict,outer,code,dLvl);

    EL=--DP;
    genIns(code,NULL,initv,EL);

    DefLabel(code,L0);

    genIns(code,"pick up head of list",mlist,Lst,EL,Lst);
    genIns(code,"test for empty list",jmp,L1);

    compPttrn(lhs,EL,DP,&DP,L0,ndict,&nd,dict,outer,code,singleAss,dLvl);
    if(dLvl>0)
      genPtnVarDebug(code,DP,nd,dict,dict);

    genIns(code,NULL,jmp,LF);
    DefLabel(code,L1);
  }

  else if(isBinaryCall(input,kand,&lhs,&rhs) || isBinaryCall(input,kdand,&lhs,&rhs)){
    lblpo L0 = NewLabel(code);
    int dp;
    dictpo idict;

    posTst(input,depth,&dp,L0,dict,&idict,outer,code,dLvl);
    clearDict(idict,dict);
    genIns(code,NULL,jmp,LF);
    DefLabel(code,L0);
  }

  else if(isBinaryCall(input,kor,&lhs,&rhs) || isBinaryCall(input,kdor,&lhs,&rhs)){
    lblpo L0 = NewLabel(code);
    lblpo L1 = NewLabel(code);
    int dp;
    dictpo idict;

    posTst(lhs,depth,&dp,L1,dict,&idict,outer,code,dLvl);
    genIns(code,NULL,jmp,LF);
    DefLabel(code,L1);
    posTst(rhs,depth,&dp,L0,dict,&idict,outer,code,dLvl);
    clearDict(idict,dict);
    genIns(code,NULL,jmp,LF);
    DefLabel(code,L0);
  }

  else if(isUnaryCall(input,knot,&lhs) || isUnaryCall(input,kdnot,&lhs)){
    lblpo L0 = NewLabel(code);
    int dp;
    dictpo idict;

    posTst(lhs,depth,&dp,L0,dict,&idict,outer,code,dLvl);
    clearDict(idict,dict);
    genIns(code,NULL,jmp,LF);
    DefLabel(code,L0);
  }
  else{
    int E=0;
    compExp(input,NULL,&E,depth,&depth,dict,outer,code,dLvl);

    genIns(code,NULL,iftrue,E,LF);
  }
}

static void compTst(cellpo input,int depth,int *dp,lblpo LF,
		    dictpo dict,dictpo *ndict,dictpo outer,cpo code,int dLvl,
		    logical negated)
{
  cellpo lhs,rhs,llhs,lrhs;

  if(ndict!=NULL)
    *ndict=dict;		/* default -- no new dictionary */

  if(dp!=NULL)
    *dp=depth;

#ifdef _COMPTRACE
  if(traceCompile)
    outMsg(aprilLogFile,"Compile test %5w\n",input);
#endif

  if(IsHashStruct(input,kdebug,&input))
    compTst(input,depth,dp,LF,dict,ndict,outer,code,dLvl+1,negated);
  else if(IsHashStruct(input,knodebug,&input))
    compTst(input,depth,dp,LF,dict,ndict,outer,code,dLvl-1,negated);
  else if(isSymb(input)){
    symbpo f=symVal(input);

    if(f==ktrue)
      return;			/* Nothing to do for a true test */
    else if(f==kfalse)
      genIns(code,NULL,jmp,LF);
    else{
      int E=0;
      input = foldExp(input,input,dict);
      compExp(input,NULL,&E,depth,&depth,dict,outer,code,dLvl);
      genIns(code,NULL,iffalse,E,LF);
    }
  }
  else if(IsEnumStruct(input,&input)){
    symbpo f=symVal(input);

    if(f==ktrue)
      return;			/* Nothing to do for a true test */
    else if(f==kfalse)
      genIns(code,NULL,jmp,LF);
    else
      syserr("problem in compiling test");
  }

  else if(isBinaryCall(input,kmatch,&lhs,&rhs)){
    int E=0;
    cellpo eType = typeInfo(rhs); /* get the type of the expression */

    assert(eType!=NULL);

    rhs = foldExp(rhs,rhs,dict); /* fold the expression ... */

    compExp(rhs,eType,&E,depth,&depth,dict,outer,code,dLvl);

    compPttrn(lhs,E,depth,dp,LF,dict,ndict,dict,outer,code,
	      singleAss,dLvl);
    if(dLvl>0)
      genPtnVarDebug(code,*dp,*ndict,dict,dict);
  }

  else if(isBinaryCall(input,knomatch,&lhs,&rhs)){
    lblpo L0=NewLabel(code);
    cellpo eType = typeInfo(rhs); /* get the type of the expression */
    int E=0;

    assert(eType!=NULL);

    rhs = foldExp(rhs,rhs,dict); /* fold the expression ... */

    compExp(rhs,eType,&E,depth,&depth,dict,outer,code,dLvl);
    compPttrn(lhs,E,depth,dp,L0,dict,ndict,dict,outer,code,
	      singleAss,dLvl);
    if(dLvl>0)
      genPtnVarDebug(code,*dp,*ndict,dict,dict);
    genIns(code,NULL,jmp,LF);
    DefLabel(code,L0);
  }

  else if(isBinaryCall(input,kequal,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);

    rhs = foldExp(stripCell(rhs),rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    if(SimpleLiteral(lhs,dict) && SimpleLiteral(rhs,dict)){
      if(!sameCell(lhs,rhs))
	genIns(code,"constant failure",jmp,LF);
    }
    else{
      int E1=0, E2=0;

      compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
      compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);
      if(IsSymbol(lType,knumberTp))
	genIns(code,NULL,fneq,E1,E2);
      else
	genIns(code,NULL,neq,E1,E2);
      genIns(code,NULL,jmp,LF);
    }
  }
  else if(isBinaryCall(input,knotequal,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);

    rhs = foldExp(rhs,rhs,dict);	/* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    if(SimpleLiteral(lhs,dict) && SimpleLiteral(rhs,dict)){
      if(sameCell(lhs,rhs))
	genIns(code,"constant failure",jmp,LF);
    }
    else{
      int E1=0, E2=0;

      compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
      compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

      if(IsSymbol(lType,knumberTp))
	genIns(code,NULL,feq,E1,E2);
      else
	genIns(code,NULL,eq,E1,E2);

      genIns(code,NULL,jmp,LF);
    }
  }
  else if(isBinaryCall(input,kgreaterthan,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);
    int E1=0, E2=0;

    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
    compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fle,E1,E2);
    else
      genIns(code,NULL,le,E1,E2);

    genIns(code,NULL,jmp,LF);
  }
  else if(isBinaryCall(input,klessthan,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);
    int E1=0, E2=0;

    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
    compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fle,E2,E1);
    else
      genIns(code,NULL,le,E2,E1);

    genIns(code,NULL,jmp,LF);
  }

  else if(isBinaryCall(input,klesseq,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);
    int E1=0, E2=0;

    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
    compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fgt,E1,E2);
    else
      genIns(code,NULL,gt,E1,E2);

    genIns(code,NULL,jmp,LF);
  }
  else if(isBinaryCall(input,kgreatereq,&lhs,&rhs)){
    cellpo lType = typeInfo(lhs);
    int E1=0, E2=0;

    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    compExp(lhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);
    compExp(rhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fgt,E2,E1);
    else
      genIns(code,NULL,gt,E2,E1);

    genIns(code,NULL,jmp,LF);
  }

  else if(isBinaryCall(input,kin,&lhs,&rhs) && /* ptn in low..high */
	  isBinaryCall(rhs,kdots,&llhs,&lrhs)){
    cellpo lType = typeInfo(llhs);
    cellpo rType = typeInfo(lrhs);
    int E1=0, E2=0, E=0;

    lrhs = foldExp(lrhs,lrhs,dict); /* fold the bounds expressions ... */
    llhs = foldExp(llhs,llhs,dict);

    compExp(lhs,NULL,&E,depth,&depth,dict,outer,code,dLvl);
    compExp(llhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(lType,knumberTp))
      genIns(code,NULL,fgt,E1,E);
    else
      genIns(code,NULL,gt,E1,E);

    genIns(code,NULL,jmp,LF);

    compExp(lrhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);

    if(IsSymbol(rType,knumberTp))
      genIns(code,NULL,fgt,E,E2);
    else
      genIns(code,NULL,gt,E,E2);

    genIns(code,NULL,jmp,LF);
  }

  else if(isBinaryCall(input,kin,&lhs,&rhs)){ /* ptn in set */
    int Lst;
    int EL;
    int DP;
    lblpo L0=NewLabel(code);
    lblpo L1=NewLabel(code);
    lblpo L2=NewLabel(code);
    logical safe = False;

    rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
    lhs = foldExp(lhs,lhs,dict);

    dict=EmbeddedVars(lhs,depth,dp,safe,singleAss,False,dict,dict,outer,code);

    DP = *dp;
    Lst= --DP;

    if(!safe)
      genIns(code,NULL,initv,Lst);

    compExp(rhs,NULL,&Lst,DP,&DP,dict,outer,code,dLvl);

    EL=--DP;			/* element from tuple */
    if(!safe)
      genIns(code,NULL,initv,EL);
    genIns(code,"Start by testing end of list",jmp,L1);
    DefLabel(code,L0);
    genIns(code,"pick up element of list",ulst,Lst,EL,Lst);

    compPttrn(lhs,EL,DP,&DP,L1,dict,ndict,dict,outer,code,singleAss,dLvl);

    genIns(code,"we have success",jmp,L2);
    DefLabel(code,L1);
    genIns(code,"test of empty list",mnil,Lst,L0);
    genIns(code,"failed in test",jmp,LF);
    DefLabel(code,L2);
    if(dLvl>0)
      genPtnVarDebug(code,DP,*ndict,dict,dict);
  }

  else if(isBinaryCall(input,kand,&lhs,&rhs) || 
	  isBinaryCall(input,kdand,&lhs,&rhs)){
    compTst(lhs,depth,&depth,LF,dict,&dict,outer,code,dLvl,negated);
    compTst(rhs,depth,dp,LF,dict,ndict,outer,code,dLvl,negated);
  }
  else if(isBinaryCall(input,kor,&lhs,&rhs) ||
	  isBinaryCall(input,kdor,&lhs,&rhs)){
    lblpo L0 = NewLabel(code);
    cellpo ntcall = BuildUnaryCall(knot,lhs,allocSingle()); /* not(lhs) */
    dictpo idict;
    int DP;

    compTst(ntcall,depth,&DP,L0,dict,&idict,outer,code,dLvl,!negated);

    dict=idict;
    compTst(rhs,depth,&DP,LF,idict,&idict,outer,code,dLvl,negated);

    if(negated)
      clearDict(idict,dict);
    else
      *ndict=idict;
    DefLabel(code,L0);
  }
  /* Negated test */
  else if(isUnaryCall(input,knot,&lhs) || isUnaryCall(input,kdnot,&lhs)){
    if(isSymb(lhs)){
      symbpo f=symVal(lhs);
      
      if(f==kfalse)
	return;
      else if(f==ktrue)
	genIns(code,NULL,jmp,LF);
      else{
	int E=0;
	
	compExp(lhs,NULL,&E,depth,&depth,dict,outer,code,dLvl);
	genIns(code,"test for truth of expression",iftrue,E,LF);
      }
    }
    else if(IsEnumStruct(lhs,&lhs)){ /* only true or false possible */
      symbpo f=symVal(lhs);
      
      if(f==kfalse)
	return;
      else if(f==ktrue)
	genIns(code,NULL,jmp,LF);
      else
	syserr("problem in compiling test");
    }
    else if(isBinaryCall(lhs,kmatch,&lhs,&rhs)){
      lblpo L0=NewLabel(code);
      cellpo eType = typeInfo(rhs); /* get the type of the expression */
      int E=0;
      dictpo dd=dict;
      dictpo *nd = negated?&dd:ndict;

      assert(eType!=NULL);

      rhs = foldExp(rhs,rhs,dict); /* fold the expression ... */

      compExp(rhs,eType,&E,depth,&depth,dict,outer,code,dLvl);
      compPttrn(lhs,E,depth,dp,L0,dict,nd,dict,outer,code,singleAss,dLvl);
      if(negated)
	clearDict(dd,dict);
      if(dLvl>0)
	genPtnVarDebug(code,*dp,*nd,dict,dict);

      genIns(code,NULL,jmp,LF);
      DefLabel(code,L0);
    }

    else if(isBinaryCall(lhs,knomatch,&lhs,&rhs)){
      int E=0;
      cellpo eType = typeInfo(rhs); /* get the type of the expression */

      assert(eType!=NULL);

      rhs = foldExp(rhs,rhs,dict); /* fold the expression ... */

      compExp(rhs,eType,&E,depth,&depth,dict,outer,code,dLvl);
      
      compPttrn(lhs,E,depth,dp,LF,dict,ndict,dict,outer,code,singleAss,dLvl);
      if(dLvl>0)
	genPtnVarDebug(code,*dp,*ndict,dict,dict);
    }

    else if(isBinaryCall(lhs,kequal,&lhs,&rhs)){
      cellpo test = BuildBinCall(knotequal,lhs,rhs,allocSingle()); /* =!=(lhs,rhs) */

      compTst(test,depth,dp,LF,dict,ndict,outer,code,dLvl,negated);
    }
    else if(isBinaryCall(lhs,knotequal,&lhs,&rhs)){
      cellpo test = BuildBinCall(kequal,lhs,rhs,allocSingle());

      compTst(test,depth,dp,LF,dict,ndict,outer,code,dLvl,negated);
    }
    else if(isBinaryCall(lhs,kgreaterthan,&lhs,&rhs)){
      cellpo test = BuildBinCall(klesseq,lhs,rhs,allocSingle()); /* <=(lhs,rhs) */

      compTst(test,depth,dp,LF,dict,ndict,outer,code,dLvl,negated);
    }
    else if(isBinaryCall(lhs,klessthan,&lhs,&rhs)){
      cellpo test = BuildBinCall(kgreatereq,lhs,rhs,allocSingle());	/* >=(lhs,rhs) */

      compTst(test,depth,dp,LF,dict,ndict,outer,code,dLvl,negated);
    }
    else if(isBinaryCall(lhs,klesseq,&lhs,&rhs)){
      cellpo test = BuildBinCall(kgreaterthan,lhs,rhs,allocSingle());	/* >(lhs,rhs) */

      compTst(test,depth,dp,LF,dict,ndict,outer,code,dLvl,negated);
    }
    else if(isBinaryCall(lhs,kgreatereq,&lhs,&rhs)){
      cellpo test = BuildBinCall(klessthan,lhs,rhs,allocSingle());	/* <(lhs,rhs) */

      compTst(test,depth,dp,LF,dict,ndict,outer,code,dLvl,negated);
    }


    else if(isBinaryCall(lhs,kin,&lhs,&rhs) && /* ptn in low..high */
	    isBinaryCall(rhs,kdots,&llhs,&lrhs)){
      cellpo lType = typeInfo(llhs);
      cellpo rType = typeInfo(lrhs);
      int E1=0, E2=0, E=0;
      lblpo L0=NewLabel(code);

      lrhs = foldExp(lrhs,lrhs,dict); /* fold the bounds expressions ... */
      llhs = foldExp(llhs,llhs,dict);

      compExp(lhs,NULL,&E,depth,&depth,dict,outer,code,dLvl);
      compExp(llhs,NULL,&E1,depth,&depth,dict,outer,code,dLvl);

      if(IsSymbol(lType,knumberTp))
	genIns(code,NULL,fgt,E1,E);
      else
	genIns(code,NULL,gt,E1,E);

      genIns(code,NULL,jmp,L0);

      compExp(lrhs,NULL,&E2,depth,&depth,dict,outer,code,dLvl);
      
      if(IsSymbol(rType,knumberTp))
	genIns(code,NULL,fle,E,E2);
      else
	genIns(code,NULL,le,E,E2);

      genIns(code,NULL,jmp,LF);
      DefLabel(code,L0);
    }

    else if(isBinaryCall(lhs,kin,&lhs,&rhs)){
      int Lst;
      int EL;
      int DP;
      lblpo L0=NewLabel(code);
      lblpo L1=NewLabel(code);
      logical safe = False;
      
      rhs = foldExp(rhs,rhs,dict); /* fold the expressions ... */
      lhs = foldExp(lhs,lhs,dict);

      dict=EmbeddedVars(lhs,depth,dp,safe,singleAss,False,dict,dict,outer,code);

      DP = *dp;
      Lst = --DP;

      if(!safe)
	genIns(code,NULL,initv,Lst);

      compExp(rhs,NULL,&Lst,DP,&DP,dict,outer,code,dLvl);

      EL=--DP;
      if(!safe)
	genIns(code,NULL,initv,EL);

      DefLabel(code,L0);

      genIns(code,"pick up head of list",mlist,Lst,EL,Lst);
      genIns(code,"test for empty list",jmp,L1);

      if(negated){
	dictpo idict=dict;
	compPttrn(lhs,EL,DP,&DP,L0,dict,&dict,dict,outer,code,singleAss,dLvl);
	if(dLvl>0)
	  genPtnVarDebug(code,DP,dict,idict,idict);
      }
      else{
	compPttrn(lhs,EL,DP,&DP,L0,dict,ndict,dict,outer,code,singleAss,dLvl);
	if(dLvl>0)
	  genPtnVarDebug(code,DP,*ndict,dict,dict);
      }

      genIns(code,NULL,jmp,LF);
      DefLabel(code,L1);
    }

    else if(isBinaryCall(lhs,kand,&lhs,&rhs) || 
	    isBinaryCall(lhs,kdand,&lhs,&rhs)){
      cellpo test = allocSingle();

      p_cell(lhs);
      pUnary(cnot);
      p_cell(rhs);
      pUnary(cnot);
      pBinary(cor);		/* !lhs || !rhs */
      popcell(test);
			
      compTst(test,depth,dp,LF,dict,ndict,outer,code,dLvl,negated);
    }
    else if(isBinaryCall(lhs,kor,&lhs,&rhs) ||
	    isBinaryCall(lhs,kdor,&lhs,&rhs)){
      cellpo test = allocSingle();

      p_cell(cnot);
      p_cell(lhs);
      p_cons(1);
      p_cell(cnot);
      p_cell(rhs);
      p_cons(1);
      pBinary(cand);		/* !lhs && !rhs */
      popcell(test);
			
      compTst(test,depth,dp,LF,dict,ndict,outer,code,dLvl,negated);
    }
    else if(isUnaryCall(lhs,knot,&llhs) || isUnaryCall(lhs,kdnot,&llhs))
      compTst(llhs,depth,dp,LF,dict,ndict,outer,code,dLvl,negated); /* double negation */
    else{
      int E=0;
      compExp(lhs,NULL,&E,depth,&depth,dict,outer,code,dLvl);

      genIns(code,NULL,iftrue,E,LF);
    }
  }
  else{				/* compile the expression and test result */
    int E=0;
    compExp(input,NULL,&E,depth,&depth,dict,outer,code,dLvl);
    
    genIns(code,NULL,iffalse,E,LF);
  }
}



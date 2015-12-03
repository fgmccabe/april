/*
  Expression code generator for April compiler
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
#include <assert.h>		/* Debugging assertion support */
#include "compile.h"		/* Compiler structures */
#include "generate.h"		/* Code generation interface */
#include "makesig.h"
#include "types.h"		/* Type cheking interface */
#include "dict.h"
#include "keywords.h"		/* Standard April keywords */
#include "opcodes.h"		/* April machine opcodes */
#include "assem.h"		/* Code generation*/
#include "encode.h"

/*
 * Forward declarations 
*/
static void genEscapeSymbol(cellpo esc,cellpo type,int *dest,int depth,int *dp,
			    dictpo dict, cpo code,int dLvl);


static void coerceExp(cellpo exp,cellpo caste,cellpo type,int *dest,int depth,
		      int *dp,dictpo dict,dictpo outer,cpo code,int dLvl);

static logical compRewrite(cellpo input,int arity,int srcvar,
			   int depth,lblpo Next,lblpo LF,
			   dictpo dict,cpo code,
			   logical last,int dLvl,void *cl);

static int genInitDest(int *dest,int depth,int *dp,cpo code);

typedef struct {
  int *dest;
} rewriteInfo;

/*
 * General code for an expression
 */
void compExp(cellpo input,cellpo type,int *dest,int depth,int *dp,
	     dictpo dict,dictpo outer,cpo code,int dLvl)
{
  cellpo fn,lhs,rhs;
  int DP;			/* throw away depth marker */
  unsigned long ar;

  *dp = depth;			/* default return depth */

#ifdef COMPTRACE_
  if(traceCompile)
    outMsg(logFile,"expression (depth=%d) `%5w'\n",depth,input);
#endif

  if(typeInfo(input)!=NULL)
    type=deRef(typeInfo(input));

  if(IsHashStruct(input,kdebug,&input))
    compExp(input,type,dest,depth,dp,dict,outer,code,dLvl+1);
  else if(IsHashStruct(input,knodebug,&input))
    compExp(input,type,dest,depth,dp,dict,outer,code,dLvl-1);
  else if(isSymb(input)){	/* look for keywords, variables etc.  */
    cellpo ci;
    vartype vt;
    centry et;
    int ind;			/* index variable */
    int off;			/* offset of variable */
    symbpo sy = symVal(input);
    initStat inited;

    if(IsSymbol(input,ktrue) || IsSymbol(input,kfalse)) /* truth value */
      genIns(code,NULL,movl,input,genDest(dest,depth,dp,code));

    else if(IsVar(input,&et,&ci,&off,&ind,NULL,&inited,&vt,dict)){
      switch(vt){
      case localvar:
	genMove(code,off,dest,symN(sy));
	if(off==*dest && inited==notInited)
	  genIns(code,"initialize",initv,off);
	break;
      case indexvar:
	genIns(code,symN(sy),indxfld,off,ind,genDest(dest,depth,dp,code));
	break;
      case envvar:
	genIns(code,symN(sy),emove,off,genDest(dest,depth,dp,code));
	break;
      case thetavar:
	genIns(code,"Theta variable",emove,off,genDest(dest,depth,dp,code));
	genIns(code,symN(sy),indxfld,*dest,ind,*dest);
	break;
      }
    }
    else if(IsEscape(sy,NULL))
      genEscapeSymbol(input,type,dest,depth,dp,dict,code,dLvl);
    else{			/* straight symbol */
      reportWarning(lineInfo(input),"unexpected symbol '%w'",input);
      genIns(code,NULL,movl,input,genDest(dest,depth,dp,code));
    }
  }
  else if(IsEnumStruct(input,&fn)){
    genIns(code,"enumerated symbol",movl,fn,genDest(dest,depth,dp,code));
  }
  else if(IsInt(input))
    genIns(code,NULL,movl,input,genDest(dest,depth,dp,code));
  else if(IsFloat(input))
    genIns(code,NULL,movl,input,genDest(dest,depth,dp,code));
  else if(isChr(input))
    genIns(code,NULL,movl,input,genDest(dest,depth,dp,code));
  else if(IsString(input))	/* a string literal ... */
    genIns(code,NULL,movl,input,genDest(dest,depth,dp,code));
  else if(IsQuote(input,&input)){ /* quoted expressions */
    if(isSymb(input))
      genIns(code,NULL,movl,input,genDest(dest,depth,dp,code));
    else
      reportErr(lineInfo(input),"invalid quoted expression `%4.1800w'",input);
  }

  /* Handle standard operators and normal function calls */

  /* type cast to type any */
  else if(IsAnyStruct(input,&rhs))
    anyExp(rhs,dest,depth,dp,dict,outer,code,dLvl);

  else if(isBinaryCall(input,ktcoerce,&lhs,&rhs)) /* type coercion expression */
    coerceExp(rhs,lhs,type,dest,depth,dp,dict,outer,code,dLvl);

  else if(isUnaryCall(input,kvalueof,&rhs)){ /* valof expression */
    lblpo L0 = NewLabel(code);
    lblpo Lx = NewLabel(code);
    context ctxt;
    int DP;
    cont cnt;
    dictpo ndict=dict;
	
    ctxt.ct=valof;		/* set up the right context */
    ctxt.dest=genInitDest(dest,depth,dp,code);
    ctxt.brk=L0;
    ctxt.valCont=NULL;
    ctxt.prev = NULL;

    if(dLvl>0)
      genScopeDebug(code,*dp,ndict);

    compStmt(rhs,*dp,&DP,&ctxt,L0,dict,&ndict,outer,&cnt,code,dLvl);

    clearDict(ndict,dict);
    DefLabel(code,L0);
    if(dLvl>0)
      genScopeDebug(code,DP,ndict);
    genIns(code,"Test for void",mlit,ctxt.dest,voidcell);
    genIns(code,"We are ok",jmp,Lx);
    genError(input,"failed to set valis",cfailed,DP,dict,code,dLvl);
    DefLabel(code,Lx);
  }
  
  else if(isUnaryCall(input,kcollect,&rhs)){ /* collect expression */
    lblpo L0 = NewLabel(code);
    int d = genDest(dest,depth,dp,code);
    context ctxt;
    int DP;
    cont cnt;
    dictpo ndict=dict;
    int tmpV;                   // Temporary tail variable
	
    if(depth==*dp){		/* Not a new variable ... */
      DP = d = --depth;		/* grab a new space */
      genIns(code,"initialize copy",initv,d);
    }
    else
      DP = *dp;
      
    tmpV = --DP;
	
    ctxt.ct=collect;		/* set up the right context */
    ctxt.dest=tmpV;
    ctxt.valCont=NULL;
    ctxt.prev = NULL;		/* No prior context allowed */
	
    if(dLvl>0)
      genScopeDebug(code,DP,ndict);

    genIns(code,"start set",movl,nilcell,d); /* start the result list */
    genIns(code,"overhead list pair",lstpr,d,d,d);
    genIns(code,"set up temp pointer",move,d,tmpV);
    compStmt(rhs,DP,&DP,&ctxt,L0,dict,&ndict,outer,&cnt,code,dLvl);
    DefLabel(code,L0);
    genIns(code,"extract set",ulst,d,tmpV,*dest);

    if(dLvl>0)
      genScopeDebug(code,*dp,ndict);

    clearDict(ndict,dict);
  }
      
  else if(isBinaryCall(input,kelse,&lhs,&rhs)){
    cellpo test,then;

    if(isBinaryCall(lhs,kthen,&test,&then) && isUnaryCall(test,kif,&test)){
      lblpo L0=NewLabel(code);
      lblpo Lx=NewLabel(code);
      dictpo nd;
      int DP;
      cellpo t;

      if(dLvl>0)
	genLineDebugInfo(code,depth,test,True); /* generate debugging info */

      test = foldExp(test,test,dict);

      if(IsSymbol(test,ktrue) ||
	 (IsEnumStruct(test,&t) && IsSymbol(t,ktrue)))
	compExp(foldExp(then,then,dict),type,dest,depth,dp,dict,outer,code,dLvl);
      else if(IsSymbol(test,kfalse) ||
	      (IsEnumStruct(test,&t) && IsSymbol(t,kfalse)))
	compExp(foldExp(rhs,rhs,dict),type,dest,depth,dp,dict,outer,code,dLvl);
      else{
	int D=genInitDest(dest,depth,dp,code);
	
	compTest(test,*dp,&DP,L0,dict,&nd,outer,code,dLvl);

	compExp(foldExp(then,then,nd),type,&D,DP,&DP,nd,outer,code,dLvl);
	genIns(code,"exit from then",jmp,Lx); /* we are done */

	clearDict(nd,dict);	
	DefLabel(code,L0);

	compExp(foldExp(rhs,rhs,dict),type,&D,*dp,&DP,dict,outer,code,dLvl);
	DefLabel(code,Lx);
      }
    }
    else
      reportErr(lineInfo(input),"Badly formed conditional `%4.1800w'",input);
  }

  /* Check for logical test condition */
  else if(isBinaryCall(input,kand,NULL,NULL)||
	  isBinaryCall(input,kor,NULL,NULL)||
	  isBinaryCall(input,kdand,NULL,NULL)||
	  isBinaryCall(input,kdor,NULL,NULL)||
	  isUnaryCall(input,knot,NULL)||
	  isUnaryCall(input,kdnot,NULL)||
	  isBinaryCall(input,kmatch,NULL,NULL)||
	  isBinaryCall(input,knomatch,NULL,NULL)||
	  isBinaryCall(input,kequal,NULL,NULL)||
	  isBinaryCall(input,knotequal,NULL,NULL)||
	  isBinaryCall(input,kgreaterthan,NULL,NULL)||
	  isBinaryCall(input,kgreatereq,NULL,NULL)||
	  isBinaryCall(input,klessthan,NULL,NULL)||
	  isBinaryCall(input,klesseq,NULL,NULL)||
	  isBinaryCall(input,kin,NULL,NULL)){
    lblpo L0=NewLabel(code);
    lblpo LF=NewLabel(code);
    int D=genDest(dest,depth,dp,code);
    int DP;
    dictpo nd;
	  
    compTest(input,depth,&DP,LF,dict,&nd,outer,code,dLvl);
    genIns(code,"succeeded",movl,ctrue,D);
    genIns(code,NULL,jmp,L0);
    DefLabel(code,LF);
    genIns(code,"failed",movl,cfalse,D);
    DefLabel(code,L0);
    clearDict(nd,dict);
  }

  /* Specific cases of arithmetic expressions */
  else if(isBinaryCall(input,kplus,&lhs,&rhs)){
    if(IsInteger(lhs) && abs(integerVal(lhs))<128){
      int D = genDest(dest,depth,dp,code);
      int D2 = 0;
	  
      compExp(rhs,type,&D2,depth,&DP,dict,outer,code,dLvl);

      if(D2==DP)
	genIns(code,NULL,incr,D2,integerVal(lhs),D);
      else{
	int M = --DP;
	genMove(code,D2,&M,"extra move");
	genIns(code,NULL,incr,M,integerVal(lhs),D);
      }
    }

    else if(IsInteger(rhs) && abs(integerVal(rhs))<128){
      int D = genDest(dest,depth,dp,code);
      int D2 = 0;
	  
      compExp(lhs,type,&D2,depth,&DP,dict,outer,code,dLvl);

      if(D2==DP)
	genIns(code,NULL,incr,D2,integerVal(rhs),D);
      else{
	int M = --DP;
	genMove(code,D2,&M,"extra move");
	genIns(code,NULL,incr,M,integerVal(rhs),D);
      }
    }
    else{
      int D = genInitDest(dest,depth,dp,code);
      int D1 = 0;
      int D2 = 0;
	  
      compExp(lhs,type,&D1,*dp,&DP,dict,outer,code,dLvl);
      compExp(rhs,type,&D2,DP,&DP,dict,outer,code,dLvl);

      if(D1==DP || D2==DP)
	genIns(code,NULL,plus,D1,D2,D);
      else{
	int M = --DP;
	genMove(code,D1,&M,"extra move");
	genIns(code,NULL,plus,M,D2,D);
      }
    }
  }

  else if(isUnaryCall(input,kminus,&rhs)){
    int D = genInitDest(dest,depth,dp,code);
    int D1 = 0;
    int D2 = 0;
	  
    compExp(c_zero,numberTp,&D1,*dp,&DP,dict,outer,code,dLvl);
    compExp(rhs,type,&D2,DP,&DP,dict,outer,code,dLvl);

    if(D1==DP || D2==DP)
      genIns(code,NULL,minus,D1,D2,D);
    else{
      int M = --DP;
      genMove(code,D1,&M,"extra move");
      genIns(code,NULL,minus,M,D2,D);
    }
  }
  else if(isBinaryCall(input,kminus,&lhs,&rhs)){
    if(IsInteger(rhs) && abs(integerVal(rhs))<128){
      int D = genDest(dest,depth,dp,code);
      int D2 = 0;
	  
      compExp(lhs,type,&D2,depth,&DP,dict,outer,code,dLvl);

      if(D2==DP)
	genIns(code,NULL,incr,D2,-integerVal(rhs),D);
      else{
	int M = --DP;
	genMove(code,D2,&M,"extra move");
	genIns(code,NULL,incr,M,-integerVal(rhs),D);
      }
    }
    else{
      int D = genInitDest(dest,depth,dp,code);
      int D1 = 0;
      int D2 = 0;
	  
      compExp(lhs,type,&D1,*dp,&DP,dict,outer,code,dLvl);
      compExp(rhs,type,&D2,DP,&DP,dict,outer,code,dLvl);

      if(D1==DP || D2==DP)
	genIns(code,NULL,minus,D1,D2,D);
      else{
	int M = --DP;
	genMove(code,D1,&M,"extra move");
	genIns(code,NULL,minus,M,D2,D);
      }
    }
  }

  else if(isBinaryCall(input,kstar,&lhs,&rhs)){
    int D = genInitDest(dest,depth,dp,code);
    int D1 = 0;
    int D2 = 0;
	  
    compExp(lhs,type,&D1,*dp,&DP,dict,outer,code,dLvl);
    compExp(rhs,type,&D2,DP,&DP,dict,outer,code,dLvl);

    if(D1==DP || D2==DP)
      genIns(code,NULL,times,D1,D2,D);
    else{
      int M = --DP;
      genMove(code,D1,&M,"extra move");
      genIns(code,NULL,times,M,D2,D);
    }
  }

  else if(isBinaryCall(input,kslash,&lhs,&rhs)){
    int D = genInitDest(dest,depth,dp,code);
    int D1 = 0;
    int D2 = 0;
	  
    compExp(lhs,type,&D1,*dp,&DP,dict,outer,code,dLvl);
    compExp(rhs,type,&D2,DP,&DP,dict,outer,code,dLvl);

    if(D1==DP || D2==DP)
      genIns(code,NULL,divide,D1,D2,D);
    else{
      int M = --DP;
      genMove(code,D1,&M,"extra move");
      genIns(code,NULL,divide,M,D2,D);
    }
  }

  /* simple list operations */
  else if(isBinaryCall(input,kindex,&lhs,&rhs)){
    int D = genInitDest(dest,depth,dp,code);
    int D1 = 0;
    int D2 = 0;
	  
    compExp(lhs,NULL,&D1,*dp,&DP,dict,outer,code,dLvl);
    compExp(rhs,NULL,&D2,DP,&DP,dict,outer,code,dLvl);

    genIns(code,NULL,nthel,D1,D2,D);
  }

  else if(isBinaryCall(input,kdot,&lhs,&rhs)){
    cellpo rec=lhs;
    cellpo fld=rhs;
    cellpo rctype = typeInfo(rec);	/* Extract the type of the record */
    int D=genDest(dest,depth,dp,code);
    int R=0;
    dictpo ndict;

    compExp(rec,rctype,&R,depth,&DP,dict,outer,code,dLvl);
      
    ndict = extendDict(rctype,R,code,DP,&DP,dict);

    compExp(fld,type,&D,DP,&DP,ndict,outer,code,dLvl);

    clearDict(ndict,dict);
  }

  /* String conversion expressions */
  else if(isBinaryCall(input,ktilda,&lhs,&rhs)){
    cellpo data=lhs,width=rhs,precision=voidcell;
    int A2=0;
    int W=0;
    int reslt = depth-1;

    genDest(dest,depth,dp,code);
    
    if(!isBinaryCall(lhs,khat,&data,&precision))
      isBinaryCall(rhs,khat,&width,&precision);

    compExp(data,NULL,&A2,depth--,&DP,dict,outer,code,dLvl);
    genMove(code,A2,&depth,NULL);

    if(precision==voidcell)	/* use precision 0 */
      genIns(code,"precision",movl,c_zero,--depth);
    else{
      int P = 0;

      compExp(precision,numberTp,&P,depth--,&DP,dict,outer,code,dLvl);
      genMove(code,P,&depth,"precision");
    }

    compExp(width,numberTp,&W,depth--,&DP,dict,outer,code,dLvl);
    genMove(code,W,&depth,"width");

    genIns(code,"strof",esc_fun,depth,IsEscapeFun(locateC("strof"),NULL,NULL,NULL));
    genMove(code,reslt,dest,NULL);
  }

  /* Precision only specified */
  else if(isBinaryCall(input,khat,&lhs,&rhs)){
    cellpo data=lhs,precision=rhs;
    int A2=0;
    int P=0;
    int reslt = depth-1;
    
    genDest(dest,depth,dp,code);
	  
    compExp(data,NULL,&A2,depth--,&DP,dict,outer,code,dLvl);
    genMove(code,A2,&depth,NULL);

    compExp(precision,numberTp,&P,depth--,&DP,dict,outer,code,dLvl);
    genMove(code,P,&depth,"precision");

    genIns(code,"width",movl,c_zero,--depth);

    genIns(code,"strof",esc_fun,depth,IsEscapeFun(locateC("strof"),NULL,NULL,NULL));
    genMove(code,reslt,dest,NULL);
  }

  /* Type string construction primitive */
  else if(isUnaryCall(input,ktstring,&lhs)){
    cellpo sig = makeTypeSig(typeInfo(lhs));

    genDest(dest,depth,dp,code);
    genIns(code,"type signature",movl,sig,*dest);
  }

  /* Theta expression */
  else if(IsTheta(input,&input))
    compTheta(input,type,dest,depth,dp,code,dict,dLvl);

  /* function abstraction */
  else if(isBinaryCall(input,kfn,NULL,NULL))
    compLambda(input,type,dest,depth,dp,code,dict,dLvl);

  /* mu abstraction */
  else if(isBinaryCall(input,kstmt,NULL,NULL))
    compMu(input,type,dest,depth,dp,code,dict,dLvl);

  else if(isBinaryCall(input,kchoice,NULL,NULL)){
    if(type!=NULL && isFunType(type,NULL,NULL))
      compLambda(input,type,dest,depth,dp,code,dict,dLvl);
    else if(type!=NULL && isProcType(type,NULL))
      compMu(input,type,dest,depth,dp,code,dict,dLvl);
    else
      reportErr(lineInfo(input),"illegal choice expression `%5.1800w'",input);
  }

  else if(IsZeroStruct(input,kself))
    genIns(code,NULL,self,genDest(dest,depth,dp,code));

  else if(isUnaryCall(input,ktry,&input))
    compExp(input,type,dest,depth,dp,dict,outer,code,dLvl);
  
  else if(isBinaryCall(input,kcatch,&lhs,&rhs)){
    int D = genInitDest(dest,depth,dp,code);
    int EC = depth = (*dp)-2;		/* place for the error code */
    lblpo L0=NewLabel(code);
    lblpo Lx=NewLabel(code);

    genIns(code,NULL,errblk,EC,L0); /* start an error block */

    compExp(lhs,type,dest,depth,&depth,dict,outer,code,dLvl);

    genIns(code,NULL,errend,EC);	/* end the error block */
    genIns(code,NULL,jmp,Lx); /* Exit to next statement */

    DefLabel(code,L0);
    genIns(code,"Pick up error value",moverr,++EC);
    if(dLvl>0)
      genIns(code,"report the error",error_d,depth,EC);

    {
      rewriteInfo info = {&D};
      lblpo LE = NewLabel(code);

      if(compChoice(rhs,1,EC,EC,Lx,LE,dict,code,dLvl,compRewrite,&info)){
	DefLabel(code,LE);
	genIns(code,"re-raise error",generr,EC);
      }
      else
	DefLabel(code,LE);
    }

    DefLabel(code,Lx);
  }

  else if(isUnaryCall(input,kraise,&rhs)){
    int D=0;
    compExp(foldExp(rhs,rhs,dict),NULL,&D,depth,&depth,dict,outer,code,dLvl);
    genIns(code,"user exception",generr,D);
  }

  else if(isBinaryCall(input,kdefn,NULL,&input) ||
	  isBinaryCall(input,kfield,NULL,&input))
    compExp(input,type,dest,depth,dp,dict,outer,code,dLvl);
    
  else if(isBinaryCall(input,kchrcons,&lhs,&rhs)){       // A bit of legacy cruft
    int H=0,T=0;
    int DP;
    int D=genDest(dest,depth,dp,code);

    compExp(lhs,NULL,&H,depth,&DP,dict,outer,code,dLvl);
    compExp(rhs,type,&T,DP,&DP,dict,outer,code,dLvl);

    if(H==DP || T==DP || D==DP)
      genIns(code,NULL,lstpr,H,T,D);
    else{
      int M = --DP;
      genIns(code,"extra var for safe GC",initv,M);
      genIns(code,NULL,lstpr,H,T,M);
      genMove(code,M,&D,"extra move generated");
    }
  }
    
  else if(IsList(input)){	/* List? */
    if(isEmptyList(input)){
      int D=genDest(dest,depth,dp,code);
      genIns(code,NULL,movl,nilcell,D); /* Construct empty list */
    }
    else{
      int H=0,T=0;
      int DP;
      int D=genDest(dest,depth,dp,code);

      input=listVal(input);
      compExp(input,NULL,&H,depth,&DP,dict,outer,code,dLvl);
      compExp(Next(input),type,&T,DP,&DP,dict,outer,code,dLvl);

      if(H==DP || T==DP || D==DP)
	genIns(code,NULL,lstpr,H,T,D);
      else{
	int M = --DP;
	genIns(code,"extra var for safe GC",initv,M);
	genIns(code,NULL,lstpr,H,T,M);
	genMove(code,M,&D,"extra move generated");
      }
	
    }
  }
  
  else if(isCall(input,khdl,&ar) && ar==HANDLE_ARITY-1){
    int D = genDest(dest,depth,dp,code);
    long arity = compArguments(input,depth,&depth,dict,outer,code,dLvl);
    int esc_code = IsEscapeFun(locateC("_handle"),NULL,NULL,NULL);

    genIns(code,"handle",esc_fun,depth,esc_code);
    genMove(code,depth+arity-1,&D,"extra move");
  }
  
  else if(isUnaryCall(input,kcase,&lhs) && isBinaryCall(lhs,kin,&lhs,&rhs)){
    int D = 0;
    lblpo LE = NewLabel(code);
    lblpo Lx = NewLabel(code);
    rewriteInfo info = {dest};
    
    compExp(lhs,NULL,&D,depth,&depth,dict,outer,code,dLvl);
    
    if(compChoice(rhs,1,D,depth,Lx,LE,dict,code,dLvl,compRewrite,&info)){
      DefLabel(code,LE);
      failError(lhs,depth,dict,code,dLvl);
    }
    else
      DefLabel(code,LE);
      
    DefLabel(code,Lx);
    
  }

  else if(IsStruct(input,&fn,&ar)){             // Function call or constructor
    cellpo ftype = typeInfo(fn);
    int D = genDest(dest,depth,dp,code);

    assert(ftype!=NULL);

    if(isConstructorType(ftype,NULL,NULL)){
      long ar;
      
      genIns(code,"Set up constructor",movl,fn,--depth);
      ar = compArguments(input,depth,&depth,dict,outer,code,dLvl);
      genIns(code,"Constructor",loc2cns,depth,ar,D);
    }
    else{
      long ar = compArguments(input,depth,&depth,dict,outer,code,dLvl);
      
      funCall(consFn(input),ar,depth,dict,code,dLvl);
      genMove(code,depth-1+ar,dest,"Collect result of equation");
    }
  }
  else if(isTpl(input)){	/* ordinary tuple? */
    int D=genDest(dest,depth,dp,code);
    int ar=tplArity(input);
    int i;
    int DP;

    for(i=0;i<ar;i++){
      int eD=0;
      cellpo el = tplArg(input,i);
      cellpo tp = typeInfo(el);

      compExp(el,tp,&eD,depth--,&DP,dict,outer,code,dLvl);
      genMove(code,eD,&depth,NULL);
    }
    DP=depth;
    genIns(code,NULL,loc2tpl,DP,ar,D);
  }
  else
    reportErr(lineInfo(input),"cant understand expression `%4.1800w'",input); 
}

long compArguments(cellpo input,int depth,int *dp,dictpo dict,dictpo outer,cpo code,int dLvl)
{
  unsigned int ar=consArity(input);
  unsigned int i;

  for(i=0;i<ar;i++){
    int D=0;
    int DP;
    cellpo el = consEl(input,i);
    cellpo tp = typeInfo(el);

    if(dLvl>0 && source_file(el)!=NULL)
      genLineDebugInfo(code,depth,el,True); /* are we debugging? */

    compExp(el,tp,&D,depth--,&DP,dict,outer,code,dLvl);
    genMove(code,D,&depth,NULL);
  }
  if(dp!=NULL)
    *dp=depth;
  return ar;
}

void funCall(cellpo f,int arity,int depth,dictpo dict,cpo code,int dLvl)
{
  int index,off;
  vartype vt;
  int esc_code;

#ifdef _COMPTRACE
  if(traceCompile)
    outMsg(logFile,"function call (depth=%d) `%5w'",depth,f);
#endif

  if(isSymb(f)){
    int farity;

    if(IsFunVar(f,&farity,NULL,NULL,&off,&index,&vt,dict)){
      switch(vt){
      case localvar:
	genIns(code,symN(symVal(f)),call,depth,farity,off);
	break;
      case envvar:
	genIns(code,symN(symVal(f)),ecall,depth,farity,off);
	break;
      case indexvar:{
	int D=depth-1;
	genIns(code,"Access function from record",indxfld,off,index,D);
	genIns(code,symN(symVal(f)),call,depth,farity,D);
	break;
      }
      case thetavar:{
	int D=depth-1;
	genIns(code,"Theta function variable",emove,off,D);
	genIns(code,symN(symVal(f)),indxfld,D,index,D);
	genIns(code,"function call",call,depth,farity,D);
	break;
      }
      }
    }
    else if((esc_code=IsEscapeFun(symVal(f),&farity,NULL,NULL))!=-1)
      genIns(code,symN(symVal(f)),esc_fun,depth,esc_code);
    else 
      reportErr(lineInfo(f),"undefined function %w in function call",f);
  }
  else{
    int D=0;
    int DP;

    compExp(f,NULL,&D,depth,&DP,dict,dict,code,dLvl);
    genIns(code,NULL,call,depth,arity,D);
  }
}

static void genEscapeSymbol(cellpo esc,cellpo type,int *dest,int depth,int *dp,
			    dictpo dict, cpo code,int dLvl)
{
  cellpo escType = allocSingle();
  cellpo lhs,rhs;

#ifdef COMPTRACE_
  if(traceCompile)
    outMsg(logFile,"compiling escape symbol into lambda",esc);
#endif

  IsEscape(symVal(esc),escType);

  if(isFunType(escType,&lhs,&rhs)){
    cellpo funForm = BuildBinCall(kfn,voidcell,voidcell,allocSingle()); /* * => * */
    if(isTpl(lhs)){
      long ar = tplArity(lhs);
      long i;
      cellpo call = BuildStruct(esc,ar,callArg(funForm,1));       // esc(...)
      cellpo head = BuildTuple(ar,callArg(funForm,0));
      
      for(i=0;i<ar;i++){
	uniChar vName[20];

	strMsg(vName,NumberOf(vName),"X%d",i);
	mkSymb(consEl(call,i),locateU(vName));
	mkSymb(tplArg(head,i),locateU(vName));
	setTypeInfo(consEl(call,i),tplArg(lhs,i));
	setTypeInfo(tplArg(head,i),tplArg(lhs,i));
      }

      setTypeInfo(callArg(funForm,0),lhs);
    }
    else
      reportErr(lineInfo(esc),"Arguments should form a tuple");

    setTypeInfo(funForm,escType);
    compExp(funForm,escType,dest,depth,dp,dict,dict,code,dLvl);
  }
  else if(isProcType(escType,NULL)){	/* procedure escape */
    cellpo procForm = BuildBinCall(kstmt,voidcell,voidcell,allocSingle());
    unsigned long ar = consArity(escType);
    unsigned long i;
    cellpo call = BuildStruct(esc,ar,callArg(procForm,1));       // esc(...)
    cellpo head = BuildTuple(ar,callArg(procForm,0));
      
    for(i=0;i<ar;i++){
      uniChar vName[20];

      strMsg(vName,NumberOf(vName),"X%d",i);
      mkSymb(consEl(call,i),locateU(vName));
      mkSymb(tplArg(head,i),locateU(vName));
      setTypeInfo(consEl(call,i),tplArg(lhs,i));
      setTypeInfo(tplArg(head,i),tplArg(lhs,i));
    }
    setTypeInfo(callArg(procForm,0),tupleOfArgs(escType,allocSingle()));

    setTypeInfo(procForm,escType);
    compExp(procForm,escType,dest,depth,dp,dict,dict,code,dLvl);
  }
  else
    syserr("invalid call to genEscapeSymbol");
}

/*
 * Caste an expression into an any value
 */

void anyExp(cellpo exp,int *dest,int depth,
	    int *dp,dictpo dict,dictpo outer,cpo code,int dLvl)
{
  cellpo etype = deRef(typeInfo(exp));
  int DD = genDest(dest,depth,dp,code);
  int DP = depth;
  int X = 0;
  int Y = 0;

  compExp(exp,etype,&X,depth--,&DP,dict,outer,code,dLvl);
  genMove(code,X,&depth,NULL);
  compTypeExp(exp,etype,&Y,depth--,&DP,dict,code,dLvl);
  genMove(code,Y,&depth,NULL);

  genIns(code,"construct any value",loc2any,Y,X,DD);
}

/* Coerce a value to the appropriate type */
static void coerceExp(cellpo exp,cellpo caste,cellpo type,int *dest,int depth,
		      int *dp,dictpo dict,dictpo outer,cpo code,int dLvl)
{
  int DP;
  cellpo atype = typeInfo(exp);
  cellpo ctype = typeInfo(caste); /* pick up the computed type */

  assert(atype!=NULL && ctype!=NULL);

  if(IsTypeVar(ctype=deRef(ctype))) /* ignore type caste for type variables */
    compExp(exp,NULL,dest,depth,dp,dict,outer,code,dLvl);
  else if(IsSymbol(ctype,kany))
    anyExp(exp,dest,depth,dp,dict,outer,code,dLvl);
  else if(!IsTypeVar(atype) && sameType(ctype,atype)) 
    compExp(exp,type,dest,depth,dp,dict,outer,code,dLvl);
  else if(sameCell(ctype,numberTp)){
    int A=0;

    genDest(dest,depth,dp,code);
	    
    compExp(exp,NULL,&A,depth--,&DP,dict,outer,code,dLvl);
    genMove(code,A,&depth,NULL);
    genIns(code,"_number",esc_fun,depth,IsEscapeFun(locateC("_number"),NULL,NULL,NULL));
    genMove(code,depth,dest,NULL);
  }
  else if(sameCell(ctype,handleTp)){
    int A=depth-1;
    int D=genDest(dest,depth,dp,code);
	    
    compExp(exp,NULL,&A,depth--,&DP,dict,outer,code,dLvl);
    genIns(code,"_canonical_handle",esc_fun,depth,IsEscapeFun(locateC("_canonical_handle"),NULL,NULL,NULL));
    genMove(code,depth,&D,NULL);
  }
  else if(sameCell(ctype,symbolTp) && sameCell(atype,stringTp)){ /* caste to symbol? */
    int A=depth-1;

    genDest(dest,depth,dp,code);
	    
    compExp(exp,NULL,&A,depth--,&DP,dict,outer,code,dLvl);
    genIns(code,"implode",esc_fun,depth,IsEscapeFun(locateC("implode"),NULL,NULL,NULL));
    genMove(code,depth,dest,NULL);
  }

  else{
    int A2 = 0;
    int tD = 0;
    int DP;

    genDest(dest,depth,dp,code);

    compTypeExp(exp,ctype,&tD,depth--,&DP,dict,code,dLvl);
    compExp(exp,NULL,&A2,depth--,&DP,dict,outer,code,dLvl);
    genMove(code,A2,&depth,NULL);

    genIns(code,"coerce",esc_fun,depth,IsEscapeFun(kcoerce,NULL,NULL,NULL));
    genMove(code,depth+1,dest,NULL);
  }
}

static logical compRewrite(cellpo input,int arity,int srcvar,
			   int depth,lblpo Next,lblpo LF,
			   dictpo dict,cpo code,
			   logical last,int dLvl,void *cl)
{
  rewriteInfo *info = (rewriteInfo*)cl;
  cellpo ptn,rhs;
  cellpo a1,a2;
  logical mayFail = False;	/* no possibility of failure...yet */

  if(isBinaryCall(input,kfn,&ptn,&rhs)){
    dictpo ndict,lo;
    cellpo guard=ctrue;
    logical anyPresent = EmbeddedAny(ptn);
    lblpo Lf = LF;

    isBinaryCall(ptn,kguard,&ptn,&guard); /* remove the guard for a while */

    ndict=lo=BoundVars(ptn,srcvar,arity,addMarker(dict),depth,code,dLvl);

    if(anyPresent){
      Lf = NewLabel(code);
      genIns(code,"we may need to do type unification",urst,depth);
    }

    mayFail=compPttrn(ptn,srcvar,depth,&depth,LF,ndict,&ndict,lo,dict,code,singleAss,dLvl);

    compTest(guard,depth,&depth,Lf,ndict,&ndict,dict,code,dLvl);

    if(!IsSymbol(guard,ktrue) ||
       (IsEnumStruct(guard,&guard) && IsSymbol(guard,ktrue)))
      mayFail = True;

    if(dLvl>0 && source_file(rhs)!=NULL)
      genLineDebugInfo(code,depth,rhs,True); /* are we debugging? */

    compExp(rhs,NULL,info->dest,depth,&depth,ndict,dict,code,dLvl);
    genIns(code,NULL,jmp,Next); /* we are done with the choice */
    
    if(anyPresent){
      DefLabel(code,Lf);
      genIns(code,"reset unification",undo,depth);
      genIns(code,"and then fail",jmp,LF);
    }
    
    clearDict(ndict,dict);	/* remove excess dictionary */
  }
  else if(isBinaryCall(input,kchoice,&a1,&a2)){
    lblpo L0 = NewLabel(code);

    compRewrite(a1,arity,srcvar,depth,Next,L0,dict,code,False,dLvl,cl);

    DefLabel(code,L0);
    return compRewrite(a2,arity,srcvar,depth,Next,LF,dict,code,False,dLvl,cl);
  }
  else{
    reportErr(lineInfo(input),"invalid form of case clause %w",input);
    return True;
  }
  return mayFail;
}


/* Generate an initialized location on the stack */
static int genInitDest(int *dest,int depth,int *dp,cpo code)
{
  if(*dest!=0)
    return genDest(dest,depth,dp,code);
  else{
    int D = genDest(dest,depth,dp,code);
    genIns(code,NULL,initv,D);
    return D;
  }
}

/* Try to decide if an expression is complicated -- i.e., might involve
   calling the garbage collector */
logical complexExp(cellpo exp)
{
  unsigned long ar;
  cellpo args;

  if(IsHashStruct(exp,kdebug,&exp))
    return complexExp(exp);
  else if(IsHashStruct(exp,knodebug,&exp))
    return complexExp(exp);
  else if(IsInt(exp) || IsFloat(exp) || IsString(exp) || isSymb(exp) || isChr(exp) || isEmptyList(exp))
    return False;
  else if(IsEnumStruct(exp,NULL))
    return False;
  else if(isNonEmptyList(exp))
    return True;
//    return complexExp(exp=listVal(exp)) || complexExp(Next(exp));  fgm 9/5/2001
  else if(IsQuote(exp,&args)){
    if(isSymb(args))
      return False;
    else
      return True;
  }
  else if(isCons(exp))
    return True;
  else if(IsTuple(exp,&ar))
    return ar>0;

/*  fgm 9/5/2001
  else if(IsTuple(exp,&ar)){
    int i;
    for(i=0;i<ar;i++)
      if(complexExp(tplArg(exp,i)))
	return True;
    return False;
  }
*/
  else{
    outStr(logFile,"problem in complex expression\n");
    return True;
  }
}

logical genMove(cpo code,int from,int *to,char *cmmnt)
{
  if(*to==0)
    *to=from;
  else if(*to!=from)
    genIns(code,cmmnt,move,from,*to);
  return True;
}

/* Evaluate an expression to see if its safe for GC purposes */

logical gcSafeExp(cellpo input,dictpo dict)
{
  cellpo lhs,rhs;

  if(IsHashStruct(input,kdebug,&input))
    return gcSafeExp(input,dict);
  else if(IsHashStruct(input,knodebug,&input))
    return gcSafeExp(input,dict);
  else if(isSymb(input))
    return True;		/* whether symbol or variable, this is safe */
  else if(IsEnumStruct(input,NULL))
    return True;
  else if(IsInt(input))
    return True;
  else if(IsFloat(input))
    return True;
  else if(IsString(input))	/* a string literal ... */
    return True;
  else if(IsQuote(input,&input)) /* quoted expressions */
    return True;

  /* Handle standard operators and normal function calls */
  else if(IsAnyStruct(input,NULL)) /* type cast expressions */
    return False;		/* not safe in general */

  else if(isBinaryCall(input,ktcoerce,&lhs,&rhs)) /* type coercion expression */
    return False;

  else if(isUnaryCall(input,kvalueof,&rhs)) /* valof expression */
    return False;
  
  else if(isUnaryCall(input,kcollect,&rhs)) /* collect expression */
    return False;
      
  else if(isBinaryCall(input,kelse,&lhs,&rhs)){
    cellpo test,then;

    if(isBinaryCall(lhs,kthen,&test,&then) && 
       isUnaryCall(test,kif,&test))
      return gcSafeExp(test,dict) && gcSafeExp(then,dict) && gcSafeExp(rhs,dict);
    else
      return False;
  }

  /* Check for logical test condition */
  else if(isBinaryCall(input,kand,&lhs,&rhs)||
	  isBinaryCall(input,kor,&lhs,&rhs)||
	  isBinaryCall(input,kdand,&lhs,&rhs)||
	  isBinaryCall(input,kdor,&lhs,&rhs)||
	  isBinaryCall(input,kequal,&lhs,&rhs)||
	  isBinaryCall(input,knotequal,&lhs,&rhs)||
	  isBinaryCall(input,kgreaterthan,&lhs,&rhs)||
	  isBinaryCall(input,kgreatereq,&lhs,&rhs)||
	  isBinaryCall(input,klessthan,&lhs,&rhs)||
	  isBinaryCall(input,klesseq,&lhs,&rhs))
    return gcSafeExp(lhs,dict) && gcSafeExp(rhs,dict);

  else if(isUnaryCall(input,knot,&lhs) || isUnaryCall(input,kdnot,&lhs))
    return gcSafeExp(lhs,dict);

  else if(isBinaryCall(input,kmatch,&lhs,&rhs)||
	  isBinaryCall(input,knomatch,&lhs,&rhs) ||
	  isBinaryCall(input,kin,&lhs,&rhs))
    return gcSafePtn(lhs,dict) && gcSafeExp(rhs,dict);


  /* Specific cases of arithmetic expressions */
  else if(isBinaryCall(input,kplus,&lhs,&rhs))
    return gcSafeExp(lhs,dict) && gcSafeExp(rhs,dict);

  else if(isBinaryCall(input,kminus,&lhs,&rhs))
    return gcSafeExp(lhs,dict) && gcSafeExp(rhs,dict);

  else if(isBinaryCall(input,kstar,&lhs,&rhs))
    return gcSafeExp(lhs,dict) && gcSafeExp(rhs,dict);

  else if(isBinaryCall(input,kslash,&lhs,&rhs))
    return gcSafeExp(lhs,dict) && gcSafeExp(rhs,dict);

  /* simple list operations */
  else if(isBinaryCall(input,kindex,&lhs,&rhs))
    return gcSafeExp(lhs,dict) && gcSafeExp(rhs,dict);

  else if(isBinaryCall(input,kdot,&lhs,&rhs))
    return gcSafeExp(lhs,dict) && gcSafeExp(rhs,dict);

  /* record environment */
  else if(IsTheta(input,&input))
    return False;

  /* function abstraction */
  else if(isBinaryCall(input,kfn,&lhs,&rhs))
    return False;

  /* mu abstraction */
  else if(isBinaryCall(input,kstmt,&lhs,&rhs))
    return False;

  else if(isBinaryCall(input,kchoice,NULL,NULL))
    return False;

  else if(IsZeroStruct(input,kself))
    return True;

  else if(isUnaryCall(input,ktry,&input))
    return gcSafeExp(input,dict);
  
  else if(isBinaryCall(input,kcatch,&lhs,&rhs))
    return False;

  else if(isUnaryCall(input,kraise,&rhs))
    return gcSafeExp(rhs,dict);

  else if(isCons(input))
    return False;

  else if(IsList(input)){	/* List? */
    input=listVal(input);

    if(input==NULL)
      return True;
    else
      return False;
  }
  else
    return False;
}


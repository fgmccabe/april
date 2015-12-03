/*
  Pattern code generator for April compiler
  Polymorphic type version of pattern generator
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
#include <assert.h>		/* debugging assertions */
#include "compile.h"		/* Compiler structures */
#include "generate.h"		/* Code generation interface */
#include "makesig.h"
#include "types.h"		/* Type cheking interface */
#include "dict.h"
#include "keywords.h"		/* Standard April keywords */
#include "opcodes.h"		/* April machine opcodes */
#include "assem.h"		/* Code generation*/
#include "cdbgflags.h"		/* Debugging option flags */

static int PtnDest(cellpo input,dictpo dict,int *depth);
static logical compPtn(cellpo input,int var,lblpo LF,int depth,
		       dictpo dict,dictpo outer,cpo code,assMode mode,
		       int dLvl);

logical compPttrn(cellpo input,int var,int depth,int *dp,lblpo LF,
		  dictpo dict,dictpo *nd,dictpo lo,dictpo outer,cpo code,
		  assMode mode,int dLvl)
{
  cellpo test;

#ifdef COMPTRACE
  if(traceCompile)
    outMsg(logFile,"Compile pattern: `%5w'\n",input);
#endif

  input = deRef(input);

  if(IsHashStruct(input,kdebug,&input))
    return compPttrn(input,var,depth,dp,LF,dict,nd,lo,outer,code,mode,dLvl);
  else if(IsHashStruct(input,knodebug,&input))
    return compPttrn(input,var,depth,dp,LF,dict,nd,lo,outer,code,mode,False);
  else if(isBinaryCall(input,kguard,&input,&test)){
    /* find embedded vars*/
    dictpo ndict = EmbeddedVars(input,depth,&depth,False,mode,False,
				dict,lo,outer,code);

    compPtn(input,var,LF,depth,ndict,outer,code,mode,dLvl);
    compTest(test,depth,dp,LF,ndict,nd,outer,code,dLvl); /* semantic test */

    return True;
  }
  else{
    dictpo ndict=*nd=EmbeddedVars(input,depth,dp,False,singleAss,False,
				  dict,lo,outer,code);
    return compPtn(input,var,LF,*dp,ndict,outer,code,mode,dLvl);
  }
}

logical compExpPttrn(cellpo input,cellpo exp,int depth,int *dp,
		     lblpo LF,dictpo dict,dictpo *nd,dictpo lo,dictpo outer,
		     cpo code,assMode mode,logical assign,int dLvl)
{
  cellpo test;

  input = deRef(input);

  if(IsHashStruct(input,kdebug,&input))
    return compExpPttrn(input,exp,depth,dp,LF,dict,nd,lo,outer,code,mode,
			assign,dLvl);
  else if(IsHashStruct(input,knodebug,&input))
    return compExpPttrn(input,exp,depth,dp,LF,dict,nd,lo,outer,code,mode,
			assign,dLvl-1);
  else if(isBinaryCall(input,kguard,&input,&test)){
    int ND;			/* Intermediate depth */
    int var=0;
    cellpo eType = deRef(typeInfo(exp));
    dictpo ndict=(assign ? dict :
		  EmbeddedVars(input,depth,&depth,False,mode,assign,
			       dict,lo,outer,code));

    compExp(exp,eType,&var,depth,&ND,dict,outer,code,dLvl);

    compPtn(input,var,LF,ND,ndict,outer,code,mode,dLvl);
    compTest(test,depth,dp,LF,ndict,nd,outer,code,dLvl); /* semantic test */

    if(dLvl>0)
      genPtnVarDebug(code,*dp,ndict,NULL,outer);

    return True;		/* We might have a failure */
  }
  else{
    int var=0;
    cellpo eType = deRef(typeInfo(exp));
    dictpo ndict=*nd=(assign?dict:
		      EmbeddedVars(input,depth,dp,False,mode,assign,
				   dict,lo,outer,code));
    logical mayfail;

    if(assign)
      *dp = depth;

    compExp(exp,eType,&var,*dp,&depth,dict,outer,code,dLvl);            /* The expression doesnt include lhs variables */

    mayfail = compPtn(input,var,LF,depth,*nd,outer,code,mode,dLvl);

    if(dLvl>0)
      genPtnVarDebug(code,*dp,ndict,NULL,outer);

    return mayfail;
  }
}

/*
 * The main compile pattern code generator
 */

static logical compPtn(cellpo input,int var,lblpo LF,int depth,
		       dictpo dict,dictpo outer,cpo code,assMode mode,
		       int dLvl)
{
  cellpo lhs,rhs,fn;

  if(IsHashStruct(input,kdebug,&input))
    return compPtn(input,var,LF,depth,dict,outer,code,mode,dLvl);
  else if(IsHashStruct(input,knodebug,&input))
    return compPtn(input,var,LF,depth,dict,outer,code,mode,dLvl);
  else if(isSymb(input)){
    symbpo sy=symVal(input);

    if(sy==kuscore)
      return False;		/* Nothing to do here */

    else{			/* literal symbol or variable */
      vartype vt;
      int off,ind;
      initStat inited;
      assMode rw;
      cellpo vtp;

      if(IsVar(input,NULL,&vtp,&off,&ind,&rw,&inited,&vt,dict)){
    	if(inited==Inited && mode==singleAss)
	  switch(vt){
	  case localvar:
	    if(off!=var){
	      genIns(code,symN(sy),neq,off,var);
	      genIns(code,"not same value",jmp,LF);
	      return True;
	    }
	    else
	      return False;

	  case indexvar:
	    genIns(code,symN(sy),indxfld,off,ind,--depth);
	    genIns(code,"test var",neq,depth,var);
	    genIns(code,"not same value",jmp,LF);
	    return True;

	  case envvar:
	    genIns(code,symN(sy),emove,off,--depth);
	    genIns(code,"test",neq,depth,var);
	    genIns(code,"not same value",jmp,LF);
	    return True;

	  case thetavar:
	    genIns(code,"Theta variable",emove,off,--depth);
	    genIns(code,symN(sy),indxfld,depth,ind,depth);
	    genIns(code,"test",neq,depth,var);
	    genIns(code,"not same value",jmp,LF);
	    return True;
	  }
	else {
	  if(rw==singleAss && inited==Inited && mode==multiAss)
	    reportErr(lineInfo(input),"assignment to read-only variable -- %w",
		      input);
	  switch(vt){
	  case localvar:
	    genMove(code,var,&off,symN(sy)); /* write out the matched object */
	    break;

	  case indexvar:
	    genIns(code,symN(sy),tpupdte,var,off,ind);
	    break;

	  case envvar:{
	    int DD = depth-1;
	    genIns(code,"Have to update environment",stoe,DD);
	    genIns(code,symN(sy),tpupdte,var,off,DD);
	    break;
	  }

	  case thetavar:{
	    int DD = depth-1;
	    genIns(code,"Theta variable",emove,off,DD);
	    genIns(code,symN(sy),tpupdte,var,ind,DD);
	    break;
	  }
	  }

	  InitVar(input,True,dict); /* mark the variable as being initialised */
	  /* Generate a pattern for the variable's type */
	  return False;		/* no failure possible here */
	}
      }
      else{
	genIns(code,NULL,mlit,var,input); /* literal symbol */
	reportWarning(lineInfo(input),"unexpected symbol %w",input);
	genIns(code,"not same symbol",jmp,LF);
	return True;
      }
    }
  }
  else if(IsEnumStruct(input,&lhs)){
    genIns(code,"enumerated symbol",mlit,var,lhs); /* literal symbol */
    genIns(code,"not same symbol",jmp,LF);
    return True;
  }

  else if(IsInt(input)){
    genIns(code,NULL,mlit,var,input);
    genIns(code,"not same integer",jmp,LF);
    return True;
  }

  else if(IsFloat(input)){
    genIns(code,NULL,mfloat,var,input);
    genIns(code,"not same float",jmp,LF);
    return True;
  }
  
  else if(isChr(input)){
    genIns(code,"character",mchar,var,CharVal(input));
    genIns(code,NULL,jmp,LF);
    return True;
  }

  else if(IsQuote(input,&input) && isSymb(input)){
    genIns(code,"quoted",mlit,var,input);
    genIns(code,"not same symbol",jmp,LF);
    return True;
  }

  else if(IsString(input)){
    genIns(code,NULL,mlit,var,input);
    genIns(code,"Failing literal test",jmp,LF);
    return True;
  }

  else if(IsQuery(input,NULL,&input))
    return compPtn(input,var,LF,depth,dict,outer,code,mode,dLvl);

  else if(isUnaryCall(input,kquery,&rhs))
    return compPtn(rhs,var,LF,depth,dict,outer,code,mode,dLvl);

  else if(IsAnyStruct(input,&lhs)){
    cellpo ptype = deRef(typeInfo(lhs)); /* pick up the result type */
    int X = --depth;
    initStat itd;
    cellpo rlhs = lhs;

    isBinaryCall(lhs,kguard,&rlhs,NULL);

    /* We only generate matching code for the type signature if we need it */

    if(!isSymb(rlhs) ||
       !IsEnumStruct(rlhs,NULL) ||
       !(IsVar(rlhs,NULL,NULL,NULL,NULL,NULL,&itd,NULL,dict) && 
	 !(itd==Inited && mode==singleAss))){		/* true if there are any free type vars */
      int S = depth-1;
      
      genIns(code,"Match an any structure",many,var,S,X);
      genIns(code,"Jump if not an any value",jmp,LF);
      compTypePtn(input,ptype,S,depth-1,LF,dict,code,dLvl);
    }
    else{
      int S = depth-1;
      genIns(code,"Match an any structure",many,var,S,X);
      genIns(code,"Jump if not an any value",jmp,LF);
    }

    compPtn(lhs,X,LF,depth,dict,outer,code,mode,dLvl);
    return True;
  }

  else if(IsShriek(input,&rhs)){
    int D=0;
    int ND;

    compExp(rhs,NULL,&D,depth,&ND,dict,outer,code,dLvl);

    genIns(code,"Shriek test",neq,D,var);
    genIns(code,"not same value",jmp,LF);
    return True;
  }

  /* list & string matching ...*/
  else if(isBinaryCall(input,kcatenate,NULL,NULL) ||
	  isBinaryCall(input,kappend,NULL,NULL) ||
	  isBinaryCall(input,ktcoerce,NULL,NULL) ||
	  isBinaryCall(input,kin,NULL,NULL) ||
	  isBinaryCall(input,ktilda,NULL,NULL) ||
	  isBinaryCall(input,kchrcons,NULL,NULL) ||
	  isNonEmptyList(input))
    return compListPtn(input,var,depth,&depth,LF,dict,outer,code,mode,dLvl);

  else if(isBinaryCall(input,kguard,&input,&rhs)){ /* semantic test */
    dictpo ndict = EmbeddedVars(input,depth,&depth,False,singleAss,False,
				dict,dict,outer,code);

    compPtn(input,var,LF,depth,ndict,outer,code,mode,dLvl);
    compTest(rhs,depth,&depth,LF,ndict,&ndict,outer,code,dLvl);
    clearDict(ndict,dict);
    return True;
  }

  else if(IsList(input)){	/* We are matching a list pair ... */
    if(isEmptyList(input))
      genIns(code,NULL,mnil,var,LF);
    else{
      int X;
      int Y;

      input = listVal(input);

      X = PtnDest(input,dict,&depth);
      Y = PtnDest(Next(input),dict,&depth);

      genIns(code,NULL,mlist,var,X,Y);
      genIns(code,"not a list",jmp,LF);
      
      compPtn(input,X,LF,depth,dict,outer,code,mode,dLvl);
      compPtn(Next(input),Y,LF,depth,dict,outer,code,mode,dLvl);
    }
    return True;
  }

  else if(isBinaryCall(input,kmatch,&lhs,NULL))
    return compPtn(lhs,var,LF,depth,dict,outer,code,mode,dLvl);

  /* handle constructor terms ... */
  else if(isCons(input) && 
          isConstructorType(typeInfo(fn=consFn(input)),NULL,NULL)){
    unsigned long ar = consArity(input);

    if(IsSymbol(fn,khdl) && consArity(input)==HANDLE_ARITY-1){
      unsigned int i;

      genIns(code,"Unpack constructor",mcons,var,HANDLE_ARITY);
      genIns(code,NULL,jmp,LF);
      genIns(code,"Check constructor",mcnsfun,var,fn);
      genIns(code,NULL,jmp,LF);
      
      for(i=0;i<HANDLE_ARITY-1;i++){ /* we dont look at the final argument */
	cellpo arg = consEl(input,i);
	int Depth=depth;
	int X = PtnDest(arg,dict,&Depth);
	cellpo aa = stripCell(arg);

	if(!IsSymbol(aa,kuscore)){
	  genIns(code,"access constructor element",consfld,var,i,X);

	  compPtn(arg,X,LF,Depth,dict,outer,code,mode,dLvl);
	}
      }
    }
    else{                               // regular constructor
      unsigned int i;

      genIns(code,"Unpack constructor",mcons,var,ar);
      genIns(code,NULL,jmp,LF);
      genIns(code,"Check constructor",mcnsfun,var,fn);
      genIns(code,NULL,jmp,LF);

      for(i=0;i<ar;i++){
	cellpo arg = consEl(input,i);
	int Depth=depth;
	int X = PtnDest(arg,dict,&Depth);
	cellpo aa = stripCell(arg);

	if(!IsSymbol(aa,kuscore)){
	  genIns(code,"access constructor element",consfld,var,i,X);

	  compPtn(arg,X,LF,Depth,dict,outer,code,mode,dLvl);
	}
      }
    }
    return True;		/* we assume the potential for failure  */
  }

  else if(IsTuple(input,NULL)){
    int ar=tplArity(input);
    int i=0;

    for(i=0;i<ar;i++){
      int X,Depth=depth;
      cellpo arg = tplArg(input,i);

      X = PtnDest(arg,dict,&Depth);

      if(!IsSymbol(arg,kuscore)){
	genIns(code,"access tuple el",indxfld,var,i,X);

	compPtn(arg,X,LF,Depth,dict,outer,code,mode,dLvl);
      }
    }
    return True;
  }
  else
    reportErr(lineInfo(input),"cant understant pattern `%4.1800w'",input);
  return True;
}

/*
 * Look for embedded variable declarations in a pattern
*/
static dictpo Embedded(cellpo input,int depth,int *dp,
		       logical isSafe,assMode mode,logical assign,
		       dictpo dict,dictpo local,dictpo outer,cpo code);

static dictpo EmbeddedTVars(cellpo input,int depth,int *dp,
			    logical isSafe,assMode mode,logical assign,
			    dictpo dict,dictpo local,dictpo outer,cpo code);

static dictpo GuardEmbedded(cellpo input,int depth,int *dp,
			    logical isSafe,assMode mode,logical assign,
			    dictpo dict,dictpo outer,cpo code);

dictpo EmbeddedVars(cellpo input,int depth,int *dp,logical isSafe,
		    assMode mode,logical assign,
		    dictpo dict,dictpo lo,dictpo outer,cpo code)
{
  return Embedded(input,depth,dp,isSafe,mode,assign,dict,lo,outer,code);
}

static dictpo Embedded(cellpo input,int depth,int *dp,
		       logical isSafe,assMode mode,logical assign,
		       dictpo dict,dictpo lo,dictpo outer,cpo code)
{
  cellpo lhs,rhs;

  if(IsHashStruct(input=deRef(input),kdebug,&input))
    return Embedded(input,depth,dp,isSafe,mode,assign,dict,lo,outer,code);
  else if(IsHashStruct(input,knodebug,&input))
    return Embedded(input,depth,dp,isSafe,mode,assign,dict,lo,outer,code);

  else if((IsQuery(input,NULL,&rhs)||
	   isUnaryCall(input,kquery,&rhs))&&isSymb(rhs)){
    if(!IsLocalVar(rhs,dict)||!IsLocalVar(rhs,lo)){
      cellpo type = typeInfo(rhs);

      assert(type!=NULL);

      dict=declVar(rhs,type,localvar,mode,notInited,False,--depth,-1,dict);

      if(!isSafe)
	genIns(code,"not safe",initv,depth);
    }
    *dp = depth;
    return dict;
  }

  else if(isBinaryCall(input,ktcoerce,NULL,&rhs))
    return Embedded(rhs,depth,dp,isSafe,mode,assign,dict,lo,outer,code);

  else if(isBinaryCall(input,ktilda,&lhs,NULL))
    return Embedded(lhs,depth,dp,isSafe,mode,assign,dict,lo,outer,code);

  else if(isBinaryCall(input,khat,&lhs,NULL))
    return Embedded(lhs,depth,dp,isSafe,mode,assign,dict,lo,outer,code);

  else if(IsAnyStruct(input,&rhs)){
    dict = Embedded(rhs,depth,&depth,isSafe,mode,assign,dict,lo,outer,code);
    return EmbeddedTVars(deRef(typeInfo(rhs)),depth,dp,isSafe,mode,assign,
			 dict,lo,outer,code);
  }

  /* Field name pattern */
  else if(isBinaryCall(input,kmatch,&lhs,NULL))
    return Embedded(lhs,depth,dp,isSafe,mode,assign,dict,lo,outer,code);

  else if(IsEnumStruct(input,NULL)){
    *dp = depth;
    return dict;
  }

  else if(isSymb(input) && !IsSymbol(input,kuscore)){
    cellpo type = typeInfo(input);

    assert(type!=NULL);

    if(!isConstructorType(typeInfo(input),NULL,NULL) &&
       (!IsLocalVar(input,dict)
	||(IsLocalVar(input,outer)&&!IsLocalVar(input,lo)))){
      dict=declVar(input,type,localvar,mode,notInited,False,--depth,-1,dict);
      if(!isSafe)
	genIns(code,"not safe",initv,depth);
    }
    else if(mode==multiAss && !assign && IsLocalVar(input,outer))
      reportErr(lineInfo(input),"read/write variable %w redeclared",input);
    *dp = depth;
    return dict;
  }

  else if(isBinaryCall(input,kguard,&lhs,&rhs)){
    dict=Embedded(lhs,depth,&depth,isSafe,mode,assign,dict,lo,outer,code);
    return GuardEmbedded(rhs,depth,dp,isSafe,mode,assign,dict,outer,code);
  }

  else if(IsQuote(input,NULL)){
    *dp = depth;
    return dict;
  }

  else if(IsShriek(input,NULL)){
    *dp = depth;
    return dict;
  }
  
  else if(isCons(input)){
    unsigned int ar=consArity(input);
    unsigned int i;
                    
    for(i=0;i<ar;i++)
      dict=Embedded(consEl(input,i),depth,&depth,isSafe,mode,assign,dict,lo,outer,code);

    *dp=depth;
    return dict;
  }
  else if(isTpl(input)){
    int ar=tplArity(input);
    int i;

    for(i=0;i<ar;i++)
      dict=Embedded(tplArg(input,i),depth,&depth,isSafe,mode,assign,dict,lo,outer,code);

    *dp=depth;
    return dict;
  }
  else if(isNonEmptyList(input)){
    input=listVal(input);
    dict=Embedded(input,depth,&depth,isSafe,mode,assign,dict,lo,outer,code);
    return Embedded(Next(input),depth,dp,isSafe,mode,assign,dict,lo,outer,code);
  }
  else{
    *dp = depth;
    return dict;
  }
}

static dictpo GuardEmbedded(cellpo input,int depth,int *dp,logical isSafe,
			    assMode mode,logical assign,
			    dictpo dict,dictpo outer,cpo code)
{
  cellpo lhs,rhs;

  if(IsHashStruct(input,kdebug,&input))
    return GuardEmbedded(input,depth,dp,isSafe,mode,assign,dict,outer,code);
  else if(IsHashStruct(input,knodebug,&input))
    return GuardEmbedded(input,depth,dp,isSafe,mode,assign,dict,outer,code);
  else if(isBinaryCall(input,kmatch,&lhs,NULL))
    return EmbeddedVars(lhs,depth,dp,isSafe,mode,assign,dict,dict,outer,code);
  else if(isBinaryCall(input,kin,&lhs,NULL))
    return EmbeddedVars(lhs,depth,dp,isSafe,mode,assign,dict,dict,outer,code);
  else if(isBinaryCall(input,kand,&lhs,&rhs) ||
	  isBinaryCall(input,kdand,&lhs,&rhs)){
    dict = GuardEmbedded(lhs,depth,&depth,isSafe,mode,assign,dict,outer,code);
    return GuardEmbedded(rhs,depth,dp,isSafe,mode,assign,dict,outer,code);
  }
  else{
    if(dp!=NULL)
      *dp = depth;
    return dict;
  }
}

static dictpo EmbeddedTVars(cellpo input,int depth,int *dp,
			    logical isSafe,assMode mode,logical assign,
			    dictpo dict,dictpo lo,dictpo outer,cpo code)
{
  cellpo lhs,rhs;

  if(IsHashStruct(input=deRef(input),kdebug,&input))
    return EmbeddedTVars(input,depth,dp,isSafe,mode,assign,dict,lo,outer,code);
  else if(IsHashStruct(input,knodebug,&input))
    return EmbeddedTVars(input,depth,dp,isSafe,mode,assign,dict,lo,outer,code);

  else if(IsQuery(input,&lhs,&rhs) && isSymb(rhs))
    return EmbeddedTVars(lhs,depth,dp,isSafe,mode,assign,dict,lo,outer,code);

  else if(isSymb(input)){
    *dp = depth;
    return dict;
  }

  else if(isListType(input,&lhs)) 
    return EmbeddedTVars(lhs,depth,dp,isSafe,mode,assign,dict,lo,outer,code);

  else if(isCons(input)){
    unsigned int ar=consArity(input);
    unsigned int i;

    for(i=0;i<ar;i++)
      dict=EmbeddedTVars(consEl(input,i),depth,&depth,isSafe,mode,assign,
			 dict,lo,outer,code);

    *dp=depth;
    return dict;
  }

  else if(isTpl(input)){
    int ar=tplArity(input);
    int i;

    for(i=0;i<ar;i++)
      dict=EmbeddedTVars(tplArg(input,i),depth,&depth,isSafe,mode,assign,
			 dict,lo,outer,code);

    *dp=depth;
    return dict;
  }
  else if(IsForAll(input,&lhs,&rhs)){
    dictpo nouter = declVar(lhs,lhs,localvar,mode,notInited,False,0,0,outer);
    dict = EmbeddedTVars(rhs,depth,dp,isSafe,mode,assign,dict,lo,nouter,code);
    clearDict(nouter,outer);
    return dict;
  }
  else if(IsTypeVar(input)){
    if(!IsLocalVar(input,dict)
       ||(IsLocalVar(input,outer)&&!IsLocalVar(input,lo))){
      dict=declVar(input,input,localvar,mode,notInited,False,--depth,-1,dict);
      if(!isSafe)
	genIns(code,"not safe",initv,depth);
    }
    *dp = depth;
    return dict;
  }
  else{
    *dp = depth;
    return dict;
  }
}


/* Verify that a pattern has or doesn't have an any pattern in it */

static logical GuardEmbeddedAny(cellpo input);

logical EmbeddedAny(cellpo input)
{
  cellpo lhs,rhs;

  if(IsHashStruct(input=deRef(input),kdebug,&input))
    return EmbeddedAny(input);
  else if(IsHashStruct(input,knodebug,&input))
    return EmbeddedAny(input);

  else if((IsQuery(input,NULL,&rhs)||
	   isUnaryCall(input,kquery,&rhs))&&isSymb(rhs))
    return False;

  else if(isBinaryCall(input,ktcoerce,NULL,&rhs))
    return EmbeddedAny(rhs);

  else if(isBinaryCall(input,ktilda,&lhs,NULL))
    return EmbeddedAny(lhs);

  else if(isBinaryCall(input,khat,&lhs,NULL))
    return EmbeddedAny(lhs);

  else if(IsAnyStruct(input,&rhs))
    return True;

  /* Field name pattern */
  else if(isBinaryCall(input,kmatch,&lhs,NULL))
    return EmbeddedAny(lhs);

  else if(IsEnumStruct(input,NULL))
    return False;

  else if(isSymb(input))
    return False;

  else if(isBinaryCall(input,kguard,&lhs,&rhs))
    return EmbeddedAny(lhs) || GuardEmbeddedAny(rhs);

  else if(IsQuote(input,NULL))
    return False;

  else if(IsShriek(input,NULL))
    return False;
  
  else if(isCons(input)){
    unsigned int ar=consArity(input);
    unsigned int i;
                    
    for(i=0;i<ar;i++)
      if(EmbeddedAny(consEl(input,i)))
        return True;
    return False;
  }
  else if(isTpl(input)){
    int ar=tplArity(input);
    int i;

    for(i=0;i<ar;i++)
      if(EmbeddedAny(tplArg(input,i)))
        return True;
    return False;
  }
  else if(isNonEmptyList(input))
    return EmbeddedAny(listHead(input)) || EmbeddedAny(listTail(input));
  else
    return False;
}


static logical GuardEmbeddedAny(cellpo input)
{
  cellpo lhs,rhs;

  if(IsHashStruct(input,kdebug,&input))
    return GuardEmbeddedAny(input);
  else if(IsHashStruct(input,knodebug,&input))
    return GuardEmbeddedAny(input);
  else if(isBinaryCall(input,kmatch,&lhs,NULL))
    return EmbeddedAny(lhs);
  else if(isBinaryCall(input,kin,&lhs,NULL))
    return EmbeddedAny(lhs);
  else if(isBinaryCall(input,kand,&lhs,&rhs) ||
	  isBinaryCall(input,kdand,&lhs,&rhs)){
    return GuardEmbeddedAny(lhs) || GuardEmbeddedAny(rhs);
  }
  else
    return False;
}



/* Check a pattern to determine where to extract to */
static int PtnDest(cellpo input,dictpo dict,int *depth)
{
  cellpo rhs;

  if(IsHashStruct(input,kdebug,&input))
    return PtnDest(input,dict,depth);
  else if(IsHashStruct(input,knodebug,&input))
    return PtnDest(input,dict,depth);
  else if(IsQuery(input,NULL,&rhs)){
    if(isSymb(rhs)){
      vartype vt;
      int off,ind;
      initStat inited;
      assMode rw;
      cellpo vtp;

      if(IsVar(rhs,NULL,&vtp,&off,&ind,&rw,&inited,&vt,dict) && 
	 vt==localvar && inited==notInited){
	return off;
      }
      else if(IsSymbol(rhs,kuscore)) /* We are ignoring the result... */
	return (*depth)-2;
      else
	return --(*depth);
    }
    else
      return --(*depth);	/* Should never happen */
  }
  else if(isUnaryCall(input,kquery,&rhs)){
    if(isSymb(rhs)){
      vartype vt;
      int off,ind;
      initStat inited;
      assMode rw;
      cellpo vtp;

      if(IsVar(rhs,NULL,&vtp,&off,&ind,&rw,&inited,&vt,dict) && 
	 vt==localvar && inited==notInited){
	return off;
      }
      else if(IsSymbol(rhs,kuscore))
	return (*depth)-2;
      else
	return --(*depth);
    }
    else
      return --(*depth);	/* Should never happen */
  }
  else if(IsSymbol(input,kuscore)) /* We are ignoring the result... */
    return (*depth)-2;
  else if(isSymb(input)){
    vartype vt;
    int off,ind;
    initStat inited;
    assMode rw;
    cellpo vtp;

    if(IsVar(input,NULL,&vtp,&off,&ind,&rw,&inited,&vt,dict) && 
       vt==localvar && inited==notInited){
      return off;
    }
    else
      return --(*depth);
  }
  else
    return --(*depth);		/* Might happen */
}

/* Check a pattern for var-pattern */
cellpo VarDecPtn(cellpo input,cellpo type,cellpo *name,dictpo dict)
{
  cellpo lhs,rhs;

  if(IsHashStruct(input,kdebug,&input))
    return VarDecPtn(input,type,name,dict);
  else if(IsHashStruct(input,knodebug,&input))
    return VarDecPtn(input,type,name,dict);
  else if(isSymb(input)){
    *name = input;
    return typeInfo(input);
  }
  else if(IsQuery(input,&lhs,&rhs)){
    *name = input;
    return typeInfo(rhs);
  }
  else if(isUnaryCall(input,kquery,&rhs)){
    *name = input;
    return typeInfo(rhs);
  }
  else
    return NULL;
}

/* Is a pattern safe wrt GC -- i.e., can GC be invoked during it? */

logical gcSafePtn(cellpo input,dictpo dict)
{
  cellpo lhs,rhs;

  input = deRef(input);

  if(IsHashStruct(input,kdebug,&input))
    return gcSafePtn(input,dict);
  else if(IsHashStruct(input,knodebug,&input))
    return gcSafePtn(input,dict);
  else if(IsTypeVar(input))
    return False;		/* should never happen */
  else if(isSymb(input))
    return True;		/* symbols are always safe ... */
  else if(IsEnumStruct(input,NULL))
    return True;
  else if(IsInt(input))
    return True;
  else if(IsFloat(input))
    return True;
  else if(IsQuery(input,&lhs,&rhs))
    return gcSafePtn(lhs,dict);
  else if(isUnaryCall(input,kquery,&rhs))
    return True;
  /* string matching ... */
  else if(isBinaryCall(input,kcatenate,NULL,NULL) ||
	  isBinaryCall(input,kappend,NULL,NULL) ||
	  isUnaryCall(input,kstar,NULL))
    return False;

  else if(isBinaryCall(input,kguard,&input,&rhs))
    return gcSafePtn(input,dict) && gcSafeExp(rhs,dict);

  else if(isBinaryCall(input,kfn,&lhs,&rhs) && isTpl(lhs=deRef(lhs)))
    return True;
  else if(isCall(input,kblock,NULL))
    return True;

  else if(IsQuote(input,&input))
    return True;

  else if(IsString(input))
    return True;

  else if(IsShriek(input,NULL))
    return False;		/* expression pattern */

  else if(IsTuple(input,NULL)){
    int ar=tplArity(input);
    int i=0;

    for(i=0;i<ar;i++){
      cellpo arg = tplArg(input,i);

      if(!gcSafePtn(arg,dict))
	return False;
    }
    return True;
  }
  
  else if(isCons(input))
    return False;		/* An approximation ... */

  else if(IsList(input)){	/* We are matching a list pair ... */
    if(isEmptyList(input))
      return True;
    else{
      input = listVal(input);

      return gcSafePtn(input,dict) && gcSafePtn(Next(input),dict);
    }
  }
  else
    reportErr(lineInfo(input),"cant understant pattern `%4.1800w'",input);
  return True;
}


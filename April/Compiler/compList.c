/*
  List pattern matching implementation
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

static lblpo lstPtn(cellpo ptn,logical last,int this,int *next,
		    int depth,int *dp,lblpo LF,dictpo dict,dictpo outer,
		    cpo code,assMode mode,int dLvl)
{
  cellpo lhs,rhs;

  if(isBinaryCall(ptn,kcatenate,&lhs,&rhs)||isBinaryCall(ptn,kappend,&lhs,&rhs)){
    int I = 0;
    lblpo Lf = lstPtn(lhs,False,this,&I,depth,&depth,LF,dict,outer,code,mode,dLvl);

    return lstPtn(rhs,last,I,next,depth,dp,Lf,dict,outer,code,mode,dLvl);
  }
  /* slice off a single character */
  else if(isBinaryCall(ptn,kchrcons,&lhs,&rhs)){
    int C = --depth;
    int I = *dp = --depth;
    
    genIns(code,"must be non-empty",mlist,this,C,I);
    genIns(code,"fail list comp",jmp,LF);	/* try something else */

    compPttrn(lhs,C,depth,&depth,LF,dict,&dict,dict,outer,code,mode,dLvl);
    return lstPtn(rhs,last,I,next,depth,dp,LF,dict,outer,code,mode,dLvl);
  }
  
  else if(isEmptyList(ptn)){
    if(last)
      genIns(code,"must be empty",mnil,this,LF);
    else{
      *next = this;
      *dp = depth;
    }
    return LF;
  }
  else if(isNonEmptyList(ptn)){
    int C = --depth;
    int I = *dp = --depth;
        
    genIns(code,"must be non-empty",mlist,this,C,I);
    genIns(code,"fail list comp",jmp,LF);	/* try something else */
  
    compPttrn(listHead(ptn),C,depth,&depth,LF,dict,&dict,dict,outer,code,mode,dLvl);
    
    return lstPtn(listTail(ptn),last,I,next,depth,dp,LF,dict,outer,code,mode,dLvl);
  }  

  else if(isSymb(ptn)){
    int var = 0;
    initStat inited;

    if(IsSymbol(ptn,kuscore)){
      if(last){
	*next = *dp = depth;
	return LF;
      }
      else{
	lblpo Le = NewLabel(code);
	lblpo Lf = NewLabel(code);

	*next = *dp = --depth;
	genIns(code,"Prepare the end variable",move,this,*next);
	genIns(code,"skip backtrack code",jmp,Le);
	DefLabel(code,Lf);
	genIns(code,"next element",mlist,*next,depth-1,*next);
	genIns(code,"run out of elements",jmp,LF);
	DefLabel(code,Le);
	return Lf;
      }
    }
    else if(IsVar(ptn,NULL,NULL,&var,NULL,NULL,&inited,NULL,dict)){
      if(!inited||mode==multiAss){
	if(last){
	  *next = *dp = depth;
	  genIns(code,symN(symVal(ptn)),move,this,var);
	  InitVar(ptn,True,dict);
	  return LF;
	}
	else{
	  lblpo Le = NewLabel(code);
	  lblpo Lf = NewLabel(code);

          genIns(code,"Try empty list first",movl,emptylist,var);
	  *next = *dp = --depth;
	  genIns(code,"Prepare the end variable",move,this,*next);
	  InitVar(ptn,True,dict);
	  genIns(code,"skip backtrack code",jmp,Le);
	  DefLabel(code,Lf);
	  genIns(code,"move pointer one element to the right",mlist,*next,depth-1,*next);
	  genIns(code,"on failure...",jmp,LF);
	  genIns(code,"add to var",add2lst,depth-1,depth-1,var);
	  DefLabel(code,Le);
	  return Lf;
	}
      }
      else{     // A literal sub-string match
	int l = 0;

	compExp(ptn,typeInfo(ptn),&l,depth,&depth,dict,outer,code,dLvl);

	if(last){
	  *next = *dp = depth;
	  genIns(code,"Test literal",neq,this,l);
	  genIns(code,NULL,jmp,LF);
	}
	else{	  
	  *next = *dp = --depth;
	  genIns(code,"copy input",move,this,*next);
	  
	  genIns(code,"test for literal",prefix,*next,l,*next);
	  genIns(code,"failing...",jmp,LF);
	}
	return LF;
      }
    }
    else{
      reportErr(lineInfo(ptn),"invalid list pattern %w",ptn);
      return LF;
    }
  }
  else if(IsQuery(ptn,&lhs,&ptn)) /* this is treated the same as a variable... */
    return lstPtn(ptn,last,this,next,depth,dp,LF,dict,outer,code,mode,dLvl);
  else if(isUnaryCall(ptn,kquery,&ptn))
    return lstPtn(ptn,last,this,next,depth,dp,LF,dict,outer,code,mode,dLvl);
  else if(isBinaryCall(ptn,ktcoerce,&lhs,&rhs)){
    if(isSymb(lhs) && symVal(lhs)==kstring)     // a little bit cruft
      return lstPtn(rhs,last,this,next,depth,dp,LF,dict,outer,code,mode,dLvl);
    else{
      reportErr(lineInfo(lhs),"this pattern -- %w --is not supported",ptn);
      return LF;
    }
  }
  else if(isBinaryCall(ptn,kin,&lhs,&rhs)){
    if(!isSymb(lhs)){
      reportErr(lineInfo(lhs),"this pattern -- %w --is not supported",ptn);
      return NULL;
    }
    else{
      int L = --depth;

      compExp(rhs,NULL,&L,depth,&depth,dict,outer,code,dLvl);
      
      if(last){                                 // This becomes a regular in test
	lblpo Lloop = NewLabel(code);
	int el = --depth;
	
	*next = *dp = depth;
	DefLabel(code,Lloop);
	
	genIns(code,"test for end of set",mlist,L,el,L);
	genIns(code,"failed",jmp,LF);
	
	genIns(code,"is it the right data?",neq,el,this);
	genIns(code,"Try another",jmp,Lloop);
	return Lloop;                           // Come back here on failure
      }
      else{
	lblpo Lloop = NewLabel(code);
	int el = --depth;
	
	*next = *dp = --depth;
	DefLabel(code,Lloop);
	
	genIns(code,"test for end of set",mlist,L,el,L);
	genIns(code,"failed",jmp,LF);
	
	genIns(code,"is it the right data?",prefix,this,el,this);
	genIns(code,"Try another",jmp,Lloop);
	return Lloop;                           // Come back here on failure
      }
    }
  }

  /* fixed width portion of a list */
  else if(isBinaryCall(ptn,ktilda,&lhs,&rhs)){
    int l=0;
    int T=0;
    int end=0;

    /* compute the width of the string to pinch off */
    compExp(rhs,numberTp,&l,depth,&depth,dict,outer,code,dLvl);

    *next = *dp = --depth;
    
    genIns(code,"copy string",move,this,*next);
    genIns(code,"snip off fixed prefix",snip,*next,l,T=--depth);
    genIns(code,"fail...",jmp,LF);	/* try something else */
    
    LF = lstPtn(lhs,last,T,&end,depth,&depth,LF,dict,outer,code,mode,dLvl);
    genIns(code,"Test for end of string",mnil,end,LF);
    return LF;
  }

  else if(isBinaryCall(ptn,kguard,&lhs,&rhs)){
    lblpo Lf=lstPtn(lhs,last,this,next,depth,&depth,LF,dict,outer,code,mode,dLvl);

    compTest(rhs,depth,dp,Lf,dict,&dict,outer,code,dLvl);
    return Lf;
  }
  else{
    int l = 0;

    compExp(ptn,typeInfo(ptn),&l,depth,&depth,dict,outer,code,dLvl);

    *next = *dp = --depth;

    if(last){
      genIns(code,"Test literal",neq,this,l);
      genIns(code,NULL,jmp,LF);
      genIns(code,"dummy",movl,emptylist,*next);
    }
    else{
      genIns(code,"copy string to out",move,this,*next);
      genIns(code,"test now",prefix,*next,l,*next);
      genIns(code,"failing...",jmp,LF);
    }
	  
    return LF;
  }
}

logical compListPtn(cellpo input,int var,int depth,int *dp,lblpo LF,
		   dictpo dict,dictpo outer,cpo code,assMode mode,int dLvl)
{
  int DP;
  int end=0;
 
  lstPtn(input,True,var,&end,depth,&DP,LF,dict,outer,code,mode,dLvl);

  return True;			/* dummy */
}

/*
  Compile a choice -- of expressions or functions 
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
#include <limits.h>		/* We need these */
#include "compile.h"		/* Compiler structures */
#include "generate.h"		/* Code generation interface */
#include "types.h"		/* Type cheking interface */
#include "dict.h"
#include "keywords.h"		/* Standard April keywords */
#include "opcodes.h"		/* April machine opcodes */
#include "assem.h"		/* Code generation*/
#include "cellP.h"
#include "cdbgflags.h"		/* Debugging option flags */

static long pushStk(cellpo input)
{
  cellpo lhs,rhs;
  if(isBinaryCall(input,kchoice,&lhs,&rhs))
    return pushStk(rhs) + pushStk(lhs);	/* reverse the order! */
  else{
    p_cell(input);
    return 1;
  }
}

static void reshapeTree(cellpo input,cellpo choices,dictpo dict,long *segs)
{
  long count = pushStk(input);

  while(count-->0){
    cellpo top = topStack();
    cellpo lhs,rhs;
    cellpo tgt = choices++;

    (*segs)++;

    if(isBinaryCall(top,kstmt,&lhs,&rhs)||isBinaryCall(top,kfn,&lhs,&rhs)){
      if(LiteralSymbPtn(lhs,NULL,dict)){
	cellpo t = tgt;
	popcell(tgt);		/* put last into the target */

	while(count>0 && (isBinaryCall(top=topStack(),kstmt,&lhs,NULL)||
			  isBinaryCall(top,kfn,&lhs,NULL)) && 
	      LiteralSymbPtn(lhs,NULL,dict)){
	  BuildBinCall(kchoice,t,top,t);
	  t = callArg(t,1);
	  count--;
	  p_drop();
	}
      }
      else if(IsInteger(lhs)){
	cellpo t = tgt;
	popcell(tgt);		/* put last into the target */

	while(count>0 && (isBinaryCall(top=topStack(),kstmt,&lhs,NULL)||
			  isBinaryCall(top,kfn,&lhs,NULL)) && 
	      IsInteger(lhs)){
	  BuildBinCall(kchoice,t,top,t);
	  t = callArg(t,1);
	  count--;
	  p_drop();
	}
      }
      else
	popcell(tgt);
    }
    else
      popcell(tgt);
  }
}

static long countDisj(cellpo input)
{
  cellpo lhs,rhs;
  if(isBinaryCall(input,kchoice,&lhs,&rhs))
    return countDisj(lhs)+countDisj(rhs);
  else
    return 1;
}

logical compChoice(cellpo input,int arity,int srcvar,int depth,
		   lblpo Next,lblpo LF,dictpo dict,cpo code,
		   int dLvl,choiceFun comp,void *cl)
{
  long i,segs=0;
  long max = countDisj(input);
  cell choices[max];
  logical mayFail = False;
  
  reshapeTree(input,choices,dict,&segs);

  for(i=0;i<segs;i++){
    logical last = i==segs-1;	/* last choice block in the sequence? */
    lblpo Lf = (last?LF:NewLabel(code));

    if(buildHash(&choices[i],arity,srcvar,depth,Lf,Next,dict,code,
		 dLvl,comp,cl))
      mayFail = True;
    else
      mayFail|=comp(&choices[i],arity,srcvar,depth,Next,Lf,dict,code,
		    !mayFail&&last,dLvl,cl);

    if(!last)
      DefLabel(code,Lf);	/* define the error label */
  }

  return mayFail;
}


/*
  Type construction and type matching code
  (c) 1994-2002 Imperial College and F.G.McCabe

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
#include "fold.h"		/* Expression fold */
#include "dict.h"
#include "keywords.h"		/* Standard April keywords */
#include "opcodes.h"		/* April machine opcodes */
#include "assem.h"		/* Code generation*/
#include "cdbgflags.h"		/* Debugging option flags */

/* 
 * Generate the code to generate a type signature, possibly by inserting
 * values of type variables
 */
 
static void compTp(int lineInfo,cellpo type,int *dest,int depth,int *dp,dictpo dict,cpo code,int dLvl);


void compTypeExp(cellpo input,cellpo type,int *dest,int depth,int *dp,dictpo dict,cpo code,int dLvl)
{
  compTp(lineInfo(input),type,dest,depth,dp,dict,code,dLvl);
}


static void compTp(int lineInfo,cellpo type,int *dest,int depth,int *dp,dictpo dict,cpo code,int dLvl)
{
  if(isSymb(type=deRef(type)))
    genIns(code,"type symbol",movl,type,genDest(dest,depth,dp,code));
  else if(IsTypeVar(type)){
    cellpo ci;
    vartype vt;
    centry et;
    int ind;			/* index variable */
    int off;			/* offset of variable */
    initStat inited;

    if(IsVar(type,&et,&ci,&off,&ind,NULL,&inited,&vt,dict)){
      switch(vt){
      case localvar:
        if(inited==notInited){
          genIns(code,"new type var",ivar,depth,off);
          genMove(code,off,dest,NULL);
          InitVar(type,True,dict);
        }
        else{
          genMove(code,off,dest,"subsequent type var");
        }
	break;
      case indexvar:
        if(inited==Inited){
          InitVar(type,True,dict);
          genIns(code,"indexed type var",indxfld,off,ind,genDest(dest,depth,dp,code));
        }
        else{
          int D = genDest(dest,depth,dp,code);
          genIns(code,"new tvar",ivar,depth,D);
          genIns(code,"update now",tpupdte,D,ind,off);
        }
	break;
      case envvar:
        assert(inited==Inited);	// We're in big trouble
        genIns(code,"environment tvar",emove,off,genDest(dest,depth,dp,code));
	break;
      default:
        reportErr(lineInfo,"invalid place for a type variable");
	break;
      }
    }
    else{
      int DP;
      int D = genDest(dest,depth,&DP,code);
      
      genIns(code,"dummy type var",ivar,depth,D);
    }
  }
  else if(IsQuery(type,&type,NULL))
    compTp(lineInfo,type,dest,depth,dp,dict,code,dLvl);
  else if(isCons(type)){
    int D = genDest(dest,depth,dp,code);
    unsigned long ar=consArity(type);
    unsigned long i;

    genIns(code,"Set up constructor",movl,consFn(type),--depth);
      
    for(i=0;i<ar;i++){
      int eD=depth-1;
      int DP;
      cellpo el = consEl(type,i);

      compTp(lineInfo,el,&eD,depth--,&DP,dict,code,dLvl);
    }
    genIns(code,"Type constructor",loc2cns,depth,ar,D);
  }
  else if(isTpl(type)){
    int D=genDest(dest,depth,dp,code);
    int ar=tplArity(type);
    int i;
    int DP;
    
    genIns(code,"Set up constructor",movl,tplTp,--depth);

    for(i=0;i<ar;i++){
      int eD=0;
      cellpo el = tplArg(type,i);

      compTp(lineInfo,el,&eD,depth--,&DP,dict,code,dLvl);
      genMove(code,eD,&depth,NULL);
    }
    DP=depth;
    genIns(code,NULL,loc2cns,DP,ar,D);
  }
  else
    reportErr(lineInfo,"can't generate code for type %w",type);
}

static void compTpPtn(int lineInfo,cellpo type,int var,int depth,lblpo LF,dictpo dict,cpo code,int dLvl);

void compTypePtn(cellpo where,cellpo type,int var,int depth,lblpo LF,dictpo dict,cpo code,int dLvl)
{
  compTpPtn(lineInfo(where),type,var,depth,LF,dict,code,dLvl);
}



static void compTpPtn(int lineInfo,cellpo type,int var,int depth,lblpo LF,dictpo dict,cpo code,int dLvl)
{
  if(isSymb(type=deRef(type))){
    genIns(code,"type symbol",ulit,var,type);
    genIns(code,"on failure",jmp,LF);
  }
  else if(IsTypeVar(type)){
    cellpo ci;
    vartype vt;
    centry et;
    int ind;			/* index variable */
    int off;			/* offset of variable */
    initStat inited;

    if(IsVar(type,&et,&ci,&off,&ind,NULL,&inited,&vt,dict)){
      switch(vt){
      case localvar:
        if(inited==notInited){
          genMove(code,var,&off,"new type var");
          InitVar(type,True,dict);
        }
        else{
          genIns(code,"subsequent type var",uvar,var,off);
          genIns(code,"in case of failure",jmp,LF);
        }
	break;
      case indexvar:
        if(inited==Inited){
          genIns(code,"indexed type var",indxfld,off,ind,depth-1);
          genIns(code,"test it",uvar,var,depth-1);
          genIns(code,NULL,jmp,LF);
        }
        else
          reportErr(lineInfo,"uninited index var");
	break;
      case envvar:
        genIns(code,"environment tvar",emove,off,--depth);
        genIns(code,"test it",uvar,var,depth);
        genIns(code,NULL,jmp,LF);
	break;
      default:
        reportErr(lineInfo,"invalid place for a type variable");
	break;
      }
    }
    else
      reportErr(lineInfo,"unknown type variable");
  }
  else if(IsQuery(type,&type,NULL))
    return compTpPtn(lineInfo,type,var,depth,LF,dict,code,dLvl);
  else if(isCons(type)){
    unsigned long ar=consArity(type);
    unsigned long i;
    lblpo Lx = NewLabel(code);
    lblpo Le = NewLabel(code);
    dictTrail trail = currentTrail(dict);
    
    genIns(code,"first test for tvar",isvr,var,Le);
    compTp(lineInfo,type,&var,depth,&depth,dict,code,dLvl);
    genIns(code,"We're OK",jmp,Lx);
    
    resetDict(trail,dict);
    
    DefLabel(code,Le);
    genIns(code,"Test constructor",ucns,var,ar);
    genIns(code,"Wrong arity",jmp,LF);
    genIns(code,"Try the constructor",mcnsfun,var,consFn(type));
    genIns(code,"Wrong contructor",jmp,LF);
    
    for(i=0;i<ar;i++){
      int eD=depth-1;
      cellpo el = consEl(type,i);
      
      genIns(code,"access constructor element",consfld,var,i,eD);

      compTpPtn(lineInfo,el,eD,depth-1,LF,dict,code,dLvl);
    }
    DefLabel(code,Lx);
  }
  else if(isTpl(type)){
    long ar=tplArity(type);
    long i;
    lblpo Lx = NewLabel(code);
    lblpo Le = NewLabel(code);
    dictTrail trail = currentTrail(dict);
    
    genIns(code,"first test for tvar",isvr,var,Le);
    compTp(lineInfo,type,&var,depth,&depth,dict,code,dLvl);
    genIns(code,"We're OK",jmp,Lx);
    
    resetDict(trail,dict);
    
    DefLabel(code,Le);
    genIns(code,"Test tuple",ucns,var,ar);
    genIns(code,"Wrong arity",jmp,LF);
    genIns(code,"Try the tuple constructor",mcnsfun,var,tplTp);
    genIns(code,"Wrong contructor",jmp,LF);
    
    for(i=0;i<ar;i++){
      int eD=depth-1;
      cellpo el = tplArg(type,i);
      
      genIns(code,"access constructor element",consfld,var,i,eD);

      compTpPtn(lineInfo,el,eD,depth-1,LF,dict,code,dLvl);
    }
    DefLabel(code,Lx);
  }
  else
    reportErr(lineInfo,"can't generate pattern code for type %w",type);
}

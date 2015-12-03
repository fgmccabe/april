/*
  Generate debugging instructions
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
#include <assert.h>		/* Compile-time assertions */
#include <string.h>		/* standard string processing functions */
#include "compile.h"		/* Compiler structures */
#include "types.h"		/* Type cheking interface */
#include "dict.h"
#include "keywords.h"		/* Standard April keywords */
#include "opcodes.h"		/* April machine opcodes */
#include "assem.h"		/* Code generation*/
#include "makesig.h"		/* Signature handling*/
#include "cdbgflags.h"		/* Debugging option flags */
#include "generate.h"		/* Code generation ...  */

/* Symbolic debugging code generation */
void genLineDebugInfo(cpo code,int depth,cellpo in,logical test)
{
  int line = source_line(in);

  if(line!=-1 && line!=LDebugCode(code)){
    int farity;
    int esc_code=IsEscapeProc(kwaitdebug,&farity,NULL);
    
    assert(esc_code!=-1);
    
    if(test){
      lblpo lX = NewLabel(code);

      genIns(code,"Test for debugging",debug_d,lX);
      genIns(code,NULL,line_d,depth,source_file(in),source_line(in));
      genIns(code,symN(kwaitdebug),esc_fun,depth,esc_code);
      SetDebugLine(code,line);	/* set the debug flag */
      DefLabel(code,lX);
    }
    else{
      genIns(code,NULL,line_d,depth,source_file(in),source_line(in));
      genIns(code,symN(kwaitdebug),esc_fun,depth,esc_code);
      SetDebugLine(code,line);	/* set the debug flag */
    }    
  }
}

void genDebugExp(cpo code,int *dest,int depth,int *dp,cellpo type,int res,
		 dictpo dict,dictpo outer)
{
  genDest(dest,depth,dp,code);

  if(!IsTypeVar(deRef(type)) &&
     IsVar(cdebugdisplay,NULL,NULL,NULL,NULL,NULL,NULL,NULL,dict)){
    int tD = --depth;
    int DP;

    compTypeExp(voidcell,type,&tD,depth,&DP,dict,code,0);

    genIns(code,"set up value",move,res,--depth);
    genIns(code,NULL,loc2any,tD,depth,DP);

    funCall(cdebugdisplay,1,DP,dict,code,0);
    genMove(code,DP,dest,"Collect result of debugging display");
  }
  else{
    int reslt = --depth;

    genMove(code,res,&depth,NULL);

    genIns(code,"Full precision",movl,c_zero,--depth);
    genIns(code,"Full depth",movl,c_zero,--depth);
    genIns(code,"strof",esc_fun,depth,IsEscapeFun(locateC("strof"),NULL,NULL,NULL));
    genMove(code,reslt,dest,NULL);
  }
}

void genRtnDebug(cpo code,int depth,cellpo name,cellpo type,int res,dictpo dict,dictpo outer)
{
  int DD = 0;
  int MM = --depth;
  lblpo lX = NewLabel(code);

  genIns(code,"Test for debugging",debug_d,lX);
  genIns(code,"name of function",movl,name,MM);
    
  genDebugExp(code,&DD,depth,&depth,type,res,dict,outer);
  genIns(code,NULL,return_d,depth,MM,DD);
  DefLabel(code,lX);
}

void genEntryDebug(cpo code,int depth,cellpo name)
{
  if(symVal(name)!=ktoplevel){
    genIns(code,NULL,entry_d,depth,name);
  }
}

void genExitDebug(cpo code,int depth,cellpo name)
{
  genIns(code,NULL,exit_d,depth,name);
}

void genScopeDebug(cpo code,int depth,dictpo dict)
{
  int lexLevel = countMarkers(dict);

  if(lexLevel!=CodeScope(code)){
    SetCodeScope(code,lexLevel);
    genIns(code,"Variable scope marker",scope_d,depth,lexLevel);
  }
}

void genForkDebug(cpo code,int depth,int var)
{
  genIns(code,"debug a fork",fork_d,depth,var);
}

static logical isHashName(cellpo name)
{
  uniChar *n;
  if(isSymb(name))
    n = symVal(name);
  else if(IsString(name))
    n = strVal(name);
  else
    return False;
   
  return uniSearch(n,uniStrLen(n),'#')!=NULL;
}

void genVarDebug(cpo code,int depth,cellpo name,cellpo type,int res,
		 dictpo dict,dictpo outer)
{
  if(!isHashName(name)){
    int DD = 0;
    int MM = --depth;
    lblpo lX = NewLabel(code);

    genIns(code,"Test for debugging",debug_d,lX);

    genIns(code,"name of variable",movl,name,MM);
    
    genDebugExp(code,&DD,depth,&depth,type,res,dict,outer);

    genIns(code,NULL,assign_d,depth,MM,DD);
    DefLabel(code,lX);
  }
}

void genSendDebug(cpo code,int depth,int to,int msg,cellpo type,
		  dictpo dict,dictpo outer)
{
  int DD = 0;
  lblpo lX = NewLabel(code);

  genIns(code,"Test for debugging",debug_d,lX);

  genDest(&DD,depth,&depth,code);

  if(!IsTypeVar(deRef(type)) &&
     IsVar(cdebugdisplay,NULL,NULL,NULL,NULL,NULL,NULL,NULL,dict)){
    genIns(code,"set up value",move,msg,--depth);

    funCall(cdebugdisplay,1,depth,dict,code,0);
    genMove(code,depth,&DD,"Collect result of debugging display");
    genIns(code,"Message send debug",send_d,depth,to,DD);
  }
  else{
    int S = --depth;
    int X = --depth;
    lblpo LF = NewLabel(code); 
    
    genIns(code,"Match message's any structure",many,msg,S,X);
    genIns(code,"Shouldnt need this",jmp,LF);
    
    DefLabel(code,LF);
    genIns(code,"Full precision",movl,c_zero,--depth);
    genIns(code,"Full depth",movl,c_zero,--depth);
    genIns(code,"strof",esc_fun,depth,IsEscapeFun(locateC("strof"),NULL,NULL,NULL));
    genMove(code,depth+2,&DD,NULL);
    genIns(code,"Message send debug",send_d,DD,to,DD);
  }
  DefLabel(code,lX);
}

typedef struct {
  int depth;
  cpo code;
  dictpo dict;
  dictpo outer;
} vdebugRec;

static void iDebugVar(dictpo var,void *cl)
{
  cellpo name = dictName(var);

  if(name!=NULL && isSymb(name) && 
     dictInited(var,name) && !dictReported(var,name) && 
     dictCe(var,name)==cvar && dictVt(var,name)==localvar){
    vdebugRec *info = (vdebugRec*)cl;


    genVarDebug(info->code,info->depth,name,dictType(var,name),
		dictOffset(var,name),info->dict,info->outer);
    setDictReported(var,name,True);
  }
}


void genPtnVarDebug(cpo code,int depth,dictpo top,dictpo dict,dictpo outer)
{
  vdebugRec info = {depth,code,top,outer};

  processDict(top,dict,iDebugVar,(void*)&info);
}

void genSuspendDebug(int dLvl,cpo code,int depth)
{
  if(dLvl>0)
    genIns(code,"report suspension",suspend_d,depth);
}

void genErrorDebug(dictpo dict,int depth,cpo code)
{
  int D=depth-1;
  genIns(code,"Extract error value",moverr,D);
  genIns(code,"Error debug",error_d,D,D);
}

#ifndef _GENERATE_H_
#define _GENERATE_H_

/* Interface for code generation in the April compiler */
void compileProg(cellpo prog,dictpo dict,ioPo outFile,optionPo info);

void compLambda(cellpo fun,cellpo type,int *dest,int depth,int *dp,
		cpo cde,dictpo dict,int dLvl);

void compMu(cellpo proc,cellpo type,int *dest,int depth, int *dp, cpo code,dictpo dict,
	    int dLvl);

void compTheta(cellpo body,cellpo type,int *dest,int depth,int *dp,cpo code,
	       dictpo dict,int dLvl);

dictpo BoundVars(cellpo args,int srcvar,int ar,dictpo base,int depth,cpo code,
		 int dLvl);

void compStmt(cellpo input,int depth,int *dp,cxpo ctxt,lblpo Next,
	      dictpo dict,dictpo *ndict,dictpo outer,cont *cnt, cpo code,
	      int dLvl);

void procCall(cellpo f,int arity,int depth,dictpo dict,cpo code,int dLvl);

logical LiteralSymbPtn(cellpo input,cellpo *sym,dictpo dict);

typedef logical (*choiceFun)(cellpo input,int arity,int srcvar,
			     int depth,lblpo Next,lblpo LF,
			     dictpo dict,cpo code,
			     logical last,int dLvl,void *cl);

logical compChoice(cellpo input,int arity,int srcvar,int depth,
		   lblpo Next,lblpo LF,dictpo dict,cpo code,
		   int dLvl,choiceFun comp,void *cl);

logical buildHash(cellpo input,int arity,int srcvar,int depth,
		  lblpo LF,lblpo Next,dictpo dict,cpo code,
		  int dLvl,
		  choiceFun comp,void *cl);

void matchError(cellpo input,int depth,dictpo dict,cpo code,int dLvl);
void failError(cellpo input,int depth,dictpo dict,cpo code,int dLvl);
void genError(cellpo input,char *msg,cellpo ecode,int depth,dictpo dict,
	      cpo code,int dLvl);

void compExp(cellpo input,cellpo type,int *dest,int depth,int *dp,
	     dictpo dict,dictpo outer,cpo code,int dLvl);

void anyExp(cellpo exp,int *dest,int depth,
	    int *dp,dictpo dict,dictpo outer,cpo code,int dLvl);

void compTypeExp(cellpo input,cellpo type,int *dest,int depth,int *dp,dictpo dict,cpo code,int dLvl);

long compArguments(cellpo input,int depth,int *dp,dictpo dict,dictpo outer,
		   cpo code,int dLvl);

void funCall(cellpo f,int arity,int depth,dictpo dict,cpo code,int dLvl);

logical EmbeddedAny(cellpo input);

logical compPttrn(cellpo input,int var,int depth,int *dp,lblpo LF,
		  dictpo dict,dictpo *nd,dictpo lo,dictpo outer,cpo code,
		  assMode mode,int dLvl);
	       
logical compExpPttrn(cellpo input,cellpo exp,int depth,int *dp,
		     lblpo LF,dictpo dict,dictpo *nd,dictpo lo,dictpo outer,
		     cpo code,assMode mode,logical assign,int dLvl);
		    

logical compListPtn(cellpo input,int var,int depth,int *dp,lblpo LF,
		   dictpo dict,dictpo outer,cpo code,assMode mode,int dLvl);

void compTypePtn(cellpo where,cellpo type,int var,int depth,lblpo LF,dictpo dict,cpo code,int dLvl);

logical gcSafePtn(cellpo input,dictpo dict);
logical gcSafeExp(cellpo input,dictpo dict);

/* Decide if a pattern needs a separate type test */
logical safePtn(cellpo input,dictpo dict);

logical safeType(cellpo type);

dictpo EmbeddedVars(cellpo input,int depth,int *dp,logical isSafe,
		    assMode mode,logical assign,
		    dictpo dict,dictpo lo,dictpo outer,cpo code);

void compTest(cellpo input,int depth,int *dp,lblpo LF,
	      dictpo dict,dictpo *ndict,dictpo outer,cpo code,int dLvl);
	      
logical genMove(cpo code,int from,int *to,char *cmmnt);

dictpo extendDict(cellpo exp,int E,cpo code,int depth,int *dp,dictpo dict);

/* generate symbolic debugging info */

extern int dLvl;	/* True if generating symbolic debugging */

void genDebugExp(cpo code,int *dest,int depth,int *dp,cellpo type,int res,
		 dictpo dict,dictpo outer);
void genLineDebugInfo(cpo code,int depth,cellpo in,logical testDebug);
void genRtnDebug(cpo code,int depth,cellpo name,cellpo type,int res,dictpo dict,dictpo outer);
void genEntryDebug(cpo code,int depth,cellpo fn);
void genExitDebug(cpo code,int depth,cellpo fn);
void genVarDebug(cpo code,int depth,cellpo vr,cellpo type,int res,dictpo dict,dictpo outer);
void genScopeDebug(cpo code,int depth,dictpo dict);
void genSendDebug(cpo code,int depth,int to,int msg,cellpo type,dictpo dict,dictpo outer);
void genAcceptDebug(cpo code,int depth,cellpo input);
void genForkDebug(cpo code,int depth,int var);
void genDieDebug(cpo code,int depth,cellpo input);
void genArgDebug(dictpo dict,int depth,cpo code,cellpo args);
void genSuspendDebug(int dLvl,cpo code,int depth);
void genPtnVarDebug(cpo code,int depth,dictpo top,dictpo dict,dictpo outer);
void genErrorDebug(dictpo dict,int depth,cpo code);

#include "fold.h"
#define HANDLE_ARITY 3		/* hdl(tgt,name,pid) */
#endif

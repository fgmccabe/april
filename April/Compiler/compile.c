/* 
   Top-level of April compiler
   (c) 1994-2000 Imperial College and F.G. McCabe

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
#include "config.h"		/* Access configuration details */
#include <stdlib.h>
#include <string.h>

#include "compile.h"		/* access compiler functions */
#include "tree.h"		/* Tree parsing and dumping */
#include "types.h"
#include "keywords.h"
#include "operators.h"		/* operator definition etc. */
#include "grammar.h"
#include "token.h"
#include "macro.h"		/* access macro functions */
#include "generate.h"
#include "assem.h"
#include "ooio.h"

logical traceCompile = False;	/* tracing preprocessor */

extern long comperrflag;	/* Has there been a syntax error? */

dictpo global = NULL;		/* Global dictionary */

/* 
 * Initialize the preprocessor section of the compiler 
 */

void initCompiler(void)
{
  static logical inited = False;
  
  if(!inited){
    initHeap();			/* Initialize the heaps */
    init_dict();		/* Start up the dictionaries */
    init_op();			/* initialize the operators */

    initDisplay();		/* Extend outMsg with our display function */

    init_compiler_dict();	/* initialize compiler-level dictionary */

    init_macro();		/* Initialize the macro-processor */
    init_escapes();		/* We must build the search tables */
    initChecker();		/* Initialise type checker */
    install_escapes();		/* install the escape table */
    init_instr();		/* start up the opcode tables */
    inited=True;
  }
}

static cellpo resetCompiler(logical noWarnings)
{
  extern logical suppressWarnings;

  comperrflag = compwarnflag = 0;

  standardTypes=resetChecker();	/* reset the type checker */

  init_op();			/* Re-initialize the operator table */
  initLineTable();		/* clear the line numbers table */
  global = NULL;
  suppressWarnings = noWarnings;
  resetRstack();
  return topStack();
}

static void ProcessHeader(ioPo outFile,optPo info)
{
  ioPo hFile=NULL;
  
  if((hFile=openURL(info->sysinclude,info->header,unknownEncoding))!=NULL){
    OptInfoRec subInfo = *info;   // nested information block
    subInfo.include = fileName(hFile);
	  
    /* call processor on include file */
    Process(hFile,outFile,&subInfo);
    closeFile(hFile);
  }
  else{
    outMsg(logFile,"Cant find standard header file %s\n",info->header);
    syserr("compiler aborted");
  }
}


logical compile(ioPo inFile,ioPo outFile,optPo info)
{
  long orig;

  resetCompiler(info->noWarnings); /* Initialize the heap structures */

  orig = stackDepth();

  /* process header file */
  ProcessHeader(outFile,info);
  /* the source file */
  Process(inFile,outFile,info);

  reverseStack(orig);

  while(stackDepth()>orig){
    cellpo element = topStack();
    cellpo lhs,rhs;

    if(info->preProcOnly)
      outMsg(outFile,"%w;\n",element);
    else if(isBinaryCall(element,ktype,&lhs,&rhs))
      standardTypes = defType(element,standardTypes);
    else if(!comperrflag){
      cellpo type=allocSingle();
      envPo tt = standardTypes;

      generalize(whichType(element,&tt,standardTypes,type,True),
		 standardTypes,type);

      if(!comperrflag)		/* only compile if no previous errors */
	compileProg(element,global,outFile,info);
    }
    p_drop();
  }
  
  mac_top = clearMacros(mac_top);	/* clear down the macro tables */

  clearDict(global,NULL);		/* clear down the dictionary */

  global = NULL;

  resetStack(orig);
  
  if(comperrflag>=MAXERRORS)
    reportWarning(-1,"error reporting truncated, more errors may follow");
  
  if(comperrflag==0)
    return True;
  else
    return False;
}

/*
 * Main procedure which reads in source files and invokes macro processor 
 */

void Process(ioPo inFile,ioPo outFile,optPo info)
{
  cellpo orig = topStack();	/* just in case we have a problem... */
  long line = 1;

  if(info->verbose){
    outMsg(logFile,"Reading from %s\n",fileName(inFile)); 
    flushFile(logFile);
  }

  while(comperrflag<MAXERRORS && parse(inFile,2000,True,&line)!=uniEOF){
    if(synerrors==0){
      cellpo lhs,rhs,body;
      cellpo opArg,formArg,priorArg;
      cellpo element = topStack();
      
#ifdef TRACEPARSE
      if(traceParse){
        outMsg(logFile,"Term in file is %w\n",element);
        flushFile(logFile);
      }
#endif
        
      if(IsHashStruct(element,kinclude,&body)){
	cellpo file;
	uniChar fn[1024];
	ioPo incFile=NULL;		/* Include file no. */
	uniChar *includePath = info->include;
      
	if((isUnaryCall(body,kchoice,&file) &&
	    isUnaryCall(file,kchoice,&file)) ||
	   (isUnaryCall(body,kgreaterthan,&file) &&
	    isUnaryCall(file,klessthan,&file))){
	  rmDots("sys:",file,fn,NumberOf(fn));
	  includePath = info->sysinclude;
	}
	else if(isSymb(body))
	  uniCpy(fn,NumberOf(fn),symVal(body));
	else if(IsString(body))
	  uniCpy(fn,NumberOf(fn),strVal(body));
	else{
	  reportErr(lineInfo(element),"cant understand include statement: `%5w'",element);
	  continue;
	}
	
	if((incFile=findURL(includePath,fileName(inFile),fn,unknownEncoding))==NULL){
	  reportErr(lineInfo(body),"include file not found: `%U'",fn);
	  continue;
	}

	p_drop();		/* remove the include command */
	
	{
	  OptInfoRec subInfo = *info;   // nested information block
	  
	  subInfo.include = fileName(incFile);
	  
	  /* call processor on include file */
	  Process(incFile,outFile,&subInfo);
	}
	closeFile(incFile);
	continue;
      }

      /* operator declaration */
      else if(isUnaryCall(element,khash,&body) &&
	      IsTernaryStruct(body,kop,&opArg,&formArg,&priorArg)){
	if(IsInt(priorArg)||IsFloat(priorArg)){
	  int prior = IsInt(priorArg)?intVal(priorArg):(int)fltVal(priorArg);
	  uniChar str[MAXSTR];
	  uniChar op[MAXSTR];

	  if(IsString(formArg))
	    uniCpy(str,NumberOf(str),strVal(formArg));
	  else if(isSymb(formArg))
	    uniCpy(str,NumberOf(str),symVal(formArg));
	  else
	    uniIsLit(str,"void");
	    
	  if(IsString(opArg))
	    uniCpy(op,NumberOf(str),strVal(formArg));
	  else if(IsQuote(opArg,&opArg) && isSymb(opArg)){
	    uniChar *s = symVal(opArg);
	    
	    if(*s=='\'')
	      uniCpy(op,NumberOf(op),s+1);
	    else        
	      uniCpy(op,NumberOf(op),s);
	  }

	  if(!new_op(op,str,prior,lineInfo(opArg)))
	    reportErr(lineInfo(element),"cant understand operator declaration: %w\n"
		      "should use form #op({op},left,999)",element);
	}
	else
	  reportErr(lineInfo(element),"cant understand operator declaration: %w\n"
		    "should use form #op({op},\"(*)f*\",999)",element);
	p_drop();
	continue;
      }
      
      else if(IsHashStruct(element,kecho,&body)){ /* compile time line echo */
	Display(logFile,body);
	p_drop();
	continue;
      }

      else if(IsHashStruct(element,kmacro,&body)){
	if(isBinaryCall(body,kmacreplace,&lhs,&rhs))
	  mac_top=define_macro(lhs,rhs,mac_top);	/* define the macro */
	else
	  reportErr(lineInfo(element),"should use `%U' for defining macros: `%5w'",
		    kmacreplace,element);
	p_drop();
	continue;
      }

      /* Apply macros */
      if(info->verbose){
	outMsg(logFile,"Macro processing...\n"); 
	flushFile(logFile);
      }

      element = macro_replace(element,info);
#ifdef TRACEPARSE
      if(traceParse){
        outMsg(logFile,"Actual term in file is %w\n",element);
        flushFile(logFile);
      }
#endif
        
    }
  }
  if(comperrflag || synerrors)
    reset_stack(orig);
}


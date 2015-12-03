/* 
  Dissassambly of April code
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
#include "april.h"
#include "opcodes.h"
#include "dict.h"
#include "symbols.h"
#include "process.h"
#include "msg.h"
#include "astring.h"		/* String handling interface */
#include "debug.h"
#include "sign.h"

#ifdef EXECTRACE

#define relpc(PC) ((*(PC)&0xffff)<<16>>16)

static void p_s(objPo *fp,WORD16 offset,char *x)
{
  outMsg(logFile,"fp[%d]",offset);
  if(fp!=NULL)
    outMsg(logFile,"=%#.3w",fp[offset]);
  outMsg(logFile,"%s",x);
}

static void p_t(objPo *fp,WORD16 offset,char *x)
{
  outMsg(logFile,"fp[%d]",offset);
  if(fp!=NULL)
    outMsg(logFile,"=%.3t",fp[offset]);
  outMsg(logFile,"%s",x);
}

static void p_e(objPo *e,WORD16 offset,char *x)
{
  outMsg(logFile,"e[%d]",offset);
  if(e!=NULL)
    outMsg(logFile,"=%#.3w",e[offset]);
  outMsg(logFile,"%s",x);
}

static void p_d(WORD16 offset,char *x)
{
  outMsg(logFile,"%d%s",offset,x);
}

static void p_a(insPo pc,WORD16 offset,char *x)
{
  outMsg(logFile,"pc%+d%s",offset,x);
}

static void p_lit(objPo x,char *sep)
{
  outMsg(logFile,"%#.3w%s",x,sep);
}

static void p_type_lit(objPo x,char *sep)
{
  outMsg(logFile,"%.3t%s",x,sep);
}

/* disassemble instruction at pc */
insPo dissass(insPo pc,insPo base,objPo *fp, objPo *e,objPo *lits)
{
  register WORD32 pcx = *pc;

  outMsg(logFile,"PC+0x%x: ",(unsigned WORD32)(pc-base));
  switch(pcx&op_mask){

  case halt:			/* stop execution */
    outMsg(logFile,"halt");
    return pc+1;
	
/* Variable move instructions */
  case movl:			/* Move a literal value into variable */
    outMsg(logFile,"movl ");
    p_lit(lits[op_o_val(pcx)],",");
    p_s(NULL,op_sh_val(pcx),"");
    return pc+1;
	
  case move:			/* Move variable */
    outMsg(logFile,"move ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case emove:			/* Move variable from environment */
    outMsg(logFile,"emove ");
    p_e(e,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case stoe:			/* Store environment in local */
    outMsg(logFile,"stoe ");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case loade:			/* Load environment from local */
    outMsg(logFile,"loade ");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case jmp:			/* jump */
    outMsg(logFile,"jmp ");
    p_a(pc+1,op_lo_val(pcx),"");
    return pc+1;

  case ijmp:			/* indirect jump */
    outMsg(logFile,"ijmp ");
    p_d(op_so_val(pcx),",");
    p_s(fp,op_sh_val(pcx),"");
    return pc+1;

  case hjmp:			/* hash-table jump */
    outMsg(logFile,"hjmp ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_so_val(pcx),"");
    return pc+1;

  case tjmp:			/* tag table jump */
    outMsg(logFile,"tjmp ");
    p_s(fp,op_sh_val(pcx),"");
    return pc+1;

  case esc_fun:			/* escape into service functions */
    outMsg(logFile,"escape ");
    p_s(NULL,op_sh_val(pcx),",");
    p_d(op_o_val(pcx),"=");
    outMsg(logFile,"%s",escapeName(op_o_val(pcx)));
    return pc+1;
	
  case call:			/* call procedure*/
    outMsg(logFile,"call ");
    p_d(op_sh_val(pcx),",");
    p_d(op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case ecall:{			/* call procedure */
    outMsg(logFile,"ecall ");
    p_d(op_sh_val(pcx),",");
    p_d(op_sm_val(pcx),",");
    p_e(e,op_sl_val(pcx),"");
    return pc+1;
  }

  case ret:			/* Return from procedure */
    outMsg(logFile,"ret");
    return pc+1;

  case result:			/* return a result from a function */
    outMsg(logFile,"result ");
    p_d(op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case allocv:{			/* Allocate a block of variables */
    outMsg(logFile,"allocv ");

    p_d(op_so_val(pcx),"");
    return pc+1;
  }

  case initv:			/* initialize a variable */
    outMsg(logFile,"initv ");
    p_d(op_sl_val(pcx),"");
    return pc+1;

    /* pattern patching instructions */
  case mlit:			/* Match a literal value */
    outMsg(logFile,"mlit ");
    p_s(fp,op_sh_val(pcx),",");
    p_lit(lits[op_o_val(pcx)],"");
    return pc+1;

  case mfloat:			/* Match a floating-point */
    outMsg(logFile,"mfloat ");
    p_s(fp,op_sh_val(pcx),",");
    p_lit(lits[op_o_val(pcx)],"");
    return pc+1;

  case mnil:			/* Match an empty list */
    outMsg(logFile,"mnil ");
    p_s(fp,op_sh_val(pcx),",");
    p_a(pc+1,op_so_val(pcx),"");
    return pc+1;
    
   case mchar:			/* Match a literal character */
    outMsg(logFile,"mchar ");
    p_s(fp,op_sh_val(pcx),",");
    outMsg(logFile,"''%c",op_o_val(pcx));
    return pc+1;

  case mlist:			/* Match a non-empty list */
    outMsg(logFile,"mlist ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(NULL,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case mcons:			/* Match a constructor term */
    outMsg(logFile,"mcons ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_so_val(pcx),"");
    return pc+1;

  case mtpl:			/* Match an n-tuple */
    outMsg(logFile,"mtpl ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_so_val(pcx),"");
    return pc+1;

    /* Type signature matching & handling */
  case many:			/* Match against specific any value  */
    outMsg(logFile,"many ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_sm_val(pcx),",");
    p_d(op_sl_val(pcx),",");
    return pc+1;
    
  case isvr:                    // Test for variable
    outMsg(logFile,"isvr ");
    p_t(fp,op_sh_val(pcx),",");
    p_a(pc+1,op_so_val(pcx),"");
    return pc+1;

  case ivar:                    // Create a new variable
    outMsg(logFile,"ivar ");
    p_d(op_sh_val(pcx),",");
    p_d(op_sl_val(pcx),"");
    return pc+1;
    
  case uvar:                    // Unify against a variable
    outMsg(logFile,"uvar ");
    p_t(fp,op_sm_val(pcx),",");
    p_t(fp,op_sl_val(pcx),"");
    return pc+1;
    
  case ulit:                    // Unify against a literal
    outMsg(logFile,"ulit ");
    p_t(fp,op_sh_val(pcx),",");
    p_type_lit(lits[op_o_val(pcx)],"");
    return pc+1;

  case utpl:                    // Unify against a tuple
    outMsg(logFile,"utpl ");
    p_t(fp,op_sh_val(pcx),",");
    p_d(op_so_val(pcx),"");
    return pc+1;
        
  case ucns:                    // Unify against a constructor
    outMsg(logFile,"ucns ");
    p_t(fp,op_sh_val(pcx),",");
    p_d(op_so_val(pcx),"");
    return pc+1;
  
  case urst:                    // Reset unification tables
    outMsg(logFile,"urst ");
    p_d(op_sh_val(pcx),"");
    return pc+1;
    
  case undo:                    // Undo unification
    outMsg(logFile,"undo ");
    p_d(op_sh_val(pcx),"");
    return pc+1;
    
    /* list matching instructions */

  case prefix:			/* Check a list prefix */
    outMsg(logFile,"prefix ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case snip:			/* Chop off a list prefix */
    outMsg(logFile,"snip ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

    /* Error block instructions */
  case errblk:			/* start an error block */
    outMsg(logFile,"errblk ");
    p_d(op_sh_val(pcx),",");
    p_a(pc+1,op_so_val(pcx),"");
    return pc+1;

  case errend:			/* end an error block */
    outMsg(logFile,"errend ");
    p_d(op_sl_val(pcx),"");
    return pc+1;

  case generr:			/* gnerate an error */
    outMsg(logFile,"generr ");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case moverr:			/* load the error value into a register */
    outMsg(logFile,"moverr ");
    p_d(op_sl_val(pcx),"");
    return pc+1;
    
  case loc2cns:			/* create a constructor from the locals */
    outMsg(logFile,"loc2cns ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;
    
  case mcnsfun:                 // Match the function symbol
    outMsg(logFile,"mcnsfun ");
    p_s(fp,op_sh_val(pcx),",");
    p_lit(lits[op_o_val(pcx)],"");
    return pc+1;

  case consfld:			/* index field from constructor */
    outMsg(logFile,"consfld ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case conscns:			/* function symbol from constructor */
    outMsg(logFile,"conscns ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case cnupdte:			/* udpate a constructor */
    outMsg(logFile,"cnupdte ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case loc2tpl:			/* create a tuple from the locals */
    outMsg(logFile,"loc2tpl ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case indxfld:			/* index field from record */
    outMsg(logFile,"indxfld ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case tpupdte:			/* udpate a tuple */
    outMsg(logFile,"tpupdte ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

    /* List handling instructions */
  case lstpr:			/* Construct a list pair */
    outMsg(logFile,"lstpr ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case ulst:			/* Unpack a listConstruct a list pair */
    outMsg(logFile,"ulst ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(NULL,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case nthel:			/* Extract nth element of list */
    outMsg(logFile,"nthel ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case add2lst:			/* Add a new element to end of list */
    outMsg(logFile,"add2lst ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case lsupdte:			/* Add a new element to end of list */
    outMsg(logFile,"lsupdte ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case loc2any:			/* Build a new any value */
    outMsg(logFile,"loc2any ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

/* Arithmetic expression instructions */
  case plus:			/* addition */
    outMsg(logFile,"plus ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case minus:			/* subtraction */
    outMsg(logFile,"minus ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case times:			/* multiply */
    outMsg(logFile,"times ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case divide:			/* divide */
    outMsg(logFile,"divide ");
    p_s(fp,op_sh_val(pcx),",");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case incr:			/* increment variable */
    outMsg(logFile,"incr ");
    p_s(fp,op_sh_val(pcx),",");
    p_d(op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

/* Predicate testing instructions */

  case eq:			/* equality */
    outMsg(logFile,"eq ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case neq:			/* inequality */
    outMsg(logFile,"neq ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case le:			/* less than or equal */
    outMsg(logFile,"le ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case gt:			/* greater than */
    outMsg(logFile,"gt ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case eqq:			/* pointer equality */
    outMsg(logFile,"eqq ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case neqq:			/* pointer inequality */
    outMsg(logFile,"neqq ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case feq:			/* Numeric equality */
    outMsg(logFile,"feq ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case fneq:			/* Numeric inequality */
    outMsg(logFile,"fneq ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case fle:			/* Numeric less than or equal */
    outMsg(logFile,"fle ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case fgt:			/* Numeric greater than */
    outMsg(logFile,"fgt ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case ieq:			/* integer equality */
    outMsg(logFile,"ieq ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case ineq:			/* integer inequality */
    outMsg(logFile,"ineq ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case ile:			/* integer less than or equal */
    outMsg(logFile,"ile ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case igt:			/* integer greater than */
    outMsg(logFile,"igt ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;

  case iftrue:			/* test for var being true */
    outMsg(logFile,"iftrue ");
    p_s(fp,op_sh_val(pcx),",");
    p_a(pc+1,op_so_val(pcx),""); /* label */
    return pc+1;

  case iffalse:			/* test for var not being true */
    outMsg(logFile,"iffalse ");
    p_s(fp,op_sh_val(pcx),",");
    p_a(pc+1,op_so_val(pcx),""); /* label */
    return pc+1;

  /* special instructions */
  case snd:
    outMsg(logFile,"send ");
    p_d(op_sh_val(pcx),",");
    p_s(fp,op_sm_val(pcx),",");
    p_s(fp,op_sl_val(pcx),"");
    return pc+1;
    
  case self:
    outMsg(logFile,"self ");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case die:			/* kill current process */
    outMsg(logFile,"die");
    return pc+1;

  case use:			/* change click counter */
    outMsg(logFile,"use ");
    p_s(fp,op_sm_val(pcx),",");
    p_s(NULL,op_sl_val(pcx),"");
    return pc+1;

  case gc:			/* invoke garbage collector */
    outMsg(logFile,"gc ");
    p_s(NULL,op_sh_val(pcx),","); /* depth marker */
    p_d(op_so_val(pcx),"");	/* required quantity */
    return pc+1;

    /* debugging support instructions */

  case line_d:			/* report debugging point */
    outMsg(logFile,"line_d ");
    p_d(op_sh_val(pcx),",");	/* depth */
    p_lit(lits[relpc(pc+1)],","); /* file name */
    p_d(op_so_val(pcx),"");	/* line number */
    return pc+2;

  case entry_d:			/* report entry point */
    outMsg(logFile,"entry_d ");
    p_lit(lits[op_so_val(pcx)],","); /* procedure name */
    p_d(op_sh_val(pcx),""); /* depthr */
    return pc+1;

  case exit_d:			/* report procedure exit */
    outMsg(logFile,"exit_d ");
    p_lit(lits[op_so_val(pcx)],","); /* procedure name */
    p_d(op_sh_val(pcx),""); /* depthr */
    return pc+1;

  case assign_d:		/* report variable assignment */
    outMsg(logFile,"assign_d ");
    p_d(op_sh_val(pcx),","); /* depth */
    p_s(fp,op_sm_val(pcx),","); /* print variable name */
    p_s(fp,op_sm_val(pcx),""); /* print assigned value */
    return pc+1;

  case return_d:		/* report function value */
    outMsg(logFile,"return_d ");
    p_d(op_sh_val(pcx),","); /* depth */
    p_s(fp,op_sm_val(pcx),","); /* print function name */
    p_s(fp,op_sm_val(pcx),""); /* print return value */
    return pc+1;

  case accept_d:		/* report on a message receive */
    outMsg(logFile,"accept_d ");
    p_d(op_sh_val(pcx),","); /* depth */
    p_s(fp,op_sm_val(pcx),","); /* Replyto & sender */
    p_s(fp,op_sl_val(pcx),","); /* Message */
    return pc+1;

  case send_d:		/* report on a message send */
    outMsg(logFile,"send_d ");
    p_s(fp,op_sh_val(pcx),","); /* Message */
    p_s(fp,op_sm_val(pcx),","); /* Replyto & sender */
    p_s(fp,op_sl_val(pcx),","); /* To */
    return pc+1;

  case scope_d:			/* report on variable scope */
    outMsg(logFile,"scope_d ");
    p_d(op_sm_val(pcx),",");	/* which scope */
    p_d(op_sl_val(pcx),"");	/* depth */
    return pc+1;

  case suspend_d:			/* report on process suspension */
    outMsg(logFile,"suspend_d ");
    p_d(op_sl_val(pcx),"");	/* depth */
    return pc+1;

  case error_d:			/* report on error */
    outMsg(logFile,"error_d ");
    p_d(op_sm_val(pcx),",");	/* depth */
    p_s(fp,op_sl_val(pcx),"");	/* error message */
    return pc+1;

  case debug_d:			/* are we debugging? */
    outMsg(logFile,"debug_d ");
    p_a(pc+1,op_so_val(pcx),"");
    return pc+1;

  default:			/* anything else? */
    outMsg(logFile,"unknown[%lx]",pcx);
    return pc+1;
  }
}

void dc(objPo trm)
{
  if(trm!=NULL)
    outMsg(logFile,"%.10w\n",trm);
  else
    outMsg(logFile,"NULL");
  flushFile(logFile);
}

void da(objPo trm)
{
  if(trm!=NULL)
    outMsg(logFile,"%#.10w\n",trm);
  else
    outMsg(logFile,"NULL");
  flushFile(logFile);
}

void dt(objPo trm)
{
  if(trm!=NULL)
    outMsg(logFile,"%t\n",trm);
  else
    outMsg(logFile,"NULL");
  flushFile(logFile);
}

void showPC(processpo P)
{
  if(P==NULL)
    P = current_process;

  if(P!=NULL){
    insPo pc = P->pc;
    objPo *e = tupleData(P->e)-1;
    WORD16 i;

    for(i=0;i<8;i++){
      pc=dissass(pc,CodeCode(e[2]),P->fp,e,CodeLits(e[2]));
      outMsg(logFile,"\n");
    }
    flushFile(logFile);
  }
}

#endif

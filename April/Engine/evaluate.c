/* 
  The run-time emulator for April programs
  (c) 1994-1998 Imperial College and F.G. McCabe

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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "april.h"		/* main header file */
#include "arith.h"		/* Arithmetic stuff */
#include "opcodes.h"		/* The definitions of the opcodes */
#include "dict.h"		/* Dictionary handling stuff */
#include "process.h"		/* Process handling */
#include "msg.h"		/* Functions which implement message passing */
#include "symbols.h"
#include "clock.h"
#include "astring.h"		/* string handling interface */
#include "sign.h"
#include "sort.h"		/* sorting */
#include "setops.h"		/* value comparison */
#include "term.h"
#include "hash.h"		/* we need access to the hash functions */
#include "debug.h"		/* Debugger access functions */
#include "types.h"

extern logical debugging;	/* Level of debugging */
extern logical SymbolDebug;	/* Symbolic debugging switched on? */

#ifdef EXECTRACE
logical traceEscapes=False;	/* Trace escape calls */
WORD32 pcCount = 0;
#endif

#ifdef PROCTRACE
#define tickle(SP) {\
  if(stressSuspend||wakeywakey){\
      save_regs(SP,PC);\
      P=ps_pause(P);\
      restore_regs();\
    }\
  }
#else
#define tickle(SP) {\
  if(wakeywakey){\
      save_regs(SP,PC);\
      P=ps_pause(P);\
      restore_regs();\
    }\
  }
#endif

#define save_regs(SP,PC) {P->sp=(SP);P->e=env;P->fp=FP;P->pc=PC;}
#define restore_regs()\
{\
  SP=P->sp;\
  env=P->e;\
  E=codeFreeVector(env);\
  Lits = CodeLits(consFn(env));\
  FP=P->fp;\
  PC=P->pc;\
}

#define min3(u,v,w) min(u,min(v,w))
#define min(u,v) (u<v?u:v)

#define RunErr(msg,code){\
  make_error_message(msg,code,P);\
  goto error_recover;\
}      

static uniChar *file_tail(uniChar *n,WORD32 len);
static objPo snipList(objPo list,int count);

/* Main execution program */
void emulate(register processpo P)
{
  register insPo PC = P->pc;
  register WORD32 PCX = 0;	/* 'Current' instruction register  */
  register objPo *FP = P->fp;	/* FramePointer */
  objPo *SP = P->sp;		/* Copy out important registers StackTop */
  objPo env = P->e;
  objPo *E=codeFreeVector(env);	/* Environment pointer */
  objPo *Lits= CodeLits(consFn(env)); /* literals pointer */

  for(;;){			/* Loop forever, until execution terminates */
#ifdef EXECTRACE
    pcCount++;
    if(debugging)
      debug_stop(pcCount,P,PC,SP,FP,env,Lits,P->handle);
#endif

    PCX=*PC++;

    switch(op_cde(PCX)){
    case halt:			/* Stop execution */
      if(!P->priveleged){
	RunErr("halt is privileged",eprivileged);
      }
      else{
	save_regs(SP,PC);	/* Copy back important registers */
	return;
      }

      /* Move instructions */
    case movl:			/* Put a literal value into a variable */
      FP[op_sh_val(PCX)]=Lits[op_o_val(PCX)]; /* Copy cell @ PC */
      continue;
    
    case move:			/* Local - local move */
      FP[op_sl_val(PCX)]=FP[op_sm_val(PCX)];
      continue;

    case emove:			/* move from environment */
      FP[op_sl_val(PCX)]=E[op_sm_val(PCX)];
      continue;

    case stoe:			/* store environment in local */
      FP[op_sl_val(PCX)]=env;
      continue;

    case loade:			/* restore environment from local */
      env = FP[op_sl_val(PCX)];
      E = tupleData(env)-1;
      continue;

    case jmp:			/* Jump to new location */
      PC+=op_lo_val(PCX);	/* Relative jump */
      continue;

    case ijmp:{			/* indirect jump ... usually to another jump */
      integer ix = IntVal(FP[op_sh_val(PCX)]);
      if(ix<0 || ix>=op_o_val(PCX))
	PC++;			/* error case */
      else
	PC+=ix+1;		/* jump to the kth jump instruction */
      continue;
    }

    case cjmp:{			/* character hash-table jump ... */
      int max = op_o_val(PCX);
      objPo val = FP[op_sh_val(PCX)];

      if(isChr(val)){
	int ix = CharVal(val)%max;

	if(ix<0 || ix>=max)
	  PC++;			/* error case */
	else
	  PC+=ix+1;		/* jump to the kth jump instruction */
      }
      else
	PC++;			/* not a symbol */
      continue;
    }

    case hjmp:{			/* hash-table jump ... */
      int max = op_o_val(PCX);
      objPo val = FP[op_sh_val(PCX)];

      if(isSymb(val)){
	int ix = uniHash(SymVal(val))%max;

	if(ix<0 || ix>=max)
	  PC++;			/* error case */
	else
	  PC+=ix+1;		/* jump to the kth jump instruction */
      }
      else
	PC++;			/* not a symbol */
      continue;
    }

    case tjmp:{			/* tag table jump ... */
      int max = op_o_val(PCX);
      objPo val = FP[op_sh_val(PCX)];
      WORD32 ix = Tag(val);

      if(ix<0 || ix>=max)
	PC++;			/* error case */
      else
	PC+=ix+1;		/* jump to the kth jump instruction */
      continue;
    }

    case esc_fun:{		/* escape into 1st level builtins */
      register retCode ret;
      funpo ef = escapeCode(op_o_val(PCX));

#ifdef EXECTRACE
      if(traceEscapes)
	showEscape(P,op_o_val(PCX),&FP[op_sh_val(PCX)]);
#endif

      SP = FP+op_sh_val(PCX);	/* establish SP in case of G/C */
      save_regs(SP,PC);

#ifdef EXECTRACE
      if(ef==NULL){	/* Something seriously wrong here */
	make_error_message("invalid escape function",esystem,P);
	restore_regs();
	goto error_recover;
      }
#endif

      ret = (*ef)(P,SP);

      restore_regs();		/* restore registers in case of g/c */

      switch(ret){
      case Ok:
	tickle(SP);		/* tickle the scheduler */
	continue;

      case Suspend:
        P = current_process;
        restore_regs();
        continue;

      case Switch:{		/* Switch to another process */
        save_regs(SP,PC);
	P=ps_pause(P);
	restore_regs();
	continue;
      }

      case Space:{		/* Ran out of space */
	RunErr("out of heap Space",esystem);
      }

      case Error:		/* Report a run-time error */
	goto error_recover;

      default:
	logMsg(logFile,"Invalid return code `%d' from escape function `%d'",
	       ret,op_so_val(PCX));
	syserr("Problem in escape function return code");
      }
      
      tickle(SP);		/* tickle the scheduler */
      continue;
    }

    case call:{			/* Call a local procedure */
      register objPo pr = FP[op_sl_val(PCX)]; /* pick up procedure from locals */
      register objPo *nEnv = codeFreeVector(pr);

      if(isClosure(pr)){
	register objPo cp = consFn(pr);

	SP = FP+op_sh_val(PCX); /* reset the stack top */

	*--SP=(objPo)PC;	/* set up for a normal procedure call */
	*--SP=env;		/* save the environment pointer */
	env = pr;		/* set up the environment register */
	E = nEnv;
	PC = CodeCode(cp);	/* new program counter */
	Lits = CodeLits(cp);
	*--SP=(objPo)FP;	/* Store existing frame pointer */
	FP=SP;			/* Redefine frame pointer */
      }
      else
	RunErr("illegal procedure code",eexec);

      /* Not safe to reschedule at this point */

      continue;
    }

    case ecall:{		/* Call a procedure from env */
      register objPo pr = E[op_sl_val(PCX)]; /* pick up procedure from env */
      register objPo *nEnv = codeFreeVector(pr);

      if(isClosure(pr)){
	register objPo cp = consFn(pr);

	SP = FP+op_sh_val(PCX); /* reset the stack top */

	*--SP=(objPo)PC;	/* set up for a normal procedure call */
	*--SP=env;		/* save the environment pointer */
	env = pr;		/* set up the environment register */
	E = nEnv;
	PC = CodeCode(cp);	/* new program counter */
	Lits = CodeLits(cp);	/* new literals pointer */

	*--SP=(objPo)FP;	/* Store existing frame pointer */
	FP=SP;			/* Redefine frame pointer */
      }
      else
	RunErr("illegal procedure code",eexec);

      continue;
    }

    case allocv:{		/* Establish a new frame pointer */
      register WORD32 amnt = op_so_val(PCX);
      if(SP-4+amnt<=P->stack){	/* allow for an extra call */
	save_regs(SP,PC);
	if(grow_stack(P,2)==Space) /* Double this stack */
	  RunErr("cant extend process stack",esystem);
	restore_regs();
      }

      tickle(SP);
      continue;
    }

    case initv:			/* Initialize a variable */
      FP[op_sl_val(PCX)]=kvoid;
      continue;

    case ret:{			/* Return from a procedure */
      SP = FP;			/* Remove existing stack */
      FP = (objPo*)(*SP++);	/* Restore old frame pointer*/
      env = *SP++;
      E = codeFreeVector(env);	/* Restore environment pointer */
      PC = (insPo)(*SP++);	/* restore program counter */
      Lits = CodeLits(consFn(env));	/* restore the literals vector */

      tickle(SP);
      continue;
    }
    
    case result:{		/* Return from a function */
      objPo res = FP[op_sl_val(PCX)]; /* collect value of function */

      SP = FP;			/* Remove existing stack */
      FP = (objPo*)(*SP++);	/* Restore old frame pointer*/
      env = *SP++;
      E = codeFreeVector(env);	/* Restore environment pointer */

      PC= (insPo)(*SP);		/* extract return address */
      Lits = CodeLits(consFn(env));	/* restore the literals vector */

      SP=SP+op_sm_val(PCX);	/* reset the stack pointer */
      *SP = res;		/* store result of function */

      tickle(SP);
      continue;
    }
    
    case die:{			/* kill current sub-process */
      save_regs(FP,PC);

      P = ps_terminate(P);	/* Kill current proces */
      if(P==NULL)		/* Last process? */
	return;			/* Quit altogether */

      restore_regs();		/* Copy out the relevant registers */
      continue;
    }

    /* Individual match instructions */
    case mlit:			/* match against a literal */
      switch(equalcell(FP[op_sh_val(PCX)],Lits[op_o_val(PCX)])){
      case Ok:
	PC++;
	continue;
      case Error:
	RunErr("incomparable value",ecompval);
      default:
      continue;
      }
    
    case mfloat:{		/* match against a floating-point */
      objPo T = FP[op_sh_val(PCX)];
      Number N = FloatVal(Lits[op_o_val(PCX)]);
      if((IsFloat(T) && FloatVal(T)==N) ||
	 (IsInteger(T) && (Number)IntVal(T)==N))
	PC++;
      continue;
    }

    case mnil:			/* Match empty list */
      if(!isEmptyList(FP[op_sh_val(PCX)]))
	PC+=op_so_val(PCX);	/* Relative jump */
      continue;
      
    case mchar:{
      objPo x = FP[op_sh_val(PCX)];
      
      if(isChr(x) && CharVal(x)==op_o_val(PCX))
        PC++;                   // Skip the failure jump
      continue;
    }

    case mlist:{		/* Access and match head of non-empty list */
      objPo lst = FP[op_sh_val(PCX)];

      if(isNonEmptyList(lst)){
	FP[op_sm_val(PCX)]=ListHead(lst);
	FP[op_sl_val(PCX)]=ListTail(lst);
	PC++;
      }
      continue;
    }
    
    case prefix:{               // Check and step over a list prefix
      objPo lst = FP[op_sh_val(PCX)];
      objPo pre = FP[op_sm_val(PCX)];
      
      while(isNonEmptyList(pre)){
        if(isNonEmptyList(lst)){
          if(equalcell(ListHead(pre),ListHead(lst))!=Ok){
            break;
          }
          pre = ListTail(pre);
          lst = ListTail(lst);
        }
        else
          break;
      }
      
      if(isEmptyList(pre)){
        FP[op_sl_val(PCX)] = lst;
        PC++;
      }
      continue;
    }
    
    case snip:{                 // Snip off a fixed front segment off a list
      integer cnt = IntVal(FP[op_sm_val(PCX)]);
      
      if(cnt<0)
	RunErr("negative allocation",eexec);
      
      reserveSpace(ListCellCount*cnt);
      
      save_regs(FP+op_sh_val(PCX),PC)
      
      {
        objPo lst = snipList(FP[op_sh_val(PCX)],cnt);
        
        if(lst!=NULL){
          FP[op_sl_val(PCX)]=lst;
          PC++;
        }
      }
      continue;
    }
            
    case mcons:{			/* Match against constructor */
      register objPo T = FP[op_sh_val(PCX)];

      if(isCons(T) && consArity(T)==op_o_val(PCX))
	PC++;

      continue;
    }
    
    case mcnsfun:{                      // Match the constructor symbol
      register objPo T = FP[op_sh_val(PCX)];

      if(isCons(T) && equalcell(consFn(T),Lits[op_o_val(PCX)])==Ok)
        PC++;                           // Skip the failure jump
      continue;
    }

    case mtpl:{			/* Match against tuple */
      register objPo T = FP[op_sh_val(PCX)];

      if(IsTuple(T) && tupleArity(T)==op_o_val(PCX))
	PC++;

      continue;
    }

    case mhdl:{			/* Match against literal handle */
      register objPo T = FP[op_sh_val(PCX)];

      if(IsHandle(T)){
	if(sameHandle(T,Lits[op_o_val(PCX)]))
	  PC++;
      }

      continue;
    }

    case many:{			/* We are looking for an ANY value */
      register objPo T = FP[op_sh_val(PCX)];

      if(IsAny(T)){
	FP[op_sm_val(PCX)]=AnySig(T);
	FP[op_sl_val(PCX)]=AnyData(T);
	PC++;
	continue;
      }
    }
    
    case ivar:{                 // create a new type variable
      save_regs(&FP[op_sh_val(PCX)],PC);
              
      FP[op_sl_val(PCX)]  = allocateVariable();
      
      restore_regs();
      continue;
    }
    
    case uvar:{                 // Unify a subsequent occcurence
      objPo left = FP[op_sm_val(PCX)];
      objPo right = FP[op_sl_val(PCX)];
      
      switch(unifyTypes(left,right)){
        case Ok:
          PC++;
          continue;
        case Fail:
          continue;
        default:
   	  RunErr("problem in unify",einval);
   	  continue;
      }
    }

    case ulit:{                 // Unify against a literal
      switch(unifyTypes(FP[op_sh_val(PCX)],Lits[op_o_val(PCX)])){
        case Ok:
          PC++;
          continue;
        case Fail:
          continue;
        default:
   	  RunErr("problem in unify",einval);
   	  continue;
      }
    }

    case urst:{
      clearResets();
      continue;
    }
    
    case undo:{
      undoUnify();
      continue;
    }
    
    case isvr:{                 // Test for a variable
      if(!IsVar(deRefVar(FP[op_sh_val(PCX)])))
        PC+=op_so_val(PCX);
      continue;
    }
    
    case utpl:{
      objPo trm = deRefVar(FP[op_sh_val(PCX)]);
      unsigned WORD32 ar = op_o_val(PCX);
      
      if(IsVar(trm)){
        save_regs(&FP[op_sh_val(PCX)],PC);

        {
          objPo tpl = allocateTuple(ar);
          unsigned WORD32 i;
          void *root = gcAddRoot(&tpl);
          
          for(i=0;i<ar;i++){
            objPo el = allocateVariable();
            
            updateTuple(tpl,i,el);
          }
          
          bindVar(deRefVar(FP[op_sh_val(PCX)]),tpl);
          
          gcRemoveRoot(root);
          PC++;                 // skip over the failure jump
        }
      }
      else if(IsTuple(trm) && tupleArity(trm)==ar)
        PC++;
      continue;               // This is going to pick up the failure jump
    }

    case ucns:{
      objPo trm = deRefVar(FP[op_sh_val(PCX)]);
      unsigned WORD32 ar = op_o_val(PCX);
      
      if(IsVar(trm)){
        save_regs(&FP[op_sh_val(PCX)],PC);

        {
          objPo cns = allocateConstructor(ar);
          unsigned WORD32 i;
          void *root = gcAddRoot(&cns);
          
          {
            objPo fn = allocateVariable();
            
            updateConsFn(cns,fn);
          }
          
          for(i=0;i<ar;i++){
            objPo el = allocateVariable();
            
            updateConsEl(cns,i,el);
          }
          
          bindVar(deRefVar(FP[op_sh_val(PCX)]),cns);
          
          gcRemoveRoot(root);
          PC++;                 // skip over the failure jump
        }
      }
      else if(isCons(trm) && consArity(trm)==ar)
        PC++;
      continue;               // This is going to pick up the failure jump
    }

    case errblk:{		/* start a new error block */
      objPo *base = &FP[op_sh_val(PCX)];

      base[1] = (objPo)(PC+op_so_val(PCX));
      base[0] = (objPo)P->er;

      P->er = base;
      continue;
    }

    case errend:{		/* end an error block */
      objPo *er = (objPo*)P->er[0];

      assert(&FP[op_sl_val(PCX)]==P->er);
      P->er[1] = kvoid;		/* allow for G/C */
      P->er[0] = kvoid;
      P->er = er;
      continue;
    }

    case generr:{		/* generate a run-time error explicitly */
      P->errval = FP[op_sl_val(PCX)];

    error_recover:		/* Error recovery entry point */
      if(P->er<P->sb){		/* we have an error handler in place... */
	objPo *er = (objPo*)P->er[0];

	while(FP<P->er){
	  SP = FP;
	  FP = (objPo*)(*SP++);	/* unwind the stack until we get to the ER */
	  env = *SP++;
	  E = codeFreeVector(env);
	}
	PC = (insPo)(P->er[1]);	/* new PC */
	Lits = CodeLits(consFn(env));
	P->er[0] = P->er[1] = kvoid; /* make sure the stack is clean */
	P->er = er;		/* we have a new error handler base */
      }
      else{
	outMsg(logFile,"Run-time error `%.5w' in %#w\n",P->errval,P->handle);
	save_regs(FP,PC);
	if(P==rootProcess)
	  return;
	else{
	  P = ps_terminate(P);
	  if(P==NULL)
	    return;
	}
	restore_regs();		/* pick up the new register set */
      }
      continue;
    }

    case moverr:		/* load a register from the error value */
      FP[op_sl_val(PCX)] = P->errval;
      continue;
      
      
    /* Constructor management instructions */
    case loc2cns:{		/* copy a constructor from the locals */
      register int len = op_m_val(PCX);
      register objPo *t1 = FP+op_sh_val(PCX); /* where the constructor starts */
      register objPo tpl,*ptr;

      save_regs(t1,PC);

      tpl = allocateCons(len);
      ptr = consData(tpl)+len;
      
      while(len-->=0)
	*--ptr = *t1++;		/* copy elements of the constructor */
      restore_regs();
      FP[op_sl_val(PCX)]=tpl;
      continue;
    }

    case consfld:{		/* index a field from a constructor*/
      objPo t1 = FP[op_sh_val(PCX)];
      unsigned WORD32 i = op_m_val(PCX);
#ifdef EXECTRACE
      unsigned WORD32 arity = consArity(t1);

      if(!isCons(t1)){
	RunErr("tried to access non-constructor",einval);
	continue;
      }

      if(i >= arity){
	RunErr("bounds error in accessing record",einval);
	continue;
      }
#endif

      FP[op_sl_val(PCX)]=consEl(t1,i);
      continue;
    }

    case cnupdte:{		/* update a constructor element */
      objPo t1 = FP[op_sh_val(PCX)]; /* new element */
      unsigned WORD32 i = op_m_val(PCX);	/* offset to update */
      objPo t2 = FP[op_sl_val(PCX)]; /* tuple to update */
      
      if(!isCons(t2)){
	RunErr("tried to update non-constructor",einval);
	continue;
      }
      else if(i>=consArity(t2)){
	RunErr("out of range constructor element request",einval);
	continue;
      }

      updateConsEl(t2,i,t1);	/* update the constructor */
      continue;
    }

    case loc2tpl:{		/* copy a tuple from the locals */
      register int len = op_sm_val(PCX);
      register objPo *t1 = FP+op_sh_val(PCX); /* where the tuple starts */
      register objPo tpl,*ptr;

      save_regs(t1,PC);

      tpl = allocateTpl(len);
      ptr = tupleData(tpl);

      t1 += len;

      while(len--)
	*ptr++ = *--t1;		/* copy elements of the tuple */
      restore_regs();
      FP[op_sl_val(PCX)]=tpl;
      continue;
    }

    case indxfld:{		/* index a field from a record*/
      objPo t1 = FP[op_sh_val(PCX)];
      WORD32 i = op_m_val(PCX);
#ifdef EXECTRACE
      WORD32 arity = tupleArity(t1);

      if(!IsTuple(t1)){
	RunErr("tried to access non-tuple",einval);
	continue;
      }

      if(i >= arity || i<0){
	RunErr("bounds error in accessing record",einval);
	continue;
      }
#endif

      FP[op_sl_val(PCX)]=tupleArg(t1,i);
      continue;
    }

    case tpupdte:{		/* update a tuple */
      objPo t1 = FP[op_sh_val(PCX)]; /* new element */
      unsigned WORD32 i = op_m_val(PCX);	/* offset to update */
      objPo t2 = FP[op_sl_val(PCX)]; /* tuple to update */
      
      if(!IsTuple(t2) || tupleArity(t2)<=i){
	RunErr("tried to update non-tuple",einval);
	continue;
      }

      updateTuple(t2,i,t1);	/* update the tuple */
      continue;
    }

    /* List manipulation instructions */
    case lstpr:{			/* Construct a list pair */
      int hi = op_sh_val(PCX);
      int mid = op_sm_val(PCX);
      int low = op_sl_val(PCX);
      int off = min3(hi,mid,low);
	
      save_regs(FP+off,PC);

      FP[low]=allocatePair(&FP[hi],&FP[mid]);

      restore_regs();
      continue;
    }

    case ulst:{
      register objPo lst = FP[op_sh_val(PCX)];

      if(!isNonEmptyList(lst)){
	RunErr("tried to unpack empty or non-list",einval);
      }
      else{
	FP[op_sm_val(PCX)] = ListHead(lst);
	FP[op_sl_val(PCX)] = ListTail(lst);
      }
      continue;
    }

    case nthel:{		/* Extract the nth el. of a list */
      register objPo lst = FP[op_sh_val(PCX)];
      register objPo el = FP[op_sm_val(PCX)];
      register integer off =  (IsInteger(el)?IntVal(el):IsFloat(el)?FloatVal(el):-1);

      while(--off>0 && isNonEmptyList(lst))
	lst=ListTail(lst);

      if(isNonEmptyList(lst)){
	FP[op_sl_val(PCX)] = ListHead(lst);
      }
      else
	RunErr("invalid index",einval);
      continue;
    }

    case add2lst:{		/* Add an element to end of a list */
      register objPo pair;

      save_regs(FP+op_sh_val(PCX),PC);

      pair = allocatePair(&FP[op_sm_val(PCX)],&emptyList); /* Grab list pair */

      restore_regs();

      if(FP[op_sl_val(PCX)]==emptyList)
	FP[op_sl_val(PCX)]=pair;
      else{
	register objPo lst = FP[op_sl_val(PCX)];

	while(ListTail(lst)!=emptyList)
	  lst = ListTail(lst);
	updateListTail(lst,pair);
      }

      continue;
    }

    case lsupdte:{		/* Modify list with new element */
      register objPo pair;

      save_regs(FP+op_sh_val(PCX),PC);

      pair = allocatePair(&FP[op_sm_val(PCX)],&emptyList); /* Grab list pair */

      restore_regs();

      if(FP[op_sl_val(PCX)]==emptyList)
	FP[op_sl_val(PCX)]=pair;        // should never happen
      else{
	register objPo lst = FP[op_sl_val(PCX)];
	
	updateListTail(lst,pair);       // Plant new tail
	FP[op_sl_val(PCX)] = pair;      // New list tail pointer
      }

      continue;
    }

    case loc2any:{		/* construct an any value */
      register objPo any;

      save_regs(FP+op_sh_val(PCX),PC);
      any = allocateAny(&FP[op_sh_val(PCX)],&FP[op_sm_val(PCX)]);
      
      restore_regs();

      FP[op_sl_val(PCX)]=any;
      continue;
    }
      
      /* Integer arithmetic expression instructions */
    
    case plus:{			/* Var to var addition */
      register objPo e1 = FP[op_sh_val(PCX)];
      register objPo e2 = FP[op_sm_val(PCX)];

      save_regs(FP+min(op_sh_val(PCX),op_sm_val(PCX)),PC);

      if(IsInteger(e1)){
	if(IsInteger(e2))
	  FP[op_sl_val(PCX)] = allocateInteger(IntVal(e1)+IntVal(e2));
	else
	  FP[op_sl_val(PCX)] = allocateNumber(IntVal(e1)+FloatVal(e2));
      }
      else if(IsInteger(e2))
	FP[op_sl_val(PCX)] = allocateNumber(IntVal(e2)+FloatVal(e1));
      else
	FP[op_sl_val(PCX)] = allocateNumber(FloatVal(e1)+FloatVal(e2));

      restore_regs();
      continue;
    }

    case incr:{			/* increment int variable by fixed amount */
      register int off = op_sh_val(PCX);
      register objPo e1 = FP[off];

      save_regs(FP+min(off,op_sl_val(PCX)),PC);

      if(IsInteger(e1))
	FP[op_sl_val(PCX)] = allocateInteger(IntVal(e1)+op_sm_val(PCX));
      else if(IsFloat(e1))
	FP[op_sl_val(PCX)] = allocateNumber(FloatVal(e1)+op_sm_val(PCX));
      else
	RunErr("Illegal value in arithmetic",earith);

      restore_regs();
      continue;
    }

    case minus:{		/* Var to var subtract */
      register objPo e1 = FP[op_sh_val(PCX)];
      register objPo e2 = FP[op_sm_val(PCX)];

      save_regs(FP+min(op_sh_val(PCX),op_sm_val(PCX)),PC);

      if(IsInteger(e1)){
	if(IsInteger(e2))
	  FP[op_sl_val(PCX)] = allocateInteger(IntVal(e1)-IntVal(e2));
	else
	  FP[op_sl_val(PCX)] = allocateNumber(IntVal(e1)-FloatVal(e2));
      }
      else if(IsInteger(e2))
	FP[op_sl_val(PCX)] = allocateNumber(FloatVal(e1)-IntVal(e2));
      else
	FP[op_sl_val(PCX)] = allocateNumber(FloatVal(e1)-FloatVal(e2));

      restore_regs();
      continue;
    }

    case times:{		/* multiply */
      register objPo e1 = FP[op_sh_val(PCX)];
      register objPo e2 = FP[op_sm_val(PCX)];

      save_regs(FP+min(op_sh_val(PCX),op_sm_val(PCX)),PC);

      switch(TwoTag(Tag(e1),Tag(e2))){
      case TwoTag(integerMarker,integerMarker):{
	FP[op_sl_val(PCX)] = allocateNumber(((Number)IntVal(e1))*IntVal(e2));
	break;
      }

      case TwoTag(integerMarker,floatMarker):
	FP[op_sl_val(PCX)] = allocateNumber(IntVal(e1)*FloatVal(e2));
	break;

      case TwoTag(floatMarker,integerMarker):
	FP[op_sl_val(PCX)] = allocateNumber(FloatVal(e1)*IntVal(e2));
	break;

      case TwoTag(floatMarker,floatMarker):
	FP[op_sl_val(PCX)] = allocateNumber(FloatVal(e1)*FloatVal(e2));
	break;

      default:
	RunErr("Illegal value in arithmetic",earith);
      }
      restore_regs();
      continue;
    }

    case divide:{		/*  divide */
      register objPo e1 = FP[op_sh_val(PCX)];
      register objPo e2 = FP[op_sm_val(PCX)];
      Number A,B;

      switch(Tag(e1)){
      case integerMarker:
	A = (Number)IntVal(e1);
	break;
      case floatMarker:
	A = FloatVal(e1);
	break;
      default:
	RunErr("Illegal value in arithmetic",earith);
	continue;
      }

      switch(Tag(e2)){
      case integerMarker:
	B = (Number)IntVal(e2);
	break;
      case floatMarker:
	B = FloatVal(e2);
	break;
      default:
	RunErr("Illegal value in arithmetic",earith);
	continue;
      }

      if(B==0.0){
	RunErr("divide by zero",earith);
      }
      else{
	save_regs(FP+min(op_sh_val(PCX),op_sm_val(PCX)),PC);
	FP[op_sl_val(PCX)] = allocateNumber(A/B);
	restore_regs();
      }
      continue;
    }

    case eq:			/* Test two elements for equality */
      switch(equalcell(FP[op_sm_val(PCX)],FP[op_sl_val(PCX)])){
      case Fail:
	PC++;			/* skip the next instruction */
	continue;
      case Error:
	RunErr("incomparable value",ecompval);
      default:
	continue;
      }

    case neq:			/* Test two elements of stack for inequality */
      switch(equalcell(FP[op_sm_val(PCX)],FP[op_sl_val(PCX)])){
      case Ok:
	PC++;			/* skip the next instruction */
	continue;
      case Error:
	RunErr("incomparable value",ecompval);
      default:
	continue;
      }

    case le:			/* Test two elements of stack for less than */
      if(cmpcell(FP[op_sm_val(PCX)],FP[op_sl_val(PCX)])>0)
	PC++;			/* skip the next instruction */
      continue;

    case gt:			/* Test two elements of stack for gt than */
      if(cmpcell(FP[op_sm_val(PCX)],FP[op_sl_val(PCX)])<=0)
	PC++;			/* skip the next instruction */
      continue;

    case eqq:{			/* Test two elements for pointer equality */
      register objPo e1 = FP[op_sm_val(PCX)];
      register objPo e2 = FP[op_sl_val(PCX)];

      switch(TwoTag(Tag(e1),Tag(e2))){
      case TwoTag(integerMarker,integerMarker):
	if(IntVal(e1)!=IntVal(e2))
	  PC++;
	continue;
      case TwoTag(integerMarker,floatMarker):
	if(((Number)(IntVal(e1)))!=FloatVal(e2))
	  PC++;
	continue;
      case TwoTag(floatMarker,integerMarker):
	if(FloatVal(e1)!=((Number)(IntVal(e2))))
	  PC++;
	continue;
      case TwoTag(floatMarker,floatMarker):
	if(FloatVal(e1)!=FloatVal(e2))
	  PC++;
	continue;
      case TwoTag(symbolMarker,symbolMarker):
      case TwoTag(charMarker,charMarker):
      case TwoTag(tupleMarker,tupleMarker):
      case TwoTag(consMarker,consMarker):
      case TwoTag(listMarker,listMarker):
      case TwoTag(anyMarker,anyMarker):
      case TwoTag(handleMarker,handleMarker):
	if(e1==e2)
	  PC++;
	continue;
      default:
	continue;
      }
    }

    case neqq:{			/* Test two elements for pointer inequality */
      register objPo e1 = FP[op_sm_val(PCX)];
      register objPo e2 = FP[op_sl_val(PCX)];

      switch(TwoTag(Tag(e1),Tag(e2))){
      case TwoTag(integerMarker,integerMarker):
	if(IntVal(e1)==IntVal(e2))
	  PC++;
	continue;
      case TwoTag(integerMarker,floatMarker):
	if(((Number)(IntVal(e1)))==FloatVal(e2))
	  PC++;
	continue;
      case TwoTag(floatMarker,integerMarker):
	if(FloatVal(e1)==((Number)(IntVal(e2))))
	  PC++;
	continue;
      case TwoTag(floatMarker,floatMarker):
	if(FloatVal(e1)==FloatVal(e2))
	  PC++;
	continue;
      case TwoTag(symbolMarker,symbolMarker):
      case TwoTag(charMarker,charMarker):
      case TwoTag(consMarker,consMarker):
      case TwoTag(tupleMarker,tupleMarker):
      case TwoTag(listMarker,listMarker):
      case TwoTag(anyMarker,anyMarker):
      case TwoTag(handleMarker,handleMarker):
	if(e1!=e2)
	  PC++;
	continue;
      default:
	PC++;
	continue;
      }
    }

    /* Numeric equality */
    case feq:{			/* Test two elements for equality */
      register objPo e1 = FP[op_sm_val(PCX)];
      register objPo e2 = FP[op_sl_val(PCX)];

      switch(TwoTag(Tag(e1),Tag(e2))){
      case TwoTag(integerMarker,integerMarker):
	if(IntVal(e1)!=IntVal(e2))
	  PC++;
	continue;
      case TwoTag(integerMarker,floatMarker):
	if(((Number)(IntVal(e1)))!=FloatVal(e2))
	  PC++;
	continue;
      case TwoTag(floatMarker,integerMarker):
	if(FloatVal(e1)!=((Number)(IntVal(e2))))
	  PC++;
	continue;
      case TwoTag(floatMarker,floatMarker):
	if(FloatVal(e1)!=FloatVal(e2))
	  PC++;
	continue;
      default:
	RunErr("Illegal value in arithmetic",earith);
      }
      continue;
    }

    case fneq:{			/* Test two elements of stack for inequality */
      register objPo e1 = FP[op_sm_val(PCX)];
      register objPo e2 = FP[op_sl_val(PCX)];

      switch(TwoTag(Tag(e1),Tag(e2))){
      case TwoTag(integerMarker,integerMarker):
	if(IntVal(e1)==IntVal(e2))
	  PC++;
	continue;
      case TwoTag(integerMarker,floatMarker):
	if(((Number)(IntVal(e1)))==FloatVal(e2))
	  PC++;
	continue;
      case TwoTag(floatMarker,integerMarker):
	if(FloatVal(e1)==((Number)(IntVal(e2))))
	  PC++;
	continue;
      case TwoTag(floatMarker,floatMarker):
	if(FloatVal(e1)==FloatVal(e2))
	  PC++;
	continue;
      default:
	RunErr("Illegal value in arithmetic",earith);
      }
    }

    case fle:{			/* Test two elements of stack for less than */
      register objPo e1 = FP[op_sm_val(PCX)];
      register objPo e2 = FP[op_sl_val(PCX)];

      switch(TwoTag(Tag(e1),Tag(e2))){
      case TwoTag(integerMarker,integerMarker):
	if(IntVal(e1)>IntVal(e2))
	  PC++;
	continue;
      case TwoTag(integerMarker,floatMarker):
	if(((Number)(IntVal(e1)))>FloatVal(e2))
	  PC++;
	continue;
      case TwoTag(floatMarker,integerMarker):
	if(FloatVal(e1)>((Number)(IntVal(e2))))
	  PC++;
	continue;
      case TwoTag(floatMarker,floatMarker):
	if(FloatVal(e1)>FloatVal(e2))
	  PC++;
	continue;
      default:
	RunErr("Illegal value in arithmetic",earith);
      }
    }

    case fgt:{			/* Test two elements of stack for gt than */
      register objPo e1 = FP[op_sm_val(PCX)];
      register objPo e2 = FP[op_sl_val(PCX)];

      switch(TwoTag(Tag(e1),Tag(e2))){
      case TwoTag(integerMarker,integerMarker):
	if(IntVal(e1)<=IntVal(e2))
	  PC++;
	continue;
      case TwoTag(integerMarker,floatMarker):
	if(((Number)(IntVal(e1)))<=FloatVal(e2))
	  PC++;
	continue;
      case TwoTag(floatMarker,integerMarker):
	if(FloatVal(e1)<=((Number)(IntVal(e2))))
	  PC++;
	continue;
      case TwoTag(floatMarker,floatMarker):
	if(FloatVal(e1)<=FloatVal(e2))
	  PC++;
	continue;
      default:
	RunErr("Illegal value in arithmetic",earith);
      }
    }
    
    case ieq:			/* Test two integers for equality */
      if(IntVal(FP[op_sm_val(PCX)])!=IntVal(FP[op_sl_val(PCX)]))
	PC++;			/* skip the next instruction */
      continue;

    case ineq:			/* Test two integers for inequality */
      if(IntVal(FP[op_sm_val(PCX)])==IntVal(FP[op_sl_val(PCX)]))
	PC++;			/* skip the next instruction */
      continue;

    case ile:			/* Test two integers for less than */
      if(IntVal(FP[op_sm_val(PCX)])>IntVal(FP[op_sl_val(PCX)]))
	PC++;			/* skip the next instruction */
      continue;

    case igt:			/* Test two integers for gt than */
      if(IntVal(FP[op_sm_val(PCX)])<=IntVal(FP[op_sl_val(PCX)]))
	PC++;			/* skip the next instruction */
      continue;
    
    case iftrue:		/* does a var contain true? */
      if(FP[op_sh_val(PCX)]==ktrue)
	PC+=op_so_val(PCX);	/* skip if true */
      continue;

    case iffalse:		/* does a var not contain true? */
      if(FP[op_sh_val(PCX)]==kfalse)
	PC+=op_so_val(PCX);	/* skip if false */
      continue;

    /* miscellaneous instructions */
    case snd:{			/* send a message */
      save_regs(FP+op_sh_val(PCX),PC);

      sendAmsg(FP[op_sm_val(PCX)],FP[op_sl_val(PCX)],P->handle,emptyList);
      
      P=ps_pause(P);

      restore_regs();
      continue;
    }

    case self:			/* report id of this process */
      FP[op_sl_val(PCX)]=P->handle;
      continue;

    case gc:{                   // reserve space on heap
      logical ok;

      save_regs(FP+op_sh_val(PCX),PC);
      ok = reserveSpace(op_so_val(PCX));
      restore_regs();

      if(!ok)
	RunErr("out of heap space",esystem);
      continue;
    }

    case use:{			/* use a different click counter */
      if(P->priveleged){
	objPo clik_arg;

	FP[op_sl_val(PCX)] = P->clicks;	/* the order is important */
	clik_arg = FP[op_sm_val(PCX)]; /* new click counter */

	if(isClickCounter(clik_arg)){
	  if(taxiFare(P)==Ok)		/* allocate to current click counter  */
	    P->clicks = clik_arg;
	  else
	    goto error_recover;
	}
	else
	  RunErr("illegal click counter",einval);
      }
      else
	RunErr("privileged instruction",eprivileged);
      continue;
    }

    case line_d:		/* report on file and line number */
      if(SymbolDebug){		/* are we debugging? */
	if(TCPDebug){
	  save_regs(FP+op_sh_val(PCX),PC); /* save all registers */
	  line_debug(P,Lits[(*PC)&0xffff],op_o_val(PCX));
	  restore_regs();
	}
	else{
	  uniChar *fl = SymVal(Lits[(*PC)&0xffff]);
	  
          flushOut();
	  outMsg(logFile,"[%#w] line %U:%d\n",
		 P->handle,file_tail(fl,uniStrLen(fl)),op_so_val(PCX));
	}
      }

      PC++;
      continue;

    case entry_d:		/* report on procedure entry */
      if(SymbolDebug){
	if(TCPDebug){
	  save_regs(FP+op_sh_val(PCX),PC); /* save all registers */
	  entry_debug(P,Lits[op_o_val(PCX)]);
	  restore_regs();
	}
	else{
          flushOut();
	  outMsg(logFile,"[%#w] enter %U\n",P->handle,SymVal(Lits[op_o_val(PCX)]));
        }
      }
      continue;

    case exit_d:		/* report on procedure exit */
      if(SymbolDebug){
	if(TCPDebug){
	  save_regs(FP+op_sh_val(PCX),PC); /* save all registers */
	  exit_debug(P,Lits[op_o_val(PCX)]);
	  restore_regs();
	}
	else{
          flushOut();
	  outMsg(logFile,"[%#w] exit %U\n",P->handle,SymVal(Lits[op_o_val(PCX)]));
        }
      }
      continue;

    case assign_d:		/* report on variable assignment */
      if(SymbolDebug){
	if(TCPDebug){
	  save_regs(FP+op_sh_val(PCX),PC); /* save all registers */
	  assign_debug(P,FP[op_sm_val(PCX)],FP[op_sl_val(PCX)]);
	  restore_regs();
	}
	else{
          flushOut();
	  outMsg(logFile,"[%#w] assign %U = %#L\n",P->handle,
		 SymVal(FP[op_sm_val(PCX)]),FP[op_sl_val(PCX)]);
        }
      }
      continue;

    case return_d:		/* report on function return */
      if(SymbolDebug){
	if(TCPDebug){
	  save_regs(FP+op_sh_val(PCX),PC); /* save all registers */
	  return_debug(P,FP[op_sm_val(PCX)],FP[op_sl_val(PCX)]);
	  restore_regs();
	}
	else{
          flushOut();
	  outMsg(logFile,"[%#w] return %#L = %#L\n",P->handle,
		 FP[op_sm_val(PCX)],FP[op_sl_val(PCX)]);
        }
      }

      continue;

    case accept_d:			/* report accepting a message */
      if(SymbolDebug){
	objPo msg = FP[op_sl_val(PCX)];
	objPo from = FP[op_sm_val(PCX)];

	if(TCPDebug){
	  save_regs(FP+op_sh_val(PCX),PC); /* save all registers */
	  accept_debug(P,msg,from);
	  restore_regs();
	}
	else{
          flushOut();
	  outMsg(logFile,"[%#w] accept %#w from %w\n",P->handle,msg,from);
        }
      }
      continue;

    case send_d:			/* report sending a message */
      if(SymbolDebug){
	objPo msg = FP[op_sl_val(PCX)];
	objPo to = FP[op_sm_val(PCX)];

	if(TCPDebug){
	  save_regs(FP+op_sh_val(PCX),PC); /* save all registers */
	  send_debug(P,msg,P->handle,to);
	  restore_regs();
	}
	else{
          flushOut();
	  outMsg(logFile,"[%#w] send `%#L' to %w\n",P->handle,msg,to);
        }
      }
      continue;

    case fork_d:		/* report a process fork */
      if(SymbolDebug){
	if(TCPDebug){
	  save_regs(FP+op_sm_val(PCX),PC); /* save all registers */
	  fork_debug(P,FP[op_sl_val(PCX)]);
	  restore_regs();
	}
	else{
          flushOut();
	  outMsg(logFile,"[%#w] fork %w \n",P->handle,FP[op_sl_val(PCX)]);
        }
      }
      continue;

    case die_d:			/* report a process dying */
      if(SymbolDebug){
	if(TCPDebug){
	  save_regs(FP+op_sl_val(PCX),PC); /* save all registers */
	  die_debug(P);
	  restore_regs();
	}
	else{
          flushOut();
	  outMsg(logFile,"[%#w] dying\n",P->handle);
        }
      }
      continue;

    case scope_d:		/* report variable scope level */
      if(SymbolDebug){
	if(TCPDebug){
	  save_regs(FP+op_sm_val(PCX),PC); /* save all registers */
	  scope_debug(P,op_sl_val(PCX));
	  restore_regs();
	}
#ifdef EXECTRACE
	else{
          flushOut();
	  outMsg(logFile,"[%#w] variable scope %d\n",P->handle,op_sl_val(PCX));
        }
#endif
      }
      continue;

    case suspend_d:		/* report process suspension */
      if(SymbolDebug){
	if(TCPDebug){
	  save_regs(FP+op_sl_val(PCX),PC); /* save all registers */
	  suspend_debug(P);
	  restore_regs();
	}
	else{
          flushOut();
	  outMsg(logFile,"[%#w] suspend\n",P->handle);
        }
      }
      continue;

    case error_d:		/* report an error condition */
      if(SymbolDebug){
	if(TCPDebug){
	  save_regs(FP+op_sm_val(PCX),PC); /* save all registers */
	  error_debug(P,FP[op_sl_val(PCX)]);
	  restore_regs();
	}
	else{
          flushOut();
	  outMsg(logFile,"[%#w] error %w\n",P->handle,FP[op_sl_val(PCX)]);
        }
      }
      continue;

    case debug_d:		/* test for debugging */
      if(!SymbolDebug)		/* are we debugging? */
	PC+=op_so_val(PCX);
      continue;

    default:
      syserr("Unimplemented instruction");	/* unimplemented instruction */
    }
  }
}
  

static uniChar *file_tail(uniChar *n,WORD32 len)
{
  uniChar *p = uniLast(n,len,'/');
  if(p!=NULL)
    return p+1;
  else
    return n;
}

static objPo snipList(objPo list,int count)
{
  if(count<=0)
    return emptyList;
  else if(isNonEmptyList(list)){
    objPo sub = snipList(ListTail(list),count-1);
    objPo el = ListHead(list);
    
    if(sub!=NULL)
      return allocatePair(&el,&sub);
    else
      return NULL;
  }
  else
    return NULL;
}

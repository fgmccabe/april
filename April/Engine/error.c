/* 
  Error handling and exiting functions
  (c) 1994-1998 F.G. McCabe and Imperial College

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

#include "config.h"		/* pick up standard configuration header */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include "april.h"
#include "dict.h"
#include "symbols.h"
#include "process.h"
#include "astring.h"
#include "debug.h"

/* Fatal system error */
void syserr(char *msg)
{
  outMsg(logFile,"Fatal error: %s in %#w\n", msg,current_process->handle);
  april_exit(EXIT_FAIL);
}

/* Raise an error in a process */
void raiseError(processpo P,objPo errval)
{
  if(P->er<P->sb){		/* we have an error handler in place... */
    objPo *er = (objPo*)P->er[0];
    objPo *FP = P->fp;
    objPo *SP = P->sp;
    objPo env = P->e;

    while(FP<P->er){
      SP = FP;
      FP = (objPo*)(*SP++);	/* unwind the stack until we get to the ER */
      env = *SP++;
    }
    P->pc = (insPo)(P->er[1]);	/* new PC */
    P->e = env;
    P->er[0] = P->er[1] = kvoid; /* make sure the stack is clean */
    P->er = er;		        /* we have a new error handler base */
    P->fp = FP;
    P->sp = SP;
    P->errval = errval;         /* set up the error value */
    if(P->state!=runnable)
      ps_resume(P,False);       /* Wake up the process */
  }
  else{
    outMsg(logFile,"Run-time error `%.5w' in %#w\n",P->errval,P->handle);
    if(P==rootProcess)
      exit(1);
    else{
      P = ps_terminate(P);
      if(P==NULL)
	exit(1);
    }
  }
}

static retCode make_U_error_message(uniChar *errmsg,objPo code,processpo p);

/* Non-fatal library run-time error -- used mostly in escapes */
retCode Uliberror(char *name, int arity, uniChar *errmsg, objPo code)
{
  processpo p=current_process;

  if(p!=NULL){
    make_U_error_message(errmsg,code,p);

#ifdef EXECTRACE_
    if(SymbolDebug){		/* are we debugging? */
      if(TCPDebug)		/* are we debugging remotely? */
	error_debug(p,p->errval);
      else
	outMsg(logFile,"[%#w] run-time error: %w\n",p->handle,p->errval);
    }
#endif
  }

  return Error;		/* return the error flag */
}

uniChar errorMsg[1024];         // Available for reporting error message details

/* Run-time error support 
   - recover by sending error message, then get new process
 */
static retCode make_U_error_message(uniChar *errmsg,objPo code,processpo p)
{
  void *root = gcAddRoot(&code);
  objPo err = allocateConstructor(2);

  assert(code!=NULL);

  gcAddRoot(&err);
  
  {
    objPo msg = allocateString(errmsg);

    updateObj(err);		/* we need to let GC know we are updating. */
    
    ((consPo)err)->fn = kerror;
    ((consPo)err)->data[0] = msg;
    ((consPo)err)->data[1] = code;
  }


  p->errval=err;
  gcRemoveRoot(root);
  return Ok;
}



/* Non-fatal library run-time error -- used mostly in escapes */
retCode liberror(char *name, int arity, char *errmsg, objPo code)
{
  processpo p=current_process;

  if(p!=NULL){
    make_error_message(errmsg,code,p);

#ifdef EXECTRACE_
    if(SymbolDebug){		/* are we debugging? */
      if(TCPDebug)		/* are we debugging remotely? */
	error_debug(p,p->errval);
      else
	outMsg(logFile,"[%#w] run-time error: %w\n",p->handle,p->errval);
    }
#endif
  }

  return Error;		/* return the error flag */
}

/* Run-time error support 
   - recover by sending error message, then get new process
 */
retCode make_error_message(char *errmsg,objPo code,processpo p)
{
  void *root = gcAddRoot(&code);
  objPo err = allocateConstructor(2);

  assert(code!=NULL);

  gcAddRoot(&err);

  {
    objPo msg = allocateCString(errmsg);

    updateObj(err);		/* we need to let GC know we are updating. */
  
    ((consPo)err)->fn = kerror;
    ((consPo)err)->data[0] = msg;
    ((consPo)err)->data[1] = code;
  }

  p->errval=err;
  gcRemoveRoot(root);
  return Ok;
}

/* Almost fatal error -- running out of space */
retCode SpaceErr(void)
{
  logMsg(logFile,"Almost fatal error: run out of space in %#w",
	 current_process->handle);
  return Space;
}
  
void april_exit(int code)
{
  if(code!=0)
    outMsg(logFile,"Terminating with code %d\n",code);

  reset_stdin();		/* reset the standard input to be blocking */

  exit(code);
}



/*
  Process scheduling functions for the April run-time system
  (c) 1994-2003 Imperial College and F.G. McCabe

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
#include "april.h"		/* Main april header file */
#include "unix.h"
#include "dict.h"
#include "msg.h"
#include "clock.h"
#include "handle.h"
#include "fileio.h"
#include "process.h"
#include <sys/times.h>
#include <time.h>
#include <limits.h>

processpo run_q = NULL;		/* the process run queue */
processpo current_process;	/* This is the currently executing process */
int LiveProcesses = 0;		/* number of live processes */

#ifdef PROCTRACE
logical traceSuspend = False;	/* suspension tracing  */
logical stressSuspend=False;
#endif

#ifdef PROCTRACE_
static const char* state_names[] = {"quiescent", "runnable",
				    "wait_io", "wait_msg",
				    "wait_timer", "wait_clicks", "wait_memory","wait_lock","wait_child",
				    "dead"};
#endif

static void checkOutIo(void)
{
  fd_set fdin, fdout;
  struct timeval period;
  int inCount = set_in_fdset(&fdin);
  int outCount = set_out_fdset(&fdout);
  int fdCount = (inCount>outCount?inCount:outCount)+1;
  
  period.tv_sec = 0;
  period.tv_usec = 0;

  flushOut();

  if(select(fdCount,&fdin,&fdout,NULL,&period)>0)
    trigger_io(&fdin,&fdout,fdCount);
}

/*
 * No runnable processes.
 * Wait for a file such as the TCP connection or the keyboard to go ready
 * (or any other event that will resume a process)
 */
static void wait_for_event(void)
{
#ifdef PROCTRACE
  if(traceSuspend){
    logMsg(logFile,"%d process(es) suspended ...", LiveProcesses);
    displayProcesses();
  }
#endif
  
  stopTicks();			/* suppress the scheduler's heart beat */

  while(run_q==NULL){
    int status;
    fd_set inSet, outSet;
    int inCount = set_in_fdset(&inSet);
    int outCount = set_out_fdset(&outSet);
    int fdCount = (inCount>outCount?inCount:outCount)+1;

    flushOut();

    if(childDone)
      checkOutShells();

     if(wakeywakey) {		/* Has an interrupt happened to us? */
       wakeywakey = False;
       reset_timer();
       checkOutIo();		/* See if any IO has become ready */
     }
     if(run_q!=NULL)
       break;			/* The run_q might not be empty anymore */
				/* wait for something to happen */
#ifdef CLOCKTRACE
    if(traceClock)
      display_time_q();
#endif

    status = select(fdCount, &inSet, &outSet, NULL, nextTimeOut());

    if(childDone)
      checkOutShells();

    if(status == 0)
      wakeywakey = True;
    else if (status < 0 && !wakeywakey) {
      if(errno==EINTR){		/* got a signal other than SIGALRM */
	errno = 0;		/* clear the error flag */
	continue;
      }
      else logMsg(logFile,"select error %s in wait_for_event()",strerror(errno));
    }

    if(status>0)		/* trigger suspended processes */
      trigger_io(&inSet,&outSet,fdCount);

    if(wakeywakey) {
      wakeywakey = False;
      reset_timer();
    }
  }
#ifdef PROCTRACE
  if(traceSuspend && run_q!=NULL)
    logMsg(logFile,"restart with process %#w ...", run_q->handle);
#endif

  taxiFlag();			/* Start the taxi meter again */
  startTicks(-1);		/* restart the scheduler's heart beat */
}

/*
 *  suspend this process, and return next runnable
 */
processpo ps_suspend(register processpo p,process_state reason)
{
  assert(p==current_process);

  if(p->state==runnable)
    remove_from_run_q(p,reason);

  if(childDone)
    checkOutShells();

  if(wakeywakey){
    wakeywakey = False;
    reset_timer();
    checkOutIo();
  }

  if(run_q==NULL || p!=run_q)
    taxiFare(p);		/* decrement tank's click counter */

  if(run_q==NULL && LiveProcesses>0)
    wait_for_event();

#ifdef PROCTRACE_
  if(traceSuspend)
    logMsg(logFile,"resume %#w",run_q->handle);
#endif

  return current_process = run_q;
}

processpo ps_resume(register processpo p, register logical front)
{
  if(childDone)
    checkOutShells();

  if(wakeywakey){
    wakeywakey = False;
    reset_timer();
    checkOutIo();
  }

#ifdef PROCTRACE_
  if(traceSuspend)
    logMsg(logFile,"resume %#w",p->handle);
#endif

  taxiFare(current_process);	/* decrement tank's click counter */

  return current_process = add_to_run_q(p, front);
}

/*
 *  temporarily suspend this process but stay in the run queue
 */
processpo ps_pause(register processpo p)
{
  if(childDone)
    checkOutShells();

  if(wakeywakey){
    wakeywakey = False;
    reset_timer();
    checkOutIo();
  }

  taxiFare(current_process);	/* decrement tank's click counter */
  run_q = run_q->pnext;

#ifdef PROCTRACE_
  if(traceSuspend)
    logMsg(logFile,"Switching to %#w",run_q->handle);
#endif

  return current_process = run_q;
}

/*
 *  pause current process and switch to a new process
 */
processpo ps_switch(register processpo p,processpo P)
{
  if(childDone)
    checkOutShells();

  if(wakeywakey){
    wakeywakey = False;
    reset_timer();
    checkOutIo();
  }

  taxiFare(current_process);	/* decrement tank's click counter */

  if(P!=NULL){
    if(P->state!=runnable)
      current_process = add_to_run_q(P, True);
    else
      current_process = run_q = P;
  }
  else
    current_process = run_q = run_q->pnext;

#ifdef PROCTRACE_ 
  if(traceSuspend)
    logMsg(logFile,"resume %#w",run_q->handle);
#endif

  return run_q;
}

void ps_threadProcess(processpo P,processpo *queue,logical front)
{
  sigset_t blocked = stopInterrupts();  /* prevent interrupts now */

  assert(P!=NULL);

  if(*queue!=NULL){
    processpo Prev = (*queue)->pnext;
    
    assert(ps_state(*queue)!=runnable);
    
    P->pnext = (*queue);
    P->pprev = Prev;
      
    (*queue)->pprev = P;
    Prev->pnext = P;

    if(front)
      (*queue)=P;
  }
  else{
    *queue = P;
    P->pnext = P->pprev = P;	/* thread into self */
  }
  startInterrupts(blocked);
}

void ps_unthreadProcess(processpo P,processpo *queue)
{
  sigset_t blocked = stopInterrupts();  /* prevent interrupts now */

  assert(*queue!=NULL);

  if(P->pnext==P){
    assert(*queue==P);

    *queue = P->pnext = P->pprev = NULL;
  }
  else{
    if(*queue==P)
      *queue = P->pnext;

    P->pnext->pprev = P->pprev;
    P->pprev->pnext = P->pnext;
  }
  startInterrupts(blocked);
}


/*
 *  add process to run queue if not already in it
 *  Process may be added to the front or back
 */
processpo add_to_run_q(processpo p,logical front)
{
  processpo next = NULL;
  sigset_t blocked = stopInterrupts();  /* prevent interrupts now */

#ifdef PROCTRACE_
  if(traceSuspend)
    logMsg(logFile,"Put process %#w in run Q",p->handle);
#endif

  if(p->state!=runnable){
    if(p->state==quiescent || clicksLeft(p->clicks)>0){
      p->state=runnable;
      if(run_q!=NULL) {
	if (front == True) {
	  p->pnext = run_q->pnext;
	  run_q->pnext->pprev = p;
	  run_q->pnext = p;
	  p->pprev = run_q;

	  next = p;
	}
	else {
	  p->pprev = run_q->pprev;
	  run_q->pprev->pnext = p;
	  run_q->pprev = p;
	  p->pnext = run_q;

	  next = run_q;
	}
      }
      else {
	p->pprev = p;
	p->pnext = p;
	run_q = p;

	next = p;
      }
    }
    else
      next = run_q;		/* ignore request to schedule this process */
  }
  else
    next = p;

  startInterrupts(blocked);
  return next;
}

/* process time management ... */
#define CLICKS_OPAQUE 'C'

logical isClickCounter(objPo c)
{
  return c!=NULL && IsOpaque(c) && OpaqueType(c)==CLICKS_OPAQUE;
}

integer clicksLeft(objPo c)
{
  if(!isClickCounter(c))
    return 0;
  else
    return (WORD32)OpaqueVal(c);
}

static retCode clickOpaqueHdlr(opaqueEvalCode code,void *p,void *cd,void *cl);

objPo newClicks(integer cnt)
{
  static logical registered = False;

  if(!registered){
    registerOpaqueType(clickOpaqueHdlr,CLICKS_OPAQUE,NULL);
    registered=True;
  }
  return allocateOpaque(CLICKS_OPAQUE,(void*)cnt);
}

static retCode ps_credit_clicks(objPo to,integer amnt,objPo from)
{
  WORD32 *f_clicks = (WORD32*)(&OpaqueVal(from));
  WORD32 *t_clicks = (WORD32*)(&OpaqueVal(to));

  if(*f_clicks-amnt>0){
    *t_clicks+=amnt;
    *f_clicks-=amnt;
    return Ok;
  }
  else
    return Error;
}

static retCode clickOpaqueHdlr(opaqueEvalCode code,void *p,void *cd,void *cl)
{
  switch(code){
  case showOpaque:{
    ioPo f = (ioPo)cd;
    outMsg(f,"<<clicks %l>>",(WORD32)p);
    return Ok;
  }
  default:
    return Error;
  }
}

/*
 * Count relative number of ticks between successive calls
 * each tick is 0.1 millisec
 */
integer taxiFlag(void)
{
  static clock_t lasttime = 0;
  clock_t thistime = clock();	/* What time is it now? */

  if(lasttime==0){		/* First time around ... */
    lasttime = thistime;
    return 0;
  }
  else{
    register WORD32 gap = ((thistime-lasttime)*10000)/CLOCKS_PER_SEC;

    lasttime = thistime;
    return gap;
  }
}

/* Debit the current process with currently consumed clicks
   return error if there is no time remaining for the process
*/
static void tF(processpo p,void *c)
{
  objPo cl = (objPo)c;

  if(p->clicks==cl){
    p->errval = kclicked;	/* ran out of time */

    if(p->er<p->sb){		/* force the process into error recovery */
      while(p->fp<p->er){
	objPo *sp = p->sp = p->fp;
	p->fp = (objPo*)(*sp++); /* unwind the stack until we get to the ER */
	p->e = *sp++;
      }
      p->pc = (insPo)(p->er[1]);
      p->er = (objPo*)(p->er[0]); /* we have a new error handler base */
      p->clicks = rootClicks;	/* use the root's clicks */
      ps_resume(p,True);	/* resume the process... */
    }
    else
      ps_terminate(p);
  }
}

retCode taxiFare(processpo p)
{
  WORD32 clicks=taxiFlag();	/* Current state of the taxi flag */
  WORD32 *p_clicks = (WORD32*)(&OpaqueVal(p->clicks));

#ifdef RESTRACE
  if(traceResource && clicks!=0)
    logMsg(logFile,"Subtract %d cputime from %#w - new time = %d",
	   clicks,p->handle,*p_clicks-clicks);
#endif

  if(*p_clicks==LONG_MAX || (*p_clicks-=clicks)>0)
    return Ok;
  else{
    p->errval = kclicked;	/* ran out of time */
    processProcesses(tF,p->clicks);
    return Error;
  }
}

retCode m_new_clicks(processpo p,objPo *args)
{
  objPo clik_arg = args[0];	/* how many clicks to credit */

  if(!IsInteger(clik_arg)||IntVal(clik_arg)<=0)
    return liberror("__new_clicks",1,"1st argument invalid click count",einval);
  else if(!p->priveleged)
    return liberror("__new_clicks",1,"permission denied",eprivileged);
  else{
    args[0]=newClicks(IntVal(clik_arg));
    return Ok;
  }
}

retCode m_split_clicks(processpo p,objPo *args)
{
  objPo clik_arg = args[1];	/* Which counter to debit */
  objPo count = args[0];	/* How many to allocate */
  integer amount = IntVal(count);

  if(!isClickCounter(clik_arg))
    return liberror("__split_clicks",2,"1st argument invalid click counter",einval);
  else if(!IsInteger(count)||amount<=0)
    return liberror("__split_clicks",2,"2nd argument invalid count",einval);
  else if(amount>clicksLeft(clik_arg))
    return liberror("__spilt_clicks",2,"insufficient clicks left",efail);
  else{
    void *root = gcAddRoot(&clik_arg);
    objPo new = args[1] = newClicks(0);

    gcRemoveRoot(root);
    return ps_credit_clicks(new,amount,clik_arg);
  }
}

retCode m_use_clicks(processpo p,objPo *args)
{
  objPo clik_arg = args[0];	/* how many clicks to credit */

  if(!isClickCounter(clik_arg))
    return liberror("__use_clicks",1,"1st argument invalid click counter",einval);
  else if(!p->priveleged)
    return liberror("__use_clicks",1,"permission denied",eprivileged);
  else if(clicksLeft(clik_arg)<=0)
    return liberror("__use_clicks",1,"insufficient clicks",efail);
  else{
    taxiFare(p);		/* allocate to current click counter  */
    p->clicks = clik_arg;
    return Ok;
  }
}

retCode m_get_clicks(processpo p,objPo *args)
{
  objPo nme_arg = args[0];
  processpo P;

  if(!IsHandle(nme_arg) || (P=handleProc(nme_arg))==NULL)
    return liberror("__get_clicks",1,"argument invalid process handle",einval);
  else if(!isClickCounter(P->clicks))
    return liberror("__get_clicks",1,"no click counter",efail);
  else{
    args[0]=P->clicks;
    return Ok;
  }
}

retCode m_credit_clicks(processpo p,objPo *args)
{
  objPo from_arg = args[2];	/* handle of the process to debit */
  objPo to_arg = args[1];	/* handle of the process to credit */
  objPo clik_arg = args[0];	/* how many clicks to credit */
  WORD32 clicks;

  if(!isClickCounter(from_arg))
    return liberror("__credit_clicks",3,"1st argument should be valid click counter ",einval);
  else if(!isClickCounter(to_arg))
    return liberror("__credit_clicks",3,"2nd argument should be valid click counter ",einval);
  else if(!IsInteger(clik_arg)||(clicks=IntVal(clik_arg))<=0)
    return liberror("__credit_clicks",3,"3rd argument invalid click count",einval);
  else if(!p->priveleged)
    return liberror("__credit_clicks",3,"permission denied",eprivileged);
  else if(ps_credit_clicks(from_arg,clicks,to_arg)!=Ok)
    return liberror("__credit_clicks",3,"insufficient clicks in source",efail);
  else
    return Ok;
}

retCode m_merge_clicks(processpo p,objPo *args)
{
  objPo from_arg = args[1];	/* click counter to empty */
  objPo to_arg = args[0];	/* click counter to credit */

  if(!isClickCounter(from_arg))
    return liberror("__merge_clicks",2,"1st argument should be valid click counter ",einval);
  else if(!isClickCounter(to_arg))
    return liberror("__merge_clicks",2,"2nd argument should be valid click counter ",einval);
  else if(ps_credit_clicks(from_arg,clicksLeft(from_arg),to_arg)!=Ok)
    return liberror("__merge_clicks",2,"insufficient clicks in source",efail);
  else
    return Ok;
}


retCode m_clicks(processpo p,objPo *args)
{
  objPo clik_arg = args[0];

  if(!isClickCounter(clik_arg))
    return liberror("__clicks",1,"argument should be valid click counter ",einval);
  else{
    args[0]=allocateInteger(clicksLeft(clik_arg)); /* return clicks left */
    return Ok;
  }
}

/*
 *  Remove process from run queue.
 */
processpo remove_from_run_q(register processpo p,process_state reason)
{
  sigset_t blocked = stopInterrupts();  /* prevent interrupts now */

  assert(p->state==runnable);

  if(p->pnext != p) {
    p->state = reason;
    run_q = p->pnext;
    p->pprev->pnext = p->pnext;
    p->pnext->pprev = p->pprev;
  }
  else{
    p->state = reason;
    run_q = NULL;
  }
  startInterrupts(blocked);
  if(run_q==NULL && LiveProcesses>0)
    wait_for_event();

  return current_process = run_q;
}


#ifdef PROCTRACE
static logical inRunQ(processpo p)
{
  processpo q = run_q;

  do{
    if(p==q)
      return True;
    q = q->pnext;
  } while(q!=run_q);
  return False;
}

static void verify_proc(processpo p,void *c)
{
  if(p->state==runnable && !inRunQ(p))
    logMsg(logFile,"runnable process %#w not in runQ",p->handle);
}

void verifyRunQ(void)
{
  if(run_q!=NULL){
    processpo p = run_q;

    do{
      if(p->state!=runnable)
	logMsg(logFile,"non runnable process %#w in runQ",p->handle);
      p = p->pnext;
    } while(p!=run_q);
  }
  processProcesses(verify_proc,NULL);
}
#endif

void displayRunQ(void)
{
  processpo q = run_q;

  outMsg(logFile,"Run Q:\n");

  if(run_q!=NULL)
    do{
      displayProcess(q);
      if(q->state!=runnable)
	outMsg(logFile,"non runnable process %#w in runQ",q->handle);
      q = q->pnext;
    } while(q!=run_q);
  outMsg(logFile,"\n");
  flushFile(logFile);
}


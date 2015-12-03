/*
  Process control functions for the April run-time system
  (c) 1994-1999 Imperial College and F.G. McCabe

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
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>

#include <string.h>
#include <assert.h>
#include "april.h"		/* Main april header file */
#include "dict.h"
#include "msg.h"
#include "symbols.h"
#include "clock.h"
#include "astring.h"		/* String handling interface */
#include "fileio.h"
#include "unix.h"
#include "debug.h"
#include "opcodes.h"
#include "hash.h"		/* access the hash functions */
#include "std-types.h"

poolPo proc_pool=NULL;		/* pool of process records */

processpo monitorH = NULL;	/* Handle of the monitor process */
processpo newP = NULL;		/* process being constructed */
processpo rootProcess = NULL;

static WORD32 processno = 0;      /* This counts the total number of processes */

#ifdef RESTRACE
logical traceResource = False;
#endif


#define oprnd_msk 0xff

#define instr(c,v) (c|(v&vl_mask<<8)) /*  */
#define instrss(c,o1,o2) (c|((o1&oprnd_msk)<<16)|(o2&oprnd_msk)<<8)
#define instrsd(c,o1,o2) (c|((o1&oprnd_msk)<<24)|(o2&0xffff)<<8)
#define instrsss(c,o1,o2,o3) (c|((o1&oprnd_msk)<<24)|((o2&oprnd_msk)<<16)|(o3&oprnd_msk)<<8)

#define ccell(t) ((WORD32)t)

/* Forward declarations */
static processpo fork_process(uniChar *tgt,objPo name,
			      objPo code,logical priv,
			      objPo *creator,objPo *filer,
			      processpo mailer,objPo clicks);

static const char* state_names[] = {"quiescent", "runnable",
				    "wait_io", "wait_msg",
				    "wait_timer", 
				    "dead"};


static objPo dieEnv = NULL;	/* closure for outer level of all processes */
objPo rootClicks;		/* an infinite number of clicks */


static instruction end_code[] = {
  instr(die,0)		/* only a single instruction */
};

static objPo buildEnv(insPo cd,WORD32 cdlen,WORD32 litcnt,WORD32 frcount)
{
  objPo code =  allocateCode(cdlen,litcnt);
  void *root = gcAddRoot(&code);
  objPo env;

  SetCodeFormArity(code,procedure,0);
  
  {
    codePo pc = CodeVal(code);
    
    pc->type = kvoid;
    pc->frtype = kvoid;
    pc->size = cdlen;
    pc->litcnt = litcnt;
    
    memcpy(&pc->data,cd,cdlen*sizeof(instruction));
  }
  
  env = allocateConstructor(frcount);
  updateConsFn(env,code);

  gcRemoveRoot(root);
  return env;
}

/*
 * hand compiled the program:
 * defLoader(Url) => _load_code_(Url)(defLoader,Url)
 * which is the core of the default loader
 */

static objPo buildLoader(void)
{
  static instruction load_code[] = {    // The default loader helper
    instr(allocv,1),
    instrss(move,3,-1),                 // Save the URL
    instrss(move,3,-2),
    instrsd(esc_fun,-2,150),            // _load_code_(URL)
    instrss(emove,0,-3),                // pass in self
    instrss(move,-1,-4),
    instrsss(call,-4,2,-2),             // call the loaded program
    instrss(result,1,-3)
  };
  objPo ldEnv = buildEnv(load_code,NumberOf(load_code),0,1);
  void *root = gcAddRoot(&ldEnv);
  objPo type = allocateConstructor(2);
  objPo el = kvoid;
  
  gcAddRoot(&type);
  
  updateConsFn(type,kfunTp);
  
  el = allocateConstructor(1);
  
  updateConsFn(el,ktplTp);
  updateConsEl(el,0,kstringTp);
  updateConsEl(type,0,el);
  updateConsEl(type,1,kanyTp);
  
  updateConsEl(ldEnv,0,ldEnv);   // Its recursive
  
  updateCodeSig(codeOfClosure(ldEnv),type);
  
  gcRemoveRoot(root);
  
  return ldEnv;
}

static objPo buildRoot(uniChar *cwd,uniChar *bootfile)
{
  static instruction boot_seq[] = {
    instrsd(movl,-3,0),		/* the loaded code */
    instrss(emove,0,-1),        /* the loader helper */
    instrss(emove,1,-2),	/* the bootfile's URL */
    instrsss(call,-2,2,-3),	/* evaluate loaded code */
    instrss(emove,2,-2),        // The CWD
    instrsss(many,-1,-3,-4),    // unpack the result
    instr(die,0),               // If not a proper value
    instrsss(call,-2,1,-4),	/* call boot code */
    instr(die,0)		/* we are done */
  };
  objPo rootEnv = buildEnv(boot_seq,NumberOf(boot_seq),1,3);
  void *root = gcAddRoot(&rootEnv);
  objPo code = kvoid;
  objPo aprilCWD = allocateString(cwd);
  objPo boot = kvoid;
  objPo loader = kvoid;
  objPo type = allocateConstructor(1);
  
  updateConsFn(type,kprocTp);
  updateConsEl(type,0,kstringTp);

  gcAddRoot(&code);
  gcAddRoot(&boot);
  gcAddRoot(&aprilCWD);
  gcAddRoot(&loader);
  
  updateCodeSig(codeOfClosure(rootEnv),type);
  boot = allocateString(bootfile);
  
  loader = buildLoader();       // The default loader helper

  updateConsEl(rootEnv,0,loader);
  updateConsEl(rootEnv,1,boot);
  updateConsEl(rootEnv,2,aprilCWD);
  
  if(load_code_file(aprilSysPath,bootfile,&code,False)!=Ok){
    if(!uniStrLen(errorMsg)!=0)
      logMsg(logFile,"code in boot file [%U] doesn't verify: %U",bootfile,errorMsg);
    else
      logMsg(logFile,"failed to load from boot file [%U]",bootfile);
    april_exit(EXIT_FAIL);
  }

  updateCodeLit(codeOfClosure(rootEnv),0,code);

  gcRemoveRoot(root);
  return rootEnv;
}


void init_proc_tbl(int initial)
{
  proc_pool = newPool(sizeof(struct process_),initial);

  /* create standard outer environment for each process */
  dieEnv = buildEnv(end_code,NumberOf(end_code),0,0); 

  rootClicks = newClicks(LONG_MAX);
}

static processpo rootProc(uniChar *tgt,uniChar *name,objPo code)
{
  processpo top = fork_process(tgt,newUniSymbol(name),code,True,&kvoid,&kvoid,NULL,rootClicks);

  top->creator = top->handle;
  top->filer = top->handle;

  return rootProcess = top;
}

void bootstrap(uniChar *tgt,uniChar *name,uniChar *cwd,uniChar *bootfile)
{
  emulate(rootProc(tgt,name,buildRoot(cwd,bootfile)));
}

/*
 *  Terminate a process 
 */
processpo ps_terminate(processpo p)
{
  if(p!=NULL && p->state!=dead){
    objPo handle = p->handle;
    void *root = gcAddRoot(&handle);

    flush_from_time_q(p);	/* remove any active timer records */

    discard_msgs(p);

    if(monitorH!=NULL){		/* this must be done first... */
      objPo msg = allocateConstructor(1);
      
      gcAddRoot(&msg);
      
      updateConsFn(msg,ktermin);
      updateConsEl(msg,0,handle);

      msg = allocateAny(&monitorTp,&msg);

      LocalMsg(monitorH,msg,handle,emptyList); /* send the message */
    }

    if(p->state==runnable)
      remove_from_run_q(p,dead);

    p->state = dead;		/* mark this process as a zombie */
    
    free(p->stack);		/* Dispose of the old evaluation stack */

    LiveProcesses--;


    bindHandle(p->handle,NULL); /* clear the handle structures */
    p->handle=emptyList;	/* flag that the process no longer exists */

    freePool(proc_pool,p);	/* add record back into the process pool */

    if(wakeywakey){
      wakeywakey = False;
      reset_timer();
    }

    gcRemoveRoot(root);
  }
  return current_process;
}

process_state ps_state(processpo p)
{
  return p->state;
}

void *ps_client(processpo p)	/* Get the process's client information */
{
  return p->cl;
}

void ps_set_client(processpo p,void *cl)
{
  p->cl = cl;
}

processpo runerr(processpo p)
{
  p = ps_terminate(p);		/* Kill erroneous proces */
  if(LiveProcesses<=2)
    return NULL;
  else
    return p;
}

/*********************************************************************/
/*                Display Processes                                  */
/*********************************************************************/
void displayProcess(processpo p)
{
  struct msg_rec *m=p->mfront;

  outMsg(logFile,"%#w [%s]\n",p->handle,state_names[p->state]);

  /* print the messages for this process */
  while(m){
    outMsg(logFile,"Msg %d from %#w `%.5w'\n",m->sequence,m->sender,m->msg);
    m=m->next;
  }
}

static void dP(processpo p,void *c)
{
  displayProcess(p);
}

void displayProcesses(void)
{
  processProcesses(dP,NULL);
  flushFile(logFile);
}

/* Display the whole stack trace of a process */
void stackTrace(processpo p)
{
  if(p->state!=dead){		/* only process live processes */
    objPo *fp=p->fp;
    objPo *sp=p->sp;
    objPo *sb=p->sb;
    msgpo m=p->mfront;
    WORD32 level = 0;
    objPo *er=p->er;
    objPo e=p->e;
    WORD32 codeOffset = p->pc-CodeCode(codeOfClosure(p->e));

    outMsg(logFile,"Stack trace of [%s] %w",state_names[p->state],
	   p->handle);

    while(sp<sb){
      register WORD32 i = fp-sp;	/* the length of the local environment */

      outMsg(logFile,"\nStack frame #%d\n",level);

      outMsg(logFile,"sp=0x%x\n",sp);
      outMsg(logFile,"fp=0x%x\n",fp);
      outMsg(logFile,"e=%.3w\n",e);
      outMsg(logFile,"pc=%+d\n",codeOffset);

      for(i=fp-sp;i>0;i--){
	if(&sp[i-2]==er){
	  outMsg(logFile,"Error frame: EPC=PC+%d\n",
		 (insPo)er[1]-CodeCode(codeOfClosure(e)));
	  er = (objPo*)er[0];	/* move the error handler pointer up */
	  i--;
	}
	else
	  outMsg(logFile,"fp[%d]=%.3w\n",i-(fp-sp)-1,sp[i-1]);
      }

      e = fp[1];		/* The environment of next frame */
      codeOffset = (insPo)(fp[2])-CodeCode(codeOfClosure(e));
      sp = fp+3;		/* step over the return address and env*/
      fp = *(objPo**)fp;	/* go back to the previous frame pointer */

      level++;
    }

    outMsg(logFile,"errval = %.2w\n",p->errval);
    outMsg(logFile,"creator = %.2w\n",p->creator);
    outMsg(logFile,"filer = %.2w\n",p->filer);
    if(p->mailer!=NULL)
      outMsg(logFile,"mailer = %.2w\n",p->mailer->handle);

    while(m){
      outMsg(logFile,"msg %d from %#w `%.5w'\n",m->sequence,m->sender,m->msg);
      m=m->next;
    }
    flushFile(logFile);
  }
}


/* 
  Process stack manipulations 
*/

int grow_stack(processpo p, int factor)
{
  int nsz = (p->sb-p->stack)*factor; /* New stack size */
  objPo *st = (objPo*)malloc(sizeof(objPo)*nsz);
  register objPo *s;
  register WORD32 i;
  register objPo *fp = p->fp;
  register objPo *sp = p->sp;
  register objPo *er = p->er;

#ifdef PROCTRACE
  if(traceSuspend)
    logMsg(logFile,"Growing e-stack of process %#w",p->handle); 
#endif

  if(st==NULL)
    return SpaceErr();

  p->sp=s= st+nsz-(p->sb-sp);	/* Set up a pointer to the new top of stack */
  p->fp = st+nsz-(p->sb-fp);	/* And the current frame pointer */
  p->er = st+nsz-(p->sb-er);	/* And the current error handler frame */
  
  while(sp<p->sb){
    i=fp-sp;

    while(i--){			/* Copy the local variables */
      if(er==sp){		/* we are at an error handler */
	*s++=(objPo)(st+nsz-(p->sb-(objPo*)(*sp++))); /* adjust error frame */
	*s++=*sp++;		/* copy the program counter across */
	er = (objPo*)*er;	/* next error handler frame */
	i--;			/* we moved two slots */
      }
      else
	*s++ = *sp++;
    }

    *s++ = (objPo)(st+nsz-(p->sb-(objPo*)*fp)); /* Adjusted frame pointer */
    *s++ = *(fp+1);		/* and the environment */
    *s++ = *(fp+2);		/* and the return address */
    fp = (objPo*)*fp;
    sp+=3;
  }

  free(p->stack);		/* Dispose of the old evaluation stack */
  p->stack = st;
  p->sb = st+nsz;		/* New stackbase */
  return Ok;
}

static processpo fork_process(uniChar *tgt,objPo name,
			      objPo code,logical priv,
			      objPo *creator,objPo *filer,processpo mailer,
			      objPo clicks)
{
  void *root = gcAddRoot(&code);
  register processpo p = allocPool(proc_pool);
  objPo *scr;			/* Scratch pointer */

  if(p==NULL){
    logMsg(logFile,"unable to allocate new process record");
    return(NULL);
  }

  p->stack = (objPo*)malloc(sizeof(objPo)*DEFSTACK); /* allocate a run-time stack */
  p->sb = &p->stack[DEFSTACK];
  p->priveleged = priv;        /* Privileged process? */
  p->er = p->sb;		/* set the error handler to stack base */
  p->errval = kvoid;		/* No error messages at the moment */
  p->e=dieEnv;			/* standard outer closure */

  p->creator = *creator;	/* store the creator of the process */
  p->filer = *filer;		/* and the file manager  */

  if(mailer!=NULL)
    p->mailer = mailer;	/* and the mail manager */
  else
    p->mailer = p;		/* we are our own mailer also */

  p->mfront = p->mback = NULL;/* initialize the message queue */
  p->sequence = 0;		/* No messages received yet */

  scr = p->sb;		/* initialize the stack */
  {
    insPo *pc = (insPo*)(--scr);

    *pc = CodeCode(codeOfClosure(dieEnv));  /* address for process to die in extremis */
  }
  *--scr = dieEnv;		/* dummy environment pointer */
  *--scr=(objPo)p->sb;	/* and the dummy fp */
  p->sp = p->fp = scr;	/* set up the stack and frame pointers */
  p->e = code;                // Push in the call to the code to run
  p->pc = CodeCode(codeOfClosure(code));
    
  p->state = quiescent;	/* not yet executing */
  p->cl = NULL;		/* no client data yet */

  p->clicks = clicks;		/* set the click counter */

  newP = p;			/* we need to set this up incase we get a GC */
  p->handle = kvoid;		/* in case of GC */
  p->handle = buildHandle(p,tgt,SymText(name));
  
  newP = NULL;

  LiveProcesses++;

#ifdef PROCTRACE
  if(traceSuspend)
    logMsg(logFile,"New process [%#w]",p->handle);
#endif
  current_process = add_to_run_q(p,True);
  gcRemoveRoot(root);
  return p;
}

/* Fork a process --  normal mode _fork_(code) */
retCode m_fork(processpo p,objPo *args)
{
  processpo NP;			/* process record*/
  objPo code = args[0];		/* pointer to code argument */
  uniChar tgt[MAX_SYMB_LEN];

  if(!(isClosure(code) && CodeArity(codeOfClosure(code))==0))
    return liberror("_fork_",1,"illegal code fragment",einval);

  strMsg(tgt,NumberOf(tgt),"%d",processno++);

  NP = fork_process(tgt,handleName(p->handle),code,p->priveleged,
		    &p->handle,
		    &p->filer,
		    p->mailer,p->clicks);
  
  if(NP==NULL)
    return liberror("_fork_",1,"cant fork process",einval);
  else{
    args[0] = NP->handle;

    return Switch;		/* will cause new process to be entered */
  }
}

/* Fork a process --  priveleged mode fork
   _fork_(name,creator,filer,mailer,clicks,priv,code) */
retCode m_fork2(processpo p,objPo *args)
{
  processpo NP;			/* process record*/
  objPo nme_arg = args[5];	/* pointer to name argument */
  objPo *creator = args+4;	/* pointer to creator argument */
  objPo *filer = args+3;	/* pointer to file manager process */
  objPo ml_arg = args[2];	/* pointer to mailer process */
  objPo prv_arg = args[1];	/* is process priveleged? */
  objPo proc = args[0];		/* pointer to code argument */
  processpo mailer;
  logical priveleged = True;

  if(!p->priveleged)
    return liberror("__fork_",6,"permission denied",eprivileged);
  else
    priveleged = prv_arg==ktrue;

  if(!IsHandle(nme_arg))
    return liberror("__fork_",6,"1st argument should be handle",einval);
  
  if(handleProc(nme_arg)!=NULL)
    return liberror("__fork_",6,"duplicate process name",einval);

  if(!IsHandle(*creator))
    return liberror("__fork_",6,"2nd argument should be handle",einval);

  if(!IsHandle(*filer))
    return liberror("__fork_",6,"3rd argument should be handle",einval);

  if(!IsHandle(ml_arg) || (mailer=handleProc(ml_arg))==NULL)
    return liberror("__fork_",6,"4th argument should be local handle",einval);

  if(!isSymb(prv_arg) || (prv_arg!=ktrue && prv_arg!=kfalse))
    return liberror("__fork_",6,"5th argument should be logical",einval);

  if(!(isClosure(proc) && CodeArity(codeOfClosure(proc))==0))
    return liberror("__fork_",6,"tth argument should be 0-ary procedure",einval);

  {
    uniChar tgt[MAX_SYMB_LEN];
    objPo name;
    
    if(nme_arg==knullhandle){
      strMsg(tgt,NumberOf(tgt),"%d",processno++);
      name = handleName(p->handle);
    }
    else{
      strMsg(tgt,NumberOf(tgt),"%U",SymText(handleTgt(nme_arg)));
      name = handleName(nme_arg);
    }
      
    NP = fork_process(tgt,name,proc,priveleged,creator,filer,mailer,p->clicks);
  }

  if(NP==NULL)
    return liberror("__fork_",6,"cant fork process",einval);
  else{
    args[5] = NP->handle;

    return Switch;		/* will cause new process to be entered */
  }
}

/* Identification functions */
/* Self name */
retCode m_self(processpo p,objPo *args)
{
  args[-1] = p->handle;
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

/* Name of the creator */
retCode m_creator(processpo p,objPo *args)
{
  if(p->creator==NULL)
    return liberror("creator",0,"root process",eprivileged);
  else{
    args[-1]=p->creator;
    p->sp=args-1;			/* adjust caller's stack pointer */
    return Ok;
  }
}

/* Name of the creator of an arbitrary process */
retCode m__creator(processpo p,objPo *args)
{
  if(p->creator==NULL)
    return liberror("_creator",1,"root process",eprivileged);
  else if(!p->priveleged)
    return liberror("_creator",1,"permission denied",eprivileged);
  else if(!IsHandle(args[0]))
    return liberror("_creator",1,"argument should be a handle",einval);
  else{
    processpo h = handleProc(args[0]);

    if(h==NULL)
      return liberror("_creator",1,"non-local process",einval);
    args[0]=h->creator;
    return Ok;
  }
}

/* True if top-most process */
retCode m_topmost(processpo p,objPo *args)
{
  if(p->creator==NULL)
    args[-1]=ktrue;
  else
    args[-1]=kfalse;
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

/* Host identification */
retCode m_host(processpo p,objPo *args)
{
  uniChar *mn = machineName();
  args[-1]=allocateString(mn);
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

/* Escape functions to access process state */
retCode m_done(processpo p,objPo *args)
{
  processpo P = handleProc(args[0]);

  if(P!=NULL && P->state!=dead)
    args[0]=kfalse;
  else
    args[0]=ktrue;
  return Ok;
}

retCode m_messages(processpo p,objPo *args)
{
  args[-1]=allocateInteger(msgcount(p));
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

void MonitorDie(objPo H)	/* send a termination message */
{
}

/* Name of the monitor process */
retCode m_monitor(processpo p,objPo *args)
{
  if(monitorH!=NULL)
    args[-1]=monitorH->handle;
  else
    args[-1]=kvoid;
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

retCode m_set_monitor(processpo p,objPo *args) /* set name of the monitor process */
{
  if(!IsHandle(args[0]))
    return liberror("_monitor",1,"argument should be a handle",einval);
  else{
    processpo m = handleProc(args[0]);

    if(m==NULL)
      return liberror("_monitor",1,"argument should be a handle",einval);

    monitorH = m;		/* set the id of the monitor */
    return Ok;
  }
}

/* Name of the filer process */
retCode m_filer(processpo p,objPo *args)
{
  args[-1]=p->filer;
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

/* set name of the filer process */
retCode m_file_manager(processpo p,objPo *args) 
{
  if(!p->priveleged)
    return liberror("_filer",1,"permission denied",eprivileged);

  if(!IsHandle(args[0]))
    return liberror("_filer",1,"argument should be a handle",einval);

  p->filer = args[0];
  return Ok;
}

retCode m_state(processpo p,objPo *args)
{
  if(!IsHandle(args[0]))
    return liberror("state",1,"argument should be a handle",einval);
  else{
    processpo P = handleProc(args[0]);

    if(P==NULL)
      args[0]=newSymbol("unknown");
    else
      args[0]=newSymbol(state_names[p->state]);
    return Ok;			/* return nothing to do */
  }
}

retCode m_kill(processpo p,objPo *args)
{
  processpo P;

  if(IsHandle(args[0]) && ((P=handleProc(args[0]))!=NULL)){
    if(p==P)
      return liberror("_kill",1,"cant kill own process",eprivileged);
    else if(P!=handleProc(p->creator) && p!=monitorH)
      return liberror("_kill",1,"premission denied",eprivileged);
    else
      ps_terminate(P);
  }
  else
    return liberror("_kill",1,"permission denied",eprivileged);

  return Ok;
}

retCode m_interrupt(processpo p,objPo *args)
{
  processpo other;
  objPo proc = args[0];
  objPo err = args[1];
  
  if(!p->priveleged)
    return liberror("_interrupt",2,"permission denied",eprivileged);
  else if(IsHandle(proc) && ((other=handleProc(proc))!=NULL)){
    raiseError(other,err);
    return Switch;
  }
  else
    return liberror("_interrupt",2,"invalid target process",einval);
}

retCode m_wait_msg(processpo p,objPo *args)
{
  if(msgcount(p)==0){
    p->pc--;
    ps_suspend(p,wait_msg);
    return Suspend;
  }
  else
    return Ok;
}

static inline insPo CodeBase(objPo b)
{
  objPo e = Forwarded(b)?FwdVal(b):b;
  objPo p = consFn(e);

  codePo pc = CodeVal(Forwarded(p)?FwdVal(p):p);

  return &pc->data[0];
}

static inline insPo CodeEnd(objPo b)
{
  objPo e = Forwarded(b)?FwdVal(b):b;
  objPo p = codeOfClosure(e);

  codePo pc = CodeVal(Forwarded(p)?FwdVal(p):p);

  return &pc->data[pc->size];
}

/* Support for GC of processes */

/* This one is for normal copying process... */
void scanProcess(processpo p)
{
  if(p->state!=dead){		/* only process live processes */
    objPo *fp=p->fp;
    objPo *sp=p->sp;
    objPo *sb=p->sb;
    objPo *er=p->er;
    objPo pe=p->e;
    msgpo msgs=p->mfront;

    while(sp<sb){
      objPo e = fp[1];		/* The environment of this frame */
      register WORD32 i = fp-sp;	/* the length of the local environment */

      while(i--){
	if(sp==er){
          //	  er[1] = (objPo)((insPo)er[1]-CodeBase(pe));
	  er = (objPo*)er[0];	/* move the error handler pointer up */
	  sp+=2;
	  i--;
	}
	else{
	  *sp=scanCell(*sp);	/* scan the local variables */
	  sp++;
	}
      }
	
      /* replace absolute return with relative one */
      //      fp[2]=(objPo)((insPo)fp[2]-CodeBase(e));
      fp[1]=scanCell(fp[1]);	/* scan the environment part of the frame */
      fp = *(objPo**)fp;	/* go back to the previous frame pointer */
      sp +=3;			/* step over the return address and env*/
      pe = e;
    }

    p->errval = scanCell(p->errval);	/* and the error message */
    p->handle = scanCell(p->handle);	/* process handle */
    p->creator = scanCell(p->creator);	/* process creator */
    p->filer = scanCell(p->filer);	/* process file manager */
    //    p->pc = (insPo)(p->pc-CodeBase(p->e));
    p->e = scanCell(p->e);	/* scan the process's environment */
    p->clicks = scanCell(p->clicks);	/* click counter */

    while(msgs!=NULL){
      msgs->msg = scanCell(msgs->msg); /* Scan this message */
      msgs->opts = scanCell(msgs->opts);
      msgs->sender = scanCell(msgs->sender);
      msgs = msgs->next;
    }
  }
}

/* Pre scan of process -- make all return pointers relative */
void presetProcess(processpo p)
{
  if(p->state!=dead){		/* only process live processes */
    objPo *fp=p->fp;
    objPo *sb=p->sb;
    objPo *er=p->er;
    objPo pe=p->e;

    while(fp<sb){
      objPo e = fp[1];		/* The environment of this frame */

      while(er<fp){		/* readjust the error frames */
	er[1] = (objPo)(((insPo)er[1])-CodeBase(pe));
	er = (objPo*)er[0];
      }

      fp[2] = (objPo)(((insPo)fp[2])-CodeBase(e));

      fp = *(objPo**)fp;	/* go back to the previous frame pointer */
      pe = e;
    }

    p->pc = (insPo)(p->pc-CodeBase(p->e));
  }
}

static void presetProc(processpo p,void *cl)
{
  presetProcess(p);
}

void presetProcesses(void)
{
  processProcesses(presetProc,NULL);

  if(newP!=NULL)
    presetProcess(newP);        /* fixes a hole in GC ... */
}


/* Post scan of process -- readjust all return pointers back to absolute */
void resetProcess(processpo p)
{
  if(p->state!=dead){		/* only process live processes */
    objPo *fp=p->fp;
    objPo *sb=p->sb;
    objPo *er=p->er;
    objPo pe=p->e;

    while(fp<sb){
      objPo e = fp[1];		/* The environment of this frame */

      while(er<fp){		/* readjust the error frames */
	er[1] = (objPo)(((WORD32)er[1])+CodeBase(pe));
	er = (objPo*)er[0];
      }

      fp[2] = (objPo)(((WORD32)fp[2])+CodeBase(e));

      assert(((insPo)fp[2])>=CodeBase(e) && ((insPo)fp[2])<CodeEnd(e));

      fp = *(objPo**)fp;	/* go back to the previous frame pointer */
      pe = e;
    }

    p->pc = CodeBase(p->e) + (WORD32)(p->pc);
  }
}

void scanProcesses(objPo base,objPo limit)
{
  presetProcesses();
  scanHandles(base,limit);	/* this also marks all active processes */
  if(newP!=NULL)
    scanProcess(newP);		/* fixes a hole in GC ... */
  dieEnv = scanCell(dieEnv);	/* scan the standard process wrapper closure */
  rootClicks = scanCell(rootClicks);
}

static void resetProc(processpo p,void *cl)
{
  resetProcess(p);
}

void resetProcesses(void)
{
  processProcesses(resetProc,NULL);

  if(newP!=NULL)
    resetProcess(newP);		/* fixes a hole in GC ... */
}

/* This one is for compacting garbage collection... */
void markProcess(processpo p)
{
  if(p->state!=dead){		/* only process live processes */
    objPo *fp=p->fp;
    objPo *sp=p->sp;
    objPo *sb=p->sb;
    objPo *er=p->er;
    objPo pe=p->e;
    msgpo msgs=p->mfront;

    while(sp<sb){
      objPo e = fp[1];		/* The environment of this frame */
      register WORD32 i = fp-sp;	/* the length of the local environment */

      while(i--){
	if(sp==er){
          //	  assert((insPo)er[1]>=CodeBase(pe) && (insPo)er[1]<CodeEnd(pe));
          //	  er[1] = (objPo)((insPo)er[1]-CodeBase(pe));
	  i--;
	  er = (objPo*)er[0];
	  sp+=2;
	}
	else
	  markCell(*sp++);	/* scan the local variables */
      }

      pe = e;
      /* replace absolute return with relative one */
      //      fp[2]=(objPo)((insPo)fp[2]-CodeBase(e));
      markCell(fp[1]);		/* mark the environment part of the frame */
      fp = *(objPo**)fp;	/* go back to the previous frame pointer */
      sp +=3;			/* step over the return address and env*/
    }

    markCell(p->e);		/* mark the process's environment */
    markCell(p->errval);	/* and the error message */
    markCell(p->handle);	/* process handle */
    markCell(p->creator);	/* process creator */
    markCell(p->filer);		/* process file manager */
    markCell(p->clicks);	/* process click counter */
    //    p->pc = (insPo)(p->pc-CodeBase(p->e));

    while(msgs!=NULL){
      markCell(msgs->msg); /* Scan this message */
      markCell(msgs->opts);
      markCell(msgs->sender);
      msgs = msgs->next;
    }
  }
}

/* Post mark of process -- readjust all return pointers back to absolute */
void adjustProcess(processpo p)
{
  if(p->state!=dead){		/* only process live processes */
    objPo *fp=p->fp;
    objPo *sp=p->sp;
    objPo *sb=p->sb;
    objPo *er=p->er;
    objPo pe = p->e = adjustCell(p->e); /* scan the process's environment */

    msgpo msgs=p->mfront;

    while(sp<sb){
      objPo e = fp[1] = adjustCell(fp[1]); /* The environment of this frame */
      register WORD32 i = fp-sp;	/* the length of the local environment */

      while(i--){
	if(sp==er){
	  er[1] = (objPo)(((WORD32)er[1])+CodeBase(pe));
	  assert((insPo)er[1]>=CodeBase(pe) && (insPo)er[1]<CodeEnd(pe));
	  er = (objPo*)er[0];
	  i--;
	  sp+=2;
	}
	else{
	  *sp=adjustCell(*sp);	/* adjust the local variables */
	  sp++;
	}
      }
	
      pe = e;
      fp[2] = (objPo)(((WORD32)fp[2])+CodeBase(e));
      fp = *(objPo**)fp;	/* go back to the previous frame pointer */
      sp +=3;			/* step over the return address and env*/
    }

    p->errval = adjustCell(p->errval);	/* and the error message */
    p->handle = adjustCell(p->handle);	/* process handle */
    p->creator = adjustCell(p->creator); /* process creator */
    p->filer = adjustCell(p->filer);	/* process file manager */
    p->clicks = adjustCell(p->clicks);	/* process click counter */
    p->pc = CodeBase(p->e) + (WORD32)(p->pc);

    while(msgs!=NULL){
      msgs->msg = adjustCell(msgs->msg); /* Scan this message */
      msgs->opts = adjustCell(msgs->opts);
      msgs->sender = adjustCell(msgs->sender);
      msgs = msgs->next;
    }
  }
}

void markProcesses(void)
{
  presetProcesses();
  markHandles();
  if(newP!=NULL)
    markProcess(newP);		/* fixes a hole in GC ... */
  
  markCell(dieEnv);		/* mark the standard process wrapper closure */
  markCell(rootClicks);
}

static void adjustProc(processpo p,void *cl)
{
  adjustProcess(p);
}

void adjustProcesses(void)
{
  processProcesses(adjustProc,NULL);
  if(newP!=NULL)
    adjustProcess(newP);	/* fixes a hole in GC ... */

  dieEnv = adjustCell(dieEnv);	/* mark the standard process wrapper closure */
  rootClicks = adjustCell(rootClicks);
}


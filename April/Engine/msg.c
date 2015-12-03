/*
  Message handling functions
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

#include "config.h"		/* pick up standard configuration header */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "april.h"
#include "process.h"
#include "signature.h"
#include "clock.h"
#include "dict.h"
#include "symbols.h"
#include "msg.h"
#include "astring.h"		/* String handling interface */
#include "debug.h"
#include "std-types.h"

#ifdef MSGTRACE
logical traceMessage = False;	/* message tracing  */
#endif

#define MSGMAX 10		/* reschedule if too many messages waiting */
#define MAXMSG 100		/* initial size of message pool */

poolPo msg_pool;		/* pool of message records */

void init_msgs(void)
{
  msg_pool = newPool(sizeof(struct msg_rec),MAXMSG); /* make a message pool */
}

static time_t leaseTime(objPo opts)
{
  while(opts!=emptyList){
    objPo el = ListHead(opts);
    
    if(isConstructor(el,kleasetime) && IsInteger(consEl(el,0)))
      return (time_t)IntVal(consEl(el,0));

    opts = ListTail(opts);
  }
  return (time_t)0;		/* no time out */
}

static objPo replyHandle(objPo opts,objPo sender)
{
  while(opts!=emptyList){
    objPo el = ListHead(opts);
    
    if(isConstructor(el,kreplyto))
      return consEl(el,0);

    opts = ListTail(opts);
  }
  return sender;		/* no special replyto */
}

static objPo receiptRequest(objPo opts)
{
  while(opts!=emptyList){
    objPo el = ListHead(opts);
    
    if(isConstructor(el,kreceiptrequest))
      return consEl(el,0);

    opts = ListTail(opts);
  }
  return NULL;			/* no receipt required */
}

retCode sendAmsg(objPo to,objPo msg,objPo sender,objPo opts)
{
  processpo p;
  processpo mailer;

  /* is this to be handled locally? */
  if(to==knullhandle)
    return Ok;
  else if((p=handleProc(to))!=NULL)
    LocalMsg(p,msg,sender,opts);
  else if((mailer=current_process->mailer)!=NULL){
    void *root = gcAddRoot(&msg);
    objPo m;

    gcAddRoot(&opts);
    gcAddRoot(&sender);
    gcAddRoot(&to);

    m = allocateTuple(4);
    gcAddRoot(&m);

    updateTuple(m,0,to);	/* construct the message structure */
    updateTuple(m,1,sender);
    updateTuple(m,2,opts);
    updateTuple(m,3,msg);

    m = allocateAny(&msgTp,&m);

    LocalMsg(mailer,m,sender,emptyList);
    gcRemoveRoot(root);
  }
  else
    return Error;
  return Switch;		/* Try to switch to the recipient */
}

static void leaseExpireAck(processpo p,objPo sender,objPo receipt)
{
  void *root = gcAddRoot(&receipt);
  gcAddRoot(&sender);

  {
    objPo rec = allocateConstructor(1);

    gcAddRoot(&rec);
    
    updateConsFn(rec,ktimedout);
    updateConsEl(rec,0,receipt);
    
    rec = allocateAny(&monitorTp,&rec);

    sendAmsg(sender,rec,p->handle,emptyList);

#ifdef MSGTRACE
    if(traceMessage)
      outMsg(logFile,"Sending receipt of discarded msg  `%.4w' from %#w\n",
	     receipt,sender);
#endif

  }
  gcRemoveRoot(root);
}

void LocalMsg(processpo p,objPo msg,objPo sender,objPo opts)
{
  sigset_t blocked = stopInterrupts();  /* prevent interrupts now */

  {
    time_t lease = leaseTime(opts); /* pick out the lease time */

    if(lease!=(time_t)0 && lease<time(NULL)){	/* verify that message is OK */
      objPo receipt = receiptRequest(opts);

#ifdef MSGTRACE
      if(traceMessage)
	outMsg(logFile,"Discarding out of date msg  `%.4w' from %#w\n",
	       msg,sender);
#endif

      if(receipt!=NULL)		/* acknowledge discarding of old message */
	leaseExpireAck(p,sender,receipt);
    }
    else{
      register msgpo m = (msgpo)allocPool(msg_pool);
      objPo receipt;

      if(m==NULL)
	syserr("message pool exhausted");

#ifdef MSGTRACE
      if(traceMessage)
	outMsg(logFile,"Handing msg  `%.4w' to %#w from%#w\n",
	       msg,p->handle,sender);
#endif

      m->to = p;
      m->sender = sender;
      m->opts = opts;
      m->msg = msg;
      m->lease = lease;	
      m->next = NULL;
      m->sequence = ++p->sequence; /* set sequence number of this message */

      if(p->mback) {
	p->mback->next = m;
	m->prev = p->mback;
	p->mback = m;
      }
      else {
	m->prev = NULL;
	p->mfront = p->mback = m;
      }

      if(p->state==wait_msg)	/* has this process been descheduled? */
	add_to_run_q(p,True);	/* Then reschedule it */

      if((receipt=receiptRequest(opts))!=NULL){
	void *root = gcAddRoot(&sender);
	
	gcAddRoot(&receipt);

	receipt = allocateAny(&kanyTp,&receipt);
	
	sendAmsg(sender,receipt,p->handle,emptyList);
	gcRemoveRoot(root);
      }
    }
  }

  startInterrupts(blocked);		/* re-anable interrupts */
}

/* Basic message escape */
retCode m_send(processpo p,objPo *args)
{
  objPo to = args[2];
  objPo opts = args[1];
  objPo msg = args[0];

  if(!IsHandle(to))
    return liberror("_send",3,"argument should be a handle",einval);

  switch(sendAmsg(to,msg,current_process->handle,opts)){
  case Switch:
    return Switch;
  case Space:
    return liberror("_send",3,"out of heap space",esystem);
  default:
    return Ok;
  }
}

/* Priviledged send message escape */
retCode m_send2(processpo p,objPo *args)
{
  objPo to = args[3];
  objPo opts = args[2];
  objPo msg = args[1];
  objPo sender = args[0];

  if(!IsHandle(to))
    return liberror("__send",4,"argument should be a handle",einval);
  if(!IsHandle(sender))
    return liberror("__send",4,"argument should be a handle",einval);
  if(!p->priveleged)
    return liberror("__send",4,"permission denied",eprivileged);

  switch(sendAmsg(to,msg,sender,opts)){
  case Switch:
    return Switch;
  case Space:
    return liberror("__send",4,"out of heap space",esystem);
  case Ok:
    return Ok;
  default:
    return Error;
  }
}

retCode m_front_msg(processpo p,objPo *args)
{
  objPo msg = args[2];
  objPo opts = args[1];
  objPo sender = args[0];

  if(!IsHandle(args[0]))
    return liberror("_front_msg",3,"argument should be a handle",einval);
  else{
    register msgpo m = (msgpo)allocPool(msg_pool);

    if(m == NULL)
      return SpaceErr();

#ifdef MSGTRACE
    if(traceMessage)
      outMsg(logFile,"Put message `%.5w' on front of %#w's msg Q \n",
	     msg,p->handle);
#endif

    m->to = p;			/* this message is being sent to ourselves */
    m->sender = sender;
    m->opts = opts;
    m->msg = msg;
    m->lease = leaseTime(opts);	/* pick out the lease time */
    m->next = NULL;
    m->prev = NULL;

    if(p->mfront!=NULL){
      m->sequence = p->mfront->sequence-1; /* Fake the sequence number ... */
      m->next = p->mfront;
      p->mfront = m;
    }
    else{
      m->sequence = ++p->sequence; /* set sequence number of this message */
      p->mfront=p->mback= m;
    }

    return Ok;			/* return OK */
  }
}

/* Get the next available message, return an error if no messages available */
retCode m_getmsg(processpo p,objPo *args)
{
  integer msgNo = IntVal(args[0]);
  msgpo msg = p->mfront; /* Start by looking at the message Q */
  time_t now = time(NULL);

  while(msg!=NULL){
    if(msg->sequence<=msgNo)
      msg = msg->next;
    else if(msg->lease!=(time_t)0 && msg->lease<now){
      msgpo next = msg->next;
      objPo receipt = receiptRequest(msg->opts);

#ifdef MSGTRACE
      if(traceMessage)
	outMsg(logFile,"Discarding out of date msg  `%.4w' from %#w\n",
	       msg->msg,msg->sender);
#endif

      if(receipt!=NULL)
	leaseExpireAck(p,msg->sender,receipt);

      free_msg(p,msg);
      msg = next;
    }
    else
      break;
  }

  if(msg!=NULL){		/* set up the return tuple for this message */
    objPo seq = allocateInteger(msg->sequence);
    void *root = gcAddRoot(&seq);
    objPo reply = replyHandle(msg->opts,msg->sender);

    gcAddRoot(&reply);
    args[0] = allocateTuple(5);

    updateTuple(args[0],0,msg->msg);	/* construct the result structure */
    updateTuple(args[0],1,msg->sender);
    updateTuple(args[0],2,reply);
    updateTuple(args[0],3,msg->opts);
    updateTuple(args[0],4,seq);

    gcRemoveRoot(root);
    free_msg(p,msg);
    return Ok;
  }
  else				/* no messages ... */
    return liberror("__getmsg",1,"no messages",efail);
}


/* Get a message, but suspend if no message available */
retCode m_nextmsg(processpo p,objPo *args)
{
  integer msgNo = IntVal(args[0]);
  msgpo msg = p->mfront; /* Start by looking at the message Q */
  time_t now = time(NULL);

  while(msg!=NULL){
    if(msg->sequence<=msgNo)
      msg = msg->next;
    else if(msg->lease!=(time_t)0 && msg->lease<now){
      msgpo next = msg->next;
      objPo receipt = receiptRequest(msg->opts);

#ifdef MSGTRACE
      if(traceMessage)
	outMsg(logFile,"Discarding out of date msg  `%.4w' from %#w\n",
	       msg->msg,msg->sender);
#endif

      if(receipt!=NULL)
	leaseExpireAck(p,msg->sender,receipt);

      free_msg(p,msg);
      msg = next;
    }
    else
      break;
  }

  if(msg!=NULL){		/* set up the return tuple for this message */
    objPo reply = replyHandle(msg->opts,msg->sender);
    void *root = gcAddRoot(&reply);
    objPo seq = allocateInteger(msg->sequence);

    gcAddRoot(&seq);

    args[0] = allocateTuple(5);

    updateTuple(args[0],0,msg->msg);	/* construct the result structure */
    updateTuple(args[0],1,msg->sender);
    updateTuple(args[0],2,reply);
    updateTuple(args[0],3,msg->opts);
    updateTuple(args[0],4,seq);

    gcRemoveRoot(root);

    free_msg(p,msg);
    return Ok;
  }
  else{				/* no messages ... */
    p->pc--;
    ps_suspend(p,wait_msg);
    return Suspend;		/* suspend for a message receive */
  }
}

static void wakeMeUp(processpo p,void *cl)
{
  add_to_run_q(p,True);
}

/* Wait a specified time for a message */
retCode m_waitmsg(processpo p,objPo *args)
{
  integer msgNo = IntVal(args[1]);
  msgpo msg = p->mfront;	/* Start by looking at the message Q */
  time_t now = time(NULL);

  while(msg!=NULL){
    if(msg->sequence<=msgNo)
      msg = msg->next;
    else if(msg->lease!=(time_t)0 && msg->lease<now){
      msgpo next = msg->next;

#ifdef MSGTRACE
    if(traceMessage)
      outMsg(logFile,"Discarding out of date msg  `%.4w' from %#w\n",
	     msg->msg,msg->sender);
#endif
      free_msg(p,msg);
      msg = next;
    }
    else
      break;
  }

  if(msg!=NULL){		/* set up the variables for this message */
    objPo replyH = replyHandle(msg->opts,msg->sender);
    void *root = gcAddRoot(&replyH);
    objPo seq = allocateInteger(msg->sequence);

    gcAddRoot(&seq);

    args[1] = allocateTuple(5);

    updateTuple(args[1],0,msg->msg);	/* construct the result structure */
    updateTuple(args[1],1,msg->sender);
    updateTuple(args[1],2,replyH);
    updateTuple(args[1],3,msg->opts);
    updateTuple(args[1],4,seq);

    free_msg(p,msg);

    gcRemoveRoot(root);
    return Ok;
  }
  else{				/* no messages ... check the timeout */
    Number timeout;
    if(IsInteger(args[0]))
      timeout=(Number)IntVal(args[0]);
    else
      timeout=FloatVal(args[0]);

    switch(set_alarm(p,timeout,wakeMeUp,NULL)){
    case Ok:                          /* already fired ... raise an exception */
      p->errval=ktimedout;
      return Error;                     /* return an error condition... */
    case Space:
      return liberror("__waitmsg",2,"out of space",esystem);
    case Suspend:
      p->pc--;
      ps_suspend(p,wait_msg);
      return Suspend;                   /* suspend for a message receive */
    default:
      p->errval=kfailed;
      return Error;                     /* shouldnt happen */
    }
  }
}

retCode m_replacemsg(processpo p,objPo *args)
{
  sigset_t blocked = stopInterrupts();  /* prevent interrupts now */

  {
    objPo msg = args[3];
    objPo sender = args[2];
    objPo opts = args[1];
    integer msgNo = IntVal(args[0]);
    time_t lease = leaseTime(opts);

    if(lease!=(time_t)0&&lease<time(NULL)){
      objPo receipt = receiptRequest(opts);

#ifdef MSGTRACE
      if(traceMessage)
	outMsg(logFile,"Discarding out of date msg  `%.4w' from %#w\n",
	       msg,sender);
#endif

      if(receipt!=NULL)		/* acknowledge discarding of old message */
	leaseExpireAck(p,sender,receipt);
    }

    {
      register msgpo m = (msgpo)allocPool(msg_pool);
      msgpo mp = p->mfront;

      if(m==NULL)
	syserr("message pool exhausted");

#ifdef MSGTRACE
      if(traceMessage)
	outMsg(logFile,"Replacing msg `%.4w' from %w\n",msg,sender);
#endif

      m->to = p;
      m->sender = sender;
      m->opts = opts;
      m->lease = lease;
      m->msg = msg;
      m->next = NULL;
      m->sequence = msgNo;

      while(mp!=NULL && mp->sequence<msgNo)
	mp=mp->next;

      if(mp!=NULL){		/* mp points to the message AFTER replacement msg */
	m->prev = mp->prev;
	m->next = mp;
	if(mp->prev!=NULL)
	  mp->prev->next = m;
	else
	  p->mfront = m;	/* first message in the queue */
	mp->prev = m;
      }
      else{
	if(p->mback) {
	  p->mback->next = m;
	  m->prev = p->mback;
	  p->mback = m;
	}
	else {
	  m->prev = NULL;
	  p->mfront = p->mback = m;
	}
      }
    }
  }

  startInterrupts(blocked);		/* re-anable interrupts */
  return Ok;
}

/*
 * return message record to message pool
 */
void free_msg(processpo p,msgpo m)
{
  if (m->prev == NULL)
    p->mfront = m->next;
  else m->prev->next = m->next;

  if (m->next == NULL)
    p->mback = m->prev;
  else m->next->prev = m->prev;

  /* add back to message pool */
  freePool(msg_pool,(void*)m);
}

/* count the number of messages waiting at a process */
int msgcount(processpo p)
{
  register msgpo m = p->mfront;
  register int cnt = 0;

  while(m){
    cnt++;
    m = m->next;
  }
  return(cnt);
}

/*
 * throw away pending messages for a dying process
 */
void discard_msgs(processpo p)
{
  register msgpo m = p->mfront;

  while(m!=NULL){
    void *r = (void*)m;
    m=m->next;
    freePool(msg_pool,r);
  }
}

#ifdef MSGTRACE

/* print the messages for one process */
static void print_proc_msgs(processpo p,void *c)
{
  if(p!=NULL && p->state!=dead && p->mfront!=NULL){
    struct msg_rec *m=p->mfront;
    
    outMsg(logFile,"Messages waiting for [%w]\n",p->handle);

    while(m){
      outMsg(logFile,"Message from %w[%w]: `%.5w'\n",
	     m->sender,m->opts,m->msg);
      m=m->next;
    }
  }
}

/* print all the messages that are outstanding for each process */
void printMessages(void)
{
  outMsg(logFile,"Printing out messages\n");
  processProcesses(print_proc_msgs,NULL);
}

#endif

/*
 * Additional escape functions and procedures
 */

/*
 * Support for the mailer process
 */

retCode m_mailer(processpo p,objPo *args)
{
  if(p->mailer!=NULL)
    args[-1]=p->mailer->handle;
  else
    args[-1]=kvoid;
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

retCode m_set_mailer(processpo p,objPo *args) /* set name of the mailer process */
{
  processpo mlr;

  if(!IsHandle(args[0]) || (mlr=handleProc(args[0]))==NULL)
    return liberror("_set_mailer",1,"argument should be a handle",einval);
  else if(!p->priveleged)
    return liberror("_set_mailer",1,"permission denied",eprivileged);

  p->mailer = mlr;		/* set the mailer handle */

  return Ok;
}


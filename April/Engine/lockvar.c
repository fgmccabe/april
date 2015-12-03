/* 
   Lock synchronization functions
   (c) 2002 F.G. McCabe

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
#include <string.h>
#include <stdlib.h>
 
#include <assert.h>
#include <limits.h>

#include "april.h"
#include "process.h"
#include "dict.h"
#include "symbols.h"
#include "opaque.h"
#include "str.h"
#include "lock.h"

typedef struct _lock_ *lockPo;

typedef struct _lock_ {
  WORD32 count;                   // How many times has the current thread acquired this lock?
  processpo owner;              // Owner process of the lock
  processpo queue;              // Queue of threads waiting on this lock
} LockRec;

retCode acquireLock(processpo P,objPo l)
{
  lockPo lock = (lockPo)OpaqueVal(l);
  
  if(lock->count!=0){
    if(lock->owner!=P){
      P->pc--;
      if(ps_suspend(P,wait_lock)==NULL)
        return Error;           // deadlock detected (albeit crudely)
      
      ps_threadProcess(P,&lock->queue,False);        // put this process on the queue
      return Switch;
    }
    else{
      lock->count++;
      return Ok;                // This is our lock
    }
  }
  else{
    lock->count=1;
    lock->owner = P;
    return Ok;                  // We just got ourselves a new lock
  }
}

retCode releaseLock(processpo P,objPo l)
{
  lockPo lock = (lockPo)OpaqueVal(l);
  
  if(lock->owner==P){
    assert(lock->count>0);
  
    lock->count--;

    if(lock->count==0){
      lock->owner = NULL;
    
      if(lock->queue!=NULL){
        processpo q = lock->queue;
        while(lock->queue!=NULL){
          ps_unthreadProcess(q=lock->queue,&lock->queue);
          ps_resume(q,True);        // Stick the new process on the front
        }
        return Switch;
      }
      else
        return Ok;                // No switching needed
    }
    else
      return Ok;
  }
  else
    return Ok;                  // We don't have the lock anyway
}

// waitLock releases a lock and puts the client on the lock's wait queue
retCode waitLock(processpo P,objPo l)
{
  lockPo lock = (lockPo)OpaqueVal(l);
  
  if(lock->count==1){
    lock->owner = NULL;
    lock->count = 0;
    
    if(lock->queue!=NULL){
      processpo q = lock->queue;
      while(lock->queue!=NULL){ // release others waiting for the lock
        ps_unthreadProcess(q=lock->queue,&lock->queue);
        ps_resume(q,True);        // Stick the new process on the front
      }
    }
    P->pc--;
    if(ps_suspend(P,wait_lock)==NULL)
      return Error;           // deadlock detected (albeit crudely)
      
    ps_threadProcess(P,&lock->queue,False);        // put this process on the queue
    return Switch;
  }
  else
    return Error;               // Can't wait reliably if have the lock more than once
}

static retCode lockOpaqueHdlr(opaqueEvalCode code,void **p,void *cd,void *cl)
{
  switch(code){
  case showOpaque:{
    ioPo f = (ioPo)cd;
    lockPo ff = (lockPo)p;
    processpo o = ff->owner;
    char *sep = "";
    
    if(o!=NULL)
      outMsg(f,"<<lock %d %#w [",ff->count,&o->handle);
    else
      outMsg(f,"<<lock %d [",ff->count);
      
    o = ff->queue;
    while(o!=NULL){
      processPo n = (o->pnext==ff->queue?NULL:o->pnext);
        
      outMsg(f,"%s %#w",sep,&o->handle);
      o = n;
      sep = ",";
    }
     
    return outMsg(f,"]>>");
  }
  default:
    return Ok;
  }
}
      
retCode m_newLock(processpo P,objPo *args)
{
   LockRec lk = {0,NULL,NULL};
   objPo var = permOpaque(lockHandle,sizeof(LockRec),(void**)&lk);
    
   return unify(P,&var,&a[1]);
  }
}

retCode g_acquireLock(processPo P,ptrPo a)
{
  ptrI A1 = deRefI(&a[1]);
  objPo o1 = objV(A1);
  
  if(isvar(A1))
    return liberror("__acquireLock",eINSUFARG);
  else if(!IsOpaque(o1) || OpaqueType((opaquePo)o1)!=lockHandle)
    return liberror("__acquireLock",eINVAL);
  else{
    switch(acquireLock(P,A1)){
      case Ok:
        return Ok;
      case Switch:
        P->PC--;                // Step the processor back one instruction
        return Switch;
      case Error:
      default:
        return liberror("__acquireLock",eDEAD);
    }
  }
}
        
retCode g_waitLock(processPo P,ptrPo a)
{
  ptrI A1 = deRefI(&a[1]);
  objPo o1 = objV(A1);
  
  if(isvar(A1))
    return liberror("__waitLock",eINSUFARG);
  else if(!IsOpaque(o1) || OpaqueType((opaquePo)o1)!=lockHandle)
    return liberror("__waitLock",eINVAL);
  else{
    switch(waitLock(P,A1)){
      case Ok:
        return Ok;
      case Switch:
        return Switch;
      case Error:
        return liberror("__waitLock",eFAIL);
      default:
        return liberror("__waitLock",eINVAL);
    }
  }
}
        
retCode g_releaseLock(processPo P,ptrPo a)
{
  ptrI A1 = deRefI(&a[1]);
  objPo o1 = objV(A1);
  
  if(isvar(A1))
    return liberror("__releaseLock",eINSUFARG);
  else if(!IsOpaque(o1) || OpaqueType((opaquePo)o1)!=lockHandle)
    return liberror("__releaseLock",eINVAL);
  else{
    switch(releaseLock(P,A1)){
      case Ok:
        return Ok;
      case Switch:
        return Switch;
      case Error:
      default:
        return liberror("__releaseLock",eINVAL);
    }
  }
}
        
void initLocks(void)
{
  registerOpaqueType(lockOpaqueHdlr,lockHandle,NULL);
}


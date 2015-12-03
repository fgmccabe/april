/*
  April library utility functions
  (c) 1994-2003 Imperial College and F.G.McCabe

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
#include <limits.h>
#include <string.h>
#include <ctype.h>

#include "unix.h"		/* include stuff for Unix compilation */
#include "april.h"
#include "process.h"
#include "dict.h"
#include "astring.h"		/* String handling interface */
#include "fileio.h"
#include "setops.h"
#include "pool.h"

/*
 * envir()
 * 
 * report environmment variables into a list whose elements are pairs of the
 * form (variable,value) .
 * 
 * The function uses the external variable environ, created by the Unix system,
 * which is a table of strings of the form "VARIABLE=value". (Same
 * information as obtained by the shell command setenv).
 * 
 */

retCode m_envir(processpo p,objPo *args)
{
  extern char **environ;	/* the environment table provided by Unix*/
  char *pt;
  int i;
  objPo out = emptyList;
  objPo last = emptyList;
  objPo pair = emptyList;
  void *root = gcAddRoot(&out);

  gcAddRoot(&pair);
  gcAddRoot(&last);

  /* construction of pairs [variable,value] */
  for(i=0; environ[i]; i++){
    if((pt=strchr(environ[i],'=')) != NULL){
      WORD32 nlen = strlen(environ[i]);
      uniChar nbuffer[MAX_SYMB_LEN];
      uniChar *name = (nlen<NumberOf(nbuffer)?nbuffer:(uniChar*)malloc(sizeof(uniChar)*(nlen+1)));
      objPo val = kvoid;

      gcAddRoot(&val);

      utf8_uni((unsigned char*)environ[i],pt-environ[i],name,NumberOf(name));

      pair = allocateTuple(2);
      val = allocateString(name);
      updateTuple(pair,0,val);
      val = allocateCString(pt+1);
      updateTuple(pair,1,val);

      if(name!=nbuffer)
        free(name);

      if(last==emptyList)
	last = out = allocatePair(&pair,&emptyList);
      else{
	updateListTail(last,allocatePair(&pair,&emptyList));
	last = ListTail(last);
      }
    }
  }

  args[-1]=out;			/* return the result list */
  p->sp = args-1;		/* adjust sp in case of gc */
  gcRemoveRoot(root);
  return Ok;
}


/*
 * setenv(variable,value)
 *
 * sets an environment variable to be the specified value
 *
 */
retCode m_setenv(processpo p,objPo *args)
{
  if(!p->priveleged)
    return liberror("setenv", 2, "permission denied",eprivileged);
  else{
    WORD32 nlen = ListLen(args[1])+1;
    WORD32 vlen = ListLen(args[0])+1;
    char nBuff[nlen];
    char vBuff[vlen];

    Cstring(args[1],nBuff,nlen);
    Cstring(args[0],vBuff,vlen);

    if(setenv(nBuff,vBuff,1)==0)
      return Ok;
    else
      return liberror("setenv",2,"not enough space to extend environment",efail);
  }
}

/*
 * getenv(variable)
 *
 * returns the value of an environment variable
 *
 */
retCode m_getenv(processpo p,objPo *args)
{
  char ename[MAX_SYMB_LEN];
  char *name = Cstring(args[0],ename,NumberOf(ename));
  char *ret = getenv(name);

  if(ret==NULL){
    uniChar em[MAX_SYMB_LEN];

    strMsg(em,NumberOf(em),"environment var '%s' not defined",name);
    return Uliberror("getenv",1,em,efail);
  }

  args[0]=allocateCString(ret);
  return Ok;
}


/* This is used to attach a shell to a process, so that when the child terminates
   becomes available, the right process is kicked off
*/

logical childDone = False;              /* Set to true when a child process has signalled to us */

typedef struct _shellAttach_ *attachPo;

typedef struct _shellAttach_ {
  int pid;                 /* What child process are we looking at? */
  processpo p;                          /* What process is involved */
  attachPo next;                /* Next in the attachments */
} ShellAttachRec;

static attachPo attachments = NULL;
static poolPo atPool = NULL;

static retCode attachProcessToShell(int pid,processpo P)
{
  attachPo a = attachments;

  while(a!=NULL){
    if(a->pid!=0 && a->p==P){
      P->pc--;		/* step back to allow a re-trial */
      ps_suspend(P,wait_child);
      return Switch;
    }
    a = a->next;
  }

  if(atPool==NULL)              /* This is the first time */
    atPool = newPool(sizeof(ShellAttachRec),8);
    
  a = (attachPo)allocPool(atPool);
  
  a->pid = pid;
  a->p = P;
  a->next = attachments;
  attachments = a;
  P->pc--;		      /* step back to allow a re-trial */
  ps_suspend(P,wait_child);
  return Switch;
}

static void detachProcessFromShell(int pid,processpo P)
{
  attachPo *a = &attachments;

  while((*a)!=NULL){
    if((*a)->pid==pid){
      attachPo n = *a;
      (*a) = (*a)->next;
      freePool(atPool,n);
      break;
    }
    else
      a = &((*a)->next);
  }
}

void checkOutShells(void)
{
  if(childDone){
    attachPo a = attachments;
    childDone = False;

    while(a!=NULL){
      ps_resume(a->p,False);
      a = a->next;
    }
  }
}

/*
 * shell()
 * 
 * invoke Unix shell
 * 
 * argument : a symbol passed as a command to sh.
 * 
 * Note : this function suspends the thread until the command is terminated and returns the
 * termination status. 
 * 
 */

retCode m_shell(processpo p,objPo *args)
{
  int pid = (int)ps_client(p);

  if(pid!=0){
    int r;
    int status;

    if((r=waitpid(pid,&status,WNOHANG))==0){
      p->pc--;                     /* step back to allow a re-trial */
      ps_suspend(p,wait_child);
      return Switch;
    }
    else if(r<0){
      if(errno!=ECHILD && errno!=EINTR)
        return liberror("shell",3,"problem with child",efail);
      else{
        p->pc--;		      /* step back to allow a re-trial */
        ps_suspend(p,wait_child);
        return Switch;
      }
    }
    else{
      args[2] = allocateInteger(WEXITSTATUS(status)); /* command result */

      ps_set_client(p,NULL); /* clear client-information from process */
      detachProcessFromShell(pid,p);
      return Ok;
    }
  }
  else{
    objPo pth = args[2];
    objPo ags = args[1];
    objPo envs = args[0];
    WORD32 blen = ListLen(pth)*2;
    char cmdBuff[blen];
    char *cmd = Cstring(pth,cmdBuff,blen);
  
    if(!executableFile(cmd))
      return liberror("shell",3,"non-executable file",eprivileged);
    else if(!IsList(ags))
      return liberror("shell",3,"2nd argument should be a list of strings",einval);
    else if(!IsList(envs))
      return liberror("shell",3,"3rd argument should be a list of strings",einval);
    else{
      int i,pid;
      WORD32 a_len = ListLen(ags);
      WORD32 e_len = ListLen(envs);
      char **argv = (char **) calloc(a_len + 2, sizeof(char *));
      char **envp = (char **) calloc(e_len + 1, sizeof(char *));

      argv[0] = cmd;

      for(i=1;isNonEmptyList(ags);i++,ags=ListTail(ags)){
        objPo ag = ListHead(ags);
        char Abuff[1024];
    
        if(!isListOfChars(ag))
          return liberror("shell",3,"2nd argument should be a list of strings",einval);
        else
          argv[i] = strdup(Cstring(ag,Abuff,NumberOf(Abuff)));
      }

      argv[i] = NULL;

      for(i=0;isNonEmptyList(envs);i++,envs=ListTail(envs)){
        objPo env = ListHead(envs);
        char Ebuff[1024];
    
        if(!isListOfChars(env))
          return liberror("shell",3,"2nd argument should be a list of strings",einval);
        else
          envp[i] = strdup(Cstring(env,Ebuff,NumberOf(Ebuff)));
      }

      envp[i] = NULL;

      if ((pid=fork()) == 0) {
        /* child process, terminating after execve */
        execve(cmd, argv, envp);
        /* abnormal termination -- should never get here */
        _exit(127);
      }
      else{
        /* parent process (agent) */
        for(i=1;argv[i]!=NULL;i++)	/* argv[0] is a local string */
          free(argv[i]);

        for(i=0;envp[i]!=NULL;i++)
          free(envp[i]);

        free(argv);
        free(envp);

        ps_set_client(p,(void*)pid);
        return attachProcessToShell(pid,p);
      }
    }
  }
}

/*
 * exec(path,argv,envp)
 * 
 * invoke a Unix command. the current process is forked and execve is called,
 * with the argument and environment lists passed as tuples of symbols to the
 * function.
 * 
 * If the fork fails, a April error with unix code errno is triggered. If the
 * execve fails, the child process exits with 127 as a return code.
 * The PID of the forked process is returned
 */

retCode m_exec(processpo p,objPo *args)
{
  objPo pth = args[2];
  objPo ags = args[1];
  objPo envs = args[0];

  char cmdBuff[MAX_SYMB_LEN];
  char *cmd = Cstring(pth,cmdBuff,NumberOf(cmdBuff));
  
  if(!executableFile(cmd))
    return liberror("exec",3,"non-executable file",eprivileged);
  else if(!IsList(ags))
    return liberror("exec",3,"2nd argument should be a list of strings",einval);
  else if(!IsList(envs))
    return liberror("exec",3,"3rd argument should be a list of strings",einval);
  else{
    int i,pid;
    WORD32 a_len = ListLen(ags);
    WORD32 e_len = ListLen(envs);
    char **argv = (char **) calloc(a_len + 2, sizeof(char *));
    char **envp = (char **) calloc(e_len + 1, sizeof(char *));

    argv[0] = cmd;

    for(i=1;isNonEmptyList(ags);i++,ags=ListTail(ags)){
      objPo ag = ListHead(ags);
      char Abuff[MAX_SYMB_LEN];
    
      if(!isListOfChars(ag))
	return liberror("exec",3,"2nd argument should be a list of strings",
			einval);
      else
	argv[i] = strdup(Cstring(ag,Abuff,NumberOf(Abuff)));
    }

    argv[i] = NULL;

    for(i=0;isNonEmptyList(envs);i++,envs=ListTail(envs)){
      objPo env = ListHead(envs);
      char Ebuff[MAX_SYMB_LEN];
    
      if(!isListOfChars(env))
	return liberror("exec",3,"2nd argument should be a list of strings",
			einval);
      else
	envp[i] = strdup(Cstring(env,Ebuff,NumberOf(Ebuff)));
    }

    envp[i] = NULL;

    if((pid=fork())==0){
      /* child process, terminating after execve */
      execve(cmd, argv, envp);
      /* abnormal termination -- should never get here */
      _exit(127);
    }
    else
      args[2] = allocateInteger(pid); /* pid of the forked process */
    return Ok;
  }
}

retCode m_waitpid(processpo p,objPo *args)
{
  int pid = IntVal(args[0]);
  int status;

  while(waitpid(pid,&status,0) == -1){
    if(errno!=EINTR)
      return liberror("waitpid",1,"problem with waitpid",efail);
  }

  if(WIFEXITED(status)){
    args[0] = allocateInteger(WEXITSTATUS(status)); /* result of the command */
    return Ok;
  }
  else
    return Switch;
}


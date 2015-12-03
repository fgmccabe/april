/*
  pipe process handling for April run-time system
  (c) 1994-2002 Imperial College and F.G. McCabe


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
#include "april.h"
#include "process.h"
#include "astring.h"
#include "fileio.h"
#include <signal.h>
#include <string.h>

/*
 * popen(command)
 * opens a pipe between the caller and a process created with the unix
 * "command", in write or read mode.
 */

retCode m_popen(processpo p,objPo *args)
{
  objPo cmd = args[2];
  objPo ags = args[1];
  objPo env = args[0];
  
  if(!p->priveleged)
    return liberror("__popen", 3, "permission denied",eprivileged);
  else{
    ioPo inPipe,outPipe,errPipe;
    char *argv[256];
    int argc = 0;
    char *envv[256];
    int envc = 0;
    char Buff[MAX_SYMB_LEN];

    argv[argc++] = strdup(Cstring(cmd,Buff,NumberOf(Buff)));

    while(isNonEmptyList(ags)){
      char *arg = Cstring(ListHead(ags),Buff,NumberOf(Buff));

      argv[argc++]=strdup(arg); /* set up the argument vector */
      ags = ListTail(ags);
    }

    argv[argc]=NULL;		/* Null terminated list of strings */

    while(isNonEmptyList(env)){
      char *arg = Cstring(ListHead(env),Buff,NumberOf(Buff));

      envv[envc++]=strdup(arg); /* set up the argument vector */
      env = ListTail(env);
    }

    envv[envc]=NULL;		/* Null terminated list of strings */

    signal(SIGTTOU,SIG_IGN);	/* We ignore this -- for the moment */

    switch(openPipe(argv[0],argv,envv,&inPipe,&outPipe,&errPipe,utf8Encoding)){
    case Error:{
      int i=0;
      for(i=0;i<argc;i++)
	free(argv[i]);		/* release the strings we allocated */
      for(i=0;i<envc;i++)
	free(envv[i]);		/* release the strings we allocated */

      return liberror("__popen",3,"problem in opening pipe",efail);
    }
    case Fail:{
      int i=0;
      for(i=0;i<argc;i++)
	free(argv[i]);		/* release the strings we allocated */

      for(i=0;i<envc;i++)
	free(envv[i]);		/* release the strings we allocated */

      return liberror("__popen",3,"file not found or permission denied",eprivileged);
    }
    case Ok:{
      objPo in = allocOpaqueFilePtr(inPipe);
      void *root = gcAddRoot(&in);
      objPo out = allocOpaqueFilePtr(outPipe);
      objPo errIn;
      
      gcAddRoot(&out);
      
      errIn = allocOpaqueFilePtr(errPipe);
      
      gcAddRoot(&errIn);

      args[2]=allocateTuple(3);
      updateTuple(args[2],0,in);
      updateTuple(args[2],1,out);
      updateTuple(args[2],2,errIn);

      configureIo(O_FILE(in),turnOffBlocking);
      configureIo(O_FILE(in),enableAsynch);
      configureIo(O_FILE(out),turnOffBlocking);
      configureIo(O_FILE(out),enableAsynch);
      configureIo(O_FILE(errIn),turnOffBlocking);
      configureIo(O_FILE(errIn),enableAsynch);

      gcRemoveRoot(root);
      return Ok;
    }
    default:{
      int i=0;
      for(i=0;i<argc;i++)
	free(argv[i]);		/* release the strings we allocated */

      for(i=0;i<envc;i++)
	free(envv[i]);		/* release the strings we allocated */

      return liberror("__popen",3,"problem",efail);
    }
    }
  }
}




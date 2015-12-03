/* 
  Main program for the April run-time system
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
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include "april.h"		/* Main header file */
#include "dict.h"
#include "symbols.h"
#include "msg.h"
#include "clock.h"
#include "arith.h"
#include "fileio.h"

#include "process.h"
#include "debug.h"

logical debugging = False;	/* instruction tracing option */
logical interactive = False;	/* Whether it should be interactive */
logical SymbolDebug = False;	/* symbolic debugging generation */
logical TCPDebug = False;	/* true if remote debugging */
logical verifyCode = True;      // True if we verify loaded code

logical normalIO = True;	/* False if pause for I/O from keyboard */

static uniChar *defaultSysPath(void);
static uniChar *currentWD(void);

uniChar *aprilSysPath=NULL;     /* Search path for loading system files */

static uniChar *Invocation=NULL;	/* name of this invocation */
static uniChar *thName=NULL;

static uniChar *bootfile = NULL;          /*  bootstrap file */

#include "version.h"		/* Version ID for the evaluator */
char copyRight[]="(c) 1994-2003 Imperial College and F.G.McCabe\n"
                 "All rights reserved";

static int initHeapSize = 100*1024;

int getOptions(int argc, char **argv)
{
  int opt;
  extern char *optarg;
  extern int optind;

  while((opt=getopt(argc,argv, GNU_GETOPT_NOPERMUTE "I:i:d:b:g:vh:L:V"))>=0){
    switch(opt){
    case 'd':{			/* turn on various debugging options */
      char *c = optarg;

      while(*c){
	switch(*c++){
	case 'e':		/* Escape call tracing */
#ifdef EXECTRACE
	  traceEscapes = True;
	  continue;
#else
	  logMsg(logFile,"Escape tracing not enabled\n");
	  return -1;
#endif

	case 'd':		/* single step instruction tracing */
#ifdef EXECTRACE
	  debugging = True;
	  interactive = True;
	  continue;
#else
	  logMsg(logFile,"Instruction-level debugging not enabled\n");
	  return -1;
#endif

	case 't':		/* turn on instruction tracing */
#ifdef EXECTRACE
	  debugging = True;
	  interactive = False;
	  continue;
#else
	  logMsg(logFile,"Instruction-level debugging not enabled\n");
	  return -1;
#endif

	case 'v':		/* turn on verify tracing */
#ifdef VERIFYTRACE
	  traceVerify=True;
	  continue;
#else
	  logMsg(logFile,"code verification not enabled\n");
	  return -1;
#endif

	case 's':		/* trace process suspensions */
#ifdef PROCTRACE
	  if(traceSuspend)
	    stressSuspend=True;
	  else
	    traceSuspend = True;
	  continue;
#else
	  logMsg(logFile,"process tracing not enabled");
	  return -1;
#endif

	case 'c':		/* trace message passing */
#ifdef MSGTRACE
	  traceMessage = True;
	  continue;
#else
	  logMsg(logFile,"message tracing not enabled");
	  return -1;
#endif

	case 'm':		/* trace memory allocations  */
#ifdef MEMTRACE
	  if(traceMemory)
	    stressMemory=True;
	  else
	    traceMemory = True;
	  continue;
#else
	  logMsg(logFile,"memory tracing not enabled");
	  return -1;
#endif 

	case 'i':		/* Dont set non-blocking I/O mode in stdin*/
	  normalIO = False;
	  continue;

	case 'r':		/* trace resource management  */
#ifdef RESTRACE
	  traceResource = True;
	  continue;
#else
	  logMsg(logFile,"resource tracing not enabled");
	  return -1;
#endif 

	case 'o':
#ifdef CLOCKTRACE
	  traceClock = True;	/* trace clocks and timers */
	  continue;
#else
	  logMsg(logFile,"clock tracing not enabled");
	  return -1;
#endif

	case 'G':		/* Local symbolic debugging */
	  SymbolDebug = True;
	  interactive = False;
	  TCPDebug=False;
	  break;

	case 'g':		/* Local symbolic debugging */
	  SymbolDebug = True;
	  interactive = True;
	  TCPDebug=False;
	  break;

	case '*':		/* trace everything */
#ifdef ALLTRACE
	  traceEscapes = True;
	  debugging = True;
	  interactive = True;
	  traceVerify=True;
	  traceMessage = True;
	  normalIO = False;
	  traceResource = True;
	  traceClock = True;	/* trace clocks and timers */
#else
	  logMsg(logFile,"tracing not enabled\n");
	  return -1;
#endif
	}
      }
      break;
    }

    case 'g':{
      WORD32 len = strlen(optarg);
      uniChar host[len+1];
      WORD32 port;
      char *ptr = strchr(optarg,':');
      
      SymbolDebug = True;	/* turn on symbolic debugging generation */
      TCPDebug = True;
      interactive = True;       // Initially its also interactive
      
      if(ptr!=NULL){
        *ptr = '\0';
        _uni((unsigned char *)optarg,host,len+1);
        *ptr = ':';
        port = atoi(ptr+1);     // Collect the port number
      }
      else{
        _uni((unsigned char *)optarg,host,len+1);
        port = 9999;            // Default port for the debugging connection
      }
      
      initDebugger(host,port);
      break;
    }
    
    case 'I':{			/* set the default agent name */
      WORD32 len = strlen(optarg);
      uniChar buff[len+1];
      
      _uni((unsigned char *)optarg,buff,len+1);
      
      Invocation = uniDuplicate(buff);
      break;
    }

    case 'i':{			/* set the default thread name */
      WORD32 len = strlen(optarg);
      uniChar buff[len+1];
      
      _uni((unsigned char *)optarg,buff,len+1);
      
      thName = uniDuplicate(buff);
      break;
    }

    case 'b':{			/* non-standard boot file */
      WORD32 len = strlen(optarg);
      uniChar buff[len];
      uniChar *pt;
      
      _uni((unsigned char *)optarg,buff,len);
      
      if(bootfile!=NULL)
        free(bootfile);
      bootfile = uniDuplicate(buff);
      if((pt=uniSearch(buff,len,DIR_CHR))!=NULL){
        *pt = 0;
        free(aprilSysPath);
        aprilSysPath = uniDuplicate(buff);
      }
      
      break;
    }

    case 'L':{
      WORD32 len = strlen(optarg)+1;
      uniChar fname[len];
      
      _uni((unsigned char *)optarg,fname,len);
      
      if(initLogfile(fname)!=Ok){
	logMsg(logFile,"log file %s not found",optarg);
	return -1;
      }
      break;
    }

    case 'v':			/* Display version ID */
      outMsg(logFile,"%s",version);
      break;
      
    case 'V':                   // Flip the verification toggle
      verifyCode = !verifyCode;
      break;

    case 'h':			/* set up initial heap size */
      initHeapSize = atoi(optarg)*1024;
      break;

    default:
      return -1;
    }
  }
  return optind;
}

/*
 * April evaluator main program
 */
int main(int argc, char **argv)
{
  int narg;

  {
    uniChar fname[]={'-',0};
    initLogfile(fname);
  }
  
  aprilSysPath = defaultSysPath();
  
  {
    uniChar bt[] = {'s','y','s',':','b','o','o','t','.','a','a','m',0};
    
    bootfile = uniDuplicate(bt);
  }

  if((narg=getOptions(argc,argv))<0){
    outMsg(logFile,"usage: %s [-I invocation] [-i thName] [-L dir]*"
	   " [-g] [-D debugagent] [-v] [-h sizeK]"
	   " args ...\n",argv[0]);
    exit(1);
  }

  initFiles();
  
  /* IMPORTANT -- Keep the order of these set up calls */
  initHandles();		/* initialize table of handles */
  initHeap(initHeapSize);	/* start up the heap */
  init_dict();			/* Start up the dictionaries */
  install_escapes();		/* Initialize the escape table */
  init_proc_tbl(256);		/* Initialize the process pool */
  init_args(argv,argc,narg);	/* Initialize the argument tuple */
  init_time();			/* Initialize time stuff */
  init_msgs();			/* Initialize message tables */
  

  if(Invocation==NULL) {        /* compute a low-level name */
    uniChar mname[MAX_SYMB_LEN];
    uniChar *pt;
    
    strMsg(mname,NumberOf(mname),"%d-%U",getpid(),machineName());
    
    if((pt=uniSearch(mname,NumberOf(mname),'.'))!=NULL)
      *pt = '\0';
    
    Invocation = uniDuplicate(mname);
  }
  
  if(thName==NULL){
    uniChar n[] = {'<','m','a','i','n','>',0};
    
    thName = uniDuplicate(n);
  }

  setupSignals();
  
  if(normalIO && !interactive)
    setup_stdin();		/* Set standard input to be non-blocking */

  bootstrap(thName,Invocation,currentWD(),bootfile);

  april_exit(EXIT_SUCCEED);	/* exit the april system cleanly */
  return 0;
}


static uniChar *defaultSysPath(void)
{
  char *path = getenv("APRIL_DIR");

  if(path==NULL)
      path = "file:///" APRILDIR;         /* Default installation path */
      
  {
    WORD32 len = strlen(path)+10;
    uniChar buff[len];
    
    strMsg(buff,len,"%s/april",path);

    return uniDuplicate(buff);
  }
}

static uniChar *currentWD(void)
{
  char cbuff[MAXPATHLEN];
  char *cwd = getcwd(cbuff,NumberOf(cbuff)); /* compute current starting directory */

  if(cwd==NULL){
    syserr("cant determine current directory");
    return NULL;
  }
  else{
    WORD32 len = strlen(cwd)+10;
    uniChar buff[len];
    
    strMsg(buff,len,"file:///%s/",cwd);
    return uniDuplicate(buff);
  }
}


/*
  Unix Signal handling for the April run-time engine
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
#include <signal.h>
#include "april.h"		/* Main header file */
#include "symbols.h"
#include "process.h"
#include "clock.h"
#include "signals.h"

/* 
 * Signal handling functions
 */

static void sig_fatal(int sig)
{
  outMsg(logFile, "bus error or segmentation fault\n");
  april_exit(EXIT_FAIL);
}

/* Handle the suspension of April reasonably ... */
static void sig_suspend(int sig)
{
  outMsg(logFile,"Suspending\n");
  if(!interactive){
    reset_stdin();		/* Reset the standard input channel */
    raise(SIGSTOP);             /* Actually suspend */
    setup_stdin();              /* Put it back */
  }
  else
    raise(SIGSTOP);             /* Actually suspend */
}

static void sig_debug(int sig)
{
  outMsg(logFile, "Turning on symbolic debug tracing\n");
  SymbolDebug = True;
}

static void sig_child(int sig)
{
  childDone = True;                    /* The real handling is done elsewhere */
}

static void interruptMe(int ignored);

void setupSignals(void)
{
  signal(SIGBUS, sig_fatal);
  signal(SIGTSTP, sig_suspend);	/* When the user suspends an April program */
  signal(SIGPIPE, SIG_IGN);	/* We not want to be interrupted by this */
  signal(SIGSEGV, sig_fatal);
  signal(SIGFPE, sig_fatal);
  signal(SIGHUP, sig_debug);
  signal(SIGCHLD, sig_child);
  signal(SIGINT, interruptMe);
}

sigset_t stopInterrupts(void)	/* stop control-C from generating a signal */
{
  sigset_t set;
  sigset_t blocked;

  sigemptyset(&set);
  sigaddset(&set,SIGINT);	/* The interrupt signal */
  sigaddset(&set,SIGCHLD);	/* The wait for child signal */

  sigprocmask(SIG_BLOCK,&set,&blocked);
  return blocked;
}

void startInterrupts(sigset_t blocked)	/* enable control-C again */
{
  sigset_t mask;
  sigprocmask(SIG_SETMASK,&blocked,NULL);
  sigprocmask(SIG_SETMASK,NULL,&mask);
}

/* 
 * Interrupt handling -- on a control^C we send a message to the monitor process
 */

/* Warning --- important that SIGINT is blocked during this handler */

static void interruptMe(int sig) /* This one is invoked when user presses ^C */
{
  sigset_t blocked = stopInterrupts();  /* prevent interrupts now */

  wakeywakey = True;		/* Signal scheduler */

#ifdef CLOCKTRACE
  if(traceSuspend){
    outMsg(logFile,"control^C interrupt\n");
    flushFile(logFile);
  }
#endif

#if 0
  if(monitorH!=NULL)
    LocalMsg(monitorH,kinterruptmsg,monitorH->handle,emptyList);
  else
#endif
    april_exit(EXIT_FAIL);	/* We just abort everything */
  startInterrupts(blocked);
}


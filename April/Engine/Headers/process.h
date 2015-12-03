/*
  A process record structure
*/

#ifndef _PROCESS_H_
#define _PROCESS_H_

#ifndef DEFSTACK
#define DEFSTACK 128		/* Default size of a process stack */
#endif

#define MAXERROR 32		/* Maximum depth of error blocks */

typedef struct msg_rec *msgpo; /* pointer to a message record */

typedef enum {quiescent, runnable, wait_io, wait_msg, wait_timer, wait_lock, wait_child, dead
} process_state;

typedef struct process_{
  objPo *stack;			/* On board stack */
  objPo *sp;			/* Top of stack pointer */
  objPo e;			/* Pointer to current environment */
  objPo *sb;			/* Base of stack pointer */
  objPo *fp;			/* Frame pointer */
  insPo pc;			/* Program counter */
  objPo *er;			/* Error recovery frame pointer */
  objPo errval;			/* Error value */
  msgpo mfront;			/* front of message queue */
  msgpo mback;			/* back of message queue */
  long sequence;		/* sequence number of last message received */
  objPo handle;			/* The handle of this process */
  objPo creator;		/* The handle of the creator of this process */
  objPo filer;			/* Handle of the file manager process */
  processpo mailer;		/* Mail manager process */
  processpo pnext;		/* Next in run queue */
  processpo pprev;		/* Previous in run queue */
  void *cl;			/* Client specific data */
  objPo clicks;			/* pointer to the click counter object */
  logical priveleged;		/* Is this process priveleged? */
  process_state state;		/* What is the status of this process? */
} process;

extern int LiveProcesses;	/* Number of executing processes */
extern processpo current_process;
extern processpo rootProcess;

#define popstk(sp) (*sp++)

#define PNULL	((processpo) NULL)

void ps_threadProcess(processpo P,processpo *queue,logical front);
void ps_unthreadProcess(processpo P,processpo *queue);
processpo ps_suspend(processpo p,process_state reason);
processpo ps_resume(register processpo p, register logical fr); /* resume process */
processpo ps_pause(register processpo p); /* pause - keep runnable */
processpo ps_switch(register processpo p,processpo P);
processpo ps_terminate(register processpo p); /* kill process */

integer clicksLeft(objPo c);
logical isClickCounter(objPo c);
integer ps_clicks(processpo p);
retCode ps_taxi_fare(processpo p); /* compute current amount to decrement clicks */
integer taxiFlag(void);
retCode taxiFare(processpo p);
extern objPo rootClicks;	/* an infinite number of clicks */

void *ps_client(processpo p);	/* Get the process's client information */
void ps_set_client(processpo p,void *cl);

process_state ps_state(processpo p);

retCode make_error_message(char *errmsg,objPo data,processpo p);
processpo runerr(processpo); /* non-fatal error */

extern logical childDone;               /* set when a child process sends a signal */
void checkOutShells(void);              /* pick up any child processes */

int grow_stack(processpo p, int factor); /* Grow a stack by a given size */
processpo add_to_run_q(processpo p,logical fr); /* add a process to the run Q */
processpo remove_from_run_q(register processpo p,process_state reason);
void MonitorDie(objPo H);	/* send termination message to monitor */

void init_proc_tbl(int initial); /* initiali process table */
void bootstrap(uniChar *tgt,uniChar *name,uniChar *cwd,uniChar *bootfile);
void displayRunQ(void);
void displayProcesses(void);
void displayProcess(processpo p);
void stackTrace(processpo p);

void scanProcesses(objPo base,objPo limit);
void scanProcess(processpo p);
void resetProcess(processpo p);
void resetProcesses(void);
void markProcesses(void);
void markProcess(processpo p);
void adjustProcesses(void);
void adjustProcess(processpo p);

/* Escape functions definitions */
retCode m_fork(processpo p,objPo *args);
retCode m_fork2(processpo p,objPo *args);
retCode m_self(processpo p,objPo *args);
retCode m_current_process(processpo p,objPo *args);
retCode m_creator(processpo p,objPo *args);
retCode m_host(processpo p,objPo *args);
retCode m_commserver(processpo p,objPo *args);
retCode m_set_commserver(processpo p,objPo *args);
retCode m_state(processpo p,objPo *args);
retCode m_done(processpo p,objPo *args);
retCode m_messages(processpo p,objPo *args);
retCode m_monitor(processpo p,objPo *args);
retCode m_set_monitor(processpo p,objPo *args); /* set name of the monitor */
retCode m_filer(processpo p,objPo *args);
retCode m_file_manager(processpo p,objPo *args); /* set name of the filer */
retCode m_kill(processpo p,objPo *args);
retCode m_wait_msg(processpo p,objPo *args);

retCode m_credit_clicks(processpo p,objPo *args);
retCode m_clicks(processpo p,objPo *args);
retCode m_new_clicks(processpo p,objPo *args);
retCode m_use_clicks(processpo p,objPo *args);
objPo newClicks(integer cnt);

#endif

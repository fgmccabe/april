/*
  Clock and interval timer management for the April system 
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
#include <signal.h>
#include <time.h>
#include <math.h>
#include "april.h"
#include "process.h"
#include "clock.h"
#include "symbols.h"

#define MAXTIMEOUT 200		/* initial number of time records */

typedef struct time_rec {
  processpo ps;			/* process to wake up */
  struct timeval tval;		/* what time ? */
  timeFun onWakeup;             // What to do when we wake up
  void *cl;                     // client data
  struct time_rec *next;	/* chain in linked list */
} *timepo;


static inline Number numval(objPo c)
{
  if(IsInteger(c))
    return (Number)IntVal(c);
  else
    return FloatVal(c);
}

#ifdef CLOCKTRACE
logical traceClock = False;	/* Clock and Timer tracing  */
#endif

timepo time_q = NULL;		/* the timeout queue                */

poolPo time_pool;		/* pool of time records */

WORD32 timezone_offset;		/* offset in seconds from GMT       */

logical wakeywakey = False;

struct timeval initial_time;

static void sig_alarm(int);

void init_time(void)
{
  time_t tloc;
  struct tm *tmptr;

  gettimeofday(&initial_time, NULL);

  tloc = initial_time.tv_sec;
  tmptr = localtime(&tloc);
  tmptr->tm_hour = 0;
  tmptr->tm_min = 0;
  tmptr->tm_sec = 0;

  time_pool = newPool(sizeof(struct time_rec),MAXTIMEOUT);

  timezone_offset = mktime(tmptr) - (tloc - tloc % SECSINDAY);

  signal(SIGALRM, sig_alarm);	/* install signal handler for timer */

  startTicks(SCHEDULETICKS);	/* set up the cpu clicker */
}

/*
  Compare two times incorporating a delta 
  returns true if t1+delta usecs < t2
*/

static logical before_time(struct timeval *t1, struct timeval *t2, int delta)
{
  struct timeval t = *t1;

  t.tv_usec +=delta;
  if(t.tv_usec<0){
    t.tv_usec+=1000000;		/* adjust usecs */
    t.tv_sec--;
  }
  else if(t.tv_usec>1000000){
    t.tv_usec-=1000000;
    t.tv_sec++;
  }

  if(t.tv_sec<t2->tv_sec)	/* compare times */
    return True;
  else if(t.tv_sec>t2->tv_sec || t.tv_usec>=t2->tv_usec)
    return False;
  else
    return True;
}

/*
 * timeout signal handler
 * real-time signal handler ....
 */
void sig_alarm(int sig)		/* UNIX version of timer handler */
{
  wakeywakey = True;		/* Let the scheduler know about alarm call */

  signal(SIGALRM, sig_alarm);	/* reinstall signal handler */
}

static void timerWakeup(int ignored);

void setupTicks(WORD32 gap)	/* set up the tick timer every gap millisecs*/
{
  struct sigaction act;
  struct itimerval period;
  static WORD32 lastGap = 0;

  act.sa_handler = timerWakeup;
  act.sa_flags = 0;
  
  sigemptyset(&act.sa_mask);

  sigaction(SIGVTALRM,&act,NULL);

  if(gap<0)
    gap = lastGap;

  lastGap = gap;
  
  if(gap<SCHEDULETICKS/10)
    gap = lastGap = SCHEDULETICKS;

  period.it_value.tv_sec = gap/1000;
  period.it_value.tv_usec = gap*1000;
  period.it_interval = period.it_value;

  setitimer(ITIMER_VIRTUAL,&period,NULL);
}

static void timerWakeup(int sig) /* This one is invoked for cpu time */
{
  wakeywakey = True;		/* Signal scheduler */

#ifdef CLOCKTRACE
  if(traceClock){
    outMsg(logFile,"tickle me\n");
    flushFile(logFile);
  }
#endif

  setupTicks(-1);		/* reset the timer for the next one */
}

static sigset_t blocked;

void stopTicks(void)		/* stop the cpu timer from delivering any signals */
{
  sigset_t set;

  sigemptyset(&set);
  sigaddset(&set,SIGVTALRM);	/* The CPU timer */

  sigprocmask(SIG_BLOCK,&set,&blocked);
}

void startTicks(WORD32 gap)	/* enable the virtual timer again */
{
  setupTicks(gap);		/* restart the timer */
  sigprocmask(SIG_SETMASK,&blocked,NULL);
}
  
/*
 * reset the interval timer for the new period
 */
void reset_timer(void)
{
  while(time_q!=NULL) {
    struct timeval now;

    gettimeofday(&now, NULL);	/* More accurate but slower in the loop */

    /* Look for timed out entries? (0.01secs delta)*/
    if(before_time(&time_q->tval,&now,1000)){ 
      timepo t = time_q;

      wakeywakey = False;
      time_q = time_q->next;

#ifdef CLOCKTRACE
      if(traceClock)
	outMsg(logFile,"Wakeup [%#w] (timed out)\n",t->ps->handle);
#endif

      t->onWakeup(t->ps,t->cl);
      freePool(time_pool,(void*)t);
    }
    else{
      struct itimerval period;

      /* calculate interval between now and the timeout */
      period.it_value.tv_sec = time_q->tval.tv_sec - now.tv_sec;
      period.it_value.tv_usec = time_q->tval.tv_usec - now.tv_usec;
      if (period.it_value.tv_usec < 0) {	/* -ve microseconds */
	period.it_value.tv_usec += 1000000;
	period.it_value.tv_sec--;
      }
      period.it_interval.tv_sec = period.it_interval.tv_usec = 0l;

      if(period.it_value.tv_sec>=0 && period.it_value.tv_usec>0){
  	setitimer(ITIMER_REAL, &period, NULL);
	
#ifdef CLOCKTRACE
	if (traceClock)
	  outMsg(logFile, "timer set for %f\n",
		  period.it_value.tv_sec+period.it_value.tv_usec/1e6);
#endif
	break;
      }
    }
  }
}

struct timeval *nextTimeOut(void)
{
  if(time_q==NULL)
    return NULL;
  else{
    static struct timeval period;
    struct timeval now;

    gettimeofday(&now, NULL);
    /* calculate interval between now and the timeout */
    /* allow extra tenth of second so that interval
       timer times out before select() does */
    period.tv_sec = time_q->tval.tv_sec - now.tv_sec;
    period.tv_usec = time_q->tval.tv_usec - now.tv_usec + 10000;
    if(period.tv_usec<0){	/* -ve microseconds */
      period.tv_usec += 1000000;
	period.tv_sec--;
    }
    if(period.tv_usec>1000000){ /* too many microseconds */
      period.tv_usec -= 1000000;
      period.tv_sec++;
    }

    if(period.tv_sec<0){
      period.tv_sec = 0;
      period.tv_usec = 0;
    }
    return &period;
  }
}

/*
 * add a timeout to the queue 
 * this must be as fast as possible to avoid timer races
 */

static void add_to_time_q(timepo t)
{
  timepo tp = time_q;
  timepo *prev = &time_q;
  timepo tr;
  timepo *trp;

  register WORD32 t_secs = t->tval.tv_sec; /* Optimizing the search loop */
  register WORD32 t_usecs = t->tval.tv_usec;
  processpo p = t->ps;

  /* find place in queue to insert */
  while(tp!=NULL && (tp->tval.tv_sec<t_secs || 
		     (tp->tval.tv_sec==t_secs &&tp->tval.tv_usec<t_usecs)))
    if(p!=tp->ps){		/* skip timer entry for different process */
      prev = &tp->next;
      tp = *prev;
    }
    else{			/* There is already a timer entry for p in Q */
      *prev = tp->next;		/* delete old timeout record */
      freePool(time_pool, (void*)tp);
      tp = *prev;
    }

  tr = tp;			/* look for redundant entries for the same process*/
  trp = prev;

  while(tr!=NULL){
    if(tr->ps==t->ps){		/* a redundant entry later in the Q */
      *trp = tr->next;		/* removing redundant entry */
      freePool(time_pool,(void*)tr);
      tr = *trp;
    }
    else{
      trp = &tr->next;
      tr = *trp;
    }
  }

  /* we are either at the end of the Q or somewhere where we can insert 
     a timeQ entry */
  t->next = *prev;		/* link into the Q */
  *prev = t;

  if(prev==&time_q)		/* first entry? */
    reset_timer();
}

/*
 * set an alarm to go after a specified delay
 */
retCode set_timer(processpo p, Number time,timeFun onWakeup,void *cl)
{
  if(time<=0.001)
    return Ok;		/* No delay of less than 1 millisecond */
  else{
    timepo t=(timepo)allocPool(time_pool);
    struct timeval now;

    gettimeofday(&now, NULL);

    if(t==NULL)
      return Space;		/* insufficient space */
  
    t->ps = p;
    t->onWakeup = onWakeup;
    t->cl = cl;
    t->tval.tv_sec = time + now.tv_sec;
    t->tval.tv_usec = (time - (WORD32)time) * 1000000 + now.tv_usec;
    if(t->tval.tv_usec>1000000){
      t->tval.tv_usec-=1000000;
      t->tval.tv_sec++;
    }
    t->next = NULL;

#ifdef CLOCKTRACE
    if(traceClock)
      outMsg(logFile,"Set timeout in %f secs\n",time);
#endif

    add_to_time_q(t);
    return Suspend;
  }
}

/*
 * set an alarm to go at specified time -- returns true if alarm set
 */
retCode set_alarm(processpo p,Number time,timeFun onWakeup,void *cl)
{
  struct timeval now,when;

  gettimeofday(&now, NULL);
  when.tv_sec=time;
  when.tv_usec = (time - (integer)time) * 1000000;

#ifdef CLOCKTRACE
  if(traceClock)
    outMsg(logFile,"Set alarm at %f in %f secs\n",time,
	   when.tv_sec-now.tv_sec+(when.tv_usec-now.tv_usec)/1e6);
#endif

  if(when.tv_sec<now.tv_sec ||
     (when.tv_sec==now.tv_sec && when.tv_usec<now.tv_usec)) /* already gone */
    return Ok;			/* timeout already fired */
  else{
    timepo t=(timepo)allocPool(time_pool);

    if(t==NULL)
      return Space;		/* insufficient space */
  
    t->ps = p;
    t->onWakeup = onWakeup;
    t->cl = cl;
    t->tval = when;
    t->next = NULL;
  
    add_to_time_q(t);
    return Suspend;		/* we set the alarm */
  }
}

static void wakeMeUp(processpo p,void *cl)
{
  add_to_run_q(p,True);
}

retCode m_delay(processpo p, objPo *args)
{
  Number f;
  register objPo e1 = args[0];

  if(IsInteger(e1))
    f=(Number)IntVal(e1);
  else
    f=FloatVal(e1);

  switch(set_timer(p,f,wakeMeUp,NULL)){
  case Suspend:
    ps_suspend(p,wait_timer);
    return Suspend;		/* not to retry delay */
  case Ok:			/* Invalid or too small an interval */
    return Ok;
  case Space:
    return liberror("delay",1,"out of heap space",esystem);
  default:
    return liberror("delay",1,"problem",esystem);
  }
}

retCode m_sleep(processpo p, objPo *args)
{
  Number f;
  register objPo e1 = args[0];

  if(IsInteger(e1))
    f=(Number)IntVal(e1);
  else
    f=FloatVal(e1);

  switch(set_alarm(p,f,wakeMeUp,NULL)){
  case Ok:			/* already fired */
    return Ok;
  case Suspend:
    p->pc--;
    ps_suspend(p,wait_timer);
    return Suspend;		/* not to re-try the sleep */
  case Space:
    return liberror("sleep",1,"out of heap space",esystem);
  default:
    return liberror("sleep",1,"problem",esystem);
  }
}

/* remove a process which has died from the event queue */
void flush_from_time_q(processpo p)
{
  timepo tp = time_q;
  timepo prev = NULL;

  while(tp!=NULL){
    if(tp->ps==p){		/* We have found a time entry for the process*/
      timepo next = tp->next;
      if(prev==NULL)
	time_q = next;
      else
	prev->next = next;
      freePool(time_pool,(void*)tp);
      tp = next;
    }
    else{
      prev = tp;
      tp = tp->next;
    }
  }
}

/*
 *  returns the current ticks
 */

integer get_ticks(void)
{
  struct timeval t;

  gettimeofday(&t, NULL);

  return ((t.tv_sec-initial_time.tv_sec)*1000 +
	  (t.tv_usec-initial_time.tv_usec)/1000)%MSECSWRAP;
}


/*
 *  returns the current time
 */
Number get_time(void)
{
  struct timeval t;

  gettimeofday(&t, NULL);

  return t.tv_sec+t.tv_usec/1.0e6;
}

/*
 *  returns the time at midnight this morning
 */
WORD32 get_date(void)
{
  struct timeval t;

  gettimeofday(&t, NULL);

  t.tv_sec -= t.tv_sec % SECSINDAY;
  return (t.tv_sec + timezone_offset);
}

void display_time_q(void)
{
  struct timeval t;
  struct timeval n;
  timepo tm = time_q;

  t = initial_time;

  gettimeofday(&n, NULL);

  outMsg(logFile, "Time Q @(%8.5f)\n",
	  n.tv_sec-t.tv_sec+(n.tv_usec-t.tv_usec)/1e6);
  if(tm!=NULL){
    while(tm!=NULL){
      outMsg(logFile, "%#w(%8.5f) ", tm->ps->handle,
	     tm->tval.tv_sec-n.tv_sec+(tm->tval.tv_usec-n.tv_usec)/1e6);
      tm = tm->next;
    }
    outMsg(logFile, "\n");
  }
  flushFile(logFile);
}

/*
 * Implementation of time related escapes 
 */

retCode m_ticks(processpo p, objPo *args)
{
  args[-1]=allocateInteger(get_ticks());
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

retCode m_now(processpo p, objPo *args)
{
  args[-1]=allocateNumber(get_time());
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

retCode m_today(processpo p, objPo *args)
{
  args[-1]=allocateNumber(get_date());
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

/* Convert a time to a date record:-
   date{year month day hours mins seconds}
*/
retCode m_tval2date(processpo p, objPo *args)
{
  WORD32 ttval = (WORD32)(numval(args[0]));
  struct tm *now = localtime((time_t*)&ttval);
  objPo dta = allocateConstructor(7);
  void *root = gcAddRoot(&dta);
  objPo val = kvoid;

  gcAddRoot(&val);

  updateConsFn(dta,kdate);
  val = allocateInteger(now->tm_year+1900); /* Current year */
  updateConsEl(dta,0,val);
  val = allocateInteger(now->tm_mon+1); /* Current month */
  updateConsEl(dta,1,val);
  val = allocateInteger(now->tm_mday); /* Day of the month */
  updateConsEl(dta,2,val);
  val = allocateInteger(now->tm_wday+1); /* Day of the week */
  updateConsEl(dta,3,val);
  val = allocateInteger(now->tm_hour); /* Hour in the day */
  updateConsEl(dta,4,val);
  val = allocateInteger(now->tm_min); /* Minutes in the hour */
  updateConsEl(dta,5,val);
  val = allocateInteger(now->tm_sec); /* Seconds in the minutes */
  updateConsEl(dta,6,val);

  args[0] = dta;

  gcRemoveRoot(root);

  return Ok;
}

/* Convert a time to a gmt date record:-
 *  time=>date(year,month,day,hours,mins,seconds,weekday,yearday) 
 */
retCode m_tval2gmt(processpo p, objPo *args)
{
  WORD32 ttval = (WORD32)numval(args[0]);
  struct tm *now = gmtime((time_t*)&ttval);
  objPo dta = allocateConstructor(7);
  void *root = gcAddRoot(&dta);
  objPo val = kvoid;

  gcAddRoot(&val);

  updateConsFn(dta,kdate);
  val = allocateInteger(now->tm_year+1900); /* Current year */
  updateConsEl(dta,0,val);
  val = allocateInteger(now->tm_mon+1); /* Current month */
  updateConsEl(dta,1,val);
  val = allocateInteger(now->tm_mday); /* Day of the month */
  updateConsEl(dta,2,val);
  val = allocateInteger(now->tm_wday+1); /* Day of the week */
  updateConsEl(dta,3,val);
  val = allocateInteger(now->tm_hour); /* Hour in the day */
  updateConsEl(dta,4,val);
  val = allocateInteger(now->tm_min); /* Minutes in the hour */
  updateConsEl(dta,5,val);
  val = allocateInteger(now->tm_sec); /* Seconds in the minutes */
  updateConsEl(dta,6,val);

  args[0] = dta;

  gcRemoveRoot(root);

  return Ok;
}

/* Convert a date record to a time value
 *  date(year,month,day,hours,mins,seconds,weekday,yearday) -> time
 */
retCode m_date2tval(processpo p, objPo *args)
{
  objPo date = args[0];
  struct tm now;
  time_t when;

  if(!isConstructor(date,kdate))
    return liberror("datetotime",1,"invalid argument",einval);

  now.tm_year = NumberVal(consEl(date,0))-1900; /* Year */
  now.tm_mon = NumberVal(consEl(date,1))-1; /* Month */
  now.tm_mday = NumberVal(consEl(date,2)); /* day */
  now.tm_hour = NumberVal(consEl(date,4)); /* hour */
  now.tm_min = NumberVal(consEl(date,5)); /* min */
  now.tm_sec = NumberVal(consEl(date,6)); /* seconds */

  now.tm_isdst = -1;		/* dont know about daylight savings */

  when = mktime(&now);

  args[0]=allocateNumber(when);
  return Ok;
}

/* Convert a GMT date record to a time value
 *  date(year,month,day,hours,mins,seconds,weekday,yearday) -> time
 */
retCode m_gmt2tval(processpo p, objPo *args)
{
  objPo date = args[0];
  struct tm now;
  time_t when;

  if(!isConstructor(date,kdate))
    return liberror("gmttotime",1,"invalid argument",einval);

  now.tm_year = IntVal(consEl(date,0))-1900; /* Year */
  now.tm_mon = IntVal(consEl(date,1))-1;	/* Month */
  now.tm_mday = IntVal(consEl(date,2)); /* day */
  now.tm_hour = IntVal(consEl(date,4)); /* hour */
  now.tm_min = IntVal(consEl(date,5));	/* min */

  if(IsInteger(consEl(date,6)))
    now.tm_sec = IntVal(consEl(date,6));	/* sec */
  else
    now.tm_sec = floor(FloatVal(consEl(date,6)));
  now.tm_isdst = -1;		/* dont know about daylight savings */

  when = mktime(&now);

  args[0]=allocateNumber(when);

  return Ok;
}




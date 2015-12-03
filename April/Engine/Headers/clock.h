#ifndef _CLOCK_H_
#define _CLOCK_H_

#ifdef _WIN32			/* Windows'95 specific stuff */
#include <winsock.h>
struct  itimerval {		/* duplicate Unix declaration */
  struct  timeval it_interval;	/* timer interval */
  struct  timeval it_value;	/* current value */
};
void gettimeofday(struct timeval *t, void *ignored);
#else				/* UNIX only */
#include <sys/time.h>
#endif

void init_time(void);

typedef void (*timeFun)(processpo P,void *cl);

extern retCode set_alarm(processpo P,number tm,timeFun onWakeup,void *cl);
extern retCode set_timer(processpo P,number tm,timeFun onWakeup,void *cl);

void flush_from_time_q(processpo p);
void reset_timer(void);
struct timeval *nextTimeOut(void);
integer taxiFlag(void);
void display_time_q(void);

integer get_ticks(void);
Number get_time(void);
integer get_date(void);

extern logical wakeywakey;

/* cpu time based heart beat timer */
void setupTicks(long gap);	/* set up the tick timer every gap millisecs*/
void stopTicks(void);		/* stop the cpu timer from delivering any signals */
void startTicks(long gap);	/* enable the CPU timer */
#define SCHEDULETICKS 150	/* We re-schedule every 150 milliseconds */


#define SECSINDAY 86400		/* Number of seconds in a day */
#define MSECSWRAP 259200000L	/* Number of milliseconds in 3 days */

#define EPOCH 1041346800	/* Conversion from Unix EPOCH to April EPOCH */
				/* April EPOCH is 00:00 1/1/2003 */

/* Escape interface */
retCode m_delay(processpo p,objPo *args);
retCode m_sleep(processpo p,objPo *args);
retCode m_ticks(processpo p,objPo *args);
retCode m_now(processpo p,objPo *args);
retCode m_today(processpo p,objPo *args);
retCode m_secs(processpo p,objPo *args);
retCode m_mins(processpo p,objPo *args);
retCode m_hours(processpo p,objPo *args);
retCode m_days(processpo p,objPo *args);
retCode m_time2date(processpo p,objPo *args);
retCode m_time2gmt(processpo p,objPo *args);
retCode m_date2time(processpo p,objPo *args);

#endif

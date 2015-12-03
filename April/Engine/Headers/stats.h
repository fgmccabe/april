#ifndef _STATS_H_
#define _STATS_H_

#ifdef STATISTICS
extern logical statistics;	/* true if stats collection enabled */
void stat_ins_start(void);	/* Initialize stats collection for instructions */
void stat_ins_usage(long ins);	/* instruction usage */
void dump_stats(void);		/* Display statistics */
void dump_escape_stats(void);

#endif

typedef enum {initTime,execTime, gcTime, allocTime, escTime, endTime, noTime} statMode;
void startClock(void);
void stopClock(void);
void statTime(statMode mode);
void popTime(void);

#endif

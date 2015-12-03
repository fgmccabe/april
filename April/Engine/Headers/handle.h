#ifndef _HANDLE_H_
#define _HANDLE_H_

#include "symbols.h"

#define HANDLE_ARITY 3		/* (tgt,name,pid) */
#define TGT_OFFSET 0		/* The target offset in a handle struct */
#define NAME_OFFSET 1		/* The name offset in a handle struct */
#define PROCESS_OFFSET 2	/* The process offset in a handle struct */

void initHandles(void);
extern objPo voidhandle;

logical sameHandle(objPo h1,objPo h2);
logical sameAgent(objPo h1,objPo h2);
objPo locateHandle(uniChar *tgt,uniChar *name,processpo p);

retCode displayHandle(ioPo out,void *H,int width,int prec,logical alt);
retCode displayHdl(ioPo out,handlePo h);

void bindHandle(objPo H,processpo p);

extern inline logical IsHandle(objPo T)
{
  if(T==knullhandle)
    return True;
  else
    return isConstructor(T,khandle) && consArity(T)==HANDLE_ARITY;
}

objPo handleTgt(objPo h1);
objPo handleName(objPo h1);
processpo handleProc(objPo h1);
objPo buildHandle(processpo p,uniChar *tgt,uniChar *name);
objPo parseHandle(processpo p,uniChar *name);

void scanHandle(handlePo H);
void scanHandles(objPo base,objPo limit);
void postScanHandles(void);
void markHandle(handlePo h);
void markHandles(void);
void installHandle(handlePo H);
void adjustHandles(void);
void verifyHandles(void);

typedef void (*procProc)(processpo p,void *cl);

void processProcesses(procProc p,void *cl);

retCode m_make_handle(processpo p,objPo *args); /* construct handle from components */
retCode m_local_handle(processpo p,objPo *args);
retCode m_location(processpo p,objPo *args);
retCode m_set_location(processpo p,objPo *args);

#endif

/*
 * Private header file for the heap management of April
 */

#ifndef _GC_P_H_
#define _GC_P_H_

objPo compactHeap(objPo heap,objPo end,objPo limit);
void markRoots(void);
void adjustRoots(void);
void scanLabels(void);
void markLabels(void);
void adjustLabels(void);

/* Used for recording old->new pointers */
#ifndef CARDSHIFT
#define CARDSHIFT 5		/* 32 bits in a long */
#define CARDWIDTH  (1<<CARDSHIFT)
#define CARDMASK  (CARDWIDTH-1)
#endif

typedef unsigned long cardMap;

extern cardMap *cards;	/* this is a table of cards */
extern integer ncards;
extern cardMap masks[CARDWIDTH];

extern objPo heap;		/* The full heap */
extern objPo heapEnd;
extern integer heapSize;

#endif

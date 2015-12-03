/*
 * Memory structure handling functions for April compiler
 * (c) 1994-2002 Imperial College and F.G.McCabe
 * All rights reserved
*/

#include "config.h"
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include "compile.h"
#include "assem.h"		/* The code scanner is here */
#include "cellP.h"
#include "pool.h"

static cellpo allocated = NULL; // chain of allocated cells

logical traceMemory = False;	/* True if we want to trace our heap etc. */


#ifndef MEMPAGESIZE
#define MEMPAGESIZE 32768
#endif

#define ALIGN(x) (((x+0x7)>>3)<<3)

/* Initialize the heap spaces */
void initHeap(void)
{  
  initStack(4);			/* initialize the read stack also */
  initLineTable();
}

/* We invoke this resetting the heap */
void clearHeap(void)
{
  while(allocated!=NULL){
    cellpo pr = allocated->prev;
    
    free(allocated);
    allocated = pr;
  }
}

/* Used when no space left in heap */
void MemExit(void)
{
  syserr("compilation space exhausted");
}

/* Heap allocation functions */

codepo allocCode(void)
{
  return (codepo)malloc(codeSize());
}

floatpo allocFloat(FLOAT f)
{
  FLOAT *ff = (FLOAT*)malloc(sizeof(FLOAT));

  *ff = f;
  return ff;
}

cellpo allocChar(uniChar ch)
{
  cellpo c = allocSingle();
  
  c->tag = character;
  c->d.ch = ch;
  return c;
}

/* Allocate a string */
uniChar *allocString(const uniChar *data)
{
  return uniDuplicate((uniChar*)data);
}

uniChar *allocCString(const char *data)
{
  long len = strlen(data)+1;
  uniChar buff[len];
  
  _uni((const unsigned char*)data,buff,len);
  
  return allocString(buff);
}

/* Allocate a list pair or application pair */
cellpo allocPair(void)
{
  return (cellpo)malloc(2*sizeof(cell));
}

/* Allocate a tuple on the heap*/
cellpo allocTuple(long arity)
{
  cellpo h = (cellpo)malloc(sizeof(cell)*(arity+1));
  
  mkInt(h,arity);		/* Mark the length of the tuple */

  return h;
}

// Allocate a constructor on the heap
cellpo allocCons(long arity)
{
  cellpo h = (cellpo)malloc(sizeof(cell)*(arity+2));
  
  mkInt(h,arity);		/* Mark the length of the constructor */

  return h;
}

/* Allocate a single cell on the heap */
cellpo allocSingle(void)
{
  cellpo c = (cellpo)malloc(sizeof(cell));
  c->type = NULL;
  return c;
}

/*
 * read stack management for the April compiler 
 * (c) 1994-1998 Imperial College and F.G.McCabe
 * All rights reserved
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "compile.h"
#include "cellP.h"
#include "keywords.h"

static cellpo Stack = NULL;	/* Stack of cells */
static cellpo Base = NULL;
static cellpo StackLimit = NULL;
static long stackSize;

/* Initialise the stack spaces */
void initStack(long size)
{
  stackSize = size*1024;

  Base = (cellpo)malloc(sizeof(cell)*stackSize);

  Stack = StackLimit = &Base[stackSize];	/* reset the stack */

  /* we want to know if we could initialize */
  if(Base==NULL)
    syserr("Cant allocate heap");
}

/* Allocate a block on the stack */
static cellpo alloc_stack()
{
  if(Stack-1<=Base)
    MemExit();
  return --Stack;
}

/* Manage the stack of cells */

void resetRstack()
{
  if(Base==NULL)
    initStack(2);		/* Initialize the stack */
  Stack = &Base[stackSize];	/* reset the stack */
}

logical emptyStack(void)
{
  return Stack==&Base[stackSize];
}

int stackDepth(void)
{
  return &Base[stackSize]-Stack;
}

void resetStack(int prior)
{
  Stack = &Base[stackSize-prior];
}

cellpo topStack(void)
{
  return Stack;			/* current top of the stack */
}

#define swapcell(a,b) {cell c = (*a); (*a)=(*b); (*b) = c;}

void popcell(cellpo p)		/* get an element off the cell stack */
{
  *p = *Stack++;
}

void p_drop(void)
{
  Stack++;
}

/* reverse all the elements from base to current top */
void reverseStack(long base)
{
  cellpo S = Stack;
  cellpo B = &Base[stackSize-base];

  while(S<B){
    cell T = *--B;
    *B=*S;
    *S++=T;
  }
}

void reset_stack(cellpo point)
{
  Stack=point;
}

cellpo p_cell(cellpo p)		/* Push a cell on to stack */
{
  register cellpo c = alloc_stack();
  *c = *p;			/* store the cell */
  return c;
}

void p_swap(void)		/* Swap the top two elements on the read stack */
{
  cell x = *Stack;
  *Stack = Stack[1];
  Stack[1] = x;
}

void p_int(long i)		/* Push an integer onto stack */
{
  register cellpo c = alloc_stack();
  mkInt(c,i);
}

void p_flt(double f)		/* Push a floating point number onto stack */
{
  register cellpo c = alloc_stack();
  mkFlt(c,f);
}

void p_sym(symbpo s)		/* Push a symbol onto the cell stack */
{
  register cellpo c = alloc_stack();
  mkSymb(c,s);
}

void p_char(uniChar s)		/* Push a character onto the cell stack */
{
  register cellpo c = alloc_stack();
  mkChar(c,s);
}

void p_string(uniChar *s)	/* Push a string onto the cell stack */
{
  register cellpo c = alloc_stack();
  mkStr(c,s);
}

void join_strings()		/* concatenate top two strings */
{
  cell s1,s2;

  popcell(&s2);			/* The second string */
  popcell(&s1);			/* The first string -- first on the stack */

  if(!IsString(&s1) || !IsString(&s2))
    syserr("Problem in join_strings");
  else{
    long len = uniStrLen(strVal(&s1))+uniStrLen(strVal(&s2))+1;
    uniChar buff[len];
    
    strMsg(buff,len,"%U%U",strVal(&s1),strVal(&s2));
    
    p_string(allocString(buff));
  }
}

void prepend_char()		/* concatenate character with a string */
{
  cell s1,s2;

  popcell(&s2);			/* The second string */
  popcell(&s1);			/* The head char -- first on the stack */

  if(!isChr(&s1) || !IsString(&s2))
    syserr("Problem in prepend_char");
  else{
    long len = uniStrLen(strVal(&s2))+2;
    uniChar buff[len];
    
    strMsg(buff,len,"%C%U",CharVal(&s1),strVal(&s2));
    
    p_string(allocString(buff));
  }
}

void p_tpl(cellpo p)		/* Push a tuple onto the cell stack */
{
  register cellpo c = alloc_stack();
  mkTpl(c,p);
}

cellpo opentple()		/* Start the building of a new tuple */
{
  return Stack;			/* return the base marker */
}

void clstple(cellpo tpl)	/* Close a tuple, get elements from stack */
{
  cellpo st,tp;			/* Address of the final tuple */
  long l=tpl-Stack;		/* Length of the tuple */

  if((st=allocTuple(l))==NULL) /* grab space from the heap */
    MemExit();			/* no space */

  Stack = tpl;			/* reset the stack */

  tp = Next(st);

  while(l--)
    *tp++ = *--tpl;		/* copy a value down */

  p_tpl(st);			/* push back a tuple */
}

void p_tuple(long ar)
{
  cellpo tpl = allocTuple(ar);
  int i;

  for(i=ar;i>0;i--)
    copyCell(&tpl[i],Stack++);

  p_tpl(tpl);
}

long p_untuple(void)
{
  if(isTpl(topStack())){
    cell X;
    long ar = tplArity(topStack());
    long i;
    
    popcell(&X);
    
    for(i=0;i<ar;i++)
      p_cell(tplArg(&X,i));
    return ar;
  }
  else
    return 1;
}

void p_cons(long ar)
{
  cellpo tpl = allocCons(ar);
  int i;
  
  for(i=ar;i>=0;i--)
    copyCell(&tpl[i+1],Stack++);

  {
    register cellpo c = alloc_stack();
    mkCons(c,tpl);
  }
}

void p_null()			/* Push an empty list */
{
  register cellpo c = alloc_stack();
  mkList(c,NULL);
}

void p_list()
{
  cellpo lst = allocPair();	/* Grab a list pair */
  *(lst+1) = *Stack++;		/* Copy the tail of the list from the stack */
  *lst = *Stack;		/* Copy the head of the list */
  mkList(Stack,lst);		/* Make top of stack a list pair again */
}

void mkcall(int ar)		/* Construct an n-ary function call  */
{
  p_cons(ar);                   // The same as a constructor
}

void mkLine(int at)
{
  stLineInfo(Stack,at);
}

/* Higher-level stack construction */
cellpo pBinary(cellpo fn)
{
  cell a2 = *Stack++;
  cell a1 = *Stack++;

  p_cell(fn);
  p_cell(&a1);
  p_cell(&a2);
  p_cons(2);
  return Stack;
}

cellpo pUnary(cellpo fn)
{
  p_cell(fn);
  p_swap();
  p_cons(1);
  return Stack;
}

void dumpStack()
{
  int i = 0;
  cellpo stack = Stack;

  while(stack!=StackLimit)
    outMsg(logFile,"%d: %10w\n",i++,stack++);
  flushFile(logFile);
}


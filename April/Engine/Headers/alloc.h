/*
 * GC interface
 */
#ifndef _ALLOC_H_
#define _ALLOC_H_

extern objPo create,createSpaceEnd;
extern objPo oldSpace,oldSpaceEnd;
extern objPo heap;

retCode initHeap(integer minsize);
integer createRoom(void);
objPo scanCell(objPo f);
void *gcAddRoot(objPo *ptr);
void gcRemoveRoot(void *mk);
logical gCollect(integer amount);
logical gcTest(integer amount);
void markCell(objPo f);
objPo adjustCell(objPo scan);

#ifdef MEMTRACE
logical permitGC(logical allowed);
#endif


#include "symbols.h"
#include "gcP.h"

/* Allocation functions */

extern inline logical reserveSpace(size_t size)
{
 #ifdef MEMTRACE
  if(stressMemory)
    gCollect(size);		/* gc on every allocation */
#endif

  if(create+size>createSpaceEnd)
    return gCollect(size);		/* this aborts if there is no memory */
  else
    return True;
}

extern inline objPo allocate(size_t size,wordTag tag)
{
  objPo new;

  if(create+size>createSpaceEnd)
    gCollect(size);		/* this aborts if there is no memory */

  new = create;
  create+=size;
  new->sign = tag;

  return new;
}

extern inline objPo allocateVariable(void)
{
  variablePo new = (variablePo)allocate(VariableCellCount,variableMarker);

  new->val = (objPo)new;
  return (objPo)new;
}

extern void updateVariable(objPo var,objPo val);

extern void growRoots(void);
extern integer topRoot;
extern integer maxRoot;
extern objPo **roots;

extern inline void *gcAddRoot(objPo *ptr)
{
  if(topRoot>=maxRoot)
    growRoots();
      
  roots[topRoot]=ptr;
  return (void*)topRoot++;
}

extern inline objPo allocateTpl(integer count)
{
  return allocate(TupleCellCount(count),tupleMark(count));
}

inline extern objPo allocateTuple(integer count)
{
  tuplePo tpl = (tuplePo)allocate(TupleCellCount(count),tupleMark(count));

  if(tpl!=NULL){
    int i;
    objPo *el = tupleData((objPo)tpl);
    for(i=0;i<count;i++)
      *el++=kvoid;		/* in case of GC */
  }
  return (objPo)tpl;
}

extern inline objPo allocateInteger(integer i)
{
  integerPo new;

#ifdef MEMTRACE
  if(stressMemory)
    gCollect(IntegerCellCount);		/* gc on every allocation */
#endif

  if(create+IntegerCellCount>createSpaceEnd)
    gCollect(IntegerCellCount);	/* this aborts if there is no memory */

#if LLONG_ALIGNMENT
  if(!ALIGNED(create,integerAlignment)){
    allocateTuple(0);		/* a small pad ... */
    assert(ALIGNED(create,integerAlignment));
  }
#endif

  new = (integerPo)create;
  assert(new!=NULL);

  create+=IntegerCellCount;
  new->sign = integerMarker;
  new->i = i;
  
  return (objPo)new;
}

extern objPo allocateSymbol(uniChar *p);
extern objPo allocateSymb(uniChar *p,integer symlen);

extern inline objPo allocateFloat(double f)
{
  floatPo new;

#if DOUBLE_ALIGNMENT
  if(create+FloatCellCount+TupleCellCount(0)>createSpaceEnd)
    gCollect(FloatCellCount+TupleCellCount(0)); /* we need to do ensure  */

  if(!ALIGNED(create,doubleAlignment)){ /* alignment AFTER a possble GC */
    allocateTuple(0);		/* a small pad ... */
    assert(ALIGNED(create,doubleAlignment));
  }
#endif
  new = (floatPo)allocate(FloatCellCount,floatMarker);

  new->f = f;
  return (objPo)new;
}
extern objPo allocateNumber(Number f);

extern inline objPo allocateChar(uniChar ch)
{
  charPo new = (charPo)allocate(CharCellCount,charMarker);

  new->ch = ch;
  return (objPo)new;
}

extern objPo allocateSubString(uniChar *p,integer size);
extern objPo allocateString(uniChar *p);
extern objPo allocateCString(char *p);

extern inline objPo allocatePair(objPo *head,objPo *tail)
{
  listPo new = (listPo)allocate(ListCellCount,listMarker);

  new->data[0] = *head;
  new->data[1] = *tail;
  return (objPo)new;
}

extern objPo oldSpace,oldSpaceEnd;
extern cardMap *cards;		/* this is a table of cards */
extern cardMap masks[];

extern inline void updateObj(objPo obj)
{
  /* are we updating an old pointer to a new value? */
  if(obj>=oldSpace && obj<oldSpaceEnd){
    long add = obj-heap;	/* the bit number to set */

    cards[add>>CARDSHIFT] |= masks[add&CARDMASK];
  }
}							

static inline void updateListHead(objPo p,objPo el)
{
  updateObj(p);
  ((listPo)p)->data[0]=el;
}

static inline void updateListTail(objPo p,objPo el)
{
  updateObj(p);
  ((listPo)p)->data[1]=el;
}

extern inline void updateTuple(objPo tuple,uinteger offset,objPo el)
{
  updateObj(tuple);
  
  assert(IsTuple(tuple));
  
  ((tuplePo)tuple)->data[offset]=el;
}

extern inline objPo allocateCons(integer count)
{
  return allocate(ConsCellCount(count),consMark(count));
}

extern inline objPo allocateConstructor(integer count)
{
  consPo cns = (consPo)allocate(ConsCellCount(count),consMark(count));
  integer i;
  objPo *el = &cns->data[0];
  
  cns->fn = kvoid;              // initialize this structure
  
  for(i=0;i<count;i++)
    *el++=kvoid;
  return (objPo)cns;
}

extern inline void updateConsEl(objPo p,integer offset,objPo el)
{
  assert(isCons(p));
  
  updateObj(p);
  ((consPo)p)->data[offset]=el;
}

extern inline void updateConsFn(objPo p,objPo fn)
{
  assert(isCons(p));
  
  updateObj(p);
  ((consPo)p)->fn = fn;
}

extern inline objPo allocateAny(objPo *type,objPo *data)
{
  anyPo new = (anyPo)allocate(AnyCellCount,anyMarker);

  new->sig = *type;
  new->data = *data;
  return (objPo)new;
}

extern inline void updateAnySig(objPo any,objPo el)
{
  updateObj(any);
  
  assert(IsAny(any));
  
  ((anyPo)any)->sig = el;
}

extern inline void updateAnyData(objPo any,objPo el)
{
  updateObj(any);
  assert(IsAny(any));
  
  ((anyPo)any)->data = el;
}

objPo allocateCode(integer size,integer count);

#include "opaque.h"

#endif


 

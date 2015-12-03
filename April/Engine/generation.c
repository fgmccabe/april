/*
  Garbage collector module for April
  Generational half-space copy system
  (c) 1994-1999 Imperial College & F.G. McCabe

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
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include "april.h"
#include "gcP.h"		/* private header info for G/C */
#include "symbols.h"
#include "process.h"
#include "msg.h"

objPo heap = NULL;		/* The full heap */
objPo heapEnd = NULL;
WORD32 heapSize = 0;

objPo createSpace;
objPo createSpaceEnd;
objPo create;			/* Where are we creating new objects */

objPo oldSpace;
objPo oldSpaceEnd;
static objPo threshold;		/* when old space crosses this, we do a major collect */

static objPo next;		/* Where are we copying to? */
static objPo scan;		/* Where are we scanning now? */

cardMap *cards = NULL;	/* this is a table of cards */
integer ncards = 0;
cardMap masks[CARDWIDTH];

#ifdef MEMTRACE
logical traceMemory = False;
logical stressMemory = False;
logical gcPermitted = True;
static int gcCount = 0;
#endif

#define markForward(p,d) { ((forwardPo)p)->fwd = (objPo)d; p->sign = forwardMarker; }

#ifdef MEMTRACE
static void verifySpace(objPo base,objPo end);
static void verifyRoots(void);
static void verifyProcesses(void);
#endif

retCode initHeap(WORD32 minsize)
{
  heap = (objPo)malloc(minsize*sizeof(objPo));
  heapEnd = &heap[minsize];
  heapSize = minsize;

  /* accompagnying card table 1bit/word of the heap */
  ncards = (minsize+CARDWIDTH-1)/CARDWIDTH;	
  cards = (cardMap*)malloc(ncards*sizeof(cardMap));

  if(heap!=NULL && cards!=NULL){
    int i;
    integer mark = minsize/2+1;	/* allow slightly less room in the create */

    createSpace = &heap[mark];
    createSpaceEnd = &heap[minsize];

    threshold = &heap[(minsize*2)/3];

    oldSpace = oldSpaceEnd = heap; /* we have no old generation at the start */
    create = createSpace;		/* we always start creating here */

    for(i=0;i<CARDWIDTH;i++)
      masks[i] = 1<<i;		/* compute 2**i for i=0 to i=31 */

    for(i=0;i<ncards;i++)
      cards[i]=0;		/* clear the card table */

    return Ok;
  }
  else
    return Space;
}

inline logical reserveSpace(size_t size)
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

inline objPo allocate(size_t size,wordTag tag)
{
  objPo new;

#ifdef MEMTRACE
  if(stressMemory)
    gCollect(size);		/* gc on every allocation */
#endif

  if(create+size>createSpaceEnd)
    gCollect(size);		/* this aborts if there is no memory */

  new = create;
  create+=size;
  new->sign = tag;

  return new;
}

integer createRoom(void)
{
  return (createSpaceEnd-create)*100/(createSpaceEnd-createSpace);
}


/* Allocate dynamic variable */
inline objPo allocateVariable(void)
{
  variablePo new = (variablePo)allocate(VariableCellCount,variableMarker);

  new->val = (objPo)new;
  return (objPo)new;
}

objPo deRefVar(objPo var)
{
  while(IsVar(var)){
    objPo p = VarVal(var);
    if(p==var)
      return var;
    else
      var = p;
  }
  return var;
}

void updateVariable(objPo var,objPo val)
{
  updateObj(var);
  VarVal(var) = val;
}

/* allocateInteger has to be sensitive to the underlying hardware --
 * on some machines, 64 bit numbers must be allocated on a 64 bit boundary.
 * This is decided during configuration
 */
inline objPo allocateInteger(integer i)
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

objPo allocateSymbol(uniChar *p)
{
  integer symlen = uniStrLen(p);
  integer len = SymbolCellCount(symlen);
  symbPo new = (symbPo)allocate(len,symbolMark(len));

  uniCpy(new->data,symlen+1,p);		/* copy the symbol's text */
  return (objPo)new;
}

objPo allocateSymb(uniChar *p,integer symlen)
{
  integer len = SymbolCellCount(symlen);
  symbPo new = (symbPo)allocate(len,symbolMark(len));

  uniCpy(new->data,symlen+1,p);		/* copy the symbol's text */
  return (objPo)new;
}

objPo allocateChar(uniChar ch)
{
  charPo new = (charPo)allocate(CharCellCount,charMarker);

  new->ch = ch;
  return (objPo)new;
}

inline objPo allocateFloat(double f)
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

    
objPo allocateSubString(uniChar *base,integer size)
{
  assert(size>=0);
  
  reserveSpace(size*(CharCellCount+ListCellCount));
  
  {
    objPo l = emptyList;
    uniChar *p = base+size;
    
    while(p>base){
      objPo chr = allocateChar(*--p);
      l = allocatePair(&chr,&l);
    }
    
    return l;
  }
}

objPo allocateString(uniChar *p)
{
  return allocateSubString(p,uniStrLen(p));
}

objPo allocateCString(char *p)
{
  integer len = strlen(p);
  uniChar buffer[MAX_SYMB_LEN];
  uniChar *buff = (len<NumberOf(buffer)?buffer:((uniChar*)malloc(sizeof(uniChar)*(len+1))));
  objPo str;

  _uni((unsigned char*)p,buff,len+1);

  str = allocateSubString(buff,len);

  if(buff!=buffer)
    free(buff);

  return str;
}

inline objPo allocatePair(objPo *head,objPo *tail)
{
  listPo new = (listPo)allocate(ListCellCount,listMarker);

  new->data[0] = *head;
  new->data[1] = *tail;
  return (objPo)new;
}

inline objPo allocateTuple(integer count)
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

/* This allocation does NOT initialize the entries */
inline objPo allocateTpl(integer count)
{
  return allocate(TupleCellCount(count),tupleMark(count));
}

inline objPo allocateCons(integer count)
{
  return allocate(ConsCellCount(count),consMark(count));
}

inline objPo allocateConstructor(integer count)
{
  consPo cns = (consPo)allocate(ConsCellCount(count),consMark(count));
  integer i;
  objPo *el = &cns->data[0];
  
  cns->fn = kvoid;              // initialize this structure
  
  for(i=0;i<count;i++)
    *el++=kvoid;
  return (objPo)cns;
}

inline void updateConsEl(objPo p,integer offset,objPo el)
{
  assert(isCons(p));
  
  updateObj(p);
  ((consPo)p)->data[offset]=el;
}

inline void updateConsFn(objPo p,objPo fn)
{
  assert(isCons(p));
  
  updateObj(p);
  ((consPo)p)->fn = fn;
}

inline objPo allocateAny(objPo *sig,objPo *data)
{
  anyPo new = (anyPo)allocate(AnyCellCount,anyMarker);

  new->sig = *sig;
  new->data = *data;
  return (objPo)new;
}

objPo allocateCode(integer size,integer count)
{
  return allocate(CodeCellCount(size,count),CodeMark);
}

objPo allocateHdl(processpo p,uniChar *hdl)
{
  integer symlen = uniStrLen(hdl);
  integer len = HdlCellCount(symlen);
  handlePo new = (handlePo)allocate(len,hdlMark(len));

  new->p = p;
  uniCpy(new->name,symlen+1,hdl);	/* copy the symbol's text */
  return (objPo)new;
}

objPo allocateOpaque(short int type,void *data)
{
  opaquePo new=(opaquePo)allocate(OpaqueCellCount(),OpaqueMark(type));

  new->data = data;

  return (objPo)new;
}

/* Management of inter-generational pointers */
inline void updateObj(objPo obj)
{
  /* are we updating an old pointer to a new value? */
  if(obj>=oldSpace && obj<oldSpaceEnd){
    integer add = obj-heap;	/* the bit number to set */

    cards[add>>CARDSHIFT] |= masks[add&CARDMASK];
  }
}

inline void updateTuple(objPo tuple,uinteger offset,objPo el)
{
  updateObj(tuple);
  
  assert(IsTuple(tuple));
  
  ((tuplePo)tuple)->data[offset]=el;
}

inline void updateAnySig(objPo any,objPo el)
{
  updateObj(any);
  
  assert(IsAny(any));
  
  ((anyPo)any)->sig = el;
}

inline void updateAnyData(objPo any,objPo el)
{
  updateObj(any);
  assert(IsAny(any));
  
  ((anyPo)any)->data = el;
}

void updateCodeLit(objPo pc,uinteger offset,objPo el)
{
  assert(IsCode(pc));
  
  updateObj(pc);

  CodeLits(pc)[offset]=el;
}

void updateCodeSig(objPo pc,objPo el)
{
  assert(IsCode(pc));
  
  updateObj(pc);
  
  ((codePo)pc)->type = el;
}

void updateCodeFrSig(objPo pc,objPo el)
{
  assert(IsCode(pc));
  
  updateObj(pc);

  ((codePo)pc)->frtype = el;
}


/* Garbage collection proper */

/* copy function used during GC */
static inline objPo newObj(size_t size,wordTag tag)
{
  register objPo new = next;

  next+=size;
  new->sign = tag;

  return new;
}


/* Temporary definition of current generation */
static inline logical currentGeneration(objPo p)
{
  if(p>=createSpace && p<createSpaceEnd)
    return True;
  else if(p>=heap && p<heapEnd)
    return False;
  else
    syserr("attempt to scan object not in heap");
  return True;
}


/* Scan a cell and place an appropriate forwarding pointer if nec. */
objPo scanCell(objPo f)
{
  if(f!=NULL && currentGeneration(f)){
    switch(Tag(f)){
    case variableMarker:{
      variablePo old = (variablePo)f;
      variablePo new = (variablePo)newObj(VariableCellCount,old->sign);
      
      new->sign = variableMarker;
      new->val = old->val;
      
      markForward(f,(objPo)new);
      return (objPo)new;
    }      
      
    case integerMarker:{        //This is also sensitive to alignment issues
      register integerPo new = (integerPo)next;

#if LLONG_ALIGNMENT
      if(!ALIGNED(next, integerAlignment)){
        newObj(TupleCellCount(0),tupleMark(0)); /* create dummy tuple */
        assert(ALIGNED(next,integerAlignment));
        new = (integerPo)next;
      }
#endif

      next+=IntegerCellCount;
      new->sign = integerMarker;
      new->i = ((integerPo)f)->i;
      markForward(f,(objPo)new);
      return (objPo)new;
    }

    case floatMarker:{
      register floatPo new = (floatPo)next;

#if DOUBLE_ALIGNMENT
      if(!ALIGNED(next,doubleAlignment)){
        newObj(TupleCellCount(0),tupleMark(0)); /* create dummy tuple */
        assert(ALIGNED(next,doubleAlignment));
        new = (floatPo)next;
      }
#endif

      next+=FloatCellCount;
      new->sign = floatMarker;
      new->f = ((floatPo)f)->f;
      markForward(f,new);
      return (objPo)new;
    }

    case symbolMarker:{
      symbPo old = (symbPo)f;
      symbPo new = (symbPo)newObj(SymbCellLength(f),old->sign);

      uniCpy(new->data,SymLen(f)+1,old->data);
      installSymbol((objPo)new);
      markForward(f,new);

      return (objPo)new;
    }
    
    case charMarker:{
      charPo old = (charPo)f;
      charPo new = (charPo)newObj(CharCellCount,old->sign);
      
      new->ch = old->ch;
      markForward(f,new);
      return (objPo)new;
    }

    case codeMarker:{
      codePo old = (codePo)f;
      integer len = CodeSize(f); /* total length of the code block */
      integer litcnt = CodeLitcnt(f);
      codePo new = (codePo)newObj(CodeCellCount(len,litcnt),CodeMark);
      instruction *o = old->data;
      instruction *n = new->data;

      new->size = old->size;
      new->litcnt = old->litcnt;
      new->formity = old->formity;
      new->spacereq = old->spacereq;
      new->type = old->type;
      new->frtype = old->frtype;

      while(len--)		/* copy the code block */
	*n++=*o++;

      while(litcnt--)
	*n++=*o++;

      markForward(f,new);
      return (objPo)new;
    }

    case listMarker:{
      listPo new = (listPo)newObj(ListCellCount,listMarker);
      listPo old = (listPo)f;

      new->data[0] = old->data[0];
      new->data[1] = old->data[1];

      markForward(f,new);
      return (objPo)new;
    }

    case consMarker:{
      uinteger arity = consArity(f);
      uinteger i;
      objPo new = newObj(ConsCellCount(arity),consMark(arity));
      objPo *o=consData(f);
      objPo *n=consData(new);
      
      ((consPo)new)->fn = ((consPo)f)->fn;

      for(i=0;i<arity;i++)
	*n++=*o++;

      markForward(f,new);
      return new;
    }

    case tupleMarker:{
      uinteger arity = tupleArity(f);
      uinteger i;
      objPo new = newObj(TupleCellCount(arity),tupleMark(arity));
      objPo *o=tupleData(f);
      objPo *n=tupleData(new);

      for(i=0;i<arity;i++)
	*n++=*o++;

      if(arity>0)		/* zero-tuples dont have space for a forward marker */
	markForward(f,new);
      return new;
    }

    case anyMarker:{		/* an any value */
      anyPo new = (anyPo)newObj(AnyCellCount,anyMarker);
      anyPo old = (anyPo)f;

      new->sig = old->sig;
      new->data = old->data;

      markForward(f,new);
      return (objPo)new;
    }

    case handleMarker:{
      handlePo old = (handlePo)f;
      handlePo new = (handlePo)newObj(HdlCellLength(f),old->sign);

      uniCpy(new->name,uniStrLen(old->name)+1,old->name);
      new->p = old->p;
      scanHandle(new);
      markForward(f,new);

      return (objPo)new;
    }

    case opaqueMarker:{
      opaquePo old = (opaquePo)f;
      opaquePo new = (opaquePo)newObj(OpaqueCellCount(),OpaqueMark(OpaqueType(f)));

      new->sign = old->sign;
      new->data = old->data;

      markForward(f,new);
      return (objPo)new;
    }
      
    case forwardMarker:
      return ((forwardPo)f)->fwd;

    default:
      syserr("illegal cell found in scancell");
      return next;		/* will never execute this... */
    }
  }
  else
    return f;
}

integer oCount[lastMarker];

static objPo scanObject(objPo scan)
{
  switch(Tag(scan)){
    case variableMarker:{
      variablePo var = (variablePo)scan;
      
      oCount[variableMarker]++;
      
      var->val = scanCell(var->val);
      return scan+VariableCellCount;
    }

  case integerMarker:
    oCount[integerMarker]++;
    return scan + IntegerCellCount; /* move over the integer value */

  case symbolMarker:		/* no internal structure to a symbol */
    oCount[symbolMarker]++;
    return scan + SymbCellLength(scan); 
    
  case charMarker:
    oCount[charMarker]++;
    return scan+CharCellCount;

  case floatMarker:
    oCount[floatMarker]++;
    return scan + FloatCellCount;

  case listMarker:{
    listPo lst = (listPo)scan;

    oCount[listMarker]++;
    lst->data[0]=scanCell(lst->data[0]);
    lst->data[1]=scanCell(lst->data[1]);
    return scan + ListCellCount;
  }

  case consMarker:{
    consPo tpl = (consPo)scan;
    integer i;
    integer arity = SignVal(scan);
    
    tpl->fn = scanCell(tpl->fn);

    for(i=0;i<arity;i++)
      tpl->data[i]=scanCell(tpl->data[i]);

    oCount[consMarker]++;
    return scan + ConsCellCount(arity);
  }

  case tupleMarker:{
    tuplePo tpl = (tuplePo)scan;
    integer i;
    integer arity = SignVal(scan);

    for(i=0;i<arity;i++)
      tpl->data[i]=scanCell(tpl->data[i]);

    oCount[tupleMarker]++;
    return scan + TupleCellCount(arity);
  }

  case anyMarker:{
    anyPo lst = (anyPo)scan;

    oCount[anyMarker]++;
    lst->sig=scanCell(lst->sig);
    lst->data=scanCell(lst->data);
    return scan + AnyCellCount;
  }

  case codeMarker:{
    int i;
    codePo pc = (codePo)scan;
    int count = CodeLitcnt(scan);
    objPo *lits = CodeLits(scan);

    for(i=0;i<count;i++,lits++)
      *lits = scanCell(*lits);

    pc->type = scanCell(pc->type);
    pc->frtype = scanCell(pc->frtype);

    oCount[codeMarker]++;
    return scan + CodeCellCount(pc->size,count);
  }

  case handleMarker:
    oCount[handleMarker]++;
    return scan+HdlCellLength(scan);

  case opaqueMarker:
    oCount[opaqueMarker]++;
    return scan+OpaqueCellCount();

  default:
    syserr("illegal cell found in GC scanning");
    return scan;
  }
}

static void scanOldGen(void)
{
  register integer i;
  register integer max = ((oldSpaceEnd-heap)+CARDMASK)>>CARDSHIFT;

  for(i=0;i<max;i++)
    if(cards[i]!=0){
      int j;

      for(j=0;j<CARDWIDTH;j++)
	if((cards[i]&masks[j])!=0)
	  scanObject(heap+(i<<CARDSHIFT)+j);
    }
}

/* Extra root management */
#ifndef MAXROOT
#define MAXROOT 1024
#endif

static objPo *rts[MAXROOT];
objPo **roots=rts;
integer topRoot = 0;
integer maxRoot = MAXROOT;

void growRoots(void)
{
  if(topRoot>=maxRoot){
    integer nmax = maxRoot+(maxRoot>>2); /* 25% growth */
    
    if(roots!=rts)
      roots = realloc(roots,sizeof(objPo*)*nmax);
    else{
      integer i;
      roots = malloc(sizeof(objPo*)*nmax);
      for(i=0;i<topRoot;i++)
	roots[i]=rts[i];
    }
    maxRoot = nmax;
  }
}

void *gcAddRoot(objPo *ptr)
{
  if(topRoot>=maxRoot)
    growRoots();
      
  roots[topRoot]=ptr;
  return (void*)topRoot++;
}

void gcRemoveRoot(void *mk)
{
  topRoot=((integer)mk);
}

void markRoots(void)
{
  integer i;
  for(i=0;i<topRoot;i++)
    markCell(*roots[i]);
}

void adjustRoots(void)
{
  integer i;
  for(i=0;i<topRoot;i++)
    *roots[i] = adjustCell(*roots[i]);
}

/*
 * Basic garbage collection program
 */

static void gC(objPo base,objPo limit)
{
  integer i;

#ifdef MEMTRACE
  memset(oCount,0,sizeof(oCount));
#endif

  /* Scan various kinds of roots ... */
  scanDict(oldSpace,oldSpaceEnd);/* Scan the dictionary */
  scanEscapes();

  scanProcesses(oldSpace,oldSpaceEnd); /* First phase -- we scan the roots */

  for(i=0;i<topRoot;i++)
    *roots[i]=scanCell(*roots[i]); /* scan the extra roots */

  scanLabels();

  scanOldGen();			/* scan the old generation also */

  /* Scan the newly copied objects */

  while(scan!=next)
    scan = scanObject(scan);	/* Second phase -- we scan the main heap */

  resetProcesses();		/* clean up processes */
}

logical gCollect(integer amount)
{
  assert(!(oldSpaceEnd>=createSpace && oldSpaceEnd<createSpaceEnd));

#ifdef MEMTRACE
  gcCount++;
  if(traceMemory){
    assert(gcPermitted);
    logMsg(logFile,"Garbage collector requested by %.1w: %d words",
	   current_process->handle,amount);
    verifyHandles();
    verifySpace(createSpace,create);
    verifySpace(oldSpace,oldSpaceEnd);
    verifyProcesses();
    verifyRoots();
  }
#endif

  scan = next = oldSpaceEnd;	/* all data is copied here... */

  gC(scan,heapEnd);		/* invoke main garbage collector */

#ifdef MEMTRACE
  if(traceMemory){
    logMsg(logFile,"Adding %d words to old space",next-oldSpaceEnd);

    oldSpaceEnd=next;
    verifySpace(heap,next);
    verifyProcesses();
    verifyHandles();
    verifyRoots();
  }
#endif

  if(next>=threshold)		/* we have to do a major collect now */
    next = compactHeap(heap,next,heapEnd);	/* we compact everything down */
  oldSpaceEnd = next;		/* reset the `old' generation marker */
  createSpace = create = next+(heapEnd-next)/2+1; /* new creation space */

  /* 
   * We test for enough space, and either gc the whole lot, or even grow the heap
   */

  if(amount>((createSpaceEnd-createSpace)*75)/100||
     createSpaceEnd-createSpace<(heapSize>>3)){
    integer nsize = heapSize+(heapSize>>1)+amount*2; /* grow a new heap */
    objPo nheap = (objPo)malloc(nsize*sizeof(objPo));
    integer ncrdsize = (nsize+CARDWIDTH-1)/CARDWIDTH;
    cardMap *newmap = (cardMap*)realloc(cards,ncrdsize*sizeof(cardMap));

    if(nheap==NULL || newmap==NULL)
      syserr("unable to grow heap");

#ifdef MEMTRACE
    if(traceMemory)
      logMsg(logFile,"grow heap to %d words",nsize);
#endif

    createSpace = heap;		/* this should force everything to be copied */
    createSpaceEnd = heapEnd;
    oldSpace = oldSpaceEnd = heap;
    scan = next = nheap;

    memset(newmap,0,ncrdsize*sizeof(cardMap));
    cards = newmap;
    ncards = ncrdsize;


    gC(scan,&nheap[nsize]);	/* copy everything to the new heap */

    free(heap);
    heapSize = nsize;
    heap = nheap;
    heapEnd = &heap[nsize];
    threshold = &heap[(nsize*2)/3];

    oldSpace = heap;
    oldSpaceEnd = next;		/* reset the `old' generation marker */
    createSpace = create = next+(heapEnd-next)/2+1; /* new creation space */
    createSpaceEnd = heapEnd;
  }

#ifdef MEMTRACE
  if(traceMemory){
    verifySpace(heap,oldSpaceEnd);
    verifyProcesses();
    verifyHandles();
    verifyRoots();
    logMsg(logFile,"%d words for creation in heap",createSpaceEnd-createSpace);
  }
#endif

  return True;
}

logical gcTest(integer amount)
{
  return create+amount<=createSpaceEnd;
}

#ifdef MEMTRACE

objPo checkObject(objPo scan,int depth);

static logical checkPtr(objPo src,objPo ptr,int depth)
{
  if(depth<=0)
    return True;
  else if(ptr==NULL) /* Integers are out of the heap */
    return True;
  else if(!((ptr>=oldSpace && ptr<oldSpaceEnd) || 
	    (ptr>=createSpace && ptr< create)))
    return False;
  else if(src!=NULL && src>=oldSpace && src<oldSpaceEnd &&
	  ptr>=createSpace && ptr<create){
    integer i = src-heap;

    if(!(cards[i>>CARDSHIFT]&masks[i&CARDMASK])!=0)
      return False;
  }
  checkObject(ptr,depth);	/* scan the object itself */
  return True;
}

objPo checkObject(objPo scan,int depth)
{
  switch(Tag(scan)){
  case variableMarker:{
    variablePo var = (variablePo)scan;

    if(!checkPtr(scan,VarVal(var),depth-1))
      syserr("variable value out of heap");

    return scan + VariableCellCount;
  }

  case integerMarker:
#if LLONG_ALIGNMENT
    if(!ALIGNED(scan,integerAlignment))
      syserr("integer value not properly aligned");
#endif
    return scan + IntegerCellCount; /* move over the integer value */

  case symbolMarker:		/* no internal structure to a symbol */
    return scan + SymbCellLength(scan); 
  
  case charMarker:
    return scan+CharCellCount;

  case floatMarker:
#if DOUBLE_ALIGNMENT
    if(!ALIGNED(scan,doubleAlignment))
      syserr("float value not properly aligned");
#endif
    return scan+FloatCellCount;

  case listMarker:{
    listPo lst = (listPo)scan;

    if(!checkPtr(scan,lst->data[0],depth-1))
      syserr("list head out of heap");
    else if(!checkPtr(scan,lst->data[1],depth-1))
      syserr("list tail out of heap");

    return scan + ListCellCount;
  }

  case consMarker:{
    consPo tpl = (consPo)scan;
    integer i;
    integer arity = SignVal(scan);

    if(!checkPtr(scan,tpl->fn,depth-1))
	syserr("constructor symbol out of heap");

    for(i=0;i<arity;i++)
      if(!checkPtr(scan,tpl->data[i],depth-1))
	syserr("constructor element out of heap");

    return scan+ConsCellCount(arity);
  }

  case tupleMarker:{
    tuplePo tpl = (tuplePo)scan;
    integer i;
    integer arity = SignVal(scan);

    for(i=0;i<arity;i++)
      if(!checkPtr(scan,tpl->data[i],depth-1))
	syserr("tuple element out of heap");

    return scan+TupleCellCount(arity);
  }

  case anyMarker:{
    anyPo any = (anyPo)scan;

    if(!checkPtr(scan,any->sig,depth-1))
      syserr("any type signature out of heap");
    else if(!checkPtr(scan,any->data,depth-1))
      syserr("any data value out of heap");

    return scan + AnyCellCount;
  }

  case codeMarker:{
    int i;
    codePo pc = (codePo)scan;
    int litcnt = CodeLitcnt(scan);
    objPo *lits = CodeLits(scan);

    for(i=0;i<litcnt;i++,lits++)
      if(!checkPtr(scan,*lits,depth-1))
	syserr("code literal out of heap");

    if(!checkPtr(scan,pc->type,depth-1))
      syserr("code type signature out of heap");

    if(!checkPtr(scan,pc->frtype,depth-1))
      syserr("code free type signature out of heap");

    return scan + CodeCellCount(pc->size,litcnt);
  }

  case opaqueMarker:
    return scan + OpaqueCellCount();

  case handleMarker:		/* no internal structure to a handle */
    return scan + HdlCellLength(scan); 

  default:
    syserr("illegal cell found in space verify");
    return scan;
  }
}

static void verifySpace(objPo base,objPo end)
{
  objPo scan = base;

  while(scan<end)
    scan = checkObject(scan,5);

  if(scan>end)
    syserr("heap overflow");
}

static void verifyRoots(void)
{
  int i=0;

  for(i=0;i<topRoot;i++)
    checkObject(*roots[i],5);
}

static inline insPo CodeBase(objPo b)
{
  objPo e = Forwarded(b)?FwdVal(b):b;
  objPo p = codeOfClosure(e);

  codePo pc = CodeVal(Forwarded(p)?FwdVal(p):p);

  return &pc->data[0];
}

static inline insPo CodeEnd(objPo b)
{
  objPo e = Forwarded(b)?FwdVal(b):b;
  objPo p = codeOfClosure(e);

  codePo pc = CodeVal(Forwarded(p)?FwdVal(p):p);

  return &pc->data[pc->size];
}

void verifyProcess(processpo p,void *cl)
{
  if(p->state!=dead){		/* only process live processes */
    objPo *fp=p->fp;
    objPo *sp=p->sp;
    objPo *sb=p->sb;
    objPo *er=p->er;
    msgpo msgs = p->mfront;
    objPo pe = p->e;

    while(sp<sb){
      objPo e = fp[1];		/* The environment of this frame */
      register integer i = fp-sp;	/* the length of the local environment */
      insPo rtn = (insPo)fp[2];

      if(!checkPtr(NULL,e,5))
	syserr("invalid E value in process stack");

      while(i--){
	if(sp==er){
	  insPo brk = (insPo)er[1];
	  
	  if(!(brk>=CodeBase(pe) && brk<CodeEnd(pe)))
	    syserr("invalid error label on process stack");
	  er = (objPo*)er[0];
	  i--;
	  sp+=2;		/* move over the error handler frame */
	}
	else if(!checkPtr(NULL,*sp++,5)){
	  outMsg(logFile,"invalid FP[%d] in process stack\n",sp-fp);
	  flushFile(logFile);
	  syserr("invalid process stack");
	}
      }

      pe = e;
	
      if(!(rtn>=CodeBase(e) && rtn<CodeEnd(e)))
	syserr("invalid return address on process stack");

      fp = *(objPo**)fp;	/* go back to the previous frame pointer */
      sp +=3;			/* step over the return address and env*/
    }

    if(!checkPtr(NULL,p->e,5))
	syserr("invalid E register in process");

    if(!checkPtr(NULL,p->errval,5))
	syserr("invalid error value register in process");

    if(!checkPtr(NULL,p->handle,2))
	syserr("invalid handle in process");

    if(!checkPtr(NULL,p->creator,2))
	syserr("invalid creator value register in process");

    if(!checkPtr(NULL,p->filer,2))
	syserr("invalid filer value register in process");

    if(!(p->pc>=CodeBase(p->e) && p->pc<=CodeEnd(p->e)))
	syserr("invalid PC in process");

    while(msgs!=NULL){
      if(!checkPtr(NULL,msgs->msg,5))
	syserr("invalid message structure in process");
      if(!checkPtr(NULL,msgs->opts,5))
	syserr("invalid message option structure in process");
      if(!checkPtr(NULL,msgs->sender,2))
	syserr("invalid sender in message structure");
      msgs = msgs->next;
    }
  }
}

static void verifyProcesses(void)
{
  processProcesses(verifyProcess,NULL);
}

logical permitGC(logical allowed)
{
  logical p = gcPermitted;
  gcPermitted = allowed;
  return p;
}

#endif

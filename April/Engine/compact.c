/*
  Compacting garbage collector
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
#include "process.h"
#include "dict.h"
#include "sign.h"

#ifdef MEMTRACE
static int compactCount = 0;
#endif


static WORD32 usedObjects = 0;
static WORD32 usedWords = 0;

void initMarking(void)
{
  WORD32 i;

  for(i=0;i<ncards;i++)
    cards[i]=0;			/* clear the mark table */

  usedObjects = 0;
  usedWords = 0;
}

static inline void mrkWord(objPo p)
{
  WORD32 add = p-heap;		/* the bit number to set */

  cards[add>>CARDSHIFT] |= masks[add&CARDMASK];
}

static inline logical marked(objPo p)
{
  WORD32 add = p-heap;		/* the bit number to set */

  return (cards[add>>CARDSHIFT]&masks[add&CARDMASK])!=0;
}

static WORD32 oCount[lastMarker];
static WORD32 oCnt;

/* mark a cell -- by setting the appropriate bit in the card table */
void markCell(objPo f)
{
 again:
  if(f!=NULL && !marked(f)){
    switch(Tag(f)){

    case variableMarker:{
      variablePo var = (variablePo)f;

      mrkWord(f);

      oCount[variableMarker]++;
      oCnt++;
      usedWords+=VariableCellCount;

      f = var->val;		/* tail recursive call to markcell */
      goto again;
    }

    case integerMarker:{
      mrkWord(f);
      oCount[integerMarker]++;
      oCnt++;
      usedWords+=IntegerCellCount;
      return;
    }

    case floatMarker:{
      mrkWord(f);
      oCount[floatMarker]++;
      oCnt++;
      usedWords+=FloatCellCount;
      return;
    }

    case symbolMarker:{
      mrkWord(f);
      oCount[symbolMarker]++;
      oCnt++;
      usedWords+=SymbCellLength(f);
      return;
    }
    
    case charMarker:{
      mrkWord(f);
      oCount[charMarker]++;
      oCnt++;
      usedWords+=CharCellCount;
      return;
    }

    case codeMarker:{
      int i;
      codePo pc = (codePo)f;
      WORD32 litcnt = pc->litcnt;
      objPo *lits = CodeLits(f);

      mrkWord(f);

      oCount[codeMarker]++;
      oCnt++;
      usedWords+=CodeCellCount(pc->size,litcnt);

      for(i=0;i<litcnt;i++,lits++)
	markCell(*lits);

      markCell(pc->type);
      markCell(pc->frtype);

      return;
    }

    case listMarker:{
      listPo lst = (listPo)f;

      mrkWord(f);

      oCount[listMarker]++;
      oCnt++;
      usedWords+=ListCellCount;

      markCell(lst->data[0]);

      f = lst->data[1];		/* tail recursive call to markcell */
      goto again;
    }

    case consMarker:{
      unsigned WORD32 arity = consArity(f);
      unsigned WORD32 i;
      objPo *tpl = consData(f);

      mrkWord(f);
      oCount[consMarker]++;
      oCnt++;
      usedWords+=ConsCellCount(arity);
      
      markCell(consFn(f));

      for(i=0;i<arity;i++)
	markCell(*tpl++);
      return;
    }

    case tupleMarker:{
      unsigned WORD32 arity = tupleArity(f);
      unsigned WORD32 i;
      objPo *tpl = tupleData(f);

      mrkWord(f);
      oCount[tupleMarker]++;
      oCnt++;
      usedWords+=TupleCellCount(arity);

      for(i=0;i<arity;i++)
	markCell(*tpl++);
      return;
    }

    case anyMarker:{
      anyPo any = (anyPo)f;

      mrkWord(f);

      oCount[anyMarker]++;
      oCnt++;
      usedWords+=AnyCellCount;

      markCell(any->sig);

      f = any->data;		/* tail recursive call to markcell */
      goto again;
    }

    case handleMarker:
      mrkWord(f);

      oCount[handleMarker]++;
      oCnt++;
      usedWords+=HdlCellLength(f);
      return;

    case opaqueMarker:
      mrkWord(f);

      oCount[opaqueMarker]++;
      oCnt++;
      usedWords+=OpaqueCellCount();
      return;
      
    default:
      syserr("illegal cell found in markCell");
      return;			/* will never execute this... */
    }
  }
}

typedef struct {
  objPo start;
  objPo final;
} breakEntry, *breakPo;

static breakPo Brk;
static breakPo endBrk;

static objPo searchBreak(breakPo Brk,breakPo endBrk,objPo p)
{
  while(endBrk>=Brk){
    breakPo middle = &Brk[(endBrk-Brk)/2];

    if(middle->start>p){
      if(middle!=Brk)
	endBrk=middle;
      else
	return NULL;		/* couldnt find it in the table */
    }
    else if(middle->start<p){
      if(middle!=Brk)
	Brk=middle;
      else
	return NULL;
    }
    else if(middle->start==p)
      return middle->final;
    else
      return NULL;
  }
  return NULL;
}

objPo adjustCell(objPo scan)
{
  objPo final = searchBreak(Brk,endBrk,scan);

  if(final!=NULL)
    return final;

  return scan;			/* leave other pointers alone */
}

static objPo adjustObject(objPo scan)
{
  switch(Tag(scan)){
  case variableMarker:{
    variablePo lst = (variablePo)scan;

    oCount[variableMarker]++;
    lst->val=adjustCell(lst->val);
    return scan + VariableCellCount;
  }

  case integerMarker:
    oCount[integerMarker]++;
    return scan + IntegerCellCount; /* move over the integer value */

  case symbolMarker:
    oCount[symbolMarker]++;
    installSymbol(scan);                /* reinstall symbol in dictionary */
    
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
    lst->data[0]=adjustCell(lst->data[0]);
    lst->data[1]=adjustCell(lst->data[1]);
    return scan + ListCellCount;
  }

  case consMarker:{
    consPo tpl = (consPo)scan;
    WORD32 i;
    WORD32 arity = SignVal(scan);
    
    tpl->fn = adjustCell(tpl->fn);

    for(i=0;i<arity;i++)
      tpl->data[i]=adjustCell(tpl->data[i]);

    oCount[consMarker]++;
    return scan + ConsCellCount(arity);
  }

  case tupleMarker:{
    tuplePo tpl = (tuplePo)scan;
    WORD32 i;
    WORD32 arity = SignVal(scan);

    for(i=0;i<arity;i++)
      tpl->data[i]=adjustCell(tpl->data[i]);

    oCount[tupleMarker]++;
    return scan + TupleCellCount(arity);
  }

  case anyMarker:{
    anyPo any = (anyPo)scan;

    oCount[anyMarker]++;
    any->sig=adjustCell(any->sig);
    any->data=adjustCell(any->data);
    return scan + AnyCellCount;
  }

  case codeMarker:{
    int i;
    codePo pc = (codePo)scan;
    int count = CodeLitcnt(scan);
    objPo *lits = CodeLits(scan);

    oCount[codeMarker]++;

    for(i=0;i<count;i++,lits++)
      *lits = adjustCell(*lits);

    pc->type = adjustCell(pc->type);
    pc->frtype = adjustCell(pc->frtype);

    return scan + CodeCellCount(pc->size,count);
  }

  case handleMarker:
    installHandle((handlePo)scan); /* put this back in the handle table */
    oCount[handleMarker]++;
    return scan + HdlCellLength(scan); 

  case opaqueMarker:
    oCount[opaqueMarker]++;
    return scan+OpaqueCellCount();

  default:
    syserr("illegal cell found in GC adjusting");
    return scan;
  }
}

#ifdef MEMTRACE
static WORD32 countBits(cardMap *map,WORD32 limit)
{
  WORD32 count = 0;
  WORD32 i;

  for(i=0;i<limit;i++)
    if(map[i]!=0){
      int j;

      for(j=0;j<CARDWIDTH;j++)
	if((cards[i]&masks[j])!=0)
	  count++;
    }

  return count;
}
#endif

objPo compactHeap(objPo heap,objPo end,objPo heaplimit)
{
  objPo next = heap;		/* we have to scan the whole heap... */
  objPo scan = heap;
  WORD32 i=0;
  WORD32 limit = (end-heap+CARDMASK)>>CARDSHIFT;

#ifdef MEMTRACE
  compactCount++;

  if(traceMemory)
    logMsg(logFile,"compacting heap");
#endif
  
  Brk = (breakPo)end;		/* there is always space here for the break table */
  endBrk = Brk;

  memset(oCount,0,sizeof(oCount));
  oCnt = 0;

  initMarking();

  markDict();			/* we have to mark everything again... */
  markEscapes();
  markProcesses();
  markRoots();
  markLabels();

  if(oCnt>(breakPo)heaplimit-(breakPo)end)
    Brk = endBrk = (breakPo)malloc(sizeof(breakEntry)*oCnt);

#ifdef MEMTRACE
  if(traceMemory){
    logMsg(logFile,"%d dynamic variables found",oCount[variableMarker]);
    logMsg(logFile,"%d integers found",oCount[integerMarker]);
    logMsg(logFile,"%d floats found",oCount[floatMarker]);
    logMsg(logFile,"%d symbols found",oCount[symbolMarker]);
    logMsg(logFile,"%d chars found",oCount[charMarker]);
    logMsg(logFile,"%d list pairs found",oCount[listMarker]);
    logMsg(logFile,"%d constructors found",oCount[consMarker]);
    logMsg(logFile,"%d tuples found",oCount[tupleMarker]);
    logMsg(logFile,"%d any values found",oCount[anyMarker]);
    logMsg(logFile,"%d code blocks found",oCount[codeMarker]);
    logMsg(logFile,"%d handles found",oCount[handleMarker]);
    logMsg(logFile,"%d opaque pointers found",oCount[opaqueMarker]);
    logMsg(logFile,"%d entries in bitmap",countBits(cards,limit));
  }
#endif

  for(i=0;i<limit;i++)
    if(cards[i]!=0){
      int j;

      for(j=0;j<CARDWIDTH;j++){
	if((cards[i]&masks[j])!=0){
	  objPo ptr = heap+(i<<CARDSHIFT)+j;
	  wordTag tag = Tag(ptr);

	  endBrk->start = ptr;	/* store entry in break table */
	  endBrk->final = next;	/* where the new object will be */
	  
	  assert(ptr>=heap && ptr<=end);

	  switch(tag){
	  case variableMarker:{
	    variablePo new = (variablePo)next;
	    variablePo old = (variablePo)ptr;

	    new->sign = old->sign;
	    new->val = old->val;

	    next += VariableCellCount;
	    break;
	  }

	  case integerMarker:{
#if LLONG_ALIGNMENT
	    if(!ALIGNED(next,integerAlignment)){
	      next->sign = tupleMark(0);
	      next++;
	      endBrk->final = next; /* update final pointer */
	      assert(ALIGNED(next,integerAlignment));
	    }	      
#endif
	    next->sign = integerMarker;
	    ((integerPo)next)->i = IntVal(ptr);

	    next+=IntegerCellCount;
	    break;
	  }

	  case floatMarker:{
	    floatPo old = (floatPo)ptr;
	    floatPo new = (floatPo)next;
	    
#if DOUBLE_ALIGNMENT
	    if(!ALIGNED(next,doubleAlignment)){
	      next->sign = tupleMark(0);
	      next++;
	      endBrk->final = next; /* update final pointer */
	      assert(ALIGNED(next,doubleAlignment));
	      new = (floatPo)next;
	    }	      
#endif
	    new->sign = old->sign;
	    new->f = old->f;
	    
	    next += FloatCellCount;
	    break;
	  }

	  case symbolMarker:{	/* no internal structure to a symbol */
	    symbPo old = (symbPo)ptr;
	    symbPo new = (symbPo)next;
	    WORD32 len = SymbCellLength(ptr);

	    memmove(new,old,SymbolSize(ptr));
	    next += len;
	    break;
	  }
	  
	  case charMarker:{
	    charPo old = (charPo)ptr;
	    charPo new = (charPo)next;
	    
	    new->sign = old->sign;
	    new->ch = old->ch;
	    next+=CharCellCount;
	    break;
	  }

	  case listMarker:{
	    listPo new = (listPo)next;
	    listPo old = (listPo)ptr;

	    new->sign = old->sign;
	    new->data[0] = old->data[0];
	    new->data[1] = old->data[1];

	    next += ListCellCount;
	    break;
	  }

	  case consMarker:{
	    unsigned WORD32 arity = consArity(ptr);
	    
	    memmove(next,ptr,ConsCellCount(arity)*sizeof(objPo));

	    next += ConsCellCount(arity);
	    break;
	  }

	  case tupleMarker:{
	    unsigned WORD32 arity = tupleArity(ptr);
	    
	    memmove(next,ptr,TupleCellCount(arity)*sizeof(objPo));

	    next += TupleCellCount(arity);
	    break;
	  }

	  case anyMarker:{
	    anyPo new = (anyPo)next;
	    anyPo old = (anyPo)ptr;

	    new->sign = old->sign;
	    new->sig = old->sig;
	    new->data = old->data;

	    next += AnyCellCount;
	    break;
	  }

	  case codeMarker:{
	    codePo old = (codePo)ptr;
	    WORD32 len = CodeSize(ptr); /* total length of the code block */
	    WORD32 litcnt = CodeLitcnt(ptr);
	    codePo new = (codePo)next;
	    instruction *o = old->data;
	    instruction *n = new->data;

	    next += CodeCellCount(len,litcnt);

	    new->sign = old->sign;
	    new->size = old->size;
	    new->litcnt = old->litcnt;
	    new->formity = old->formity;
	    new->spacereq = old->spacereq;
	    new->type = old->type;
	    new->frtype = old->frtype;

	    while(len--)	/* copy the code block */
	      *n++=*o++;

	    while(litcnt--)
	      *n++=*o++;

	    break;
	  }

	  case handleMarker:{
	    handlePo old = (handlePo)ptr;
	    handlePo new = (handlePo)next;
	    WORD32 size = sizeof(handleRec)+sizeof(uniChar)*(uniStrLen(old->name)+1);
	    next += HdlCellLength(ptr);

	    memmove(new,old,size);

	    break;
	  }

	  case opaqueMarker:{
	    opaquePo old = (opaquePo)ptr;
	    opaquePo new = (opaquePo)next;

	    new->sign = old->sign;
	    new->data = old->data;

	    next+=OpaqueCellCount();
	    break;
	  }

	  default:
	    syserr("illegal cell found in GC compact phase");
	  }

	  endBrk++;
	  assert(next>=heap && next<=end);
	}
      }
    }

  memset(oCount,0,sizeof(oCount));

  while(scan<next)
    scan = adjustObject(scan);

#ifdef MEMTRACE
  if(traceMemory){
    logMsg(logFile,"%d dynamic variables adjusted",oCount[variableMarker]);
    logMsg(logFile,"%d integers adjusted",oCount[integerMarker]);
    logMsg(logFile,"%d floats adjusted",oCount[floatMarker]);
    logMsg(logFile,"%d symbols adjusted",oCount[symbolMarker]);
    logMsg(logFile,"%d chars adjusted",oCount[charMarker]);
    logMsg(logFile,"%d list pairs adjusted",oCount[listMarker]);
    logMsg(logFile,"%d constructors adjusted",oCount[consMarker]);
    logMsg(logFile,"%d tuples adjusted",oCount[tupleMarker]);
    logMsg(logFile,"%d any values adjusted",oCount[anyMarker]);
    logMsg(logFile,"%d code blocks adjusted",oCount[codeMarker]);
    logMsg(logFile,"%d handles adjusted",oCount[handleMarker]);
    logMsg(logFile,"%d opaque pointers adjusted",oCount[opaqueMarker]);
  }
#endif

  adjustRoots();
  adjustDict();
  adjustEscapes();
  adjustProcesses();
  adjustHandles();
  adjustLabels();

  if(Brk!=(breakPo)end)
    free(Brk);

  memset(cards,0,limit*sizeof(cardMap));

  return next;			/* this should be the pointer to the first free locn */
}


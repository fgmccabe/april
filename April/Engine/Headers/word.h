/* 
  Main memory layout definitions
  (c) 1994-2002 Imperial College and F.G.McCabe

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

#ifndef _ENGINE_WORD_H_
#define _ENGINE_WORD_H_

/* 1/7/2000 Changed the way that any and opaque values are represented */
/* 12/17/1999 Added definitions for dynamic variables  */

/* BE VERY CAREFUL if you change this .... */
/* Definition of a cell/pointer for the April run-time engine
 * The value pointed at is preceded by a descriptor word which gives the type of 
 * the object and its size if necessary
 */

/* Each object in the heap is headed by a 1 word marker giving its type and length */
#define MARK_SHIFT 4
#define MARK_MASK ((1<<MARK_SHIFT)-1)
#define objectMark(tag,len) (tag|((len)<<MARK_SHIFT))

#ifndef MAX_SYMB_LEN
#define MAX_SYMB_LEN 256
#endif

typedef enum { integerMarker=0,	/* an integer value */
               variableMarker, // An unbound type variable
	       symbolMarker,	/* a symbol structure */
	       charMarker,      /* a character */
	       floatMarker,	/* a floating point number */
	       listMarker,	/* a list pair */
	       consMarker,      // A constructor tuple
	       tupleMarker,	/* a tuple/record */
	       anyMarker,	/* an any value */
	       codeMarker,	/* a code structure */
	       handleMarker,	/* a handle structure */
	       opaqueMarker,	/* an opaque C type */
	       forwardMarker,	/* a forwarded pointer */
	       lastMarker	/* count of the number of marker types */
             } wordTag;

typedef struct _general_record_ {
  long sign;
} generalRec, *objPo;		/* Basic object record has a signature ... */

#define CLLSZE sizeof(objPo)

#define Tag(p) ((wordTag)(((p)->sign)&MARK_MASK))
#define SignVal(p) (((p)->sign)>>MARK_SHIFT)

#define TwoTag(x,y)     (((x&0xf)<<4)|(y&0xf))

#define ALIGNPTR(count,size) (((count+size-1)/size)*(size))
#define CellCount(size) (ALIGNPTR(size,CLLSZE)/CLLSZE)
#define ALIGNED(ptr,size) (((((long)ptr)+size-1)/size)*(size)==(long)ptr)


/* Dynamic variable */
typedef struct _variable_record_ {
  long sign;			/* The sign of the entity */
  objPo val;			/* value to which variable is bound */
} variableRec, *variablePo;

#define IsVar(p) (Tag(p)==variableMarker)
#define VarVal(p) (((variablePo)p)->val)
#define variableMark objectMark(variableMarker,0)
#define VariableCellCount CellCount(sizeof(variableRec))

extern inline objPo deRefVar(objPo var)
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

#include "integer.h"		/* pick up the integer type */

typedef struct _integer_record_ {
  long sign;
  integer i;
} integerRec, *integerPo;

#if LLONG_ALIGNMENT
#define integerAlignment sizeof(integer)
#else
#define integerAlignment sizeof(long)
#endif

#ifndef MAX_APRIL_INT
#define MIN_APRIL_INT ((((integer)-1)<<(sizeof(integer)*8-1))+1)
#define MAX_APRIL_INT   -MIN_APRIL_INT /* largest april integer */
#endif

#define IsInteger(p) (Tag(p)==integerMarker)
#define IntegerCellCount CellCount(sizeof(integerRec))

static inline integer IntVal(objPo p)
{
  assert(IsInteger(p));
  
  return ((integerPo)p)->i;
}

typedef double Number;

typedef struct _float_record_ {
  long sign;			/* Signature of the floating point block */
  Number f;
} floatRec, *floatPo;

static inline logical IsFloat(objPo p)
{
  return Tag(p)==floatMarker;
}

static inline Number FloatVal(objPo p)
{
  assert(IsFloat(p));
  
  return ((floatPo)p)->f;
}

#define FloatCellCount CellCount(sizeof(floatRec))

#if DOUBLE_ALIGNMENT
#define doubleAlignment sizeof(double)
#else
#define doubleAlignment sizeof(long)
#endif

static inline Number NumberVal(objPo p)
{
  if(IsFloat(p))
    return FloatVal(p);
  else
    return IntVal(p);
}

#include "chars.h"

typedef struct _symb_record_ {
  long sign;			/* Signature of the symbol block */
  uniChar data[0];		/* The symbol's print name */
} symbolRec, *symbPo;

static inline logical isSymb(objPo p)
{
  return Tag(p)==symbolMarker;
}

static inline logical isUserSymb(objPo p)
{
  if(Tag(p)==symbolMarker)
    return (((symbPo)p)->data)[0]=='\'';
  else
    return False;
}

#define SymbCellLength(p) SignVal(p)

static inline uniChar* SymVal(objPo p)
{
  assert(isSymb(p));
  
  return ((symbPo)p)->data;
}

static inline uniChar* SymText(objPo p)
{
  assert(isSymb(p));
  
  {
    uniChar *text = ((symbPo)p)->data;
    
    if(text[0]=='\'')
      return text+1;
    else
      return text;
  }
}

static inline long SymLen(objPo p)
{
  assert(isSymb(p));
  
  return uniStrLen(((symbPo)p)->data);
}

#define symbolMark(len) objectMark(symbolMarker,len)
#define SymbolCellCount(count) CellCount(sizeof(symbolRec)+(count+1)*sizeof(uniChar))
#define SymbolSize(p) (SignVal(p)*sizeof(long))

static inline logical LogicalVal(objPo p)
{
  extern objPo ktrue;
  
  assert(isSymb(p));
  
  if(p==ktrue)
    return True;
  else
    return False;
}

typedef struct _list_record_ {
  long sign;			/* signature of the list pair record */
  objPo data[2];		/* The head and tail of the list */
} listRec, *listPo;

static inline logical IsList(objPo p)
{
  return Tag(p)==listMarker;
}

static inline logical isEmptyList(objPo p)
{
  extern objPo emptyList;
  
  return p==emptyList;
}

static inline logical isNonEmptyList(objPo p)
{
  extern objPo emptyList;
  
  return Tag(p)==listMarker && p!=emptyList;
}

static inline objPo *ListData(objPo p)
{
  assert(isNonEmptyList(p));
  
  return &((listPo)p)->data[0];
}

static inline objPo ListHead(objPo p)
{
  assert(isNonEmptyList(p));
  
  return ((listPo)p)->data[0];
}

static inline objPo ListTail(objPo p)
{
  assert(isNonEmptyList(p));
  
  return ((listPo)p)->data[1];
}

#define ListVal(p) ((listPo)p)
#define ListCellCount CellCount(sizeof(listRec))

extern inline long ListLen(objPo lst)
{
  long len = 0;
  while(isNonEmptyList(lst)){
    lst = ListTail(lst);
    len++;
  }
  if(isEmptyList(lst))
    return len;
  else
    return -len;		/* non-null terminated list */
}

static inline logical isListOfInts(objPo p)
{
  while(isNonEmptyList(p)){
    if(!IsInteger(ListHead(p)))
      return False;
    else
      p = ListTail(p);
  }
  return isEmptyList(p);
}

extern inline logical isListOfChars(objPo p)
{
  while(isNonEmptyList(p)){
    if(!isChr(ListHead(p)))
      return False;
    else
      p = ListTail(p);
  }
  return isEmptyList(p);
}

extern inline uniChar *StringText(objPo p,uniChar *buffer,unsigned long len)
{
  uniChar *txt = buffer;
  
  assert(isListOfChars(p));
  
  while(isNonEmptyList(p) && --len>0){
    *txt++=CharVal(ListHead(p));
    p = ListTail(p);
  }
  
  *txt=0;
  return buffer;
}

/* Constructor term */

typedef struct _cons_term_ {
  long sign;			/* Signature of the tuple -- includes its length */
  objPo fn;                     // The constructor symbol itself
  objPo data[0];		/* The elements of the tuple */
} consRec, *consPo;

static inline logical isCons(objPo p)
{
  return Tag(p)==consMarker;
}

static inline logical isConstructor(objPo p,objPo f)
{
  return Tag(p)==consMarker && ((consPo)p)->fn==f;
}

#define ConsVal(p) ((consPo)p)	/* point to the constructor structure */

static inline unsigned long consArity(objPo p)
{
  assert(isCons(p));
  
  return SignVal(p);
}

static inline objPo consFn(objPo p)
{
  assert(isCons(p));
  
  return ((consPo)p)->fn;
}

static inline objPo consEl(objPo p,unsigned long n)
{
  assert(isCons(p) && n>=0 && n<consArity(p));
  
  return ((consPo)p)->data[n];
}

static inline objPo *consData(objPo p)
{
  assert(isCons(p));
  
  return &((consPo)p)->data[0];
}

static inline logical isBinCall(objPo s,objPo c,objPo *a1,objPo *a2)
{
  if(isCons(s) && consArity(s)==2 && consFn(s)==c){
    if(a1!=NULL)
      *a1 = consEl(s,0);
    if(a2!=NULL)
      *a2 = consEl(s,1);
    return True;
  }
  else
    return False;
}

static inline logical isUnCall(objPo s,objPo c,objPo *a1)
{
  if(isCons(s) && consArity(s)==1 && consFn(s)==c){
    if(a1!=NULL)
      *a1 = consEl(s,0);
    return True;
  }
  else
    return False;
}

#define consMark(len) objectMark(consMarker,len)
#define ConsCellCount(count) CellCount(sizeof(consRec)+(count*CLLSZE))

typedef struct _tuple_record_ {
  long sign;			/* Signature of the tuple -- includes its length */
  objPo data[0];		/* The elements of the tuple */
} tupleRec, *tuplePo;

static inline logical IsTuple(objPo p)
{
  return Tag(p)==tupleMarker;
}

#define TplVal(p) ((tuplePo)p)	/* point to the tuple structure */

static inline unsigned long tupleArity(objPo p)
{
  assert(IsTuple(p));
  
  return SignVal(p);
}

static inline objPo *Nth(objPo p,long n)
{
  assert(IsTuple(p));
  
  return &((tuplePo)p)->data[n];
}

static inline objPo tupleArg(objPo p,unsigned long n)
{
  assert(IsTuple(p) && n>=0 && n<tupleArity(p));
  
  return ((tuplePo)p)->data[n];
}

#define tupleMark(len) objectMark(tupleMarker,len)
#define TupleCellCount(count) CellCount(sizeof(tupleRec)+(count*CLLSZE))

static inline objPo *tupleData(objPo p)
{
  assert(IsTuple(p));
  
  return &((tuplePo)p)->data[0];
}

typedef struct _any_record_ {
  long sign;			/* Signature of the any structure */
  objPo sig;			/* The type signature */
  objPo data;			/* The data value */
} anyRec, *anyPo;

static inline logical IsAny(objPo p)
{
  return Tag(p)==anyMarker;
}

static inline objPo AnySig(objPo p)
{
  assert(IsAny(p));
  
  return ((anyPo)p)->sig;
}

static inline objPo AnyData(objPo p)
{
  assert(IsAny(p));
  
  return ((anyPo)p)->data;
}

#define AnyVal(p) ((anyPo)p)	/* point to the tuple structure */
#define AnyCellCount CellCount(sizeof(anyRec))

typedef enum{function=0,procedure,invalid_code} entrytype;

typedef struct _code_record_ {
  long sign;			/* Signature of the code segment */
  unsigned long size;		/* Number of instruction words */
  unsigned long litcnt;		/* Number of literals -- including the signatures */
  long formity;			/* Arity and form of the program */
  unsigned long spacereq;	/* Approx. number of words of space needed to execute */
  objPo type;			/* Type signature string */
  objPo frtype;			/* Type signature of free variables */
  instruction data[0];		/* Instruction words */
} codeRec, *codePo;

static inline logical IsCode(objPo p)
{
  return Tag(p)==codeMarker;
}

static inline codePo CodeVal(objPo p)
{
  return (codePo)(p);
}

#define CodeMark objectMark(codeMarker,0)
#define CodeCellCount(code,lits) CellCount(sizeof(codeRec)+lits*CLLSZE+code*sizeof(long))

static inline instruction *CodeCode(objPo pc)
{
  assert(IsCode(pc));
  return &(CodeVal(pc)->data[0]);
}

static inline long CodeLength(objPo p)
{
  assert(IsCode(p));
  
  return SignVal(p);
}

static inline long CodeSpaceReq(objPo p)
{
  assert(IsCode(p));
  
  return CodeVal(p)->spacereq;
}

static inline entrytype CodeForm(objPo p)
{
  assert(IsCode(p));
  
  return ((entrytype)(((CodeVal(p)->formity)>>8)&0xff));
}

#define SetCodeFormArity(pc,form,arity) CodeVal(pc)->formity = CodeFormity(form,arity)
#define CodeFormity(form,arity) ((((form)&0xff)<<8)|((arity)&0xff))

static inline unsigned int CodeArity(objPo p)
{
  assert(IsCode(p));
  
  return (CodeVal(p)->formity)&0xff;
}

static inline unsigned long CodeSize(objPo p)
{
  assert(IsCode(p));
  
  return CodeVal(p)->size;
}

static inline unsigned long CodeLitcnt(objPo p)
{
  assert(IsCode(p));
  
  return CodeVal(p)->litcnt;
}

static inline objPo *CodeLits(objPo p)
{
  assert(IsCode(p));
  
  {
    codePo pc = CodeVal(p);
    
    return (objPo*)(&pc->data[pc->size]);
  }
}

static inline objPo CodeSig(objPo p)
{
  assert(IsCode(p));
  
  return CodeVal(p)->type;
}

static inline objPo CodeFrSig(objPo p)
{
  assert(IsCode(p));
  
  return CodeVal(p)->frtype;
}

extern inline logical isClosure(objPo t)
{
  return isCons(t) && IsCode(consFn(t));
}

extern inline objPo codeOfClosure(objPo t)
{
  assert(isClosure(t));
  
  return consFn(t);
}

extern inline objPo *codeFreeVector(objPo p)
{
  assert(isClosure(p));
  
  return consData(p);
}


objPo allocateCode(integer size,integer count);
void updateCodeLit(objPo pc,uinteger offset,objPo el);
void updateCodeSig(objPo pc,objPo el);
void updateCodeFrSig(objPo pc,objPo el);

typedef struct _forward_record_ {
  integer sign;			/* Signature of a forwarded pointer */
  objPo fwd;
} forwardRec, *forwardPo;

#define Forwarded(p) (Tag(p)==forwardMarker)
#define FwdVal(p) (((forwardPo)p)->fwd)

typedef struct _handle_record_ {
  integer sign;			/* signature of a handle structure */
  processpo p;			/* Is there a live process associated with this? */
  uniChar name[0];			/* Name of the handle */
} handleRec, *handlePo;  

#define IsHdl(p) (Tag(p)==handleMarker)
#define HdlVal(p) ((handlePo)(p))
#define HdlCellLength(p) SignVal(p)
#define hdlMark(len) objectMark(handleMarker,len)
#define HdlCellCount(count) CellCount(sizeof(handleRec)+(count+1)*sizeof(uniChar))

objPo allocateHdl(processpo p,uniChar *name);

/* An opaque boxed C pointer */
typedef struct _opaque_record_ {
  integer sign;			/* Signature of the opaque block */
  void *data;			/* The value itself */
} opaqueRec, *opaquePo;

#define OPAQUE_SHIFT 8
#define OPAQUE_MASK ((1<<OPAQUE_SHIFT)-1)

#define IsOpaque(p) (Tag(p)==opaqueMarker)
#define OpaqueType(p) ((short int)(SignVal(p)&OPAQUE_MASK))
#define OpaqueVal(p) (((opaquePo)(p))->data)

#define OpaqueCellCount() CellCount(sizeof(opaqueRec))
#define OpaqueMark(type) objectMark(opaqueMarker,type)


#endif

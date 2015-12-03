/*
  Escapes which implement set and list processing operations
  (c) 1994-1998 Imperial College and F.G.McCabe

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
#include "april.h"
#include "process.h"
#include "setops.h"		/* Set manipulation interface */
#include "symbols.h"		/* standard April symbols */
#include "astring.h"

/* return the first N elements of a list */
retCode m_front(processpo p,objPo *args)
{
  objPo t1 = args[1];
  register objPo t2 = args[0];
  WORD32 len = IntVal(args[0]);
  WORD32 pos=0;
  if(!IsList(t1))
    return liberror("front",2,"1st argument should be a list",einval);
  else if(!IsInteger(t2)||len<0)
    return liberror("front",2,"2nd argument should a positive integer",einval);
  else{
    objPo last = args[1] = emptyList;
    objPo elmnt = emptyList;
    void *root = gcAddRoot(&last);

    gcAddRoot(&t1);
    gcAddRoot(&elmnt);

    while(pos++<len && !isEmptyList(t1)){
      elmnt = ListHead(t1);
      elmnt = allocatePair(&elmnt,&emptyList);

      if(last==emptyList)
	last = args[1] = elmnt;
      else{
	updateListTail(last,elmnt);
	last = elmnt;
      }
      t1 = ListTail(t1);
    }

    gcRemoveRoot(root);
    return Ok;
  }
}

/* return the last N elements of a list */
retCode m_back(processpo p,objPo *args)
{
  register objPo t1 = args[1];
  register objPo t2 = args[0];
  WORD32 pos = IntVal(t2);
  WORD32 len=0;

  if(!IsList(t1))
    return liberror("back",2,"1st argument should be a list",einval);
  else if(!IsInteger(t2)||pos<0)
    return liberror("back",2,"2nd argument should be a positive integer",einval);

  while(isNonEmptyList(t1)){ /* Count the length of the list */
    len++;
    t1=ListTail(t1);
  }

  t1 = args[1];			/* Now we step over len-pos elements */
  pos = len-pos;

  while(isNonEmptyList(t1) && pos-->0)
    t1=ListTail(t1);

  args[1]=t1;			/* Assign the result tail */
  return Ok;
}

/* return the list after the first N elements */
retCode m_tail(processpo p,objPo *args)
{
  register objPo t1 = args[1];
  register objPo t2 = args[0];
  WORD32 pos = IntVal(t2);

  if(!IsList(t1))
    return liberror("tail",2,"1st argument should be a list",einval);
  else if(!IsInteger(t2)||pos<0)
    return liberror("tail",2,"2nd argument should be a positive integer",einval);

  while(isNonEmptyList(t1) && pos-->0)
    t1 = ListTail(t1);

  args[1] = t1;			/* Assign the result tail */
  return Ok;
}

/* Length of a list */
retCode m_listlen(processpo p,objPo *args)
{
  args[0] = allocateInteger(ListLen(args[0]));
  return Ok;
}

WORD32 ListLen(objPo lst)
{
  WORD32 len = 0;
  while(isNonEmptyList(lst)){
    lst = ListTail(lst);
    len++;
  }
  if(isEmptyList(lst))
    return len;
  else
    return -len;		/* non-null terminated list */
}

 /* First element of a list */
retCode m_head(processpo p,objPo *args)
{
  if(isNonEmptyList(args[0])){
    args[0] = ListHead(args[0]);	/* Pick off the head of the list */
    return Ok;
  }
  else
    return liberror("head",1,"argument should be a non-empty list",einval);
}

/* Index the nth element of a list */
retCode m_nth(processpo p,objPo *args)
{
  register objPo lst = args[1];
  objPo a2 = args[0];
  register int off;
  
  if(!IsList(lst))
    return liberror("#",2,"1st argument should be a list",einval);
  else if(!IsInteger(a2) || (off=IntVal(a2))<=0)
    return liberror("#",2,"2nd argument should be a positive integer",einval);

  while(--off>0 && isNonEmptyList(lst))
    lst = ListTail(lst);

  if(isNonEmptyList(lst)){
    args[1] = ListHead(lst);
    return Ok;
  }
  else
    return liberror("[]",2,"index greater than length of list",einval);
}

/* list append 
 * The somewhat tricky implementation is to ensure that GC tracing can find 
 * the input while we are building the output
 */
retCode m_app(processpo p,objPo *args)
{
  objPo t1 = args[1];
  objPo t2 = args[0];

  if(!IsList(t1))
    return liberror("<>",2,"1st argument should be a list",einval);
  else if(!IsList(t2))
    return liberror("<>",2,"2nd argument should be a list",einval);
  else{
    objPo oset = emptyList;
    objPo last = emptyList;
    objPo elmnt = emptyList;
    void *root = gcAddRoot(&oset);

    gcAddRoot(&t1);
    gcAddRoot(&elmnt);
    gcAddRoot(&last);

    while(isNonEmptyList(t1)){
      elmnt = ListHead(t1);
      elmnt = allocatePair(&elmnt,&emptyList);

      if(last==emptyList)
	last = oset = elmnt;
      else{
	updateListTail(last,elmnt);
	last = elmnt;
      }

      t1 = ListTail(t1);
    }
    if(last==emptyList)
      args[1] = args[0];
    else{
      updateListTail(last,args[0]);
      args[1] = oset;
    }
    gcRemoveRoot(root);
    return Ok;
  }
}

/* Generate a list of integers */
retCode m_iota(processpo p,objPo *args)
{
  register objPo t1 = args[1];
  register objPo t2 = args[0];

  if(!IsInteger(t1))
    return liberror("..",2,"1st argument should be an integer",einval);
  else if(!IsInteger(t2))
    return liberror("..",2,"2nd argument should be an integer",einval);
  else{
    register WORD32 mn=IntVal(t1);
    register WORD32 mx=IntVal(t2);
    objPo last = emptyList;
    objPo i = emptyList;
    void *root = gcAddRoot(&last);

    gcAddRoot(&i);
  
    args[1] = emptyList;

    for(;mn<=mx;mn++){
      i = allocateInteger(mn);

      i = allocatePair(&i,&emptyList);

      if(last==emptyList)
	args[1] = last = i;
      else{
	updateListTail(last,i);
	last = i;
      }
    }

    gcRemoveRoot(root);
    return Ok;
  }
}

/* Generate a list of integers -- with step value */
retCode m_iota3(processpo p,objPo *args)
{
  register Number mn = NumberVal(args[2]);
  register Number mx = NumberVal(args[1]);
  register Number st = NumberVal(args[0]);
  
  if(!st==0.0)
    return liberror("iota",2,"3rd argument should be non-zero",einval);
  else{
    objPo out = emptyList;
    objPo i = emptyList;
    objPo last = emptyList;
    void *root = gcAddRoot(&out);

    gcAddRoot(&i);
    gcAddRoot(&last);

    if(st>0.0)
      for(;mn<mx;mn+=st){
	i = allocateNumber(mn);
	i = allocatePair(&i,&emptyList);

	if(last==emptyList)
	  out = last = i;
	else{
	  updateListTail(last,i);
	  last = i;
	}
      }
    else
      for(;mn>mx;mn+=st){
	i = allocateNumber(mn);
	i = allocatePair(&i,&emptyList);

	if(last==emptyList)
	  out = last = i;
	else{
	  updateListTail(last,i);
	  last = i;
	}
      }

    args[2] = out;		/* return the list */
    gcRemoveRoot(root);
    return Ok;
  }
}

retCode equalcell(register objPo c1,register objPo c2)
{
  if(c1==c2)
    return Ok;
  else switch(Tag(c1)){
  case integerMarker:
    switch(Tag(c2)){
    case integerMarker:
      if(IntVal(c1)==IntVal(c2))
	return Ok;
      else
	return Fail;
    case floatMarker:
      if(FloatVal(c2)==(Number)IntVal(c1))
	return Ok;
      else
	return Fail;
    default:
      return Fail;
    }
      
  case symbolMarker:
    if(isSymb(c2) && c1==c2)
      return Ok;
    else
      return Fail;
  case charMarker:
    if(isChr(c2) && CharVal(c1)==CharVal(c2))
      return Ok;
    else
      return Fail;
  case floatMarker:
    switch(Tag(c2)){
    case integerMarker:
      if(FloatVal(c1)==(Number)IntVal(c2))
	return Ok;
      else
	return Fail;
    case floatMarker:
      if(FloatVal(c1)==FloatVal(c2))
	return Ok;
      else
	return Fail;
    default:
      return Fail;
    }

  case codeMarker:
    return Error;
  case listMarker:
    if(IsList(c2)){
      while(isNonEmptyList(c1)){
        if(isNonEmptyList(c2)){
          objPo *l1 = ListData(c1);
          objPo *l2 = ListData(c2);
          retCode ret = equalcell(*l1++,*l2++);

          if(ret==Ok){
            c1 = *l1;
            c2 = *l2;
          }
          else
            return ret;
        }
        else
          return Fail;
      }
      if(isEmptyList(c1) && isEmptyList(c2))
        return Ok;
      else
        return Fail;
    }
    else
      return Fail;

  case consMarker:
    if(IsHandle(c1)){
      if(IsHandle(c2) && sameHandle(c1,c2))
	return Ok;
      else
	return Fail;
    }
    else if(c1==kvoid || c2==kvoid)
      return Fail;
    else if(isClosure(c1)||isClosure(c2))
      return Error;
    else if(isCons(c2) && equalcell(consFn(c1),consFn(c2))==Ok){
      register unsigned WORD32 i= consArity(c1);
      objPo *t1 = consData(c1);
      objPo *t2 = consData(c2);
      
      if(i!=consArity(c2))
	return Fail;

      while(i--){
	retCode ret = equalcell(*t1++,*t2++);

	if(ret!=Ok)
	  return ret;
      }
      return Ok;
    }
    else
      return Fail;
  
    case tupleMarker:
    if(IsTuple(c2)){
      register unsigned WORD32 i= tupleArity(c1);
      objPo *t1 = tupleData(c1);
      objPo *t2 = tupleData(c2);
      
      if(i!=tupleArity(c2))
	return Fail;

      while(i--){
	retCode ret = equalcell(*t1++,*t2++);

	if(ret!=Ok)
	  return ret;
      }
      return Ok;
    }
    else
      return Fail;

  case anyMarker:
    if(IsAny(c2))
      return equalcell(AnyData(c1),AnyData(c2));
    else
      return Fail;

  case handleMarker:		/* taken care of by the c1==c2 test */
    return Fail;
  default:
    return Error;
  }
}

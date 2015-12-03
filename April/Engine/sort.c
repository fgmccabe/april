/*
  A sort function for use as an April escape function
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

  Uses quicksort algorithm over lists
*/
#include "config.h"		/* pick up standard configuration header */
#include "april.h"
#include "process.h"
#include "setops.h"
#include "sort.h"
#include "astring.h"
#include <string.h>

static void qs(objPo *first,objPo *last);

retCode m_sort(processpo p,objPo *args)
{
  register objPo in = args[0];
  WORD32 len=ListLen(args[0]);
  objPo vect[len];
  WORD32 i=0;
  void *root=NULL;

  while(isNonEmptyList(in)){
    if(i==0)
      root = gcAddRoot(&vect[i]);
    else
      gcAddRoot(&vect[i]);
    vect[i++]=ListHead(in);	/* copy the list into a table for efficient sorting */
    
    in = ListTail(in);		/* push list elements onto read stack */
  }

  if(!isEmptyList(in)){
    gcRemoveRoot(root);
    return liberror("sort",1,"argument should be a list",einval);
  }

  /* Now sort the elements directly */
  qs(&vect[0],&vect[len-1]);

  {
    objPo last = args[0] = emptyList;

    gcAddRoot(&last);

    for(i=0;i<len;i++)
      if(last==emptyList)
	last = args[0] = allocatePair(&vect[i],&emptyList);
      else{
	objPo tail = allocatePair(&vect[i],&emptyList);
	updateListTail(last,tail);
	last=tail;
      }

    gcRemoveRoot(root);
    return Ok;
  }
}

#define swapcell(a,b) {objPo c = (*a); (*a)=(*b); (*b) = c;}

/* Sort the data on the cell stack directly */
static void qs(objPo *first,objPo *last)
{
  objPo pivot = *first;
  objPo *less = first;
  objPo *greater = last+1;

  if(first<last){		/* we have something to sort... */

    /* split the tuple into two parts... */
    do{
      do{
	less++;
      } while(less<greater && cmpcell(*less,pivot)<=0); /* advance bottom while below pivot value */
    
      do{
	greater--;
	} while(greater>=less && cmpcell(*greater,pivot)>0); /* decrement top while above pivot */

      if(less<greater)		/* have we collided yet? */
	swapcell(less,greater);
    } while(less<greater);

    swapcell(first,greater);	/* place pivot into correct final position */

    qs(first,greater-1);
    qs(greater+1,last);		/* recursion turned into a loop */
  }
}

/* Convert a list into a set ... */
retCode m_setof(processpo p,objPo *args)
{
  register objPo in = args[0];
  WORD32 len=ListLen(in);
  objPo vect[len];
  WORD32 i=0;
  void *root=NULL;

  while(isNonEmptyList(in)){
    if(i==0)
      root = gcAddRoot(&vect[i]);
    else
      gcAddRoot(&vect[i]);
    vect[i++]=ListHead(in);	/* copy the list into a table for efficient sorting */
    
    in = ListTail(in);		/* push list elements onto read stack */
  }

  if(!isEmptyList(in)){
    gcRemoveRoot(root);
    return liberror("_setof",1,"argument should be a list",einval);
  }

  /* Now sort the elements directly */
  qs(&vect[0],&vect[len-1]);


  {
    objPo last = args[0] = emptyList;
    WORD32 l = 0;

    gcAddRoot(&last);

    for(i=0;i<len;i++){
      if(i!=0 && cmpcell(vect[i],vect[l])==0) /* Identical elements? */
	continue;
      else{
	if(last==emptyList)
	  last = args[0] = allocatePair(&vect[i],&emptyList);
	else{
	  objPo tail = allocatePair(&vect[i],&emptyList);
	  updateListTail(last,tail);
	  last=tail;
	}
	l = i;			/* last unique element offset */
      }
    }
    gcRemoveRoot(root);
    return Ok;
  }
}

/* cmpcell: returns =0 if c1==c2, <0 if c1<c2 and >0 if c1>c2 */
int cmpcell(register objPo c1,register objPo c2)
{
  if(c1==NULL)
    return -1;
  else if(c2==NULL)
    return 1;
  else if(c1==c2)
    return 0;
  else{
    wordTag tg1 = Tag(c1);
    wordTag tg2 = Tag(c2);

    if(tg1!=tg2){
      if(tg1==integerMarker && tg2==floatMarker){
	if(IntVal(c1)<FloatVal(c2))
	  return -1;
	else if(IntVal(c1)>FloatVal(c2))
	  return 1;
	else
	  return 0;
      }
      else if(tg1==floatMarker && tg2==integerMarker){
	if(FloatVal(c1)<IntVal(c2))
	  return -1;
	else if (FloatVal(c1)>IntVal(c2))
	  return 1;
	else
	  return 0;
      }
      else
	return tg1-tg2;		/* This is arbitrary */
    }
    else
      switch(tg1){
      case integerMarker:
	return IntVal(c1)-IntVal(c2); 
      case floatMarker:{
	Number f1 = FloatVal(c1)-FloatVal(c2);
	return f1<0?-1:f1>0?1:0;
      }
      case symbolMarker:
	return uniCmp(SymVal(c1),SymVal(c2));
      case charMarker:{
        int i = CharVal(c1)-CharVal(c2);
        
        return i<0?-1:i>0?1:0;
      }
      case listMarker:{
	if(isEmptyList(c1)){
	  if(isEmptyList(c2))
	    return 0;
	  else
	    return -1;		/* Empty list is smaller than non-empty */
	}
	else if(isEmptyList(c2))
	  return 1;
	else{
	  int res = cmpcell(ListHead(c1),ListHead(c2));

	  if(res!=0)
	    return res;
	  else
	    return cmpcell(ListTail(c1),ListTail(c2));
	}
      }

      case consMarker:{
	register unsigned WORD32 i1= consArity(c1);
	register unsigned WORD32 i2= consArity(c2);
	
	if(i1<i2)
	  return -1;		/*  1st constructor is shorter than second*/
	else if(i1>i2)
	  return 1;		/* 1st constructor is longer than second */
	else{
	  objPo *e1 = consData(c1);
	  objPo *e2 = consData(c2);
	  
	  if((i2=cmpcell(consFn(c1),consFn(c2)))!=0)
	    return i2;
	  while(i1--){
	    if((i2=cmpcell(*e1++,*e2++))!=0)
	      return i2;
	  }
	  return 0;		/* Two tuples are equal */
	}
      }

      case tupleMarker:{
	register WORD32 i1= tupleArity(c1);
	register WORD32 i2= tupleArity(c2);
	if(i1<i2)
	  return -1;		/*  1st tuple is shorter than second*/
	else if(i1>i2)
	  return 1;		/* 1st tuple is longer than second */
	else{
	  objPo *e1 = tupleData(c1);
	  objPo *e2 = tupleData(c2);
	  while(i1--){
	    if((i2=cmpcell(*e1++,*e2++))!=0)
	      return i2;
	  }
	  return 0;		/* Two tuples are equal */
	}
      }

      case anyMarker:
	return cmpcell(AnyData(c1),AnyData(c2));

      case codeMarker:		/* Code is never comparable */
      case forwardMarker:
	return -1;
      default:
	syserr("unfortunate problem in cmpcell");
      }
  }
  return 0;			/* should never get here */
}


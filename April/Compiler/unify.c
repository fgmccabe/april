/* 
  Type unification with recursive types requires 
  a trace to be constructed -- which gives the current proof
  (c) 1994-2000 Imperial College and F.G.McCabe

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

#include "config.h"
#include <stdlib.h>		/* Standard functions */
#include <assert.h>
#include "compile.h"		/* Compiler structures */
#include "types.h"		/* Type cheking interface */
#include "cellP.h"
#include "typesP.h"		/* Private data structures for type checking */
#include "keywords.h"		/* Standard April keywords */

lstPo topReset = NULL;

static void clearBindings(lstPo last);

logical unifyType(cellpo t1,cellpo t2,tracePo lscope,tracePo rscope,
		  logical inForall,cellpo *f1,cellpo *f2)
{
  if(deRef(t1)==deRef(t2))	/* Identical types are equal */
    return True;
  else{
    cellpo lhs1,lhs2,rhs1,rhs2;
    unsigned long ar1,ar2;
    cellpo T1 = deRef(t1);
    cellpo T2 = deRef(t2);

    if(T1==T2)
      return True;
    else if(IsQuery(T1,&lhs1,&rhs1) && IsQuery(T2,&lhs2,&rhs2))
      return unifyType(rhs1,rhs2,lscope,rscope,inForall,f1,f2) &&
	unifyType(lhs1,lhs2,lscope,rscope,inForall,f1,f2);
    else if(IsTypeVar(T1))	/* try binding type variable on left */
      return bindType(T1,T2);
    else if(IsTypeVar(T2))	/* try binding to the right */
      return bindType(T2,T1);

    /* We have to be able to unify two universally quantified formulae */
    else if(IsForAll(T1,NULL,NULL) || IsForAll(T2,NULL,NULL)){
      if(IsForAll(T1,NULL,NULL) && IsForAll(T2,NULL,NULL)){
        cellpo left[16];		/* the left quantified variables */
        unsigned int lTop = 0;
        cellpo right[16];		/* right quantified variables */
        unsigned int rTop = 0;
        
        while(IsForAll(T1,&left[lTop],&T1)){
          if(lTop<NumberOf(left)){
            left[lTop]=deRef(left[lTop]);
            lTop++;
          }
          else{
            reportErr(lineInfo(T1),"Too many quantifiers: %w",T1);
            *f1 = T1; *f2 = T2;
            return False;
          }
        }
        while(IsForAll(T2,&right[rTop],&T2)){
          if(rTop<NumberOf(right)){
            right[rTop]=deRef(right[rTop]);
            rTop++;
          }
          else{
            reportErr(lineInfo(T2),"Too many quantifiers: %w",T2);
            *f1 = T1; *f2 = T2;
            return False;
          }
        }
        if(inForall<=(lTop==rTop)){
          if(unifyType(T1,T2,lscope,rscope,True,f1,f2)){
            unsigned int i;		/* we must now check the quantified variables*/
            
            if(inForall){		        /* only check if not subsumption test */
              for(i=0;i<lTop;i++){
                if(left[i]!=NULL){ /* there must a right variable bound to this */
                  if(IsTypeVar(deRef(left[i]))){
                    unsigned int j;
                    for(j=0;j<rTop;j++)
                      if(right[j]!=NULL && deRef(right[j])==deRef(left[i])){
                        left[i] = right[j] = NULL; /* cancel both variables */
                        break;
                      }
                  }
                  else{		/* quantified variable got bound */
                    *f1 = T1; *f2 = T2;
                    return False;
                  }
                }
              }
            }
            for(i=0;i<rTop;i++){
              if(right[i]!=NULL){	/* there must a left variable bound to this */
                if(IsTypeVar(deRef(right[i]))){
                  unsigned int j;
                  for(j=0;j<lTop;j++)
                    if(left[j]!=NULL && deRef(left[j])==deRef(right[i])){
                      left[i] = right[j] = NULL; /* cancel both variables */
                      break;
                    }
                }
                else{		/* quantified variable got bound */
                  *f1 = T1; *f2 = T2;
                  return False;
                }
              }
            }
            return True;		/* we were able to unify two quantified terms*/
          }
          else return False;
        }
        else{
          *f1 = T1; *f2 = T2;
          return False;
        }
      }
      else{
        *f1 = T1; *f2 = T2;
        return False;
      }
    }
    else if(isSymb(T1)){	/* Explicit or standard symbol */
      if(isSymb(T2)){
	symbpo k1 = symVal(T1);
	symbpo k2 = symVal(T2);

	if(k1==k2)
	  return True;
	else{
	  *f1 = T1;
	  *f2 = T2;
	  return False;
	}
      }
      else{
	*f1 = T1;
	*f2 = T2;
	return False;
      }
    }

    else if(isSymb(T2)){
      *f1 = T1;
      *f2 = T2;
      return False;
    }

    else if(isListType(T1,&lhs1)){ /* This is how lists are specified ... */
      if(isListType(T2,&lhs2))
	return unifyType(lhs1,lhs2,lscope,rscope,inForall,f1,f2);
      else{
	*f1 = T1;
	*f2 = T2;
	return False;
      }
    }

    else if(isCons(T1) && isCons(T2) && consArity(T1)==consArity(T2)){
      unsigned long i;
      unsigned long ar = consArity(T1);
      
      if(!unifyType(consFn(T1),consFn(T2),lscope,rscope,False,f1,f2))
        return False;

      for(i=0;i<ar;i++)
	if(!unifyType(consEl(T1,i),consEl(T2,i),lscope,rscope,inForall,f1,f2))
	  return False;

      return True;
    }
    else if(IsTuple(T1,&ar1) && IsTuple(T2,&ar2) && ar1==ar2){
      unsigned int i;

      for(i=0;i<ar1;i++)
	if(!unifyType(tplArg(T1,i),tplArg(T2,i),lscope,rscope,inForall,f1,f2))
	  return False;

      return True;
    }

    else{
      *f1 = T1;
      *f2 = T2;
      return False;		/* No other possiblities */
    }
  }    
}

/* Bind a type variable ... */
logical bindType(cellpo var,cellpo type)
{
  assert(refVal(var)==NULL||refVal(var)==var);

  if(var!=(type=deRef(type))){
    if(occInType(var,type))	/* occurs check */
      return False;

    topReset=extendNonG(var,topReset);

    copyCell(var,type);		/* Bind the type variable */
  }
  return True;
}

static void clearBindings(lstPo last)
{
  lstPo top = topReset;

  while(topReset!=last){
    lstPo tail = topReset->tail;

    NewTypeVar(topReset->type);
    topReset = tail;
  }

  popLst(top,last);
}


logical sameType(cellpo t1,cellpo t2)
{
  lstPo last = topReset;
  cellpo f1,f2;

  if(!unifyType(t1,t2,NULL,NULL,False,&f1,&f2)){
    clearBindings(last);
    return False;
  }
  return True;
}

logical checkType(cellpo t1,cellpo t2,cellpo *f1,cellpo *f2)
{
  lstPo last = topReset;
  cellpo F1,F2;

  if(f1==NULL)
    f1 = &F1;
  if(f2==NULL)
    f2 = &F2;

  if(!unifyType(t1,t2,NULL,NULL,False,f1,f2)){
    clearBindings(last);
    return False;
  }
  return True;
}


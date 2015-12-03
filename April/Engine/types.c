/*
  Type unification 
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
#include "config.h"		/* pick up standard configuration header */
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "april.h"
#include "ioP.h"
#include "astring.h"
#include "sign.h"
#include "symbols.h"
#include "process.h"
#include "pool.h"


typedef struct _type_var_ *chainPo;

typedef struct _type_var_ {
  objPo var;
  chainPo prev;
} ChainRec;

static inline chainPo createChain(objPo v,chainPo ch);

static inline logical boundVar(objPo var,chainPo bound)
{
  var = deRefVar(var);
  
  while(bound!=NULL){
    if(bound->var==var)
      return True;
    else
      bound = bound->prev;
  }
  return False;
}

static chainPo resets = NULL;

void bindVar(objPo var,objPo val)
{
  assert(IsVar(var));
  
  {
    resets = createChain(var,resets);
    updateVariable(var,val);
  }
}

static poolPo chainPool = NULL;

static chainPo createChain(objPo var,chainPo chain)
{
  chainPo ch = (chainPo)allocPool(chainPool);
  
  ch->var = var;
  ch->prev = chain;
  
  return ch;
}

  
static retCode match(objPo left,chainPo lbound,objPo right,chainPo rbound)
{
  left = deRefVar(left);
  right = deRefVar(right);

  if(IsVar(right)){             // Handl a variable on the right first
    if(boundVar(right,rbound)){
      if(IsVar(left)){          // lhs is a variable
        if(boundVar(left,lbound))
          return Ok;            //  slight cheat
        else{
          bindVar(left,right);
          return Ok;
        }
      }
      else
        return Fail;            // lhs is not a bound var, so must fail
    }
    else{
      bindVar(right,left);
      return Ok;
    }
  }
  else if(IsVar(left)){
    if(boundVar(left,lbound))
      return Fail;              // rhs wasn't a variable
    else{
      bindVar(left,right);
      return Ok;
    }
  }
  else{
    switch(Tag(left)){
      case symbolMarker:
        if(left==right)
          return Ok;
        else
          return Fail;
      case tupleMarker:{
        unsigned WORD32 ar = tupleArity(left);
        
        if(!IsTuple(right) || tupleArity(right)!=ar)
          return Fail;
        else{   
          unsigned WORD32 i;
          
          for(i=0;i<ar;i++){
            retCode subret = match(tupleArg(left,i),lbound,tupleArg(right,i),rbound);
            
            if(subret!=Ok)
              return subret;
          }
          return Ok;
        }
      }
      
      case consMarker:{
        if(!isCons(right) || consFn(left)!=consFn(right))
          return Fail;
        else{
          if(consFn(left)==kallQ){
            ChainRec nLeft = { deRefVar(consEl(left,0)), lbound};
            ChainRec nRight = { deRefVar(consEl(right,0)), rbound};
            
            if(!IsVar(nLeft.var) || !IsVar(nRight.var))
              return Fail;
            else
              return match(consEl(left,1),&nLeft,consEl(right,1),&nRight);
          }
          else{
            unsigned WORD32 ar = consArity(left);
            
            if(ar!=consArity(right))
              return Fail;
            else{
              unsigned WORD32 i;
              
              for(i=0;i<ar;i++){
                retCode subret = match(consEl(left,i),lbound,consEl(right,i),rbound);
                
                if(subret!=Ok)
                  return subret;
              }
              return Ok;
            }
          }
        }
      }
      
      default:
        return Error;           // Should never happen
    }
  }
}

void clearResets(void)
{
  while(resets!=NULL){
    chainPo p = resets->prev;
    
    freePool(chainPool,resets);
    
    resets = p;
  }
}

void undoUnify(void)
{
  while(resets!=NULL){
    chainPo p = resets->prev;
    
    assert(IsVar(resets->var));
    
    ((variablePo)resets->var)->val = resets->var;
    
    freePool(chainPool,resets);
    
    resets = p;
  }
}

retCode unifyTypes(objPo left,objPo right)
{
  if(chainPool==NULL)
    chainPool = newPool(sizeof(ChainRec),64);
    
  {
    retCode ret = match(left,NULL,right,NULL);
    
    if(ret!=Ok)
      undoUnify();
    return ret;
  }
}
  
retCode m_match(processpo p,objPo *args)
{
  objPo left=args[1];
  objPo right=args[0];

  switch(unifyTypes(left,right)){
  case Ok:
    args[1] = ktrue;		/* A successful match */
    return Ok;
  case Fail:
    args[1] = kfalse;		/* An unsuccessful match */
    return Ok;
  default:
    return liberror("_match", 2,"Invalid type signature(s)",einval);
  }
}

unsigned WORD32 progTypeArity(objPo type)
{
  while(isBinCall(type=deRefVar(type),kallQ,NULL,&type))
    ;
  if(isConstructor(type,kprocTp))
    return consArity(type);
  else if(isBinCall(type,kfunTp,&type,NULL))
    return progTypeArity(type);
  else if(isConstructor(type,ktplTp))
    return consArity(type);
  else if(IsTuple(type))
    return tupleArity(type);
  else
    return 1;
}
    

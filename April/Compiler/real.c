/*
  Compute the real type of a type expression
  Used mostly for converting a user's type expression into one we can process
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
#include <string.h>
#include "compile.h"		/* Compiler structures */
#include "cellP.h"
#include "types.h"		/* Type cheking interface */
#include "typesP.h"		/* Private data structures for type checking */
#include "keywords.h"		/* Standard April keywords */

/*
 * Construct a `real' type expression from a parse representation of a type
 */
static logical realTp(cellpo input,envPo *env,envPo *tvars,tracePo scope,cellpo tgt)
{
  cellpo lhs,rhs,fn;
  cellpo type;
  unsigned long ar;

  if(IsHashStruct(input=deRef(input),knodebug,&input)) /* Strip off the debugging stuff */
    return realTp(input,env,tvars,scope,tgt);
  else if(IsHashStruct(input,kdebug,&input))
    return realTp(input,env,tvars,scope,tgt);

  else if(isSymb(input)){	/* We have a symbol ... */
    symbpo s = symVal(input);
    
    // Standard types
    if(s==knumber){
      copyCell(tgt,numberTp);
      return True;
    }
    else if(s==ksymbol){
      copyCell(tgt,symbolTp);
      return True;
    }
    else if(s==kchar){
      copyCell(tgt,charTp);
      return True;
    }
    else if(s==klogical){
      copyCell(tgt,logicalTp);
      return True;
    }
    else if(s==kany){
      copyCell(tgt,anyTp);
      return True;
    }
    else if(s==khandle){
      copyCell(tgt,handleTp);
      return True;
    }
    else if(s==kstring){
      copyCell(tgt,stringTp);
      return True;
    }
    else if(s==kuscore){	/* Each occ of _ is a fresh typ var */
      NewTypeVar(tgt);
      return True;
    }
    else if((type=pickupTypeRename(input,*env))!=NULL){
      cellpo lhs,rhs;

      freshType(type,*env,tgt);

      if(isBinaryCall(tgt,ktpname,&lhs,&rhs) && IsSymbol(deRef(lhs),kblock)){
	copyCell(tgt,rhs);
	return True;
      }
      else{
	copyCell(tgt,rhs);
	return False;
      }
    }
    else if((type=pickupTypeDef(input,*env))!=NULL){
      freshType(type,*env,tgt);
      if(isBinaryCall(type,ktype,&lhs,&rhs) && IsSymbol(deRef(lhs),kblock)){
	copyCell(tgt,rhs);
	return True;
      }
      else
	return False;
    }
    else{
      copyCell(tgt,input);	/* return symbol */
      return False;		/* not a known type symbol */
    }
  }

  else if(isUnaryCall(input,kbkets,&lhs)){
    cellpo El=allocSingle();
    logical ok = realTp(lhs,env,tvars,scope,El);
    BuildListType(El,tgt);
    return ok;
  }

  else if(IsTypeVar(input)){
    copyCell(tgt,input);
    return True;
  }

  else if(IsTvar(input,&rhs)){
    cellpo type;
    if((type=pickupType(input,*env))!=NULL||
       (type=pickupType(input,*tvars))!=NULL){
      copyCell(tgt,type);
      return True;
    }
    else{			/* a `random' new type variable... */
      cellpo var = NewTypeVar(tgt);
      *env = newEnv(input,var,*env);
      *tvars = newEnv(input,var,*tvars);
      return True;
    }
  }

  else if(isUnaryCall(input,ktypeof,&input)){
    whichType(input,env,*env,tgt,False);
    return True;
  }

  else if(isUnaryCall(input,ktof,&input)){
    whichType(input,env,*env,tgt,False);
    return True;
  }

  else if(isBinaryCall(input,kdefn,&lhs,&rhs)||
	  isBinaryCall(input,kfield,&lhs,&rhs)){
    reportErr(lineInfo(input),"Default values %w no longer supported",input);
    whichType(rhs,env,*env,tgt,False);
    return True;
  }

  else if(isBinaryCall(input,kquery,&lhs,&rhs)){
    BuildBinCall(kquery,voidcell,rhs,tgt);
    return realTp(lhs,env,tvars,scope,callArg(tgt,0));
  }

  else if(isUnaryCall(input,kquery,&rhs)){
    NewTypeVar(tgt);
    
    BuildBinCall(kquery,tgt,rhs,tgt);
    return True;
  }

  else if(IsTuple(input,NULL)){	/* Regular tuple */
    long ar = tplArity(input);
    long i;
    logical ok = True;

    mkTpl(tgt,allocTuple(ar));	/* create a copy of the tuple */

    for(i=0;i<ar;i++)
      ok = ok&realTp(tplArg(input,i),env,tvars,scope,tplArg(tgt,i));

    return ok;
  }

  else if(isBinaryCall(input,kfn,&lhs,&rhs)){
    cellpo fun = BuildFunType(voidcell,voidcell,tgt);
    return realTp(tupleIze(lhs),env,tvars,scope,callArg(fun,0)) &
      realTp(rhs,env,tvars,scope,callArg(fun,1));
  }

  else if(isConstructorType(input,&lhs,&rhs)){
    cellpo fun = BuildBinCall(kconstr,voidcell,voidcell,tgt);
    realTp(lhs,env,tvars,scope,callArg(fun,0));
    return realTp(rhs,env,tvars,scope,callArg(fun,1));
  }

  else if(isCall(input,kblock,&ar)){
    cellpo tp = BuildProcType(ar,tgt);
    logical ok = True;
    long i;
    
    for(i=0;i<ar;i++)
      ok = ok&realTp(consEl(input,i),env,tvars,scope,consEl(tp,i));
    
    return ok;
  }

  else if(isBinaryCall(input,kforall,&lhs,&rhs)){
    if(IsTvar(lhs,NULL)){
      cellpo res = BuildForAll(voidcell,voidcell,tgt);
      cellpo var = NewTypeVar(callArg(res,0)); 
      envPo nEnv = newEnv(lhs,var,*env);
      logical ok = realTp(rhs,&nEnv,tvars,scope,callArg(res,1));

      popEnv(nEnv,*env);
      return ok;
    }
    else{
      reportErr(lineInfo(input),"Quantifier %w should be a type variable",lhs);
      NewTypeVar(tgt);
      return False;
    }
  }

  else if(isBinaryCall(input,kdot,&lhs,&rhs) && IsTvar(lhs,NULL)){
    reportErr(lineInfo(input),"Fixpoint type %w no longer supported",input);
    copyCell(tgt,input);
    return False;
  }

  /* A user defined type ... ? */
  else if(isCons(input) && isSymb(fn=consFn(input))){
    cellpo args = tupleOfArgs(input,allocSingle());
    cellpo type=pickupTypeDef(fn,*env);
    cell At;

    realTp(args,env,tvars,scope,&At);

    if(type!=NULL){
      cellpo tt=NULL;
      cellpo ta=NULL;

      type=freshType(type,*env,allocSingle());

      assert(isBinaryCall(type,ktype,NULL,NULL));
      isBinaryCall(type,ktype,&ta,&tt);
      
      if(isTpl(ta=deRef(ta))){
        if(sameType(&At,ta)){
          copyCell(tgt,tt);
          return True;
        }
        else{
	  reportErr(lineInfo(input),"type arguments not compatible with actual"
		    "type");
	  copyCell(tgt,input);
	  return False;
	}
      }
      else{
	reportErr(lineInfo(input),"type arguments not compatible with actual"
		  "type");
	copyCell(tgt,input);
	return False;
      }
    }
    else if((type=pickupTypeRename(fn,*env))!=NULL){
      freshType(type,*env,tgt);

      if(isBinaryCall(tgt,ktpname,&lhs,&rhs)){
	if(sameType(lhs,&At)){
	   copyCell(tgt,rhs);
	   return True;
	}
      }
      NewTypeVar(tgt);
      return False;
    }
    else{
      unsigned long ar = consArity(input);
      unsigned long i;
      cell OldInput = *input;
      
      BuildStruct(consFn(input),ar,tgt);
            
      for(i=0;i<ar;i++)
        realTp(consEl(&OldInput,i),env,tvars,scope,consEl(tgt,i));
      return False;		/* not actually known ... */
    }
  }
  else{
    copyCell(tgt,input);	/* no idea ... */
    return False;
  }
}
  
logical realType(cellpo exp,envPo env,cellpo tgt)
{
  envPo subE = env;
  envPo tvars = NULL;
  logical new = realTp(exp,&subE,&tvars,NULL,tgt);

  popEnv(subE,env);
  popEnv(tvars,NULL);

  return new;
}

logical typeExpression(cellpo exp,envPo env,envPo *tvars,cellpo tgt)
{
  envPo subE = env;
  logical new = realTp(exp,&subE,tvars,NULL,tgt);

  popEnv(subE,env);

  return new;
}


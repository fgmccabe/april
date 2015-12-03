/*
 * Type checker for April patterns
 * (c) 1994-1998 Imperial College and F.G.McCabe
 * All rights reserved
 *
 */
#include "config.h"
#include <stdlib.h>		/* Standard functions */
#include <assert.h>
#include "compile.h"		/* Compiler structures */
#include "cellP.h"		/* this is temporary */
#include "types.h"		/* Type cheking interface */
#include "typesP.h"		/* Private data structures for type checking */
#include "keywords.h"		/* Standard April keywords */

/* Patterns are similar to types; but a type can be a pattern... */
static cellpo ptnType(cellpo ptn,logical islhs,envPo *env,
		      envPo outer,cellpo tgt,logical inH);

cellpo pttrnType(cellpo ptn,logical islhs,envPo *env,envPo outer,
		 cellpo tgt,logical inH)
{
  cellpo type = ptnType(ptn,islhs,env,outer,tgt,inH);

#ifdef TYPETRACE_
  if(traceType)
    outMsg(logFile,"type of pattern `%5w' is `%5t'\n",ptn,type);
#endif

  copyLineInfo(ptn,type);

  return type;
}

cellpo typeIs(cellpo ptn,cellpo type)
{
  type = deRef(type);
  assert(ptn->type==NULL||ptn->type==type);

  setTypeInfo(ptn,type);
  return type;
}

static cellpo ptnType(cellpo ptn,logical islhs,envPo *env,
		      envPo outer,cellpo tgt,logical inH)
{
  cellpo a1,a2,args;
  unsigned long ar;

  if(IsHashStruct(ptn,knodebug,&ptn)) /* Strip off the debugging stuff */
    return ptnType(ptn,islhs,env,outer,tgt,inH);
  else if(IsHashStruct(ptn,kdebug,&ptn))
    return ptnType(ptn,islhs,env,outer,tgt,inH);

  else if(IsInt(ptn))		/* Integer literals are of type integer */
    return typeIs(ptn,copyCell(tgt,numberTp));

  else if(IsFloat(ptn))		/* Floating points are of type number */
    return typeIs(ptn,copyCell(tgt,numberTp));
    
  else if(isChr(ptn))          // Characters are of type char
    return typeIs(ptn,copyCell(tgt,charTp));

  else if(IsString(ptn))	/* Strings are also of fixed type */
    return typeIs(ptn,copyCell(tgt,stringTp));

  else if(isSymb(ptn)){		/* We have a symbol ... */
    symbpo s = symVal(ptn);

    if(s==kuscore){		/* Each occ of _ is a fresh typ var */
      setTypeInfo(ptn,NewTypeVar(tgt));

      return tgt;
    }
    else if(s==kdot)
      return typeIs(ptn,copyCell(tgt,stringTp));
    else{
      cellpo tp = pickupType(ptn,*env);
      cellpo ttp;

      if(tp==NULL || (islhs && !IsEnumeration(ptn,outer,&ttp) &&
		      !IsConstructor(ptn,outer,&ttp))){
	cellpo ltype = NewTypeVar(tgt);

	*env = newEnv(ptn,ltype,*env);

	return typeIs(ptn,ltype);	/* hack in the type information */
      }
      else {
	cellpo atype = allocSingle();

	freshType(tp,*env,atype);
	
	if(isConstructorType(atype,&a1,&a2)){
	  if(IsSymbol(deRef(a1),symVal(ptn))){
	    typeIs(ptn,atype);	/* set the whole type in the symbol */
	    BuildEnumStruct(ptn,ptn);
	    return typeIs(ptn,copyCell(tgt,a2)); /* we return the found type */
	  }
	  else{
	    reportErr(lineInfo(ptn),"%U is a constructor of type %t,"
		      "not an enumeration symbol",symVal(ptn),tp);
	    return typeIs(ptn,NewTypeVar(tgt));
	  }
	}
	else
	  return typeIs(ptn,copyCell(tgt,tp));
      }
    }
  }

  else if(isEmptyList(ptn))	/* Empty list -> _[] */
    return typeIs(ptn,BuildListType(NULL,tgt));

  else if(isNonEmptyList(ptn)){	/* Each element of list must be same type */
    cellpo et = allocSingle();
    cellpo f1,f2;
    cellpo head = listVal(ptn);
    cellpo tail = Next(head);
    
    ptnType(head,islhs,env,outer,et,True);
    ptnType(tail,islhs,env,outer,tgt,inH);

    /* All elements of a list same type */
    if(!checkType(et,elementType(tgt),&f1,&f2)){
      reportErr(lineInfo(ptn),"type - %W%t - of element - %5w - is "
		"inconsistent with the type - %W%t - of other list"
		" elements",f1,et,ptn,f2,tgt);
      NewTypeVar(tgt);
    }

    return typeIs(ptn,tgt);
  }

  /* Pattern specific operators ... */
  else if(isBinaryCall(ptn,kappend,&a1,&a2)){
    cellpo lt = ptnType(a1,islhs,env,outer,tgt,inH);
    cellpo rt = ptnType(a2,islhs,env,outer,allocSingle(),True);
    cellpo f1,f2;
    cellpo tp = BuildListType(NULL,allocSingle());

    /* All elements of a list same type */
    if(!checkType(lt,tp,&f1,&f2)){
      reportErr(lineInfo(ptn),"type - %W%t - of lhs - %5w - "
		"should be a list type ",f1,lt,ptn,f2,rt);
      NewTypeVar(tgt);
    }
    else if(!checkType(lt,rt,&f1,&f2)){
      reportErr(lineInfo(ptn),"type - %W%t - of lhs - %5w - is "
		"inconsistent with the type - %W%t - of rhs"
		" list",f1,lt,ptn,f2,rt);
      NewTypeVar(tgt);
    }

    return typeIs(ptn,tgt);
  }
  else if(isBinaryCall(ptn,kguard,&a1,&a2)){
    cellpo type = ptnType(a1,islhs,env,outer,tgt,inH);

    testType(a2,islhs,env,outer);

    return typeIs(ptn,type);
  }

  else if(isBinaryCall(ptn,kchar,&a1,&a2)){
    cellpo lhs = ptnType(a1,islhs,env,outer,allocSingle(),True);
    cellpo rhs = ptnType(a2,islhs,env,outer,tgt,inH);

    if(!sameType(lhs,symbolTp)) /* The first arguments should be symbol */
      reportErr(lineInfo(a1),"type of %3w should be `symbol', not %3t",a1,lhs);
    if(!sameType(rhs,stringTp)){ /* The arguments should be string */
      reportErr(lineInfo(a2),"type of %3w should be `string', not %3t",a2,rhs);
      copyCell(tgt,stringTp);
    }
    return typeIs(ptn,tgt);
  }

   else if(isBinaryCall(ptn,kcatenate,&a1,&a2)){
    cellpo lhs = ptnType(a1,islhs,env,outer,allocSingle(),True);
    cellpo rhs = ptnType(a2,islhs,env,outer,tgt,inH);

    if(!sameType(lhs,stringTp)) /* The arguments should be string */
      reportErr(lineInfo(a1),"type of %3w should be `string', not %3t",a1,lhs);
    if(!sameType(rhs,stringTp)){ /* The arguments should be string */
      reportErr(lineInfo(a2),"type of %3w should be `string', not %3t",a2,rhs);
      copyCell(tgt,stringTp);
    }
    return typeIs(ptn,tgt);
  }

  else if(IsQuery(ptn,&a1,&a2)){ /* sub-variable */
    if(isSymb(a2)){
      if(!typeExpression(a1,*env,env,tgt)){
	reportErr(lineInfo(a1),"type of var declaration `%5w' not a well formed type",
		  a1);
	return typeIs(ptn,NewTypeVar(tgt));
      }
      else{
	if(!inH)
	  tgt = dupCell(tgt);

	*env = newEnv(a2,tgt,*env);

	setTypeInfo(a2,tgt);	/* hack in the type information */
	return typeIs(ptn,tgt);
      }
    }
    else{
      reportErr(lineInfo(a1),"invalid form of pattern %w",a1);
      return typeIs(ptn,NewTypeVar(tgt));
    }
  }

  else if(isUnaryCall(ptn,kquery,&a2)){ /* sub-variable */
    cellpo ltype = NewTypeVar(tgt);

    *env = newEnv(a2,ltype,*env);

    setTypeInfo(a2,ltype);	/* hack in the type information */
    return typeIs(ptn,ltype);
  }

  else if(IsAnyStruct(ptn,&a1)){
    typeIs(a1,ptnType(a1,islhs,env,outer,allocSingle(),True));
    
    return typeIs(ptn,copyCell(tgt,anyTp));
  }

  else if(isBinaryCall(ptn,ktcoerce,&a1,&a2)){ /* string match sub-variable */
    if(isSymb(a2)){
      cellpo atype = allocSingle();
      if(!typeExpression(a1,*env,env,atype)){
	reportErr(lineInfo(a1),"type of var declaration `%5t' not a well formed type",a1);
	return typeIs(ptn,NewTypeVar(tgt));
      }
      else{
	*env = newEnv(a2,atype,*env);

	setTypeInfo(a2,atype);	/* hack in the type information */
	return typeIs(ptn,copyCell(tgt,stringTp)); /* but the pattern is a string!! */
      }
    }
    else{
      reportErr(lineInfo(a1),"invalid form of pattern %w",ptn);
      return typeIs(ptn,NewTypeVar(tgt));
    }
  }

  else if(isBinaryCall(ptn,kin,&a1,&a2)){ /* select from a list of strings */
    cellpo rtype = whichType(a2,env,outer,allocSingle(),True);
    cellpo etype = elementType(rtype);

    if(etype==NULL || !sameType(etype,stringTp))
      reportErr(lineInfo(a2),"%w should be a list of strings, not a `%t'",a2,rtype);
    else{
      cellpo lhs = ptnType(a1,islhs,env,outer,allocSingle(),True);
      if(!sameType(lhs,stringTp))
	reportErr(lineInfo(a1),"type of %w should be string not `%t'",a1,lhs);
    }
    return typeIs(ptn,copyCell(tgt,stringTp));
  }

  else if(isBinaryCall(ptn,khat,&a1,&a2)){
    cellpo lhs = ptnType(a1,islhs,env,outer,tgt,inH);
    cellpo rhs = whichType(a2,env,outer,allocSingle(),True);

    if(!sameType(lhs,stringTp))	/* The arguments should be string */
      reportErr(lineInfo(a1),"type of %3w should be `string', not %3t",a1,lhs);
    if(!sameType(rhs,numberTp)){ /* The arguments should be string */
      reportErr(lineInfo(a2),"type of %3w should be `number', not %3t",a2,rhs);
      copyCell(tgt,stringTp);
    }
    return typeIs(ptn,tgt);
  }

  else if(isBinaryCall(ptn,ktilda,&a1,&a2)){
    cellpo lhs = ptnType(a1,islhs,env,outer,tgt,inH);
    cellpo rhs = whichType(a2,env,outer,allocSingle(),True);

    if(!sameType(lhs,stringTp)) /* The arguments should be string */
      reportErr(lineInfo(a1),"type of %3w should be `string', not %3t",a1,lhs);
    if(!sameType(rhs,numberTp)){ 
      reportErr(lineInfo(a2),"type of %3w should be `number', not %3t",a2,rhs);
      copyCell(tgt,stringTp);
    }
    return typeIs(ptn,tgt);
  }

  else if(IsQuote(ptn,&args)){	/* Quoted symbol */
    if(isSymb(args))
      return typeIs(ptn,copyCell(tgt,symbolTp));
    else{
      reportErr(lineInfo(args),"invalid quoted pattern - %w",ptn);

      return typeIs(ptn,NewTypeVar(tgt)); /* This means we dont know the type */
    }
  }

  else if(isUnaryCall(ptn,kshriek,&args))
    return typeIs(ptn,whichType(args,env,outer,tgt,inH));

  else if(isBinaryCall(ptn,kmatch,&a1,&a2) && isSymb(a2)){
    cellpo type = BuildBinCall(kquery,voidcell,a2,tgt);
    ptnType(a1,islhs,env,outer,callArg(type,0),True);
    return typeIs(ptn,type);
  }

  else if(isCons(ptn) && isSymb(a1=consFn(ptn))){
    cellpo tt;
    cellpo ca,ct;

    if(IsConstructor(a1,*env,&tt)){
      cellpo f1,f2;
      cellpo args = tupleOfArgs(ptn,allocSingle());
      cellpo aType = ptnType(args,islhs,env,outer,allocSingle(),True);

      isConstructorType(tt,&ca,&ct);

      if(!unifyType(ca,aType,NULL,NULL,False,&f1,&f2)){
	reportErr(lineInfo(ptn),"arguments to labelled tuple `%w' ptn "
		  "of type %W%t not consistent with %W%t",
		  ptn,f2,aType,f1,ca);
	ct=NewTypeVar(tgt);
      }
      else{
        unsigned long ar = consArity(ptn);
        unsigned long i;
        
	copyCell(tgt,ct);
	
	for(i=0;i<ar;i++)
	  copyCell(consEl(ptn,i),tplArg(args,i));
      }
	
      typeIs(a1,tt);		/* we record that we have a constructor */
	  
      return typeIs(ptn,ct);
    }
    else{
      reportErr(lineInfo(ptn),"%w is not a known constructor term",ptn);
      return typeIs(ptn,NewTypeVar(tgt));
    }
  }

  else if(IsTuple(ptn,&ar)){	/* Regular tuple */
    int i;
      
    mkTpl(tgt,allocTuple(ar));

    for(i=0;i<ar;i++){
      cellpo p = tplArg(ptn,i);
      cellpo t = tplArg(tgt,i);

      ptnType(p,islhs,env,outer,t,True);
    }

    return typeIs(ptn,tgt);
  }
  else{
    reportErr(lineInfo(ptn),"cant determine type of pattern %w",ptn);
    return typeIs(ptn,NewTypeVar(tgt));
  }
}



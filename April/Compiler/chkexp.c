/*
  Type checker for April expressions
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
#include "cellP.h"		/* this is temporary */
#include "types.h"		/* Type cheking interface */
#include "typesP.h"		/* Private data structures for type checking */
#include "keywords.h"		/* Standard April keywords */

static cellpo switchType(cellpo cases,cellpo etype,
			  envPo env,cellpo tgt,logical inH);

/* Try to compute the type of an expression
   Involves type inference .... */
cellpo whichType(cellpo exp,envPo *env,envPo outer,cellpo tgt,logical inH)
{
  cellpo a1,a2,t;
  unsigned long ar;

  if(IsHashStruct(exp,knodebug,&a1)) /* Strip off the debugging stuff */
    return typeIs(exp,whichType(a1,env,outer,tgt,inH));
  else if(IsHashStruct(exp,kdebug,&a1))
    return typeIs(exp,whichType(a1,env,outer,tgt,inH));

  else if(typeInfo(exp)!=NULL){
#ifdef TYPETRACE
    if(traceType)
      outMsg(logFile,"Type information %4w found\n",typeInfo(exp));
#endif
    return copyCell(tgt,typeInfo(exp));	/* Use what we found */
  }

  else if(IsInt(exp))		/* Integer literals are of type integer */
    return typeIs(exp,copyCell(tgt,numberTp));

  else if(IsFloat(exp))		/* Floating points are of type number */
    return typeIs(exp,copyCell(tgt,numberTp));
    
  else if(isChr(exp))
    return typeIs(exp,copyCell(tgt,charTp));

  else if(IsString(exp))	/* Strings are also of fixed type */
    return typeIs(exp,copyCell(tgt,stringTp));

  else if(isSymb(exp)){		/* We have a symbol ... */
    cellpo type = pickupType(exp,*env);

    if(type!=NULL){
      cellpo atype = allocSingle();

      freshType(type,*env,atype);

      if(isConstructorType(deRef(atype),&a1,&a2)){
	if(IsSymbol(deRef(a1),symVal(exp))){
	  typeIs(exp,atype);	/* set the whole type in the symbol */
	  BuildEnumStruct(exp,exp);
	  return typeIs(exp,copyCell(tgt,a2)); /* we return the found type */
	}
	else{
	  reportErr(lineInfo(exp),"Symbol %U is not an enumeration of %t",symVal(exp),type);
	  return typeIs(exp,NewTypeVar(tgt));
	}
      }
      else
	return typeIs(exp,copyCell(tgt,type)); /* we dont freshen here */
    }
    else{
      reportErr(lineInfo(exp),"Symbol %U not defined",symVal(exp));
      return typeIs(exp,NewTypeVar(tgt));
    }
  }

  else if(isEmptyList(exp)){	/* Empty list -> alpha[] */
    cellpo tp = BuildListType(voidcell,tgt);
    cellpo el;

    isListType(tp,&el);	        /* Pick up the place where to put the var */

    NewTypeVar(el);		/* Create a new type variable */

    return typeIs(exp,tp);
  }

  else if(isNonEmptyList(exp)){	/* Each element of list must be same type */
    cellpo ht=allocSingle();
    cellpo head = listVal(exp);
    cellpo tail = Next(head);
    cellpo f1,f2;
    
    whichType(head,env,outer,ht,True);
    whichType(tail,env,outer,tgt,inH);

    if(!checkType(ht,elementType(tgt),&f1,&f2)) /* make all elements same */
      reportErr(lineInfo(exp),"type - %W%t - of element - %5w - is "
		"inconsistent with "
		"the type - %W%t - of other list elements",f1,ht,head,f2,tgt);

    return typeIs(exp,tgt);
  }

  /* Type caste to any  */
  else if(IsAnyStruct(exp,&a1)){
    whichType(a1,env,outer,allocSingle(),True); 

    return typeIs(exp,copyCell(tgt,anyTp));
  }

  else if(isBinaryCall(exp,ktcoerce,&a1,&a2)){ /* Type coercion expression */
    whichType(a2,env,outer,allocSingle(),True);

    if(!realType(a1,*env,tgt))
      reportErr(lineInfo(a1),"invalid type `%t' expression in type coercion "
		"expression `%4w'",a1,exp);
    else
      setTypeInfo(a1,tgt);

    return typeIs(exp,tgt);
  }

  /* A dot expression ... */
  else if(isBinaryCall(exp,kdot,&a1,&a2)){
    envPo re=*env;
    envPo renv = extendRecordEnv(a1,whichType(a1,env,outer,allocSingle(),True),re);
    cellpo type = whichType(a2,&renv,outer,tgt,inH);

    popEnv(renv,re);		/* Get rid of the excess stuff */
    return typeIs(exp,type);
  }

  /* special string generators */
  else if(isBinaryCall(exp,ktilda,&a1,&a2)){
    cellpo data=a1,width=a2,precision=voidcell;
    cellpo wtype;

    if(!isBinaryCall(a1,khat,&data,&precision))
      isBinaryCall(a2,khat,&width,&precision);

    /* Compute the type of the actual data expression */
    whichType(data,env,outer,allocSingle(),True);
       
    wtype = whichType(width,env,outer,allocSingle(),True);

    if(!checkType(wtype,numberTp,NULL,NULL)) /* The width should be numeric */
      reportErr(lineInfo(width),"width specifier should be `number' not %3w",wtype);

    if(precision!=voidcell){
      cellpo pt = whichType(precision,env,outer,allocSingle(),True);

      if(!checkType(pt,numberTp,NULL,NULL))
	reportErr(lineInfo(precision),"precision specifier should be a number");
    }

    return typeIs(exp,copyCell(tgt,stringTp));
  }

  else if(isBinaryCall(exp,khat,&a1,&a2)){
    cellpo rtype = whichType(a2,env,outer,allocSingle(),True);

    whichType(a1,env,outer,allocSingle(),True);

    if(!checkType(rtype,numberTp,NULL,NULL))
      reportErr(lineInfo(a2),"precision specifier should be `number' not %3w",rtype);

    return typeIs(exp,copyCell(tgt,stringTp));
  }
  else if(isUnaryCall(exp,ktstring,&a1)){
    cellpo type = allocSingle();
    if(!realType(a1,*env,type))
      reportErr(lineInfo(a1),"invalid type `%t' expression in type generator "
		"expression `%4w'",a1,exp);
    else
      setTypeInfo(a1,type);
    
    return typeIs(exp,copyCell(tgt,stringTp));
  }

  /* Logical expressions */
  else if(isBinaryCall(exp,kand,&a1,&a2) || isBinaryCall(exp,kdand,&a1,&a2)){
    if(!testType(a1,False,env,outer))
      reportErr(lineInfo(a1),"predicate expected in %4w",a1);
    else if(!testType(a2,False,env,outer))
      reportErr(lineInfo(a2),"predicate expected in %4w",a2);
    return typeIs(exp,copyCell(tgt,logicalTp));
  }
  else if(isBinaryCall(exp,kor,&a1,&a2)||isBinaryCall(exp,kdor,&a1,&a2)){
    envPo subE = *env;

    if(!testType(a1,False,&subE,outer))
      reportErr(lineInfo(a1),"predicate expected in %4w",a1);

    popEnv(subE,*env);
    subE = *env;

    if(!testType(a2,False,&subE,outer))
      reportErr(lineInfo(a2),"predicate expected in %4w",a2);

    popEnv(subE,*env);
    return typeIs(exp,copyCell(tgt,logicalTp));
  }

  else if(isUnaryCall(exp,knot,&a1)||isUnaryCall(exp,kdnot,&a1)){
    envPo subE = *env;

    if(!testType(a1,False,&subE,outer))
      reportErr(lineInfo(a1),"predicate expected in %4w",a1);

    popEnv(subE,*env);
    return typeIs(exp,copyCell(tgt,logicalTp));
  }

  else if(isBinaryCall(exp,kequal,&a1,&a2) ||
	  isBinaryCall(exp,knotequal,&a1,&a2) ||
	  isBinaryCall(exp,kgreatereq,&a1,&a2) ||
	  isBinaryCall(exp,kgreaterthan,&a1,&a2)||
	  isBinaryCall(exp,klessthan,&a1,&a2) ||
	  isBinaryCall(exp,klesseq,&a1,&a2)){
    cellpo lhs=whichType(a1,env,outer,allocSingle(),True);
    cellpo rhs=whichType(a2,env,outer,allocSingle(),True);
    cellpo f1,f2;

    if(!checkType(lhs,rhs,&f1,&f2))
      reportErr(lineInfo(a1),"lhs - %4w - which is of type - %W%t - "
		"is not compatible with - %4w - which is a - %W%4t -",
		a1,f1,lhs,a2,f2,rhs);

    return typeIs(exp,copyCell(tgt,logicalTp));
  }

  else if(isBinaryCall(exp,kmatch,&a1,&a2)){
    cellpo ptype=pttrnType(a1,False,env,outer,allocSingle(),True); 
    cellpo etype=whichType(a2,env,outer,allocSingle(),True);
    cellpo f1,f2;

    if(!checkType(ptype,etype,&f1,&f2))
      reportErr(lineInfo(a1),"lhs - %4w - which is of type - %W%t - is "
		"not compatible with - %4w - which is a - %W%4t -",
		a1,f1,ptype,a2,f2,etype);

    return typeIs(exp,copyCell(tgt,logicalTp));
  }

  else if(isBinaryCall(exp,knomatch,&a1,&a2)){
    pttrnType(a1,False,env,outer,allocSingle(),True); 
    whichType(a2,env,outer,allocSingle(),True);

    /* We make NO assumption about types across a nomatch */

    return typeIs(exp,copyCell(tgt,logicalTp));
  }

  else if(isBinaryCall(exp,kin,&a1,&a2)){
    cellpo ptype = pttrnType(a1,False,env,*env,allocSingle(),True); 
    cellpo ltype= whichType(a2,env,outer,allocSingle(),True);
    cellpo f1,f2;

    if(!checkType(ptype,elementType(ltype),&f1,&f2))
      reportErr(lineInfo(a1),"elements of list - %4w - which is of type - %W%t"
		"- are not compatible with - %4w - which is a - %W%4t -",
		a1,f1,ptype,a2,f2,ltype);

    return typeIs(exp,copyCell(tgt,logicalTp));
  }

  /* New-style lambda expression */
  else if(isBinaryCall(exp,kfn,&a1,&a2)){/* Ptn => Exp */
    envPo bodyEnv = *env;
    cellpo ptype = pttrnType(tupleIze(a1),True,&bodyEnv,*env,allocSingle(),True);
    cellpo rtype = whichType(a2,&bodyEnv,*env,allocSingle(),True);
    cellpo type = BuildFunType(ptype,rtype,tgt);

    popEnv(bodyEnv,*env);
    return typeIs(exp,type);
  }

  /* New-style mu expression */
  else if(isBinaryCall(exp,kstmt,&a1,&a2)){/* Ptn->Stmt */
    envPo bEnv = *env;
    cellpo ptype = pttrnType(tupleIze(a1),True,&bEnv,*env,allocSingle(),True);
    long ar = tplArity(ptype);
    long i;

    checkStmt(a2,bEnv,*env,allocSingle(),True); /* may bind type vars */
    
    BuildProcType(ar,tgt);
    
    for(i=0;i<ar;i++)
      copyCell(consEl(tgt,i),tplArg(ptype,i));

    popEnv(bEnv,*env);

    return typeIs(exp,tgt);
  }

  /* Function or procedure union */
  else if(isBinaryCall(exp,kchoice,&a1,&a2)){
    cellpo atype = whichType(a1,env,outer,allocSingle(),True);
    cellpo btype = whichType(a2,env,outer,allocSingle(),True);
    cellpo f1,f2;

    if(!checkType(atype,btype,&f1,&f2)){
      cellpo alhs,arhs,blhs,brhs;
      unsigned long arity,ar;

      if(isFunType(atype,&alhs,&arhs)){
	if(isFunType(btype,&blhs,&brhs)){
	  if(!checkType(alhs,blhs,NULL,NULL))
	    reportErr(lineInfo(a1),"lhs of equation %4w of type %t "
		      " not compatible with lhs of type %t %s",a1,
		      alhs,btype,sourceLocator(" @ ",lineInfo(a2)));
	  else 
	    reportErr(lineInfo(a1),"result of equation %4w of type %W%t "
		      " not compatible with rhs of type %W%t %s",
		      a1,f1,arhs,f2,btype,sourceLocator(" @ ",lineInfo(a2)));
	}
	else
	  reportErr(lineInfo(a1),"function %w should not be mixed with %w",
		    a1,a2);
      }
      else if(isProcType(atype,&arity)){
        if(!isProcType(btype,&ar)){
          if(ar!=arity)
	    reportErr(lineInfo(a1),"procedure %w of type %W%t "
		    " has different arity to %W%t %s",
		    a1,f1,alhs,f2,btype,sourceLocator(" @ ",lineInfo(a2)));
 	  else
	    reportErr(lineInfo(a1),"procedure %w of type %W%t "
		    " not compatible with type %W%t %s",
		    a1,f1,alhs,f2,btype,sourceLocator(" @ ",lineInfo(a2)));
        }
	else
	  reportErr(lineInfo(a1),"procedure %w should not be mixed with %w",
		    a1,a2);
      }
      else
	reportErr(lineInfo(a1),"program %w of type %W%t not same type as"
		  " program(s) %s which has type `%W%t'",
		  a1,f1,atype,sourceLocator(" @ ",lineInfo(a2)),f2,btype);
    }

    return typeIs(exp,copyCell(tgt,btype));
  }

  /* Valof/valis type computation */
  else if(isUnaryCall(exp,kvalueof,&a1)){
    if(checkStmt(a1,*env,outer,tgt,inH))
      return typeIs(exp,tgt);
    else{
      reportWarning(lineInfo(a1),"there should be a valis statement in %w",a1);
      return typeIs(exp,NewTypeVar(tgt));
    }
  }

  /* collect or setof computation */
  else if(isUnaryCall(exp,kcollect,&a1)){
    cellpo vtype;
    cellpo type = BuildListType(voidcell,tgt);

    isListType(type,&vtype); /* This is to avoid a heap allocation */

    if(!checkStmt(a1,*env,outer,vtype,True)){
      reportErr(lineInfo(a1),"body of collect exp. - %4w - has no elemis "
		"statement",a1);
      NewTypeVar(vtype);
    }

    return typeIs(exp,tgt);
  }

  /* Conditional expression */
  else if(isBinaryCall(exp,kelse,&a1,&a2) && isBinaryCall(a1,kthen,&t,&a1) &&
	  isUnaryCall(t,kif,&t)){
    envPo bEnv = *env;
    if(testType(t,False,env,outer)){
      cellpo ttype = whichType(a1,env,outer,allocSingle(),True);
      cellpo f1,f2;

      whichType(a2,&bEnv,outer,NewTypeVar(tgt),True);

      if(!checkType(ttype,tgt,&f1,&f2))
	reportErr(lineInfo(exp),"then branch - %4w - of type - %W%t - "
		      "should be the same type as - %w - which is - %W%t",
		  a1,f1,ttype,a2,f2,tgt);
    }
    else
      NewTypeVar(tgt);		/* We dont bother with the conditional test */
    return typeIs(exp,tgt);
  }
  
  else if(isUnaryCall(exp,kcase,&a1) && isBinaryCall(a1,kin,&a1,&a2)){
    cellpo etype = whichType(a1,env,outer,allocSingle(),True);
    return typeIs(exp,switchType(a2,etype,*env,tgt,inH));
  }

  else if(isUnaryCall(exp,ktry,&a2))
    return typeIs(exp,whichType(a2,env,outer,tgt,inH));

  else if(isBinaryCall(exp,kcatch,&a1,&a2)){
    cellpo etype = whichType(a1,env,outer,tgt,inH);
    cellpo xtype = switchType(a2,callArg(pickupTypeDef(cerror,*env),1),*env,allocSingle(),True);
    cellpo f1,f2;

    if(!checkType(etype,xtype,&f1,&f2))
      reportErr(lineInfo(a2),"type returned by error handler %W%t"
                             " not consistent with %W%t",f2,xtype,f1,etype);
    return typeIs(exp,tgt);
  }

  else if(isUnaryCall(exp,kraise,&a1)){
    NewTypeVar(tgt);		/* type is unknown */
    whichType(a1,env,outer,allocSingle(),True); /* compute type anyway */
    return typeIs(exp,tgt);
  }

  else if(isBinaryCall(exp,ksemi,NULL,NULL)||
	  isUnaryCall(exp,ksemi,NULL)) /* Theta record */
    return typeIs(exp,thetaType(exp,*env,tgt,inH));

  else if(isBinaryCall(exp,kdefn,&a1,&a2) && isSymb(a1)){
    cellpo tp = BuildBinCall(kquery,voidcell,a1,tgt);

    whichType(a2,env,outer,callArg(tp,0),True);
    return typeIs(exp,tp);
  }

  else if(isBinaryCall(exp,kfield,&a1,&a2) && isSymb(a1)){
    cellpo tp = BuildBinCall(kquery,voidcell,a1,tgt);

    whichType(a2,env,outer,callArg(tp,0),True);
    return typeIs(exp,tp);
  }

  else if(IsQuote(exp,&a1)){	/* Quoted expressions */
    if(isSymb(a1))
      return typeIs(exp,copyCell(tgt,symbolTp));
    else{
      reportErr(lineInfo(exp),"cant determine type of quoted expression - %w",exp);

      return typeIs(exp,NewTypeVar(tgt)); /* we dont know the type */
    }
  }

  else if(isUnaryCall(exp,kminus,&a1)){ /* special handling for -KK */
    cellpo tp = whichType(a1,env,outer,tgt,inH);

    if(!sameType(tp,numberTp))
      reportErr(lineInfo(a1),"unary - expects a numeric argument");
    return typeIs(exp,copyCell(tgt,numberTp));
  }
  
  else if(isCons(exp)){ /* application of some sort */
    cellpo lhs,rhs;
    cellpo ct;
    cellpo fn = consFn(exp);

    if(isSymb(fn) && IsConstructor(fn,*env,&ct) && 
       isConstructorType(ct,&lhs,&rhs)){
      cellpo f1,f2;
      cellpo args = tupleOfArgs(exp,allocSingle());
      cellpo aType = whichType(args,env,outer,allocSingle(),True);

      typeIs(fn,ct);		/* remember that we have a constructor */

      if(!unifyType(aType,lhs,NULL,NULL,False,&f1,&f2))
	reportErr(lineInfo(fn),"arguments to labelled tuple `%w'\n"
		  "of type %W%t\nare not consistent with \n%W%t",
		  exp,f1,aType,f2,lhs);
      {
        unsigned long ar = consArity(exp);
        unsigned long i;
        
        for(i=0;i<ar;i++)
          copyCell(consEl(exp,i),tplArg(args,i));
      }

      return typeIs(exp,copyCell(tgt,rhs));
    }
    else{
      cell Xtype;
      cell Lhs,Rhs;
      cellpo ftype = freshType(whichType(fn,env,outer,allocSingle(),True),*env,
			       allocSingle());
      cellpo args = tupleOfArgs(exp,allocSingle());
      cellpo atype = whichType(args,env,outer,allocSingle(),True);
      cellpo f1,f2;

      NewTypeVar(&Lhs);
      NewTypeVar(&Rhs);
      
      {
        unsigned long ar = consArity(exp);
        unsigned long i;
        
        for(i=0;i<ar;i++)
          copyCell(consEl(exp,i),tplArg(args,i));
      }

      BuildFunType(&Lhs,&Rhs,&Xtype); /* Function type may be computed */

      if(!checkType(ftype,&Xtype,&f1,&f2)){
	if(!isFunType(ftype,NULL,NULL)){
	  if(IsSymbol(ftype,ksymbolTp))
	    reportErr(lineInfo(fn),"undefined function `%4w'",fn);
	  else
	    reportErr(lineInfo(fn),"non function `%w' of type `%t' in function call",
		      fn,ftype);
	}
	else{
	  reportErr(lineInfo(fn),"function `%2w' of type:\n%W%t\n"
		    "is not applicable to arguments of type:\n%W%t",
		    fn,f1,ftype,f2,atype);
	}
      }
      else if(!checkType(atype,&Lhs,&f1,&f2))
	reportErr(lineInfo(fn),"function `%2w' of type:\n%W%t\n"
		  "is not applicable to arguments of type:\n%W%t",
		  fn,f1,ftype,f2,atype);
      else
	return typeIs(exp,copyCell(tgt,&Rhs));

      return typeIs(exp,NewTypeVar(tgt));
    }
  }

  else if(IsTuple(exp,&ar)){	/* Regular tuple */
    int i;
      
    mkTpl(tgt,allocTuple(ar));
    
    for(i=0;i<ar;i++){
      cellpo el = tplArg(exp,i);
      cellpo lhs;
      cellpo tp = tplArg(tgt,i);

      if(isBinaryCall(el,kdefn,&lhs,&el) ||
	 isBinaryCall(el,kfield,&lhs,&el))
	tp = callArg(BuildBinCall(kquery,voidcell,lhs,tplArg(tgt,i)),0);
      else
	tp = tplArg(tgt,i);

      whichType(el,env,outer,tp,True);
    }

    return typeIs(exp,tgt);
  }
  else{
    reportErr(lineInfo(exp),"cant determine the type of - %4w",exp);
    return typeIs(exp,NewTypeVar(tgt));
  }
}

logical testType(cellpo test,logical islhs,envPo *env,envPo outer)
{
  cellpo tt,a1,a2;

  if(isBinaryCall(test,kmatch,&a1,&a2)){
    cellpo pType = pttrnType(a1,islhs,env,outer,allocSingle(),True); 
    cellpo eType = whichType(a2,env,outer,allocSingle(),True);
    cellpo f1,f2;

    if(!checkType(pType,eType,&f1,&f2))
      reportErr(lineInfo(test),"pattern type %W%t not compatible with "
		"expression type %W%t",f1,pType,f2,eType);
    return True;
  }

  else if(isBinaryCall(test,knomatch,&a1,&a2)){
    envPo subE = *env;

    pttrnType(a1,islhs,env,outer,allocSingle(),True); 
    whichType(a2,env,outer,allocSingle(),True);

				/* We make no type assumptions here */
    popEnv(subE,*env);
    return True;
  }

  else if(isBinaryCall(test,kin,&a1,&a2)){
    cellpo pType = pttrnType(a1,islhs,env,outer,allocSingle(),True);
    cellpo eType = whichType(a2,env,outer,allocSingle(),True);
    cellpo Xtype = BuildListType(pType,allocSingle());
    cellpo f1,f2;
    
    if(!checkType(Xtype,eType,&f1,&f2)){
      if(!isListType(deRef(eType),NULL))
	reportErr(lineInfo(test),"expression %w should be a list, not a %W%t",
		  a2,f2,eType);
      else
	reportErr(lineInfo(test),"ptn type %W%t not compatible with "
		  "expression type %W%t",
		  f1,pType,f2,eType);
    }

    return True;
  }

  else if(isBinaryCall(test,kand,&a1,&a2) || isBinaryCall(test,kdand,&a1,&a2))
    return testType(a1,islhs,env,outer) && testType(a2,islhs,env,outer);

  else if(isBinaryCall(test,kor,&a1,&a2)||isBinaryCall(test,kdor,&a1,&a2)){
    envPo subE = *env;

    if(!testType(a1,islhs,&subE,outer)){
      popEnv(subE,*env);
      reportErr(lineInfo(a1),"predicate expected in %4w",a1);
      return False;
    }

    popEnv(subE,*env);
    subE = *env;

    if(!testType(a2,islhs,&subE,outer)){
      popEnv(subE,*env);
      reportErr(lineInfo(a2),"predicate expected in %4w",a2);
      return False;
    }
    else{
      popEnv(subE,*env);
      return True;
    }
  }

  else if(isUnaryCall(test,knot,&a1) || isUnaryCall(test,kdnot,&a1)){
    envPo subE = *env;

    if(!testType(a1,islhs,&subE,outer)){
      reportErr(lineInfo(a1),"predicate expected in %4w",a1);
      popEnv(subE,*env);
      return False;
    }
    else{
      popEnv(subE,*env);
      return True;
    }
  }

  else{
    whichType(test,env,outer,tt=allocSingle(),True);
    if(!checkType(tt,logicalTp,NULL,NULL)){ /* unify the result with logical */
      reportErr(lineInfo(test),"type of test should be logical not %t",tt);
      return False;
    }
    else
      return True;
  }
}

static cellpo switchType(cellpo cases,cellpo etype,envPo env,cellpo tgt,logical inH)
{
  cellpo c1,c2,t,s;
  cellpo f1,f2;

  if(isBinaryCall(cases,kchoice,&c1,&c2)){
    cellpo ct1 = switchType(c1,etype,env,allocSingle(),True);
    
    switchType(c2,etype,env,tgt,inH);
    
    if(!checkType(ct1,tgt,&f1,&f2))
      reportErr(lineInfo(cases),"Case result type %W%t in case expression "
		"`%3w' is incompatible with type %W%t",
		f1,ct1,cases,f2,tgt);

    return tgt;
  }
  else if(isBinaryCall(cases,kfn,&t,&s)){
    envPo envS = env;
    cellpo ltype = pttrnType(t,True,&env,envS,allocSingle(),True);

    if(!checkType(ltype,etype,&f1,&f2))
      reportErr(lineInfo(cases),"Case pattern type %W%t in "
		"switch `%3w' is incompatible with type %W%t",
		f1,ltype,cases,f2,etype);
    whichType(s,&env,envS,tgt,inH);

    popEnv(env,envS);
    return tgt;
  }
  else{
    reportErr(lineInfo(cases),"Invalid clause in case expression: %w",cases);
    return tgt;
  }
}

/*
  Type checker for April statements
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
#include "cellP.h"
#include "types.h"		/* Type cheking interface */
#include "typesP.h"		/* Private data structures for type checking */
#include "keywords.h"		/* Standard April keywords */

static logical stmtSeqType(cellpo pttrn,envPo env,
			   envPo outer,cellpo tgt,logical inH);
static logical switchType(cellpo cases,cellpo exp,cellpo Etype,envPo env,
			  cellpo tgt,logical inH);

/* type check a series of statements -- this might bind type variables */
logical checkStmt(cellpo stmt,envPo env,envPo outer,cellpo tgt,logical inH)
{
  return stmtSeqType(stmt,env,outer,tgt,inH);
}

/* Check the type of a single statement -- and return any valof type */
static logical stmtType(cellpo input,envPo *env,envPo outer,cellpo tgt,
			logical inH)
{
  cellpo s1,s2,fn;

  if(IsHashStruct(input,knodebug,&input))
    return stmtType(input,env,outer,tgt,inH);
  else if(IsHashStruct(input,kdebug,&input))
    return stmtType(input,env,outer,tgt,inH);
  else if(isBinaryCall(input,ksemi,NULL,NULL))
    return stmtSeqType(input,*env,outer,tgt,inH);
  else if(isUnaryCall(input,ksemi,&input))
    return stmtType(input,env,outer,tgt,inH);
  else if(isBinaryCall(input,klabel,NULL,&input)|| /* labelled statement */
	  isBinaryCall(input,kguard,NULL,&input)) /* previous lbld statement */
    return stmtType(input,env,outer,tgt,inH);
  else if(isBinaryCall(input,kelse,&s1,&s2)){
    cellpo test, then;

    if(isBinaryCall(s1,kthen,&test,&then) && isUnaryCall(test,kif,&test)){
      envPo Env = *env;
      logical F1,F2;
      cellpo tt = allocSingle();
      cellpo f1,f2;

      testType(test,False,&Env,outer);

      F1=stmtSeqType(then,Env,outer,tt,False);
      F2=stmtSeqType(s2,*env,outer,tgt,inH);

      if(F1&&F2){
	if(!checkType(tt,tgt,&f1,&f2))
	  reportErr(lineInfo(input),"type of valis/elemis in `%5.1100w' "
		    "of type `%W%t' is not compatible with valis/elemis "
		    "in `%5.1100w' which is of type `%W%t'",
		    then,f1,tt,s2,f2,tgt);
      }
      else if(F1)
	copyCell(tgt,tt);	/* We have computed the type of the valof */

      popEnv(Env,*env);		/* Get rid of any excess declarations */

      return F1 || F2;
    }
    else
      return False;		/* This form is actually illegal */
  }
  else if(isBinaryCall(input,kthen,&s1,&s2) && isUnaryCall(s1,kif,&s1)){
    envPo Env = *env;
    logical found;

    testType(s1,False,&Env,outer);

    found = stmtSeqType(s2,Env,outer,tgt,inH);

    popEnv(Env,*env);		/* Get rid of any excess declarations */

    return found;
  }
  else if(isBinaryCall(input,kdo,&s1,&s2)){
    cellpo test;
    logical ret;

    if(isUnaryCall(s1,kfor,&test)){
      cellpo el,set;
      if(isBinaryCall(test,kin,&el,&set)){
	envPo Env = *env;

	testType(test,False,&Env,outer);

	ret = stmtSeqType(s2,Env,outer,tgt,inH);

	popEnv(Env,*env);	/* Get rid of any excess declarations */

	return ret;
      }
      else{
	reportErr(lineInfo(input),"badly formed for statement %w",input);
	NewTypeVar(tgt);
	return False;
      }
    }
    else if(isUnaryCall(s1,kwhile,&test)){
      envPo Env = *env;

      testType(test,False,&Env,outer);

      ret = stmtSeqType(s2,Env,outer,tgt,inH);
      popEnv(Env,*env);		/* Get rid of any excess declarations */
      return ret;
    }
    else{
      reportErr(lineInfo(input),"badly formed for statement %w",input);
      NewTypeVar(tgt);
      return False;
    }
  }

  else if(isUnaryCall(input,ktry,&s1))
    return stmtType(s1,env,outer,tgt,inH);

  else if(isBinaryCall(input,kcatch,&s1,&s2)){
    cellpo s2type=allocSingle();
    logical s1found = stmtSeqType(s1,*env,outer,tgt,inH);
    logical s2found = switchType(s2,s1,callArg(pickupTypeDef(cerror,*env),1),*env,s2type,True);

    if(s1found&&s2found)
      sameType(tgt,s2type);
    else if(s2found)
      copyCell(tgt,s2type);

    return s1found || s2found;
  }

  else if(isUnaryCall(input,kvalueof,&s1)){
    reportErr(lineInfo(input),"valof -- `%3w' -- not permitted as a statement",
	      input);
    return False;
  }

  else if(isBinaryCall(input,kwithin,&s1,&s2)){
    logical found = stmtSeqType(s1,*env,outer,tgt,inH);
    cellpo ctype = whichType(s2,env,outer,allocSingle(),True);

    if(!checkType(ctype,opaqueTp,NULL,NULL))
      reportErr(lineInfo(s2),"%w should be a click counter",ctype);

    return found;
  }

  else if(isBinaryCall(input,ksend,&s1,&s2)){
    cellpo ttype = whichType(s2,env,outer,allocSingle(),True);

    if(!checkType(handleTp,ttype,NULL,NULL))
      reportErr(lineInfo(s2),"%w should be a handle",ttype);

    whichType(s1,env,outer,allocSingle(),True);

    return False;
  }

  else if(isBinaryCall(input,kassign,&s1,&s2)){
    if(isSymb(s1)){
      cell Ltype;
      cellpo rtype = whichType(s2,env,outer,allocSingle(),True);
      cellpo f1,f2;

      if(knownType(s1,*env,&Ltype)){
	if(!checkType(&Ltype,rtype,&f1,&f2))
	  reportErr(lineInfo(input),"assignment of variable `%1w' of type "
		    "%W`%t' not compatible with %W`%3w'",
		    s1,f1,&Ltype,f2,rtype);
      }
      else if(IsQuery(s1,NULL,NULL))
	reportErr(lineInfo(s1),"Use = statement to declare variables");
    }
    else{			/* generalize the := to be like a match */
      cellpo ltype = pttrnType(s1,False,env,outer,allocSingle(),True);
      cellpo rtype = whichType(s2,env,outer,allocSingle(),True);
      cellpo f1,f2;

      if(!checkType(ltype,rtype,&f1,&f2))
	reportErr(lineInfo(input),"lhs of assignment %w of type %W%t is not"
		  " consistent with rhs %w which has type %W%t",
		  s1,f1,ltype,s2,f2,rtype);

      return False;
    }

    return False;
  }
  else if(isBinaryCall(input,kmatch,&s1,&s2)){
    cellpo ltype = pttrnType(s1,False,env,outer,allocSingle(),True);
    cellpo rtype = whichType(s2,env,outer,allocSingle(),True);
    cellpo f1,f2;

    if(!checkType(ltype,rtype,&f1,&f2))
      reportErr(lineInfo(input),"lhs of match %w of type %W%t is not "
		"consistent with rhs of match %w which has type %W%t",
		s1,f1,ltype,s2,f2,rtype);

    return False;
  }
  else if(isBinaryCall(input,kfield,&s1,&s2) ||
	  isBinaryCall(input,kdefn,&s1,&s2)){
    if(isSymb(s1)){ /* variable declaration */
      cellpo vType = whichType(s2,env,outer,allocSingle(),True);

#ifdef TYPETRACE
      if(traceType)
	outMsg(logFile,"Type of variable %w is %5.1800t\n",s1,vType);
#endif

      if(!IsSymbol(s1,kuscore))
	*env = newEnv(s1,vType,*env); /* we must extend the environment */
      setTypeInfo(s1,vType);
      return False;
    }
    else if(IsQuery(s1,&fn,&s1) && isSymb(s1)){
      cellpo vType = whichType(s2,env,outer,allocSingle(),True);
      cellpo dType = allocSingle();
      cellpo f1,f2;

      if(!realType(fn,*env,dType))
	reportErr(lineInfo(fn),"invalid type `%t' associated with declaration",fn);

#ifdef TYPETRACE
      if(traceType){
	outMsg(logFile,"Found type of variable %w is %5.1800w\n",s1,vType);
	outMsg(logFile,"Declared type of variable %w is %5.1800w\n",s1,dType);
      }
#endif

      if(!checkType(dType,vType,&f1,&f2)){
	reportErr(lineInfo(s1),"inferred type of %w -- %W%t not compatible "
		  "with declared type %W%t",s1,f2,vType,f1,dType);
      }

      *env = newEnv(s1,vType,*env); /* we must extend the environment */
      setTypeInfo(s1,vType);
      return False;
    }
    else{			/* generalize the : decl. to be like a match */
      cellpo ltype = pttrnType(s1,True,env,outer,allocSingle(),True);
      cellpo rtype = whichType(s2,env,outer,allocSingle(),True);
      cellpo f1,f2;

      if(!checkType(ltype,rtype,&f1,&f2))
	reportErr(lineInfo(input),"lhs of decl %w of type %W%t is not"
		  " consistent with initial value %w which has type %W%t",
		  s1,f1,ltype,s2,f2,rtype);

      return False;
    }
  }
  
  else if(isBinaryCall(input,kdot,&s1,&s2)){
    cellpo type = whichType(s1,env,outer,allocSingle(),True);
    envPo rEnv = extendRecordEnv(s1,type,*env);
    logical checked = stmtSeqType(s2,rEnv,outer,tgt,inH);

    popEnv(rEnv,*env);		/* Get rid of any excess declarations */
    return checked;
  }
  else if(isUnaryCall(input,kleave,NULL))
    return False;
  else if(IsSymbol(input,kdie))
    return False;

  else if(IsSymbol(input,krelax) || IsSymbol(input,kblock))
    return False;		/* Nothing here */
  else if(IsSymbol(input,khalt))
    return False;		/* Nothing here */
  else if(isUnaryCall(input,kvalis,&s1)){
    whichType(s1,env,outer,tgt,inH);
    return True;
  }
  else if(isUnaryCall(input,kelement,&s1)){
    whichType(s1,env,outer,tgt,inH);
    return True;
  }
  else if(isUnaryCall(input,kraise,&s1)){
    whichType(s1,env,outer,allocSingle(),True);
    return False;
  }
  else if(isUnaryCall(input,kdebug,&s1)){ /* Debugging message */
    whichType(s1,env,outer,tgt,inH);
    return True;
  }
  else if(isUnaryCall(input,kcase,&s1) && isBinaryCall(s1,kin,&s1,&s2)){
    cellpo etype = whichType(s1,env,outer,allocSingle(),True);
    return switchType(s2,s1,etype,*env,tgt,inH);
  }
  
  else if(isCons(input)){
    cellpo args = tupleOfArgs(input,allocSingle());
    cellpo atype = whichType(args,env,outer,allocSingle(),True);
    unsigned long arity = consArity(input);
    unsigned long i;
        
    for(i=0;i<arity;i++)
      copyCell(consEl(input,i),tplArg(args,i));
          
    {
      envPo subEnv = *env;
      cellpo xt = allocSingle();
      cellpo fn = consFn(input);
      cellpo ptype = freshType(whichType(fn,&subEnv,*env,xt,True),*env,allocSingle());
      cellpo f1,f2;
      unsigned long ar;
        
      if(isProcType(ptype=deRef(ptype),&ar)){
        if(ar==arity){
          unsigned long i;
        
          for(i=0;i<ar;i++){
            if(!checkType(consEl(ptype,i),tplArg(atype,i),&f1,&f2)){
              reportErr(lineInfo(fn),"argument no %d of type %W%t "
                  "is not compatible with %W%t",
                  i,f2,tplArg(atype,i),f1,consEl(ptype,i));
            }
          }
        }
        else{
	  reportErr(lineInfo(fn),"procedure `%2w' of type:\n%t\n"
		  "has different arity %d than expected",fn,ptype,arity);
          NewTypeVar(ptype);
        }
      }
      else if(IsTypeVar(ptype)){
        cellpo xx = BuildProcType(arity,allocSingle());
        unsigned long i;
      
        for(i=0;i<arity;i++)
          copyCell(consEl(xx,i),tplArg(atype,i));
        
        checkType(ptype,xx,&f1,&f2);
      }
      else{
        reportErr(lineInfo(fn),"procedure `%2w' of type:\n%t\n"
		  "is not applicable to arguments of type:\n%t",
		  fn,ptype,atype);
      }
    
      setTypeInfo(fn,ptype);	/* record the found type */

      return False;		/* anyway -- no valis here ... */
    }
  }
  else{
    reportErr(lineInfo(input),"Cant understand statement %w",input);
    return False;
  }
}

static logical stmtSeqType(cellpo input,envPo env,envPo outer,
			   cellpo tgt,logical inH)
{
  cellpo st1,st2;
  envPo envS = env;

  if(isBinaryCall(input,ksemi,&st1,&st2)){
    cellpo t1=allocSingle();
    logical found1 = stmtType(st1,&env,outer,t1,True);
    logical found2 = stmtSeqType(st2,env,outer,NewTypeVar(tgt),True);
    cellpo f1,f2;
    
    if(found1&&found2){
      if(!checkType(t1,tgt,&f1,&f2))
	reportErr(lineInfo(st1),"valis/elemis type %W%t reported in "
		  "statement %4.1100w is not compatible with "
		  "valis/elemis type %W%t reported in %.1100w",
		  f1,t1,st1,f2,tgt,st2);
    }
    else if(found1)
      copyCell(tgt,t1);

    popEnv(env,envS);

    return found1||found2;
  }
  else if(isUnaryCall(input,ksemi,&st1))
    return stmtSeqType(st1,env,outer,tgt,inH);

  else{
    envPo Env = env;
    logical f = stmtType(input,&Env,outer,tgt,inH);

    popEnv(Env,env);
    return f;
  }
}

static logical switchType(cellpo cases,cellpo exp,cellpo etype,
			  envPo env,cellpo tgt,logical inH)
{
  cellpo c1,c2,t,s;

  if(isBinaryCall(cases,kchoice,&c1,&c2)){
    cellpo ct1=allocSingle();
    cellpo ct2=NewTypeVar(tgt);
    logical found1 = switchType(c1,exp,etype,env,ct1,False);
    logical found2 = switchType(c2,exp,etype,env,ct2,True);

    if(found1&&found2){
      cellpo f1,f2;
      if(!checkType(ct1,ct2,&f1,&f2))
	reportErr(lineInfo(c1),"pattern of clause %4w of type `%W%t' is not "
		  "compatible with pattern of clause %4w of type `%W%t'",
		  c1,f1,ct1,c2,f2,ct2);
    }
    else if(found1)
      copyCell(tgt,ct1);

    return found1 || found2;
  }
  else if(isBinaryCall(cases,kstmt,&t,&s)){
    envPo envS = env;
    cellpo ltype = pttrnType(t,True,&env,envS,allocSingle(),True);
    logical ret = stmtSeqType(s,env,envS,tgt,inH);
    cellpo f1,f2;

    if(!checkType(ltype,etype,&f1,&f2))
      reportErr(lineInfo(cases),"Case pattern type %W%t in procedure call or "
		"switch `%3w' is incompatible with expression `%4w' "
		"which has type %W%t", f1,ltype,t,exp,f2,etype);

    popEnv(env,envS);
    return ret;
  }
  else{
    reportErr(lineInfo(cases),"Invalid form of case in case statement: %w",cases);
    return False;
  }
}

/*
 * Expression folding module for the April compiler
 * $Id: foldexp.c,v 1.3 2003/06/30 22:06:48 fmccabe Exp $
 * $Log: foldexp.c,v $
 * Revision 1.3  2003/06/30 22:06:48  fmccabe
 * fixed to permit compilation with gcc 3.3
 *
 * Revision 1.2  2003/05/07 18:30:10  fmccabe
 * My version
 *
 * Revision 1.6  2000/11/15 01:48:32  fmccabe
 * Fixed another problem in foldexp, maybe more to do here
 *
 * Revision 1.5  2000/11/09 23:32:01  fmccabe
 * Fixed problem with ++ handling in foldexp
 *
 * Revision 1.4  2000/11/09 18:29:22  fmccabe
 * Fixed problems identified by insure++, plus Theo's bug
 *
 * Revision 1.3  2000/08/10 19:41:29  fmccabe
 * Had to work around a glitch in gcc
 *
 * Revision 1.2  2000/06/20 18:53:31  fmccabe
 * re-engineered the type system
 *
 * Revision 1.1.1.1  2000/05/04 20:44:30  fmccabe
 * Initial import of April in sourceforge's CVS
 *
 *
 * Revision 2.9  1997/01/14 00:25:39  fgm
 * Added cases in fold expression to handle debugging and type information
 *
 * Revision 2.8  1996/12/20 02:00:21  fgm
 * Extended to deal with string concatenation and unary minus
 *
 * Revision 2.7  1996/09/30 07:48:51  fgm
 * Not a lot
 *
 *
 */
#include "config.h"
#include <stdlib.h>		/* Standard functions */
#include <string.h>		/* String functions */
#include <math.h>
#include <assert.h>
#include "compile.h"		/* Compiler structures */
#include "cellP.h"
#include "fold.h"		/* Fold expression interface */
#include "keywords.h"		/* Standard April keywords */
#include "types.h"		/* Type checker needed for type manipulation */
#include "generate.h"
#include "url.h"

static logical IsNGrnd(cellpo input, dictpo dict)
{
  return IsInt(input) || IsFloat(input);
}

static logical IsIGrnd(cellpo input, dictpo dict)
{
  return IsInt(input);
}

static logical IsStGrnd(cellpo input, dictpo dict)
{
  return IsString(input);
}

/* Literal list? */
static logical IsLstGrnd(cellpo input, dictpo dict)
{
  while(IsList(input)){
    if(isEmptyList(input))
      return True;
    else
      input=Next(listVal(input));
  }
  return False;
}

logical IsGrnd(cellpo input, dictpo dict)
{
  if(IsInt(input) || IsFloat(input) || isChr(input) || IsString(input) || isEmptyList(input))
    return True;
  else if(isSymb(input))
    return !IsVar(input,NULL,NULL,NULL,NULL,NULL,NULL,NULL,dict);
  else if(isCons(input)){
    logical clean = IsGrnd(consFn(input),dict);
    unsigned int ar = consArity(input);
    unsigned int i;
    
    for(i=0; clean && i<ar; i++)
      clean = IsGrnd(consEl(input,i),dict);
    return clean;
  }
  else if(isTpl(input)){
    logical clean = True;
    int ar=tplArity(input);
    int i;

    for(i=0;clean && i<ar;i++)
      clean=IsGrnd(tplArg(input,i),dict);
    return clean;
  }
  else if(IsList(input)){
    input=listVal(input);

    return IsGrnd(input,dict) && IsGrnd(Next(input),dict);
  }
  else
    return False;
}

logical SimpleLiteral(cellpo input,dictpo dict)
{
  if(IsInt(input) || IsFloat(input) || isChr(input) || IsString(input) || isEmptyList(input))
    return True;
  else if(isSymb(input))
    return !IsVar(input,NULL,NULL,NULL,NULL,NULL,NULL,NULL,dict);
  else
    return False;
}

static logical BldCall(cellpo args,int ar,cellpo f,cellpo in,
		       long info,cellpo out)
{
  int i;
  cellpo type = typeInfo(in);
  
  p_cell(f);

  for(i=0;i<ar;i++){
    p_cell(args);
    args=Next(args);
  }

  p_cons(ar);			/* construct the application */

  stLineInfo(topStack(),info);
  setTypeInfo(topStack(),type);
  popcell(out);

  return False;			/* Not clean */
}

/* Special functions to implement compile-time evaluation */
/* Catenate two strings together */
static logical Catenate(cellpo a1, cellpo a2,cellpo input,long info,
			cellpo output)
{
  cellpo type1 = typeInfo(a1);
  uniChar *s1 = strVal(a1);
  uniChar *s2 = strVal(stripCell(a2));
  int len = uniStrLen(s1)+uniStrLen(s2)+1;
  uniChar ss[len];
  
  uniCpy(ss,len,s1);
  uniCat(ss,len,s2);
  
  mkStr(output,allocString(ss));
  stLineInfo(output,info);
  if(type1!=NULL)
    setTypeInfo(output,type1);
  return False;
}

static logical MergeURL(cellpo a1, cellpo a2,cellpo input,long info,
			cellpo output)
{
  cellpo type1 = typeInfo(a1);
  uniChar *s1 = strVal(a1);
  uniChar *s2 = strVal(stripCell(a2));
  int len = uniStrLen(s1)+uniStrLen(s2)+1;
  uniChar ss[len];
  uniChar u1[len];
  uniChar u2[len];
  
  uniCpy(u1,len,s1);
  uniCpy(u2,len,s2);
  
  mergeURLs(u1,u2,ss,len);
    
  mkStr(output,allocString(ss));
  stLineInfo(output,info);
  if(type1!=NULL)
    setTypeInfo(output,type1);
  return False;
}


/* Compute length of a string */
static logical StrLen(cellpo a1,cellpo input,long info,cellpo output)
{
  mkInt(output,uniStrLen(strVal(stripCell(a1))));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);
  return False;
}

/* Append fixed list to other list */
static logical Append(cellpo a1, cellpo a2, cellpo input,long info,cellpo output)
{
  cellpo out = output;

  a1 = stripCell(a1);

  while(isNonEmptyList(a1)){
    cellpo el = allocPair();
    cellpo type = typeInfo(a1);

    mkList(out,el);
    copyLineInfo(a1,out);
    setTypeInfo(out,type);

    a1 = listVal(a1);
    copyCell(el,a1);
    a1 = Next(a1);
    out = Next(el);
  }
  copyCell(out,stripCell(a2));	/* put 2nd arg in tail */
  stLineInfo(output,info);
  return False;
}


/* Index into a list */
static logical Index(cellpo a1,cellpo a2,cellpo input,long info,cellpo output)
{
  long index = intVal(a2=stripCell(a2));

  if(index<=0){
    reportErr(lineInfo(a1),"negative index %w into list %w",a2,a1);
    copyCell(output,input);
    return True;		/* do nothing */
  }
  else{
    cellpo a = a1;
    while(isNonEmptyList(a) && index-->1)
      a = Next(listVal(a));

    if(isEmptyList(a)){
      reportErr(lineInfo(a1),"index %w too large for list %w",a2,a1);
      copyCell(output,input);
      return True;
    }
    else{
      copyCell(output,listVal(a));
      stLineInfo(output,info);
      return False;
    }
  }
}

static logical Head(cellpo a1,cellpo input,long info,cellpo output)
{
  typeInfo(a1);

  if(!isNonEmptyList(a1)){
    reportErr(lineInfo(a1),"head taken of empty list");
    copyCell(output,input);
    return True;
  }
  else{
    copyCell(output,listVal(a1));
    stLineInfo(output,info);
    return False;
  }
}

static logical Tail(cellpo a1,cellpo a2,cellpo input,long info,cellpo output)
{
  int index = intVal(a2=stripCell(a2));

  if(index<=0){
    reportErr(lineInfo(a1),"negative tail count %w in %w",a2,a1);
    copyCell(output,input);
    return True;		/* do nothing */
  }
  else{
    cellpo a = stripCell(a1);
    while(isNonEmptyList(a) && index-->1)
      a = Next(listVal(a));

    copyCell(output,a);
    stLineInfo(output,info);
    return False;
  }
}

/* Compute length of fixed list */
static logical ListLength(cellpo a1, cellpo input,long info,cellpo output)
{
  int len = 0;

  while(isNonEmptyList(a1)){
    len++;
    a1 = Next(listVal(a1));
  }
  mkInt(output,len);
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);
  return False;
}

/* Add two numbers together */
static logical Plus(cellpo a1, cellpo a2, cellpo input,long info,cellpo output)
{
  if(IsInt(a1) && IsInt(a2))
    mkInt(output,intVal(a1)+intVal(a2));
  else if(IsInt(a1))
    mkFlt(output,intVal(a1)+fltVal(a2));
  else if(IsInt(a2))
    mkFlt(output,intVal(a2)+fltVal(a1));
  else
    mkFlt(output,fltVal(a1)+fltVal(a2));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);
  return False;
}

/* Subtract two numbers */
static logical Minus(cellpo a1,cellpo a2,cellpo input,long info,cellpo output)
{
  if(IsInt(a1) && IsInt(a2))
    mkInt(output,intVal(a1)-intVal(a2));
  else if(IsInt(a1))
    mkFlt(output,intVal(a1)-fltVal(a2));
  else if(IsInt(a2))
    mkFlt(output,intVal(a2)-fltVal(a1));
  else
    mkFlt(output,fltVal(a1)-fltVal(a2));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);

  return False;
}

/* Negate a number */
static logical Negate(cellpo a1, cellpo input,long info,cellpo output)
{
  if(IsInt(a1))
    mkInt(output,-intVal(a1));
  else
    mkFlt(output,-fltVal(a1));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);

  return False;
}

/* Multiply two numbers together */
static logical Times(cellpo a1,cellpo a2,cellpo input,long info,cellpo output)
{
  if(IsInt(a1) && IsInt(a2))
    mkInt(output,intVal(a1)*intVal(a2));
  else if(IsInt(a1))
    mkFlt(output,intVal(a1)*fltVal(a2));
  else if(IsInt(a2))
    mkFlt(output,intVal(a2)*fltVal(a1));
  else
    mkFlt(output,fltVal(a1)*fltVal(a2));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);
  return False;
}

/* Divide two numbers */
static logical Divide(cellpo a1,cellpo a2,cellpo input,long info,cellpo output)
{
  if(IsInt(a1) && IsInt(a2))
    mkFlt(output,intVal(a1)/intVal(a2));
  else if(IsInt(a1))
    mkFlt(output,intVal(a1)/fltVal(a2));
  else if(IsInt(a2))
    mkFlt(output,fltVal(a1)/intVal(a2));
  else
    mkFlt(output,fltVal(a1)/fltVal(a2));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);
  return False;
}

/* Compute pi */
#ifndef PI
#define PI 3.14159265358979323846
#endif

static logical Pi(cellpo input,long info,cellpo output)
{
  mkFlt(output,PI);
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);
  return False;
}


/* Bitwise arithmetic */
/* And two integers together */
static logical Band(cellpo a1,cellpo a2,cellpo input,long info,cellpo output)
{
  mkInt(output,intVal(a1)&intVal(a2));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);

  return False;
}

/* Or two integers together */
static logical Bor(cellpo a1,cellpo a2,cellpo input,long info,cellpo output)
{
  mkInt(output,intVal(a1)|intVal(a2));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);

  return False;
}

/* Exclusive or two integers together */
static logical Bxor(cellpo a1,cellpo a2,cellpo input,long info,cellpo output)
{
  mkInt(output,intVal(a1)^intVal(a2));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);

  return False;
}

/* Left shift */
static logical Bleft(cellpo a1,cellpo a2,cellpo input,long info,cellpo output)
{
  mkInt(output,intVal(a1)<<intVal(a2));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);

  return False;
}

/* Rght shift */
static logical Bright(cellpo a1,cellpo a2,cellpo input,long info,cellpo output)
{
  mkInt(output,intVal(a1)>>intVal(a2));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);

  return False;
}

/* Arithmetic complement */
static logical Bnot(cellpo a1, cellpo input,long info,cellpo output)
{
  mkInt(output,~intVal(a1));
  stLineInfo(output,info);
  setTypeInfo(output,numberTp);

  return False;
}


static logical fldExp(cellpo input,cellpo output,long info,
		      dictpo dict,dictpo *ndict);

static logical ValOf(cellpo body,cellpo input,cellpo output,
		     dictpo dict,dictpo *ndict)
{
  cellpo exp,lhs,rhs;

  if(IsHashStruct(body,knodebug,&body))
    return ValOf(body,input,output,dict,ndict);
  else if(IsHashStruct(body,kdebug,&body))
    return ValOf(body,input,output,dict,ndict);
  else if(isUnaryCall(body,ksemi,&body))
    return ValOf(body,input,output,dict,ndict);
  else if(isBinaryCall(body,ksemi,&lhs,&rhs) && IsSymbol(lhs,krelax))
    return ValOf(rhs,input,output,dict,ndict);
  else if(isUnaryCall(body,kvalis,&exp)){
    fldExp(exp,output,lineInfo(exp),dict,ndict);
    return False;
  }
  else{
    copyCell(output,input);
    return True;		/* Clean output */
  }
}

static logical fldExp(cellpo input,cellpo output,long info,
		      dictpo dict,dictpo *ndict)
{
  unsigned long ar;
  cellpo f,lhs,rhs;

  *ndict = dict;

  if(IsTypeVar(input))
    return fldExp(refVal(input),output,info,dict,ndict);
  else if(isUnaryCall(input,kvalueof,&lhs))
    return ValOf(lhs,input,output,dict,ndict);

  else if(isUnaryCall(input,kcollect,NULL) ||
	  isUnaryCall(input,ksetof,NULL) ||
	  isUnaryCall(input,kcase,NULL) ||
	  isBinaryCall(input,kelse,NULL,NULL)){
    copyCell(output,input);
    *ndict = dict;
    return True;
  }

  else if(IsHashStruct(input,kdebug,&rhs)){
    cellpo right = allocSingle();
    logical clean = fldExp(rhs,right,lineInfo(rhs),dict,ndict);

    if(clean)
      copyCell(output,input);
    else{
      cellpo type = typeInfo(input);

      BuildHashStruct(kdebug,right,output);
      stLineInfo(output,info);
      setTypeInfo(output,type);
    }

    return clean;
  }

  else if(IsHashStruct(input,knodebug,&rhs)){
    cellpo right = allocSingle();
    logical clean = fldExp(rhs,right,lineInfo(rhs),dict,ndict);

    if(clean)
      copyCell(output,input);
    else{
      cellpo type = typeInfo(input);

      BuildHashStruct(knodebug,right,output);
      stLineInfo(output,info);
      setTypeInfo(output,type);
    }      

    return clean;
  }

  else if(IsQuote(input,NULL)){
    copyCell(output,input);
    return True;
  }

  else if(IsTheta(input,NULL)){
    copyCell(output,input);
    return True;
  }

  else if(IsEnumStruct(input,NULL)){
    copyCell(output,input);
    return True;
  }

  else if(IsZeroStruct(input,kpi))
    return Pi(input,info,output);

  else if(isBinaryCall(input,kfn,NULL,NULL) ||
	  isBinaryCall(input,kstmt,NULL,NULL)){
    copyCell(output,input);
    return True;
  }

  else if(isBinaryCall(input,ktcoerce,NULL,NULL)){
    copyCell(output,input);
    return True;
  }

  else if(IsAnyStruct(input,NULL)){
    copyCell(output,input);
    return True;
  }
  
  else if(isBinaryCall(input,kcatenate,&lhs,&rhs)){
    logical clean=True;
    cell A1,A2;
    
    clean = fldExp(lhs,&A1,lineInfo(lhs),dict,&dict);
    clean &= fldExp(rhs,&A2,lineInfo(rhs),dict,&dict);

    *ndict = dict;
    
    if(IsStGrnd(&A1,dict) && IsStGrnd(&A2,dict))
      return Catenate(&A1,&A2,input,info,output);
    else if(clean){
      copyCell(output,input);
      return True;
    }
    else{
      BuildBinStruct(consFn(input),&A1,&A2,output);
      setTypeInfo(output,typeInfo(input));
      return False;                     // Not clean
    }
  }
  
  else if(isBinaryCall(input,kmergeURL,&lhs,&rhs)){
    logical clean=True;
    cell A1,A2;
    
    clean = fldExp(lhs,&A1,lineInfo(lhs),dict,&dict);
    clean &= fldExp(rhs,&A2,lineInfo(rhs),dict,&dict);

    *ndict = dict;
    
    if(IsStGrnd(&A1,dict) && IsStGrnd(&A2,dict))
      return MergeURL(&A1,&A2,input,info,output);
    else if(clean){
      copyCell(output,input);
      return True;
    }
    else{
      BuildBinStruct(consFn(input),&A1,&A2,output);
      setTypeInfo(output,typeInfo(input));
      return False;                     // Not clean
    }
  }
  
  else if(isBinaryCall(input,kappend,&lhs,&rhs)){
    logical clean=True;
    cell A1,A2;
    
    clean = fldExp(lhs,&A1,lineInfo(lhs),dict,&dict);
    clean &= fldExp(rhs,&A2,lineInfo(rhs),dict,&dict);

    *ndict = dict;
    
    if(IsLstGrnd(&A1,dict))
      return Append(&A1,&A2,input,info,output);
    else if(clean){
      copyCell(output,input);
      return True;
    }
    else{
      BuildBinStruct(consFn(input),&A1,&A2,output);
      setTypeInfo(output,typeInfo(input));
      return False;                     // Not clean
    }
  }
  
  else if(isUnaryCall(input,kstrlen,&lhs)){
    cell A1;
    logical clean = fldExp(lhs,&A1,lineInfo(lhs),dict,&dict);

    *ndict = dict;
    
    if(IsStGrnd(&A1,dict))
      return StrLen(&A1,input,info,output);
    else if(clean){
      copyCell(output,input);
      return True;
    }
    else{
      BuildUnaryStruct(consFn(input),&A1,output);
      setTypeInfo(output,typeInfo(input));
      return False;                     // Not clean
    }
  }
  
  else if(isUnaryCall(input,kcardinality,&lhs)){
    cell A1;
    logical clean = fldExp(lhs,&A1,lineInfo(lhs),dict,&dict);

    *ndict = dict;
    
    if(IsStGrnd(&A1,dict))
      return ListLength(&A1,input,info,output);
    else if(clean){
      copyCell(output,input);
      return True;
    }
    else{
      BuildUnaryStruct(consFn(input),&A1,output);
      setTypeInfo(output,typeInfo(input));
      return False;                     // Not clean
    }
  }
  
  else if(isBinaryCall(input,kindex,&lhs,&rhs)){
    logical clean=True;
    cell A1,A2;
    
    clean = fldExp(lhs,&A1,lineInfo(lhs),dict,&dict);
    clean &= fldExp(rhs,&A2,lineInfo(rhs),dict,&dict);

    *ndict = dict;
    
    if(IsLstGrnd(&A1,dict) && IsIGrnd(&A2,dict))
      return Index(&A1,&A2,input,info,output);
    else if(clean){
      copyCell(output,input);
      return True;
    }
    else{
      BuildBinStruct(consFn(input),&A1,&A2,output);
      setTypeInfo(output,typeInfo(input));
      return False;                     // Not clean
    }
  }
  else if(IsStruct(input,&f,&ar) && ar<=MAXARG && isSymb(f)){
    logical clean=True;
    symbpo fn=symVal(f);
    volatile int i;
    cell avect[MAXARG];

    for(i=0;i<ar;i++){
      cellpo arg = consEl(input,i);
      clean &= fldExp(arg,&avect[i],lineInfo(arg),dict,&dict);
    }

    *ndict = dict;

    if(fn==ktail && ar==2 && IsLstGrnd(&avect[0],dict) &&
	    IsIGrnd(&avect[1],dict))
      return Tail(&avect[0],&avect[1],input,info,output);
    else if(fn==khead && ar==1 && IsLstGrnd(&avect[0],dict))
      return Head(&avect[0],input,info,output);
    else if(fn==kappend && ar==2 && IsLstGrnd(&avect[0],dict))
      return Append(&avect[0],&avect[1],input,info,output);
    else if(fn==kplus && ar==2 && IsNGrnd(&avect[0],dict) &&
	    IsNGrnd(&avect[1],dict))
      return Plus(&avect[0],&avect[1],input,info,output);
    else if(fn==kminus && ar==2 && IsNGrnd(&avect[0],dict) &&
	    IsNGrnd(&avect[1],dict))
      return Minus(&avect[0],&avect[1],input,info,output);
    else if(fn==kminus && ar==1 && IsNGrnd(&avect[0],dict))
      return Negate(&avect[0],input,info,output);
    else if(fn==kstar && ar==2 && IsNGrnd(&avect[0],dict) &&
	    IsNGrnd(&avect[1],dict))
      return Times(&avect[0],&avect[1],input,info,output);
    else if(fn==kslash && ar==2 && IsNGrnd(&avect[0],dict) &&
	    IsNGrnd(&avect[1],dict))
      return Divide(&avect[0],&avect[1],input,info,output);
    else if(fn==kband && ar==2 && IsIGrnd(&avect[0],dict) &&
	    IsIGrnd(&avect[1],dict))
      return Band(&avect[0],&avect[1],input,info,output);
    else if(fn==kbor && ar==2 && IsIGrnd(&avect[0],dict) &&
	    IsIGrnd(&avect[1],dict))
      return Bor(&avect[0],&avect[1],input,info,output);
    else if(fn==kbxor && ar==2 && IsIGrnd(&avect[0],dict) &&
	    IsIGrnd(&avect[1],dict))
      return Bxor(&avect[0],&avect[1],input,info,output);
    else if(fn==kbnot && ar==1 && IsIGrnd(&avect[0],dict))
      return Bnot(&avect[0],input,info,output);
    else if(fn==kbleft && ar==2 && IsIGrnd(&avect[0],dict) &&
	    IsIGrnd(&avect[1],dict))
      return Bleft(&avect[0],&avect[1],input,info,output);
    else if(fn==kbright && ar==2 && IsIGrnd(&avect[0],dict) &&
	    IsIGrnd(&avect[1],dict))
      return Bright(&avect[0],&avect[1],input,info,output);
    else if(fn==kequal  && ar==2 && IsGrnd(&avect[0],dict) &&
	    IsGrnd(&avect[1],dict)){
      if(sameCell(&avect[0],&avect[1]))
	copyCell(output,ctrue);
      else
	copyCell(output,cfalse);

      setTypeInfo(output,logicalTp);

      return False;
    }
    else if(fn==knotequal  && ar==2 && IsGrnd(&avect[0],dict) &&
	    IsGrnd(&avect[1],dict)){
      if(sameCell(&avect[0],&avect[1]))
	copyCell(output,cfalse);
      else
	copyCell(output,ctrue);

      setTypeInfo(output,logicalTp);

      return False;
    }
    else if(fn==klessthan  && ar==2 && IsNGrnd(&avect[0],dict) &&
	    IsNGrnd(&avect[1],dict)){
      cellpo a1 = stripCell(&avect[0]);
      cellpo a2 = stripCell(&avect[1]);
      FLOAT A1 = (IsInt(a1)?((FLOAT)intVal(a1)):fltVal(a1));
      FLOAT A2 = (IsInt(a2)?((FLOAT)intVal(a2)):fltVal(a2));

      if(A1<A2)
	copyCell(output,ctrue);
      else
	copyCell(output,cfalse);

      setTypeInfo(output,logicalTp);

      return False;
    }
    else if(fn==kgreaterthan  && ar==2 && IsNGrnd(&avect[0],dict) &&
	    IsNGrnd(&avect[1],dict)){
      cellpo a1 = stripCell(&avect[0]);
      cellpo a2 = stripCell(&avect[1]);
      FLOAT A1 = (IsInt(a1)?((FLOAT)intVal(a1)):fltVal(a1));
      FLOAT A2 = (IsInt(a2)?((FLOAT)intVal(a2)):fltVal(a2));

      if(A1>A2)
	copyCell(output,ctrue);
      else
	copyCell(output,cfalse);

      setTypeInfo(output,logicalTp);

      return False;
    }
    else if(fn==klesseq  && ar==2 && IsNGrnd(&avect[0],dict) &&
	    IsNGrnd(&avect[1],dict)){
      cellpo a1 = stripCell(&avect[0]);
      cellpo a2 = stripCell(&avect[1]);
      FLOAT A1 = (IsInt(a1)?((FLOAT)intVal(a1)):fltVal(a1));
      FLOAT A2 = (IsInt(a2)?((FLOAT)intVal(a2)):fltVal(a2));

      if(A1<=A2)
	copyCell(output,ctrue);
      else
	copyCell(output,cfalse);

      setTypeInfo(output,logicalTp);

      return False;
    }
    else if(fn==kgreatereq  && ar==2 && IsNGrnd(&avect[0],dict) &&
	    IsNGrnd(&avect[1],dict)){
      cellpo a1 = stripCell(&avect[0]);
      cellpo a2 = stripCell(&avect[1]);
      FLOAT A1 = (IsInt(a1)?((FLOAT)intVal(a1)):fltVal(a1));
      FLOAT A2 = (IsInt(a2)?((FLOAT)intVal(a2)):fltVal(a2));

      if(A1>=A2)
	copyCell(output,ctrue);
      else
	copyCell(output,cfalse);

      setTypeInfo(output,logicalTp);

      return False;
    }
    else if((fn==kin || fn==kmatch) && ar==2){
      int dp;

      *ndict=EmbeddedVars(callArg(input,0),0,&dp,True,
			  singleAss,False,dict,dict,dict,NULL);

      copyCell(output,input);
      return True;
    }
    /* fold constant conditional expressions */
    else if((fn==kand || fn ==kdand) && ar==2){
      cellpo a1 = &avect[0];
      cellpo a2 = &avect[1];

      if(IsSymbol(a1,kfalse))
	BuildEnumStruct(cfalse,output);
      else if(IsEnumStruct(a1,&a1) && IsSymbol(a1,kfalse))
	BuildEnumStruct(cfalse,output);
      else if(IsSymbol(a1,ktrue))
	copyCell(output,a2);
      else if(IsEnumStruct(a1,&a1) && IsSymbol(a1,ktrue))
	copyCell(output,a2);
      else if(IsSymbol(a2,ktrue))
	copyCell(output,a1);
      else if(IsEnumStruct(a2,&a2) && IsSymbol(a2,ktrue))
	copyCell(output,a1);
      else{
	if(clean)
	  copyCell(output,input);
	else{
	  BuildBinCall(fn,&avect[0],&avect[1],output);
	  setTypeInfo(output,logicalTp);
	  return False;
	}
      }
      setTypeInfo(output,logicalTp);

      return False;
    }
      
    else if((fn==kor || fn ==kdor) && ar==2){
      cellpo a1 = &avect[0];
      cellpo a2 = &avect[1];
      
      if(IsSymbol(a1,kfalse))
	copyCell(output,a2);
      else if(IsEnumStruct(a1,&a1) && IsSymbol(a1,kfalse))
	copyCell(output,a2);
      else if(IsSymbol(a2,kfalse))
	copyCell(output,a1);
      else if(IsSymbol(a1,ktrue))
	BuildEnumStruct(cfalse,output);
      else if(IsEnumStruct(a1,&a1) && IsSymbol(a1,ktrue))
	BuildEnumStruct(cfalse,output);
      else if(IsSymbol(a2,ktrue))
	BuildEnumStruct(cfalse,output);
      else if(IsEnumStruct(a2,&a2) && IsSymbol(a2,ktrue))
	BuildEnumStruct(cfalse,output);
      else{
	if(clean)
	  copyCell(output,input);
	else{
	  BuildBinCall(fn,&avect[0],&avect[1],output);
	  setTypeInfo(output,logicalTp);
	}
	return clean;
      }

      setTypeInfo(output,logicalTp);

      return False;
    }
      
    else if((fn==knot || fn ==kdnot) && ar==1){
      cellpo a1 = &avect[0];

      if(IsSymbol(a1,kfalse))
	BuildEnumStruct(ctrue,output);
      else if(IsSymbol(a1,ktrue))
	BuildEnumStruct(cfalse,output);
      else{
	if(clean)
	  copyCell(output,input);
	else{
	  BuildUnaryCall(fn,&avect[0],output);
	  setTypeInfo(output,logicalTp);
	}
	return clean;
      }

      setTypeInfo(output,logicalTp);

      return False;
    }
      
    else if(clean){
      copyCell(output,input);
      return True;
    }
    else			/* At least one of the args was modified */
      return BldCall(avect,ar,f,input,info,output);
  }

  else if(isSymb(input) || IsInt(input) || IsFloat(input) || isChr(input) || IsString(input)){
    copyCell(output,input);
    return True;
  }
  else if(isTpl(input)){
    int ar = tplArity(input);
    logical clean = True;
    cellpo top = topStack();
    int i;

    for(i=0;i<ar;i++){
      cell buff;
      cellpo arg = tplArg(input,i);
      clean &= fldExp(arg,&buff,lineInfo(arg),dict,&dict);
      p_cell(&buff);
    }

    *ndict = dict;		/* in case we have modified the dictionary */

    if(clean){
      reset_stack(top);
      copyCell(output,input);
      return True;
    }
    else{
      cellpo type = typeInfo(input);

      clstple(top);		/* Copy the tuple */
      popcell(output);
      stLineInfo(output,info);
      setTypeInfo(output,type);
      return False;
    }
  }

  else if(isEmptyList(input)){
    copyCell(output,input);
    return True;
  }
  else if(isNonEmptyList(input)){
    cell head,tail;
    cellpo in=listVal(input);
    logical clean = fldExp(in,&head,lineInfo(in),dict,&dict) & 
      fldExp(Next(in),&tail,lineInfo(Next(in)),dict,ndict);

    if(clean){
      copyCell(output,input);
      return True;
    }
    else{
      in = allocPair();	/* Copy the list pair */
      mkList(output,in);
      stLineInfo(output,info);
      copyCell(in,&head);
      copyCell(Next(in),&tail);
      return False;
    }
  }
  
  else if(isCons(input)){
    unsigned int ar = consArity(input);
    cellpo top = topStack();
    unsigned int i;
    cell buff;
    cellpo fn = consFn(input);
    logical clean = fldExp(fn,&buff,lineInfo(fn),dict,&dict);

    for(i=0;i<ar;i++){
      cell buff;
      cellpo arg = consEl(input,i);
      clean &= fldExp(arg,&buff,lineInfo(arg),dict,&dict);
      p_cell(&buff);
    }

    *ndict = dict;		/* in case we have modified the dictionary */

    if(clean){
      reset_stack(top);
      copyCell(output,input);
      return True;
    }
    else{
      cellpo type = typeInfo(input);
      
      p_cons(ar);

      popcell(output);
      stLineInfo(output,info);
      setTypeInfo(output,type);
      return False;
    }
  }
  else
    reportErr(lineInfo(input),"illegal expression %w",input);
  return False;
}

cellpo foldExp(cellpo input,cellpo output,dictpo dict)
{
  dictpo ndict;

  fldExp(input,output,lineInfo(input),dict,&ndict);
  clearDict(ndict,dict);
  return output;
}


/*
  Code generation/term encoding function for the April compiler
  (c) 1994-1999 Imperial College & F.G. McCabe

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
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "compile.h"
#include "keywords.h"
#include "dict.h"
#include "assem.h"
#include "encode.h"		/* File encoding headers */
#include "signature.h"
#include "ioP.h"

typedef struct _label_ *labelPo;
typedef struct _label_ {
  cellpo term;
  int ref;
  labelPo prev;
} LabelRec;

static void encodeInt(ioPo out,short tag,long val);
static labelPo collectTvars(cellpo input,labelPo chain);

static void encode(ioPo out,cellpo input,labelPo chain,labelPo tvars);
static void encodeType(ioPo out,cellpo input,labelPo chain,labelPo tvars);

void encodeICM(ioPo out,cellpo input)
{
  labelPo tvars = collectTvars(input,NULL);
  encode(out,input,NULL,tvars);
  
  while(tvars!=NULL){
    labelPo p = tvars->prev;
    
    free(tvars);
    tvars = p;
  }
}


/* Encode a generic structure into ICM format */
static void encode(ioPo out,cellpo input,labelPo chain,labelPo tvars)
{
  labelPo links = chain;
  
  input=deRef(input);
  
  while(links!=NULL){
    if(links->term==input){
      encodeInt(out,trmRef,links->ref);
      return;
    }
    links=links->prev;
  }

  switch(tG(input)){

  case intger: {
    encodeInt(out,trmInt,intVal(input));
    break;
  }

  case floating: {
    unsigned char buff[16], *bf = buff;
    int i,exp,len,sign=trmFlt;
    double f = fltVal(input);

    /* Convert floating point into buffer first -- to get the length */

    if(f<0.0){
      sign = trmNegFlt;
      f = -f;
    }

    f = frexp(f, &exp);

    for(len=0;f!=0.0;len++){
      double ip;
      f = modf(f*256,&ip);

      *bf++ = (int)ip;
    }

    outChar(out,sign|len);	/* the lead character of the number */
    encodeInt(out,trmInt,exp);	/* the exponent */
    for(i=0;i<len;i++)
      outChar(out,buff[i]);
    break;
  }

  case symbolic:{
    symbpo sym = symVal(input);
    int len = uniStrLen(sym);
    long uLen = len*4;
    unsigned char buff[uLen];
    int i;

    len = uni_utf8(sym,uniStrLen(sym)+1,buff,uLen);
    
    encodeInt(out,trmSym,len);	/* the symbol length */
    for(i=0;i<len;i++)
      outChar(out,buff[i]);
    break;
  }

  case character: {
    encodeInt(out,trmChar,CharVal(input));
    break;
  }

  case string:{
    uniChar *str = strVal(input);
    long len = uniStrLen(str);
    integer i;
    uniChar code = 0;
          
    for(i=0;i<len;i++){
      if((str[i]&0xff)!=str[i]){
        code = uniBOM;                  // Unicode Byte Order Mark
        break;
      }
    }
          
    if(code==uniBOM){
      encodeInt(out,trmString,(len+1)*sizeof(uniChar));
      outChar(out,((uniBOM>>8)&0xff));
      outChar(out,(uniBOM&0xff));
      for(i=0;i<len;i++){
        outChar(out,(str[i]>>8)&0xff);
        outChar(out,(str[i]&0xff));
      }
    }
    else{
      encodeInt(out,trmString,len);
            
      for(i=0;i<len;i++)
        outChar(out,str[i]&0xff);
    }
    break;
  }

  case codeseg:{
    codepo pc = codeVal(input);
    int size,litcnt;
    long signature = SIGNATURE;	/* A special signature word */
    ioPo tmp = openOutStr(rawEncoding);
    int i;
    instruction *code = codecode(pc);
    labelPo typeVars = NULL;
    
    litcnt = codelitcnt(pc);
    size = codesize(pc);	/* Compute the size of the code section */

    for(i=0;i<litcnt;i++)
        typeVars = collectTvars(codelit(pc,i),typeVars);
        
    typeVars = collectTvars(codetypes(pc),typeVars);	
    typeVars = collectTvars(codefrtypes(pc),typeVars);

    /* write out a signature to handle little endians and big endians,
       and to prove that this is April code */
    outChar(tmp,(signature>>24)&0xff);
    outChar(tmp,(signature>>16)&0xff);
    outChar(tmp,(signature>>8)&0xff);
    outChar(tmp,signature&0xff);

    encodeInt(tmp,trmInt,size);	                /* The size of the code block */
    encodeInt(tmp,trmInt,litcnt);               /* The number of literals in the code */
    
    for(i=0;i<size;i++){
      outChar(tmp,(code[i]>>24)&0xff);
      outChar(tmp,(code[i]>>16)&0xff);
      outChar(tmp,(code[i]>>8)&0xff);
      outChar(tmp,code[i]&0xff);
    }

    for(i=0;i<litcnt;i++)
      encode(tmp,codelit(pc,i),NULL,typeVars);
    encodeType(tmp,codetypes(pc),NULL,typeVars);	        /* write out the type signature */
    encodeType(tmp,codefrtypes(pc),NULL,typeVars);       /* write out the free type signature */

    {
      long len;
      uniChar *buff = getStrText(O_STRING(tmp),&len);
      int i;
      
      /* We have completed the code, now we write  */

      encodeInt(out,trmCode,len);
      for(i=0;i<len;i++)
        outChar(out,buff[i]);
      
      closeFile(tmp);
    }
    break;
  }

  case list:
    if(listVal(input)==NULL)
      outChar(out,trmNil);
    else{
      outChar(out,trmList);
      encode(out,(input=listVal(input)),chain,tvars);
      encode(out,Next(input),chain,tvars);
    }
    break; 

  case constructor: {
    unsigned int i,ar;
    LabelRec link;

    ar = consArity(input);

    /* Do we need to handle circular structures? */
    if(isCodeClosure(input)){
      link.term = input;
      link.ref = chain==NULL?0:chain->ref+1;
      link.prev=chain;
      chain=&link;
	
      encodeInt(out,trmTag,chain->ref);
    }

    encodeInt(out,trmStruct,ar);
    encode(out,consFn(input),NULL,NULL);
    for(i=0;i<ar;i++)
      encode(out,consEl(input,i),chain,tvars);
    break;
  }

  case tuple: {
    long i,ar = tplArity(input);

    encodeInt(out,trmStruct,ar);
    encode(out,ctpl,NULL,NULL);
    for(i=0;i<ar;i++)
      encode(out,tplArg(input,i),chain,tvars);
    break;
  }

  case tvar:{
    while(tvars!=NULL){
      if(tvars->term==input)
        return encodeInt(out,trmVariable,tvars->ref);
      else
        tvars = tvars->prev;
    }
        
    reportErr(lineInfo(input),"Illegal type variable in code generation");
    break;
  }
  default:{
    reportErr(lineInfo(input),"Illegal cell %3w in code generation",input);
    break;
  }
  }
}

static void encodeType(ioPo out,cellpo input,labelPo chain,labelPo tvars)
{
  labelPo links = chain;
  
  input=deRef(input);
  
  while(links!=NULL){
    if(links->term==input){
      encodeInt(out,trmRef,links->ref);
      return;
    }
    links=links->prev;
  }

  switch(tG(input)){

  case symbolic:{
    symbpo sym = symVal(input);
    int len = uniStrLen(sym);
    long uLen = len*4;
    unsigned char buff[uLen];
    int i;

    len = uni_utf8(sym,uniStrLen(sym)+1,buff,uLen);
    
    encodeInt(out,trmSym,len);	/* the symbol length */
    for(i=0;i<len;i++)
      outChar(out,buff[i]);
    break;
  }

  case constructor: {
    unsigned int i,ar;
    LabelRec link;

    ar = consArity(input);

    /* Do we need to handle circular structures? */
    if(isCodeClosure(input)){
      link.term = input;
      link.ref = chain==NULL?0:chain->ref+1;
      link.prev=chain;
      chain=&link;
	
      encodeInt(out,trmTag,chain->ref);
    }

    encodeInt(out,trmStruct,ar);
    encodeType(out,consFn(input),NULL,NULL);
    for(i=0;i<ar;i++)
      encodeType(out,consEl(input,i),chain,tvars);
    break;
  }

  case tuple: {
    long i,ar = tplArity(input);

    encodeInt(out,trmStruct,ar);
    encodeType(out,tplTp,NULL,NULL);
    for(i=0;i<ar;i++)
      encodeType(out,tplArg(input,i),chain,tvars);
    break;
  }

  case tvar:{
    while(tvars!=NULL){
      if(tvars->term==input)
        return encodeInt(out,trmVariable,tvars->ref);
      else
        tvars = tvars->prev;
    }
        
    reportErr(lineInfo(input),"Illegal type variable in code generation");
    break;
  }
  default:{
    reportErr(lineInfo(input),"Illegal cell %3w in code generation",input);
    break;
  }
  }
}


static void encodeInt(ioPo out,short tag,long val)
{
  int len=0,i;
  unsigned char bytes[16];
  unsigned long v = val;

  if(v==0)			/* 0 has special meaning which we dont use */
    bytes[len++]=0;
  else
    while(v>0){
      bytes[len++]=v&0xff;
      v >>= 8;
    }

  if(val>0 && (bytes[len-1]&0x80))
    bytes[len++]=0;		/* stuff in a leading zero for funny numbers */

  outChar(out,tag|len);
  for(i=len;i>0;i--)
    outChar(out,bytes[i-1]);
}

static labelPo collectTvars(cellpo input,labelPo chain)
{
  input = deRef(input);
  switch(tG(input)){
    case tvar:{
      labelPo ch = chain;
      int vNo = 0;
      
      while(ch!=NULL){
        if(ch->term==input)
          return chain;
        if(ch->ref>=vNo)
          vNo = ch->ref;
        ch = ch->prev;
      }
      
      ch = (labelPo)malloc(sizeof(LabelRec));
      
      ch->prev = chain;
      ch->ref = vNo+1;
      ch->term = input;
      
      return ch;
    }
    case list:{
      while(isNonEmptyList(input)){
        chain = collectTvars(listHead(input),chain);
        input = listTail(input);
      }
      if(!isEmptyList(input))
        return collectTvars(input,chain);
      else
        return chain;
    }
    case constructor:{
      unsigned long ar = consArity(input);
      unsigned long i;
      
      for(i=0;i<ar;i++)
        chain = collectTvars(consEl(input,i),chain);
      return collectTvars(consFn(input),chain);
    }
    case tuple:{
      long ar = tplArity(input);
      long i;
      
      for(i=0;i<ar;i++)
        chain = collectTvars(tplArg(input,i),chain);
      return chain;
    }
    case codeseg:
      return chain;
      
    default:
      return chain;
  }
}

/* Handle construction of type signatures -- for dynamically loaded code */

#define MAXVAR 1024

static struct{
  cellpo var;			/* The type variable */
  unsigned int varNo;		/* Its numeric code number */
  logical free;			/* Is this a free variable? */
}vars[MAXVAR];			/* Table of type variables */

static unsigned int vartop = 0;
static unsigned int varNo = 0;
static void genSig(ioPo out,cellpo type);

/* Generate a type signature string */

cellpo makeTypeSig(cellpo type)
{
  cellpo sig = allocSingle();
  ioPo str = openOutStr(rawEncoding);

  vartop = 0;
  varNo = 0;
  
  genSig(str,type);
  
  outChar(str,0);
  
  {
    long len;
    uniChar *buff = getStrText(O_STRING(str),&len);
      
    /* We have completed the code, now we make the symbol  */
    
    mkSymb(sig,locateU(buff));
  }
      
  closeFile(str);
  return sig;
}

static int newVar(cellpo type,logical free)
{
  int vNo = vars[vartop].varNo = varNo++;

  if(vartop>=NumberOf(vars))
    syserr("too many type variables in type signature");

  vars[vartop].var = type;
  vars[vartop].free = free;
  vartop++;

  return vNo;
}

static void resetVarNo(int vNo)
{
  vars[vNo].var=NULL;
  vars[vNo].free=False;
}

static void putSymbol(ioPo out,uniChar *sym);

static void genSig(ioPo out,cellpo type)
{
  cellpo fn,lhs,rhs;
  unsigned long ar;

  type=deRef(type);

  if(IsForAll(type,&lhs,&rhs)){
    int vNo = newVar(deRef(lhs),False);

    outChar(out,forall_sig);
    outChar(out,vNo);

    genSig(out,rhs);
    resetVarNo(vNo);
  }

  else if(IsTypeVar(type)){
    unsigned int i=0;

    for(i=0;i<vartop;i++)
      if(vars[i].var==type){

	outChar(out,var_sig);
	outChar(out,vars[i].varNo);
	return;
      }

    {
      int vNo = newVar(type,True);

      outChar(out,var_sig);
      outChar(out,vNo);
      return;
    }
  }
  else if(isSymb(type)){
    symbpo sym = symVal(type);

    /* Look for standard types */
    if(sym==ksymbol)
      outChar(out,symbol_sig);
    else if(sym==knumberTp)
      outChar(out,number_sig);
    else if(sym==khandleTp)
      outChar(out,handle_sig);
    else if(sym==klogicalTp)
      outChar(out,logical_sig);
    else if(sym==kanyTp)
      outChar(out,unknown_sig);
    else if(sym==kopaqueTp)
      outChar(out,opaque_sig);
    else{
      outChar(out,usr_sig);
      putSymbol(out,sym);
    }
  }

  else if(isUnaryCall(type,klistTp,&lhs) && IsSymbol(deRef(lhs),kcharTp))
    outChar(out,string_sig);
    
  else if(isBinaryCall(type,kfunTp,&lhs,&rhs)){
    outChar(out,funct_sig);               /* We have a function type */
    genSig(out,lhs);                    /* argument types */
    genSig(out,rhs);		        /* result types */
  }
  else if(isCall(type,kprocTp,&ar)){
    long i;
    
    outChar(out,proc_sig);
    outChar(out,tuple_sig);		/* We lose names */
    outChar(out,ar);
    for(i=0;i<ar;i++)
      genSig(out,callArg(type,i)); /* signature of each element */
  }
  else if(isUnaryCall(type,klistTp,&rhs)){
    outChar(out,list_sig);		/* A list type */
    genSig(out,rhs);
  }

  else if(IsQuery(type,&type,&rhs)){
    outChar(out,query_sig);
    putSymbol(out,symVal(rhs));
    genSig(out,type);
  }

  /* Polymorphic type */
  else if(IsStruct(type,&fn,&ar)){
    long i;
    
    outChar(out,poly_sig);
    putSymbol(out,symVal(fn));
    outChar(out,ar);
    for(i=0;i<ar;i++)
      genSig(out,callArg(type,i)); /* signature of each element */
  }
  
  else if(IsTuple(type,&ar) && ar<255){	/* Regular tuple type */
    int i;

    outChar(out,tuple_sig);		/* We lose names */
    outChar(out,ar);
    for(i=0;i<ar;i++)
      genSig(out,tplArg(type,i)); /* signature of each element */
  }
  else
    syserr("Invalid type signature requested");
}

static void putSymbol(ioPo out,uniChar *sym)
{
  char *delims = "'\"/#$\\A0"; /* Potential delimiters */
  long len = uniStrLen(sym);

  while(*delims!='\0' && uniSearch(sym,len,(uniChar)*delims)!=NULL)
    delims++;		/* look for a suitable delimiter character */

  outChar(out,*delims);
  while(*sym!='\0')
    outChar(out,*sym++);
  outChar(out,*delims);
}





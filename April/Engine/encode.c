/*
  Term encoding functions
  (c) 1994-2002 Imperial College and F.G. McCabe

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
#include <math.h>
#include <string.h>
#include <assert.h>

#include "april.h"
#include "process.h"
#include "ioP.h"
#include "dict.h"
#include "symbols.h"
#include "sign.h"
#include "astring.h"		/* String handling interface */
#include "encoding.h"
#include "labels.h"             // Support for label management

static retCode encode(ioPo out,objPo input,lblPo chain,lblPo tvars);
static logical IsTupleOfCode(objPo input);
static lblPo collectTvars(objPo input,lblPo stack,lblPo chain);

#define ICM_VAL_MASK 0x0f
#define ICM_TAG_MASK 0xf0

#define try(S) {retCode ret = S; if(ret!=Ok) return ret;}


retCode encodeICM(ioPo out,objPo input)
{
#ifdef MEMTRACE
  logical allowed = permitGC(False);
#endif
  lblPo tVars = collectTvars(input,NULL,NULL);
  retCode ret = encode(out,input,NULL,tVars);
  
  tVars = freeAllLabels(tVars);
  
#ifdef MEMTRACE
  permitGC(allowed);
#endif
  return ret;
}

retCode encodeInt(ioPo out,WORD32 val,int tag)
{
  int len=0,i;
  unsigned char bytes[16];
  unsigned WORD32 v = val;

  if(v==0)			/* special case for 0 to allow large numbers */
    bytes[len++]=0;
  else
    while(v>0){
      bytes[len++]=v&0xff;
      v >>= 8;
    }

  if(val>0 && (bytes[len-1]&0x80)) /* correct issues to do with signs */
    bytes[len++]=0;

  outByte(out,tag|len);
  for(i=len;i>0;i--)
    outByte(out,bytes[i-1]);

  return Ok;
}

static retCode encode(ioPo out,objPo input,lblPo chain,lblPo tvars)
{
  lblPo links = chain;
  
  while(links!=NULL){
    if(links->term==input)
      return encodeInt(out,links->ref,trmRef);
    links=links->prev;
  }

  switch(Tag(input)){
  case variableMarker:{
    input = deRefVar(input);
    
    if(Tag(input)==variableMarker){
      int ref = searchLabels(input,tvars);
      
      if(ref>=0)
        return encodeInt(out,ref,trmVariable);
      else
        return Error;
    }
    else
      return encode(out,input,chain,tvars);
  }
  
  case integerMarker:
    return encodeInt(out,IntVal(input),trmInt);

  case floatMarker: {
    unsigned char buff[16], *bf = buff;
    int i,exp,len,sign=trmFlt;
    Number f = FloatVal(input);

    /* Convert floating point into buffer first -- to get the length */

    if(f<0.0){
      sign = trmNegFlt;
      f = -f;
    }

    f = frexp(f, &exp);

    for(len=0;f!=0.0;len++){
      Number ip;
      f = modf(f*256,&ip);

      *bf++ = (int)ip;
    }

    outByte(out,sign|len);	/* the lead character of the number */
    encodeInt(out,exp,trmInt);	/* the exponent */

    for(i=0;i<len;i++)
      outByte(out,buff[i]);

    return Ok;
  }

  case charMarker:
    return encodeInt(out,CharVal(input),trmChar);

  case symbolMarker:{
    uniChar *sym = SymVal(input);
    int len = uniStrLen(sym);
    unsigned char buff[3*len+1];
    WORD32 ulen = uni_utf8(sym,len,buff,3*len+1);
    int i;

    encodeInt(out,ulen,trmSym);	/* the exponent */
    for(i=0;i<ulen;i++)
      outByte(out,buff[i]);

    return Ok;
  }
  
  case codeMarker:{
    ioPo tmpFile = openOutStr(rawEncoding);
    objPo *tmp;
    WORD32 i,size,litcnt;
    WORD32 signature = SIGNATURE;	/* A special signature word */
    insPo PC = CodeCode(input);

    /* write out a signature to handle little endians and big endians */
    outByte(tmpFile,((signature>>24)&0xff));
    outByte(tmpFile,((signature>>16)&0xff));
    outByte(tmpFile,((signature>>8)&0xff));
    outByte(tmpFile,(signature&0xff));

    size = CodeSize(input);	/* Compute the size of the code section */
    litcnt = CodeLitcnt(input);

    encodeInt(tmpFile,size,trmInt); /* code size */
    encodeInt(tmpFile,litcnt,trmInt); /* number of literal values */

    /* write out the code */
    for(i=0;i<size;i++){
      outByte(tmpFile,((PC[i]>>24)&0xff));
      outByte(tmpFile,((PC[i]>>16)&0xff));
      outByte(tmpFile,((PC[i]>>8)&0xff));
      outByte(tmpFile,(PC[i]&0xff));
    }

    tmp = CodeLits(input);		/* write out the literals in the code */
    for(;litcnt--;)		
      encode(tmpFile,*tmp++,NULL,tvars);
    encode(tmpFile,CodeSig(input),NULL,tvars); /* write out the type signature */
    encode(tmpFile,CodeFrSig(input),NULL,tvars);	/* write out the free type signature */
    
    {
      WORD32 blen;
      uniChar *buffer = getStrText(O_STRING(tmpFile),&blen);
      int i;
      retCode ret = Ok;

      encodeInt(out,blen,trmCode);      /* How big is our code? */
      for(i=0;ret==Ok && i<blen;i++)
        ret=outByte(out,buffer[i]);           /* complete package */
    }

    closeFile(tmpFile);	        /* we are done with the temporary string file */
    return Ok;
  }

  case listMarker:
    if(isEmptyList(input)){
      outByte(out,trmNil);
      return Ok;
    }
    else if(isListOfChars(input)){      // Write out the string in a single block
      WORD32 len = ListLen(input);
      uniChar buffer[MAX_SYMB_LEN];
      uniChar *buff = (len<NumberOf(buffer)?buffer:((uniChar*)malloc(sizeof(uniChar)*(len+1))));
    
      StringText(input,buff,len+1);
      
      {                                 // Check for unicode form
        integer i;
        uniChar code = 0;
          
        for(i=0;i<len;i++){
          if((buff[i]&0xff)!=buff[i]){
            code = uniBOM;            // Unicode Byte Order Mark
            break;
          }
        }
          
        if(code==uniBOM){
          retCode ret = encodeInt(out,(len+1)*sizeof(uniChar),trmString);
          if(ret==Ok){
            outByte(out,((uniBOM>>8)&0xff));
             outByte(out,(uniBOM&0xff));
            for(i=0;ret==Ok && i<len;i++){
              ret = outByte(out,(buff[i]>>8)&0xff);
              if(ret==Ok)
                outByte(out,(buff[i]&0xff));
            }
          }

          if(buff!=buffer)
            free(buff);
          return ret;
        }
        else{
          retCode ret = encodeInt(out,len,trmString);
            
          for(i=0;ret==Ok && i<len;i++)
            ret = outByte(out,buff[i]&0xff);
        
          if(buff!=buffer)
            free(buff);

          return ret;
        }
      }
    }
    else{
      outByte(out,trmList);
      encode(out,ListHead(input),chain,tvars);	/* Encode the head of the list */
      return encode(out,ListTail(input),chain,tvars); /* And the tail */
    }

  case consMarker:{
    if(IsHandle(input)){
      objPo *ptr = consData(input);

      outByte(out,trmHdl);	/* we write a handle */
      
      encode(out,*ptr++,chain,tvars);       /* The target of the handle */
      return encode(out,*ptr++,chain,tvars);/* The name of the handle */
    }
    else{
      unsigned int ar = consArity(input);
      objPo *ptr = consData(input);
      LabelRec link = { input, (chain==NULL?0:chain->ref+1), chain};
    
    /* Do we need to handle circular structures? */
      if(isClosure(input)){
        chain = &link;
        encodeInt(out,chain->ref,trmTag);
      }
    
      encodeInt(out,ar,trmStruct); /* write out the marker+arity */
      encode(out,((consPo)input)->fn,NULL,tvars);
    
      for(;ar-->0;)
        try(encode(out,*ptr++,chain,tvars));
      return Ok;
    }
  }
  
  case tupleMarker:{
    int ar = tupleArity(input);
    objPo *ptr = tupleData(input);
    LabelRec link = { input, (chain==NULL?0:chain->ref+1), chain};
    
    /* Do we need to handle circular structures? */
    if(IsTupleOfCode(input)){
      chain = &link;
      encodeInt(out,chain->ref,trmTag);
    }
    
    encodeInt(out,ar,trmStruct); /* write out the marker+arity */
    encode(out,ktpl,NULL,NULL);
    
    for(;ar--;)
      try(encode(out,*ptr++,chain,tvars));
    return Ok;
  }
  
  case anyMarker:
    outByte(out,trmSigned);
    encode(out,AnySig(input),chain,tvars); /* Encode the signature */
    return encode(out,AnyData(input),chain,tvars); /* and the value itself */

  default:			/* in particular, opaque types not allowed */
    return Error;
  }
}

static logical IsTupleOfCode(objPo input)
{
  WORD32 arity = tupleArity(input);
  objPo *ptr = tupleData(input);
  WORD32 i;

  for(i=0;i<arity;i++,ptr++){
    if(isClosure(*ptr))
      return True;
  }
  return False;
}

static lblPo collectTvars(objPo input,lblPo stack,lblPo chain)
{
  input = deRefVar(input);
  LabelRec deeper = { input,-1,stack};

  while(stack!=NULL){
    if(stack->term==input)
      return chain;
    else
      stack = stack->prev;
  }
  
  switch(Tag(input)){
    case variableMarker:{
      objPo in = deRefVar(input);
       
      if(in==input){
        lblPo ch = chain;
        int vNo = 0;
      
        while(ch!=NULL){
          if(ch->term==in)
            return chain;
          if(ch->ref>vNo)
            vNo = ch->ref+1;
          ch = ch->prev;
        }
        
        return createLabel(input,vNo++,chain);
      }
      else
        return collectTvars(in,&deeper,chain);
    }
    case listMarker:{
      while(isNonEmptyList(input)){
        chain = collectTvars(ListHead(input),&deeper,chain);
        input = ListTail(input);
      }
      if(!isEmptyList(input))
        return collectTvars(input,&deeper,chain);
      else
        return chain;
    }
    case consMarker:{
      unsigned WORD32 ar = consArity(input);
      unsigned WORD32 i;
      
      for(i=0;i<ar;i++)
        chain = collectTvars(consEl(input,i),&deeper,chain);
      return chain;
    }

    case tupleMarker:{
      WORD32 ar = tupleArity(input);
      WORD32 i;
      
      for(i=0;i<ar;i++)
        chain = collectTvars(tupleArg(input,i),&deeper,chain);
      return chain;
    }
    case anyMarker:
      return collectTvars(AnyData(input),&deeper,
                          collectTvars(AnySig(input),&deeper,chain));
    default:
      return chain;
  }
}


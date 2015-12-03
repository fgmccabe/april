/*
  Term decoding functions
  (c) 1994-1999 Imperial College and F.G. McCabe

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
#include <assert.h>
#include <string.h>

#include "april.h"
#include "process.h"
#include "dict.h"
#include "symbols.h"
#include "sign.h"
#include "astring.h"		/* String handling interface */
#include "gcP.h"
#include "term.h"
#include "ioP.h"
#include "encoding.h"
#include "labels.h"

/* Decode an ICM message ... from the file stream */

static retCode decode(ioPo in,WORD64 lbl,objPo *tgt,logical verify);

static objPo *lbls=NULL;	/* these labels are for circular refs ... */
static WORD32 maxlbl = 0;

#define ICM_VAL_MASK 0x0f
#define ICM_TAG_MASK 0xf0

#define try(S) {retCode ret = S; if(ret!=Ok) return ret;}

static lblPo tvars = NULL;
static lblPo refs = NULL;
/* 
 * Decode a structure from an input stream
 */
retCode decodeICM(ioPo in,objPo *tgt,logical verify)
{
  retCode ret = Ok;
  
  if(lbls==NULL){
    lbls = (objPo *)malloc(sizeof(objPo)*128);
    if(lbls==NULL)
      return Error;
    else
      maxlbl = 128;
  }
  
  strMsg(errorMsg,NumberOf(errorMsg),"");
  
  memset(lbls,0,sizeof(objPo)*maxlbl); /* clear the label table */
  tvars = NULL;
  refs = NULL;
  ret = decode(in,-1,tgt,verify);
  
  tvars = freeAllLabels(tvars);
  refs = freeAllLabels(refs);
  
  return ret;
}

retCode decInt(ioPo in,integer *ii,uniChar tag)
{
  integer len = tag&ICM_VAL_MASK;
  integer result = 0;
  int i;
  retCode ret = Ok;
  uniChar ch;

  if(len==0)			/* recursive length */
    ret = decInt(in,&len,inCh(in));

  for(i=0;i<len;i++){
    ch = inCh(in);
    
    if(i==0)
      result = (signed char)(ch&0xff);
    else
      result = (result<<8)|ch;
  }

  if(ii!=NULL)
    *ii = result;
  return ret;
}

static retCode decodeCode(ioPo str,objPo *tgt,logical verify);

/*
  Warning: caller assumes responsibility for ensuring that tgt is a valid rot
*/
static retCode decode(ioPo in,integer lbl,objPo *tgt,logical verify)
{
  uniChar ch = inCh(in);
  retCode res=Ok;

  if(ch==uniEOF)
    return Eof;

  switch(ch & ICM_TAG_MASK){
  case trmVariable:{
    integer v;
    
    if((res=decInt(in,&v,ch))!=Ok)
      return res;
    else{
      objPo ref = findObj(v,tvars);
      
      if(ref==NULL){
        ref = allocateVariable();
        
        tvars = createLabel(ref,v,tvars);
      }
      
      *tgt = ref;
      return Ok;
    }
  }
  case trmInt:{
    integer i;
    if((res=decInt(in,&i,ch))!=Ok)
      return res;
    else{
      objPo el = allocateInteger(i);
      *tgt= el;
      return Ok;
    }
  }
  case trmFlt:{
    integer exp;
    unsigned short len = ch&ICM_VAL_MASK;
    unsigned char buff[16];
    double f = 0.0;
    int i;

    if((res=decInt(in,&exp,inCh(in)))!=Ok)
      return res;

    for(i=0;i<len;i++)
      buff[i]=inCh(in);

    while(len--)
      f = (f + buff[len])/256;

    f = ldexp(f,exp);
    *tgt = allocateFloat(f);
    return Ok;
  }

  case trmNegFlt:{
    integer exp;
    unsigned short len = ch&ICM_VAL_MASK;
    unsigned char buff[16];
    double f = 0.0;
    int i;

    if((res=decInt(in,&exp,inCh(in)))!=Ok)
      return res;

    for(i=0;i<len;i++)
      buff[i]=inCh(in);

    while(len--)
      f = (f + buff[len])/256;

    f = ldexp(f,exp);
    *tgt = allocateFloat(-f);
    return Ok;
  }

  case trmSym:{
    integer len;

    if((res=decInt(in,&len,ch))!=Ok)
      return res;
    else{
      char sbuff[MAX_SYMB_LEN];
      char *sym = (len<NumberOf(sbuff)?sbuff:(char*)malloc((len+1)*sizeof(char)));
      int i;
      
      for(i=0;i<len;i++){
        ch = inCh(in);
        
        if(ch==uniEOF)
          return Eof;
        sym[i]=ch;
      }
      sym[i]='\0';
      
      {
        objPo el = newSymbol(sym);
        *tgt = el;                      /* we do this to ensure correct order of evaluation */
      }

      if(sym!=sbuff)
        free(sym);
      return Ok;
    }
  }
  
  case trmChar:{
    integer k;
    
    if((res=decInt(in,&k,ch))!=Ok)
      return res;
    else{
      objPo el = allocateChar((uniChar)k);
      *tgt = el;
    }
    return Ok;
  }

  case trmString:{
    integer len;

    if((res=decInt(in,&len,ch))!=Ok)
      return res;
    else{
      byte bBuff[2048];
      byte *bData = (len<NumberOf(bBuff)?bBuff:(byte*)(malloc(len*sizeof(byte)))); 
      WORD32 blen;
      uniChar uBuff[2048];
      uniChar *uData = (len<NumberOf(uBuff)?uBuff:(uniChar*)(malloc(len*sizeof(uniChar))));
      int i;
      
      res=inBytes(in,bData,len,&blen); /* read in a block of bytes */
      
      if(len>=2 && bData[0]==uniBOMhi && bData[1]==uniBOMlo){
        integer j;
        for(i=2,j=0;i<len;i+=2,j++)
          uData[j]=((bData[i]&0xff)<<8)|(bData[i+1]&0xff);
        len = j;
      }
      else if(len>=2 && uData[1]==uniBOMhi && uData[0]==uniBOMlo){
        integer j;
        for(i=2,j=0;i<len;i+=2,j++)
          uData[j]=((bData[i+1]&0xff)<<8)|(bData[i]&0xff);
        len=j;
      }
      else{
        for(i=0;i<len;i++)
          uData[i] = bData[i]&0xff;
      }

      if(res==Ok){
        objPo el = allocateSubString(uData,len);
        *tgt = el;
      }

      if(uData!=uBuff)
        free(uData);
      if(bData!=bBuff)
        free(bData);
    }
    return res;
  }

  case trmCode:{
    integer len;

    if((res=decInt(in,&len,ch))!=Ok)
      return res;
    else{
      uniChar buffBlock[2048];
      uniChar *buff = (len<NumberOf(buffBlock)?buffBlock:(uniChar*)malloc(sizeof(uniChar)*len));
      int i;

      res = Ok;
      for(i=0;res==Ok && i<len;i++){     /* read in a block of bytes */
        byte b;
        res = inByte(in,&b);
        buff[i]=b&0xff;
      }

      if(res==Ok){
        ioPo str = openInStr(buff,len,rawEncoding);
        res = decodeCode(str,tgt,verify);
        closeFile(str);
      }
      else
        res = Error;
        
      if(buff!=buffBlock)
        free(buff);
    }
    return res;
  }

  case trmNil:
  case trmList:
  case trmHdl:
  case trmSigned:
    if(ch==trmNil){
      *tgt = emptyList;
      return Ok;
    }
    else if(ch==trmList){
      objPo lst = *tgt = emptyList;
      objPo el = kvoid;
      void *root = gcAddRoot(&lst);
      
      gcAddRoot(&el);
      
      while(res==Ok && ch==trmList){
        res = decode(in,-1,&el,verify);
        
        if(res==Ok){
          if(lst==emptyList){
            lst = allocatePair(&el,&emptyList);
            *tgt = lst;
          }
          else{
            el = allocatePair(&el,&emptyList);
            updateListTail(lst,el);
            lst = el;
          }

          if((ch=inCh(in))==uniEOF)
            res=Eof;
        }
      }
      if(ch!=trmNil){
        unGetChar(in,ch);
        res = decode(in,-1,&el,verify);
        if(res==Ok){
          if(lst==emptyList)
            *tgt = el;
          else
            updateListTail(lst,el);
        }
      }
      gcRemoveRoot(root);
      return res;
    }
    else if(ch==trmHdl){
      uniChar t[MAX_SYMB_LEN];
      uniChar name[MAX_SYMB_LEN];
      objPo harg = allocateConstructor(HANDLE_ARITY);
      objPo el = kvoid;
      void *root = gcAddRoot(&harg);

      gcAddRoot(&el);
      
      updateConsFn(harg,khandle);

      if((res=decode(in,-1,&el,verify))!=Ok)   /* read handle target */
        return res;		        /* we might need to skip out early */
      else{
        if(isSymb(el))
          uniCpy(t,NumberOf(t),SymText(el));
        else{
          gcRemoveRoot(root);
          return Error;                 // The parts of a handle must be symbol
        }
        updateConsEl(harg,TGT_OFFSET,el);
      }

      if((res=decode(in,-1,&el,verify))!=Ok) /* read handle name */
        return res;
      else{
        if(isSymb(el))
          uniCpy(name,NumberOf(name),SymText(el));
        else{
          gcRemoveRoot(root);
          return Error;           // The parts of a handle must be symbol
        }
        updateConsEl(harg,NAME_OFFSET,el);
      }

      updateConsEl(harg,PROCESS_OFFSET,locateHandle(t,name,NULL));

      *tgt = harg;

      gcRemoveRoot(root);
      return Ok;
    }
    else if(ch==trmSigned){
      objPo any = allocateAny(&kvoid,&kvoid);
      objPo el = kvoid;
      void *root = gcAddRoot(&any);

      gcAddRoot(&el);

      *tgt = any;

      if((res=decode(in,-1,&el,verify))!=Ok){
        gcRemoveRoot(root);
        return res;
      }

      updateAnySig(any,el);
      if((res=decode(in,-1,&el,verify))!=Ok){
        gcRemoveRoot(root);
        return res;
      }
      updateAnyData(any,el);
      gcRemoveRoot(root);
      return Ok;
    }
    else
      return Error;
      
  case trmStruct:{
    integer len;
    objPo el = kvoid;
    WORD32 i;
    void *root = gcAddRoot(&el);

    if((res=decInt(in,&len,ch))!=Ok)
      return res;
      
    if((res=decode(in,-1,&el,verify))!=Ok){
      gcRemoveRoot(root);
      return res;
    }
    if(el==ktpl){
      objPo tpl;
      if((tpl=allocateTuple(len))==NULL)
        return SpaceErr();

      *tgt=tpl;

      gcAddRoot(&tpl);

      if(lbl>=0)
        lbls[lbl] = tpl;		/* Update the label table */

      for(i=0;res==Ok && i<len;i++){
        if((res=decode(in,-1,&el,verify))!=Ok) /* read each tuple element */
	  break;			/* we might need to skip out early */
        else
	  updateTuple(tpl,i,el);	/* stuff in the new element */
      }
    }
    else{
      objPo cns = allocateConstructor(len);

      *tgt = cns;

      gcAddRoot(&cns);

      if(lbl>=0)
        lbls[lbl] = cns;		/* Update the label table */
        
      ((consPo)cns)->fn = el;           // Establish the constructor symbol

      for(i=0;res==Ok&&i<len;i++){
        if((res=decode(in,-1,&el,verify))!=Ok) /* read each tuple element */
	  break;			/* we might need to skip out early */
        else
	  updateConsEl(cns,i,el);	/* stuff in the new element */
      }
    }
    
    gcRemoveRoot(root);
    return res;
  }

  case trmTag:{
    assert(lbl==-1);

    if((res=decInt(in,&lbl,ch))!=Ok)
      return res;

    if(lbl>maxlbl){
      WORD32 newmax = lbl+(maxlbl>>1); /* 50% growth */
      objPo *newlabels = (objPo *)realloc(lbls,sizeof(objPo)*newmax);

      if(newlabels==NULL)
	return SpaceErr();
      else{
	lbls = newlabels;
	maxlbl = newmax;
      }
    }

    return decode(in,lbl,tgt,verify); /* decode the defined structure */
  }

  case trmRef:{
    integer lbl;

    if((res=decInt(in,&lbl,ch))!=Ok)
      return res;
    *tgt = lbls[lbl];
    return Ok;
  }
  default:
    return Error;
  }
}


/* swap bytes in the little endian game */
static inline void SwapBytes(unsigned WORD32 *x)
{
  *x=((*x&0xff)<<8)|((*x>>8)&0xff)|((*x&0x00ff0000L)<<8)|((*x&0xff000000L)>>8);
}

static inline void SwapWords(unsigned WORD32 *x)
{
  *x = (*x&0x0000ffffL)<<16 | (*x&0xffff0000L)>>16;
}

static retCode decodeCode(ioPo in,objPo *tgt,logical verify)
{
  integer size;
  integer litcnt;
  unsigned WORD32 arity,i;
  unsigned WORD32 signature = (inCh(in)&0xff)<<24|
    (inCh(in)&0xff)<<16|
    (inCh(in)&0xff)<<8|
    (inCh(in)&0xff);
  objPo pc=kvoid;
  objPo el=kvoid;
  entrytype atype;
  objPo *tmp;
  retCode res;
  void *root = gcAddRoot(&pc);	/* in case of GC ... */

  gcAddRoot(&el);		/* we need a temporary pointer */

  if(signature!=SIGNATURE &&	/* endian load same as endian save */
     signature!=SIGNBSWAP &&	/* swap bytes keep words */
     signature!=SIGNWSWAP &&	/* swap words keep bytes */
     signature!=SIGNBWSWP)	/* swap words and bytes */
    return Error;

  if((res=decInt(in,&size,inCh(in)))!=Ok)
    return res;
  if((res=decInt(in,&litcnt,inCh(in)))!=Ok)
    return res;

  if((pc=allocateCode(size,litcnt))==NULL)
    return SpaceErr();	/* no room for the code block */
  *tgt = pc;

  ((codePo)pc)->litcnt = litcnt;
  ((codePo)pc)->size = size;

  tmp = CodeLits(pc);		/* clear out the literals in case of GC */

  for(i=0;i<litcnt;i++)
    *tmp++=kvoid;
    
  ((codePo)pc)->type = kvoid;
  ((codePo)pc)->frtype = kvoid;

  /* get the instructions */
  {
    int i;
    insPo cd = CodeCode(pc);
    
    for(i=0;i<size;i++)
      cd[i]=(inCh(in)&0xff)<<24| (inCh(in)&0xff)<<16|
	  (inCh(in)&0xff)<<8| (inCh(in)&0xff);
  }
  
  /* Now convert the main code to handle little endians etc */
  if(signature==SIGNATURE){}	/* endian load same as endian save */
  else if(signature==SIGNBSWAP){ /* swap bytes keep words */
    unsigned WORD32 *xx=(unsigned WORD32*)CodeCode(pc);
    WORD32 cnt = size;
    for(;cnt--;xx++) 
      SwapBytes(xx);
  }
  else if(signature==SIGNWSWAP){ /* swap words keep bytes */
    unsigned WORD32 *xx=(unsigned WORD32*)CodeCode(pc);
    WORD32 cnt = size;
    for(;cnt--;xx++) 
      SwapWords(xx);
  }
  else if(signature==SIGNBWSWP){ /* swap words and bytes */
    unsigned WORD32 *xx=(unsigned WORD32*)CodeCode(pc);
    WORD32 cnt = size;
    for(;cnt--;xx++){
      SwapWords(xx);
      SwapBytes(xx);
    }
  }
  
  /* Now get the code's literals and type signatures */
  for(i=0;i<litcnt;i++)
    if((res=decode(in,-1,&el,verify))!=Ok)
      return res;
    else
      updateCodeLit(pc,i,el);

  /* decode the type signature */
  if((res=decode(in,-1,&el,verify))!=Ok)
    return res;
  else
    updateCodeSig(pc,el);

  {
    objPo type = CodeSig(pc);
    
    while(isBinCall(type=deRefVar(type),kallQ,NULL,&type))
      ;
      
    if(isBinCall(type,kfunTp,&type,NULL)){
      atype = function;
      arity = consArity(type);
    }
    else if(isConstructor(type,kprocTp)){
      atype = procedure;
      arity = consArity(type);
    }
    else
      return Error;
  }

  SetCodeFormArity(pc,atype,arity);

  /* decode the free type signature */
  if((res=decode(in,-1,&el,verify))!=Ok)
    return res;
  else
    updateCodeFrSig(pc,el);

  gcRemoveRoot(root);		/* now for the verification */
  
  if(verify){
    char *msg = codeVerify(*tgt,True);
    
    if(msg!=NULL){
      strMsg(errorMsg,NumberOf(errorMsg),"error in code: %s",msg);
      return Error;
    }
  }
  
  return Ok;                    // Now we're done
}

void scanLabels(void)
{
  WORD32 i;
  lblPo tv = tvars;
  
  for(i=0;i<maxlbl;i++)
    lbls[i] = scanCell(lbls[i]);
    
  while(tv!=NULL){
    tv->term = scanCell(tv->term);
    tv = tv->prev;
  }
}

void markLabels(void)
{
  WORD32 i;
  lblPo tv = tvars;

  for(i=0;i<maxlbl;i++)
    markCell(lbls[i]);
    
  while(tv!=NULL){
    markCell(tv->term);
    tv = tv->prev;
  }
}

void adjustLabels(void)
{
  WORD32 i;
  lblPo tv = tvars;

  for(i=0;i<maxlbl;i++)
    lbls[i] = adjustCell(lbls[i]);
    
  while(tv!=NULL){
    tv->term = adjustCell(tv->term);
    tv = tv->prev;
  }
}


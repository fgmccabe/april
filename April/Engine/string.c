/*
  String handling functions for the April run-time system
  (c) 1994-2002 Imperial College & F.G. McCabe

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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <stdarg.h>
#include <assert.h>

#include "april.h"
#include "ioP.h"
#include "symbols.h"
#include "process.h"
#include "dict.h"
#include "astring.h"
#include "term.h"

/* Copy an April string into a C buffer -- assuming no \0's in string */
char *Cstring(objPo lst,char *buffer,unsigned WORD32 bufflen)
{
  char *p = buffer;
    
  while(--bufflen>0 && isNonEmptyList(lst)){
    *p++=CharVal(ListHead(lst));
    lst = ListTail(lst);
  }
    
  *p++='\0';
  return buffer;
}

logical isListOfChars(objPo p)
{
  while(isNonEmptyList(p)){
    if(!isChr(ListHead(p)))
      return False;
    else
      p = ListTail(p);
  }
  return isEmptyList(p);
}

uniChar *StringText(objPo p,uniChar *buffer,unsigned WORD32 len)
{
  uniChar *txt = buffer;
  
  assert(isListOfChars(p));
  
  while(isNonEmptyList(p) && --len>0){
    *txt++=CharVal(ListHead(p));
    p = ListTail(p);
  }
  
  *txt=0;
  return buffer;
}

/* Built in string functions */

/*
 * explode
 * 
 * any April object into a string
 */
#define NUMLEN 64

retCode m_explode(processpo p, objPo *args)
{
  objPo data = args[0];
  
  if(!isSymb(data))
    return liberror("explode",1,"symbol expected",einval);
  else{
    args[0] = allocateString(SymText(data));
    return Ok;
  }
}

/*
 * implode
 * 
 * string or scalar into symbol
 */

retCode m_implode(processpo p, objPo *args)
{
  if(isSymb(*args))
    return Ok;
  else if(isListOfChars(*args)){
    WORD32 tlen = ListLen(args[0]);
    uniChar buffer[MAX_SYMB_LEN];
    uniChar *tbuff = (tlen+2<=NumberOf(buffer)?buffer:((uniChar*)malloc(sizeof(uniChar)*(tlen+2))));
    
    StringText(args[0],tbuff+1,tlen+1);
    tbuff[0]='\'';              // Insert the leading quote to make this a symbol
    args[0]=newUniSymbol(tbuff);

    if(tbuff!=buffer)
      free(tbuff);
    return Ok;
  }
  else
    return liberror("implode",1,"argument should be a string",einval);
}

/*
 * gensym : generate a new symbol.
 */
retCode m_gensym(processpo p, objPo *args)
{
  static int counter=0;
  uniChar name[MAX_SYMB_LEN];

  args[-1] = newUniSymbol(strMsg(name,NumberOf(name),"'noname%d",counter++));
  p->sp=args-1;			/* adjust caller's stack pointer */
  return Ok;
}

/*
 * gensym2 : generate a new symbol.
 */
retCode m_gensym2(processpo p, objPo *args)
{
  static int counter=0;
  uniChar name[MAX_SYMB_LEN];
  uniChar buff[MAX_SYMB_LEN];

  if(isListOfChars(args[0])){
    args[0] = newUniSymbol(strMsg(name,NumberOf(name),"'%U%d",
			       StringText(args[0],buff,NumberOf(buff)),
			       counter++));
    return Ok;
  }
  else
    return liberror("gensym",1,"argument should be a string",einval);
}

/*
 * Expand a string into a list of sub-strings 
 */

retCode m_expand(processpo p, objPo *args)
{
  objPo ss = args[1]; /* string being searched */
  objPo kk = args[0]; /* key string */

  if(!isListOfChars(ss))
    return liberror("expand",2,"1st argument should be a string",einval);
  else if(!isListOfChars(kk))
    return liberror("expand",2,"2nd argument should be a string",einval);
  else{
    WORD32 klen = ListLen(kk);
    uniChar kbuffer[MAX_SYMB_LEN];
    uniChar *kbuff = (klen<NumberOf(kbuffer)?kbuffer:(uniChar*)malloc(sizeof(uniChar)*(klen+1)));
    uniChar *key = StringText(kk,kbuff,klen+1);
    WORD32 tlen = ListLen(ss);
    uniChar tbuffer[MAX_SYMB_LEN];
    uniChar *tbuff = (tlen<NumberOf(tbuffer)?tbuffer:(uniChar*)malloc(sizeof(uniChar)*(tlen+1)));
    uniChar *text = StringText(ss,tbuff,tlen+1);
    WORD32 pos = 0;
    objPo last = args[1] = emptyList;
    objPo el = emptyList;
    void *root = gcAddRoot(&last);

    gcAddRoot(&el);
    gcAddRoot(&ss);

    while(pos<tlen){
      while(pos<tlen && uniSearch(key,klen,text[pos])!=NULL)
	pos++;

      if(pos<tlen){
	WORD32 start = pos;

	while(pos<tlen && uniSearch(key,klen,text[pos])==NULL)
	  pos++;
	el = allocateSubString(&text[start],pos-start);
	el = allocatePair(&el,&emptyList);

	if(last==emptyList)
	  args[1] = last = el;
	else{
	  objPo new = el;
	  updateListTail(last,new);
	  last = new;
	}
      }
    }

    if(kbuff!=kbuffer)
      free(kbuff);
    if(tbuff!=tbuffer)
      free(tbuff);

    gcRemoveRoot(root);
    return Ok;
  }
}
  
  
/*
 * Collapse a list of strings into a single string
 */

retCode m_collapse(processpo p, objPo *args)
{
  objPo arg1 = args[1];
  objPo arg2 = args[0];

  if(!IsList(arg1))
    return liberror("collapse",2,"1st argument should be a list",einval);
  else if(!isListOfChars(arg2))
    return liberror("collapse",2,"2nd argument should be a string",einval);
  else{
    WORD32 len2 = ListLen(arg2)+1;
    uniChar buffer1[MAX_SYMB_LEN];
    uniChar *gbuff = (len2<NumberOf(buffer1)?buffer1:(uniChar*)malloc(sizeof(uniChar)*len2));
    uniChar *glue = StringText(arg2,gbuff,len2);
    ioPo out = openOutStr(utf16Encoding);

    while(isNonEmptyList(arg1)){	/* now copy into output string */
      objPo seg = ListHead(arg1);
      
      if(isListOfChars(seg))
        outMsg(out,"%#L",seg);
      else{
        if(gbuff!=buffer1)
          free(gbuff);
	return liberror("collapse",2,"1st argument should be a list of strings",einval);
      }

      arg1 = ListTail(arg1);

      if(isNonEmptyList(arg1))
	outMsg(out,"%U",glue);
    }

    {
      WORD32 len;
      uniChar *text = getStrText(O_STRING(out),&len);
      
      args[1]=allocateSubString(text,len);
      
      closeFile(out);
      
      return Ok;
    }
  }
}


/*
 * Convert a string to a list of utf codes
 */
retCode m_str2utf(processpo p, objPo *args)
{
  objPo arg1 = args[0];

  if(!isListOfChars(arg1))
    return liberror("str2utf",1,"argument should be a list",einval);
  else{
    WORD32 len = ListLen(arg1)+1;
    uniChar buffer1[MAX_SYMB_LEN];
    uniChar *gbuff = (len<NumberOf(buffer1)?buffer1:(uniChar*)malloc(sizeof(uniChar)*len));
    unsigned char buffer2[MAX_SYMB_LEN];
    unsigned char *ubuff = (3*len<NumberOf(buffer2)?buffer2:(unsigned char*)malloc(sizeof(unsigned char)*(len*3)));
    uniChar *text = StringText(arg1,gbuff,len);
    WORD32 ulen = uni_utf8(text,len,ubuff,len*3);
    int i;
    objPo last = emptyList;
    objPo I = emptyList;
    void *root = gcAddRoot(&last);

    gcAddRoot(&I);
  
    args[0] = emptyList;

    for(i=0;i<ulen;i++){
      I = allocateInteger(ubuff[i]);

      I = allocatePair(&I,&emptyList);

      if(last==emptyList)
	args[0] = last = I;
      else{
	updateListTail(last,I);
	last = I;
      }
    }

    gcRemoveRoot(root);
    if(ubuff!=buffer2)
      free(ubuff);
    if(gbuff!=buffer1)
      free(gbuff);

    return Ok;
  }
}

/*
 * Convert a list of utf codes to a string
 */
retCode m_utf2str(processpo p, objPo *args)
{
  objPo arg1 = args[0];
  WORD32 len = ListLen(arg1);

  if(len<0)
    return liberror("utf2str",1,"argument should be a list of numbers",einval);
  else{
    uniChar buffer1[MAX_SYMB_LEN];
    uniChar *gbuff = (len<NumberOf(buffer1)?buffer1:(uniChar*)malloc(sizeof(uniChar)*len));
    unsigned char buffer2[MAX_SYMB_LEN];
    unsigned char *ubuff = (len<NumberOf(buffer2)?buffer2:(unsigned char*)malloc(sizeof(unsigned char)*(len)));
    int i = 0;

    unsigned char *p = ubuff;

    for(i=0;i<len && isNonEmptyList(arg1);i++){
      if(IsInteger(ListHead(arg1)))
        *p++=IntVal(ListHead(arg1));
      else
        return liberror("utf2str",1,"argument should be a list of numbers",einval);
      arg1 = ListTail(arg1);
    }
      
    len = utf8_uni(ubuff,len,gbuff,len); /* convert to unicode */

    args[0]=allocateSubString(gbuff,len);
    if(ubuff!=buffer2)
      free(ubuff);
    if(gbuff!=buffer1)
      free(gbuff);
    return Ok;
  }
}



// num2str(Value, Width, Precision, Pad, Signed, Scientific)=>string
retCode m_num2str(processpo p, objPo *args)
{
  double val = NumberVal(args[5]);
  WORD32 width = abs(IntVal(args[4]));
  WORD32 prec = IntVal(args[3]);
  uniChar pad = CharVal(args[2]);
  logical sign = LogicalVal(args[1]);
  logical scientific = LogicalVal(args[0]);
  logical left = IntVal(args[4])<0?True:False;
  ioPo out = openOutStr(utf16Encoding);
  retCode res;

  if(IsFloat(args[1]))
    res = outDouble(out,val,scientific?'g':'f',width,prec,pad,left,"",sign);
  else if(IsInteger(args[1]))
    res = outInteger(out,IntVal(args[1]),10,width,prec,pad,left,"",sign);
  else
    return liberror("num2str",6,"invalid argument",einval);

  if(res==Ok){
    WORD32 len;
    uniChar *text = getStrText(O_STRING(out),&len);
      
    args[5]=allocateSubString(text,len);
  }
  
  closeFile(out);

  if(res==Ok)
    return Ok;
  else
    return liberror("num2str",6,"problem in formatting number",efail);
}

retCode m_int2str(processpo p, objPo *args)
{
  integer val = IsFloat(args[4])?(integer)FloatVal(args[4]):IntVal(args[4]);
  WORD32 base = IntVal(args[3]);
  ioPo out = openOutStr(utf16Encoding);
  logical sign = base<0;
  retCode res = outInteger(out,val,abs(base),IntVal(args[2]),0,CharVal(args[1]),args[0]==ktrue,"",sign);

  if(res==Ok){
    WORD32 len;
    uniChar *text = getStrText(O_STRING(out),&len);
      
    args[4]=allocateSubString(text,len);
  }
  
  closeFile(out);

  if(res==Ok)
    return Ok;
  else
    return liberror("int2str",2,"problem in formatting number",efail);
}

retCode m_hdl2str(processpo p, objPo *args)
{
  objPo hdl = args[1];
  logical alt = args[0]==ktrue;
  ioPo out = openOutStr(utf16Encoding);
  retCode res = displayHandle(out,hdl,0,0,alt);

  if(res==Ok){
    WORD32 len;
    uniChar *text = getStrText(O_STRING(out),&len);
      
    args[1]=allocateSubString(text,len);
  }
  
  closeFile(out);
  
  if(res==Ok)
    return Ok;
  else
    return liberror("hdl2str",2,"problem in formatting handle",efail);
}

/* Construct a string representation of a value */
retCode m_strof(processpo p,objPo *args)
{
  objPo obj = args[2];
  integer prec = IntVal(args[1]);
  integer width = IntVal(args[0]);
  ioPo out = openOutStr(utf16Encoding);
  retCode res;
  
  if(prec==0)
    prec = LONG_MAX;

  res = outCell(out,obj,width,prec,False);

  if(res==Ok){
    WORD32 len;
    uniChar *text = getStrText(O_STRING(out),&len);

    if(width!=0){
      uniChar buffer[MAX_SYMB_LEN];
      uniChar *tmp = (abs(width)<NumberOf(buffer)?buffer:(uniChar*)malloc(sizeof(uniChar)*abs(width)));
      WORD32 count=0;

      if(width<0){
	uniChar *dest = &tmp[-width];
	uniChar *src = &text[len];

	while(width++<0){
	  if(len-->0)
	    *--dest=*--src;	/* copy the tail part of the source */
	  else
	    *--dest=' ';	/* pad with spaces -- strpad has more control*/
	  count++;
	}
      }
      else{
	uniChar *dest = tmp;
	uniChar *src = text;

	while(width-->0){
	  if(len-->0)
	    *dest++=*src++;
	  else
	    *dest++=' ';
	  count++;
	}
      }

      args[2] = allocateSubString(tmp,count); 
      if(tmp!=buffer)
        free(tmp);
    }
    else
      args[2] = allocateSubString(text,len);
    closeFile(out);

    return Ok;
  }
  else
    return liberror("strof",3,"problem in formatting value",efail);
}

/* Put a string inside another string 
 * strpad(str,width,pad) => string
 */
retCode m_strpad(processpo p,objPo *args)
{
  objPo val = args[2];
  WORD32 width = IntVal(args[1]);
  uniChar pad = CharVal(args[0]);
  ioPo out = openOutStr(utf16Encoding);

  if(isListOfChars(val)){
    register WORD32 len = ListLen(val);
    uniChar buffer[MAX_SYMB_LEN];
    uniChar *buff = (len<NumberOf(buffer)?buffer:(uniChar*)malloc(sizeof(uniChar)*(len+1)));
    uniChar *text = StringText(val,buff,len+1);
    register WORD32 gaps;

    if(width<0){		/* right justified? */
      width = -width;
      gaps = width-len;
      while(gaps-->0)
	outChar(out,pad);		/* print leading spaces */

      width = width-gaps;
      while(len-->0 && width-->0)
	outChar(out,*text++);
    }
    else{
      gaps = width-len;
      width = width-gaps;

      while(len-->0 && width-->0)
	outChar(out,*text++);

      while(gaps-->0)
	outChar(out,pad); /* print trailing spaces */
    }
    if(buff!=buffer)
      free(buff);
  }
  else
    return liberror("strchop",3,"invalid argument",einval);
    
  {
    WORD32 len;
    uniChar *text = getStrText(O_STRING(out),&len);
      
    args[2]=allocateSubString(text,len);
  }
  
  closeFile(out);

  return Ok;
}
  

/* Generate a string -- used in coerce */
retCode genString(objPo val,objPo *tgt)
{
  ioPo str = openOutStr(utf16Encoding); 
  retCode res = outMsg(str,"%w",val);
  
  if(res==Ok){
    WORD32 len;
    uniChar *text = getStrText(O_STRING(str),&len);
      
    *tgt=allocateSubString(text,len);
  }
  
  closeFile(str);
  
  return res;
}

/* Construct an encoded string from a arbitrary data value */		
retCode m_sencode(processpo p, objPo *args)
{
  ioPo str = openOutStr(rawEncoding);
  retCode res=encodeICM(str,args[0]);

  if(res==Ok){
    WORD32 len;
    uniChar *text = getStrText(O_STRING(str),&len);
      
    args[0]=allocateSubString(text,len);
  }
  
  closeFile(str);

  if(res==Ok)
    return Ok;
  else
    return liberror("sencode",1,"too complex",efail);
}

/* Decode an encoded string */		
retCode m_sdecode(processpo p, objPo *args)
{
  WORD32 len = ListLen(args[0]);
  uniChar buffer[MAX_SYMB_LEN];
  uniChar *buff = (len<NumberOf(buffer)?buffer:((uniChar*)malloc(sizeof(uniChar)*(len+1))));
  ioPo str = openInStr(StringText(args[0],buff,len+1),len,rawEncoding);
  objPo term = kvoid;
  void *root = gcAddRoot(&term);
  retCode res=decodeICM(str,&term,verifyCode);

  closeFile(str);		/* we need to close the channel anyway */

  gcRemoveRoot(root);
  if(buff!=buffer)
    free(buff);

  if(res==Ok){
    args[0]=term;
    return Ok;
  }
  else
    return liberror("sdecode",1,"invalid",efail);
}



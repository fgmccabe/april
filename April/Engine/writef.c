/*
  Data value and formatted write programs
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
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>

#include "april.h"
#include "process.h"
#include "clock.h"
#include "dict.h"
#include "astring.h"		/* String functions */
#include "sign.h"		/* Signature handling definitions */
#include "term.h"
#include "chars.h"

static retCode displayCode(ioPo f,objPo p,WORD32 width,WORD32 prec,logical alt);
static retCode displayAnyVal(ioPo f,objPo p,WORD32 width,WORD32 prec,logical alt);
/*
 * write a cell in a basic format
 * The depth argument limits the depth that tuples are printed to
 */

typedef struct {
  ioPo f;
  WORD32 width;			/* This is used by writef */
} StrInfoRec;

static retCode outHex(ioPo f,int H)
{
  if(H<10)
    return outChar(f,'0'+H);
  else
    return outChar(f,'a'+(H-10));
}

retCode wStringChr(ioPo f,uniChar ch)
{
  switch(ch){
  case '\a':
    outStr(f,"\\a");
    break;
  case '\b':
    outStr(f,"\\b");
    break;
  case '\x7f':
    outStr(f,"\\d");
    break;
  case '\x1b':
    outStr(f,"\\e");
    break;
  case '\f':
    outStr(f,"\\f");
    break;
  case '\n': 
    outStr(f,"\\n");
    break;
  case '\r': 
    outStr(f,"\\r");
    break;
  case '\t': 
    outStr(f,"\\t");
    break;
  case '\v': 
    outStr(f,"\\v");
    break;
  case '\\':
    outStr(f,"\\\\");
    break;
  case '\"':
    outStr(f,"\\\"");
    break;
  default:
    if((ch&0177)<' '||!isChar(ch)){
      outChar(f,'\\'); outChar(f,'+');
      outHex(f,(ch>>12)&0xf);
      outHex(f,(ch>>8)&0xf);
      outHex(f,(ch>>4)&0xf);
      outHex(f,(ch&0xf));
    }
    else
      outChar(f,ch);
  }
  return Ok;
}

retCode outCell(ioPo f,objPo p,WORD32 width,int prec,logical alt) 
{
  if(p!=NULL){
    switch(Tag(p)){
    case variableMarker:
      return outMsg(f,"%%_%x",p);
      
    case integerMarker:		/* an integer value */
      return outInt(f,IntVal(p));

    case symbolMarker:{		/* a symbol structure */
      uniChar *sym = SymVal(p);
      
      if(alt && *sym=='\'')
        sym++;
      return outMsg(f,"%U",sym);
    }
    
    case charMarker:
      if(alt)
        return outChar(f,CharVal(p));
      else{
        outStr(f,"''");
        return wStringChr(f,CharVal(p));
      }

    case floatMarker:		/* a floating point number */
      return outFloat(f,FloatVal(p));

    case listMarker:
      if(isEmptyList(p))
	return outStr(f,"[]");
      else if(isListOfChars(p)){
        retCode ret = outChar(f,'"');
        if(width<=0)
          width=LONG_MAX;
        
        while(ret==Ok && isNonEmptyList(p)&&width-->0){
	  ret=wStringChr(f,CharVal(ListHead(p)));
          p = ListTail(p);
        }
        
        if(ret==Ok)
          ret = outChar(f,'"');
        return ret;
      }
      else
	if(prec>0){
	  outChar(f,'[');
	  while(isNonEmptyList(p)){
	    outCell(f,ListHead(p),width,prec-1,alt);

	    p=ListTail(p);
	    if(isNonEmptyList(p))
	      outChar(f,',');
	  }
	  if(!isEmptyList(p)){
	    outStr(f,",..");
	    outCell(f,p,width,prec-1,alt);
	  }
	  return outStr(f,"]");
	}
	else
	  return outStr(f,"[...]");
	  
    case consMarker:{		/* A constructor  */
      if(IsHandle(p))
	return displayHandle(f,(void*)p,width,prec,alt);
      else if(isClosure(p))
	return displayCode(f,p,width,prec,alt);
      else if(prec>0){
	unsigned WORD32 i;
	unsigned WORD32 ar = consArity(p);
	char *sep="";
	objPo *el = consData(p);
	retCode ret = outCell(f,consFn(p),width,prec-1,alt);
	
	if(ret!=Ok)
	  return ret;
	  
        outChar(f,'(');
        for(i=0;i<ar;i++){
	  outStr(f,sep); sep=",";
	  if((ret=outCell(f,*el++,width,prec-1,alt))!=Ok)
            return ret;
        }
        return outChar(f,')');
      }
      else
        return outMsg(f,"%w(...)",consFn(p));
    }

    case tupleMarker:{		/* A tuple or record  */
      if(prec>0){
	WORD32 i;
	WORD32 ar = tupleArity(p);
	char c='(';

	if(ar>0){
	  objPo *el = tupleData(p);
	  if(alt && isSymb(*el)){

	    {int r=outCell(f,*el++,width,prec-1,alt);
	    if(r!=Ok)
	      return r;}

	    if(ar==1)
	      outChar(f,'(');
	    else{
	      for(i=1;i<ar;i++){
		outChar(f,c); c=',';
		{int r=outCell(f,*el++,width,prec-1,alt);
		if(r!=Ok)
		  return r;}
	      }
	    }
	    outChar(f,')');
	  }
	  else{
	    for(i=0;i<ar;i++){
	      outChar(f,c); c=',';
	      {int r=outCell(f,*el++,width,prec-1,alt);
	      if(r!=Ok)
		return r;}
	    }
	    outChar(f,')');
	  }
	}
	else
	  outStr(f,"()");
	return Ok;
      }
      else
	outStr(f,"(...)");
      return Ok;
    }

    case anyMarker:
      return displayAnyVal(f,p,width,prec,alt);

    case codeMarker:
      return outMsg(f,"<$<%t>$>",CodeSig(p));

    case forwardMarker:{
      objPo fwd = FwdVal(p);
      return outMsg(f,BOLD_ESC_ON "=>%w" BOLD_ESC_OFF,fwd);
    }

    case handleMarker:
      return displayHdl(f,HdlVal(p));

    case opaqueMarker:
      return displayOpaque(f,(opaquePo)p);

    default:
      outMsg(f,"Illegal cell at [%x]",p);
      return Error;
    }
  }
  else{
    outMsg(f,"(null)");
    return Ok;
  }
}

retCode displayType(ioPo f,objPo p,WORD32 width,int prec,logical alt) 
{
  if(p!=NULL){
    switch(Tag(p)){
    case variableMarker:
      return outMsg(f,"%%_%x",p);
      
    case symbolMarker:		/* a symbol structure */
      if(!alt){
        retCode ret = Ok;
        uniChar *ptr = SymVal(p);
        
        if(*ptr=='#')
          ptr++;
        
        while(ret==Ok && *ptr!='\0' && *ptr!='#')
          ret = outChar(f,*ptr++);
        return ret;
      }
      else
        return outMsg(f,"%U",SymVal(p));
    	  
    case consMarker:{		/* A constructor  */
      objPo fn = consFn(p);
      unsigned WORD32 ar = consArity(p);
      
      if(isSymb(fn)){
        if(fn==kfunTp && ar==2){
          displayType(f,consEl(p,0),width,prec-1,alt);
          outStr(f," => ");
          displayType(f,consEl(p,1),width,prec-1,alt);
          return Ok;
        }
        else if(fn==kallQ && ar==2){
          displayType(f,consEl(p,0),width,prec-1,alt);
          outStr(f," - ");
          displayType(f,consEl(p,1),width,prec-1,alt);
          return Ok;
        }
        else if(fn==klstTp && ar==1){
          displayType(f,consEl(p,0),width,prec-1,alt);
          outStr(f,"[]");
          return Ok;
        }
        else if(fn==kprocTp){
          char *sep = "";
          retCode ret = Ok;
          unsigned WORD32 i;
          
          outChar(f,'(');
          for(i=0;ret==Ok && i<ar;i++){
            outMsg(f,"%s",sep);
            sep = ", ";
            displayType(f,consEl(p,i),width,prec-1,alt);
          }
          if(ret==Ok)
            ret=outMsg(f,"){}");
          return ret;
        }
        else if(fn==ktplTp){
          char *sep = "";
          retCode ret = Ok;
          unsigned WORD32 i;
          
          outStr(f,"(.");
          for(i=0;ret==Ok && i<ar;i++){
            outMsg(f,"%s",sep);
            sep = ", ";
            displayType(f,consEl(p,i),width,prec-1,alt);
          }
          if(ret==Ok)
            ret=outStr(f,".)");
          return ret;
        }
        else if(fn==kqueryTp && ar==2){
          retCode ret = displayType(f,consEl(p,0),width,prec-1,alt);
          
          if(ret==Ok)
            ret = outStr(f,"?");
          if(ret==Ok)
            ret = outCell(f,consEl(p,1),width,prec-1,alt);
          return ret;
        }
        else{
          char *sep = "";
          retCode ret = displayType(f,fn,width,prec-1,alt);
          unsigned WORD32 i;
          
          outChar(f,'(');
          for(i=0;ret==Ok && i<ar;i++){
            outMsg(f,"%s",sep);
            sep = ", ";
            displayType(f,consEl(p,i),width,prec-1,alt);
          }
          if(ret==Ok)
            ret=outChar(f,')');
          return ret;
        }
      }
      else
        return Error;
    }

    case tupleMarker:{		/* A tuple or record  */
      if(prec>0 || tupleArity(p)==0){
	unsigned WORD32 i;
	unsigned WORD32 ar = tupleArity(p);
	char *sep = "";
        retCode ret = Ok;
          
        outChar(f,'(');
        for(i=0;ret==Ok && i<ar;i++){
          outMsg(f,"%s",sep);
          sep = ", ";
          displayType(f,tupleArg(p,i),width,prec-1,alt);
        }
        if(ret==Ok)
          ret=outChar(f,')');
        return ret;
      }
      else
        return outStr(f,"(...)");
    }

    default:
      outMsg(f,"Illegal type cell at [%x]",p);
      return Error;
    }
  }
  else{
    outMsg(f,"(null)");
    return Ok;
  }
}


static retCode displayCode(ioPo f,objPo p,WORD32 width,WORD32 prec,logical alt)
{
  if(isClosure(p)){
    objPo pc = codeOfClosure(p);

    return outMsg(f,"< %t >",CodeSig(pc));	        // Display the type signature
  }
  else
    return Error;
}

static retCode displayAnyVal(ioPo f,objPo p,WORD32 width,WORD32 prec,logical alt)
{
  if(IsAny(p)){
    objPo sig = AnySig(p);
    objPo val = AnyData(p);

    outStr(f,"??");
    outChar(f,'(');
    outCell(f,val,width-2,prec,alt);
    if(alt){
      outChar(f,':');
      displayType(f,sig,width,prec,alt);
    }
    return outChar(f,')');
  }
  else
    return Error;
}

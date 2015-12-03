/* 
  Type coercion run-time code
  (c) 1994-1999 Imperial College and F.G.McCabe
 
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
#include <math.h>
#include <ctype.h>

#include "april.h"
#include "arith.h"
#include "dict.h"
#include "sign.h"		/* type signature handling */
#include "setops.h"
#include "process.h"
#include "astring.h"


/* Use a type signature string to coerce a value into a real value */

retCode force(objPo type,objPo ag,objPo *forced)
{
  if(type==NULL)
    return Error;
  else if(IsAny(ag))
    return force(type,AnyData(ag),forced);
  else{
    objPo lhs;
    
    type = deRefVar(type);              // Types can be constructed
    
    if(isSymb(type)){
      if(type==knumberTp){               // Are we expected to produce a number?
        if(IsInteger(ag) || IsFloat(ag)){
          *forced = ag;
	  return Ok;
        }
        else if(isListOfChars(ag)){
          WORD32 len = ListLen(ag);
          uniChar buffer[MAX_SYMB_LEN];
          uniChar *buff = (len<NumberOf(buffer)?buffer:((uniChar*)malloc(sizeof(uniChar)*(len+1))));
	  objPo num = str2num(StringText(ag,buff,len+1),len);

          if(buff!=buffer)
            free(buff);

	  if(num!=NULL){
	    *forced = num;
	    return Ok;
	  }
	  else
	    return Error;
        }
        else if(isSymb(ag)){
	  uniChar *sym=SymText(ag);
	  objPo num = str2num(sym,uniStrLen(sym));

	  if(num!=NULL){
	    *forced = num;
	    return Ok;
	  }
	  else
	    return Error;
        }
        else if(isChr(ag)){
          *forced = allocateInteger(CharVal(ag));
          return Ok;
        }
        else
	  return Error;
      }
      else if(type==ksymbolTp){
        if(isSymb(ag)){                 /* already a symbol? */
          if(isUserSymb(ag)){
            *forced = ag;
            return Ok;
          }
          else{
            WORD32 len = SymLen(ag)+2;
            uniChar buffer[MAX_SYMB_LEN];
            uniChar *buff = (len<NumberOf(buffer)?buffer:((uniChar*)malloc(sizeof(uniChar)*(len+1))));

            strMsg(buff,len,"'%U",SymVal(ag));
            *forced = newUniSymbol(buff);
            if(buffer!=buff)
              free(buff);
            return Ok;
          }
        }
        else if(isListOfChars(ag)){	/* otherwise must be a string */
          WORD32 len = ListLen(ag)+2;
          uniChar buffer[MAX_SYMB_LEN];
          uniChar *buff = (len<NumberOf(buffer)?buffer:((uniChar*)malloc(sizeof(uniChar)*(len+1))));

          strMsg(buff,len,"'%#L",ag);
          
	  *forced = newUniSymbol(buff);
          if(buffer!=buff)
            free(buff);
	  return Ok;
        }
        else if(IsInteger(ag)){
          uniChar text[MAX_SYMB_LEN];
	  
	  strMsg(text,NumberOf(text),"%ld",IntVal(ag));
	  
	  *forced = newUniSymbol(text);
	  return Ok;
	}
	else if(IsFloat(ag)){
          uniChar text[MAX_SYMB_LEN];
	  
	  strMsg(text,NumberOf(text),"%f",FloatVal(ag));
	  
	  *forced = newUniSymbol(text);
	  return Ok;
	}
        else
	  return Error;		/* couldnt do anything else */
      }
      else if(type==kstringTp){
        if(isListOfChars(ag)){
	  *forced = ag;
	  return Ok;
        }
        else if(isSymb(ag)){
	  *forced = allocateString(SymText(ag));
	  return Ok;
        }
        else{
	  ioPo str = openOutStr(utf16Encoding);

	  outMsg(str,"%w",ag);

	  {
	    WORD32 len;
	    uniChar *text = getStrText(O_STRING(str),&len);
	  
	    *forced = allocateSubString(text,len);
	  }
	  closeFile(str);
        }
        return Ok;
      }
      else if(type==khandleTp){ // Look for a handle
        if(isListOfChars(ag)){
          WORD32 len = ListLen(ag)+1;
          uniChar buffer[MAX_SYMB_LEN];
          uniChar *buff = (len<NumberOf(buffer)?buffer:((uniChar*)malloc(sizeof(uniChar)*(len+1))));
          
	  *forced = parseHandle(NULL,StringText(ag,buff,len+1));

          if(buff!=buffer)
            free(buff);
	}
        else if(isSymb(ag))
	  *forced = parseHandle(NULL,SymText(ag));
        else if(IsHandle(ag))
	  *forced=ag;
        else
	  return Error;
        return Ok;
      }
      else if(type==klogicalTp){
        if(isSymb(ag)){		/* normal logical value */
	  if(ag==ktrue || ag==kfalse)
	    *forced = ag; /* only valid logicals allowed through */
	  else
	    return Error;
	  return Ok;
        }
        else if(IsInteger(ag)){	/* coerce integers into logical */
	  if(IntVal(ag)==0)	/* 0 = false, else true */
	    *forced = kfalse;
	  else
	    *forced = ktrue;
	  return Ok;
        }
        else if(isListOfChars(ag)){
          WORD32 len = ListLen(ag)+1;
          uniChar buffer[MAX_SYMB_LEN];
          uniChar *buff = (len<NumberOf(buffer)?buffer:((uniChar*)malloc(sizeof(uniChar)*(len+1))));
	  uniChar *trueval = StringText(ag,buff,len);

	  if(uniIsLit(trueval,"true") || uniIsLit(trueval,"True") ||
	     uniIsLit(trueval,"T"))
	    *forced = ktrue;
	  else if(uniIsLit(trueval,"false") || uniIsLit(trueval,"False") ||
		uniIsLit(trueval,"F"))
	    *forced = kfalse;
	  else{
            if(buffer!=buff)
              free(buff);
	    return Error;
          }

          if(buffer!=buff)
            free(buff);
	  return Ok;
        }
        else if(IsList(ag)){	/* coerce lists into logical */
	  if(isEmptyList(ag))	/* [] = false, else true */
	    *forced = kfalse;
	  else
	    *forced = ktrue;
	  return Ok;
        }
        else
	  return Error;		/* give up! */
      }
      else if(type==kopaqueTp){
        if(IsOpaque(ag)){
	  *forced = ag; /* only valid opaques allowed through */
	  return Ok;
	}  
	else
	  return Error;
      }
      else
        return Error;           // Cannot handle other kinds of coercion
    }
    else if(IsVar(type)){       // Type variable
      *forced = ag;		/* just pass the original through */
      return Ok;
    }
    else if(isUnCall(type,klstTp,&lhs)){
      if(deRefVar(lhs)==kcharTp){       // A string?
        if(isListOfChars(ag)){
	  *forced = ag;
	  return Ok;
        }
        else if(isSymb(ag)){
          WORD32 len = uniStrLen(SymText(ag));
          uniChar *buffer = (uniChar*)malloc(sizeof(uniChar)*(len+1));

          uniCpy(buffer,len+1,SymText(ag));

	  *forced = allocateString(buffer);

          free(buffer);
	  return Ok;
        }
        else if(isChr(ag)){
          uniChar s[] = {CharVal(ag),0};
          *forced = allocateSubString(s,1);
          return Ok;
        }
        else{
	  ioPo str = openOutStr(utf16Encoding);

	  outMsg(str,"%w",ag);

	  {
	    WORD32 len;
	    uniChar *text = getStrText(O_STRING(str),&len);
	  
	    *forced = allocateSubString(text,len);
	  }
	  closeFile(str);
        }
        return Ok;
      }
      else if(isEmptyList(ag)){
	*forced = ag;
	return Ok;		/* Nil is already a list of any type */
      }
      else if(isNonEmptyList(ag)){ /* list to list coercion */
	void *root = gcAddRoot(&ag);
	objPo el = emptyList;
	objPo list = emptyList;
	objPo tail = emptyList;
	int res = Ok;

	gcAddRoot(&el);
	gcAddRoot(&list);
	gcAddRoot(&ag);
	
	type = lhs;             // What are we coercing into?
	gcAddRoot(&type);
	
	while(isNonEmptyList(ag)){
	  res = force(type,ListHead(ag),&el);
	  
	  if(list==emptyList)
	    *forced = list = tail = allocatePair(&el,&emptyList);
	  else{
	    el = allocatePair(&el,&emptyList);
	    updateListTail(tail,el);
	    tail = el;
	  }
	  
	  ag = ListTail(ag);
	}
	
	if(ag!=emptyList)
	  return Error;
	
	gcRemoveRoot(root);
	return Ok;
      }
      else
        return Error;
    }
    else if(IsTuple(type) && IsTuple(ag) && tupleArity(type)==tupleArity(ag)){
      WORD32 ar = tupleArity(type);
      WORD32 i;
      void *root = gcAddRoot(&ag);
      objPo el=emptyList;
      objPo tpl;
      retCode ret = Ok;
      
      gcAddRoot(&type);
      gcAddRoot(&el);
      
      tpl = *forced = allocateTuple(ar);
      
      for(i=0;ret==Ok && i<ar;i++){
        ret = force(tupleArg(type,i),tupleArg(ag,i),&el);
        
        if(el==Ok)
          updateTuple(tpl,i,el);
      }
      
      gcRemoveRoot(root);
      return ret;
    }
    else if(isBinCall(type,kqueryTp,NULL,&type))
      return force(type,ag,forced);
    else
      return Error;		/* We have an uncoercable type signature */
  }
}

retCode m_coerce(processpo p,objPo *args)
{
  objPo type = args[1];

  if(force(type,args[0],&args[1])==Ok)
    return Ok;
  else
    return liberror("_coerce",2,"cannot coerce value to type",einval);
}


/* Special form of coerce for command line arguments */
retCode m__coerce(processpo p,objPo *args)
{
  objPo tps = deRefVar(args[1]);
  objPo ags = args[0];

  if(IsTuple(tps) && IsList(ags)){
    WORD32 ar = tupleArity(tps);
    void *root = gcAddRoot(&tps);
    
    gcAddRoot(&ags);
    
    if(ar<=ListLen(ags)){
      retCode res = Ok;
      objPo tpl = allocateTuple(ar);
      objPo el = kvoid;
      WORD32 i;

      gcAddRoot(&tpl);
      gcAddRoot(&el);

      for(i=0;res==Ok && i<ar && isNonEmptyList(ags);i++){
	if((res=force(tupleArg(tps,i),ListHead(ags),&el))==Ok){
	  updateTuple(tpl,i,el);
	  ags = ListTail(ags);
	}
      }
      
      if(res!=Ok){
        gcRemoveRoot(root);
        return liberror("__coerce",2,"mismatch in arguments",efail);
      }

      args[1]=tpl;		/* return computed answer */

      gcRemoveRoot(root);
      return Ok;
    }
    else{
      gcRemoveRoot(root);
      return liberror("__coerce",2,"insufficient number of arguments in list",einval);
    }
  }
  else
    return liberror("__coerce",2,"invalid arguments",einval);
}

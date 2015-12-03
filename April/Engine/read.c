/*
  Parser for April data values
  (c) 1994-1998 Imperial College and F.G. McCabe

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
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include "april.h"
#include "astring.h"
#include "dict.h"
#include "term.h"
#include "sign.h"

#define MAXBUFF 2048

typedef enum{
  eofToken, strToken, numToken, symToken, chrToken, lparToken, rparToken,
  lbraToken, rbraToken, lbrceToken, rbrceToken,
  cnsToken, commaToken, semiToken,
  idToken, errorToken
} tokenType;

static retCode readTuple(ioPo in,objPo *tgt);
static tokenType nextoken(ioPo in,objPo *tgt);

static void expectToken(ioPo in,tokenType which)
{
  objPo tok=kvoid;
  void *root = gcAddRoot(&tok);
  tokenType tk;

  while((tk=nextoken(in,&tok))!=which && tk!=eofToken)
    ;

  gcRemoveRoot(root);
}

static uniChar esc_char(ioPo inFile)
{
  uniChar ch = inCh(inFile);

  switch(ch){
  case 'a': return '\a'; 
  case 'b': return '\b';
  case 'd': return '\x7f';      /* Delete */
  case 'e': return '\x1b';      /* Escape */
  case 'f': return '\f';
  case 'n': return '\n'; 
  case 'r': return '\r'; 
  case 't': return '\t';
  case 'v': return '\v'; 

  case '0': case '1': case '2': case '3': /* octal number */
  case '4': case '5': case '6': case '7':{
    uniChar c = ch&7;
    if((ch=inCh(inFile))>='0' && ch<='7'){
      c = (c<<3)|(ch&7);
      if((ch=inCh(inFile))>='0' && ch<='7')
        c = (c<<3)|(ch&7);
      else
        unGetChar(inFile,ch);
    }
    else
      unGetChar(inFile,ch);
    return c;
  }
  default:
    return ch;
  }
}

static tokenType nextoken(ioPo inFile,objPo *tgt)
{
  register uniChar ch=inCh(inFile);
  uniChar buffer[MAXBUFF];
  
restart:

  while(isZsChar(ch) || isZpChar(ch) || isZlChar(ch) || isCcChar(ch))
    ch = inCh(inFile);
    
  if(ch=='-'){
    if((ch=inCh(inFile))=='-'){
      if(isZsChar(ch=inCh(inFile))){  // --<blank> is the comment convention
        while(fileStatus(inFile)==Ok && !(ch=='\n'||isZlChar(ch)))
          ch=inCh(inFile);
      }
      else{
        unGetChar(inFile,ch);           // We have to unwind three chars
        unGetChar(inFile,'-');
        ch='-';
        goto restart;
      }
    }
    else{
      unGetChar(inFile,ch);
      ch='-';
      goto restart;
    }
  }
  else if(ch=='/'){
    if((ch=inCh(inFile))=='*'){       // Start of block comment
    
      ch = inCh(inFile);
      while(fileStatus(inFile)==Ok && ch!='*'){
        if((ch=inCh(inFile))=='/')
          break;
      }
      
      ch = inCh(inFile);
      goto restart;                     // This is pretty ugly
    }
    else{
      unGetChar(inFile,ch);
      ch='/';
    }
  }
    
  if(fileStatus(inFile)==Eof)
    return eofToken;
  else if(ch=='"'){
    uniChar *tk = buffer;		/* A quoted string here */

    while((ch=inCh(inFile))!='"'){
      *tk++=ch;
      if(ch=='\\')
	*(tk-1) = esc_char(inFile);
      else if(isZlChar(ch)||ch==uniEOF){
	outMsg(logFile,"unexpected end of quote");
	return errorToken;
      }
    }
    
    *tgt = allocateSubString(buffer,tk-buffer);
    return strToken;
  }
  else if(ch==','){		/* Check for ,.. sequence */
    if((ch=inCh(inFile))=='.'){	
      if((ch=inCh(inFile))=='.')    /* ,.. sequence */
	return cnsToken;	/* report list tail puctuation mark */
      else{
	unGetChar(inFile,ch);
	unGetChar(inFile,'.');
	return commaToken;	/* report a comma */
      }
    }
    else{
      unGetChar(inFile,ch);	        /* put the character back */
      return commaToken;	/* report a comma */
    }
  }
  else if(ch=='\''){	        /* tick mark */
    uniChar *tk = buffer;	/* A quoted symbol here */
    
    if((ch=inCh(inFile))=='\''){        // A character literal
      ch = inCh(inFile);
      
      if(ch=='\\')
        ch = esc_char(inFile);
      *tgt = allocateChar(ch);
      return chrToken;
    }
    else{
      *tk++='\'';               // Insert the symbol marker
      do{
        if(ch=='\\'){
          *tk++=esc_char(inFile);
          ch=inCh(inFile);
          continue;
        }
        else
          *tk++=ch;
        ch = inCh(inFile);
      }while(isLetterChar(ch));

      *tk = 0;
    
      *tgt = newUniSymbol(buffer);
      return symToken;
    }
  }
  else if(ch=='(')
    return lparToken;
  else if(ch==')')
    return rparToken;
  else if(ch=='{')
    return lbrceToken;
  else if(ch=='}')
    return rbrceToken;
  else if(ch=='[')
    return lbraToken;
  else if(ch==']')
    return rbraToken;
  else if(ch==';')
    return semiToken;
  else if(ch=='!'||ch=='@'||ch=='#'||ch=='$'||ch=='%'||ch=='^'||ch=='&'||
          ch=='*'||ch=='-'||ch=='='||ch=='+'||ch=='\\'||ch=='|'||ch==':'||
          ch=='<'||ch=='>'||ch=='.'||ch=='/'||ch=='?'||ch=='~'||ch=='`'){
    uniChar *tk = buffer;		/* A quoted string here */
      
    while(ch=='!'||ch=='@'||ch=='#'||ch=='$'||ch=='%'||ch=='^'||ch=='&'||
          ch=='*'||ch=='-'||ch=='='||ch=='+'||ch=='\\'||ch=='|'||ch==':'||
          ch=='<'||ch=='>'||ch=='.'||ch=='/'||ch=='?'||ch=='~'||ch=='`'){
      *tk++=ch;
      ch = inCh(inFile);
    }
    *tk++='\0';
    *tgt = newUniSymbol(buffer);
    return idToken;
  }
  else if(isLetterChar(ch)||ch=='_'){ // Unicode standard for identifiers
    uniChar *tk = buffer;
      
    while(fileStatus(inFile)==Ok &&
        (isLetterChar(ch) || ch=='_' ||
         isNdChar(ch) || isMnChar(ch) || isMcChar(ch) || isPcChar(ch) ||
         isCfChar(ch))){
      *tk++=ch;
      ch = inCh(inFile);
    }

    *tk='\0';			/* terminate the identifier */
    *tgt = newUniSymbol(buffer);
    return idToken;
  }
  else if(isNdChar(ch)){      // Digit character
    uniChar *tk = buffer;
    *tk++=ch;
      
    if(ch=='0'){
      ch =inCh(inFile);	/* look for hex or octal numbers */
      if(ch=='x'||ch=='X'){
	unsigned WORD32 X=0;

	while(isxdigit(ch=inCh(inFile)))
	  if(ch>='0' && ch<='9')
	    X=(X<<4)+(ch&0xf);
	  else
	    X=(X<<4)+(ch&0xf)+9;

	*tgt = allocateInteger(X);
	return numToken;
      }
      else if(ch>='0' && ch <= '7'){ /* Octal number? */
	unsigned integer O=0;
	while(ch>='0' && ch <= '7'){
	  O=(O<<3)+(ch&0xf);
	  ch=inCh(inFile);
	}

	*tgt = allocateInteger(0);
	return numToken;
      }
      else if(ch=='c'){	/* 0c<char> */
	ch = inCh(inFile);

	if(ch=='\\')
	  ch=esc_char(inFile);
	*tgt = allocateInteger(ch);
	return numToken;
      }
      else			/* regular decimal number */
	while(isNdChar(ch)){
	  *tk++=ch;
	  ch = inCh(inFile);
	}
    }
    else			        /* leading digit is not a zero */
      while(isNdChar(ch=inCh(inFile)))
	*tk++=ch;

    if(ch=='.'||ch=='e'||ch=='E'){    /* is this a floating point number? */
      if(ch=='.'){		        /* read in the decimal fraction part */
	*tk++=ch;
	if(isNdChar(ch=inCh(inFile))){
	  while(isNdChar(ch)){
	    *tk++=ch;
	    ch=inCh(inFile);
	  }
	}
	else{			        /* regular decimal number. Replace the char */
	  *tk='\0';
	  unGetChar(inFile,ch);
	  unGetChar(inFile,'.');	        /* all this started with a '.' */

	  errno = 0;
	  *tgt = allocateInteger(parseInteger(buffer,tk-buffer));
	  if(errno==0)
	    return numToken;
	  else{			/* integer is too large */
	    *tgt = allocateNumber(parseNumber(buffer,tk-buffer));
	    return numToken;
	  }
	}
      }
      if(ch=='e'||ch=='E'){	/* read in the exponent part */
	*tk++=ch;
	if((ch=inCh(inFile))=='-'||ch=='+'){
	  *tk++=ch;
	  ch=inCh(inFile);
	}
	while(isNdChar(ch)){
	  *tk++=ch;
	  ch=inCh(inFile);
	}
      }
      *tk='\0';		/* end of floating point number */
      *tgt = allocateNumber(parseNumber(buffer,tk-buffer));
      return numToken;
    }
    else{			/* regular integer */
      *tk='\0';

      errno = 0;
      *tgt = allocateInteger(parseInteger(buffer,tk-buffer));
      if(errno==0)
	return errorToken;
      else{			/* integer is too large */
	*tgt = allocateNumber(parseNumber(buffer,tk-buffer));
	return numToken;
      }
    }
  }
  else
    return errorToken;
}


/*
 * Read in an April data value
 */

static retCode Dterm(ioPo in,tokenType tk,objPo token,objPo *tgt);

retCode ReadData(ioPo in,objPo *tgt)
{
  objPo tok;
  tokenType tk = nextoken(in,&tok);
  
  return Dterm(in,tk,tok,tgt);
}

static retCode Dterm(ioPo in,tokenType tk,objPo token,objPo *tgt)
{
  retCode res = Ok;
  
  switch(tk){
    case eofToken:
      return Eof;
    case errorToken:
      return Error;
    case strToken:
    case numToken:
    case symToken:
    case idToken:
    case chrToken:
      *tgt = token;
      return Ok;
    case lbrceToken:{
      tk = nextoken(in,&token);
      
      if((res=Dterm(in,tk,token,tgt))!=Ok)
        return res;
      
      if(nextoken(in,&token)!=rbrceToken){
        expectToken(in,rbrceToken);
        return Error;
      }
      return Ok;
    }
    case lparToken:
      return readTuple(in,tgt);
    case lbraToken:{
      tk=nextoken(in,&token);
    
      if(tk==rbraToken){
        *tgt = emptyList;
        return Ok;
      }
      else{
        objPo last = *tgt = emptyList;
        objPo el = kvoid;
        void *root = gcAddRoot(&last);

        gcAddRoot(&token);
        gcAddRoot(&el);
      
         tk=commaToken;            // Fake this for the loop
    
        while(tk==commaToken){
          if(last==emptyList)
	  last = *tgt = allocatePair(&kvoid,&emptyList);
          else{
	    objPo tail = allocatePair(&kvoid,&emptyList);
	    updateListTail(last,tail);
	    last = tail;
          }
          if((res=Dterm(in,tk,token,&el))!=Ok)
	    return res;
          updateListHead(last,el);
        
          tk = nextoken(in,&token);
        }

        if(tk==cnsToken){
          tk = nextoken(in,&token);
        
	  if((res=Dterm(in,tk,token,&el))!=Ok)
	    return res;
	  updateListTail(last,el);
	  tk = nextoken(in,&token);
        }
      
        gcRemoveRoot(root);
        if(tk!=rbraToken){
          expectToken(in,rbraToken);
          return Error;
        }
        else
          return Ok;
      }
    }
    default:
      return Error;
  }
}

static retCode readTupleEl(ioPo in,tokenType tk,objPo token,objPo *tgt,WORD32 index,WORD32 *arity)
{
  if(tk==rparToken){
    *arity = index;		/* we have all the elements of the tuple */
    *tgt = allocateTuple(index);
    return Ok;
  }
  else{
    objPo el = kvoid;
    void *root = gcAddRoot(&el);
    retCode res;
    
    gcAddRoot(&token);
    res = Dterm(in,tk,token,&el);

    if(res==Ok){
      if((tk=nextoken(in,&token))==commaToken){
        tk = nextoken(in,&token);
	res = readTupleEl(in,tk,token,tgt,index+1,arity);
      }
      else if(tk==rparToken){
	*arity = index+1;		/* we have all the elements of the tuple */
	*tgt = allocateTuple(index+1);
      }
    }

    gcRemoveRoot(root);
    if(res==Ok)
      updateTuple(*tgt,index,el);
    return Ok;
  }
}

static retCode readTuple(ioPo in,objPo *tgt)
{
  WORD32 arity;
  objPo token=kvoid;
  void* root = gcAddRoot(&token);
  tokenType tk = nextoken(in,&token);
  retCode ret = readTupleEl(in,tk,token,tgt,0,&arity);
  gcRemoveRoot(root);
  return ret;
}

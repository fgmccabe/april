/* 
   Unicode tokeniser for April
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
   
   Contact: Francis McCabe <fmccabe@gmail>
 */

#include "config.h"
#include "compile.h"
#include "keywords.h"
#include "makesig.h"
#include "operators.h"
#include "token.h"
#include "keywords.h"
#include <errno.h>
#include <ctype.h>

/* used at end of tokenization */
#define report(X) {tok->tt = X; ahead = *tok; tkahead = X; looked = True; return X; }

#define MAXBUFF 2048

volatile static logical looked = False;
token ahead;			/* look ahead token */
tokenType tkahead;
long lineNumber;                        /* current line number */

static uniChar getChar(ioPo in)
{
  uniChar ch = inCh(in);

  if(ch=='\n')
    lineNumber++;
  return ch;
}

static retCode unChar(ioPo in,uniChar ch)
{
  if(ch=='\n')
    lineNumber--;
  return unGetChar(in,ch);
}

tokenType nextoken(ioPo inFile,tokenpo tok)
{
  if(looked){
    looked = False;
    *tok = ahead;
#ifdef TRACEPARSE
    if(traceParse)
      outMsg(logFile,"%U *\n",tokenStr(tok));
#endif
    return tkahead;
  }
  else{
    tokenType tk = hedtoken(inFile,tok);
    looked = False;
#ifdef TRACEPARSE
    if(traceParse)
      outMsg(logFile,"%U\n",tokenStr(tok));
#endif
    return tk;
  }
}

void commit_token()
{
  looked=False;			/* force current look ahead to be the one */
#ifdef TRACEPARSE
  if(traceParse)
    outMsg(logFile,"commit to %U\n",tokenStr(&ahead));
#endif
}

void expectToken(ioPo inFile,tokenType t)
{
  token tok;
  tokenType tk;

  while((tk=nextoken(inFile,&tok))!=t && tk!=eofToken)
    ;
}

static void skipComments(ioPo inFile)
{
  uniChar ch = getChar(inFile);
  
  while(True){                          // We may have many comments & blanks  
    while(isZsChar(ch) || isZpChar(ch) || isZlChar(ch) || isCcChar(ch) || ch=='\n')
      ch = getChar(inFile);
    
    if(ch=='-'){
      if((ch=getChar(inFile))=='-'){
        if(isZsChar(ch=getChar(inFile))||ch=='\t'||ch=='\n'){  // --<blank> is the comment convention
          while(fileStatus(inFile)==Ok && !(ch=='\n'||isZlChar(ch)))
            ch=getChar(inFile);
        }
        else{
          unChar(inFile,ch);           // We have to unwind three chars
          unChar(inFile,'-');
          unChar(inFile,'-');
          return;
        }
      }
      else{
        unChar(inFile,ch);
        unChar(inFile,'-');
        return;
      }
    }
    else if(ch=='/'){
      if((ch=getChar(inFile))=='*'){       // Start of block comment
        uniChar ch1 = getChar(inFile);
        uniChar ch2 = getChar(inFile);
        
        while(fileStatus(inFile)==Ok && !(ch1=='*' && ch2=='/')){
          ch1 = ch2;
          ch2 = getChar(inFile);
        }
        
        ch = getChar(inFile);
      }
      else{
        unChar(inFile,ch);
        unChar(inFile,'/');
        return;
      }
    }
    else{
      unChar(inFile,ch);
      return;                           // no comment here
    }
  }
}

static int esc_char(ioPo inFile)
{
  uniChar ch = getChar(inFile);

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
    if((ch=getChar(inFile))>='0' && ch<='7'){
      c = (c<<3)|(ch&7);
      if((ch=getChar(inFile))>='0' && ch<='7')
        c = (c<<3)|(ch&7);
      else
        unChar(inFile,ch);
    }
    else
      unChar(inFile,ch);
    return c;
  }
  case '+':{
    uniChar X = 0;
    
    while(isxdigit(ch=getChar(inFile))){
      if(ch>='0' && ch<='9')
	X=(X<<4)+(ch&0xf);
      else
	X=(X<<4)+(ch&0xf)+9;
    }
    
    unChar(inFile,ch);
    return X;
  }
    
  default:
    return ch;
  }
}

tokenType hedtoken(ioPo inFile,tokenpo tok)
{
  uniChar buffer[MAXBUFF];
  register uniChar *tk;
  uniChar ch;

  if(looked){
    *tok=ahead;
    return tkahead;
  }
  else{
    skipComments(inFile);       // Skip past any comments
    tok->line_num=mkLineInfo(fileName(inFile),lineNumber);
    
    if(fileStatus(inFile)==Eof){
      report(eofToken);
    }
    else if((ch=getChar(inFile))=='"'){
      tk = buffer;		/* A quoted string here */

      while((ch=getChar(inFile))!='"'){
	*tk++=ch;
	if(ch=='\\')
	  *(tk-1) = esc_char(inFile);
	else if(isZlChar(ch)||ch=='\n'||ch==uniEOF){
	  tok->tt=ch;
	  reportParseErr(tok->line_num,"unexpected end of quote");
	  break;
	}
      }
      
      *tk++='\0';

      if((tok->tk.name=allocString(buffer))==NULL)
	syserr("string space exhausted");
	
      report(strToken);
    }
    else if(ch==','){		/* Check for ,.. sequence */
      if((ch=getChar(inFile))=='.'){	
	if((ch=getChar(inFile))=='.'){ /* Three in a row... */
	  report(cnsToken);     /* report list tail puctuation mark */
	}
	else{
	  unChar(inFile,ch);
	  unChar(inFile,'.');  // Put back first period
	  buffer[0]=',';
	  buffer[1]=0;
	  report(commaToken);	/* report a comma */
	}
      }
      else if(ch=='+'){	        /* ,+ is string cons */
	tok->tk.name = kchrcons;
	report(idToken);
      }
      else{
	unChar(inFile,ch);	/* put the character back */
	buffer[0]=',';
	buffer[1]=0;
	report(commaToken);		/* report a comma */
      }
    }
    else if(ch=='\''){	        /* tick mark */
      if((ch=getChar(inFile))=='\''){ /* single character symbol */
	if((buffer[0]=getChar(inFile))=='\\')
	  buffer[0]=esc_char(inFile);	/* ''\001 */
	buffer[1] = '\0';

	tok->tk.ch = buffer[0];
	report(chrToken);	/* character literal */
      }
      else{
	unChar(inFile,ch);
	buffer[0] = '\'';
	buffer[1] = 0;
	report(qtToken);		/* report a tick mark */
	}
    }
    else if(ch=='('){
      if((ch=getChar(inFile))=='.'){
        report(ltplToken);
      }
      else{
        unChar(inFile,ch);
        tok->tk.ch=ch;
        report(lparToken);
      }
    }
    else if(ch==')'){
      tok->tk.ch=ch;
      report(rparToken);
    }
    else if(ch=='['){
      tok->tk.ch=ch;
      report(lbraToken);
    }
    else if(ch==']'){
      tok->tk.ch=ch;
      report(rbraToken);
    }
    else if(ch=='{'){
      tok->tk.ch=ch;
      report(lbrceToken);
    }
    else if(ch=='}'){
      tok->tk.ch=ch;
      report(rbrceToken);
    }
    else if(ch==';'){
      tok->tk.ch=ch;
     report(semiToken);
    }
    else if(ch=='.'){
      if((ch=getChar(inFile))==')'){
        report(rtplToken);
      }
      else{
        unChar(inFile,ch);
        ch = '.';
      }
    }                   // The fall through here is deliberate
    
    if(ch=='!'||ch=='@'||ch=='#'||ch=='$'||ch=='%'||ch=='^'||ch=='&'||
            ch=='*'||ch=='-'||ch=='='||ch=='+'||ch=='\\'||ch=='|'||ch==':'||
            ch=='<'||ch=='>'||ch=='.'||ch=='/'||ch=='?'||ch=='~'||ch=='`'){
            
      tk = buffer;		/* A quoted string here */
      
      while(ch=='!'||ch=='@'||ch=='#'||ch=='$'||ch=='%'||ch=='^'||ch=='&'||
            ch=='*'||ch=='-'||ch=='='||ch=='+'||ch=='\\'||ch=='|'||ch==':'||
            ch=='<'||ch=='>'||ch=='.'||ch=='/'||ch=='?'||ch=='~'||ch=='`'){
        *tk++=ch;
        ch = getChar(inFile);
      }
      *tk++='\0';
      tok->tk.name = locateU(buffer);
      unChar(inFile,ch);       /* Put back non-id character */
      report(idToken);		/* all graphics are reported as symbols */
    }
    else if(isLetterChar(ch)||ch=='_'){ // Unicode standard for identifiers
      tk = buffer;
      
      while(fileStatus(inFile)==Ok &&
        (isLetterChar(ch) || ch=='_' ||
         isNdChar(ch) || isMnChar(ch) || isMcChar(ch) || isPcChar(ch) ||
         isCfChar(ch))){
        *tk++=ch;
        ch = getChar(inFile);
      }

      *tk='\0';			/* terminate the identifier */
      tok->tk.name=locateU(buffer);
      unChar(inFile,ch);       // Put back non-identifier character
      report(idToken);
    }
    else if(isNdChar(ch)){      // Digit character
      tk = buffer;
      *tk++=ch;
      
      if(ch=='0'){              // leading zero can be 0xffff 0cG or 023
	ch =getChar(inFile);	/* look for hex, character or octal numbers */
	if(ch=='x'||ch=='X'){
	  unsigned long X=0;

	  while(isxdigit(ch=getChar(inFile)))
	    if(ch>='0' && ch<='9')
	      X=(X<<4)+(ch&0xf);
	    else
	      X=(X<<4)+(ch&0xf)+9;

	  tok->tk.i = X;
	  unChar(inFile,ch);   // put back non-digit character
	  report(intToken);
	}
	else if(ch>='0' && ch <= '7'){ /* Octal number? */
	  unsigned long oct=0;
	  
	  while(ch>='0' && ch <= '7'){
	    oct=(oct<<3)+(ch&0xf);
	    ch=getChar(inFile);
	  }

	  tok->tk.i = oct;
	  unChar(inFile,ch);   // put back the non-octal number
	  report(intToken);
	}
	else if(ch=='c'){	/* 0c<char> */
	  ch = getChar(inFile);
	  tok->tk.i = ch;	/* Its this way 'cos of broken CC in MACOSX */

	  if(ch=='\\')
	    tok->tk.i=esc_char(inFile);
	  report(intToken);
	}
	else			/* regular decimal number */
	  while(isNdChar(ch)){
	    *tk++=ch;
	    ch = getChar(inFile);
	  }                     // We fall through to allow for 0.0...
      }
      else			/* leading digit is not a zero */
	while(isNdChar(ch=getChar(inFile)))
	  *tk++=ch;
	  
      if(ch=='.'||ch=='e'||ch=='E'){ /* is this a floating point number? */
	if(ch=='.'){		/* read in the decimal fraction part */
	  *tk++=ch;
	  if(isNdChar(ch=getChar(inFile))){        // An F.P must have at one digit in the fraction
	    while(isNdChar(ch)){
	      *tk++=ch;
	      ch=getChar(inFile);
	    }                   // Fall through to allow for exponent
	  }
	  else{			/* regular decimal number. Replace the char */
	    *tk='\0';
	    unChar(inFile,ch);
	    unChar(inFile,'.');		/* all this started with a '.' */

	    tok->tk.i = parseInteger(buffer,tk-buffer);	
	    report(intToken);
	  }
	}
	if(ch=='e'||ch=='E'){	/* read in the exponent part */
	  *tk++=ch;
	  if((ch=getChar(inFile))=='-'||ch=='+'){
	    *tk++=ch;
	    ch=getChar(inFile);
	  }
	  while(isNdChar(ch)){
	    *tk++=ch;
	    ch=getChar(inFile);
	  }
	}
	unChar(inFile,ch);     // put back non-digit
	*tk='\0';		/* end of floating point number */
	tok->tk.f = parseNumber(buffer,tk-buffer);
	report(fltToken);
      }
      else{			/* regular integer */
	*tk='\0';
	
	unChar(inFile,ch);     // put back non-digit

	tok->tk.i = parseInteger(buffer,tk-buffer);
	report(intToken);
      }
    }
    else{
      reportParseErr(tok->line_num,"unknown character [%c]",ch);
      report(errorToken);
    }
  }
}

void init_token(long line)	/* must be called at the start of each file parse */
{
  looked = False;
  lineNumber = line;
}

long currentLineNumber()
{
  return lineNumber;
}
  
#define MXTK 1024

uniChar *tokenStr(tokenpo tok)
{
  static uniChar msg[MXTK];

  switch(tok->tt){
  case idToken:
    return strMsg(msg,NumberOf(msg),"ident[%U]@%d",tok->tk.name,sourceLine(tok->line_num));

  case strToken:
    return strMsg(msg,NumberOf(msg),"string[%U]",tok->tk.name);
  
  case qtToken:
    return strMsg(msg,NumberOf(msg),"tick");

  case chrToken:
    return strMsg(msg,NumberOf(msg),"char[%c]",tok->tk.ch);

  case intToken:
    return strMsg(msg,NumberOf(msg),"int[%d]",tok->tk.i);

  case fltToken:
    return strMsg(msg,NumberOf(msg),"float[%f]",tok->tk.f);

  case lparToken:
    return strMsg(msg,NumberOf(msg),"(");

  case rparToken:
    return strMsg(msg,NumberOf(msg),")");
    
  case lbraToken:
    return strMsg(msg,NumberOf(msg),"[");

  case rbraToken:
    return strMsg(msg,NumberOf(msg),"]");
    
  case lbrceToken:
    return strMsg(msg,NumberOf(msg),"{");

  case rbrceToken:
    return strMsg(msg,NumberOf(msg),"}");
    
  case commaToken:
    return strMsg(msg,NumberOf(msg),",");

  case cnsToken:
    return strMsg(msg,NumberOf(msg),",..");
    
  case semiToken:
    return strMsg(msg,NumberOf(msg),";");

  case errorToken:
    return strMsg(msg,NumberOf(msg),"<error>");
    
  case eofToken:
    return strMsg(msg,NumberOf(msg),"<eof>");

  default:
    return strMsg(msg,NumberOf(msg),"<unknown>");
  }
}


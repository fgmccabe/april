/*
  Data structure output functions 
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
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include "compile.h"
#include "keywords.h"
#include "makesig.h"
#include "operators.h"
#include "encode.h"			/* File encoding headers */
#include "types.h"
#include "formioP.h"


/* Write out an April structure specially formatted to look like a program */

typedef struct _bvar_ *bvPo;
typedef struct _bvar_ {
  cellpo var;
  long count;
  bvPo prev;
} bVarRec;

void open_bracket(ioPo f,int prec, int prior, int *offset);
void close_bracket(ioPo f,int prec, int prior, int offset);
static void show_op(ioPo ,int , int, uniChar *);
static void putUniChar(ioPo fn,uniChar c);
static void putChar(ioPo fn,uniChar c);
static void putFloat(ioPo fn,FLOAT f);
static void putInt(ioPo fn,long i);
static void putHex(ioPo fn,long i);
static void putStr(ioPo fn,char *s);
static void putSymbol(ioPo fn,uniChar *s);
static void putTypeSymbol(ioPo fn,uniChar *s);
static void putString(ioPo fn,uniChar *s);
static void putTvar(ioPo fn,cellpo p,char lead,logical alt);
static void displayChoice(ioPo fn,cellpo p,integer trace_depth,int offset,
			  int lpr,int rpr,
			  tracePo scope,logical alt);
static void prt_cell(ioPo outFile, cellpo p,int trace_depth,tracePo scope,
		     logical alt);
static void dispType(ioPo fn,cellpo p,integer trace_depth,int offset,int prior,
		     tracePo scope,logical alt);

static cellpo FlagCell = NULL;

#define LPAREN '('
#define RPAREN ')'
#define LBRACE '{'
#define RBRACE '}'

static cellpo tvars[1024];
static unsigned int topVar = 0;
  

static void display(ioPo fn,cellpo p,integer trace_depth,int offset,int prior,
		    tracePo scope,logical alt)
{
  unsigned int i;
  int lpr,rpr,pr;
  unsigned long ar;
  cellpo a1,a2,f,args;

  p=deRef(p);

  if(FlagCell==p){
    outStr(fn,RED_ESC_ON ">>>" RED_ESC_OFF);
  }

  if(trace_depth<0)
    putStr(fn,"...");	/* We dont print if we are too deep */
  else if(IsString(p))
    putString(fn,strVal(p));
  else if(isSymb(p)){		/* Display a symbol */
    uniChar *s=symVal(p);

    if(IsOperator(s)){
      putChar(fn,'(');		/* put brackets around operator symbols */

      putSymbol(fn,s);
      putChar(fn,')');
    }
    else
      putSymbol(fn,s);
  }
  else if(isChr(p)){
    putChar(fn,'\'');
    putChar(fn,'\'');
    putUniChar(fn,CharVal(p));
  }
  else if(IsQuote(p,&a1)){
    putChar(fn,'\'');

    prt_cell(fn,a1,trace_depth,scope,alt);
  }
  else if(IsInt(p))
    putInt(fn,intVal(p));
  else if(IsFloat(p)){
    putFloat(fn,fltVal(p));
  }
  else if(isEmptyList(p)){
    putChar(fn,'[');
    putChar(fn,']');
  }
  else if(isNonEmptyList(p)){	/* Display a list sequence */
    putChar(fn,'[');		/* All lists start with this ... */
    while(isNonEmptyList(p)){
      p = listVal(p);
      display(fn,p,trace_depth-1,offset+2,TERMPREC,scope,alt);
      
      p=Next(p);

      if(isNonEmptyList(p))
	putChar(fn,',');
      else if(!IsList(p)){
	putChar(fn,',');	/* Non-regular list tail */
	putChar(fn,'.');
	putChar(fn,'.');
	display(fn,p,trace_depth-1,offset+2,TERMPREC,scope,alt);
	break;
      }
    }
    putChar(fn,']');
  }
  else if(isCall(p,kblock,&ar)){
    char *sep = "";
    long i;
    
    putChar(fn,'(');
    for(i=0;i<ar;i++){
      putStr(fn,sep);
      display(fn,consEl(p,i),trace_depth-1,offset+1,TERMPREC-1,scope,alt);
      sep = ", ";
    }
    putChar(fn,')');
    putChar(fn,LBRACE); putChar(fn,RBRACE);
  }
  else if(isBinaryCall(p,kchoice,&a1,&a2)){ /* { left \n | right } */
    infprec(kchoice,&lpr,&pr,&rpr); /* Pick up priorities */
    open_bracket(fn,pr,prior,&offset);

    displayChoice(fn,p,trace_depth-1,offset,lpr,rpr,scope,alt);

    close_bracket(fn,pr,prior,offset);
  }

  else if(IsForAll(p,&a1,&a2)){ 
    infprec(kforall,&lpr,&pr,&rpr); 
    open_bracket(fn,pr,prior,&offset);

    display(fn,a1,trace_depth-1,offset+2,lpr,scope,alt);
    putSymbol(fn,kforall);
    display(fn,a2,trace_depth-1,offset,rpr,scope,alt);

    close_bracket(fn,pr,prior,offset);
  }

  else if(isBinaryCall(p,kfn,&a1,&a2)){ /* keep on same line */
    infprec(kfn,&lpr,&pr,&rpr); /* Pick up priorities for -> operator*/
    open_bracket(fn,pr,prior,&offset);
    display(fn,a1,trace_depth-1,offset+2,lpr,scope,alt);
    putSymbol(fn,kfn);
    display(fn,a2,trace_depth-1,offset,rpr,scope,alt);
    close_bracket(fn,pr,prior,offset);
  }

  else if(isConstructorType(p,NULL,NULL))
    dispType(fn,p,trace_depth,offset,prior,scope,alt);

  else if(isBinaryCall(p,ktype,&a1,&a2)){ /* keep on same line */
    infprec(ktype,&lpr,&pr,&rpr); /* Pick up priorities for type operator*/
    open_bracket(fn,pr,prior,&offset);
    display(fn,a1,trace_depth-1,offset+2,lpr,scope,alt);
    putSymbol(fn,ktype);
    display(fn,a2,trace_depth-1,offset,rpr,scope,alt);
    close_bracket(fn,pr,prior,offset);
  }

  else if(isBinaryCall(p,kelse,&a1,&a2)){
    infprec(kelse,&lpr,&pr,&rpr); /* Pick up priorities for else operator*/
    open_bracket(fn,pr,prior,&offset);
    display(fn,a1,trace_depth-1,offset,lpr,scope,alt);
    show_op(fn,lpr,offset,kelse);
    display(fn,a2,trace_depth-1,offset,rpr,scope,alt);
    close_bracket(fn,pr,prior,offset);
  }

  else if(IsBinaryStruct(p,&f,&a1,&a2) && isSymb(f) && infprec(symVal(f),&lpr,&pr,&rpr)!=-1){
    open_bracket(fn,pr,prior,&offset);
    display(fn,a1,trace_depth-(pr-lpr),offset,lpr,scope,alt);
    show_op(fn,pr,offset,symVal(f));
    display(fn,a2,trace_depth-(pr-rpr),offset+2*(pr-rpr),rpr,scope,alt);
    close_bracket(fn,pr,prior,offset);
  }

  else if(IsUnaryStruct(p,&f,&args) && isSymb(f) && preprec(symVal(f),&lpr,&pr)!= -1){
    open_bracket(fn,pr,prior,&offset);
    putSymbol(fn,symVal(f));	/* Always show prefix op on same line */
    display(fn,args,trace_depth-(pr-lpr),offset,lpr,scope,alt);
    close_bracket(fn,pr,prior,offset);
  }

  else if(IsUnaryStruct(p,&f,&args) && isSymb(f) && postprec(symVal(f),&lpr,&pr)!= -1){
    open_bracket(fn,pr,prior,&offset);
    display(fn,args,trace_depth-(pr-lpr),offset,lpr,scope,alt);
    putSymbol(fn,symVal(f));	/* Always show postfix op on same line */
    close_bracket(fn,pr,prior,offset);
  }

  else if(IsUnaryStruct(p,&f,&args) && isSymb(f) && preprec(symVal(f),&lpr,&pr)!= -1){
    open_bracket(fn,pr,prior,&offset);
    putSymbol(fn,symVal(f));	/* Always show postfix op on same line */
    display(fn,args,trace_depth-(pr-lpr),offset,lpr,scope,alt);
    close_bracket(fn,pr,prior,offset);
  }

  else if(isCons(p)){
    /* print out call using normal prefix notation */
	
    display(fn,consFn(p), trace_depth-1,offset,0,scope,alt);
    
    {
      char c='(';
      if(consArity(p)>0){
        for(i=0;i<consArity(p);i++){
	  putChar(fn,c); c=',';
	  display(fn,consEl(p,i),trace_depth-1,offset,TERMPREC-1,scope,alt);
        }
        putChar(fn,')');
      }
      else{
        putChar(fn,'(');
        putChar(fn,')');
      }
    }
  }

  else if(IsTuple(p,&ar)){
    char c='(';
    if(tplArity(p)>0){
      for(i=0;i<tplArity(p);i++){
	putChar(fn,c); c=',';
	display(fn,tplArg(p,i),trace_depth-1,offset,TERMPREC-1,scope,alt);
      }
      putChar(fn,')');
    }
    else{
      putChar(fn,'(');
      putChar(fn,')');
    }
  }
  else if(IsTypeVar(p))
    putTvar(fn,p,'%',alt);

  else if(IsCode(p))
    prt_cell(fn,p,trace_depth,scope,alt);

  else
    outMsg(fn,RED_ESC_ON "Illegal cell at 0x%x " RED_ESC_OFF,p);

  if(FlagCell == p){
    outStr(fn,RED_ESC_ON "<<<" RED_ESC_OFF);
  }

  if(alt){
    cellpo type = typeInfo(p);

    if(type!=NULL){
      outStr(fn,":");
      display(fn,type,trace_depth,offset,1500,scope,False);
      outStr(fn," ");
    }
  }
}

static void dispType(ioPo fn,cellpo p,integer trace_depth,int offset,int prior,
		     tracePo scope,logical alt)
{
  unsigned int i;
  int lpr,rpr,pr;
  unsigned long ar;
  cellpo a1,a2,f;

  p=deRef(p);

  if(FlagCell==p){
    outStr(fn,">>>");
  }

  if(trace_depth<0)
    putStr(fn,"...");	/* We dont print if we are too deep */
  else if(IsString(p))
    putString(fn,strVal(p));
  else if(isSymb(p)){		/* Display a symbol */
    uniChar *s=symVal(p);

    if(IsOperator(s)){
      putChar(fn,'(');		/* put brackets around operator symbols */

      putTypeSymbol(fn,s);
      putChar(fn,')');
    }
    else
      putTypeSymbol(fn,s);
  }
  else if(isChr(p)){
    putChar(fn,'\'');
    putChar(fn,'\'');
    putUniChar(fn,CharVal(p));
  }
  else if(isCall(p,kprocTp,&ar)){
    char *sep = "";
    unsigned long i;
    
    putChar(fn,'(');
    for(i=0;i<ar;i++){
      putStr(fn,sep);
      dispType(fn,consEl(p,i),trace_depth-1,offset+1,TERMPREC-1,scope,alt);
      sep = ", ";
    }
    putChar(fn,')');
    putChar(fn,LBRACE); putChar(fn,RBRACE);
  }
  else if(isListType(p,&a1)){
    dispType(fn,a1,trace_depth-1,offset+2,0,scope,alt);
    putChar(fn,'[');
    putChar(fn,']');
  }

  else if(IsQuery(p,&a1,&a2)){ 
    infprec(kquery,&lpr,&pr,&rpr); 
    open_bracket(fn,pr,prior,&offset);

    dispType(fn,a1,trace_depth-1,offset+2,lpr,scope,alt);
    putSymbol(fn,kquery);
    dispType(fn,a2,trace_depth-1,offset,rpr,scope,alt);

    close_bracket(fn,pr,prior,offset);
  }

  else if(IsForAll(p,&a1,&a2)){ 
    infprec(kforall,&lpr,&pr,&rpr); 
    open_bracket(fn,pr,prior,&offset);

    dispType(fn,a1,trace_depth-1,offset+2,lpr,scope,alt);
    putSymbol(fn,kforall);
    dispType(fn,a2,trace_depth-1,offset,rpr,scope,alt);

    close_bracket(fn,pr,prior,offset);
  }

  else if(isBinaryCall(p,kfunTp,&a1,&a2)){ /* keep on same line */
    infprec(kfn,&lpr,&pr,&rpr); /* Pick up priorities for -> operator*/
    open_bracket(fn,pr,prior,&offset);
    dispType(fn,a1,trace_depth-1,offset+2,lpr,scope,alt);
    putSymbol(fn,kfn);
    dispType(fn,a2,trace_depth-1,offset,rpr,scope,alt);
    close_bracket(fn,pr,prior,offset);
  }

  else if(isConstructorType(p,&a1,&a2)){ /* keep on same line */
    infprec(kconstr,&lpr,&pr,&rpr); /* Pick up priorities for ==> operator*/
    open_bracket(fn,pr,prior,&offset);
    dispType(fn,a1,trace_depth-1,offset+2,lpr,scope,alt);
    putSymbol(fn,kconstr);
    dispType(fn,a2,trace_depth-1,offset,rpr,scope,alt);
    close_bracket(fn,pr,prior,offset);
  }

  else if(IsUnaryStruct(p,&f,&a1) && isSymb(f)){
    if(preprec(symVal(f),&lpr,&pr)!= -1){
      open_bracket(fn,pr,prior,&offset);
      putTypeSymbol(fn,symVal(f));      /* Always show prefix op on same line */
      dispType(fn,a1,trace_depth-(pr-lpr),offset,lpr,scope,alt);
      close_bracket(fn,pr,prior,offset);
    }
    else if(postprec(symVal(f),&lpr,&pr)!= -1){
      open_bracket(fn,pr,prior,&offset);
      dispType(fn,a1,trace_depth-(pr-lpr),offset,lpr,scope,alt);
      putTypeSymbol(fn,symVal(f));      /* Always show prefix op on same line */
      close_bracket(fn,pr,prior,offset);
    }
    else{
      putTypeSymbol(fn,symVal(f));
      putChar(fn,'(');
      dispType(fn,a1,trace_depth-1,offset,TERMPREC-1,scope,alt);
      putChar(fn,')');
    }
  }
  else if(IsBinaryStruct(p,&f,&a1,&a2) && isSymb(f) && infprec(symVal(f),&lpr,&pr,&rpr)!= -1){
    open_bracket(fn,pr,prior,&offset);
    dispType(fn,a1,trace_depth-(pr-lpr),offset,lpr,scope,alt);
    putTypeSymbol(fn,symVal(f));        /* Always show prefix op on same line */
    dispType(fn,a2,trace_depth-(pr-lpr),offset,rpr,scope,alt);
    close_bracket(fn,pr,prior,offset);
  }
  else if(isCons(p)){
    cellpo f = consFn(p);
    unsigned long i;
    unsigned long ar = consArity(p);
    char *sep = "";
    
    putTypeSymbol(fn,symVal(f));
    putChar(fn,'(');

    for(i=0;i<ar;i++){
      putStr(fn,sep);
      dispType(fn,consEl(p,i),trace_depth-1,offset,TERMPREC-1,scope,alt);
      sep = ", ";
    }
    
    putChar(fn,')');
  }

  else if(IsTuple(p,&ar)){
    char c='(';
    if(tplArity(p)>0){
      for(i=0;i<tplArity(p);i++){
	putChar(fn,c); c=',';
	dispType(fn,tplArg(p,i),trace_depth-1,offset,TERMPREC-1,scope,alt);
      }
      putChar(fn,')');
    }
    else{
      putChar(fn,'(');
      putChar(fn,')');
    }
  }
  else if(IsTypeVar(p))
    putTvar(fn,p,'%',alt);

  else
    outMsg(fn,"Illegal type cell at 0x%x ",p);

  if(FlagCell == p){
    outStr(fn,"<<<");
  }
}

void open_bracket(ioPo f,int prec, int prior, int *offset)
{
  if(prec>TERMPREC){
    int i;

    if(prec>prior){
      *offset +=2;
      putChar(f,LBRACE); putChar(f,'\n');
      for(i=0;i<*offset;i++)
	putChar(f,' ');
    }
  }
  else if(prec>prior)
    putChar(f,LPAREN);
}

void close_bracket(ioPo f,int prec, int prior, int offset)
{
  if(prec>TERMPREC){
    if(prec>prior){
      putChar(f,'\n');
      while(--offset>0)
	putChar(f,' ');
      putChar(f,RBRACE); 
    }
  }
  else if(prec>prior)
    putChar(f,RPAREN);
}

static void show_op(ioPo f,int prec, int offset, uniChar *sym)
{
  if(prec>=1400){
    putSymbol(f,sym);
    putChar(f,'\n');
    while(--offset>=0)
      putChar(f,' ');
  }
  else
    putSymbol(f,sym);
}

static void displayChoice(ioPo fn,cellpo p,integer trace_depth,int offset,
			  int lpr,int rpr,
			  tracePo scope,logical alt)
{
  cellpo a1,a2;

  if(isBinaryCall(p,kchoice,&a1,&a2)){
    int i=offset-2;

    displayChoice(fn,a1,trace_depth,offset,lpr,rpr,scope,alt);

    if(!isSymb(deRef(a1))){
      putChar(fn,'\n');
      while(--i>=0)
	putChar(fn,' ');
    }
    else
      putChar(fn,' ');
    putSymbol(fn,kchoice);
    putChar(fn,' ');

    displayChoice(fn,a2,trace_depth,offset,lpr,rpr,scope,alt);
  }
  else
    display(fn,p,trace_depth,offset,lpr,scope,alt);
}


static uniChar lastChar='\0';

static void putChar(ioPo fn,uniChar c)
{
  lastChar=c;
  outChar(fn,c);
}

static void putStr(ioPo fn,char *s)
{
  outStr(fn,s);
  lastChar = s[strlen(s)-1];
}


static void symbolChar(ioPo fn,uniChar *s)
{
  uniChar ch = *s;
  
  if(isLetterChar(ch)||isNdChar(ch)){
    if(isLetterChar(lastChar) || isNdChar(lastChar))
      outChar(fn,' ');
  }
  else if(ch=='!'||ch=='@'||ch=='#'||ch=='$'||ch=='%'||ch=='^'||ch=='&'||
            ch=='*'||ch=='-'||ch=='='||ch=='+'||ch=='\\'||ch=='|'||ch==':'||
            ch=='<'||ch=='>'||ch=='.'||ch=='/'||ch=='?'||ch=='~'||ch=='`'){
    if(lastChar=='!'||lastChar=='@'||lastChar=='#'||lastChar=='$'||lastChar=='%'||
       lastChar=='^'||lastChar=='&'||lastChar=='*'||lastChar=='-'||lastChar=='='||
       lastChar=='+'||lastChar=='\\'||lastChar=='|'||lastChar==':'||lastChar=='<'||
       lastChar=='>'||lastChar=='.'||lastChar=='/'||lastChar=='?'||lastChar=='~'||lastChar=='`')
       outChar(fn,' ');
  }
  lastChar = s[uniStrLen(s)-1];
}

static void putSymbol(ioPo fn,uniChar *s)
{
  symbolChar(fn,s);  
  outMsg(fn,"%#U",s);
  lastChar = s[uniStrLen(s)-1];
}

static void putTypeSymbol(ioPo fn,uniChar *s)
{
  if(*s=='#'){
    uniChar *hash = uniLast(s,uniStrLen(s),'#');
  
    if(hash!=NULL && hash!=s){
      symbolChar(fn,++s);

      while(s!=hash){
    
        outChar(fn,*s);
        lastChar = *s++;
      }
    }
    else{
      symbolChar(fn,++s);

      while(*s!='\0'){
        outChar(fn,*s);
        lastChar = *s++;
      }
    }
  }
  else
    putSymbol(fn,s);
}

static void putInt(ioPo fn,long i)
{
  if(isLetterChar(lastChar) || isNdChar(lastChar))
    outChar(fn,' ');

  outInt(fn,i);
  lastChar = '0';
}

static void putHex(ioPo fn,long i)
{
  if(isLetterChar(lastChar) || isNdChar(lastChar))
    outChar(fn,' ');

  outMsg(fn,"0x%x",i);
  lastChar = '0';
}

static void putFloat(ioPo fn,FLOAT f)
{
  if(isLetterChar(lastChar) || isNdChar(lastChar))
    outChar(fn,' ');

  outFloat(fn,f);
  lastChar = '0';
}

static void putUniChar(ioPo fn,uniChar c)
{
  switch(c){
    case '\a':
      outStr(fn,"\\a");
      break;
    case '\b':
      outStr(fn,"\\b");
      break;
    case '\x7f':
      outStr(fn,"\\d");
      break;
    case '\x1b':
      outStr(fn,"\\e");
      break;
    case '\f':
      outStr(fn,"\\f");
      break;
    case '\n': 
      outStr(fn,"\\n");
      break;
    case '\r': 
      outStr(fn,"\\r");
      break;
    case '\t': 
      outStr(fn,"\\t");
      break;
    case '\v': 
      outStr(fn,"\\v");
      break;
    case '\\':
      outStr(fn,"\\\\");
      break;
    case '\"':
      outStr(fn,"\\\"");
      break;
    default:
      if(c<' '){
	outChar(fn,'\\');
	outChar(fn,((c>>6)&3)|'0');
	outChar(fn,((c>>3)&7)|'0');
	outChar(fn,(c&7)|'0');
      }
      else if(c>255){
        outMsg(fn,"\\+%4x",c);
        lastChar = '0';
      }
      else
	outChar(fn,c);
  }
}

static void putString(ioPo fn,uniChar *c)
{
  long len = uniStrLen(c);
    
  outChar(fn,'"');
  while(len--)
    putUniChar(fn,*c++);
  putChar(fn,'"');
}

static void putTvar(ioPo fn,cellpo p,char lead,logical alt)
{
  outChar(fn,' ');
  putChar(fn,lead);

  if(alt)
    putHex(fn,(long)p);
  else{
    unsigned int i;
    uniChar name[16] = {0};
    uniChar *nme = &name[0];

    for(i=0;i<topVar;i++)
      if(tvars[i]==p)
	break;

    if(i>=topVar){
      if(topVar>=NumberOf(tvars))
	syserr("too many type variables in display");

      tvars[topVar++]=p;
    }

    do{
      *nme++='a'+(i%('z'-'a'));
      i = i/('z'-'a');
    } while(i>0);

    *nme='\0';

    putSymbol(fn,name);
  }
}

void Display(ioPo f, cellpo p)
{
  lastChar = '\0';
  topVar = 0;
  display(f,p,10000,0,STMTPREC,NULL,True);
}

/* Print out the contents of a cell */
static void prt_cell(ioPo outFile,cellpo p,int trace_depth,tracePo scope,
		     logical alt)
{
  switch(tG(p=deRef(p))){
  case symbolic:
    putSymbol(outFile,symVal(p)); 		/* Display a symbol */
    break;
  
  case string:
    putString(outFile,strVal(p));
    break;

  case intger:
    putInt(outFile,intVal(p));
    break;
    
  case character:
    putChar(outFile,'\'');
    putChar(outFile,'\'');
    putUniChar(outFile,CharVal(p));
    break;

  case floating:
    putFloat(outFile,fltVal(p));
    break;

  case list:{
    if(trace_depth>0){
      char c='[';
      if(isEmptyList(p))
	outStr(outFile,"[]");
      else{
	while(isNonEmptyList(p)){
	  p = listVal(p);
	  outChar(outFile,c); c=','; /* print the element separator */
	  prt_cell(outFile,p,trace_depth,scope,alt);
	}
	p=Next(p);
	if(!IsList(p)){
	  outStr(outFile,",..");
	  prt_cell(outFile,p,trace_depth-1,scope,alt);
	}
	outChar(outFile,']');
      }
    }
    else
      outStr(outFile,"[...]");
    break;
  }

  case constructor:{
    if(trace_depth>0){
      unsigned long i;
      char c='(';
      
      prt_cell(outFile,consFn(p),trace_depth-1,scope,alt);

      if(consArity(p)>0){
	for(i=0;i<consArity(p);i++){
	  outChar(outFile,c); c=',';
	  prt_cell(outFile,consEl(p,i),trace_depth-1,scope,alt);
	}
	outChar(outFile,')');
      }
      else
	outStr(outFile,"()");
    }
    else
      outStr(outFile,"(...)");
    break;
  }
  
  case tuple:{
    if(trace_depth>0){
      unsigned long i;
      char *sep="";
      
      outStr(outFile,"(.");

      for(i=0;i<tplArity(p);i++){
	outStr(outFile,sep); sep = ",";
	prt_cell(outFile,tplArg(p,i),trace_depth-1,scope,alt);
      }
      outStr(outFile,".)");
    }
    else
      outStr(outFile,"(...)");
    break;
  }
  
  case codeseg:{
    outMsg(outFile,"<%t>" ,codetypes(codeVal(p)));
    break;
  }

  case tvar:{
    outMsg(outFile,"%%%x",p);
    break;
  }

  default:
    outMsg(outFile,"Illegal cell at 0x%x ",p);
  }

  if(typeInfo(p)!=NULL && alt){
    cellpo type = typeInfo(p);

    outStr(outFile,":");
    dispType(outFile,type,trace_depth-1,0,0,scope,alt);
    outStr(outFile," ");
  }
}

void dc(cellpo p)
{
  char *DEPTH = getenv("PRINTDEPTH");
  int depth;

  if(DEPTH==NULL||(depth=atof(DEPTH))==0)
    depth=8;

  if(p!=NULL){
    prt_cell(logFile,p,depth,NULL,True);
    if(source_line(p)!=-1){
      outMsg(logFile," @ " "%U:%d",FileTail(symVal(source_file(p))),source_line(p));
    }
  }
  else
    outMsg(logFile,"NULL");
  flushFile(logFile);
}

void dC(cellpo p)
{ 
  char *DEPTH = getenv("PRINTDEPTH");
  int depth;

  if(DEPTH==NULL||(depth=atoi(DEPTH))==0)
    depth=8;

  lastChar='\0';
  FlagCell = NULL;
  display(logFile,p,depth,0,TERMPREC,NULL,False);
  flushFile(logFile);
}

void dA(cellpo p)
{ 
  char *DEPTH = getenv("PRINTDEPTH");
  int depth;

  if(DEPTH==NULL||(depth=atoi(DEPTH))==0)
    depth=10;

  lastChar='\0';
  FlagCell = NULL;
  display(logFile,p,depth,0,TERMPREC,NULL,True);
  flushFile(logFile);
}

void dT(cellpo p)
{ 
  char *DEPTH = getenv("PRINTDEPTH");
  int depth;

  if(DEPTH==NULL||(depth=atoi(DEPTH))==0)
    depth=10;

  lastChar='\0';
  FlagCell = NULL;
  dispType(logFile,p,depth,0,TERMPREC,NULL,True);
  flushFile(logFile);
}

/* We have our own extension of outMsg */
static retCode _showCell(ioPo f,void *d,long width,long precision,logical alt)
{
  if(precision<=0)
    precision=STMTPREC;
  if(width<=0)
    width=32767;
  lastChar='\0';

  topVar = 0;
  display(f,(cellpo)d,width,0,precision,NULL,alt);
  FlagCell = NULL;
  return Ok;
}

static retCode _recCell(ioPo f,void *d,long width,long precision,logical alt)
{
  FlagCell = (cellpo)d;
  return Ok;
}

static retCode _showType(ioPo f,void *d,long width,long precision,logical alt)
{
  if(precision<=0)
    precision=STMTPREC;
  if(width<=0)
    width=32767;
  lastChar='\0';

  topVar = 0;
  dispType(f,(cellpo)d,width,0,precision,NULL,alt);
  FlagCell = NULL;
  return Ok;
}

void initDisplay(void)
{
  installMsgProc('w',_showCell);
  installMsgProc('W',_recCell);
  installMsgProc('t',_showType);
}


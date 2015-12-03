/* 
 * Operator precedence grammar parser for April programs
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

#include "config.h"
#include "compile.h"		/* main compiler related structures */
#include "token.h"
#include "operators.h"		/* definitions relating to operators */
#include "dict.h"
#include "keywords.h"
#include "grammar.h"
#include "cellP.h"
#include <string.h>

logical traceParse=False;	/* True if parse tracing is enabled */
extern long synerrors;		/* true if a syntax error has been reported */

token hedtok={0,{0},0};
token thistok={0,{0},0};	/* one token needed for look ahead */

typedef enum{
  outside,                      // Outside all brackets
  inside,                       // inside at least one level of brackets
  doubleinside                  // inside a double paren -- used for disambiguating tuples
} bracketNesting;

/* read in a term */
static int term(ioPo inFile,int prior,bracketNesting inside,logical debug); 
static long tupleTerm(ioPo inFile,long prior,logical debug,bracketNesting inside);

static int term00(ioPo inFile,logical debug,bracketNesting where)
{
  tokenType tk;
  int line_num;

  tk = nextoken(inFile,&thistok);
  line_num = thistok.line_num;

  switch(tk){

  case strToken:
    p_string(thistok.tk.name);	/* we have a string */
    if(debug)
      mkLine(thistok.line_num); /* create the line info */

    /* Concatenate sequences of strings into a single string */
    while((tk=hedtoken(inFile,&hedtok))==strToken){
      commit_token();
      p_string(hedtok.tk.name);
      join_strings();		/* concatenate the top two strings */
    }
    return MINPREC;

  case idToken:
    p_sym(thistok.tk.name);

    if(thistok.tk.name==kpcent){
      if(debug)
	mkLine(thistok.line_num); /* create the line info */

      if((tk=hedtoken(inFile,&hedtok))==rparToken)
	return MINPREC;
      else if(tk!=idToken)
	reportParseErr(hedtok.line_num,"identifier required after %c",'%');
      else{
	p_sym(hedtok.tk.name);
	if(debug)
	  mkLine(hedtok.line_num);      /* create the line info */
	p_cons(1);		        /* construct op(arg) */
	if(debug)		        /* generate line number info */
	  mkLine(hedtok.line_num);
	commit_token();
	return MINPREC;
      }
    }
    else{
      if(IsOperator(thistok.tk.name)) /* Prefix operator case already done */
	if((tk=hedtoken(inFile,&hedtok))!=rparToken){
	  reportParseErr(thistok.line_num,"unexpected operator %U",tokenStr(&thistok));
	}
      if(debug)
	mkLine(thistok.line_num); /* create the line info */
      return MINPREC;
    }

  case intToken:		/* we have an integer */
    p_int(thistok.tk.i);
    if(debug)
      mkLine(thistok.line_num); /* create the line info */
    return MINPREC;
    
  case chrToken:                /* We have a character literal */
    p_char(thistok.tk.ch);
    if(debug)
      mkLine(thistok.line_num); /* create the line info */
    return MINPREC;

  case fltToken:		/* we have a floating point number */
    p_flt(thistok.tk.f);
    if(debug)
      mkLine(thistok.line_num); /* create the line info */
    return MINPREC;
    
  case lparToken:{
    int startline = thistok.line_num;
    
    if(hedtoken(inFile,&hedtok)==rparToken){
      p_cell(ztpl);		/* () on its own = () */
      commit_token();
      if(debug)
	mkLine(thistok.line_num); /* create the line info */
      return MINPREC;
    }
    else{
      term(inFile,STMTPREC,inside,debug);
      if((tk=nextoken(inFile,&thistok))!=rparToken)
	reportParseErr(thistok.line_num,"')' or ';' missing - near %U%s",
		      tokenStr(&thistok),sourceLocator("\n')' at ",startline));
      return MINPREC;
    }
  }

  case ltplToken:{		/* we have a tuple */
    int startline = thistok.line_num;
    
    if(hedtoken(inFile,&hedtok)==rtplToken){
      p_cell(ztpl);		/* (..) on its own = () */
      commit_token();
      if(debug)
	mkLine(thistok.line_num); /* create the line info */
      return MINPREC;
    }
    else{
      
      p_tuple(tupleTerm(inFile,TERMPREC-1,debug,inside));

      if((tk=nextoken(inFile,&thistok))!=rtplToken)
	reportParseErr(thistok.line_num,"'.)' missing - near %U%s",
		      tokenStr(&thistok),sourceLocator("\n'(.' at ",startline));
      return MINPREC;
    }
  }

  case lbrceToken:{
    int startline = thistok.line_num;

    if((tk=hedtoken(inFile,&thistok))!=rbrceToken){
      term(inFile,STMTPREC,inside,debug);
      if((tk=nextoken(inFile,&thistok))!=rbrceToken){
	reportParseErr(thistok.line_num,"'}' or ';' missing - near %U%s",
		      tokenStr(&thistok),sourceLocator("\n'{' at ",startline));
	}
    }
    else{
      p_sym(kblock);		/* {} on its own = "{}" */
      commit_token();
      if(debug)
	mkLine(thistok.line_num); /* create the line info */
    }
    return MINPREC;
  }

  case lbraToken:{		/* we have a list */
    int startline = thistok.line_num;
    long count = 0;

    /* We push each el. onto stack, and pop off whole list at end */
    if((tk=hedtoken(inFile,&thistok))!=rbraToken){
      do{
	count++;
	term(inFile,TERMPREC-1,inside,debug); /* list elements */
	if(debug)		/* Insert line number information */
	  mkLine(thistok.line_num); 
      } while((tk=nextoken(inFile,&thistok))==commaToken);
      if(tk==cnsToken){		/* We have a list tail operator */
	term(inFile,TERMPREC-1,inside,debug); /* tail element */
	if((tk=nextoken(inFile,&thistok))!=rbraToken)
	  reportParseErr(thistok.line_num,"']', ',..' or ',' missing - near %U%s",
			tokenStr(&thistok),sourceLocator("\nlist starts at ",startline));
      }
      else if(tk==rbraToken)
	p_null();
      else{
	reportParseErr(thistok.line_num,"']', ',..' or ',' missing - near %U%s",
		       tokenStr(&thistok),sourceLocator("\nlist starts at ",startline));
	p_null();
      }
      while(count--){
	p_list();		/* construct list based on number of elements counted */
	if(debug)		/* Insert line number information */
	  mkLine(thistok.line_num); 
      }
    }
    else{
      commit_token();
      p_null();			/* build empty list */
    }

    if(debug)
      mkLine(thistok.line_num); /* create the line info */

    return MINPREC;
  }

  case qtToken:{			/* Escape a symbol */
    int line_num = thistok.line_num;

    if((tk=nextoken(inFile,&thistok))!=idToken)
      reportParseErr(thistok.line_num,"identifier should follow ' -- near %U",
		       tokenStr(&thistok));
    else{
      long len = uniStrLen(thistok.tk.name)+2;
      uniChar sym[len];
      
      strMsg(sym,len,"'%U",thistok.tk.name);
      p_sym(kquote);
      p_sym(locateU(sym));
      
      if(debug)			/* Insert line number information */
	mkLine(thistok.line_num); 
      p_cons(1);		/* '(S) */
      if(debug)			/* Insert line number information */
	mkLine(line_num);

#ifdef TRACEPARSE
      if(traceParse){
        outMsg(logFile,"Quoted symbol %w\n",topStack());
        flushFile(logFile);
      }
#endif

    }
    return MINPREC;
  }
      
  case eofToken:
    return uniEOF;

  case rbrceToken:
    reportParseErr(thistok.line_num,"extra '}' - near %U",tokenStr(&thistok));

    p_null();
    return MINPREC;

  case rparToken:
    reportParseErr(thistok.line_num,"extra ')' - near %U",tokenStr(&thistok));

    p_null();
    return MINPREC;

  case rtplToken:
    reportParseErr(thistok.line_num,"extra '.)' - near %U",tokenStr(&thistok));

    p_null();
    return MINPREC;

  default:{
    reportParseErr(thistok.line_num,"error near %U",tokenStr(&thistok));

    p_null();
    return MINPREC;
  }
  }
}

static long tupleTerm(ioPo inFile,long prior,logical debug,bracketNesting where)
{
  long arity = 0;
  
  while(True){
    term(inFile,prior,where,debug);        // Tuple element
    arity++;
    if(hedtoken(inFile,&hedtok)!=commaToken)
      break;
    else
      commit_token();
  }

  return arity;
}
  

static int term0(ioPo inFile,logical debug,bracketNesting where)
{

  if(term00(inFile,debug,where)==uniEOF)
    return uniEOF;

#ifdef TRACEPARSE
  if(traceParse)
    outMsg(logFile,"term00 = %w \n",topStack());
#endif

  /* Now look ahead to see if we have an application term */
  while(True){			/* We will break out of this loop */
    tokenType htk = hedtoken(inFile,&hedtok);
    int refline = hedtok.line_num;

    switch(htk){
    case ltplToken:{                     // We allow this for some reason
      commit_token();
      
      if((htk=hedtoken(inFile,&hedtok))==rtplToken){
        p_cons(0);
        if(debug)			/* generate line number info */
	  mkLine(refline); 
        commit_token();
        continue;
      }
      else{
        int startline = thistok.line_num;
        long arity = tupleTerm(inFile,TERMPREC-1,debug,inside);
      
        p_cons(arity);
        if(debug)			/* generate line number info */
	  mkLine(refline); 
	  
        if((htk=nextoken(inFile,&thistok))!=rtplToken)
	  reportParseErr(thistok.line_num,"'.)' or ';' missing - near %U%s",
		      tokenStr(&thistok),sourceLocator("\n'(.' at ",startline));
        continue;
      }
    }
    case lparToken:{
      commit_token();
      
      if((htk=hedtoken(inFile,&hedtok))==rparToken){
        p_cons(0);
        if(debug)			/* generate line number info */
	  mkLine(refline); 
        commit_token();
        continue;
      }
      else{
        int startline = thistok.line_num;
        long arity = tupleTerm(inFile,TERMPREC-1,debug,inside);
      
        p_cons(arity);
        if(debug)			/* generate line number info */
	  mkLine(refline); 
	  
        if((htk=nextoken(inFile,&thistok))!=rparToken)
	  reportParseErr(thistok.line_num,"')' or ';' missing - near %U%s",
		      tokenStr(&thistok),sourceLocator("\n'(' at ",startline));
        continue;
      }
    }
    case lbraToken:{
      commit_token();                   // We have to look for xxx[]
      
      if((htk=hedtoken(inFile,&hedtok))==rbraToken){
        cell A;
        cell B;
        
        commit_token();
        
        popcell(&A);
        
        p_cell(BuildUnaryStruct(cbkets,&A,&B));
        if(debug)			/* generate line number info */
	  mkLine(refline); 
        continue;
      }
      else{
        int startline = thistok.line_num;
        long count = 0;

        do{
	  count++;
	  term(inFile,TERMPREC-1,inside,debug); /* list elements */
	  if(debug)		/* Insert line number information */
	    mkLine(thistok.line_num); 
        } while((htk=nextoken(inFile,&thistok))==commaToken);
        if(htk==cnsToken){		/* We have a list tail operator */
	  term(inFile,TERMPREC-1,inside,debug); /* tail element */
	  if((htk=nextoken(inFile,&thistok))!=rbraToken)
	    reportParseErr(thistok.line_num,"']', ',..' or ',' missing - near %U%s",
			tokenStr(&thistok),sourceLocator("\nlist starts at ",startline));
        }
        else if(htk==rbraToken)
	  p_null();
        else{
	  reportParseErr(thistok.line_num,"']', ',..' or ',' missing - near %U%s",
		       tokenStr(&thistok),sourceLocator("\nlist starts at ",startline));
	  p_null();
        }
        while(count--){
	  p_list();		/* construct list based on number of elements counted */
	  if(debug)		/* Insert line number information */
	    mkLine(thistok.line_num); 
        }
        p_swap();
        p_cons(1);                      // Construct the closure ...
        continue;
      }
    }

    case lbrceToken:
      if(term00(inFile,debug,where)==uniEOF)
	return uniEOF;
      if(IsSymbol(topStack(),kblock)){
        long ar;
        p_swap();                       // (e,..,e){} becomes ({})(e,..,e)
        ar = p_untuple();               // pop and unwrap top tuple
        p_cons(ar);
      }
      else
        p_cons(1);		        /* generate a call */
      if(debug)			        /* generate line number info */
	mkLine(refline); 
      continue;

    case idToken:
      if(hedtok.tk.name!=kpcent && IsOperator(hedtok.tk.name))
	return MINPREC;
    case strToken:
    case intToken:			/* we have an integer */
    case fltToken:			/* we have a floating point number */
    case chrToken:                      // Character literal
    case qtToken:			/* Escape a symbol */
      if(term00(inFile,debug,where)==uniEOF)
	return uniEOF;
      p_cons(1);		        /* generate a call */
      if(debug)			        /* generate line number info */
	mkLine(refline); 
      continue;
    default:
      return MINPREC;		        /* no more applications */
    }
  }
  return MINPREC;
}

static int term(ioPo inFile,int prior,bracketNesting where,logical debug)
{
  int prec,lprec,rprec,lprior,infl,inf;
  token hedtok;
  tokenType tk=hedtoken(inFile,&hedtok);	/* pick up the lookahead token */

  switch(tk){
  case idToken:
    if(hedtok.tk.name!=kpcent)
      if(preprec(hedtok.tk.name,&lprior,&rprec)!=-1){
	if(lprior<=prior){
	  symbpo prefop = hedtok.tk.name;

	  commit_token();		/* commit to the prefix operator */

	  p_sym(hedtok.tk.name);
	  if(debug)
	    mkLine(hedtok.line_num); /* create the line info */

	  if((tk=hedtoken(inFile,&hedtok))==rparToken){
	    lprior=MINPREC;
	    break;
	  }

	  term(inFile,rprec,where,debug);
	  if(prefop==kminus&&(IsInt(topStack())||IsFloat(topStack()))){
	    cell K;
	    popcell(&K);	/* convert -(K) to -K */
	    if(IsInt(&K))
	      mkInt(&K,-intVal(&K));
	    else
	      mkFlt(&K,-fltVal(&K));
	    p_drop();		/* drop the prefix operator also */
	    p_cell(&K);
	  }
	  else
	    p_cons(1);		/* construct op(arg) */
	  if(debug)		/* generate line number info */
	    mkLine(hedtok.line_num);

	     
	  break;			/* end of prefix operator handling */
	}
	else{
	  reportParseErr(hedtok.line_num,"prefix operator -- %s -- "
			 "has priority %d greater"
			 " than allowed priority of %d -- try inserting {}s",
			 hedtok.tk.name,lprior,prior);

	  commit_token();
	  p_sym(knull);
	  return uniEOF;
	}
      }	/* otherwise fall through to term0 case */

  case strToken:
  case intToken:
  case fltToken:
  case chrToken:
  case lbraToken:
  case lbrceToken:
  case lparToken:
  case ltplToken:
  case qtToken:
    lprior = term0(inFile,debug,where); /* read in a term 0 */
#ifdef TRACEPARSE
    if(traceParse){
      outMsg(logFile,"Term0 = %w\n",topStack());
      flushFile(logFile);
    }
#endif
        
    break;

  case eofToken:
    return uniEOF;

  default:{
    reportParseErr(hedtok.line_num,"parsing error near %U",tokenStr(&hedtok));
    commit_token();

    p_null();
  }
  }

  /* at this point, we look for infix and postfix operators. 
     lprior = precedence of the left hand term */
 rator:
  switch((tk=hedtoken(inFile,&hedtok))){
  case semiToken:		/* semi-colon terminates when at outer level */
    if(where==outside)
      return lprior;
    else{
      hedtok.tk.name=ksemi; 	/* otherwise just treat as normal symbol */
      goto opsymb;                      // Lucky its a short hop
      }

  case commaToken:
    hedtok.tk.name = kcomma;	/* treat comma as a normal symbol */

  opsymb:

  case idToken:{			/* idents and symbols may be operators */
    if(postprec(hedtok.tk.name,&lprec,&prec)!=-1 && 
       lprec>=lprior && prec <= prior){
      commit_token();		/* we have a post-fix operator */

      /* might be infix as well ? */
      if(infprec(hedtok.tk.name,&infl,&inf,&rprec)!=-1 &&
	 infl>=lprior && inf <= prior){
	int ptk;
	token ptok;

	/* try to decide if the op should be postfix or infix */
	switch((ptk=hedtoken(inFile,&ptok))){
	case intToken:		/* more data means operator in infix form */
	case chrToken:
	case fltToken:
	case strToken:
	case lbraToken:
	case lbrceToken:
	case lparToken:
	case ltplToken:
	case qtToken:
	  goto useinfix;	/* another data item */
	case eofToken:
	case rbraToken:
	case cnsToken:
	case rbrceToken:
	case rparToken:
	case rtplToken:
	case commaToken:
	  goto usepostfix;	/* no more data means use postfix form */
	case semiToken:
          if(where==outside)
	    goto usepostfix;	/* ';' is a terminator only outside { }'s */
	  else
	    ptok.tk.name=ksemi;

	case idToken:
	  if(IsOperator(ptok.tk.name)){
	    int lpr,pr,rpr;
	    if((preprec(ptok.tk.name,&pr,&rpr)!=-1 && pr<=rprec)||
	       (infprec(ptok.tk.name,&lpr,&pr,&rpr)!=-1 && lpr<=lprior && pr <=rprec))
	      goto useinfix;	/* not completely foolproof */
	  }
	  else goto useinfix;	/* symbol is not an operator */
	}
      }
    usepostfix:{
      p_sym(hedtok.tk.name);
      if(debug)
	mkLine(hedtok.line_num); /* create the line info */
      p_swap();
      p_cons(1);		/* build op(arg) */
      lprior = prec;		/* set the left priority from postfix op */
      if(debug)			/* generate line number info */
	mkLine(hedtok.line_num); 

      goto rator;		/* and look for more */
      }
    }
    else if(infprec(hedtok.tk.name,&lprec,&prec,&rprec)!=-1 &&
	    lprec>=lprior && prec <= prior){
      commit_token();		/* commit to the operator */

    useinfix:
        
      if(hedtok.tk.name!=kcomma){
	p_sym(hedtok.tk.name);
	if(debug)
	  mkLine(hedtok.line_num); /* generate line info */
	p_swap();
	term(inFile,rprec,where,debug);	/* right-hand-side */
	p_cons(2);

	lprior = prec;		/* priority of left = prec of infix operator */
	if(debug)
	  mkLine(hedtok.line_num); /* generate line info */
	goto rator;
      }
      else{
	long count = 1;		/* We start counting the elements of a tuple */
	int refline = hedtok.line_num;

	while(tk==commaToken){
	  commit_token();
	  term(inFile,lprec,inside,debug);	/* elements of a tuple */

	  tk = hedtoken(inFile,&hedtok);
	  count++;
	}
	p_tuple(count);
	if(debug)
	  mkLine(refline); /* generate line info */
      }
    }
    return lprior;		/*  */
  }

  case rbraToken:		/* expression terminator symbols */
  case rbrceToken:
  case rparToken:
  case rtplToken:
  case cnsToken:
  case eofToken:
    return lprior;		/* end-of-file is also a terminator */

  default:
    reportParseErr(thistok.line_num,"horrible syntax error - near %U",tokenStr(&hedtok));

    commit_token();
    return lprior;		/* return something! */
  }
}

int parse(ioPo inFile,int prior,logical debug,long *line)
{
  tokenType tk;
  token tok;

  init_token(*line);			/* set up the tokeniser */

  do{
    cellpo orig = topStack();

    synerrors = 0;
    
    prior = term(inFile,prior,outside,debug);

    if(prior!=uniEOF && (tk=nextoken(inFile,&tok))!=semiToken && tk !=eofToken){
      reportParseErr(tok.line_num,"';' missing - near %U",tokenStr(&thistok));

      expectToken(inFile,semiToken);
    }

    if(synerrors)
      reset_stack(orig);
  } while(synerrors>0);	/* ignore any terms which have syntax errors */

  *line = currentLineNumber();          /* totally hacky, but who cares right now */
  return prior;			/* return precedence of structure found */
}

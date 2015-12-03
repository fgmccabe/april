/*
  Symbol dictionary maintenance functions 
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
#include <stdlib.h>		/* Standard functions */
#include <string.h>
#include "compile.h"		/* Compiler structures */
#include "keywords.h"		/* Standard April keywords */
#include "hash.h"
#include "dict.h"		/* main dictionary structures */

static hashPo dictionary;	/* The main symbol table */

/* syntax keywords */
symbpo ksemi,kcomma,kchoice,kguard,kassign;
symbpo kplus,kiplus,kminus,kiminus,kslash,kislash,kpcent,krem,kstar,kistar,kshriek;
symbpo ktcoerce,kquery,kuscore,ktvar,kcontain,kcoerce;
cellpo cuscore;
symbpo kunion,kappend,kintersect,kdiff,kselect,kreject,kpi;
symbpo kindex,kcardinality;
symbpo kmap,kreduce,kfront,kback,khead,ktail,krand,ksort;
symbpo kmeta,khat;
symbpo kwithin;

/* Type keywords */
symbpo kany,khandle,khdl,knullhandle,kinteger,knumber,kchar,ksymbol,kstring,klogical,kopaque;
symbpo knumberTp,ksymbolTp,kcharTp,khandleTp,kanyTp,klogicalTp,kallQ,kfunTp,kprocTp,ktplTp,klistTp,kopaqueTp;
cellpo numberTp,symbolTp,charTp,stringTp,handleTp,anyTp,logicalTp,allQ,funTp,procTp,tplTp,listTp,opaqueTp;

symbpo kstate,kdead,kquiescent,krunnable,kwaitio,kwaitmsg,kwaittimer;
symbpo kif,kthen,kelse,klabel,kfield,kdefn;
cellpo ctrue,cfalse;
cellpo cquery,cdefn;
cellpo cnullhandle,emptylist;

/* time expression keywords */
symbpo kcolon,ktimeout,know,ktoday;

/* process expressions */
symbpo kvalueof,kvalis,kresult,kof,kcollect,ksetof,kelement;
symbpo krelax;
symbpo kleave,khalt,kdie,kself,kcreator;
symbpo kblock,kempty,kbkets,kcons,ktype,ktpname,kenum,ktypeof,ktof,ktstring;
symbpo kmacreplace,kfn,kstmt,kforall,kconstr;
cellpo cblock,cempty,cbkets,cinfo,csemi,ccomma;
cellpo nilcell;

/* compiler related keywords */
symbpo kquote,kquoted,kanti;
cellpo cquote;

/* Special keywords for handling closures */
symbpo ktoplevel;
cellpo cclause,cequation,ctoplevel;

/* Special escapes which may be folded */
symbpo kcatenate,kmergeURL,kchrcons,kstrlen,kascii,kcharof;
symbpo kband,kbor,kbxor,kbnot,kbleft,kbright;

/* message handling keywords */
symbpo kguard;
symbpo kreplyto,ksender,kfrom,ksend;
cellpo creplyto,csender,csend;

/* Syntax related keywords */
symbpo ktry,kcatch,kraise,kfor,kwhile,kdo,kcase,kdots,kdot;
symbpo kenvironment,kfailed;
cellpo cenvironment,cfailed;

/* predicates */
symbpo kequal,knotequal,klessthan,kgreaterthan,klesseq,kgreatereq;
symbpo ksame,knotsame,kmatch,kin,knomatch;
symbpo kand,kdand,kor,kdor,knot,kdnot;
cellpo cnot,cor,cand;

/* Macro keywords */
symbpo kmacro,khash,kdhash,ktilda,kat,kinclude,kfile;
symbpo kdebug,knodebug,kdebugdisplay,kop,kecho,kerror,kwarning,klist2tuple,ktuple2list,ktuple,klist,ktypestring;
cellpo cdebugdisplay;
symbpo kleft,kright,knonassoc,kprefix,kaprefix,kpostfix,kapostfix;
cellpo cerror,cmatcherr,cfail;

symbpo ktclosure;
cellpo cclosure, ctclosure, ctpl;

symbpo knull,kvoid,kline,kentry,d_exit;
cellpo cnull;
symbpo d_return,d_assign,d_send,d_accept,d_fork,d_die;

symbpo kcall,ktrue,kfalse;

symbpo kwaitdebug;

/* Some special compiler entries */
cellpo c_zero,c_one,voidcell,emptystring;
cellpo ztpl;		/* The zero-tuple */

/* Locate a symbol (in ASCII) in the dictionary */
symbpo locateC(const char *name)
{
  uniChar nm[strlen(name)+1];
  
  _uni((unsigned char*)name,nm,strlen(name)+1);
  
  return locateU(nm);
}

/* Locate a symbol in the dictionary */
symbpo locateU(const uniChar *name)
{
  symbpo sym = Search((void*)name,dictionary);

  if(sym==NULL){		/* A new entry in the table */
    uniChar *s = (uniChar *)malloc(sizeof(uniChar)*(uniStrLen(name)+1));
    symbpo d = (symbpo)uniCpy(s,uniStrLen(name)+1,name);

    if(d==NULL)
      syserr("no space for dictionary entry");	/* should never happen */
    Install(d,d,dictionary);
    return d;			/* return the symbol */
  }
  else
    return sym;			/* return the existing symbol */
}

char *symN(uniChar *name)
{
  static char nn[1024];

  _utf(name,(unsigned char*)nn,NumberOf(nn));

  return nn;
}

void init_dict(void)
{
  dictionary = NewHash(1024,(hashFun)uniHash,(compFun)uniCmp,NULL);

  /* result keywords */
  mkSymb((cnull=allocSingle()),knull=locateC(""));

  /* special value symbols */
  mkSymb((ctpl=allocSingle()),(kcall=locateC("()")));		/* keyword which denotes a procedure call */
  mkSymb((cclosure=allocSingle()),locateC("<closure>")); /*  closure */

  ktclosure=locateC("<tclosure>"); /* theta closure */
  mkSymb((ctclosure=allocSingle()),ktclosure); /* theta closure */

  /* Debugging keywords */
  kecho = locateC("echo");	/* used to echo at compile time */

/* Declare the keywords and operators */
  mkSymb(ccomma=allocSingle(),kcomma=locateC(","));

/* statement level operators */
  mkSymb(csemi=allocSingle(),ksemi=locateC(";"));

  kchoice=locateC("|");

  kdo=locateC("do");
  kfor = locateC("for");
  kwhile = locateC("while");
  kelse=locateC("else"); /* set up if-then-else operators */
  kthen=locateC("then");
  kif=locateC("if");
  kcase=locateC("case");
  klabel=locateC(":-");		/* labelled statement marker */

  kfield=locateC(":");		/* record field designator */
  mkSymb((cdefn=allocSingle()),kdefn=locateC("=")); 
  mkSymb((cblock=allocSingle()),kblock=locateC("{}"));
  mkSymb((cempty=allocSingle()),kempty=locateC("()"));
  mkSymb((cbkets=allocSingle()),kbkets=locateC("[]"));
  
  kcons=locateC(",..");
  mkList((nilcell=allocSingle()),NULL);

  kplus=locateC("+");
  kiplus = locateC("+.");

  kminus=locateC("-");
  kiminus = locateC("-.");

  kstar=locateC("*");
  kistar=locateC("*.");

  kslash=locateC("/");
  kislash = locateC("/.");

  kpcent=locateC("%");
  krem = locateC("rem");

  kband = locateC("band");
  kbor = locateC("bor");
  kbnot = locateC("bnot");
  kbxor = locateC("bxor");
  kbleft = locateC("bleft");
  kbright = locateC("bright");

  mkSymb(cquery=allocSingle(),kquery=locateC("?"));
  ktvar = locateC("%");
  mkSymb(cuscore=allocSingle(),kuscore = locateC("_"));

  kcontain = locateC("./");
  
  /* Process the type keywords */
  kinteger = locateC("integer");
  knumber = locateC("number");

  mkSymb((numberTp=allocSingle()),knumberTp=locateC("#number"));

  ksymbol = locateC("symbol");
  mkSymb((symbolTp=allocSingle()),ksymbolTp=locateC("#symbol"));
  
  kopaque = locateC("opaque");
    
  khandle = locateC("handle");
  khdl = locateC("hdl");
  mkSymb((handleTp=allocSingle()),khandleTp=locateC("#handle"));
  mkSymb((cnullhandle=allocSingle()),(knullhandle = locateC("nullhandle")));
  
  kchar = locateC("char");
  mkSymb((charTp=allocSingle()),kcharTp=locateC("#char"));
   
  kany = locateC("any");
  mkSymb((anyTp=allocSingle()),kanyTp=locateC("#any"));

  kvoid = locateC("void");
  mkSymb((voidcell=allocSingle()), kvoid);

  klogical = locateC("logical");
  mkSymb((logicalTp=allocSingle()),klogicalTp=locateC("#logical"));

  mkSymb((ctrue=allocSingle()),ktrue=locateC("true"));
  mkSymb((cfalse=allocSingle()),kfalse=locateC("false"));

  mkSymb((opaqueTp=allocSingle()),kopaqueTp = locateC("#opaque"));
  
  mkSymb(allQ=allocSingle(),kallQ=locateC("#-"));
  mkSymb(funTp=allocSingle(),kfunTp=locateC("#=>"));
  mkSymb(procTp=allocSingle(),kprocTp=locateC("#{}"));
  mkSymb(tplTp=allocSingle(),ktplTp=locateC("#()"));
  mkSymb(listTp=allocSingle(),klistTp=locateC("#list"));

  stringTp = BuildListType(charTp,allocSingle());  
  kstring = locateC("string");
  
  kwithin = locateC("within_");

  kstate = locateC("process_state");
  kdead = locateC("dead");
  kquiescent = locateC("quiescent");
  krunnable = locateC("runnable");
  kwaitio = locateC("wait_io");
  kwaitmsg = locateC("wait_msg");
  kwaittimer = locateC("wait_timer");

  kcatch = locateC("onerror");
  kraise = locateC("exception");
  ktry = locateC("try");
  mkSymb(cerror=allocSingle(),kerror = locateC("error"));
  mkSymb(cmatcherr=allocSingle(),locateC("'matcherr"));
  mkSymb(cfail=allocSingle(),locateC("fail"));

  kwarning = locateC("warning");

  kassign=locateC(":=");

  kunion=locateC("\\/");

  kintersect=locateC("/\\");

  kappend = locateC("<>");

  kdiff=locateC("\\");

  kindex=locateC("#");
  kcardinality=locateC("listlen");

  kselect=locateC("^/");

  kdot=locateC(".");

  mkSymb((cenvironment=allocSingle()),(kenvironment=locateC("environment")));

  mkSymb((cfailed=allocSingle()),(kfailed=locateC("failed")));

  kreject=locateC("^\\");

  kmap=locateC("//");

  kreduce=locateC("\\\\");

  /* time expression keywords */

  kcolon = locateC(":");

  ktimeout = locateC("timeout");

  kcatenate = locateC("++");
  kchrcons = locateC(",+");
  kstrlen = locateC("strlen");
  kascii = locateC("ascii");
  kcharof = locateC("charof");
  kmergeURL = locateC("__mergeURL");

  /* metalogical and compiler keywords */

  mkInt((c_zero=allocSingle()),0);		/* some common integers */
  mkInt((c_one=allocSingle()),1);
  mkStr((emptystring=allocSingle()),allocCString(""));
  
  mkList(emptylist=allocSingle(),NULL);

  kquote = locateC("'");
  mkSymb(cquote = allocSingle(),kquote);
  kanti = locateC("&");
  kquoted = locateC("quote");

  kcoerce=locateC("_coerce");
  ktcoerce = locateC("%%");

  kmeta=locateC("`");

  kvalueof = locateC("valof");
  kvalis=locateC("valis");

  kcollect = locateC("collect");
  kelement=locateC("elemis");
  ksetof = locateC("setof");

  mkTpl((ztpl=allocSingle()),allocTuple(0));

  krelax = locateC("relax");

  kleave=locateC("leave");
  khalt = locateC("halt");
  kdie = locateC("abort");

  /* Matching keywords */
  kguard = locateC("::");
  kshriek = locateC("!");

  /* Process keywords */

  mkSymb((creplyto=allocSingle()),kreplyto = locateC("replyto"));
  mkSymb((csender=allocSingle()),ksender = locateC("sender"));
  mkSymb((csend=allocSingle()),ksend = locateC(">>"));

  kfrom = locateC("~~");

  kcreator = locateC("creator");	/* We compile this one specially too */

  khead = locateC("head");
  ktail = locateC("tail");
  kfront = locateC("front");
  kback = locateC("back");
  ksort = locateC("sort");
  krand = locateC("rand");
  kpi = locateC("pi");

  kmatch=locateC(".=");
  knomatch = locateC("!.=");
  kequal = locateC("==");
  knotequal = locateC("!=");
  ksame = locateC("===");
  knotsame = locateC("!==");
  klessthan = locateC("<");
  kgreaterthan = locateC(">");
  klesseq = locateC("<=");
  kgreatereq = locateC(">=");

  kin=locateC("in");		/* set related predicates */
  kdots=locateC("..");

  kand=locateC("and");
  mkSymb(cand=allocSingle(),kdand=locateC("&&"));

  kor = locateC("or");
  mkSymb(cor=allocSingle(),kdor = locateC("||"));
  
  mkSymb((cnot=allocSingle()),(kdnot = locateC("!")));
  knot = locateC("not");

  /* Macro-processor operators and keywords */
  kmacro = locateC("macro");

  kmacreplace = locateC("==>");
  kfn = locateC("=>");
  kstmt = locateC("->");
  kforall = locateC("-");	/* same as minus */
  kconstr = locateC("==>");

  ktype=locateC("::=");		/* Type definition */
  ktpname = locateC("::=>");	/* type renaming operator */
  kenum = locateC(":E:");	/* enumerated symbol internal operator */

  ktypestring = locateC("type");
  ktypeof=locateC("typeof");	/* Used to refer to types of other vars */
  ktof=locateC("type_of");	/* Used to refer to types of other vars */
  ktstring = locateC("_type_signature_"); /* used by the compiler to construct a type string */

  khash = locateC("#");
  kdhash = locateC("##");	/* used to `glue' tokens together */

  kat = locateC("@");		/* used to take apart function calls */

  ktilda = locateC("~");		/* negated pattern and string pattern width*/
  khat = locateC("^");		/* precision specifier */

  kop = locateC("op");
  kinclude = locateC("include"); 
  kfile = locateC("file");

  knodebug = locateC("_nodebug_");
  kdebug = locateC("_debug_");
  mkSymb(cdebugdisplay=allocSingle(),kdebugdisplay=locateC("__debug_display__")); /* debugging display function */

  kwaitdebug = locateC("_debug_wait");
  
  klist2tuple = locateC("tuple");
  ktuple2list = locateC("list");
  ktuple = locateC("tuple");
  klist = locateC("list");

  kresult = locateC("<ret>");	/* used to signal result of function */

  mkSymb(cequation=allocSingle(),locateC("equation"));
  mkSymb(cclause=allocSingle(),locateC("clause"));
  mkSymb(ctoplevel=allocSingle(),ktoplevel = locateC("toplevel"));
}


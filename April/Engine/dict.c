/*
  Dictionary handling functions for April
  (c) 1994-1999 Imperial College & F.G. McCabe

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
#include <string.h>		/* Access string defs */
#include <stdlib.h>		/* Memory allocation etc. */
#include <assert.h>		/* Run-time predicate verification */

#include "april.h"
#include "dict.h"
#include "symbols.h"
#include "hash.h"		/* access the hash functions */
#include "std-types.h"

/* standard symbols */
objPo khandle;			/* handle record label */
objPo knullhandle;		/* null handle */
objPo kprocess;			/* process record label */
objPo ktermin;			/* terminate process marker */
objPo kvoid;			/* void value marker */
objPo kany;			/* any value marker */
objPo kanyQ;                    /* any value constructor */

objPo ktplTp, kanyTp, kfunTp, kprocTp, klstTp,kallQ;
objPo knumberTp, ksymbolTp, kcharTp, khandleTp, kstringTp, klogicalTp, kqueryTp, kopaqueTp;

objPo msgTp;                    // Standard message type
objPo monitorTp;                // Standard monitor type
objPo debugTp;		// Standard debugging message type

objPo emptyList;		/* the empty list */
objPo ktpl;                     /* The empty tuple symbol */
objPo emptySymbol;              /* The empty symbol */
objPo emptyTuple;               /* The empty tuple */

objPo katts;			/* Attribute set marker */
objPo kattTp;			/* Attribute set type */

objPo kendfile;			/* end of file marker */

objPo kinterrupt;		/* control-C interrupt */
objPo ktimedout;		/* timedout event */
objPo ktrue,kfalse;
objPo kerror;			/* used for error handling */
objPo kfailed;			/* Something failed this way comes */
objPo kclicked;			/* Ran out of steam */
objPo kblock;			/* Record and theta marker */
objPo kdate;			/* date record marker */
objPo kfstat;			/* File status marker */

objPo krawencoding;             /* Raw encoding tag */
objPo kutf16encoding;           /* UTF16 encoding tag */
objPo kutf8encoding;            /* UTF8 encoding tag */
objPo kunknownencoding;         /* unknown encoding tag */

objPo kleasetime;		/* lease time on a message */
objPo kreplyto;			/* who to reply to */
objPo kreceiptrequest;		/* receipt requested */
objPo kaudittrail;		/* audit trail requested */

objPo kbreakdebug;

/* Standard error codes */
objPo ematcherr,eprivileged,esystem,eexec,ecompval,einval,earith,eabort;
objPo efail,eeof,eio,enet;


/* Permission symbols */
objPo ksetuid,ksetgid,ksticky,krusr,kwusr,kxusr,krgrp,kwgrp,kxgrp;
objPo kroth,kwoth,kxoth;

/* parsing symbols */
objPo klparen,krparen,klbracket,krbracket,klbrace,krbrace,kcons,kcomma,ktick,keof;

/* Standard dictionaries ... */
static hashPo dictionary;	/* The main symbol table */

/* Locate a symbol in the dictionary */
objPo newUniSymbol(const uniChar *name)
{
  objPo sym = Search((void*)name,dictionary);

  if(sym==NULL){		/* A new entry in the table */
    sym = allocateSymbol((uniChar*)name);

    if(sym==NULL)
      syserr("no space for symbol");	/* should never happen */
    else
      Install(SymVal(sym),sym,dictionary); /* Install in dictionary */
  }
  return sym;			/* return the symbol */
}

objPo newSymbol(const char *name)
{
  WORD32 len = strlen(name)+1;
  uniChar buffer[MAX_SYMB_LEN];
  uniChar *nm = (len<NumberOf(buffer)?buffer:((uniChar*)malloc(sizeof(uniChar)*(len+1))));
  objPo symb;
  
  _uni((const unsigned char *)name,nm,len);

  symb = newUniSymbol(nm);

  if(nm!=buffer)
    free(nm);
  
  return symb;
}

void installSymbol(objPo s)
{
  objPo sym = Search(SymVal(s),dictionary);

  if(sym==NULL){		/* A new entry in the table */
    Install(SymVal(s),s,dictionary); /* Install in dictionary */
  }
}

void init_dict()		/* Initialize the dictionary */
{
  dictionary = NewHash(512,NULL,(compFun)uniCmp, NULL);

  /* Declare run-time symbols */

  /* result keywords */
  kvoid=newSymbol("void");

  emptyList=allocatePair(&kvoid,&kvoid);
  emptyTuple = allocateTuple(0);
  emptySymbol = newSymbol("");

  kany=newSymbol("any");
  kanyQ = newSymbol("??");
  
  kanyTp = newSymbol("#any");
  kfunTp = newSymbol("#=>");
  kprocTp = newSymbol("#{}");
  ktplTp = newSymbol("#()");
  klstTp = newSymbol("#list");
  kallQ = newSymbol("#-");
  knumberTp = newSymbol("#number");
  ksymbolTp = newSymbol("#symbol");
  khandleTp = newSymbol("#handle");
  kcharTp = newSymbol("#char");
  kattTp = newSymbol("#{..}");
  
  kstringTp = allocateConstructor(1);
  
  updateConsFn(kstringTp,klstTp);
  updateConsEl(kstringTp,0,kcharTp);
  
  klogicalTp = newSymbol("#logical");
  kopaqueTp = newSymbol("#opaque");
  kqueryTp = newSymbol("#?");
  
  ktpl = newSymbol("()");
  katts = newSymbol("{..}");

  kendfile = newSymbol("EOF");

  ktrue = newSymbol("true");
  kfalse = newSymbol("false");
  kerror = newSymbol("error");
  kfailed = newSymbol("failed");
  kclicked = newSymbol("clickedout");
  kblock = newSymbol("{}");
  kdate = newSymbol("date");
  khandle = newSymbol("hdl");
  knullhandle = newSymbol("nullhandle");
  kprocess = newSymbol("process");
  kfstat = newSymbol("_file_stat");
  ktermin = newSymbol("termin");
  
  krawencoding = newSymbol("rawEncoding");             /* Raw encoding tag */
  kutf16encoding = newSymbol("utf16Encoding");         /* UTF16 encoding tag */
  kutf8encoding = newSymbol("utf8Encoding");             /* UTF8 encoding tag */
  kunknownencoding = newSymbol("unknownEncoding");         /* unknown encoding tag */

  kleasetime = newSymbol("leaseTime"); /* lease time on a message */
  kreplyto = newSymbol("replyTo"); /* who to reply to */
  kreceiptrequest = newSymbol("receiptRequest"); /* receipt requested */
  kaudittrail = newSymbol("auditTrail"); /* audit trail requested */

  /* resource manager keywords */
  kinterrupt = newSymbol("interrupt"); /* sent during on a ^C */
  ktimedout = newSymbol("timedout"); /* sent on a time out */

  /* file permissions */
  ksetuid = newSymbol("_setuid");
  ksetgid = newSymbol("_setgid");
  ksticky = newSymbol("_sticky");
  krusr = newSymbol("_rusr");
  kwusr = newSymbol("_wusr");
  kxusr = newSymbol("_xusr");
  krgrp = newSymbol("_rgrp");
  kwgrp = newSymbol("_wgrp");
  kxgrp = newSymbol("_xgrp");
  kroth = newSymbol("_roth");
  kwoth = newSymbol("_woth");
  kxoth = newSymbol("_xoth");

  klparen = newSymbol("(");
  krparen = newSymbol(")");
  klbracket = newSymbol("[");
  krbracket = newSymbol("]");
  klbrace = newSymbol("{");
  krbrace = newSymbol("}");
  kcons = newSymbol(",..");
  kcomma = newSymbol(",");
  ktick = newSymbol("'");
  keof = newSymbol("<EOF>");

  /* debugger keywords */
  kbreakdebug = newSymbol("break_debug");
  
  debugTp = newSymbol(DEBUG_TYPE);
  monitorTp = newSymbol(MONITOR_TYPE);
  
  {
    objPo elTp = allocateConstructor(1);
    
    msgTp = allocateConstructor(4);               // (handle,handle,msgAttr[],any)
    
    updateConsFn(elTp,klstTp);
    updateConsEl(elTp,0,newSymbol(MSG_ATTR_TYPE));
    
    updateConsFn(msgTp,ktplTp);
    updateConsEl(msgTp,0,khandleTp);
    updateConsEl(msgTp,1,khandleTp);
    updateConsEl(msgTp,2,elTp);
    updateConsEl(msgTp,3,kanyTp);
  }

  ematcherr = newSymbol("'matcherr");
  eprivileged = newSymbol("privileged");
  esystem = newSymbol("system");
  eexec = newSymbol("exec");
  ecompval = newSymbol("compval");
  einval = newSymbol("invalid");
  earith = newSymbol("arith");
  eabort = newSymbol("abort");
  efail = newSymbol("fail");
  eeof = newSymbol("eof");
  eio = newSymbol("io");
  enet = newSymbol("net");
}

typedef struct {
  objPo base;
  objPo limit;
} scanInfo;

/* remove all `new' entries from the dictionary */

static retCode remSym(void *n,void *r,void *c)
{
  scanInfo *info = (scanInfo*)c;
  objPo o = (objPo)r;

  if(!(o>=info->base && o<info->limit))
    Uninstall(n,dictionary);
  return Ok;
}

void scanDict(objPo base,objPo limit)
{
  scanInfo info = {base,limit};

  ProcessTable(remSym,dictionary,&info); /* clear down the dictionary */

  khandle = scanCell(khandle);	/* handle record label */
  knullhandle = scanCell(knullhandle);	/* null handle */
  kprocess = scanCell(kprocess); /* handle process label */
  ktermin = scanCell(ktermin);	/* terminate process marker */
  kvoid = scanCell(kvoid);	/* void value marker */
  emptyList = scanCell(emptyList);
  emptyTuple = scanCell(emptyTuple);
  emptySymbol = scanCell(emptySymbol);
  kany = scanCell(kany);	/* any value marker */
  kanyQ = scanCell(kanyQ);	/* any value marker */
  
  kanyTp = scanCell(kanyTp);
  kfunTp = scanCell(kfunTp);
  kprocTp = scanCell(kprocTp);
  ktplTp = scanCell(ktplTp);
  klstTp = scanCell(klstTp);
  kattTp = scanCell(kattTp);
  kallQ = scanCell(kallQ);
  knumberTp = scanCell(knumberTp);
  ksymbolTp = scanCell(ksymbolTp);
  khandleTp = scanCell(khandleTp);
  kcharTp = scanCell(kcharTp);
  kstringTp = scanCell(kstringTp);
  klogicalTp = scanCell(klogicalTp);
  kqueryTp = scanCell(kqueryTp);
  kopaqueTp = scanCell(kopaqueTp);
  
  ktpl = scanCell(ktpl);
  katts = scanCell(katts);

  kinterrupt = scanCell(kinterrupt); /* control-C interrupt */
  ktimedout = scanCell(ktimedout); /* timedout event */
  ktrue = scanCell(ktrue);
  kfalse = scanCell(kfalse);
  kerror = scanCell(kerror);	/* used for error handling */
  kfailed = scanCell(kfailed);	/* Something failed this way comes */
  kclicked = scanCell(kclicked); /* Ran out of time */
  kblock = scanCell(kblock);	/* Record and theta marker */
  kdate = scanCell(kdate);	/* date record marker */
  kfstat = scanCell(kfstat);	/* file status marker */
  kendfile = scanCell(kendfile);
  
  krawencoding = scanCell(krawencoding);             /* Raw encoding tag */
  kutf16encoding = scanCell(kutf16encoding);         /* UTF16 encoding tag */
  kutf8encoding = scanCell(kutf8encoding);             /* UTF8 encoding tag */
  kunknownencoding = scanCell(kunknownencoding);         /* unknown encoding tag */

  kleasetime = scanCell(kleasetime); /* lease time on a message */
  kreplyto = scanCell(kreplyto); /* who to reply to */
  kreceiptrequest = scanCell(kreceiptrequest); /* receipt requested */
  kaudittrail = scanCell(kaudittrail); /* audit trail requested */

  debugTp = scanCell(debugTp);
  msgTp = scanCell(msgTp);
  monitorTp = scanCell(monitorTp);
  kbreakdebug = scanCell(kbreakdebug);
  ematcherr = scanCell(ematcherr);
  eprivileged = scanCell(eprivileged);
  esystem = scanCell(esystem);
  eexec = scanCell(eexec);
  ecompval = scanCell(ecompval);
  einval = scanCell(einval);
  earith = scanCell(earith);
  eabort = scanCell(eabort);
  efail = scanCell(efail);
  eeof = scanCell(eeof);
  eio = scanCell(eio);
  enet = scanCell(enet);


  ksetuid = scanCell(ksetuid);
  ksetgid = scanCell(ksetgid);
  ksticky = scanCell(ksticky);
  krusr = scanCell(krusr);
  kwusr = scanCell(kwusr);
  kxusr = scanCell(kxusr);
  krgrp = scanCell(krgrp);
  kwgrp = scanCell(kwgrp);
  kxgrp = scanCell(kxgrp);
  kroth = scanCell(kroth);
  kwoth = scanCell(kwoth);
  kxoth = scanCell(kxoth);
  klparen = scanCell(klparen);
  krparen = scanCell(krparen);
  klbracket = scanCell(klbracket);
  krbracket = scanCell(krbracket);
  klbrace = scanCell(klbrace);
  krbrace = scanCell(krbrace);
  kcons = scanCell(kcons);
  kcomma = scanCell(kcomma);
  ktick = scanCell(ktick);
  keof = scanCell(keof);
}

void markDict(void)
{
  DelHash(dictionary);		/* clear down the existing dictionary */
  dictionary = NewHash(256,NULL,(compFun)uniCmp,NULL);

  markCell(khandle);		/* handle record label */
  markCell(knullhandle);	/* null handle */
  markCell(kprocess);		/* handle process label */
  markCell(ktermin);		/* terminate process marker */
  markCell(kvoid);		/* void value marker */
  markCell(emptyList);
  markCell(emptyTuple);
  markCell(emptySymbol);
  markCell(kany);		/* any value marker */
  markCell(kanyQ);
  
  markCell(kanyTp);
  markCell(kfunTp);
  markCell(kprocTp);
  markCell(ktplTp);
  markCell(kattTp);
  markCell(klstTp);
  markCell(kallQ);
  markCell(knumberTp);
  markCell(ksymbolTp);
  markCell(khandleTp);
  markCell(kcharTp);
  markCell(kstringTp);
  markCell(klogicalTp);
  markCell(kqueryTp);
  markCell(kopaqueTp);
  
  markCell(ktpl);
  markCell(katts);

  markCell(kinterrupt);		/* control-C interrupt */
  markCell(ktimedout);		/* timedout event */
  markCell(ktrue);
  markCell(kfalse);
  markCell(kerror);		/* used for error handling */
  markCell(kfailed);		/* Something failed this way comes */
  markCell(kclicked);		/* Ran out of time */
  markCell(kblock);		/* Record and theta marker */
  markCell(kdate);		/* date record marker */
  markCell(kfstat);		/* File status marker */
  markCell(kendfile);
  
  markCell(krawencoding);       /* Raw encoding tag */
  markCell(kutf16encoding);     /* UTF16 encoding tag */
  markCell(kutf8encoding);      /* UTF8 encoding tag */
  markCell(kunknownencoding);   /* unknown encoding tag */

  markCell(kleasetime);		/* lease time on a message */
  markCell(kreplyto);		/* who to reply to */
  markCell(kreceiptrequest);	/* receipt requested */
  markCell(kaudittrail);	/* audit trail requested */

  markCell(debugTp);
  markCell(msgTp);
  markCell(monitorTp);
  markCell(kbreakdebug);
  markCell(ematcherr);
  markCell(eprivileged);
  markCell(esystem);
  markCell(eexec);
  markCell(ecompval);
  markCell(einval);
  markCell(earith);
  markCell(eabort);
  markCell(efail);
  markCell(eeof);
  markCell(eio);
  markCell(enet);

  markCell(ksetuid);
  markCell(ksetgid);
  markCell(ksticky);
  markCell(krusr);
  markCell(kwusr);
  markCell(kxusr);
  markCell(krgrp);
  markCell(kwgrp);
  markCell(kxgrp);
  markCell(kroth);
  markCell(kwoth);
  markCell(kxoth);
  markCell(klparen);
  markCell(krparen);
  markCell(klbracket);
  markCell(krbracket);
  markCell(klbrace);
  markCell(krbrace);
  markCell(kcons);
  markCell(kcomma);
  markCell(ktick);
  markCell(keof);
}

void adjustDict(void)
{
  khandle = adjustCell(khandle); /* handle record label */
  knullhandle = adjustCell(knullhandle); /* null handle */
  kprocess = adjustCell(kprocess); /* handle process label */
  ktermin = adjustCell(ktermin); /* terminate process marker */
  kvoid = adjustCell(kvoid);	/* void value marker */
  emptyList = adjustCell(emptyList);
  emptyTuple = adjustCell(emptyTuple);
  emptySymbol = adjustCell(emptySymbol);
  kany = adjustCell(kany);	/* any value marker */
  kanyQ = adjustCell(kanyQ);
  
  kfunTp = adjustCell(kfunTp);
  kanyTp = adjustCell(kanyTp);
  kprocTp = adjustCell(kprocTp);
  ktplTp = adjustCell(ktplTp);
  kattTp = adjustCell(kattTp);
  klstTp = adjustCell(klstTp);
  kallQ = adjustCell(kallQ);
  knumberTp = adjustCell(knumberTp);
  ksymbolTp = adjustCell(ksymbolTp);
  khandleTp = adjustCell(khandleTp);
  kcharTp= adjustCell(kcharTp);
  kstringTp = adjustCell(kstringTp);
  klogicalTp = adjustCell(klogicalTp);
  kqueryTp = adjustCell(kqueryTp);
  kopaqueTp = adjustCell(kopaqueTp);
  
  ktpl = adjustCell(ktpl);
  katts = adjustCell(katts);

  kinterrupt = adjustCell(kinterrupt); /* control-C interrupt */
  ktimedout = adjustCell(ktimedout); /* timedout event */
  ktrue = adjustCell(ktrue);
  kfalse = adjustCell(kfalse);
  kerror = adjustCell(kerror);	/* used for error handling */
  kfailed = adjustCell(kfailed); /* Something failed this way comes */
  kclicked = adjustCell(kclicked); /* Ran out of time */
  kblock = adjustCell(kblock);	/* Record and theta marker */
  kdate = adjustCell(kdate);	/* date record marker */
  kfstat = adjustCell(kfstat);	/* File status marker */
  kendfile = adjustCell(kendfile);
  
  krawencoding = adjustCell(krawencoding);             /* Raw encoding tag */
  kutf16encoding = adjustCell(kutf16encoding);         /* UTF16 encoding tag */
  kutf8encoding = adjustCell(kutf8encoding);             /* UTF8 encoding tag */
  kunknownencoding = adjustCell(kunknownencoding);         /* unknown encoding tag */

  kleasetime = adjustCell(kleasetime); /* lease time on a message */
  kreplyto = adjustCell(kreplyto); /* who to reply to */
  kreceiptrequest = adjustCell(kreceiptrequest); /* receipt requested */
  kaudittrail = adjustCell(kaudittrail); /* audit trail requested */

  kbreakdebug = adjustCell(kbreakdebug);
  debugTp = adjustCell(debugTp);
  msgTp = adjustCell(msgTp);
  monitorTp = adjustCell(monitorTp);
  ematcherr = adjustCell(ematcherr);
  eprivileged = adjustCell(eprivileged);
  esystem = adjustCell(esystem);
  eexec = adjustCell(eexec);
  ecompval = adjustCell(ecompval);
  einval = adjustCell(einval);
  earith = adjustCell(earith);
  eabort = adjustCell(eabort);
  efail = adjustCell(efail);
  eeof = adjustCell(eeof);
  eio = adjustCell(eio);
  enet = adjustCell(enet);

  ksetuid = adjustCell(ksetuid);
  ksetgid = adjustCell(ksetgid);
  ksticky = adjustCell(ksticky);
  krusr = adjustCell(krusr);
  kwusr = adjustCell(kwusr);
  kxusr = adjustCell(kxusr);
  krgrp = adjustCell(krgrp);
  kwgrp = adjustCell(kwgrp);
  kxgrp = adjustCell(kxgrp);
  kroth = adjustCell(kroth);
  kwoth = adjustCell(kwoth);
  kxoth = adjustCell(kxoth);
  klparen = adjustCell(klparen);
  krparen = adjustCell(krparen);
  klbracket = adjustCell(klbracket);
  krbracket = adjustCell(krbracket);
  klbrace = adjustCell(klbrace);
  krbrace = adjustCell(krbrace);
  kcons = adjustCell(kcons);
  kcomma = adjustCell(kcomma);
  ktick = adjustCell(ktick);
  keof = adjustCell(keof);
}


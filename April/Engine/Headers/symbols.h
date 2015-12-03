/*
 * Standard symbols used in the April run-time system
 * (c) 1994-2002 Imperial College & F.G. McCabe
 * All rights reserved
 */

#ifndef _SYMBOLS_H_
#define _SYMBOLS_H_

extern objPo kvoid,kany,kanyQ;
extern objPo kanyTp,kfunTp,kprocTp,ktplTp,klstTp,kallQ;
extern objPo knumberTp, ksymbolTp, kcharTp, khandleTp, kstringTp ,klogicalTp,kqueryTp,kopaqueTp;
extern objPo msgTp, monitorTp, debugTp;

extern objPo kerror,kinterrupt,ktimedout,kfailed,kclicked,kblock;
extern objPo emptyList,emptySymbol,emptyTuple,ktpl;
extern objPo kleasetime,kreplyto,kreceiptrequest,kaudittrail;

extern objPo ktclosure;

extern objPo khandle,knullhandle; /* needed to implement the handle type */
extern objPo kprocess;		/* needed to implement the process type */

extern objPo ktrue,kfalse;
extern objPo kdate;
extern objPo kfstat;
extern objPo ktermin;

extern objPo krawencoding;             /* Raw encoding tag */
extern objPo kutf16encoding;           /* UTF16 encoding tag */
extern objPo kutf8encoding;            /* UTF8 encoding tag */
extern objPo kunknownencoding;         /* unknown encoding tag */

extern objPo ksetuid,ksetgid,ksticky,krusr,kwusr,kxusr,krgrp,kwgrp,kxgrp;
extern objPo kroth,kwoth,kxoth;
extern objPo klparen,krparen,klbracket,krbracket,klbrace,krbrace,kcons,kcomma,ktick,keof;

extern objPo ematcherr,eprivileged,esystem,eexec,ecompval,einval,earith,eabort;
extern objPo efail,eeof,eio,enet;

extern objPo kbreakdebug;

extern uniChar *aprilSysPath;	/* source for loading system files */

#endif


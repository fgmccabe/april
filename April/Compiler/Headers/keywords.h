/*
 *  Public declarations for the dictionary entries of the keywords
 * (c) 1994-1997 Imperial College & F.G. McCabe
 */
#ifndef _KEYWORDS_H_
#define _KEYWORDS_H_

/* syntax keywords */
extern symbpo kmacreplace,kfn,kstmt,kforall,kconstr;
extern symbpo ktry,kcatch,kraise,kfor,kwhile,kdo,kcase,kdots,kdot;
extern symbpo kplus,kiplus,kminus,kiminus,kslash,kislash,kpcent,krem,kstar,kistar,kshriek;
extern symbpo ktcoerce,kquery,kuscore,ktvar,kcontain,kcoerce;
extern symbpo kunion,kappend,kintersect,kdiff,kselect,kreject;
extern symbpo kindex,kcardinality;
extern symbpo kmap,kreduce,kfront,kback,khead,ktail,krand,ksort,kpi;
extern symbpo kwithin;
extern symbpo kmeta;
extern symbpo kstate,kdead,kquiescent,krunnable,kwaitio,kwaitmsg,kwaittimer;
extern symbpo kload;
extern symbpo kif,kthen,kelse,klabel,kfield,kdefn;
extern symbpo khat;

extern cellpo cmeta,ctrue,cfalse;
extern cellpo cquery,cdefn,cuscore,emptylist;

/* time expression keywords */
extern symbpo kcolon,ktimeout,know,ktoday;

/* process expressions */
extern symbpo kvalueof,kvalis,kresult,kof,kcollect,ksetof,kelement;
extern symbpo krelax,kbreak,kleave,khalt,kdie,kself,kcreator;
extern symbpo kblock,kempty,kbkets,kcons,ktype,ktypestring,ktpname,ktypeof,kenum,ktof,ktstring;
extern cellpo cblock,cempty,cbkets,nilcell;

/* compiler related keywords */
extern symbpo kquote,kquoted,kanti;
extern cellpo cquote;

/* Special keywords for handling closures */
extern symbpo ktoplevel;
extern cellpo cequation,cclause,ctoplevel;

/* process keywords */
extern symbpo ksemi,kcomma,kchoice,kguard,kassign;
extern cellpo csemi,ccomma;

/* Special escapes which may be folded */
extern symbpo kcatenate,kchrcons,kstrlen,kascii,kcharof,kmergeURL;
extern symbpo kband,kbor,kbxor,kbnot,kbleft,kbright;

/* message handling keywords */
extern symbpo kreplyto,ksender,kfrom,ksend;
extern cellpo creplyto,csender,csend;

/* Syntax keywords */
extern symbpo kenvironment;
extern cellpo cenvironment;

/* predicates */
extern symbpo kequal,knotequal,klessthan,kgreaterthan,klesseq,kgreatereq;
extern symbpo ksame,knotsame,kmatch,knomatch,kin;
extern symbpo kand,kdand,kor,kdor,knot,kdnot;
extern cellpo cnot,cor,cand;

/* Macro keywords */
extern symbpo kmacro,khash,kdhash,ktilda,kat,kinclude,kfile;
extern symbpo kdebug,knodebug,kdebugdisplay,kop,kecho,kerror,kwarning,klist2tuple,ktuple2list,ktuple,klist;
extern cellpo cdebugdisplay;
extern cellpo cerror,cmatcherr,cfail;
extern cellpo voidcell;		/* contains the symvol "void" */

extern symbpo ktclosure;
extern cellpo cclosure, ctclosure;

extern symbpo knull,kvoid,kline,kentry,d_exit;
extern cellpo cnull;
extern symbpo d_return,d_assign,d_send,d_accept,d_fork,d_die;

extern symbpo kcall,ktrue,kfalse,kfailed;
extern cellpo cfailed;

/* Type keywords */
extern cellpo cnullhandle;
extern symbpo kinteger,knumber,ksymbol,kchar,kident,kstring,khdl,knullhandle,kany,khandle,klogical,kopaque;
extern cellpo ctpl;
extern cellpo numberTp,symbolTp,charTp,stringTp,handleTp,anyTp,logicalTp,allQ,funTp,procTp,tplTp,listTp,opaqueTp;
extern symbpo knumberTp,ksymbolTp,kcharTp,khandleTp,kanyTp,klogicalTp,kallQ,kfunTp,kprocTp,ktplTp,klistTp,kopaqueTp;

// Debugging escapes
extern symbpo kwaitdebug;

/* Some special compiler entries */
extern cellpo c_zero,c_one,emptystring;
extern cellpo ztpl;		/* The zero-tuple */

void init_dict(void);		/* initialize the dictionary */
symbpo locateC(const char *name); /* locateC a symbol within the dict */
symbpo locateU(const uniChar *name); /* locateC a symbol within the dict */

extern char *symN(uniChar *name);
#endif


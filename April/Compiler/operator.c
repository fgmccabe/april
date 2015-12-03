/*
 * Handle operator declarations for April grammar 
 * (c) 1994-1998 Imperial College and F.G.McCabe
 * All rights reserved
*/

#include "config.h"
#include <string.h>		/* Access string defs */
#include "compile.h"
#include "operators.h"
#include "dict.h"
#include "keywords.h"
#include "hash.h"

static poolPo oppool = NULL; /* pool of operator declarations */
static hashPo optble = NULL;	/* operator hash table */

static void purgeOps(void);
static void setinfix(symbpo name, int lprec, int prec, int rprec,int line);
static void setprefix(symbpo name, int prec, int rprec,int line);
static void setpostfix(symbpo name, int lprec, int prec,int line);

static void setnprefix(symbpo name, int prec);
static void setaprefix(symbpo name, int prec);

static void setlinfix(symbpo name, int prec);
static void setninfix(symbpo name, int prec);
static void setrinfix(symbpo name, int prec);

static void setnpostfix(symbpo name, int prec);
void setapostfix(symbpo name, int prec);

void init_op(void)
{
  if(oppool==NULL){
    oppool=newPool(sizeof(oprecord),MAX_OP);

    optble=NewHash(MAX_OP,(hashFun)uniHash,(compFun)uniCmp,NULL);
  }
  else
    purgeOps();

  /* Declare standard operators */

  /* Core syntax operators */
  setrinfix(kfn,1200);		/* Lambda operator */
  setrinfix(kstmt,1200);	/* Mu operator */
  setninfix(ktype,1460);	/* Slightly more than | */
  setninfix(ktpname,999);	/* only used internally */
  setrinfix(kchoice,1250);
  setnprefix(kchoice,1449);
  setnpostfix(kchoice,1448);	/* this allows the old |...| construction */

  setrinfix(kcomma,1000);	/* comma is a rather special operator */
  setrinfix(ksemi,1900);	/* semi-colon is only an operator in {}'s  */
  setnpostfix(ksemi,1900);	/* it is also postfix */

  setninfix(kassign,1100);	/* assignment is not associative! */

  setninfix(kfield,999);	/* multiple assignment definition */
  setninfix(kdefn,999);		/* single assignment definition */
  setlinfix(kdot,400);		/* declare dot as an operator */

  setrinfix(kdo,1300);		/* quite an important operator */
  setnprefix(kfor,1250);
  setnprefix(kwhile,1250);

  setrinfix(kelse,1195);	/* set up if-then-else operators */
  setninfix(kthen,1190);
  setnprefix(kif,1150);

  setnprefix(kcase,1190);       // The case .. in .. operator

  setninfix(kcatch,1299);	/* error handling */
  setnprefix(kraise,900);
  setnprefix(ktry,1300);

  setninfix(klabel,1140);	/* labelled statement/guard in a pattern */
  setnprefix(kleave,310);

  setnprefix(kvalueof,500);
  setnprefix(kvalis,1100);

  setnprefix(kcollect,500);
  setnprefix(kelement,1100);
  setnprefix(ksetof,500);

  setninfix(ktcoerce,510);	/* %% is not associative */

  setninfix(kquery,750);	/* ? is not associative */
  setnprefix(kquery,310);
  setnprefix(ktvar,1);		/* A tightly bound operator introducing a type var */
  setninfix(kguard,940);	/* Pattern guard */
  setnprefix(kshriek,910);	/* escape in a pattern, negation */

  setninfix(ksend,1100);	/* message send operator */
  setninfix(kfrom,998);		/* modify the message send */

  /* operators to support macros */

  setnprefix(kmacro,1450);
  setninfix(kmacreplace,1400);

  setnprefix(khash,1500);
  setninfix(khash,650);		/* used in list indexing and relative macros */
  setnprefix(kdhash,300);
  setrinfix(kdhash,300);
  setninfix(kat,300);
  setninfix(ktilda,795);
  setninfix(khat,790);		/* string precision pattern */
  setnprefix(kecho,1499);
  setrinfix(kcontain,860);

  setnprefix(kinclude,1450);
  setnprefix(knodebug,1450);
  setnprefix(kdebug,1450);

  /* Standard function operators */

  setlinfix(kplus,720);		/* left-associative */
  setlinfix(kiplus,720);	/* left-associative */
  setaprefix(kplus,300);	/* unary plus */

  setlinfix(kminus,720);	/* left-associative */
  setlinfix(kiminus,720);	/* left-associative */
  setaprefix(kminus,300);	/* unary minus */
  setlinfix(kstar,700);		/* left-associative */
  setlinfix(kistar,700);	/* left-associative */
  setnpostfix(kstar,700);	/* string closure */

  setlinfix(kslash,700);	/* left-associative */
  setlinfix(kislash,700);	/* left-associative */
  setlinfix(kpcent,700);	/* left-associative */
  setlinfix(krem,700);		/* left-associative */

  setlinfix(kunion,820);	/* Set union */
  setlinfix(kintersect,800);	/* Set intersection */
  setrinfix(kappend,800);	/* list append */
  setlinfix(kdiff,820);		/* set difference function */
  setlinfix(kselect,840);
  setrinfix(kcatenate,800);	/* string append */
  setrinfix(kchrcons,750);	/* char prepend */

  setlinfix(kreject,840);
  setlinfix(kmap,840);
  setlinfix(kreduce,840);

  setninfix(kmatch,900);	/* predicates are all at 900 */
  setninfix(knomatch,900);
  setninfix(kequal,900);
  setninfix(knotequal,900);
  setninfix(klessthan,900);
  setnprefix(klessthan,748);
  setnpostfix(kgreaterthan,749);
  setninfix(kgreaterthan,900);
  setninfix(klesseq,900);
  setninfix(kgreatereq,900);
  setninfix(ksame,900);
  setninfix(knotsame,900);

  setninfix(kin,900);
  setninfix(kdots,820);

  setrinfix(kdand,920);
  setrinfix(kand,920);

  setrinfix(kdor,930);
  setrinfix(kor,930);
  
  setaprefix(knot,910);
  setaprefix(kdnot,910);
}


static oppo OpEntry(symbpo l);	/* return operator record */

static oppo alloc_op(symbpo name)
{
  oppo o=(oppo)allocPool(oppool); /* grab an operator entry */

  if(o==NULL)
    return NULL;		/* should never happen */

  o->p[prefixop].prec=-1;	/* initialize prefix part*/
  o->p[prefixop].rprec=-1;
  o->p[infixop].lprec=-1;	/* initialize infix part*/
  o->p[infixop].prec=-1;
  o->p[infixop].rprec=-1;
  o->p[postfixop].lprec=-1;	/* initialize postfix part*/
  o->p[postfixop].prec=-1;
  o->sym=name;
  return o;
}

static void setprefix(symbpo name, int prec, int rprec,int line)
{
  oppo c_op = OpEntry(name);

  if(c_op==NULL){
    c_op = alloc_op(name);
    Install((void*)name,c_op,optble);
  }

  if(c_op->p[prefixop].prec!=-1&& c_op->p[prefixop].prec!=prec)
    reportWarning(line,"redefinition of prefix operator: %U\n",name);

  c_op->p[prefixop].prec=prec;
  c_op->p[prefixop].rprec=rprec;
}

/* convenience functions */
static void setnprefix(symbpo name, int prec) /* non-associative prefix */
{
  setprefix(name,prec,prec-1,-1);
}

static void setaprefix(symbpo name, int prec) /* associative prefix */
{
  setprefix(name,prec,prec,-1);
}

static void setinfix(symbpo name, int lprec, int prec, int rprec,int line)
{
  oppo c_op = OpEntry(name);

  if(c_op==NULL){
    c_op = alloc_op(name);
    Install(name,c_op,optble);
  }

  if(c_op->p[infixop].prec!=-1 && c_op->p[infixop].prec!=prec)
    reportWarning(line,"redefinition of infix operator: %U\n",name);

  c_op->p[infixop].lprec=lprec;
  c_op->p[infixop].prec=prec;
  c_op->p[infixop].rprec=rprec;
}

static void setlinfix(symbpo name, int prec) /* left associative */
{
  setinfix(name,prec,prec,prec-1,-1);
}

static void setninfix(symbpo name, int prec) /* non-associative */
{
  setinfix(name,prec-1,prec,prec-1,-1);
}

static void setrinfix(symbpo name, int prec) /* right associative */
{
  setinfix(name,prec-1,prec,prec,-1);
}

static void setpostfix(symbpo name, int lprec, int prec,int line)
{
  oppo c_op = OpEntry(name);

  if(c_op==NULL){
    c_op = alloc_op(name);
    Install(name,c_op,optble);
  }

  if(c_op->p[postfixop].prec!=-1 && c_op->p[postfixop].prec!=prec)
    reportWarning(line,"redefinition of postfix operator: %s\n",name);

  c_op->p[postfixop].prec=prec;
  c_op->p[postfixop].lprec=lprec;
}

static void setnpostfix(symbpo name, int prec) /* non-associative postfix */
{
  setpostfix(name,prec-1,prec,-1);
}

void setapostfix(symbpo name, int prec) /* associative postfix */
{
  setpostfix(name,prec,prec,-1);
}

int preprec(symbpo name, int *prec, int *rprec)
{
  oppo c_op = OpEntry(name);

  if(c_op==NULL)
    return -1;

  *prec = c_op->p[prefixop].prec;
  *rprec = c_op->p[prefixop].rprec;
  return *prec;
}

int infprec(symbpo name, int *lprec, int *prec, int *rprec)
{
  oppo c_op = OpEntry(name);

  if(c_op==NULL)
    return -1;

  *lprec = c_op->p[infixop].lprec;
  *prec = c_op->p[infixop].prec;
  *rprec = c_op->p[infixop].rprec;
  return *lprec;
}

int postprec(symbpo name, int *lprec, int *prec)
{
  oppo c_op = OpEntry(name);

  if(c_op==NULL)
    return -1;

  *prec = c_op->p[postfixop].prec;
  *lprec = c_op->p[postfixop].lprec;
  return *prec;
}

static oppo OpEntry(symbpo l)
{
  return (oppo)Search(l,optble);
}
  
logical IsOperator(symbpo name)
{
  return Search(name,optble)!=NULL; /* any operator declarations? */
}

logical new_op(uniChar *name, uniChar *type, int prec,int line)
{
  name = uniDuplicate(name);
  
  if(uniIsLit(type,"infix"))
    setinfix(name,prec-1,prec,prec-1,line);
  else if (uniIsLit(type,"left"))
    setinfix(name,prec,prec,prec-1,line);
  else if (uniIsLit(type,"right"))
    setinfix(name,prec-1,prec,prec,line);
  else if (uniIsLit(type,"prefix"))
    setprefix(name,prec,prec-1,line);
  else if (uniIsLit(type,"aprefix"))
    setprefix(name,prec,prec,line);
  else if (uniIsLit(type,"postfix"))
    setpostfix(name,prec-1,prec,line);
  else if (uniIsLit(type,"apostfix"))
    setpostfix(name,prec,prec,line);
  else
    return False;
  return True;
}

static retCode purgeOp(void *n,void *r,void *c)
{
  Uninstall(n,optble);
  freePool(oppool,r);
  return Ok;
}

static void purgeOps(void)
{
  ProcessTable(purgeOp,optble,NULL);
}




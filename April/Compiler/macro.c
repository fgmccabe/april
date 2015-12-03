/* 
   Macro Preprocessor for April system
   (c) 1994-2004 Imperial College and F.G. McCabe

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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "compile.h"
#include "dict.h"
#include "keywords.h"
#include "macro.h"
#include "hash.h"
#include "grammar.h"
#include "operators.h"
#include "url.h"

#define MAX_MACRO_DEF 2048	/* Increment size of macro ins pool */
#define MAX_MAC_CHOICE	128	/* Maximum depth of choice points */
#define MAX_MAC_DEPTH   128	/* Maximum depth of pattern in macro */

#define MACRO_DEPTH_WARNING 1900 /* When do we give a warning about macro expansions */

/* Macro keyword commands - used in compiled macros */
typedef enum{
  M_INT,			/* Match an integer */
    M_FLT,			/* match a floating point number */
    M_SYM,			/* Match a symbol */
    M_CHR,			/* Match a character */
    M_STR,			/* Match a string */
    M_TAG,			/* Define macro variable */
    M_REF,			/* Use macro variable */
    M_DEEP,			/* Use a deep macro variable */
    M_NIL,			/* Match empty list */
    M_LIST,			/* Match non-empty list */
    M_EOL,			/* Match list, test for end */
    M_TPL,			/* Match a fixed arity tuple */
    M_CONS,			/* Match a function call or procedure call */
    M_SKP,			/* Match anything */
    M_AINT,			/* Match any integer */
    M_AFLT,			/* Match any floating point number */
    M_ANUM,			/* Match any number */
    M_ACHR,                     // Match any character
    M_ASYM,			/* Match any symbol */
    M_ASTR,			/* Match any string */
    M_ALST,			/* Match any list */
    M_ATPL,			/* Match any tuple */
    M_STPL,			/* Match sub tuple */
    M_QUOT,                     // Match a quoted symbol
    M_NXL,			/* Pop after processing a tuple element */
    M_CNT,			/* Containment match - part 1 */
    M_SUB,			/* Containment match - part 2 */
    M_ALT,			/* Alternatives */
    M_CUT,			/* Prolog-style cut */
    M_TOP,			/* Retrieve top of call stack */
    M_RTN,			/* End the match */
    M_JMP,			/* Jump */
    M_FAIL,			/* Fail a match sequence */
    M_BRK,			/* Establish a break point for a macro */
    
    R_INIT,			/* Initiate replacement sequence */
    R_INT,			/* Replace with integer */
    R_FLT,			/* replace with floating point number */
    R_SYM,			/* Replace with symbol */
    R_CHR,			/* Replace with character */
    R_STR,			/* Replace with string */
    R_TAG,			/* Define macro variable */
    R_REF,			/* Use macro variable */
    R_DEEP,			/* Use a deep macro-variable */
    R_NIL,			/* Replace empty list */
    R_LIST,			/* Replace non-empty list */
    R_TPL,			/* Replace a fixed arity tuple */
    R_CONS,                     // Replace a constructor
    R_SKP,			/* Replace anything */
    R_CNT,			/* Containment replace - part 1 */
    R_TOP,			/* Pick up a old reference */
    R_NXT,			/* Retrieve top o call stack */
    R_MAC,			/* macro symbol generator */
    R_TMP,			/* write into a temporary space */
    R_CAT,			/* macro symbol concatenation */
    R_LIST2TUPLE,		/* convert a list to a tuple */
    R_TUPLE2LIST,		/* convert a tuple to a list */
    R_DEF,			/* define a macro during replacement */
    R_ECHO,			/* echo input to standard error */
    R_INCLUDE,			/* include a subsiduary file */
    R_FILE,			/* generate a file name */
    R_ERROR,			/* Raise a syntax error */
    R_WARN,			/* Raise a warning */
    R_DO,			/* process replacement again */
    R_STACK,			/* stack a new macro environment */
    M_NONE} mac_op;		/* Macro opcode */

typedef struct _macro_list_ {
  macdefpo top;			/* linked list of all the macros */
  hashPo table;		/* indexed table of macros */
} MacroList;

typedef struct _mac_def_ {
  mac_po pattern;		/* Matching pattern */
  int nvars;			/* Number of macro variables */
  int size;			/* Size of the replacement text */
  symbpo topsymb;		/* The top-level symbol of the macro */
  logical applic;		/* Is the top-level of the macro an application? */
  macdefpo prev;		/* Chain of macro definitions */
  cellpo head;			/* Source of line information of the macro */
} mac_def;

typedef struct _mac_list_ *macListPo;

typedef struct _mac_list_ {
  macdefpo macros;		/* List of macros at this level */
  macListPo previous;
} MacList;

typedef struct mac_ins{
  mac_op code;			/* macro matching code */
  union{
    cellpo vl;			/* optional matching value */
    int i;			/* Integer offset */
    struct {
      int depth;		/* Depth of a deep variable */
      int off;			/* offset in a deep variable */
    } d;
    mac_po alt;			/* Alternative macro instruction */
    macdefpo mac;		/* New macro top */
  }v;
  mac_po next;			/* next macro instruction */
} MacroInstruction;

typedef struct _mac_dict_ *macVpo;

typedef struct _mac_dict_ {	/* MAcro dictionary entry */
  symbpo name;			/* Name of this macro variable */
  int index;			/* Offset of this variable */
  macVpo prev;			/* Previous variable */
} macVarRec;

typedef struct _mac_env_ *macEnvPo;

typedef struct _mac_env_ {
  long nvars;			/* Number of variables */
  macVpo vars;			/* The variables at this level */
  macEnvPo inner;		/* Inner scope */
  mac_po pc;			/* Macro program counter */
} macEnvRec;

macdefpo mac_top = NULL;	/* Top of the macro table */

typedef struct{
  mac_po choicept;		/* Choice point in macro pattern */
  cellpo input;			/* Point at which the choice applies*/
  int cur_input;			/* Current 'call' stack */
  int top_input;		/* Current top of input stack */
  int remaining;		/* Remaining no. of els of tuple to search */
} mac_choice, *mac_chpo;

typedef struct contrec_{
  cellpo ptn;			/* containment pattern */
  int vno;			/* associated variable */
} *contrec;

static poolPo defpool = NULL; /* Pool of macro headers */
static poolPo macpool = NULL; /* pool of macro instructions */
static poolPo macvpool = NULL; /* Pool of macro variables */
static poolPo maclpool = NULL; /* pool of macro list structures */

contrec contained=NULL;		/* Contained patterns */
int n_contained;
int max_contained=0;		/* current maximum pattern stack */
int macro_num = 0;		/* counter used to construct macro symbols */
int macro_count = 0;		/* number of times macros have been applied */

extern long comperrflag;
logical traceMacro = False;	/* tracing of macro processor */
static int macroCounter = 0;

static void prt_macro(ioPo f, mac_po mac);
void print_mac(ioPo f, char* msg, macdefpo mc);
void prt_mac_seq(ioPo f, mac_po seq);
static macdefpo def_macro(cellpo ptn,cellpo rep,macdefpo mac_top,
			  macEnvPo inner);
static void copy(cellpo src,cellpo dest);

/* initialize the macro processor */
void init_macro()
{
  mac_top=NULL;			/* reset the macro definition table */
  macroCounter=0;		/* reset the macro counter */

  defpool = newPool(sizeof(mac_def),128);
  macpool = newPool(sizeof(MacroInstruction),MAX_MACRO_DEF);
  macvpool = newPool(sizeof(macVarRec),128);
  maclpool = newPool(sizeof(MacroList),16);
  macro_num = 0;
  macro_count = 0;
}

macPo macroList(void)
{
  macPo new = (macPo)allocPool(maclpool);

  new->top = NULL;
  new->table = NewHash(128,(hashFun)uniHash,(compFun)strcmp,NULL);

  return new;
}

typedef enum {null_arg,int_arg,cell_arg,mac_arg,def_arg} mac_arg_tp;

static mac_po emitI(char code,int val,macEnvPo macvars)
{
  mac_po m = (mac_po)allocPool(macpool);

  if(m!=NULL){
    m->code=code;
    m->next = NULL;		/* terminate the macro sequence */
    m->v.i = val;

    if(macvars->pc)
      macvars->pc->next=m;
    macvars->pc=m;
    return m;
  }
  else
    syserr("Macro definition too large");

  return NULL;
}

static mac_po emitN(char code,macEnvPo macvars)
{
  mac_po m = (mac_po)allocPool(macpool);

  if(m!=NULL){
    m->code=code;
    m->next = NULL;		/* terminate the macro sequence */

    if(macvars->pc)
      macvars->pc->next=m;
    macvars->pc=m;
    return m;
  }
  else
    syserr("Macro definition too large");

  return NULL;
}

static mac_po emitC(char code,cellpo val,macEnvPo macvars)
{
  mac_po m = (mac_po)allocPool(macpool);

  if(m!=NULL){
    m->code=code;
    m->next = NULL;		/* terminate the macro sequence */

    m->v.vl = dupCell(val);

    if(macvars->pc)
      macvars->pc->next=m;
    macvars->pc=m;
    return m;
  }
  else
    syserr("Macro definition too large");

  return NULL;
}

static mac_po emitM(char code,mac_po val,macEnvPo macvars)
{
  mac_po m = (mac_po)allocPool(macpool);

  if(m!=NULL){
    m->code=code;
    m->next = NULL;		/* terminate the macro sequence */
    m->v.alt = val;

    if(macvars->pc)
      macvars->pc->next=m;
    macvars->pc=m;
    return m;
  }
  else
    syserr("Macro definition too large");

  return NULL;
}

static mac_po emitD(char code, macdefpo val,macEnvPo macvars)
{
  mac_po m = (mac_po)allocPool(macpool);

  if(m!=NULL){
    m->code=code;
    m->next = NULL;		/* terminate the macro sequence */
    m->v.mac = val;

    if(macvars->pc)
      macvars->pc->next=m;
    macvars->pc=m;
    return m;
  }
  else
    syserr("Macro definition too large");

  return NULL;
}

/* Define a label in the macro stream */
static mac_po m_label()
{
  mac_po m = (mac_po)allocPool(macpool);

  if(m!=NULL)
    m->code=M_NONE;
  else
    syserr("Macro definition too large");

  return m;
}

static void m_merge(mac_po m,macEnvPo macvars)
{
  macvars->pc->next = m;		/* merge stream */
  macvars->pc = m;
}

static void free_macro(mac_po list)
{
  while(list!=NULL){
    mac_po n = list;

    list = list->next;
    freePool(macpool,n);
  }
}

/* Macro compiler --
   Compiles a macro definition into a list of matching commands

   Makes it faster to check whether a macro pattern matches input.
*/

static int add_macro_var(symbpo vname,macEnvPo macvars)
{
  macVpo nv = (macVpo)allocPool(macvpool);

  nv->name = vname;
  nv->prev = macvars->vars;
  nv->index = macvars->nvars++;
  macvars->vars = nv;

  return nv->index;
}

/* Look up a macro variable name */
static int mac_var(symbpo vname,macEnvPo macvars) 
{
  macVpo vars = macvars->vars;

  while(vars!=NULL)
    if(vname==vars->name)
      return vars->index;
    else
      vars = vars->prev;

  return add_macro_var(vname,macvars);	/* add a new macro variable */
}

/* find a macro variable */
static int get_mac_var(symbpo vname,macEnvPo macvars,int *depth) 
{
  macVpo vars = macvars->vars;

  *depth = 0;

  while(vars!=NULL)
    if(vname==vars->name)
      return vars->index;
    else
      vars = vars->prev;

  if(macvars->inner!=NULL){
    int ref = get_mac_var(vname,macvars->inner,depth);

    (*depth)++;
    return ref;
  }

  return -1;
}

#ifdef MACROTRACE

/* Look up a macro variable by index */
static symbpo mac_var_name(int index,macEnvPo macvars) 
{
  macVpo vars = macvars->vars;

  while(vars!=NULL)
    if(index==vars->index){
      if(vars->name==NULL)
	return locateC("<NULL>");
      else
	return vars->name;
    }
    else
      vars = vars->prev;

  return locateC("<ERROR>");
}
#endif


/* implement the ? to assign macro-time variables */
static mac_po macro_def_var(cellpo ptn,macEnvPo macvars)
{
  if(!isSymb(ptn)){
    reportErr(lineInfo(ptn),"Illegal use of ? in %w",ptn);
    return emitN(M_SKP,macvars);
  }
  else
    return emitI(M_TAG,mac_var(symVal(ptn),macvars),macvars);
}

/* generate a place holder for sub-pattern */
static int macro_sub_pat(cellpo ptn,macEnvPo macvars)
{
  int ref;

  if(n_contained==max_contained){
    contrec cnt = (contrec)malloc((max_contained+MAX_MAC_VAR)*sizeof(struct contrec_));
    int ix = 0;

    for(ix=0;ix<n_contained;ix++)
      cnt[ix]=contained[ix];
    max_contained += MAX_MAC_VAR;
    if(contained!=NULL)
      free(contained);
    contained = cnt;		/* allocate the array */
  }

  if(ptn!=NULL && IsQuery(ptn,NULL,&ptn))
    contained[n_contained].ptn=ptn;
  else if(ptn!=NULL && isUnaryCall(ptn,kquery,&ptn))
    contained[n_contained].ptn=ptn;
  else
    contained[n_contained].ptn=ptn;

  /* mark two variables as being in use */
  ref = contained[n_contained++].vno=add_macro_var(NULL,macvars); 
  add_macro_var(NULL,macvars);	/* mark two variables as being in use */

  return ref;			/* return the index to first variable */
}

static logical sameptn(cellpo pt1,cellpo pt2)
{
  /* compare two patterns stripping out variable declarations */

  if(pt1!=NULL)
    if(IsQuery(pt1,&pt1,NULL) ||
       isUnaryCall(pt1,kquery,&pt1))
	   ;

  if(pt2!=NULL)
    if(IsQuery(pt2,&pt2,NULL) ||
       isUnaryCall(pt2,kquery,&pt2))
	   ;

  if(pt1==pt2)
    return True;
  else if(pt1==NULL||pt2==NULL)
    return False;

  switch(tG(pt1)){
  case intger:
    return IsInt(pt2)&&intVal(pt1)==intVal(pt2);

  case floating:
    return IsFloat(pt2) && fltVal(pt1)==fltVal(pt2);
    
  case character:
    return isChr(pt2) && CharVal(pt1)==CharVal(pt2);

  case strng:
    return IsString(pt2) && uniCmp(strVal(pt1),strVal(pt2))==0;

  case symbolic:
    return isSymb(pt2) && symVal(pt1)==symVal(pt2);

  case list:
    if(IsList(pt2)){
      if((pt1=listVal(pt1))==NULL)
	return listVal(pt2)==NULL;
      else
	return (pt2=listVal(pt2))!=NULL && 
	  sameptn(pt1,pt2) && sameptn(Next(pt1),Next(pt2));
    }
    else
      return False;

  case constructor:
    if(isCons(pt2) && consArity(pt1)==consArity(pt2) && sameptn(consFn(pt1),consFn(pt2))){
      unsigned int ar = consArity(pt1);
      pt1=consEl(pt1,0);
      pt2=consEl(pt2,0);

      while(ar--)
	if(!sameptn(pt1,pt2))
	  return False;		/* early exit */
	else{
	  pt1 = Next(pt1);
	  pt2 = Next(pt2);
	}
      return True;
    }
    else
      return False;
  case tuple:
    if(isTpl(pt2) && tplArity(pt1)==tplArity(pt2)){
      int ar = tplArity(pt1);
      pt1=tplArg(pt1,0);
      pt2=tplArg(pt2,0);

      while(ar--)
	if(!sameptn(pt1,pt2))
	  return False;		/* early exit */
	else{
	  pt1 = Next(pt1);
	  pt2 = Next(pt2);
	}
      return True;
    }
  default:
    return False;
  }
  return False;			/* should never get here */
}

/* find the place holder for a sub-pattern*/
static int macro_get_pat(cellpo ptn)
{
  int cx = 0;

  for(cx=0;cx<n_contained;cx++)
    if(sameptn(ptn,contained[cx].ptn))
      return contained[cx].vno;		/* index to variable used for sub-pattern */

  reportErr(lineInfo(ptn),"Contained replacement pattern not found in pattern %w",ptn);

  return -1;
}

/* Forward declarations */
static mac_po compile_quoted_pattern(cellpo ptn,int remaining,macEnvPo macvars);
static mac_po compile_macro_pattern(cellpo ptrn,int remaining,macEnvPo macvars);

static macdefpo compile_macros(cellpo defs,macdefpo base,macEnvPo inner)
{
  cellpo macro,lhs,rhs;

  if(IsHashStruct(defs,kdebug,&defs))
    return compile_macros(defs,base,inner);
  else if(IsHashStruct(defs,knodebug,&defs))
    return compile_macros(defs,base,inner);
  else if(IsHashStruct(defs,kmacro,&macro)&&isBinaryCall(macro,kmacreplace,&lhs,&rhs))
    return def_macro(lhs,rhs,base,inner);
  else if(isBinaryCall(defs,ksemi,&lhs,&rhs))
    return compile_macros(rhs,compile_macros(lhs,base,inner),inner);
  else if(isUnaryCall(defs,ksemi,&lhs))
    return compile_macros(lhs,base,inner);
  else{
    reportErr(lineInfo(defs),"Illegal macro clause %w ",defs);
    return base;
  }
}


/* Compile and check a macro search pattern into encoded format */
mac_po compile_macro_pattern(cellpo ptn,int remaining,macEnvPo macenv)
{
  switch (tG(ptn)) {

  case intger: 
    return emitC(M_INT,ptn,macenv);

  case floating:
    return emitC(M_FLT,ptn,macenv);

  case symbolic:{		/* most symbols match themselves */
    symbpo sym=symVal(ptn);
    int ref,depth;
 
    if(sym==kuscore || sym==kquery)
      return emitN(M_SKP,macenv);
    else if(sym==kinteger)
      return emitN(M_AINT,macenv);	/* Match any integer */
    else if(sym==knumber)
      return emitN(M_ANUM,macenv);	/* Match any number */
    else if(sym==ksymbol)
      return emitN(M_ASYM,macenv);	/* Match any symbol */
    else if(sym==kchar)
      return emitN(M_ACHR,macenv);	/* Match any character */
    else if(sym==kstring)
      return emitN(M_ASTR,macenv);	/* Match any string */
    else if(sym==klist)
      return emitN(M_ALST,macenv);       /* match any list */
    else if(sym==ktuple)
      return emitN(M_ATPL,macenv);       /* match any tuple */
    else if(sym==kquoted)         
      return emitN(M_QUOT,macenv);       // match a quoted symbol
    else if((ref=get_mac_var(sym,macenv,&depth))!=-1){	/* macro variable? */
      if(depth==0)
	return emitI(M_REF,ref,macenv);/* pick up the var's offset */
      else{
	mac_po mc = emitN(M_DEEP,macenv);
	mc->v.d.depth = depth;
	mc->v.d.off = ref;
	return mc;
      }
    }
    else
      return emitC(M_SYM,ptn,macenv);
  }
  
  case character:
    return emitI(M_CHR,CharVal(ptn),macenv);

  case strng:
    return emitC(M_STR,ptn,macenv);

  case list:{
    if((ptn=listVal(ptn))==NULL)
      return emitN(M_NIL,macenv);
    else{
      int tvr=0;
      mac_po mp;

      if(remaining>0){
	tvr = add_macro_var(NULL,macenv); /* mark a variable as being in use */
	mp = emitI(M_TAG,tvr,macenv);
	emitN(M_LIST,macenv);
      }
      else
	mp = emitN(M_LIST,macenv);
      
      compile_macro_pattern(ptn,1,macenv);
      compile_macro_pattern(Next(ptn),0,macenv);

      if(remaining>0){
	emitI(M_TOP,tvr,macenv);
	emitN(M_SKP,macenv);
      }
      return mp;
    }
  }
  
  case tuple:{			/* The pattern is a tuple */
    int ar = tplArity(ptn);
    mac_po mp;
    int tvr=0,i;

    if(remaining>0){
      tvr = add_macro_var(NULL,macenv);
      mp = emitI(M_TAG,tvr,macenv);
      emitI(M_TPL,ar,macenv);
    }
    else
      mp = emitI(M_TPL,ar,macenv);
      
    for(i=0;i<ar;i++)
      compile_macro_pattern(tplArg(ptn,i),ar-i-1,macenv);
      
    if(remaining>0){
      emitI(M_TOP,tvr,macenv);
      emitN(M_SKP,macenv);
    }

    return mp;
  }
  
  case constructor:{
    cellpo lhs,rhs;
    
    if(isUnaryCall(ptn,kmeta,&lhs))
      return compile_quoted_pattern(lhs,remaining,macenv);
    else if(isUnaryCall(ptn,kquery,&rhs)){
      /* straight variable asignment */
      mac_po m = macro_def_var(rhs,macenv);/* Define macro variable */
      emitN(M_SKP,macenv);
      return m;
    }
    else if(IsQuery(ptn,&lhs,&rhs)){
      mac_po m = macro_def_var(rhs,macenv); /* Define macro variable */
      compile_macro_pattern(lhs,remaining,macenv); /* embedded pattern */
      return m;
    }
    else if(isUnaryCall(ptn,ktilda,&rhs)){
      mac_po lb = m_label();
      mac_po m = emitM(M_ALT,lb,macenv); /* Alternative operator */
	  
      compile_macro_pattern(rhs,remaining,macenv);
      emitM(M_CUT,lb,macenv);	/* compile in a deep failure */
      emitN(M_FAIL,macenv);
      m_merge(lb,macenv);		/* Define the macro label */
      emitN(M_SKP,macenv);
      return m;
    }

    else if(isBinaryCall(ptn,ktilda,&lhs,&rhs)){ /* A~B => not B & A */
      mac_po lb = m_label();
      int vr = add_macro_var(NULL,macenv);
      mac_po m = emitI(M_TAG,vr,macenv);

      emitM(M_ALT,lb,macenv);	/* Alternative operator */
      compile_macro_pattern(rhs,remaining,macenv); /* check the negation */
      emitM(M_CUT,lb,macenv);	/* compile in a deep failure */
      emitN(M_FAIL,macenv);
      m_merge(lb,macenv);		/* Define the macro label */
      emitI(M_TOP,vr,macenv);

      compile_macro_pattern(lhs,remaining,macenv);
      return m;
    }

    else if(isBinaryCall(ptn,kcontain,&lhs,&rhs)){
      int s_p=macro_sub_pat(lhs,macenv); /* generate a place holder */
      mac_po m1=emitI(M_CNT,s_p,macenv); /* containment operator */

      compile_macro_pattern(lhs,0,macenv);
      emitI(M_TOP,s_p,macenv); /* restart search contained pattern */

      emitI(M_SUB,(s_p+1),macenv); /* Sub-search */
      compile_macro_pattern(rhs,0,macenv);
      if(remaining>0){
	emitI(M_TOP,s_p,macenv); /* and look at the rest of the pattern */
	emitN(M_SKP,macenv);
      }
      return m1;
    }
    else{
      unsigned int ar = consArity(ptn);
      unsigned int i;
      mac_po mp;
      int tvr=0;

      if(remaining>0){
        tvr = add_macro_var(NULL,macenv);
        mp = emitI(M_TAG,tvr,macenv);
        emitI(M_CONS,ar,macenv);
      }
      else
        mp = emitI(M_CONS,ar,macenv);
      
      compile_macro_pattern(consFn(ptn),ar,macenv);
      
      for(i=0;i<ar;i++)
        compile_macro_pattern(consEl(ptn,i),ar-i-1,macenv);
      
      if(remaining>0){
        emitI(M_TOP,tvr,macenv);
        emitN(M_SKP,macenv);
      }

      return mp;
    }
  }
  default:
    syserr("problem in compile_macro_ptn");
    return NULL;		/* Shouldnt ever get here ... */
  }
  return NULL;			/* should never get here */
}

/* Compile a quoted search pattern into encoded format */
mac_po compile_quoted_pattern(cellpo ptn,int remaining,macEnvPo macenv)
{
  switch (tG(ptn)) {

  case intger: 
    return emitC(M_INT,ptn,macenv);

  case floating: 
    return emitC(M_FLT,ptn,macenv);

  case symbolic:    		/* quoted symbols match themselves */
    return emitC(M_SYM,ptn,macenv);
    
  case character:
    return emitI(M_CHR,CharVal(ptn),macenv);

  case strng:
    return emitC(M_STR,ptn,macenv);
  
  case list:{
    if((ptn=listVal(ptn))==NULL)
      return emitN(M_NIL,macenv);
    else{
      int tvr=0;
      mac_po mp;

      if(remaining>0){
	tvr = add_macro_var(NULL,macenv); 
	mp = emitI(M_TAG,tvr,macenv);
	emitN(M_LIST,macenv);
      }
      else
	mp = emitN(M_LIST,macenv);
      
      compile_quoted_pattern(ptn,1,macenv);
      compile_quoted_pattern(Next(ptn),0,macenv);

      if(remaining>0){
	emitI(M_TOP,tvr,macenv);
	emitN(M_SKP,macenv);
      }
      return mp;
    }
  }
  
  case constructor:{			/* The pattern is an application */
    if(isUnaryCall(ptn,kanti,&ptn)) /* anti-quote operator */
      return compile_macro_pattern(ptn,remaining,macenv);
    else{
      unsigned int ar = consArity(ptn);
      unsigned int i;
      mac_po mp;
      int tvr=0;

      if(remaining>0){
        tvr = add_macro_var(NULL,macenv);
        mp = emitI(M_TAG,tvr,macenv);
        emitI(M_CONS,ar,macenv);
      }
      else
        mp = emitI(M_CONS,ar,macenv);
            
     compile_quoted_pattern(consFn(ptn),ar,macenv);

     for(i=0;i<ar;i++)
        compile_quoted_pattern(consEl(ptn,i),ar-i-1,macenv);
    
      if(remaining>0){
        emitI(M_TOP,tvr,macenv);
        emitN(M_SKP,macenv);
      }
      return mp;
    }
  }

  case tuple:{
    int ar = tplArity(ptn);
    mac_po mp;
    int tvr=0,i;

    if(remaining>0){
      tvr = add_macro_var(NULL,macenv);
      mp = emitI(M_TAG,tvr,macenv);
      emitI(M_TPL,ar,macenv);
    }
    else
      mp = emitI(M_TPL,ar,macenv);
            
    for(i=0;i<ar;i++)
      compile_quoted_pattern(tplArg(ptn,i),ar-i-1,macenv);
    
    if(remaining>0){
      emitI(M_TOP,tvr,macenv);
      emitN(M_SKP,macenv);
    }
    return mp;
  }
  default:
    syserr("problem in compile quote ptn");
    return NULL;		/* should never get here */
  }
}

void compile_quoted_replacement(cellpo, int *,int,macEnvPo); /* Forward dec. */

/* Compile and check a macro replacement pattern into encoded format */
static void compile_macro_replacement(cellpo ptrn,int *size,int remaining,
				      macEnvPo macenv)
{
  unsigned int ar;
  cellpo lhs,rhs,rep;

  if(IsInt(ptrn))
    emitC(R_INT,ptrn,macenv);
  else if(IsFloat(ptrn))
    emitC(R_FLT,ptrn,macenv);
  else if(isSymb(ptrn)){
    int depth;
    int ref = get_mac_var(symVal(ptrn),macenv,&depth);
    if(ref!=-1){
      if(depth==0)
	emitI(R_REF,ref,macenv);
      else{
	mac_po mc = emitN(R_DEEP,macenv);
	mc->v.d.depth = depth;
	mc->v.d.off = ref;
      }
    }
    else
      emitC(R_SYM,ptrn,macenv);
  }
  else if(isChr(ptrn))
    emitC(R_CHR,ptrn,macenv);
  else if(IsString(ptrn))
    emitC(R_STR,ptrn,macenv);
  else if(isEmptyList(ptrn))
    emitN(R_NIL,macenv);
  else if(isNonEmptyList(ptrn)){
    int tvr=0;
    mac_po mp;

    ptrn = listVal(ptrn);

    if(remaining>0){
      tvr = add_macro_var(NULL,macenv); 
      mp = emitI(R_TAG,tvr,macenv);
      emitN(R_LIST,macenv);
    }
    else
      mp = emitN(R_LIST,macenv);
      
    compile_macro_replacement(ptrn,size,1,macenv);
    compile_macro_replacement(Next(ptrn),size,0,macenv); 

    if(remaining>0)
      emitI(R_NXT,tvr,macenv);
  }
  else if(isUnaryCall(ptrn,kmeta,&lhs))
    compile_quoted_replacement(lhs,size,remaining,macenv); 
  
  else if(isUnaryCall(ptrn,kdhash,&rhs)){
    if(isSymb(rhs))
      emitC(R_MAC,rhs,macenv);
    else{
      reportErr(lineInfo(ptrn),"Illegal use of ## in  %w",ptrn);
      emitN(R_SKP,macenv);	/* dummy match */
    }
  }
  else if(isBinaryCall(ptrn,kdhash,&lhs,&rhs)){
    int intv = add_macro_var(NULL,macenv);
    add_macro_var(NULL,macenv);	/* we need three temporary vars */
    add_macro_var(NULL,macenv);
    emitI(R_TAG,(intv+2),macenv);
    emitI(R_TMP,intv,macenv);
    compile_macro_replacement(lhs,size,remaining,macenv);
    emitI(R_DO,intv,macenv);
    emitI(R_TMP,(intv+1),macenv);
    compile_macro_replacement(rhs,size,remaining,macenv);
    emitI(R_DO,(intv+1),macenv);
    emitI(R_CAT,intv,macenv);
  }

  else if(isBinaryCall(ptrn,kcontain,&lhs,&rhs)){
    int tvr=0;

    if(remaining>0)
      emitI(R_TAG,(tvr=add_macro_var(NULL,macenv)),macenv);

    /* pick up vname */
    emitI(R_CNT,macro_get_pat(lhs),macenv);
    compile_macro_replacement(rhs,size,remaining,macenv);

    if(remaining>0)
      emitI(R_NXT,tvr,macenv);
  }

  else if(isBinaryCall(ptrn,kmacreplace,&lhs,&rep) &&
	  isBinaryCall(lhs,ktilda,&lhs,&rhs)){
    int v = add_macro_var(NULL,macenv); /* We need to know whats going on */
    int vx = add_macro_var(NULL,macenv);
    int tmp = add_macro_var(NULL,macenv);
    mac_po lb = m_label();

    emitI(R_TAG,v,macenv);

    /* generate the initial replace */
    compile_macro_replacement(lhs,size,0,macenv); 

    emitI(R_TMP,tmp,macenv);
    emitI(R_TOP,v,macenv); /* pick up the original */
    emitN(M_STPL,macenv); /* start a new match */

    emitM(M_ALT,lb,macenv);	/* we have an alternative ... */

    emitI(M_TAG,vx,macenv);
    
    compile_macro_pattern(rhs,0,macenv); /* compile sub-pattern */

    emitM(M_CUT,lb,macenv);	/* cut the choice point */

    emitI(R_TOP,tmp,macenv);	/* initiate sub-replacement */

    compile_macro_replacement(rep,size,0,macenv); 

    emitI(R_TOP,vx,macenv);
    emitI(R_REF,tmp,macenv); /* pick up the temporary value */

    m_merge(lb,macenv);	/* define the label -- skip and move on */

    emitN(M_NXL,macenv); /* Check next element of the tuple */
  }
  else if(IsMacroDef(ptrn,&lhs,&rhs)){
    int pv=add_macro_var(NULL,macenv);
    int rv=add_macro_var(NULL,macenv);
    int tvr=0;

    if(remaining>0)
      emitI(R_TAG,(tvr=add_macro_var(NULL,macenv)),macenv);
      
    compile_macro_replacement(voidcell,size,remaining,macenv);

    emitI(R_TMP,pv,macenv);
    compile_macro_replacement(lhs,size,0,macenv);
    emitI(R_TMP,rv,macenv);
    compile_macro_replacement(rhs,size,0,macenv);
    emitI(R_DEF,pv,macenv); /* define the macro */

    if(remaining>0)
      emitI(R_NXT,tvr,macenv);
  }
  else if(IsHashStruct(ptrn,kecho,&rhs)){
    int pv=add_macro_var(NULL,macenv);
    emitI(R_TAG,pv,macenv);
    compile_macro_replacement(rhs,size,remaining,macenv);
    emitI(R_ECHO,pv,macenv);
  }
  else if(IsHashStruct(ptrn,kinclude,&rhs)){
    int pv=add_macro_var(NULL,macenv);
    emitI(R_TAG,pv,macenv);
    compile_macro_replacement(rhs,size,remaining,macenv);
    emitI(R_INCLUDE,pv,macenv);
  }
  else if(isUnaryCall(ptrn,khash,&rhs) && 
	  isUnaryCall(rhs,kfile,&rhs)){
    int fl=add_macro_var(NULL,macenv);

    emitI(R_TAG,fl,macenv);
    compile_macro_replacement(rhs,size,remaining,macenv);
    {
      mac_po mc = emitN(R_FILE,macenv);
      mc->v.d.off = fl;
    }
  }

  else if(isUnaryCall(ptrn,khash,&rhs) &&
	  isBinaryCall(rhs,kerror,&lhs,&rhs)){
    int er=add_macro_var(NULL,macenv);
    int rv=add_macro_var(NULL,macenv);

    emitI(R_TAG,rv,macenv);
    compile_macro_replacement(rhs,size,remaining,macenv);
    emitI(R_DO,rv,macenv);
    emitI(R_TMP,er,macenv);
    compile_macro_replacement(lhs,size,0,macenv);
    emitI(R_ERROR,er,macenv);
    emitI(R_NXT,rv,macenv);
  }
  else if(isUnaryCall(ptrn,khash,&rhs) &&
	  isBinaryCall(rhs,kwarning,&lhs,&rhs)){
    int er=add_macro_var(NULL,macenv);
    int rv=add_macro_var(NULL,macenv);

    emitI(R_TAG,rv,macenv);
    compile_macro_replacement(rhs,size,remaining,macenv);
    emitI(R_DO,rv,macenv);
    emitI(R_TMP,er,macenv);
    compile_macro_replacement(lhs,size,0,macenv);
    emitI(R_WARN,er,macenv);
    emitI(R_NXT,rv,macenv);
  }
  else if(IsHashStruct(ptrn,klist2tuple,&rhs)){
    int pv=add_macro_var(NULL,macenv);
    emitI(R_TAG,pv,macenv);
    compile_macro_replacement(rhs,size,remaining,macenv);
    emitI(R_DO,pv,macenv);
    emitI(R_LIST2TUPLE,pv,macenv);
  }
  else if(IsHashStruct(ptrn,ktuple2list,&rhs)){
    int pv=add_macro_var(NULL,macenv);
    emitI(R_TAG,pv,macenv);
    compile_macro_replacement(rhs,size,remaining,macenv);
    emitI(R_DO,pv,macenv);
    emitI(R_TUPLE2LIST,pv,macenv);
  }
  else if(isBinaryCall(ptrn,khash,&lhs,&rhs)){ /* nested macro scope */
    int pv=add_macro_var(NULL,macenv);
    emitI(R_TAG,pv,macenv);

    compile_macro_replacement(lhs,size,0,macenv);

    emitI(R_NXT,pv,macenv);
    emitD(R_STACK,compile_macros(rhs,NULL,macenv),macenv);
  }
  else if(isCons(ptrn)){
    int tvr=0;
    unsigned int i;
    /* If we get here we have a regular tuple */

    if(remaining>0)
      emitI(R_TAG,(tvr=add_macro_var(NULL,macenv)),macenv);
      
    emitI(R_CONS,(ar=consArity(ptrn)),macenv); /* tuple match operator */
    *size+=ar;			/* increment the size required */
    
    compile_macro_replacement(consFn(ptrn),size,ar,macenv);

    for(i=0;i<ar;i++)
      compile_macro_replacement(consEl(ptrn,i),size,ar-i-1,macenv);

    if(remaining>0)
      emitI(R_NXT,tvr,macenv);
  }
  
  else if(isTpl(ptrn)){
    int tvr=0;
    unsigned int i;
    /* If we get here we have a regular tuple */

    if(remaining>0)
      emitI(R_TAG,(tvr=add_macro_var(NULL,macenv)),macenv);
      
    emitI(R_TPL,(ar=tplArity(ptrn)),macenv); /* tuple match operator */
    *size+=ar;			/* increment the size required */

    for(i=0;i<ar;i++)
      compile_macro_replacement(tplArg(ptrn,i),size,ar-i-1,macenv);

    if(remaining>0)
      emitI(R_NXT,tvr,macenv);
  }
  else{
    reportErr(lineInfo(ptrn),"Unknown replacement pattern %w",ptrn);
    emitN(R_SKP,macenv);	/* dummy match */
  }
}

/* Compile a quoted replacement into encoded format */
void compile_quoted_replacement(cellpo ptn,int *size,int remaining,macEnvPo macenv)
{
  if(IsInt(ptn))
    emitC(R_INT,ptn,macenv);
  else if(IsFloat(ptn))
    emitC(R_FLT,ptn,macenv);
  else if(isSymb(ptn))
    emitC(R_SYM,ptn,macenv);
  else if(isChr(ptn))
    emitC(R_CHR,ptn,macenv);
  else if(IsString(ptn))
    emitC(R_STR,ptn,macenv);
  else if(isEmptyList(ptn))
    emitN(R_NIL,macenv);
  else if(isNonEmptyList(ptn)){
    int tvr=0;
    mac_po mp;

    if(remaining>0){
      tvr = add_macro_var(NULL,macenv); /* mark a variable as being in use */
      mp = emitI(R_TAG,tvr,macenv);
      emitN(R_LIST,macenv);
    }
    else
      mp = emitN(R_LIST,macenv);
    
    ptn = listVal(ptn);
    compile_quoted_replacement(ptn,size,1,macenv);
    compile_quoted_replacement(Next(ptn),size,0,macenv); 

    if(remaining>0)
      emitI(R_NXT,tvr,macenv);
  }
  else if(isUnaryCall(ptn,kanti,&ptn)) /* Look for unquote pattern: &(P) */
    compile_macro_replacement(ptn,size,remaining,macenv);
  else if(isTpl(ptn)){
    unsigned int ar = tplArity(ptn);
    int tvr=0;
    unsigned int i;
    /* If we get here we have a regular quoted tuple */

    if(remaining>0)
      emitI(R_TAG,(tvr=add_macro_var(NULL,macenv)),macenv);
      
    emitI(R_TPL,ar,macenv); /* emit a tuple match operator */
    *size+=ar;			/* increment the size required */

    for(i=0;i<ar;i++)
      compile_quoted_replacement(tplArg(ptn,i),size,ar-i-1,macenv);

    if(remaining>0)
      emitI(R_NXT,tvr,macenv);
  }
  else if(isCons(ptn)){
    unsigned int ar = consArity(ptn);
    int tvr=0;
    unsigned int i;
    /* If we get here we have a regular quoted tuple */

    if(remaining>0)
      emitI(R_TAG,(tvr=add_macro_var(NULL,macenv)),macenv);
      
    emitI(R_CONS,ar,macenv); /* emit a tuple match operator */
    *size+=ar;			/* increment the size required */

    compile_quoted_replacement(consFn(ptn),size,ar,macenv);

    for(i=0;i<ar;i++)
      compile_quoted_replacement(consEl(ptn,i),size,ar-i-1,macenv);

    if(remaining>0)
      emitI(R_NXT,tvr,macenv);
  }
  else{
    reportErr(lineInfo(ptn),"Unknown replacement pattern %w",ptn);
    emitN(R_SKP,macenv);	/* dummy match */
  }
}

/* try to determine the top-level pattern of the macro */
static logical find_top_level(cellpo input, symbpo *top,logical *applic)
{
  cellpo lhs,fn,rhs;

  if(IsInt(input) || IsFloat(input)){
    *top = knumber;		/* not really permitted */
    *applic = False;		/* special flag to denote non-structure */
    return False;		/* Invalid top-level of macro */
  }
  else if(IsString(input)){
    *top = kstring;		/* not really permitted */
    *applic = False;		/* special flag to denote non-structure */
    return False;		/* Invalid top-level of macro */
  }
  else if(isSymb(input)){
    *top = symVal(input);
    *applic = False;		/* special flag to denote non-structure */
    return True;		/* This one is allowed */
  }
  else if(IsQuote(input,&lhs) && isSymb(lhs)){
    *top = symVal(lhs);
    *applic = False;		/* special flag to denote non-structure */
    return True;		/* This one is allowed */
  }
  else if(IsList(input)){
    *top = kcons;		/* make ,.. the top-level */
    *applic = True;
    return True;		/* This one is allowed */
  }
  else if(IsQuery(input,&lhs,NULL) || 
	  isBinaryCall(input,kcontain,&lhs,NULL))
    return find_top_level(lhs,top,applic);
  else if(isBinaryCall(input,kat,&fn,&rhs)){
    if(isSymb(fn)){
      *top = symVal(fn);
      *applic = True;
      return True;
    }
    else{
      *top = kcall;
      *applic = True;
      return False;		/* not allowed... */
    }
  }
  else if(isCons(input)){
    *applic = True;
    if(isSymb(fn=consFn(input)) || (IsQuote(fn,&fn) && isSymb(fn))){
      *top = symVal(fn);
      return True;
    }
    else if(isUnaryCall(fn,kmeta,&fn) && isSymb(fn)){
      *top = symVal(fn);
      return True;
    }
    else{
      *top = kblock;
      return False;
    }
  }
  else if(isTpl(input)){
    *top = kcall;		/* Other tuples */
    *applic = False;
    return True;		/* This one is allowed */
  }
  else{
    *top = kany;		/* dont dont -- probably illegal */
    *applic = False;
    return False;		/* This one is not allowed */
  }
}

/* define a macro */
static macdefpo def_macro(cellpo ptn,cellpo rep,macdefpo mac_top,macEnvPo inner)
{
  int s_p;
  mac_po base;
  macdefpo def = (macdefpo)allocPool(defpool);
  macEnvRec macvars;
  long errcount = comperrflag;

  macvars.vars = NULL;
  macvars.nvars = 0;
  macvars.inner = inner;
  macvars.pc = NULL;

  n_contained=0;		/* Initially no containment operators */
  macroCounter++;

  s_p = add_macro_var(NULL,&macvars); /* place holder for main pattern */

  def->size=0;	/* clear the size of replacement text */
  def->pattern=base=compile_macro_pattern(ptn,0,&macvars); /* main pttrn */
  def->prev = mac_top;
  def->head = ptn;		/* We use this to help trace macros */

  if(!find_top_level(ptn,&def->topsymb,&def->applic))
    reportWarning(lineInfo(ptn),"Top-level of macro pattern `%w' should be symbol, or call",ptn);
    
  emitI(R_INIT,0,&macvars);	/* start replacement sequence */
  compile_macro_replacement(rep,&def->size,0,&macvars);
  emitN(M_RTN,&macvars);	/* successful search */

  def->nvars=macvars.nvars;

#ifdef MACROTRACE
  if(traceMacro){
    long i;

    outMsg(logFile,"Macro def: %U\n",def->topsymb);
    prt_mac_seq(logFile,def->pattern);
    outStr(logFile,"\nVars:\n");
    for(i=0;i<macvars.nvars;i++)
      outMsg(logFile,"var[%d]: %U\n",i,mac_var_name(i,&macvars));
    flushFile(logFile);
  }
#endif

  { macVpo vrs = macvars.vars;

    while(vrs!=NULL){
      macVpo pr = vrs->prev;
      freePool(macvpool,vrs);
      vrs = pr;
    }
  }

  if(comperrflag==errcount)	/* Ignore any macros that have errors */
    return def;
  else{
    free_macro(base);		/* get rid of the erroneous macro code */
    freePool(defpool,def);
    return mac_top;
  }
}

macdefpo define_macro(cellpo ptn,cellpo rep,macdefpo mac_top)
{
  return def_macro(ptn,rep,mac_top,NULL);
}

/* State variables to keep track of the pattern search */
typedef struct{
  cellpo input;			/* Next data to consider */
  int prev_push;		/* One up in the 'call' stack */
  cellpo last;			/* last element in tuple */
  mac_po ptn;			/* Used for generic tuples */
} mac_call, *mac_callpo;

static mac_call input_stack[MAX_MAC_DEPTH];
static int top_input,cur_input;

/* Alternatives stack */
static mac_choice choices[MAX_MAC_CHOICE];
static int top_choice;

/* Macro replacement invokation structure */
typedef struct _macro_invokation_ *macInvPo;

typedef struct _macro_invokation_ {
  cellpo *vars;			/* macro variables */
  macInvPo prev;		/* Previous invokation */
} MacInvRec;

void push_input(cellpo,cellpo,mac_po); /* save the pointer to the next input record */
void push_choice(mac_po, cellpo, int); /* save choice point */
logical inspect_tpl(cellpo);	/* Might we go into this tuple? */

/**********************************************************************/
/*                    Search for matching macro                       */
/**********************************************************************/

static cellpo macroReplace(cellpo orig,cellpo input,macInvPo env,
			   macListPo macTop,int depth,optPo opts);

static logical match_pattern(macdefpo mc,mac_po ptn,cellpo input,
			     macInvPo env,macListPo macros,int depth,optPo opts);
static void join_cells(cellpo c1,cellpo c2,cellpo dest);
static cellpo replycate(cellpo input, cellpo src, cellpo trg);

/* Execute macro code to match input against a pattern */
/* This is formulated as a recursive program that tries the macros in 
   reverse order in the list */
static logical try_macro(cellpo orig_input,cellpo input,
			 symbpo topsymb,logical isapplic,
			 macInvPo prev,macListPo macTop,int depth,optPo opts)
{
  macListPo top = macTop;

  while(top!=NULL){
    macdefpo mc = top->macros;

    while(mc!=NULL){
      register symbpo mctop=mc->topsymb;
      if(mctop==topsymb && mc->applic==isapplic){
	int i,nvars = mc->nvars;
	MacInvRec env;
	cellpo base_vars[32];	/* try to use stack allocation */
	cellpo *vars = (nvars>=32?(cellpo *)malloc(sizeof(cellpo)*(nvars+1)):
			base_vars);

	top_input=0;		/* initialize the macro variables stack */
	top_choice=0;
	cur_input=0;

	env.vars = vars;	/* set up the context in case of deep calls */
	env.prev = prev;

	for(i=2;i<nvars;i++)
	  vars[i]=voidcell;	/* void means macro variable is not bound */
    
	vars[0] = orig_input;	/* pre-load the top-two system variables */
	vars[1] = input;	/* set up the input pointer */

#ifdef MACROTRACE
	if(traceMacro)
	  outMsg(logFile,"\nTrying macro %U at %w/%d on %5w\n",mctop,
		 source_file(mc->head),source_line(mc->head),input);
#endif

	if(match_pattern(mc,mc->pattern,input,&env,macTop,depth,opts)){
#ifdef MACROTRACE
	  if(traceMacro)
	    outMsg(logFile,"Replaced structure is %w\n",vars[0]);
#endif
	  copyCell(input,vars[0]);	/* return the result */
	  if(vars!=base_vars)	/* Only free if previously allocated */
	    free(env.vars);
	  return True;		/* we found a macro that matches! */
	}
	else if(vars!=base_vars)
	  free(env.vars);	/* release the variable array */
      }
      mc = mc->prev;
    }
    top = top->previous;
  }
  return False;
}

static logical match_pattern(macdefpo mc,mac_po ptn,cellpo input,
			     macInvPo env,macListPo macros,int depth,optPo opts)
{
  int l_info = lineInfo(input);	/* We will copy this across */
  cellpo *vars = env->vars;
  long macro_number=macro_num;	/* Used in macro-generated symbols */

  while(ptn!=NULL){		/* This will not be exited normally ... */
#ifdef MACROTRACE
    if(traceMacro){
      prt_macro(logFile,ptn);
      flushFile(logFile);
    }
#endif
    switch(ptn->code){		/* What is the matching command? */

    case M_NONE:		/* Do nothing ... target of jmps */
      break;

    case M_INT:{		/* general integer */
      if(!IsInt(input)||
	 intVal(input)!=intVal(ptn->v.vl))
	goto fail;
      else
	input=Next(input);
      break;
    }

    case M_SYM:{		/* match a symbol */
      if(!IsSymbol(input,symVal(ptn->v.vl)))
	goto fail;
      else
	input=Next(input);
      break;
    }
    
    case M_CHR:{                // Match a character
      if(!isChr(input) || CharVal(input)!=ptn->v.i)
        goto fail;
      else
        input=Next(input);
      break;
    }

    case M_STR:{		/* string */
      if(!IsString(input)||uniCmp(strVal(input),strVal(ptn->v.vl))!=0)
	goto fail;
      else
	input=Next(input);
      break;
    }

    case M_TAG:			/* define variable */
      vars[ptn->v.i]=input;	/* assign macro-time variable */
      break;

    case M_REF:{		/* reference to other vars */
      if(!sameCell(vars[ptn->v.i],input))
	goto fail;
      input=Next(input);
      break;
    }

    case M_DEEP:{		/* Reference a deep variable */
      int depth = ptn->v.d.depth;
      macInvPo stack = env;
      
      while(depth-->0){
	assert(stack->prev!=NULL);
	stack = stack->prev;
      }

      if(!sameCell(stack->vars[ptn->v.d.off],input))
	goto fail;

      input=Next(input);
      break;
    }

    case M_FLT:{		/* floating point number */
      if(!IsFloat(input)||fltVal(input)!=fltVal(ptn->v.vl))
	goto fail;
      input=Next(input);
      break;
    }

    case M_NIL:
      if(!IsList(input) || listVal(input)!=NULL)
	goto fail;
      input=Next(input);
      break;

    case M_LIST:
      if(!IsList(input) || listVal(input)==NULL)
	goto fail;
      else{
	input = listVal(input);
	break;
      }

    case M_EOL:{		/* Check for list */
      if(!IsList(input))
	goto fail;
      else if(listVal(input)!=NULL){
	input = listVal(input);
	ptn = ptn->v.alt;
	continue;
      }
      else
	break;			/* We have done the whole list */
    }

    case M_TPL:{		/* general tuple */
      if(!isTpl(input)||tplArity(input)!=(unsigned)ptn->v.i)
	goto fail;

      if(tplArity(input)>0)
	input=tplArg(input,0);	/* Go to the 1st el. of the tuple */
      break;
    }

    case M_CONS:{		/* general constructor */
      if(!isCons(input)||consArity(input)!=(unsigned)ptn->v.i)
	goto fail;

      input=consFn(input);	/* Go to the 1st el. of the tuple */
      break;
    }

    case M_SKP:			/* Can be anything */
      input=Next(input);
      break;

    case M_AINT:		/* Any integer */
      if(IsInt(input)){
        input=Next(input);
	break;
      }
      else
	goto fail;
      
    case M_AFLT:		/* Any floating point */
      if(IsFloat(input)){
        input=Next(input);
	break;
      }
      else
	goto fail;

    case M_ANUM:{		/* Any number */
      if(IsFloat(input)||IsInt(input))
	input=Next(input);
      else
	goto fail;
      break;
    }

    case M_ASYM:{		/* Any symbol */
      if(isSymb(input))
	input=Next(input);
      else
	goto fail;
      break;
    }
    
    case M_QUOT:{               // A quoted symbol
      if(isUnaryCall(input,kquote,NULL))
        input=Next(input);
      else
        goto fail;
      break;
    }

    case M_ACHR:{		/* Any character */
      if(isChr(input))
	input=Next(input);
      else
	goto fail;
      break;
    }

    case M_ASTR:{		/* Any string */
      if(IsString(input))
	input=Next(input);
      else
	goto fail;
      break;
    }

    case M_ALST:{		/* Any list */
      if(IsList(input))
	input=Next(input);
      else
	goto fail;
      break;
    }

    case M_ATPL:{		/* Any tuple */
      if(!isTpl(input))
	goto fail;
      else
	input=Next(input);
      break;
    }
  
    case M_STPL:{		/* Sub-tuple */
      if(!isTpl(input))
	goto fail;
      else{
	push_input(Next(input),NULL,NULL); /* save the next cell to match */
	push_input(tplArg(input,1),tplArg(input,tplArity(input)-1),ptn->next); /* push call stack */
	input=tplArg(input,0);
	break;
      }
    }
  
    case M_NXL:{		/* Pop at end of arbitrary tuple element */
      cellpo last = input_stack[cur_input].last;
      mac_po sptn=input_stack[cur_input].ptn;

      input=input_stack[cur_input].input; /* where to carry on */

      cur_input=input_stack[cur_input].prev_push;
      if(sptn!=NULL)		/* Only restore if explicitly stored */
	ptn=sptn;
      else
	ptn=ptn->next;

      if(top_choice>0)		/* See where to adjust call stack to */
	top_input=(cur_input>=choices[top_choice-1].top_input?cur_input
		   :choices[top_choice-1].top_input);
      else
	top_input=cur_input+1;

      if(last!=NULL&&last>input)
	push_input(Next(input),last,sptn); /* save pointer to the next record */

      continue;			/* Do some more matching */
    }

    case M_CNT:			/* Contained search */
      vars[ptn->v.i]=input;	/* assign macro-time variable whole pattern*/
      push_input(Next(input),NULL,NULL); /* set up subsequent search */
      break;

    case M_TOP:{		/* Get top of current input */
      input=vars[ptn->v.i];	/* where to carry on */
      break;
    }

    case M_SUB:{		/* Look for a sub-pattern - try whole input*/
      if(inspect_tpl(input))		/* Might we go into this tuple? */
	push_choice(ptn,tplArg(input,0),tplArity(input));
      else if(isNonEmptyList(input))
	push_choice(ptn,listVal(input),2);
      else if(isCons(input))
        push_choice(ptn,consFn(input),consArity(input)+1);
      vars[ptn->v.i]=input;	/* assign macro-time variable whole pattern*/
      break;
    }

    case M_ALT:			/* Alternatives */
      push_choice(ptn->v.alt,input,1);
      break;			/* Try the first alternative */

    case M_JMP:			/* jump around alternatives */
      ptn = ptn->v.alt;
      continue;

    case M_RTN:			/* Finish the match */
      macro_num++;		/* Prepare for next macro */
      return True;

    case M_CUT:{		/* Prolog style cut */
      mac_po fl = ptn->v.alt;	/* where to fail back to */

      do{
	top_choice--;		/* unstack atleast one choice point */
	top_input=choices[top_choice].top_input;
	cur_input=choices[top_choice].cur_input;
      } while (choices[top_choice].choicept != fl);

      break;
    }

    case M_FAIL:		/* We come here when we are failing ... */
    fail:
      if(top_choice==0)
	return False;
      else{
	top_choice--;
	ptn=choices[top_choice].choicept; /* go for the alternative */
	top_input=choices[top_choice].top_input;
	cur_input=choices[top_choice].cur_input;
	input = choices[top_choice].input;
	if(--choices[top_choice].remaining){
	  choices[top_choice].input=Next(choices[top_choice].input); /* step on to the next element to try */
	  top_choice++;
	}
	continue;
      }

      /* replacement macro instructions */
    case R_INIT:		/* Initialize the replacement sequence */
      input = vars[ptn->v.i] = allocSingle();
#ifdef MACROTRACE
      if(traceMacro && source_file(mc->head)!=NULL){
	outMsg(logFile,"macro at %w/%d fired\n",
	       source_file(mc->head),source_line(mc->head));
	flushFile(logFile);
      }
#endif
      break;

    case R_INT:
    case R_FLT:
    case R_SYM:
    case R_CHR:
    case R_STR:
      copyCell(input,ptn->v.vl);	/* store the literal value */
      stLineInfo(input,l_info);		/* copy in the source line info */
      input=Next(input);
      break;

    case R_REF:
      copy(vars[ptn->v.i],input); 	/* pick up the value of the variable */
      input=Next(input);
      break;

    case R_DEEP:{		/* Reference a deep variable */
      int depth = ptn->v.d.depth;
      macInvPo stack = env;
      
      while(depth-->0){
	assert(stack->prev!=NULL);
	stack = stack->prev;
      }

      copy(stack->vars[ptn->v.d.off],input); /* pick up the deep value */
      input=Next(input);
      break;
    }

    case R_TAG:			/* store variable during reconstruction */
      vars[ptn->v.i]=input;	/* pick up the value of the variable */
      break;

    case R_TOP:			/* variable stored during reconstruction */
      input=Next(vars[ptn->v.i]);	/* pick up the value of the variable */
      break;

    case R_NXT:			/* variable stored during reconstruction */
      input=Next(vars[ptn->v.i]);	/* pick up the value of the variable */
      break;

    case R_NIL:			/* Construct an empty list */
      mkList(input,NULL);
      stLineInfo(input,l_info);
      input=Next(input);
      break;

    case R_LIST:{		/* Construct non-empty list */
      cellpo lst = allocPair();

      mkList(input,lst);
      stLineInfo(input,l_info);
      input = lst;
      break;
    }

    case R_TPL:{
      cellpo tpl=allocTuple(ptn->v.i); /* Allocate a tuple */

      mkTpl(input,tpl);	/* store the tuple in place */
      stLineInfo(input,l_info);
      input=Next(tpl);
      break;
    }
    
    case R_CONS:{
      cellpo tpl=allocCons(ptn->v.i); /* Allocate a constructor */

      mkCons(input,tpl);	/* store the tuple in place */
      stLineInfo(input,l_info);
      input=Next(tpl);
      break;
    }

    case R_SKP:		/* Can be anything */
      input=Next(input);
      break;

    case R_CNT:		/* Start rewriting a structure */
      input=replycate(input,vars[ptn->v.i],vars[ptn->v.i+1]); /* copy original */
      break;

    case R_TMP:			/* start writing to a macro variable */
      input = vars[ptn->v.i] = allocSingle();
      break;

    case R_MAC:{		/* A macro-time generated variable name */
      uniChar sym[300];		/* temporary place to hold string */

      strMsg(sym,NumberOf(sym),"%U%U%d",symVal(ptn->v.vl),opts->macJoin,macro_number);

      mkSymb(input,locateU(sym));
      stLineInfo(input,l_info);	/* copy in the source line info */
      input=Next(input);
      break;
    }

    case R_CAT:{		/* Concatenate two objects together */
      input = vars[ptn->v.i+2];	/* Pick up where the input is going */
      join_cells(vars[ptn->v.i],vars[ptn->v.i+1],input);

      stLineInfo(input,l_info);
      input=Next(input);
      break;
    }

    case R_LIST2TUPLE:		/* Convert a list to a tuple */
      if(IsList(input=vars[ptn->v.i])){
        List2Tuple(input,input);
	stLineInfo(input,l_info);
        input=Next(input);
      }
      else
	reportErr(lineInfo(input),"Non-list %w being converted to tuple",input);
      break;

    case R_TUPLE2LIST:
      input=vars[ptn->v.i];
      Tuple2List(input,input);
      stLineInfo(input,l_info);
      input=Next(input);
      break;

    case R_DEF:		/* define a macro */
      macros->macros = def_macro(vars[ptn->v.i],vars[ptn->v.i+1],macros->macros,NULL);
      break;

    case R_ECHO:		/* echo a message to the screen */
      outMsg(logFile,"%w\n",vars[ptn->v.i]);
      break;

    case R_INCLUDE:{		/* include a subsiduary file here */
      cellpo file=input=vars[ptn->v.i];
      cellpo f;
      uniChar fn[1024];
      ioPo incFile=NULL;		/* Include file no. */
      long orig = stackDepth();
      uniChar *includePath = opts->include;
      
      if((isUnaryCall(file,kchoice,&f) &&
	  isUnaryCall(f,kchoice,&file)) ||
	 (isUnaryCall(file,kgreaterthan,&f) &&
	  isUnaryCall(f,klessthan,&file))){
	rmDots("sys:",file,fn,NumberOf(fn));
	includePath = opts->sysinclude;
      }
      else if(isSymb(file))
	uniCpy(fn,NumberOf(fn),symVal(file));
      else if(IsString(file))
        strMsg(fn,NumberOf(fn),"%U",strVal(file));
      else{
	reportErr(lineInfo(file),"illegal form of include: %w",file);
	break;
      }

      if((incFile=findURL(opts->sysinclude,includePath,fn,unknownEncoding))==NULL){
	reportErr(lineInfo(file),"file %w not found",file);
	break;
      }

      /* call processor on include file */
      Process(incFile,NULL,opts);

      while(stackDepth()-orig>1)
	pBinary(csemi);		/* cons everything up into a sequence */

      if(stackDepth()-orig==1) /* we have an element to include */
	popcell(input);
      else
	mkSymb(input,kvoid);	/* return void */
      closeFile(incFile);

      input=Next(input);
      break;
    }

    case R_FILE:{		/* generate a file name */
      cellpo file = input=vars[ptn->v.d.off];
      cellpo f;
      uniChar fn[MAXSTR+1];
      
      if((isUnaryCall(file,kchoice,&f) &&
	  isUnaryCall(f,kchoice,&file)) ||
	 (isUnaryCall(file,kgreaterthan,&f) &&
	  isUnaryCall(f,klessthan,&file))){
	rmDots("sys:",file,fn,NumberOf(fn));
	mkStr(input,allocString(fn));

	input = Next(input);
	break;
      }
      else if(isSymb(file))
        uniCpy(fn,NumberOf(fn),symVal(file));
      else if(IsString(file))
        uniCpy(fn,NumberOf(fn),strVal(file));
      else{
	reportErr(lineInfo(file),"illegal form of name: %w",file);
	break;
      }

      mkStr(input,allocString(fn));
      input=Next(input);
      break;
    }

    case R_ERROR:{		/* Report an error */
      cellpo msg = vars[ptn->v.i];

      reportErr(lineInfo(msg),"%U",strVal(msg));
      break;
    }

    case R_WARN:{		/* Report a warning */
      cellpo msg = vars[ptn->v.i];
      reportWarning(lineInfo(msg),"%U",strVal(msg));
      break;
    }

    case R_STACK:{		/* enter a sub-macro space */
      MacList sub;
      cellpo inp = Prev(input);

      sub.macros = ptn->v.mac;
      sub.previous = macros;
      macroCounter++;		/* A fix to get visting/revisiting behaviour */

      /* use previous input */
      macroReplace(inp,inp,env,&sub,depth,opts);

      break;
    }

    case R_DO:			/* process replacement again */
      input=vars[ptn->v.i];	/* pick up input */

      macroReplace(input,input,env,macros,depth,opts);
      stLineInfo(input,l_info);
      input=Next(input);
      break;

    default:			/* Should never happen */
      outStr(logFile,"Illegal macro code - system error\n");
      return False;
    }

    ptn=ptn->next;		/* step on to next macro command */
  }
  syserr("problem in macro_replace");
  return False;			/* Shouldnt ever get here ... */
}

void push_input(cellpo input, cellpo last, mac_po ptn) /* push a 'call' record */
{  
  if(top_input>=MAX_MAC_DEPTH)
    reportErr(lineInfo(input),"Macro pattern %w too deep",input);
  else{
    input_stack[top_input].input=input;
    input_stack[top_input].prev_push=cur_input;
    input_stack[top_input].last=last;
    input_stack[top_input].ptn=ptn;
    cur_input=top_input;
    top_input++;
  }
}

void push_choice(mac_po ptn, cellpo input, int rem) /* save choice point */
{
  if(top_choice>=MAX_MAC_CHOICE)
    reportErr(lineInfo(input),"Too many alternatives in %w",input);
  else if (rem>0) {
    choices[top_choice].choicept=ptn;
    choices[top_choice].input=input;
    choices[top_choice].cur_input=cur_input;
    choices[top_choice].top_input=top_input;
    choices[top_choice].remaining=rem;
    top_choice++;
  }
}

logical inspect_tpl(cellpo input)	/* Might we go into this tuple? */
{
  if(isTpl(input) && tplArity(input)>0)
    return True;
  else
    return False;
}

/* The top-level macro matching function ...
   Tries to find a part of the input which matches a macro pattern */
static logical match_input(cellpo orig_input,cellpo input,
			   macInvPo env,macListPo macros,int depth,optPo opts)
{
  symbpo topsymb=kvoid;

  if(depth>=MACRO_DEPTH_WARNING){
    reportErr(lineInfo(input),"probable circular macro definition %4w",input);
    return False;
  }
  else{
    if(whenVisited(input)>=macroCounter) /* This structure already looked at */
      return False;
    else
      visitCell(input,macroCounter);

    switch(tG(input)){		/* compute whether this bit of the input might fit */
    case intger:			
    case floating:		/* we dont allow number macros */
    case strng:		/* or string macros */
    case character:             // or character macros
      return False;	

    case symbolic:{
      symbpo s = symVal(input);

      if(s==kcall || s==kblock || s==ksemi)
	return False;		/* We dont allow this to be macroed ... */
      else
	return try_macro(input,input,s,False,env,macros,depth,opts);
    }

    case list:
      if(!isEmptyList(input)){
	cellpo inner = listVal(input);

	macroReplace(orig_input,inner,env,macros,depth+1,opts);
	  
	inner = Next(inner);

	macroReplace(orig_input,inner,env,macros,depth+1,opts);

	return try_macro(input,input,kcons,2,env,macros,depth,opts);
      }
      else
	return False;

    case tuple:{		/* we need a case analysis on the tuple */
      /* First, we go in to the tuple and try to macro replace the elements */
      if(inspect_tpl(input)){
	int i;
	int ar = tplArity(input);	/* how many elements in the tuple? */
	
	for(i=0;i<ar;i++){
	  cellpo arg = tplArg(input,i);

	  macroReplace(orig_input,arg,env,macros,depth+1,opts);
	}

	return try_macro(input,input,kcall,ar,env,macros,depth+1,opts);
      }
      else
	return False;		/* tuples not allowed as top-level */
    }

    case constructor:{		/* we need a case analysis on the constructor */
      /* First, we go in to the tuple and try to macro replace the elements */
      unsigned int i;
      unsigned int ar = consArity(input);	/* how many elements in the constructor? */
      cellpo f = consFn(input);
	
      for(i=0;i<ar;i++){
	cellpo arg = consEl(input,i);

        macroReplace(orig_input,arg,env,macros,depth+1,opts);
      }
      
      if(isSymb(f))
        topsymb = symVal(f);
      else
        topsymb = kblock;
        
      return try_macro(input,input,topsymb,True,env,macros,depth,opts);
    }

    default:
      return False;		/* Nothing doing here... */
    }
  }
  syserr("problem 2093");
  return False;			/* Shouldnt ever get here */
}

/**************************************************************************/
/*                     Macro replacement                                  */
/**************************************************************************/

/* Function to replicate a parse structure
 * Using an external explicit stack of pointers
 */

/* Replicate a parse structure as necessary */
static cellpo repli_data(cellpo src, cellpo trg, logical *found)
{
  if(src==trg){			/* Its here! */
    *found = True;
    p_sym(kstar);		/* push the marker */
    return trg;
  }
  else if(isNonEmptyList(src)){
    cellpo mark = topStack();
    cellpo lst = listVal(src);	/* Look in the list pair */
    cellpo x=NULL;		/* marks the spot */
    int ar = 2;

    while(!*found && ar--){
      x = repli_data(lst,trg,found);
      lst=Next(lst);
    }

    if(!*found){		/* replace list pair by original pointer */
      reset_stack(mark);
      p_cell(src);
      return NULL;
    }
    else{			/* we have to copy the list pair */
      long info = lineInfo(src);

      if(ar)
	p_cell(lst);		/* Push tail if nec. */

      p_list();			/* Make a list pair */
      stLineInfo(topStack(),info);
      
      if(x==lst)		/* was it this one? if so adjust */
	x = listVal(topStack());
      else if(x==Next(lst))
	x = Next(listVal(topStack()));

      return x;			/* return the interior pointer */
    }
  }
  else if(isTpl(src)){
    int ar = tplArity(src);	/* how big is this tuple? */
    int i;
    cellpo x=NULL;
    cellpo tplmrk = topStack(); /* start copying in the tuple */
    long info = lineInfo(src);

    for(i=0;i<ar && !*found;i++)
      x = repli_data(tplArg(src,i),trg,found);

    if(!*found){		/* replace whole tuple by original pointer */
      reset_stack(tplmrk);
      p_cell(src);
      return NULL;
    }
    else{			/* we have to copy the rest of the tuple */
      int j = i;
      
      for(j=i;j<ar;j++)
        p_cell(tplArg(src,j));/* Push remaining elements */

      clstple(tplmrk);		/* close off the copied tuple */
      stLineInfo(topStack(),info);

      if(x==tplArg(src,i-1))	/* was it this one? */
	x=tplArg(topStack(),i-1); /* adjust to reflect new copy */

      return x;			/* return the interior pointer */
    }
  }
  else if(isCons(src)){
    unsigned int ar = consArity(src);	/* how big is this tuple? */
    unsigned int i;
    cellpo x=NULL;
    cellpo tplmrk = topStack(); /* start copying in the tuple */
    long info = lineInfo(src);
    
    p_cell(consFn(src));

    for(i=0;i<ar && !*found;i++)
      x = repli_data(consEl(src,i),trg,found);

    if(!*found){		/* replace whole tuple by original pointer */
      reset_stack(tplmrk);
      p_cell(src);
      return NULL;
    }
    else{			/* we have to copy the rest of the tuple */
      unsigned int j = i;
      
      for(j=i;j<ar;j++)
        p_cell(consEl(src,j));/* Push remaining elements */
        
      p_cons(ar);               // Construct the constructor
      stLineInfo(topStack(),info);

      if(x==consEl(src,i-1))	/* was it this one? */
	x=consEl(topStack(),i-1); /* adjust to reflect new copy */

      return x;			/* return the interior pointer */
    }
  }
  else{
    p_cell(src);
    return NULL;
  }
}

static cellpo replycate(cellpo input, cellpo src, cellpo trg)
{
  logical found = False;
  cellpo point = repli_data(src,trg,&found);

  popcell(input);		/* copy the top-level out */

  if(src==trg)			/* special case */
    return input;
  else
    return point;		/* return where to insert */
}

static void copy(cellpo src,cellpo dest)
{
  switch(tG(src)){
  case list:
    if(isNonEmptyList(src)){
      cellpo lst = listVal(src); /* Look in the list pair */
      cellpo pair = allocPair();
      long info = lineInfo(src);

      mkList(dest,pair);
      copy(lst,pair);
      copy(Next(lst),Next(pair));
      stLineInfo(dest,info);
    }
    else
      copyCell(dest,src);
      return;
  case constructor:{
    unsigned int ar = consArity(src);	/* how big is this constructor? */
    cellpo tpl = allocCons(ar);
    unsigned int i;
    long info = lineInfo(src);

    mkCons(dest,tpl);
    
    copy(consFn(src),consFn(dest));

    for(i=0;i<ar;i++)
      copy(consEl(src,i),consEl(dest,i));
    stLineInfo(dest,info);
    return;
  }
  case tuple:{
    int ar = tplArity(src);	/* how big is this tuple? */
    cellpo tpl = allocTuple(ar);
    int i;
    long info = lineInfo(src);

    mkTpl(dest,tpl);

    for(i=0;i<ar;i++)
      copy(tplArg(src,i),tplArg(dest,i));
    stLineInfo(dest,info);
    return;
  }
  default:
    copyCell(dest,src);
    return;
  }
}

static void join_cells(cellpo c1,cellpo c2,cellpo dest)
{
  uniChar sym[256];		/* temporary place to hold string */
  uniChar sym2[256];
  cellpo t1=NULL,t2=NULL;	/* Tuple/list concatenation ... */
  
  switch(tG(c1)){
  case intger:
    strMsg(sym,NumberOf(sym),"%d",intVal(c1));
    break;
  case symbolic:
    strMsg(sym,NumberOf(sym),"%U",symVal(c1));
    break;
  case character:
    strMsg(sym,NumberOf(sym),"%c",CharVal(c1));
    break;
  case strng:
    strMsg(sym,NumberOf(sym),"%U",strVal(c1));
    break;
  case floating:
    strMsg(sym,NumberOf(sym),"%d",(long)fltVal(c1));
    break;
  case tuple:
  case list:
    t1=c1;
    break;
  default:
    outStr(logFile,"Error in macro concatenation\n");
    sym[0]='\0';
    break;
  }
  switch(tG(c2)){
  case intger:
    strMsg(sym2,NumberOf(sym2),"%d",intVal(c2));
    break;
  case symbolic:
    strMsg(sym,NumberOf(sym),"%U",symVal(c2));
    break;
  case character:
    strMsg(sym,NumberOf(sym),"%c",CharVal(c2));
    break;
  case strng:
    strMsg(sym,NumberOf(sym),"%U",strVal(c2));
    break;
  case floating:
    strMsg(sym2,NumberOf(sym2),"%d",(long)fltVal(c2));
    break;
  case tuple:
  case list:
    t2 = c2;
    break;
  default:
    outStr(logFile,"Error in macro concatenation\n");
    sym2[0]='\0';
  }

  if(t1==NULL && t2==NULL){
    uniCat(sym,NumberOf(sym),sym2);
    mkSymb(dest,locateU(sym));
  }

  else if(t1!=NULL && t2!=NULL){
    if(isTpl(t1) && isTpl(t2))
      JoinTuples(t1,t2,dest);
    else if(isTpl(t1) && IsList(t2)){ /* stick a list to end of tuple */
      long ar1 = tplArity(t1);
      long ar2 = 0;
      cellpo lst = t2;
      cellpo tp;
      long i;

      while(isNonEmptyList(lst)){
	lst = Next(listVal(lst));
	ar2++;
      }

      tp = allocTuple(ar1+ar2);

      mkTpl(dest,tp);

      for(i=0;i<ar1;i++)
        copyCell(tplArg(dest,i),tplArg(t1,i));

      lst = t2;
      while(isNonEmptyList(lst)){
	lst = listVal(lst);
	copyCell(tplArg(dest,i++),lst);
	lst=Next(lst);
      }
    }
    else if(IsList(t1) && isTpl(t2)){
      long ar2 = tplArity(t2);
      long ar1 = 0;
      cellpo lst = t1;
      cellpo tp;
      long i=0;

      while(isNonEmptyList(lst)){
	lst = Next(listVal(lst));
	ar1++;
      }

      tp = allocTuple(ar1+ar2);

      mkTpl(dest,tp);

      lst = t1;
      while(isNonEmptyList(lst)){
	lst = listVal(lst);
	copyCell(tp=Next(tp),lst);
	lst=Next(lst);
      }

      for(i=0;i<ar1;i++)
        copyCell(tp=Next(tp),tplArg(t2,i));
    }
    else if(IsList(t1) && IsList(t2)) {
      while(isNonEmptyList(t1)){
	cellpo pair=allocPair();

	t1 = listVal(t1);
	mkList(dest,pair);
	copyCell(pair,t1);
	t1=Next(t1);
	dest=Next(pair);
      }

      copyCell(dest,t2);
    }
    else{
      outStr(logFile,"Error in macro concatenation\n");
      mkSymb(dest,kvoid);
    }
  }
}


/* Attempt to match input data against one of the available macro defs */
cellpo macro_replace(cellpo input,optPo opts)
{
  MacList globalMac;

  globalMac.macros = mac_top;
  globalMac.previous = NULL;

  input = macroReplace(input,input,NULL,&globalMac,0,opts);

  mac_top = globalMac.macros;	/* enable any global macros */

  return input;			/* If we get here - no macros applied */
}

static cellpo macroReplace(cellpo orig,cellpo input,macInvPo env,
			   macListPo macTop,int depth,optPo opts)
{
  if(depth>=MACRO_DEPTH_WARNING){
    reportErr(lineInfo(input),"probable circular macro definition %4w",input);
    return input;
  }
  else{
    while(match_input(orig,input,env,macTop,depth,opts))
      ;
  }
  return input;
}

void print_mac(ioPo f, char* msg, macdefpo mc)
{
  outStr(f,msg);
  outStr(f,"macro pattern: ");
  prt_mac_seq(f,mc->pattern);
  outChar(f,'\n');
}

void prt_mac_seq(ioPo f, mac_po seq)
{
  outChar(f,'[');
  while(seq){
    prt_macro(f,seq);
	seq=seq->next;
  }
  outChar(f,']');
}

static void prt_macro(ioPo f, mac_po mac)
{

  if(mac){
    switch(mac->code){
    case M_INT:		/* general integer */
      outMsg(f," int[%d]",intVal(mac->v.vl));
      break;

    case M_SYM:		/* symbol */
      outMsg(f," sym[%U]",symVal(mac->v.vl));
      break;
      
    case M_CHR:         // Character
      outMsg(f," chr[%c]",mac->v.i);
      break;

    case M_STR:{		/* string */
      outMsg(f," str[%U]",strVal(mac->v.vl));
      break;
    }

    case M_TAG:			/* define variable */
      outMsg(f," var[%d]",mac->v.i);
      break;

    case M_REF:			/* reference to other vars */
      outMsg(f," ref[%d]",mac->v.i);
      break;

    case M_DEEP:		/* Deep variable reference */
      outMsg(f," deep[%d,%d]",mac->v.d.depth,mac->v.d.off);
      break;

    case M_FLT:			/* floating point number */
      outMsg(f," flt[%f]",fltVal(mac->v.vl));
      break;

    case M_NIL:
      outStr(f," nil[]");
      break;

    case M_LIST:
      outStr(f," list[]");
      break;

    case M_EOL:
      outStr(f," eol[]");
      break;

    case M_TPL:			/* tuple */
      outMsg(f," tpl[%d]",mac->v.i);
      break;

    case M_CONS:		/* constructor */
      outMsg(f," cons[%d]",mac->v.i);
      break;

    case M_TOP:
      outMsg(f," top[%d]",mac->v.i);
      break;

    case M_SKP:		/* Can be anything */
      outStr(f," skip[]");
      break;

    case M_AINT:		/* Any integer */
      outStr(f," any-int[]");
      break;

    case M_AFLT:		/* Any floating point */ 
      outStr(f," any-flt[]");
      break;

    case M_ANUM:		/* Any number */
      outStr(f," any-num[]");
      break;

    case M_ASYM:		/* Any symbol */
      outStr(f," any-sym[]");
      break;
      
    case M_QUOT:                // A quoted symbol
      outStr(f," quoted[]");
      break;

    case M_ACHR:		/* Any character */
      outStr(f," any-char[]");
      break;

    case M_ASTR:		/* Any string */
      outStr(f," any-str[]");
      break;

    case M_ALST:		/* Any list */
      outStr(f," any-lst[]");
      break;

    case M_ATPL:		/* Any tuple */
      outStr(f," any-tple[]");
      break;

    case M_STPL:		/* Sub-tuple */
      outStr(f," sub-tple[]");
      break;

    case M_NXL:		/* End matching tuple element */
      outStr(f," nxl[]");
      break;

    case M_CNT:		/* Contained search */
      outMsg(f," cnt[%d]",mac->v.i);
      break;

    case M_SUB:		/* Look for a sub-pattern - try whole of input*/
      outMsg(f," sub[%d]",mac->v.i);
      break;

    case M_ALT:{		/* Alternatives */
      outMsg(f," alt[0x%x]",(long)mac->v.alt);
      break;
    }

    case M_CUT:{		/* Prolog-style cut */
      outStr(f," cut[]");
      break;
    }

    case M_FAIL:{		/* Fail this branch */
      outStr(f," fail[]");
      break;
    }

    case M_BRK:{		/* enter break point */
      outStr(f," break[]");
      break;
    }

    case M_JMP:{		/* Jump to other code */
      outMsg(f," jmp[0x%x]",(long)mac->v.alt);
      break;
    }

    case M_RTN:
      outStr(f," rtn[]");
      break;

    case M_NONE:
      outMsg(f," 0x%x: none[]",(long)mac);
      break;

    case R_INIT:		/* Initiate replacement */
      outMsg(f," r_init[%d]",mac->v.i);
      break;

    case R_INT:			/* general integer */
      outMsg(f," r_int[%d]",intVal(mac->v.vl));
      break;

    case R_SYM:			/* symbol */
      outMsg(f," r_sym[%U]",symVal(mac->v.vl));
      break;

    case R_CHR:			/* symbol */
      outMsg(f," r_chr[%U]",symVal(mac->v.vl));
      break;

    case R_STR:{		/* string */
      outMsg(f," r_str[%U]",strVal(mac->v.vl));
      break;
    }

    case R_TAG:			/* define variable */
      outMsg(f," r_tag[%d]",mac->v.i);
      break;

    case R_REF:			/* reference to other vars */
      outMsg(f," r_ref[%d]",mac->v.i);
      break;

    case R_DEEP:		/* Deep variable reference */
      outMsg(f," r_deep[%d,%d]",mac->v.d.depth,mac->v.d.off);
      break;

    case R_TMP:		/* set up macro var in replacement */
      outMsg(f," r_tmp[%d]",mac->v.i);
      break;

    case R_FLT:			/* floating point number */
      outMsg(f," r_flt[%f]",fltVal(mac->v.vl));
      break;

    case R_NIL:
      outStr(f," r_nil[]");
      break;

    case R_LIST:
      outStr(f," r_list[]");
      break;

    case R_TPL:			/* tuple */
      outMsg(f," r_tpl[%d]",mac->v.i);
      break;

    case R_CONS:		/* constructor */
      outMsg(f," r_cons[%d]",mac->v.i);
      break;

    case R_TOP:
      outMsg(f," r_top[%d]",mac->v.i);
      break;

    case R_NXT:
      outMsg(f," r_nxt[%d]",mac->v.i);
      break;

    case R_SKP:		/* Can be anything */
      outStr(f," skip[]");
      break;

    case R_CNT:		/* Contained search */
      outMsg(f," r_cnt[%d]",mac->v.i);
      break;

    case R_DEF:
      outMsg(f," r_def[%d]",mac->v.i);
      break;

    case R_MAC:
      outMsg(f," r_hsh[%U]",symVal(mac->v.vl));
      break;

    case R_CAT:
      outMsg(f," r_cat[%d]",mac->v.i);
      break;

    case R_LIST2TUPLE:
      outMsg(f," r_list2tuple[%d]",mac->v.i);
      break;

    case R_TUPLE2LIST:
      outMsg(f," r_tuple2list[%d]",mac->v.i);
      break;

    case R_ECHO:		/* echo a message */
      outMsg(f," r_echo[%d]",mac->v.i);
      break;

    case R_INCLUDE:		/* include subsiduary file */
      outMsg(f," r_include[%d]",mac->v.i);
      break;

    case R_ERROR:		/* echo a error message */
      outMsg(f," r_error[%d]",mac->v.i);
      break;

    case R_WARN:		/* echo a warning message */
      outMsg(f," r_warn[%d]",mac->v.i);
      break;

    case R_STACK:		/* sub-macro context */
      outMsg(f," r_stack[]");
      break;

    case R_DO:			/* re-enter macro processor */
      outMsg(f," r_do[%d]",mac->v.i);
      break;

    case R_FILE:		/* construct a file name */
      outMsg(f," r_file[%d,%d]",mac->v.d.depth,mac->v.d.off);
      break;

    default:			/* Should never happen */
      outStr(f," Illegal macro code");
    }
  }
}

void dumpMacros(macdefpo mc)
{
  while(mc!=NULL){
    outStr(logFile,"Macro pattern: ");
    prt_mac_seq(logFile,mc->pattern);
    outStr(logFile,"\n");
    mc = mc->prev;
  }
}

static void clearMacro(mac_po ins)
{
  while(ins!=NULL){
    mac_po next = ins->next;

    if(ins->code==R_STACK)
      clearMacros(ins->v.mac);

    freePool(macpool,ins);
    ins=next;			/* step on to next macro command */
  }
}

macdefpo clearMacros(macdefpo def)	/* clear down the macros for the next source file */
{
  while(def!=NULL){
    macdefpo prev = def->prev;
    
    clearMacro(def->pattern);
    freePool(defpool,def);
    def = prev;
  }
  return def;
}


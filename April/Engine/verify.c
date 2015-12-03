/*
  Code verifier module -- check that a code segment satisfies:
  (c) 1994-1999 Imperial College and F.G. McCabe

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
#include <assert.h>
#include <string.h>
#include "april.h"
#include "types.h"
#include "opcodes.h"
#include "astring.h"
#include "pool.h"
#include "debug.h"

#ifdef VERIFYTRACE
logical traceVerify = False;	/* True if we want to allowtracing of verify */
#endif

typedef struct {
  logical inited;		/* True if cell has real value */
  logical read;			/* Has this cell been read? */
} Var, *varpo;

#define MAXVAR 255		/* Maximum number of locals & arguments */
#define LOCAL 128		/* Base index into locals */
#define MAXPAR 123		/* Maximum number of parameters */
#define MAXCODE 1024		/* Maximum number of code segments */

#define MIN(a,b) ((a)<(b)?(a):(b))

#define relpc(PC) ((*(PC)&0xffff)<<16>>16)

#ifdef VERIFYTRACE
static char *showEnv(varpo fp,int ar,int limit);
#endif

#define check_inited(fp,ar,limit,off)\
  { char *stat = _inited_check(fp,ar,limit,off);\
    if(stat!=NULL)\
      return stat;\
  }

#define check_arity(fp,arity,args,depth,limit)\
{\
  int i = 0;\
  int ar = arity;\
  if(ar<0 || ar>MAXPAR)\
    return "invalid arity to call";\
\
  for(i=0;i<ar;i++){\
    char *stat = _inited_check(fp,args,limit,depth+i);\
    if(stat!=NULL)\
      return stat;\
  }\
}

#define check_env(free,off)\
{ if((off)>(free))\
    return "invalid access to environment";\
}

static char *_inited_check(varpo fp,int ar,int limit,int off)
{
  if(off>=0 && (off>ar+3 || off<3))
    return "illegal access to parameter";
  if(off<0 && off<limit)
    return "illegal access to local variable";
  else if(!fp[off].inited)
    return "accessing non-initialized variable";
  else{
    fp[off].read = True;
    return NULL;
  }
}

#define set_inited(fp,ar,limit,f)\
  { char *stat = _inited_set(fp,ar,limit,f);\
    if(stat!=NULL)\
      return stat;\
  }

static char *_inited_set(varpo fp,int ar,int limit,int off)
{
  if(off>ar+3 || (off>=0 && off<3) || off<limit)
    return "assigning to illegal location";
  else
    fp[off].inited = True;
  return NULL;
}

#define check_depth(fp,limit,f)\
  { char *stat = _depth_check(fp,limit,f);\
    if(stat!=NULL)\
      return stat;\
  } 

static char *_depth_check(varpo fp,int limit,int depth)
{
  if(depth>0)
    return "illegal depth";
  else if(depth<limit)
    return "depth lower than specified limit";
  else{
    int i=depth-1;
    while(i>=limit){
      fp[i].read = False;
      fp[i--].inited = False;
    }
    for(i=depth;i<0;i++)
      if(!fp[i].inited)
	return "Unsafe local variable";
    return NULL;
  }
}

static void set_depth(varpo fp,int limit,int depth)
{
  int i=depth-1;
  while(i>=limit){
    fp[i].read = False;
    fp[i--].inited = False;
  }
  for(i=depth;i<0;i++)
    fp[i].inited = True;
}

#define check_lit(cnt,lit)\
  if(lit>cnt)\
    return "invalid literal reference";

#define check_string(lits,cnt,lit) \
{\
  if(lit>cnt)\
    return "invalid literal reference";\
  else if(!isListOfChars(lits[lit]))\
    return "literal should be a string";\
}

#define check_symbol(lits,cnt,lit) \
{\
  if(lit>cnt)\
    return "invalid literal reference";\
  else if(!isSymb(lits[lit]))\
    return "literal should be a symbol";\
}


#define check_integer(lits,cnt,lit) \
{\
  if(lit<lits || lit>lits+cnt)\
    return "invalid literal reference";\
  else if(!IsInteger(*lit))\
    return "literal should be an integer";\
}

#define check_float(lits,cnt,lit) \
{\
  if(lit<0)\
    return "negative literal reference";\
  else if(lit>cnt)			\
    return "invalid reference to non-existent float";\
  else if(!IsFloat(lits[lit]))\
    return "literal should be a floating point";\
}

#define check_handle(lits,cnt,lit) \
{\
  if(lit<lits || lit>lits+cnt)\
    return "invalid literal reference";\
  else if(!IsHandle(*lit))\
    return "literal should be a handle";\
}

#define check_single(pc)\
   if(_single_check(pc))\
      return "skipped instruction too long";

#define check_jump(start,end,pc)\
  if(pc<start || pc>end)\
    return "illegal destination of branch";

/* Which instructions use more than one instruction word? */
static logical _single_check(insPo pc)
{
  WORD32 op=op_cde(*pc);

  return op==line_d;
}

/* External decl ... */
retCode privileged_escape(int escape,entrytype *mode,int *arity);

typedef struct _segment_ *segPo;

typedef struct _segment_ {
  Var variables[MAXVAR];	/* entry state for this segment */
  int depth;
  logical checked;
  insPo pc;
  WORD32 length;
  segPo prev;			/* previous physical segment */
  segPo next;			/* next physical segment */
  segPo alt;
  WORD32 entrypoints;		/* how many entry points are there here? */
} segRecord;

typedef struct {
  segPo *table;			/* vector of ptrs into the list of segments */
  int top;			/* current top of the vector */
  int size;			/* current size of the vector */
} segVect, *segVectPo;

static poolPo segpool = NULL;

static segPo initVerify(void)
{
  if(segpool==NULL)
    segpool = newPool(sizeof(segRecord),16);

  return (segPo)allocPool(segpool);
}

static segVect initVect(int max,segPo whole)
{
  segVect init = {(segPo*)malloc(sizeof(segPo)*max),1,max};

  init.table[0] = whole;
  return init;
}

static void closeVect(segVectPo vect)
{
  free(vect->table);
}

static segPo searchVect(segVectPo vect,insPo pc)
{
  int beg = 0;
  int end = vect->top;

  while(end>beg){
    int mid = beg+(end-beg)/2;	/* split down the middle */
    segPo p = vect->table[mid];

    if(p->pc<=pc && p->pc+p->length>pc)
      return p;
    else if(p->pc>pc)		/* we are too high */
      end = mid;
    else if(p->pc+p->length<=pc) /* we are too low */
      beg = mid;
  }
  return NULL;
}

static void extendVect(segVectPo vect,insPo oldpc,insPo newpc,
		       int inc,WORD32 *count)
{
  int beg = 0;
  int end = vect->top;
  int mid = 0;
  segPo p = NULL;

  /* locateC the segment to split in the table */
  while(end>beg){
    mid = beg+(end-beg)/2;		/* split down the middle */
    p = vect->table[mid];

    if(p->pc<=newpc && p->pc+p->length>newpc)
      break;
    else if(p->pc>newpc)	/* we are too high */
      end = mid;
    else if(p->pc+p->length<=newpc) /* we are too low */
      beg = mid;
  }

  if(newpc!=p->pc){		/* we have to split a segment ... */
    segPo new = (segPo)allocPool(segpool);

    assert(newpc>0 && p!=NULL && newpc<p->pc+p->length);

    new->pc = newpc;
    new->length = p->length-(newpc-p->pc);
    new->next = p->next;
    new->prev = p;
    if(newpc<oldpc)
      new->entrypoints = inc+1;	/* we are splitting an old segment */
    else
      new->entrypoints = inc;
    new->alt = p->alt;
    p->alt = NULL;
    new->depth = 1;
    p->length = newpc-p->pc;
    p->next->prev = new;
    p->next = new;
    (*count)++;

    /* Add a new entry into the table */
    if(vect->top>=vect->size){
      int nmax = vect->size+(vect->size>>1);
      vect->table = (segPo*)realloc(vect->table,nmax*sizeof(segPo));
      vect->size = nmax;

      if(vect->table==NULL)
	syserr("unable to grow vector table in code verify");
    }

    /* shuffle the top half of the table */
    {
      int i;
      for(i=vect->top;i>mid+1;i--)
	vect->table[i] = vect->table[i-1];
    }
     
    vect->top++;
    vect->table[mid+1] = new;
  }
  else
    p->entrypoints+=inc;	/* increment number of entry points into segment */

}

#ifdef VERIFYTRACE_
static void dumpSegs(segPo base)
{
  segPo p = base;
  int seg = 1;
  do{
    outMsg(logFile,"Segment %d 0x%x..0x%x->0x%x (%d/%d)\n",seg++,
	   p->pc-base->pc,p->pc+p->length-base->pc-1,
	   p->alt!=NULL?p->alt->pc-base->pc:0,p->entrypoints,p->depth);
    p = p->next;
  } while(p!=base);
  flushFile(logFile);
}
#endif

static segPo splitPhase(insPo code,WORD32 length,WORD32 *count)
{
  segPo whole = initVerify();
  segPo current = whole;
  segPo p;
  insPo pc = code;
  WORD32 pcx;
  logical endJump = False;
  logical condIns = False;
  segVect vect = initVect(64,whole);

  whole->next = whole->prev = whole;
  whole->pc = pc;
  whole->length = length;
  whole->entrypoints = 1;	/* one initial entrypoint */
  whole->depth = 1;
  whole->alt = NULL;

  (*count)=1;

  while(pc<code+length){
    if(pc>=current->pc+current->length){
      current=current->next;
      if(!endJump)
	current->entrypoints++;
    }

    endJump = False;

    switch(op_cde((pcx=*pc++))){
    case halt:
    case generr:
    case result:
    case ret:
    case die:{
      if(pc<code+length && !condIns)
	extendVect(&vect,pc,pc,0,count);

      endJump = !condIns;
      break;
    }

    case jmp:{			/* Jump to new location */
      insPo newpc = pc+op_lo_val(pcx);
      extendVect(&vect,pc,newpc,1,count);

      if(pc<code+length){
	extendVect(&vect,pc,pc,!condIns,count);

	if((p=searchVect(&vect,pc-1))==NULL)
	  syserr("problem in verify");
	else{
	  assert(p->alt == NULL);
	  p->alt = searchVect(&vect,newpc);
	}
      }
      else
	current->alt = searchVect(&vect,newpc);
	
      endJump = !condIns;
      break;
    }


    case ijmp:			/* indirect jump */
    case hjmp:			/* hash-table jump */
    case tjmp:			/* tag table jump */
				/* treated as conditional instructions */

    case mlit:			/* match against a literal */
    case mfloat:		/* match against a floating-point */
    case mchar:                 // Match a character
    case mlist:			/* Access and match head of non-empty list */
    case prefix:                // Check and step over list prefix
    case snip:                  // Snip off a prefix off a list
    case mcons:		        // Match constructor term
    case mtpl:			/* Match against tuple */
    case mcnsfun:               // match the constructor's function symbol
    case mhdl:			/* Match against literal handle */
    case many:			/* Match an ANY value */
    case uvar:                  // Type unification with variable
    case ulit:                  // Type unification with literal
    case utpl:                  // Unify with tuple
    case ucns:                  // Unify against constructor
    case eq:			/* Test two elements for equality */
    case neq:			/* Test two elements for inequality */
    case le:			/* Test two elements for less than */
    case gt:			/* Test two elements for gt than */
    case eqq:			/* Test two elements for pointer equality */
    case neqq:			/* Test two elements for pointer inequality */
    case feq:			/* Test two numbers for equality */
    case fgt:			/* Test two numbers for gt than */
    case fle:			/* Test two numbers for less than */
    case ieq:			/* Test two integers for equality */
    case ineq:			/* Test two integers for inequality */
    case ile:			/* Test two integers for less than */
    case igt:			/* Test two integers for gt than */
      condIns = True;		/* The following instruction may be executed */
      continue;
      
    case mnil:			/* Match empty list */
    case isvr:                  // Test for a variable
    case errblk:		/* start a new error block */
    case debug_d:		/* test to see if we are debugging */
    case iftrue:		/* does a var contain true? */
    case iffalse:{		/* does a var not contain true? */
      insPo newpc = pc+op_so_val(pcx);
      extendVect(&vect,pc,newpc,1,count);
      extendVect(&vect,pc,pc,0,count);

      if((p=searchVect(&vect,pc-1))==NULL)
	syserr("problem in verify");
      else{
	assert(p->alt == NULL);
	p->alt = searchVect(&vect,newpc);
      }
	
      break;
    }

    case line_d:		/* report on file and line number */
      pc++;
      break;

    default:
      break;
    }
    condIns = False;
  }

#ifdef VERIFYTRACE
  if(traceVerify){
    int i;

    for(i=0;i<vect.top;i++){
      if(i>0){
	assert(vect.table[i-1]->next == vect.table[i]);
      }
      if(i<vect.top-1){
	assert(vect.table[i+1]->prev==vect.table[i]);
	assert(vect.table[i+1]->pc==vect.table[i]->pc+vect.table[i]->length);
      }
    }
//    dumpSegs(whole);
  }
#endif

  closeVect(&vect);		/* close down the search vector */
  return whole;
}

static void clearSegments(segPo base)
{
  segPo p = base;

  do{
    segPo next = p->next;
    freePool(segpool,p);
    p = next;
  }while(p!=base);
}

/* merge in the results of a segment verification */
static char * mergeVariables(varpo vars,int depth,segPo next)
{
  int i;

  if(next->depth>0){
    for(i=0;i<MAXVAR;i++)
      next->variables[i] = vars[i];
    next->depth = depth;
    return NULL;
  }
  else{
    if(next->depth>depth)
      return "inconsistent depth in instruction sequence";

    for(i=0;i<MAXVAR;i++)
      if(next->checked){
	if(next->variables[i].inited && next->variables[i].read &&
	   !vars[i].inited){
#ifdef VERIFYTRACE
	  if(traceVerify)
	    outMsg(logFile,"fp[%d] read but not inited\n",i-LOCAL);
#else
	  return NULL;
#endif
	}
      }
      else
	next->variables[i].inited &= vars[i].inited;

    return NULL;
  }
}

static char *verifySegment(segPo base,segPo seg,int limit,
			   insPo start,insPo end_code,
			   objPo *literals,unsigned int litcnt,
			   int ar,int free,entrytype form,logical allow_priv)
{
  int i;
  insPo pc = seg->pc;		/* Starting point of real code */
  insPo end = seg->pc+seg->length;
  WORD32 pcx;
  static Var vars[MAXVAR];		/* Local copy of the variables */
  varpo fp = &vars[LOCAL];
  logical endJump = False;
  logical condIns = False;

  limit = seg->depth;

  seg->checked = True;

  for(i=0;i<MAXVAR;i++){
    vars[i].inited=seg->variables[i].inited;
    vars[i].read=False;
  }

#ifdef VERIFYTRACE
  if(traceVerify){
    outMsg(logFile,"Check segment: 0x%x..0x%x ",seg->pc-base->pc,
	   seg->pc+seg->length-1-base->pc);
    outMsg(logFile,"frame: %s\n",showEnv(fp,ar,limit));
  }
#endif

  for(;pc<end;){
#ifdef VERIFYTRACE
    if(traceVerify){
      dissass(pc,base->pc,NULL,NULL,literals);
      outMsg(logFile,"\n");
      flushFile(logFile);
    }
#endif
    endJump = False;

    switch(op_cde((pcx=*pc++))){
    case halt:
      if(condIns)
	endJump = !condIns;	/* this may result in a jump */
      break;
				/* Move instructions */
    case movl:			/* Put a literal cell into a variable */
      check_lit(litcnt,op_o_val(pcx)); /* Valid literal? */
      set_inited(fp,ar,limit,op_sh_val(pcx)); /* Check assignment */
      break;
    
    case move:			/* Local - local move */
      check_inited(fp,ar,limit,op_sm_val(pcx)); /* Inited reference? */
      set_inited(fp,ar,limit,op_sl_val(pcx)); /* Check assignment */
      break;

    case emove:			/* move from environment */
      check_env(free,op_sm_val(pcx)); /* Check reference to environment */
      set_inited(fp,ar,limit,op_sl_val(pcx)); /* Check assignment */
      break;

    case stoe:			/* store environment vector */
      set_inited(fp,ar,limit,op_sl_val(pcx)); /* Check assignment */
      break;

    case loade:			/* load environment vector */
      check_inited(fp,ar,limit,op_sl_val(pcx)); /* Check assignment */
      break;

    case jmp:			/* Jump to new location */
      check_jump(start,end_code,pc+op_lo_val(pcx));
      endJump = !condIns;
      break;

    case ijmp:
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_jump(start,end_code,pc+op_so_val(pcx));
      endJump = True;
      break;

    case hjmp:{			/* hash-table jump */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      endJump = True;
      break;
    }

    case tjmp:{			/* tag table jump */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      endJump = True;
      break;
    }

    case esc_fun:{		/* escape into 1st level builtins */
      int escape = op_o_val(pcx);
      entrytype mode;
      int esc_arity;
      int depth;

      switch(privileged_escape(escape,&mode,&esc_arity)){
      case Ok:
	if(!allow_priv)
	  return "privileged escape function";
	else break;
      case Error:
	return "undefined escape function";
      case Fail:
	break;
      default:
	return "problem with privileged escape";
      }

      check_arity(fp,esc_arity,ar,op_sh_val(pcx),limit);
      check_depth(fp,limit,op_sh_val(pcx)); /* Validate stack depth */
      
      if(mode==function){
	depth = op_sh_val(pcx)+esc_arity-1; /* depth after escape call */
	set_inited(fp,ar,limit,depth); /* Record assignment */
      }
      else
	depth = op_sh_val(pcx);
	
      set_depth(fp,limit,depth);
      break;
    }

    case call:			/* Call a local procedure */
      check_inited(fp,ar,limit,op_sl_val(pcx)); /* Inited reference? */
      check_depth(fp,limit,op_sh_val(pcx)); /* Validate stack depth */
      check_arity(fp,op_m_val(pcx),ar,op_sh_val(pcx),limit); /* Check arity of call */
      set_depth(fp,limit,op_sh_val(pcx)+op_m_val(pcx)-1); /* Reset stack depth */
      break;

    case ecall:			/* Call a procedure from env */
      check_env(free,op_sl_val(pcx)); /* Check reference to environment */
      check_depth(fp,limit,op_sh_val(pcx)); /* Check stack depth */
      check_arity(fp,op_sm_val(pcx),ar,op_sh_val(pcx),limit); /* Check arity of call */
      set_depth(fp,limit,op_sh_val(pcx)+op_m_val(pcx)-1); /* Reset stack depth */
      break;

    case allocv:		/* Establish a new frame pointer */
      limit = op_so_val(pcx);
      if(limit>0 || limit<-LOCAL)
	return "invalid allocv instruction";
      for(i=-1;i>=limit;i--)
	fp[i].inited = False;
      break;

    case initv:		/* Initialize a variable */
      set_inited(fp,ar,limit,op_sl_val(pcx)); /* Check assignment */
      break;

    case ret:			/* Return from a procedure or pattern */
      if(form==function)
	return "incorrect return from function";
      endJump = !condIns;
      break;
      
    case result:		/* Return from a function */
      endJump = !condIns;
      switch(form){
      case procedure:
	return "incorrect return from procedure";
      case function:
	check_inited(fp,ar,limit,op_sl_val(pcx));
	if(op_sm_val(pcx)>ar || op_sm_val(pcx)<0)
	  return "invalid return value offset from function";
	else
	  return NULL;
      case invalid_code:
	return "invalid code";
      }
      break;
    
    case die:			/* kill current sub-process */
      endJump = !condIns;
      break;
      
      /* Individual match instructions */
    case mlit:			/* match against a literal */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_lit(litcnt,op_o_val(pcx)); /* Valid literal? */
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;
    
    case mfloat:		/* match against a floating-point */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_float(literals,litcnt,op_o_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case mnil:			/* Match empty list */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_jump(start,end_code,pc+op_so_val(pcx));
      break;
      
    case mchar:                 // Match a character
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      break;

    case mlist:			/* Access and match head of non-empty list */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      set_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;
      
    case prefix:		/* Check and adjust for a prefix list */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;
      
    case snip:		        /* snip off a fixed prefix */
      check_depth(fp,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;
      
    case mcons:			/* Match against constructor */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case mcnsfun:		/* match against constructor's function symbol */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_lit(litcnt,op_o_val(pcx)); /* Valid literal? */
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;
    
    case mtpl:			/* Match against tuple */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      if(op_so_val(pcx)<0)
	return "invalid tuple test";
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case mhdl:			/* Match against literal handle */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_handle(literals,litcnt,(objPo*)(pc+op_so_val(pcx)));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case many:			/* Match a particular any value */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      set_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      check_single(pc);		/* check that we have a single instruction following */
      condIns = True;
      continue;
      
    case ivar:                  // Create a new variable
      check_depth(fp,limit,op_sh_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      continue;

    case isvr:			/* Test for variable */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_jump(start,end_code,pc+op_so_val(pcx));
      continue;
      
    case uvar:                  // Unify against another variable
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_single(pc);		/* check that we have a single instruction following */
      condIns = True;
      continue;
     
    case ulit:                  // Unify against literal type
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_lit(litcnt,op_o_val(pcx)); /* Valid literal? */
      check_single(pc);		/* check that we have a single instruction following */
      condIns = True;
      continue;
      
    case utpl:                  // Unify against tuple
      check_depth(fp,limit,op_sh_val(pcx));
      check_single(pc);		/* check that we have a single instruction following */
      condIns = True;
      continue;
      
    case ucns:                  // Unify against type constructor
      check_depth(fp,limit,op_sh_val(pcx));
      check_single(pc);		/* check that we have a single instruction following */
      condIns = True;
      continue;
      
      
    case urst:
    case undo:                  // undo effects of unification
      check_depth(fp,limit,op_sh_val(pcx));
      continue;
     
    case errblk:		/* start a new error block */
      check_jump(start,end_code,pc+op_so_val(pcx));
      set_inited(fp,ar,limit,op_sh_val(pcx));
      set_inited(fp,ar,limit,op_sh_val(pcx)+1);
      break;

    case errend:		/* end an error block */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case generr:		/* generate a run-time error explicitly */
      endJump = !condIns;
      check_inited(fp,ar,limit,op_sl_val(pcx));
      continue;

    case moverr:		/* load the error value */
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;
      
    case loc2cns:{		/* construct a constructor from the locals */
      int len = op_m_val(pcx);
      int i;
      
      for(i=0;i<=len;i++)
	check_inited(fp,ar,limit,op_sh_val(pcx)+i);
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;
    }

    case consfld:		/* index a field from a constructor*/
      check_inited(fp,ar,limit,op_sh_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;
      
    case conscns:               // Access constructor symbol
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case cnupdte:		/* update a constructor */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sl_val(pcx));
      if(!allow_priv)
	return "constructor update is a privilidged instruction";
      break;

    case loc2tpl:{		/* copy a tuple from the locals */
      int len = op_sm_val(pcx);

      if(len<0)
	return "invalid loc2tpl arity";
      else{
	int i;
	for(i=0;i<len;i++)
	  check_inited(fp,ar,limit,op_sh_val(pcx)+i);
	set_inited(fp,ar,limit,op_sl_val(pcx));
      }
      break;
    }

    case indxfld:		/* index a field from a record*/
      check_inited(fp,ar,limit,op_sh_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case tpupdte:		/* update a tuple */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sl_val(pcx));
      if(!allow_priv)
	return "tuple update is a privilidged instruction";
      break;

    /* List manipulation instructions */
    case lstpr:			/* Construct a list pair */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case ulst:
      check_inited(fp,ar,limit,op_sh_val(pcx));
      set_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case nthel:			/* Extract the nth el. of a list */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case add2lst:		/* Add an element to a list */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_depth(fp,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sm_val(pcx));
      break;

    case lsupdte:		/* Modify list pointer */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_depth(fp,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sm_val(pcx));
      break;

    case loc2any:		/* construct an any value */
      check_depth(fp,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

      /* Integer arithmetic expression instructions */
    
    case plus:			/* Var to var addition */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;
	
    case incr:			/* increment int variable by fixed amount */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case minus:			/* Var to var subtract */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case times:			/* multiply */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case divide:		/*  divide */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case eq:			/* Test two elements for equality */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case neq:			/* Test two elements of stack for inequality */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case le:			/* Test two elements of stack for less than */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case gt:			/* Test two elements of stack for gt than */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case eqq:			/* Test two elements for pointer equality */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case neqq:			/* Test two elements for pointer inequality */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case feq:			/* Test two numbers for equality */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case fneq:			/* Test two numbers of stack for inequality */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case fle:			/* Test two numbers of stack for less than */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case fgt:			/* Test two numbers of stack for gt than */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case ieq:			/* Test two integers for equality */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case ineq:			/* Test two integers for inequality */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case ile:			/* Test two integers for less than */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;
      
    case igt:			/* Test two integers for gt than */
      check_inited(fp,ar,limit,op_sl_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_single(pc);		/* check that we have a single instruction */
      condIns = True;
      continue;

    case iftrue:		/* does a var contain true? */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_jump(start,end_code,pc+op_so_val(pcx));
      break;

    case iffalse:		/* does a var not contain true? */
      check_inited(fp,ar,limit,op_sh_val(pcx));
      check_jump(start,end_code,pc+op_so_val(pcx));
      break;
      
      /* special instructions */
    case snd:			/* send a message */
      check_depth(fp,limit,op_sh_val(pcx));
      check_inited(fp,ar,limit,op_sm_val(pcx));
      check_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case self:			/* report id of this process */
      set_inited(fp,ar,limit,op_sl_val(pcx));
      break;

    case gc:			/* Invoke garbage collector  */
      check_depth(fp,limit,op_sh_val(pcx));
      break;

    case use:			/* use a different click counter */
      set_inited(fp,ar,limit,op_sl_val(pcx)); /* this order is important! */
      check_inited(fp,ar,limit,op_sm_val(pcx));
      break;

    case line_d:		/* report on file and line number */
      check_depth(fp,limit,op_sh_val(pcx)); /* Check stack depth */
      check_symbol(literals,litcnt,op_o_val(pcx));
      pc++;
      break;

    case entry_d:		/* report on procedure entry */
      check_depth(fp,limit,op_sh_val(pcx)); /* Check stack depth */
      check_symbol(literals,litcnt,op_o_val(pcx));
      break;
	
    case exit_d:		/* report on procedure exit */
      check_depth(fp,limit,op_sh_val(pcx)); /* Check stack depth */
      check_symbol(literals,litcnt,op_o_val(pcx));
      break;
	
    case assign_d:		/* report on variable assignment */
      check_depth(fp,limit,op_sh_val(pcx)); /* Check stack depth */
      check_inited(fp,ar,limit,op_sm_val(pcx)); /* Inited reference? */
      check_inited(fp,ar,limit,op_sl_val(pcx)); /* Inited reference? */
      pc++;
      break;

    case return_d:		/* report on function return */
      check_inited(fp,ar,limit,op_sm_val(pcx)); /* Inited reference? */
      check_inited(fp,ar,limit,op_sl_val(pcx)); /* Inited reference? */
      check_depth(fp,limit,op_sh_val(pcx)); /* Check stack depth */
      pc++;
      break;

    case accept_d:		/* report accepting a message */
      check_depth(fp,limit,op_sh_val(pcx)); /* Check stack depth */
      check_inited(fp,ar,limit,op_sh_val(pcx)); /* Inited reference? */
      check_inited(fp,ar,limit,op_sm_val(pcx)); /* Inited reference? */
      check_inited(fp,ar,limit,op_sl_val(pcx)); /* Inited reference? */
      break;

    case send_d:		/* report sending a message */
      check_depth(fp,limit,op_sh_val(pcx)); /* Check stack depth */
      check_inited(fp,ar,limit,op_sm_val(pcx)); /* Inited reference? */
      check_inited(fp,ar,limit,op_sl_val(pcx)); /* Inited reference? */
      break;

    case fork_d:		/* fork a sub-process */
      check_depth(fp,limit,op_sm_val(pcx));
      check_inited(fp,ar,limit,op_sl_val(pcx)); /* Inited reference? */
      break;

    case die_d:			/* report a process dying */
      check_depth(fp,limit,op_sl_val(pcx)); /* Check stack depth */
      break;

    case scope_d:		/* set variable scope */
      check_depth(fp,limit,op_sm_val(pcx));
      break;

    case suspend_d:		/* report process suspension */
      check_depth(fp,limit,op_sl_val(pcx));
      break;
      
    case error_d:		/* report error */
      check_depth(fp,limit,op_sm_val(pcx));
      check_inited(fp,ar,limit,op_sl_val(pcx)); /* Inited reference? */
      break;
   
   case debug_d:
      check_jump(start,end_code,pc+op_lo_val(pcx));
      break;
      
    default:
      return "unimplemented instruction";
    }
    condIns = False;
  }

  seg->depth = limit;		/* put this back */

#ifdef VERIFYTRACE
  if(traceVerify){
    outMsg(logFile,"Segment checked: 0x%x..0x%x ",seg->pc-base->pc,
	   seg->pc+seg->length-1-base->pc);
    outMsg(logFile,"frame: %s\n",showEnv(fp,ar,limit));
  }
#endif

  for(i=0;i<MAXVAR;i++)
    seg->variables[i].read = vars[i].read; /* these are the variables we read */

  // Merge the variables with the next and the alt segments
  
  if(!endJump && seg->next!=base){
    char *ret = mergeVariables(vars,limit,seg->next); 

    if(ret!=NULL)
      return ret;
  }

  if(seg->alt!=NULL){
    char *ret = mergeVariables(vars,limit,seg->alt); 

    if(ret!=NULL)
      return ret;
  }

  if(!endJump && seg->next!=base&&seg->next->depth>0){
    char *ret = verifySegment(base,seg->next,limit,start,end_code,literals,litcnt,ar,
			  free,form,allow_priv);
    if(ret!=NULL)
      return ret;
  }

  if(seg->alt!=NULL&& seg->alt->depth>0){
    char *ret = verifySegment(base,seg->alt,limit,start,end_code,literals,litcnt,ar,
			  free,form,allow_priv);
    if(ret!=NULL)
      return ret;
  }

  return NULL;
}

/* return a string indicating error, or NULL */
char *codeVerify(objPo code,logical allow_priv)
{
  if(code!=NULL){
    int size = CodeSize(code);	/* Compute the size of the code section */
    objPo *literals = CodeLits(code); /* access the literals in the code */
    int litcnt = CodeLitcnt(code);
    int ar = CodeArity(code);	/* Arity of the procedure or function */
    int free = progTypeArity(CodeFrSig(code)); /* free variable count */

    int i;
    int limit=1;		/* Maximum env reached in code */
    insPo pc = CodeCode(code);	/* Starting point of real code */
    insPo end_code = pc+size;
    objPo type = CodeSig(code);
    WORD32 segCount = 0;
    segPo segments = splitPhase(pc,end_code-pc,&segCount);
    varpo fp = &segments->variables[LOCAL];
    char *ret;

    if(ar<0 || ar>MAXPAR)
      return "invalid number of parameters";

    if(free<0 || free>MAXPAR)
      return "invalid number of free variables";
      
    while(isBinCall(type=deRefVar(type),kallQ,NULL,&type))
      ;

    switch(CodeForm(code)){
    case function:
      if(!isBinCall(type,kfunTp,NULL,NULL))
	return "invalid function code";
      else
	break;
    case procedure:
      if(!isConstructor(type,kprocTp))
	return "invalid procedure code";
      else
	break;
    case invalid_code:
      return "invalid type of code";
    }

    for(i=0;i<MAXVAR;i++){
      segments->variables[i].inited = False; /* All variables start off uninited */
      segments->variables[i].read = False; /* All variables start off unread */
    }

    for(i=0;i<ar;i++)		/* Initialise the argument cells */
      fp[i+3].inited = True;

    limit = segments->depth = op_so_val(pc[1]);	/* collect from the 2nd instruction */

    ret = verifySegment(segments,segments,limit,pc,end_code,literals,
			litcnt,ar,free,CodeForm(code),allow_priv);

#ifdef VERIFYTRACE_
    if(traceVerify)
      dumpSegs(segments);
#endif
    clearSegments(segments);	/* release the segments we constructed */

    return ret;
  }

  return NULL;
}

#ifdef VERIFYTRACE
static char *showEnv(varpo fp,int ar,int limit)
{
  static char buff[256];
  int i;
  int offset = 3+ar;

  buff[0] = '<';

  for(i=ar+2;i>2;i--)
    if(fp[i].inited)
      buff[offset-i]='*';
    else
      buff[offset-i]=' ';

  for(;i>=0;i--)
    buff[offset-i]='=';

  for(;i>limit;i--)
    if(fp[i].inited)
      buff[offset-i]='*';
    else
      buff[offset-i]=' ';

  buff[offset-i] = '>';
  buff[offset-i+1]='\0';

  return buff;
}
#endif

/* 
   Code generation (assembler) function 
   (c) 1994-1998 Imperial College and F.G.McCabe

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
#include <stdlib.h>			/* Standard C library */
#include <string.h>		/* String functions */
#include "compile.h"		/* access structure manipulation */
#include "dict.h"
#include "opcodes.h"
#include "keywords.h"
#include "assem.h"
#include "signature.h"
#include "encode.h"
#include "types.h"

#define MAXCODE 128		/* Default initial code segment size */
#define CODESEG 16		/* Initial number of code segments */
#define MAXOPCODE 256		/* Maximum number of opcodes */
#define MAXSYMBOLS 2000		/* MAX no of labels in an assembler file*/

typedef struct fxr *fixpo;	/* fixup entry */

typedef enum{			/* length space in label */
  lbl16, lbl24, lbl32}
reflen;

typedef struct fxr{
  int pc;			/* where in the code is this fixup */
  reflen whole;			/* how much to shift the number to the left */
  fixpo next;			/* next fixup entry */
} fxr;

typedef enum {literal,codelbl} lbltype;

typedef struct _label{
  lbltype lbtype;		/* The type of entry in the label table */
  int offset;			/* Address of entry in the code */
  fixpo fixups;			/* Head of the fixup list */
  cellpo lbl;			/* The actual label itself */
  lblpo next;			/* next label in the table */
} lbc;

typedef struct _code_ {		/* A segment of code */
  instruction *instr;		/* The instruction sequence itself */
  long size;			/* Size of code sequence */
  cellpo lits;			/* An array of literal values */
  int litcnt;			/* Number of literals in the code */
  int arity;			/* Arity of the procedure or function */
  int free;			/* Number of free variables this code has */
  entrytype atype;		/* What kind of code is it? */
} CodeSegment;

typedef struct _coderec_ {
  codepo cd;			/* The code segment we are dealing with */
  int pc;			/* current `program counter' */
  long limit;			/* how big is this code segment */
  lblpo labels;			/* list of active labels */
  lblpo literals;		/* list of referenced literals */
  int maxdepth;			/* maximum depth reached in code segment */
  int line;			/* Last line debugging info generated for */
  long scope;
  symbpo name;			/* The name of this block of code */
} coderec;

typedef struct oprecord{
  int opands;			/* Number of operands */
  char *opstr;			/* String defining types of operands*/
  char *mnem;			/* The mnemonic */
} mnemc, *mnempo;

mnemc opcodes[MAXOPCODE];	/* Table of opcodes -- parameter types */
static poolPo codepool=NULL;	/* pool of code segments */
static poolPo lblpool=NULL;	/* pool of labels */
static poolPo fxupool=NULL;	/* pool of fixup entries */

void assem_err(char *msg,cellpo where);

static void install_instr(char *mnem,long opcode,char *opstr)
{
  mnempo m = &opcodes[opcode];
  m->opands = strlen(opstr);
  m->opstr = opstr;
  m->mnem = mnem;
}

/* 
 * Initialise code generation functions
 * Build the table of opcode specifications 
 */
void init_instr(void)
{
  int i=0;
  for(;i<MAXOPCODE;i++){
    opcodes[i].opands=-1;	/* null entry */
    opcodes[i].opstr="";
    opcodes[i].mnem="";
  }

  codepool = newPool(sizeof(coderec),CODESEG); /* pool of code segments */
  lblpool = newPool(sizeof(lbc),MAXSYMBOLS); /* pool of labels */
  fxupool = newPool(sizeof(fxr),MAXSYMBOLS); /* pool of fixup entries */

#undef instruction
#define instruction(mnem,op,sig,tp) install_instr(#mnem,mnem,sig);

#include "instructions.h"
#undef instruction
}

/*
 * Label and fixup handling functions 
 */


static fixpo new_fixup(int pc, reflen whole, fixpo prev)
{
  fixpo fx = allocPool(fxupool); /* grab a new fixup entry */

  fx->next=prev;
  fx->pc=pc;
  fx->whole=whole;
  return fx;
}

/*
 * Create a new label entry
 */
lblpo NewLabel(cpo code)
{
  lblpo lb = (lblpo)allocPool(lblpool); /* grab a new label */
  static int LblNo=0;		/* default label numbering */

  lb->lbtype=codelbl;		/* default label type is code */
  lb->offset = -1;		/* initially undefined */
  lb->fixups = NULL;		/* no fixups yet */
  mkInt(lb->lbl=allocSingle(),LblNo++);	/* default label value */
  lb->next=code->labels;	/* insert into table of labels */
  code->labels=lb;
  return lb;
}

/* return current value of label -- generating fixup if necessary */
static long findlbl(lblpo label,int pc,reflen whole) 
{
  if(label->offset!=-1)		/* This label is defined */
    return label->offset-pc-1;
  else{				/* we need a new fixup */
    label->fixups = new_fixup(pc,whole,label->fixups);
    return -1;
  }
}

lblpo findLabel(cellpo label,cpo code)
{
  lblpo lbl = code->labels;

  while(lbl!=NULL)
    if(sameCell(lbl->lbl,label))
      return lbl;
    else
      lbl=lbl->next;

  if(lbl==NULL){		/* New code label */
    lbl = (lblpo)allocPool(lblpool); /* grab a new label */

    lbl->lbtype=codelbl;		/* default label type is code */
    lbl->offset = -1;		/* initially undefined */
    lbl->fixups = NULL;		/* no fixups yet */
    lbl->lbl=label;		/* Define the label name */
    lbl->next=code->labels;	/* insert into table of labels */
    code->labels=lbl;
  }
  return lbl;
}

/*
 * Define a label occurrence
 */
void DefLabel(cpo code,lblpo label)
{
  if(label->offset==-1){	/* check that not multiply defined */
    fixpo fx = label->fixups;
    fixpo nfx = NULL;

    label->offset = code->pc;	/* Set the location of the literal */

#ifdef COMPTRACE
    if(traceCompile){
      outMsg(logFile,"%U  0x%x: L%5w:\n",code->name,code->pc,label->lbl);
      flushFile(logFile);
    }
#endif

    /* Process the fixups */
    while(fx!=NULL){
      long cp = fx->pc;
      long gap = code->pc-cp-1;

      switch(fx->whole){
      case lbl16:
	if(gap<-32768 || gap>32767)
	  assem_err("PC relative offset too large",label->lbl);
	code->cd->instr[cp]&=~((0xffff)<<8); /* remove old offset */
	code->cd->instr[cp]|=(gap&0xffff)<<8;
	break;

      case lbl24:
	if(gap<(-(1<<24)) || gap>(1<<24)-1)
	  assem_err("PC relative offset too large",label->lbl);
	code->cd->instr[cp]&=~(0xffffff00); /* remove old offset */
	code->cd->instr[cp]|=(gap&0xffffff)<<8;
	break;

      case lbl32:
	code->cd->instr[cp]=gap;
	break;
      }

      nfx=fx->next;		/* move to the next fixup entry */
      freePool(fxupool,fx);	/* release the fixup entry */
      fx = nfx;
    }
  }
  else
    assem_err("Multiply defined label",label->lbl);
}

void DefLiteral(cpo code,long which,lblpo label)
{
  if(label->offset==-1){	/* check that not multiply defined */
    fixpo fx = label->fixups;
    fixpo nfx = NULL;

    label->offset = which;	/* Set the location of the literal */

#ifdef COMPTRACE
    if(traceCompile){
      outMsg(logFile,"%U  0x%x: L%5w:\n",code->name,which,label->lbl);
      flushFile(logFile);
    }
#endif

    /* Process the fixups */
    while(fx!=NULL){
      long cp = fx->pc;

      switch(fx->whole){
      case lbl16:
	if(which<-32768 || which>32767)
	  assem_err("PC relative offset too large",label->lbl);
	code->cd->instr[cp]&=~((0xffff)<<8); /* remove old offset */
	code->cd->instr[cp]|=(which&0xffff)<<8;
	break;

      case lbl24:
	if(which<(-(1<<24)) || which>(1<<24)-1)
	  assem_err("PC relative offset too large",label->lbl);
	code->cd->instr[cp]&=~(0xffffff00); /* remove old offset */
	code->cd->instr[cp]|=(which&0xffffff)<<8;
	break;

      case lbl32:
	code->cd->instr[cp]=which;
	break;
      }

      nfx=fx->next;		/* move to the next fixup entry */
      freePool(fxupool,fx);	/* release the fixup entry */
      fx = nfx;
    }
  }
  else
    assem_err("Multiply defined label",label->lbl);
}

/* Declare a literal value */
static long findlit(cellpo label,cpo code,long pc,reflen whole)
{
  lblpo lit = code->literals;

  while(lit!=NULL)
    if(sameCell(lit->lbl,label)){
      if(lit->offset!=-1)	/* literal already defined */
	return lit->offset;
      else{
	lit->fixups = new_fixup(pc,whole,lit->fixups);
	return -1;
      }
    }
    else
      lit=lit->next;

  lit = (lblpo)allocPool(lblpool); /* grab a new label */

  lit->lbtype=literal;		/* label type is literal */
  lit->offset = -1;		/* initially undefined */
  lit->fixups = new_fixup(pc,whole,NULL); /* one initial fixup */
  lit->lbl=label;		/* store the literal value */
  lit->next=code->literals;	/* insert into table of literals */
  code->literals=lit;
  code->cd->litcnt++;		/* increment number of known literals */

  return -1;			/* not defined initially */
}

/* 
 * Code segment managment
 */

static void grow_code(cpo code)
{
  if(code->pc+1>=code->limit){
    instruction *newcode = code->cd->instr = (instruction *)realloc(code->cd->instr,
						      (code->limit+MAXCODE)*
						      sizeof(instruction));
    if(newcode==NULL)
      syserr("no space for code");	/* we couldnt grow the code segment */
    code->limit+=MAXCODE;
  }
}

/* Allocate a new code segment */
cpo NewCode(symbpo name)
{
  cpo code = allocPool(codepool); /* grab a code segment */

  code->cd = (codepo)allocCode();

  code->cd->instr = (instruction*)malloc(sizeof(instruction)*MAXCODE);
  code->cd->lits = NULL;
  code->cd->litcnt = 0;
  code->pc=0;
  code->limit=MAXCODE;
  code->labels=code->literals=NULL;
  code->maxdepth=0;
  code->line=-1;		/* reset line debugging info */
  code->scope = -1;		/* reset scope info */
  code->pc=3;			/* set the program counter to start of code */
  code->name = name;

#ifdef COMPTRACE
  if(traceCompile){
    outMsg(logFile,"New code -- %U\n",name);
    flushFile(logFile);
  }
#endif

  return code;
}

long LDebugCode(cpo code)
{
  return code->line;
}

void SetDebugLine(cpo code,int line)
{
  code->line=line;
}

long CodeScope(cpo code)
{
  return code->scope;
}

void SetCodeScope(cpo code,long scope)
{
  code->scope = scope;
}

int MaxDepth(cpo code)
{
  if(code!=NULL)
    return code->maxdepth;
  else
    return 127;			/* illegal depth */
}

/* finish off a code segment */
codepo CloseCode(cpo code,cellpo sig,cellpo freesig,entrytype atype)
{
  int arity=arityOfType(sig);	/* arity of the code segment */
  int free = tplArity(freesig);
  int litcnt=0;
  codepo cd = code->cd;
  lblpo lit=code->literals;

  code->cd->size = code->pc;
  code->cd->lits = Next(allocTuple(code->cd->litcnt+2));
  code->cd->instr[0]=SIGNATURE; /* the universal code signature */
  code->cd->instr[1]=(atype<<24)|((arity&0xff)<<16)|(code->cd->litcnt&0xffff);
  code->cd->instr[2]=(free<<24)|(code->pc&0xffff);    /* # frees + code size */

  while(lit!=NULL){
    lblpo nlit = lit->next;
    DefLiteral(code,litcnt,lit); /* Define the literal */
    code->pc++;
    copyCell(nthCell(code->cd->lits,litcnt++),lit->lbl);	/* store the literal value */

    freePool(lblpool,lit);	/* free the label entry */
    lit=nlit;			/* step on to the next literal */
  }

  copyCell(nthCell(code->cd->lits,litcnt++),sig);/* set up the argument types */
  copyCell(nthCell(code->cd->lits,litcnt++),freesig);/* set up the types of free vars */

  code->cd->arity = arity;
  code->cd->free = free;
  code->cd->atype = atype;
  

  lit=code->labels;		/* check all the labels */
  while(lit!=NULL){
    lblpo nlit = lit->next;
    if(lit->offset==-1 && lit->fixups!=NULL)
      assem_err("Undefined label",lit->lbl);
    freePool(lblpool,lit);
    lit=nlit;
  }

  freePool(codepool,code);	/* release the code context block */
#ifdef COMPTRACE
  if(traceCompile){
    outMsg(logFile,"End code %U: %t\n",code->name,sig);
    flushFile(logFile);
  }
#endif

  return cd;			/* and return the completed code! */
}

/* Access the arity of a code segment */
int codearity(codepo pc)
{
  return pc->arity;
}

/* Access whether or not a code segment defines a function, proc, or pattern */
entrytype codeform(codepo pc)
{
  return pc->atype;
}

cellpo codetypes(codepo pc)
{
  return nthCell(pc->lits,pc->litcnt);
}

cellpo codefrtypes(codepo pc)
{
  return nthCell(pc->lits,pc->litcnt+1);
}

long codesize(codepo pc)
{
  return pc->size-3;
}

instruction *codecode(codepo pc)
{
  return pc->instr+3;
}

long codelitcnt(codepo pc)
{
  return pc->litcnt;
}

cellpo codelit(codepo pc,long i)
{
  return nthCell(pc->lits,i);
}

/* Generic function that generates an instruction */
int genIns(cpo code,char *cmnt,instruction opcode, ...)
{
  va_list args;			/* access the generic arguments */
  int opands=opcodes[opcode].opands; /* how many operands are expected */
  char *opstr=opcodes[opcode].opstr; /* what types are the operands */
  instruction iword = opcode;	/* build the instruction word */
  long opc=code->pc;		/* pc before instruction is placed in code */

  if(opands==-1)
    syserr("Uninstalled instruction");

  va_start(args,opcode);	/* start the variable argument sequence */
  grow_code(code);		/* increase code segment size */
  code->pc++;			/* increment pc for arguments */

#ifdef COMPTRACE
  if(traceCompile)
    outMsg(logFile,"%U  0x%x: %s ",code->name,opc,opcodes[opcode].mnem);
#endif

  while(opands--){
    switch(*opstr++){

    case 'l':{			/* low order byte value */
      int i = va_arg(args, int); /* pick up an integer value */
      iword|=(i&0xff)<<8;	/* put in the integer value */
      setMaxDepth(code,i);	/* set the maximum depth of code */

#ifdef COMPTRACE
      if(traceCompile)
	outInt(logFile,i);
#endif
      break;
    }

    case 'm':{			/* middle order byte value */
      int i = va_arg(args, int); /* pick up an integer value */
      iword|=(i&0xff)<<16;	/* put in the integer value */
      setMaxDepth(code,i);	/* set the maximum depth of code */

#ifdef COMPTRACE
      if(traceCompile)
	outInt(logFile,i);
#endif
      break;
    }

    case 'h':{			/* high order byte value */
      int i = va_arg(args, int); /* pick up an integer value */
      iword|=(i&0xff)<<24;	/* put in the integer value */
      setMaxDepth(code,i);	/* set the maximum depth of code */

#ifdef COMPTRACE
      if(traceCompile)
	outInt(logFile,i);
#endif
      break;
    }

    case 'o':{			/* 16bit integer value */
      int i = va_arg(args, int); /* pick up an integer value */
      iword|=(i&0xffff)<<8;	/* put in the integer value */

#ifdef COMPTRACE
      if(traceCompile)
	outInt(logFile,i);
#endif
      break;
    }

    case 'b':{			/* twobyte label offset */
      lblpo lb = va_arg(args, lblpo);	/* pick up a cell */
      iword|=(findlbl(lb,opc,lbl16)&0xffff)<<8;
#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"L%10w",lb->lbl);
#endif
      break;
    }

    case 'B':{			/* three byte label offset */
      lblpo lb = va_arg(args, lblpo);	/* pick up a cell */
      iword|=(findlbl(lb,opc,lbl24)&0xffffff)<<8;
#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"L%10w",lb->lbl);
#endif
      break;
    }

    case 't':{			/* two byte literal reference */
      cellpo ip = va_arg(args, cellpo);	/* pick up a cell */
      iword|=(findlit(ip,code,opc,lbl16)&0xffff)<<8;
#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"%10w",ip);
#endif
      break;
    }

    case 'x':{			/* long reference to label */
      lblpo lb = va_arg(args, lblpo);	/* pick up a label */
      grow_code(code);		/* do we need to grow the code buffer? */


      code->cd->instr[code->pc]=findlbl(lb,code->pc,lbl32);
      code->pc++;

#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"L%10w",lb->lbl);
#endif
      break;
    }

    case 'y':{			/* long reference to a literal */
      cellpo ip = va_arg(args, cellpo);	/* pick up a cell */
      grow_code(code);		/* do we need to grow the code buffer? */

      code->cd->instr[code->pc]=findlit(ip,code,code->pc,lbl32);
      code->pc++;

#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"%10w",ip);
#endif

      break;
    }

    default:
      assem_err("Internal error",NULL);
    }

#ifdef COMPTRACE
      if(traceCompile && opands>0)
	outStr(logFile,",");
#endif

  }

  code->cd->instr[opc] = iword;	/* install the instruction word */

#ifdef COMPTRACE
  if(traceCompile){
    if(cmnt!=NULL)
      outMsg(logFile,"\t; %s",cmnt);
    outStr(logFile,"\n");
    flushFile(logFile);
  }
#endif
  va_end(args);
  return opc;			/* return pc where instruction added */
}

/* Generic function that re-generates an instruction */
void upDateIns(cpo code, int opc, int opcode, ...)
{
  va_list args;			/* access the generic arguments */
  int opands=opcodes[opcode].opands; /* how many operands are expected */
  char *opstr=opcodes[opcode].opstr; /* what types are the operands */
  unsigned long iword = opcode;	/* build the instruction word */
  int pc=opc;			/* pc for arguments */

  va_start(args,opcode);	/* start the variable argument sequence */

#ifdef COMPTRACE
  if(traceCompile)
    outMsg(logFile,"Update 0x%x: %s ",opc,opcodes[opcode].mnem);
#endif

  while(opands--){
    switch(*opstr++){

    case 'l':{			/* low order byte value */
      int i = va_arg(args, int); /* pick up an integer value */
      iword|=(i&0xff)<<8;	/* put in the integer value */

#ifdef COMPTRACE
      if(traceCompile)
	outInt(logFile,i);
#endif
      break;
    }

    case 'm':{			/* middle order byte value */
      int i = va_arg(args, int); /* pick up an integer value */
      iword|=(i&0xff)<<16;	/* put in the integer value */

#ifdef COMPTRACE
      if(traceCompile)
	outInt(logFile,i);
#endif
      break;
    }

    case 'h':{			/* high order byte value */
      int i = va_arg(args, int); /* pick up an integer value */
      iword|=(i&0xff)<<24;	/* put in the integer value */

#ifdef COMPTRACE
      if(traceCompile)
	outInt(logFile,i);
#endif
      break;
    }

    case 'o':{			/* 16bit integer value */
      int i = va_arg(args, int); /* pick up an integer value */
      iword|=(i&0xffff)<<8;	/* put in the integer value */

#ifdef COMPTRACE
      if(traceCompile)
	outInt(logFile,i);
#endif
      break;
    }

    case 'b':{			/* twobyte label offset */
      lblpo lb = va_arg(args, lblpo);	/* pick up a cell */
      iword|=(findlbl(lb,opc,lbl16)&0xffff)<<8;
#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"L%10w",lb->lbl);
#endif
      break;
    }

    case 'B':{			/* three byte label offset */
      lblpo lb = va_arg(args, lblpo);	/* pick up a cell */
      iword|=(findlbl(lb,opc,lbl24)&0xffffff)<<8;
#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"L%10w",lb->lbl);
#endif
      break;
    }

    case 't':{			/* two byte literal reference */
      cellpo ip = va_arg(args, cellpo);	/* pick up a cell */
      iword|=(findlit(ip,code,opc,lbl16)&0xffff)<<8;
#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"%10w",ip);
#endif
      break;
    }

    case 'x':{			/* long reference to label */
      lblpo lb = va_arg(args, lblpo);	/* pick up a label */
      code->cd->instr[pc++]=findlbl(lb,opc,lbl32);

#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"L%10w",lb->lbl);
#endif
      break;
    }

    case 'y':{			/* long reference to a literal */
      cellpo ip = va_arg(args, cellpo);	/* pick up a cell */

      code->cd->instr[pc++]=findlit(ip,code,opc,lbl32);
#ifdef COMPTRACE
      if(traceCompile)
	outMsg(logFile,"%10w",ip);
#endif
      break;
    }

    default:
      assem_err("Internal error",NULL);
    }

#ifdef COMPTRACE
      if(traceCompile && opands>0)
	outStr(logFile,",");
#endif

  }

  code->cd->instr[opc] = iword;	/* install the instruction word */

#ifdef COMPTRACE
  if(traceCompile){
    outStr(logFile,"\n");
    flushFile(logFile);
  }
#endif
  va_end(args);
}

void genAlign(cpo code, int align)
{
 if((code->pc/align)*align!=code->pc){
   lblpo L0 = NewLabel(code);

   while((code->pc/align)*align!=code->pc)
     genIns(code,"for label alignment",jmp,L0);
   DefLabel(code,L0);
 }
}

/* compute destination for an expression target */
int genDest(int *dest, int depth, int *dp, cpo code)
{
  if(dest!=NULL && *dest!=0){
    *dp=depth;
    return *dest;		/* use existing destination */
  }
  else{
    *dest=--depth;
    *dp=*dest;

    if(depth<code->maxdepth)
      code->maxdepth=depth;

    return depth;
  }
}

void setMaxDepth(cpo code,int depth)
{
  if(code!=NULL && depth<code->maxdepth)
    code->maxdepth=depth;
}

void assem_err(char *msg,cellpo where)
{
  outMsg(logFile,"Error in assembly: %s at %10w\n",msg,where);
  syserr(msg);
}

long codeSize(void)
{
  return sizeof(CodeSegment);
}

void freeOffCode(codepo pc)
{
  free(pc->instr);
}

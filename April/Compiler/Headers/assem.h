#ifndef _ASSEM_H_
#define _ASSEM_H_

/*
 * Define low-level code generation functions
 */

#include <stdarg.h>

/* symbolic names for operand types */
/* b - a 16bit label reference */
/* o - a 16bit numeric value */
/* t - a 16bit reference to a literal value */
/* l - low order byte offset */
/* m - middle order byte offset */
/* h - high order byte offset */
/* x - label in separate long word */
/* y - literal in separate long word */

void init_instr(void);

typedef unsigned WORD32 instruction;

int genIns(cpo, char *,instruction, ...); /* generic instruction generation */
void genAlign(cpo, int);	/* force an alignment of the code */
void upDateIns(cpo code, int opc, int opcode, ...); /* regenerate an instr*/
void assemble(cellpo prog,cellpo tgt);

lblpo NewLabel(cpo);		/* generate a new label */
void DefLabel(cpo,lblpo);	/* define a code label */
cpo NewCode(symbpo name);	/* Create a new code segment */
lblpo findLabel(cellpo label,cpo code);
long LDebugCode(cpo code);	/* true if last instruction was line debugging */
void SetDebugLine(cpo code,int line);
long CodeScope(cpo code);	/* current scope level in code */
void SetCodeScope(cpo code,long scope);

int MaxDepth(cpo code);		/* What is the maximum depth of variables? */
void setMaxDepth(cpo code, int depth); /* Is this new location deeper? */
codepo CloseCode(cpo code,cellpo sig,cellpo freesig,entrytype atype);
int genDest(int *dest, int depth, int *dp, cpo code); /* compute destination */
long codeSize();
void freeOffCode(codepo pc);

#endif

#ifndef _CELL_H_
#define _CELL_H_

#include "unicode.h"

/* Public interface for the cell structure */
typedef enum {intger,floating,symbolic,character,identifier,strng,codeseg,
              list,tuple,constructor,tvar} tag_tp;

typedef struct _cell_ *cellpo;
typedef uniChar *symbpo;	/* A symbol is a sequence of uniChars */
typedef double	FLOAT;
typedef FLOAT *floatpo;		/* a pointer to a floating point number */
typedef struct _code_ *codepo;	/* pointer to code */
typedef struct _label *lblpo;	/* pointer to a label entry */
typedef struct _coderec_ *cpo;	/* pointer to a code resource */
typedef struct _line_data *linepo;
typedef struct _trace_rec_ *tracePo;

/* common cell operations */
tag_tp tG(cellpo x);
cellpo Next(cellpo x);
cellpo Prev(cellpo x);
cellpo nthCell(cellpo x,long i);
short int whenVisited(cellpo x);
void visitCell(cellpo x,short int i);
logical sameCell(cellpo c1,cellpo c2);
int compareCell(cellpo c1,cellpo c2);

logical isChr(cellpo x);
void mkChar(register cellpo x,uniChar ch);
uniChar CharVal(cellpo x);

logical IsInt(cellpo x);
logical IsInteger(cellpo x);
long intVal(cellpo x);
long integerVal(cellpo x);
void mkInt(register cellpo x,long numb);
cellpo allocInt(long num);

logical IsFloat(cellpo x);
FLOAT fltVal(cellpo x);
void mkFlt(register cellpo c, FLOAT f);

logical isSymb(cellpo x);
symbpo symVal(cellpo x);
void mkSymb(register cellpo x,symbpo sym);

logical IsIdent(cellpo x);
logical IsId(cellpo in,symbpo sym);
symbpo idVal(cellpo x);
void mkIdent(register cellpo x,symbpo sym);

logical IsString(cellpo x);
uniChar *strVal(cellpo x);
void mkStr(register cellpo x,uniChar *str);

logical IsList(cellpo x);
logical isEmptyList(cellpo x);
logical isNonEmptyList(cellpo x);
cellpo listVal(cellpo x);
cellpo listHead(cellpo x);
cellpo listTail(cellpo x);
void mkList(register cellpo x,register cellpo list);
long listLength(cellpo x);

logical isTpl(cellpo x);
unsigned long tplArity(cellpo x);
cellpo tplVal(cellpo x);
cellpo tplArg(cellpo x,unsigned long n);
void updateTpl(cellpo x,unsigned long n,cellpo val);
void mkTpl(register cellpo x,register cellpo tpl);

logical IsCode(cellpo x);
codepo codeVal(cellpo x);
void mkCode(register cellpo x,codepo pc);

logical isCons(cellpo x);
cellpo consFn(cellpo x);
unsigned long consArity(cellpo x);
cellpo consEl(cellpo x,unsigned long i);
void mkCons(cellpo x,cellpo seq);
void updateConsEl(cellpo x,unsigned long i,cellpo arg);

logical IsTypeVar(cellpo x);
cellpo refVal(cellpo t);
cellpo deRef(register cellpo t);
void mktvar(register cellpo x,register cellpo value);

logical IsDotVar(cellpo x);
cellpo deRef(register cellpo t);

cellpo dupCell(cellpo c);
cellpo copyCell(cellpo dst,cellpo src);

void initLineTable(void);
void copyLineInfo(cellpo in,cellpo out);
void setLineInfo(int at,uniChar *name,cellpo tgt);
void setCellLineInfo(int at,cellpo file,cellpo tgt);
int mkLineInfo(uniChar *file,int at);
void mkLine(int at);
int lineInfo(cellpo x);
void stLineInfo(cellpo x,int l);

cellpo sourceFile(int line);
int sourceLine(int line);

cellpo source_file(cellpo in);
int source_line(cellpo in);
uniChar *sourceLocator(char *prefix,int line);

cellpo typeInfo(cellpo exp);
cellpo setTypeInfo(cellpo exp,cellpo type);
cellpo copyTypeInfo(cellpo dest,cellpo in);

/* This is used when handling fixpoint structures */
typedef struct _trace_rec_ {
  cellpo t;
  tracePo prev;			/* The trace stack will be within C stack */
} TraceRec;

#endif

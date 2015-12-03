#ifndef _ENCODE_H_
#define _ENCODE_H_

#include "assem.h"

/* Interface for code generation and term encoding */
int codearity(codepo pc);
entrytype codeform(codepo pc);
cellpo codetypes(codepo pc);
cellpo codefrtypes(codepo pc);
void encodeICM(ioPo out,cellpo input);
long codesize(codepo pc);
long codelitcnt(codepo pc);
cellpo codelit(codepo pc,long i);
instruction *codecode(codepo pc);

cellpo makeTypeSig(cellpo type);

#include "encoding.h"
#endif

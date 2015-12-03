#ifndef _HEAP_H_
#define _HEAP_H_

/* Compiler heap management interface */
void initHeap(void);
void MemExit(void);

cellpo allocCons(long size);            // Allocate a constructor
cellpo allocTuple(long size);		/* Allocate a size-tuple on the heap*/
cellpo allocPair(void);			/* Allocate a list pair or an application pair */
cellpo allocSingle(void);		/* Allocate a single cell */
codepo allocCode(void);			/* Allocate a code block */
floatpo allocFloat(FLOAT f);		/* allocate a floating point entry */
uniChar *allocString(const uniChar *data);
uniChar *allocCString(const char *data);

#endif

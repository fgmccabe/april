#ifndef _STACK_H_
#define _STACK_H_

/* 
 * Definition of stack management functions 
 */
void initStack(long size);
void resetRstack(void);
logical emptyStack(void);
int stackDepth(void);
void resetStack(int prior);
cellpo topStack(void);
void reset_stack(cellpo point);
void sortStack(cellpo base,cellpo top);
void reverseStack(long base);	/* reverse the order of elements on the stack */

void popcell(cellpo p);		/* get an element off the cell stack */
void p_drop(void);		/* drop an element from the cell stack */
void p_swap(void);		/* Swap the top two elements on the read stack */
cellpo p_cell(cellpo p);	/* Push a cell on to stack */
void p_char(uniChar ch);	/* Push a character onto stack */
void p_int(long i);		/* Push an integer onto stack */
void p_flt(FLOAT f);		/* Push a floating point number onto stack */
void p_sym(symbpo s);		/* Push a symbol onto the cell stack */
void p_string(uniChar *s);	/* Push a string onto the cell stack */
void join_strings(void);	/* concatenate top two strings of stack */
void p_tpl(cellpo p);		/* Push a tuple onto the cell stack */
void p_tuple(long ar);		/* Construct a tuple from the stack */
long p_untuple(void);           // Unwrap a tuploe at hte top of the stack
cellpo opentple(void);		/* Start the building of a new tuple */
void clstple(cellpo tpl);	/* Close a tuple, get elements from stack */
void cls_list(cellpo tpl);	/* Close a list, get elements from stack */
void p_null(void);		/* Push empty list onto the stack */
void p_list(void);		/* Construct a list pair on the stack */

void p_cons(long ar);         // Construct a cons term

void mkcall(int n);		/* Construct an n-ary function call  */

cellpo pBinary(cellpo fn);
cellpo pUnary(cellpo fn);

#endif


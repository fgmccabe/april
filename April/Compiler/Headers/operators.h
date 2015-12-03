#ifndef _OPERATOR_H_
#define _OPERATOR_H_

/* Header file defining the nature of operators */

#define TERMPREC 1000		/* precedence of a data level object */
#define STMTPREC 2000		/* precedence of a statement level object */
#define MINPREC 0		/* precedence of a primitive term */

typedef enum { prefixop, infixop, postfixop } optype;

typedef struct{
  struct {
    int lprec;			/* left precedemce of the operator */
    int rprec;			/* right precedemce of the operator */
    int prec;			/* result precedence of the operator */
  }p[3];			/* precedences for prefix, infix & postfix */
  symbpo sym;			/* the symbol that this is an operator for */
} oprecord,*oppo;

void init_op(void);		/* Initialize the operator table */
logical IsOperator(symbpo);	/* True if a symbol is an operator */

int preprec(symbpo, int *, int *); /* get the prefix precedence */
int infprec(symbpo, int *, int *, int *); /* get the infix precedence */
int postprec(symbpo, int *, int *); /* get the postfix precedence */
logical new_op(uniChar *name, uniChar *type, int prec,int line);

#define MAX_OP  200		/* Max number of operator declarations allowed */

#endif

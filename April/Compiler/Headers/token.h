#ifndef _TOKEN_H_
#define _TOKEN_H_

/* Header file defining the structure of a token */

typedef enum{
  eofToken, idToken, strToken, intToken, fltToken, qtToken, chrToken,
  lparToken, rparToken, lbraToken, rbraToken, lbrceToken, rbrceToken,
  cnsToken, commaToken, semiToken,
  ltplToken,rtplToken,
  errorToken
} tokenType;

typedef struct{
  tokenType tt;			/* Token type */
  union {
    integer i;			/* When the tokenizer finds an integer */
    double f;			/* or a float */
    uniChar *name;		/* or a symbol */
    uniChar ch;                 // For a character literal
  } tk;
  int line_num;			/* which line number etc */
} token, *tokenpo;

void init_token();		/* initialize the tokeniser */
long currentLineNumber();        /* report line numbers */

tokenType nextoken(ioPo inFile,tokenpo tok);
tokenType hedtoken(ioPo inFile,tokenpo tok);
void expectToken(ioPo inFile,tokenType t);
void commit_token();		/* commit to existing lookahead */
void printtoken(char*,tokenpo,char*); /* display a token */
void synerr(tokenpo,char*);
uniChar *tokenStr(tokenpo tok);
extern int last_line;		/* line number of last token committed to */

extern logical traceParse;	/* true if we are trying to trace the parser */


#endif

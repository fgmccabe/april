#ifndef _GRAMMAR_H_
#define _GRAMMAR_H_

extern logical traceParse;	/* True if tracing the parser */

int parse(ioPo inFile,int prior,logical debug,long *line);

#endif

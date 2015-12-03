/*
   Public declarations for the dictionary functions for April
*/
#ifndef _DICT_H_
#define _DICT_H_

void init_dict(void);	/* initialize dictionary */
objPo newUniSymbol(const uniChar *name);
objPo newSymbol(const char *name); /* access symbol from a dict */
void installSymbol(objPo s);
#ifdef EXECTRACE
char *escapeName(int code);
#endif
funpo escapeCode(unsigned int code);

void install_escapes(void);
void scanEscapes(void);
void markEscapes(void);
void adjustEscapes(void);

void scanDict(objPo base,objPo limit);
void markDict(void);
void adjustDict(void);

#endif

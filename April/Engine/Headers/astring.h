/*
 * header file defining string functions and types for the April evaluator
 */
#ifndef _A_STRING_H_
#define _A_STRING_H_

#include "april.h"

char *Cstring(objPo str,char *buff,unsigned long len);
uniChar *StringText(objPo s,uniChar *buff,unsigned long len);

logical isCharList(objPo p);

#endif

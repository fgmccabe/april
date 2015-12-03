/*
 * String handling utilities
 */

#ifndef _APRIL_STR_H_
#define _APRIL_STR_H_

extern inline char *StrNCpy(char *dest,register char *s,long count)
{
  register char *d = dest;
  while(count-->0 && (*d++=*s++)!='\0')
    ;
  *d = '\0';			/* The system one doesnt always do this ... stupidly */
  return dest;
}

extern inline char *StrNCat(char *dest,register char *s,long count)
{
  register char *d = dest;

  while(count-->0 && *d!='\0')
    d++;
  while(count-->0 && (*d++=*s++)!='\0')
    ;
  *d = '\0';			/* The system one doesnt always do this ... stupidly */
  return dest;
}

char *lowerStr(char *buffer,const char *orig,long count);
char *DupStr(char *str);

#endif

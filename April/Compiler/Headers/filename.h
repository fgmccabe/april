/*
 * Header for file name computation
 */
#ifndef _FILE_NAME_H_
#define _FILE_NAME_H_

uniChar *fileNameSuffix(const uniChar *orig,const char *suffix,uniChar *buff,long len);

uniChar *fileNamePrefix(const uniChar *orig,uniChar *name,long len);

uniChar *FileTail(uniChar *n);

uniChar *rmDots(char *prefix,cellpo fn,uniChar *buff,long len);	/* construct a `proper' file name */
#endif

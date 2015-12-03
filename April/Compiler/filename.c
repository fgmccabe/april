/*
 * Compute proper file names 
 * (c) 1994-1998 Imperial College and F.G.McCabe
 * All rights reserved
 *
 */
#include "config.h"
#include "local.h"			/* pick up localization */
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#define HAS_USERS			/* Is able to interpret file names of the form ~user... */
#include <unistd.h>

#include <string.h>
#include <stdlib.h>
#include "compile.h"
#include "keywords.h"


/* 
 * Construct a new file name based on an existing name and a new suffix 
 */
uniChar *fileNameSuffix(const uniChar *orig,const char *suffix,uniChar *buff,long len)
{
  uniCpy(buff,len,orig);
  
  {
    uniChar *p = uniLast(buff,len,'.');
    
    if(p==NULL || p[1]==DIR_CHR)
      p = uniEndStr(buff);
    *p = '\0';
   
    uniTack(buff,len,suffix);
  }
  
  return buff;
}

/*
 * Construct the prefix portion of a file name -- to get its directory
 */
uniChar *fileNamePrefix(const uniChar *orig,uniChar *name,long len)
{
  uniChar *p;

  uniCpy(name,len,(uniChar*)orig);
  p = uniLast(name,len,DIR_CHR);	/* find the last directory separator char */
  if(p==NULL)
    name[0]='\0';		/* no prefix */
  else
    p[1] = '\0';		/* We have a prefix */
  return name;
}

/* return the final portion of a file name */
uniChar *FileTail(uniChar *n)
{
  if(n==NULL)
    return NULL;
  else{
    uniChar *p = uniLast(n,uniStrLen(n),DIR_SEP[0]);
    if(p!=NULL)
      return p+1;
    else
      return n;
  }
}

static void rmDts(cellpo fn,uniChar *buff,long len);

uniChar *rmDots(char *prefix,cellpo fn,uniChar *buff,long len)
{
  _uni((unsigned char*)prefix,buff,len);	/* initialize the output string buffer */
  rmDts(fn,buff,len);
  return buff;
}

static void rmDts(cellpo fn,uniChar *buff,long len)
{
  cellpo l,r;
  if(isSymb(fn))
    uniCat(buff,len,symVal(fn));	/* copy file name into the buffer */
  else if(IsString(fn))
    uniCat(buff,len,strVal(fn));	/* copy file name into the buffer */
  else if(isBinaryCall(fn,kdot,&l,&r) && isSymb(r)){
    rmDts(l,buff,len);
    uniTack(buff,len,".");
    uniCat(buff,len,symVal(r));
  }
  else if(isBinaryCall(fn,kslash,&l,&r)){
    rmDts(l,buff,len);
    uniTack(buff,len,"/");
    rmDts(r,buff,len);
  }
  else{
    reportErr(lineInfo(fn),"Illegal form of file name %w",fn);
  }
}


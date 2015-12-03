/* 
   File and directory handling functions
   (c) 1994-1999 Imperial College and F.G. McCabe

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  Contact: Francis McCabe <fgm@fla.fujitsu.com>

*/ 

#include "config.h"		/* pick up standard configuration header */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
 
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>

#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include <signal.h>

#include "april.h"
#include "process.h"
#include "dict.h"
#include "symbols.h"
#include "astring.h"		/* String handling interface */
#include "clock.h"
#include "fileio.h"
#include "term.h"
#include "sign.h"
#include "opaque.h"
#include "pool.h"
#include "encoding.h"
#include "formioP.h"                    /* need this 'cos we are installing a handler */

#define ICM_TAG_MASK 0xf0


static retCode fileOpaqueHdlr(opaqueEvalCode code,void *p,void *cd,void *cl);

/* We have our own local table of opened files so that we can keep track of the 
 * processes responsible for managing them 
 */
static poolPo atPool = NULL;

typedef struct _attachment_ *attachPo;
typedef struct _attachment_ {
  ioPo f;			/* What is the file involved? */
  processpo p;			/* What process is involved */
  ioMode mode;			/* What mode of use */
  attachPo prev;                // Previous attachment
} AttachRec;

static attachPo attachments = NULL;

/* This is used to attach a file to a process, so that when the file
   becomes available, the right process is kicked off
*/

retCode attachProcessToFile(ioPo f,processpo p,ioMode mode)
{
  attachPo a = attachments;
  
  while(a!=NULL){
    if(a->f==f && a->p==p){
      a->mode = mode;
      p->pc--;
      ps_suspend(p,wait_io);
      return Suspend;
    }
    else
      a = a->prev;
  }
  
  a = allocPool(atPool);
  
  a->f = f;
  a->p = p;
  a->mode = mode;
  a->prev = attachments;
  attachments = a;

  p->pc--;
  ps_suspend(p,wait_io);
  
  return Suspend;
}

void detachProcessFromFile(ioPo f,processpo p)
{
  attachPo a = attachments;
  
  while(a!=NULL){
    if(a->f==f && a->p==p){
      if(a==attachments)
        attachments = a->prev;
      else{
        attachPo b = attachments;
        
        while(b->prev!=a)
          b = b->prev;
        
        b->prev = a->prev;
      }
      
      freePool(atPool,a);
      return;
    }
    else
      a = a->prev;
  }
}

/* return True if a file is really an executable program or not.
   Not checked out for windows at this time
   */
logical executableFile(char *file)
{
  uid_t id = geteuid();
  gid_t grp = getegid();
  struct stat buf;
  
  if(stat(file,&buf)==-1 || !S_ISREG(buf.st_mode))
    return False;

  if(buf.st_mode&S_IXOTH)	/* anyone can execute this file */
    return True;
  if(buf.st_mode&S_IXGRP){
    if(buf.st_gid==grp)
      return True;
    else{
     int ngroups = getgroups(0,NULL); /* The number of supplementary groups */
     gid_t groups[NGROUPS_MAX+1];

     if(ngroups>0){
       int i;

       getgroups(ngroups,groups);

       for(i=0;i<ngroups;i++)
	 if(groups[i]==buf.st_gid)
	   return True;                 // Are we in the group that's allowed to execute?
     }
    }
  }
  if(buf.st_mode&S_IXUSR)
    return id==buf.st_uid;
  return False;
}


/*
 * Set up for asynchronous IO interrupt handling ....
 */
#ifdef PROCTRACE
static WORD32 ioInts = 0;
#endif

static void ioWakeup(int ignored);

static void setupSIGIO(void)
{
  struct sigaction act;

  act.sa_handler = ioWakeup;
  act.sa_flags = 0;
  
  sigemptyset(&act.sa_mask);

  sigaction(SIGIO,&act,NULL);
}

static void ioWakeup(int ignored)
{
#ifdef PROCTRACE
  ioInts++;
#endif
  wakeywakey=True;

  setupSIGIO();			/* re-establish in case its solaris */
}

int set_in_fdset(fd_set *set)
{
  int max = 0;
  attachPo a = attachments;

  FD_ZERO(set);
  
  while(a!=NULL){
    if(a->mode==input && ps_state(a->p)==wait_io){
      int fd = fileNumber(O_FILE(a->f));
      FD_SET(fd,set);
      if(fd>=max)
	max = fd+1;
    }
    a = a->prev;
  }
  return max;
}

int set_out_fdset(fd_set *set)
{
  attachPo a = attachments;
  int max = 0;

  FD_ZERO(set);
  
  while(a!=NULL){
    ioPo f = a->f;
    
    if(a->mode==output && (isOutReady(f)!=Ok || ps_state(a->p)==wait_io)){
      int fd = fileNumber(O_FILE(f));
      FD_SET(fd,set);
      if(fd>=max)
	max = fd+1;
    }
    
    a = a->prev;
  }

  return max;
}

void trigger_io(fd_set *inset,fd_set *outset,int max)
{
  int i;

  for(i=0;i<max;i++){
    if(FD_ISSET(i,inset)){
      attachPo a = attachments;
      
      while(a!=NULL){
        if((fileNumber(O_FILE(a->f)))==i && a->mode==input){
          processpo P = a->p;
          
          if(P!=NULL && ps_state(P)==wait_io)
	    add_to_run_q(P,False);
	}
	a = a->prev;
      }
    }

    if(FD_ISSET(i,outset)){
      attachPo a = attachments;
      
      while(a!=NULL){
        if((fileNumber(O_FILE(a->f)))==i && a->mode==output){
          processpo P = a->p;
          
          if(P!=NULL && ps_state(P)==wait_io)
	    add_to_run_q(P,False);
	}
	a = a->prev;
      }
    }
  }
}

ioEncoding checkEncoding(objPo p)
{
  if(p==krawencoding)                  /* Raw encoding tag */
    return rawEncoding;
  else if(p==kutf16encoding)           /* UTF16 encoding tag */
    return utf16Encoding;
  else if(p==kutf8encoding)            /* UTF8 encoding tag */
    return utf8Encoding;
  else
    return unknownEncoding;
}

/*
 * April escape function interface
 */
 
/*
 * mergeURL - merge a base URL, system root and user root with application URL
 *            to get final URL
 */

retCode m_mergeURL(processpo p,objPo *args)
{
  objPo t1 = args[1];           // The base URL
  objPo t2 = args[0];            // The requested URL
  
  if(!isListOfChars(t1))
    return liberror("__mergeURL", 4, "1st argument should be string",einval);
  else if(!isListOfChars(t2))
    return liberror("__mergeURL", 4, "2nd argument should be string",einval);
  else{
    WORD32 l1 = ListLen(t1)+1;
    WORD32 l2 = ListLen(t2)+1;
    WORD32 mx = l1+l2;            // Maximum actual URL size
    uniChar base[l1];
    uniChar user[l2];
    uniChar act[mx];
    
    StringText(t1,base,l1);
    StringText(t2,user,l2);
    
    mergeURLs(base,user,act,mx);
    
    args[1] = allocateString(act);
    return Ok;
  }
}

/*
 * mergeURL - merge a base URL, system root and user root with application URL
 *            to get final URL
 */

retCode m_parseURL(processpo p,objPo *args)
{
  objPo t1 = args[0];           // The requested URL
  
  if(!isListOfChars(t1))
    return liberror("__parseURL", 1, "1st argument should be string",einval);
  else{
    WORD32 len = ListLen(t1)+1;
    uniChar url[len];
    uniChar host[len];
    uniChar user[len];
    uniChar pass[len];
    uniChar path[len];
    uniChar query[len];
    WORD32 port;
    urlScheme scheme;
    objPo el;
    
    StringText(t1,url,len);
    
    if(parseURI(url,&scheme,user,NumberOf(user),pass,NumberOf(pass),
                host,NumberOf(host),&port,path,NumberOf(path),query,NumberOf(query))!=Ok)
      return liberror("__parseURL",1,"invalid URL",einval);
    else{
      uniChar proto[16];
      
      args[0] = allocateTpl(6);
      
      switch(scheme){
        case httpUrl:
          strMsg(proto,NumberOf(proto),"http");
          break;
        case ftpUrl:
          strMsg(proto,NumberOf(proto),"ftp");
          break;
        case newsUrl:
          strMsg(proto,NumberOf(proto),"news");
          break;
        case nntpUrl:
          strMsg(proto,NumberOf(proto),"nntp");
          break;
        case telnetUrl:
          strMsg(proto,NumberOf(proto),"telnet");
          break;
        case gopherUrl:
          strMsg(proto,NumberOf(proto),"gopher");
          break;
        case waisUrl:
          strMsg(proto,NumberOf(proto),"wais");
          break;
        case mailtoUrl:
          strMsg(proto,NumberOf(proto),"mailto");
          break;
        case fileUrl:
          strMsg(proto,NumberOf(proto),"file");
          break;
        case prosperoUrl:
          strMsg(proto,NumberOf(proto),"prospero");
          break;
        case sysUrl:
          strMsg(proto,NumberOf(proto),"sys");
          break;
        case unknownUrl:
          strMsg(proto,NumberOf(proto),"unknown");
          break;
      }
      
      el = allocateString(proto);
      updateTuple(args[0],0,el);
    
      el = allocateString(host);
      updateTuple(args[0],1,el);
    
      el = allocateInteger(port);
      updateTuple(args[0],2,el);
    
      el = allocateString(user);
      updateTuple(args[0],3,el);
    
      el = allocateString(pass);
      updateTuple(args[0],4,el);
    
      el = allocateString(path);
      updateTuple(args[0],5,el);
    
      return Ok;
    }
  }
}

/*
 * openURL(url,encoding)
 * 
 * opens a URL in read mode, with a specific encoding
 */

retCode m_openURL(processpo p,objPo *args)
{
  objPo t1 = args[1];           // The URL
  objPo t2 = args[0];           // The encoding
  ioPo file=NULL;

  if(!isListOfChars(t1))
    return liberror("__openURL", 2, "1st argument should be string",einval);
  else if(!isSymb(t2))
    return liberror("__openURL", 2, "2nd argument should be encoding",einval);
  else if(!p->priveleged)
    return liberror("__openURL", 2, "permission denied",eprivileged);
  else{
    WORD32 len = ListLen(t1)+1;
    uniChar fn[len];
  
    file = openURL(aprilSysPath,StringText(t1,fn,len),checkEncoding(t2));
    
    if(file==NULL || configureIo(O_FILE(file),turnOffBlocking)==Error){
      uniChar em[MAX_SYMB_LEN];	/* error message buffer */
      strMsg(em,NumberOf(em),"problem in opening file %U",fn);
      return Uliberror("__open",2,em,efail);
    }
    args[1] = allocOpaqueFilePtr(file); /* return open file descriptor */
    return Ok;
  }
}

/*
 * createURL(url,encoding)
 * 
 * opens a URL in write mode, with a specific encoding
 */

retCode m_createURL(processpo p,objPo *args)
{
  objPo t1 = args[1];           // The URL
  objPo t2 = args[0];           // The encoding
  ioPo file=NULL;

  if(!isListOfChars(t1))
    return liberror("__createURL", 2, "1st argument should be string",einval);
  else if(!isSymb(t2))
    return liberror("__createURL", 2, "2nd argument should be encoding",einval);
  else if(!p->priveleged)
    return liberror("__createURL", 2, "permission denied",eprivileged);
  else{
    WORD32 len = ListLen(t1)+1;
    uniChar fn[len];
  
    file = createURL(aprilSysPath,StringText(t1,fn,len),checkEncoding(t2));
    
    if(file==NULL || configureIo(O_FILE(file),turnOffBlocking)==Error)
      return liberror("__createURL",2,"problem in opening file",efail);

    args[1] = allocOpaqueFilePtr(file); /* return open file descriptor */
    return Ok;
  }
}


/*
 * fopen(fd,fm)
 * 
 * opens a file with read, write or append mode
 */

retCode m_fopen(processpo p,objPo *args)
{
  objPo t1 = args[1];
  objPo t2 = args[0];

  if(!isListOfChars(t1))
    return liberror("__open", 2, "1st argument should be string",einval);
  else if(!isSymb(t2))
    return liberror("__open", 2, "2nd argument should be file_open_mode",einval);
  else if(!p->priveleged)
    return liberror("__open", 2, "permission denied",eprivileged);
  else{
    WORD32 len = ListLen(t1)+1;
    uniChar fn[len];
    ioPo file=NULL;
    uniChar *mode = SymVal(t2);
  
    StringText(t1,fn,len);

    if(uniIsLitPrefix(fn,"file:///"))
      memmove(fn,&fn[strlen("file:///")],(uniStrLen(fn)-strlen("file:///")+1)*sizeof(uniChar));
  
    if(uniIsLit(mode,"_open_read"))
      file = openInFile(fn,unknownEncoding);
    else if(uniIsLit(mode,"_open_write"))
      file = openOutFile(fn,unknownEncoding);
    else if(uniIsLit(mode,"_open_read_write"))
      file = openInOutFile(fn,unknownEncoding);
    else if(uniIsLit(mode,"_open_append"))
      file = openAppendFile(fn,unknownEncoding);
    else if(uniIsLit(mode,"_create_append"))
      file = openInOutAppendFile(fn,unknownEncoding);
    else if(uniIsLit(mode,"_create_write"))
      file = newOutFile(fn,unknownEncoding);
    else return liberror("__open", 2, "2nd argument should be _file_open_mode",einval);

    if(file==NULL || configureIo(O_FILE(file),turnOffBlocking)==Error){
      uniChar em[MAX_SYMB_LEN];	/* error message buffer */
      strMsg(em,NumberOf(em),"problem in opening file %U",fn);
      return Uliberror("__open",2,em,efail);
    }

    args[1] = allocOpaqueFilePtr(file); /* return open file descriptor */
    return Ok;
  }
}

retCode m_dupfile(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__dupfile",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__dupfile",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    incRefCount(file);		/* increment file reference count */
    return Ok;
  }
}


/*
 * fclose()
 * 
 * close file or pipe 
 */

retCode m_fclose(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__close",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__close",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    detachProcessFromFile(file,p);

    closeFile(file);
    return Ok;
  }
}

retCode m_stdfile(processpo p,objPo *args)
{
  WORD32 fno = IntVal(args[0]);

  if(!p->priveleged)
    return liberror("__stdfile",1,"permission denied",eprivileged);

  switch(fno){
  case 0:
    args[0]=allocOpaqueFilePtr(OpenStdin()); /* return stdin file descriptor */
    return Ok;
  case 1:
    args[0]=allocOpaqueFilePtr(OpenStdout()); /* return stdout file descriptor */
    return Ok;
  case 2:
    args[0]=allocOpaqueFilePtr(OpenStderr()); /* return stderr file descriptor */
    return Ok;
  default:
    return liberror("__stdfile",1,"invalid file no",einval);
  }
}

/*
 * eof()
 * returns "true" if the file associated with process is at the end of file
 * This function reads character forward in order to position the EOF flag.
 * the character is then un-read by ungetc()
 */
retCode m_eof(processpo p,objPo *args)
{
  objPo t1 = args[0];
  if(!p->priveleged)
    return liberror("__eof",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__eof",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    if(isReadingFile(file)!=Ok)
      return liberror("__eof",1,"permission denied",eprivileged);

    if(isInReady(file)==Ok){
      detachProcessFromFile(file,p);
      if(isFileAtEof(file)==Eof)
	args[0] = ktrue;
      else
	args[0] = kfalse;
      return Ok;
    }
    else
      return attachProcessToFile(file,p,input);
  }
}

/*
 * ready()
 * returns "true" if the file is ready -- i.e., an eof test would not suspend
 */
retCode m_ready(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__ready",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__ready",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    if(isReadingFile(file)==Ok){
      if(isInReady(file)==Ok)
	args[0] = ktrue;
      else
	args[0] = kfalse;
    }
    else if(isWritingFile(file)==Ok){
      if(isOutReady(file)==Ok)
	args[0] = ktrue;
      else
	args[0] = kfalse;
    }
    else
      return liberror("__ready",1,"permission denied",eprivileged);
    
    return Ok;
  }
}

static objPo allocateByteBlock(uniChar *base,WORD32 size)
{
  assert(size>=0);
  
  reserveSpace(size*(IntegerCellCount+ListCellCount));
  
  {
    objPo l = emptyList;
    uniChar *p = base+size;
    
    while(p>base){
      objPo chr = allocateInteger(*--p);
      l = allocatePair(&chr,&l);
    }
    
    return l;
  }
}

/*
 * inbytes(file,count)
 * 
 * get a block of bytes from file attached to process, returned as a list of numbers
 * Modified to allow suspension during the read of a block of bytes
 */

retCode m_inbytes(processpo p,objPo *args)
{
  objPo t1 = args[1];

  if(!p->priveleged)
    return liberror("__inbytes",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__inbytes",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);
    ioPo str = (ioPo)ps_client(p);
    int count,i;
    objPo t2 = args[0];

    if(isReadingFile(file)!=Ok)
      return liberror("__inbytes",2,"permission denied",eprivileged);

    if(!IsInteger(t2) || (count=IntVal(t2))<=0)
      return liberror("__inbytes",2,"2nd argument should be a positive integer",einval);

    ps_set_client(p,NULL); /* clear client-information from process */
    detachProcessFromFile(file,p);

    if(str==NULL)
      str=openOutStr(utf16Encoding);

    for(i=outCPos(O_IO(str));i<count;i++){
      uniChar ch;

      switch(inChar(file,&ch)){	/* Attempt to read a character */
      case Eof:
	if(emptyOutStr(O_STRING(str))==Ok){	/* have we read anything? */
	  closeFile(str);
	  return liberror("__inbytes",2,"end of file",eeof);
	}
	else{
	  count = i;
	  break;
	}
      case Ok:		/* We are able to read byte successfully */
	outChar(str,ch);
	continue;
      case Fail:
      case Interrupt:
	ps_set_client(p,str);
	return attachProcessToFile(file,p,input);;
      default:
	return liberror("__inbytes",2,"problem with read",eio);
      }
    }

    {
      WORD32 len;
      uniChar *text = getStrText(O_STRING(str),&len);
	  
      args[1] = allocateByteBlock(text,len);
    }
    closeFile(str);
    return Ok;
  }
}

/*
 * inchars(file,count)
 * 
 * get a block of bytes from file attached to process, returned as a string
 * Modified to allow suspension during the read of a block of bytes
 */

retCode m_inchars(processpo p,objPo *args)
{
  objPo t1 = args[1];

  if(!p->priveleged)
    return liberror("__inchars",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__inchars",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);
    ioPo str = (ioPo)ps_client(p);
    int count,i;
    objPo t2 = args[0];

    if(isReadingFile(file)!=Ok)
      return liberror("__inchars",2,"permission denied",eprivileged);

    if(!IsInteger(t2) || (count=IntVal(t2))<=0)
      return liberror("__inchars",2,"2nd argument should be a positive integer",einval);

    ps_set_client(p,NULL); /* clear client-information from process */
    detachProcessFromFile(file,p);

    if(str==NULL)
      str=openOutStr(utf16Encoding);

    for(i=outCPos(O_IO(str));i<count;i++){
      uniChar ch;

      switch(inChar(file,&ch)){	/* Attempt to read a character */
      case Eof:
	if(emptyOutStr(O_STRING(str))==Ok){	/* have we read anything? */
	  closeFile(str);
	  return liberror("__inchars",2,"end of file",eeof);
	}
	else{
	  count = i;
	  break;
	}
      case Ok:		/* We are able to read byte successfully */
	outChar(str,ch);
	continue;
      case Fail:
      case Interrupt:
	ps_set_client(p,str);
	return attachProcessToFile(file,p,input);;
      default:
	return liberror("__inchars",2,"problem with read",eio);
      }
    }

    {
      WORD32 len;
      uniChar *text = getStrText(O_STRING(str),&len);
	  
      args[1] = allocateSubString(text,len);
    }
    closeFile(str);
    return Ok;
  }
}

/*
 * inascii(count)
 * 
 * get a single byte from file attached to process, returned as an ASCII
 */

retCode m_inascii(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__inascii",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__inascii",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    if(isReadingFile(file)!=Ok)
      return liberror("__inascii",1,"permission denied",eprivileged);

    detachProcessFromFile(file,p);

    switch(isInReady(file)){
    case Ok:{
      register uniChar ch = inCh(file);

      if(ch==uniEOF)		/* check for end-of-file */
	return liberror("__inascii",1,"end of file",eeof); /* eof error */
      else{
	args[0]=allocateInteger(ch);
	return Ok;
      }
    }
    case Fail:
    case Interrupt:
      return attachProcessToFile(file,p,input);
    case Eof:
      return liberror("__inascii",1,"end of file",eeof); /* eof error */
    default:
      return liberror("__inascii",1,"error in accessing",eio); /* eof error */
    }
  }
}

/*
 * inchar(count)
 * 
 * get a single character from file attached to process, 
 * returned as a single character symbol
 */

retCode m_inchar(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__inchar",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__inchar",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    if(isReadingFile(file)!=Ok)
      return liberror("__inchar",1,"permission denied",eprivileged);

    detachProcessFromFile(file,p);

    switch(isInReady(file)){
    case Ok:{
      register uniChar ch = inCh(file);

      if(ch==uniEOF)		/* check for end-of-file */
	return liberror("__inchar",1,"end of file",eeof); /* eof error */
      else{
	uniChar sym[2] = {ch,0};
	args[0]=newUniSymbol(sym);
	return Ok;
      }
    }
    case Fail:
    case Interrupt:
      return attachProcessToFile(file,p,input);
    case Eof:
      return liberror("__inchar",1,"end of file",eeof); /* eof error */
    default:
      return liberror("__inchar",1,"error in accessing",eio); /* eof error */
    }
  }
}

/*
 * inline(file)
 * 
 * get a single line from a file, returned as a string
 */
retCode m_inline(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__inline",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__inline",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);
    ioPo str = (ioPo)ps_client(p);
    uniChar txt[1];

    if(isReadingFile(file)!=Ok)
      return liberror("__inline",1,"permission denied",eprivileged);

    ps_set_client(p,NULL); /* clear client-information from process */
    detachProcessFromFile(file,p);

    if(str==NULL)
      str=openOutStr(utf16Encoding);

    while(True){
      switch(inChar(file,txt)){
      case Eof:
	if(emptyOutStr(O_STRING(str))==Ok){
	  closeFile(str);
	  return liberror("__inline",1,"end of file",eeof);
	}
	else{
	  WORD32 len;
	  uniChar *text = getStrText(O_STRING(str),&len);
	  
	  args[0] = allocateSubString(text,len);
	  closeFile(str);
	  return Ok;
	}
      case Ok:
	if(txt[0]==NEW_LINE){
	  WORD32 len;
	  uniChar *text = getStrText(O_STRING(str),&len);
	  
	  args[0] = allocateSubString(text,len);
	  closeFile(str);
	  return Ok;
	}
	else
	  outChar(str,txt[0]);
	continue;
      case Interrupt:
      case Fail:
	ps_set_client(p,str);
	return attachProcessToFile(file,p,input);
      default:
	return liberror("__inline",1,"problem with read",eio);
      }
    }
  }
}

/*
 * intext(file)
 * 
 * return all available text
 * if the file is not ready at the time of reading then IoSuspend is returned.
 * Otherwise, all text until EOF or not-ready is collected and returned.
 */
retCode m_intext(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__intext",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__intext",1,"argument is not a valid file",einval);
  else{
    ioPo file = opaqueFilePtr(t1);
    ioPo str = (ioPo)ps_client(p);

    if(isReadingFile(file)!=Ok)
      return liberror("__intext",1,"file not open for reading",efail);

    ps_set_client(p,NULL); /* clear client-information from process */
    detachProcessFromFile(file,p);

    if(str==NULL)
      str=openOutStr(utf16Encoding);

    switch(isInReady(file)){
    case Ok:{
      while(True){
	uniChar txt[1];

	switch(inChar(file,txt)){
	case Eof:
	  if(emptyOutStr(O_STRING(str))==Ok){
	    closeFile(str);
	    return liberror("__intext",1,"end of file",eeof);
	  }
	  else{         // We read some text, so return it
	    WORD32 len;
	    uniChar *text = getStrText(O_STRING(str),&len);
	  
	    args[0] = allocateSubString(text,len);
	    closeFile(str);
	    return Ok;
	  }
	case Ok:
	  outChar(str,txt[0]);
	  continue;
	case Interrupt:
	case Fail:{		/* we have read as much as we can */
	  WORD32 len;
	  uniChar *text = getStrText(O_STRING(str),&len);
	  
	  args[0] = allocateSubString(text,len);
	  closeFile(str);
	  return Ok;
	}
	default:
	  return liberror("__intext",1,"problem with read",eio);
	}
      }
    }
    case Interrupt:
    case Fail:
      ps_set_client(p, str);
      return attachProcessToFile(file,p,input);
    case Eof:
      return liberror("__intext",1,"end of file",eeof); /* eof error */
    default:
      return liberror("__intext",1,"error in accessing",eio); /* other error */
    }
  }
}

/*
 * binline()
 * 
 * read a balanced line from a file, returned as a string
 */

static retCode balRead(ioPo in,ioPo out,uniChar until)
{
  uniChar ch;

  while((ch=inCh(in))!=until && ch!=uniEOF){
    outChar(out,ch);
    switch(ch){
    case '(':{
      char unt = ')';
      retCode ret = balRead(in,out,unt);
      outChar(out,unt);
      if(ret!=Ok)
	  return ret;
      break;
    }

    case '[':{
      char unt = ']';
      retCode ret = balRead(in,out,unt);
      outChar(out,unt);
      if(ret!=Ok)
	  return ret;
      break;
    }
    case '{':{
      char unt = '}';
      retCode ret = balRead(in,out,unt);
      outChar(out,unt);
      if(ret!=Ok)
	  return ret;
      break;
    }
    case '"':
      while((ch=inCh(in))!=NEW_LINE && ch!=uniEOF && ch!='"')
	outChar(out,ch);
      if(ch==uniEOF)
	return Eof;
      else if(ch!='"')
	return Error;
      else
	outChar(out,ch);
      break;

    case ')':
    case ']':
    case '}':
      return Error;		/* we are not expecting this */

    default:
      ;
    }
  }

  if(ch==uniEOF)
    return Eof;
  else
    return Ok;
}

retCode m_binline(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__binline",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__binline",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    if(isReadingFile(file)!=Ok)
      return liberror("__binline",1,"permission denied",eprivileged);

    if(isInReady(file)==Ok){
      ioPo str = openOutStr(utf16Encoding);

      detachProcessFromFile(file,p);

      switch(balRead(file,str,NEW_LINE)){
      case Error:
	closeFile(str);
	return liberror("__binline",1,"syntax error",efail);

      case Eof:
	if(emptyOutStr(O_STRING(str))==Ok){
	  closeFile(str);
	  return liberror("__binline",1,"end of file",eeof);
	}

      case Ok:{
	WORD32 len;
	uniChar *text = getStrText(O_STRING(str),&len);
	  
	args[0] = allocateSubString(text,len);
	closeFile(str);
	return Ok;
      }
      default:
	return liberror("__binline",1,"problem",eio);
      }
    }
    else
      return attachProcessToFile(file,p,input);
  }
}

/* read in an April term */
/* __read(file) -> any */
retCode m_read(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__read",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__read",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    if(isReadingFile(file)!=Ok)
      return liberror("__read",1,"permission denied",eprivileged);

    if(skipBlanks(file)!=Ok)
      return attachProcessToFile(file,p,input);
    else{
      objPo el = kvoid;
      void *root = gcAddRoot(&el);
      retCode ret = ReadData(file,&el);

      detachProcessFromFile(file,p);

      switch(ret){
      case Ok:			/* read something */
	args[0]=el;
	break;
      case Eof:
	ret = liberror("__read",1,"end of file",eeof); /* end of file error */
	break;
      case Suspend:
	ret = attachProcessToFile(file,p,input);
	break;
      default:
	ret = liberror("__read",1,"syntax error in file",efail);
      }

      gcRemoveRoot(root);
      return ret;
    }
  }
}

/*
 * Read a coded message from a process file
 */
typedef struct{
  ioPo str;
  WORD64 count;
  WORD32 len;
} decodeData;

retCode m_decode(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__decode",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__decode",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    if(isReadingFile(file)!=Ok)
      return liberror("__decode",1,"permission denied",eprivileged);

    /* input may suspend */
    switch(isInReady(file)){
    case Ok:{
      objPo el = kvoid;
      void *root = gcAddRoot(&el);
      decodeData *ptr = (decodeData*)ps_client(p);
      retCode res = Ok;
      WORD64 i;

      detachProcessFromFile(file,p);
      ps_set_client(p,NULL);

      if(ptr==NULL){
        int ch = inCh(file);

        if((ch&ICM_TAG_MASK)!=trmString)
          return liberror("__decode",1,"not legal coded data",einval);

        ptr = (decodeData*)malloc(sizeof(decodeData));

        ptr->str = openIoStr(rawEncoding);
        ptr->len = (ch&0xf);       /* set the count to counting the initial length */
        ptr->count = 0;

        if(ptr->len>0){               /* We have to read the initial length */
          while(res==Ok && ptr->len>0){
            uniChar ch;
            res = inChar(file,&ch);    /* read one of the length characters */
            
            if(res==Ok){
              ptr->count = (ptr->count<<8)|ch;
              ptr->len--;
            }
          }
        }
      }

      for(i=outBPos(ptr->str);res==Ok&&i<ptr->count;i++){
        byte B;

        res = inByte(file,&B);
        if(res==Ok)
          outByte(ptr->str,B); /* collect the data into our temporary string */
      }
          
      switch(res){
      case Eof:
        free(ptr);
        return liberror("__decode",1,"unexpected end of file",eeof);
      default:
        free(ptr);
        return liberror("__decode",1,"problem in decoding",efail);
      case Suspend:
      case Interrupt:
      case Fail:
        ps_set_client(p,ptr);
        gcRemoveRoot(root);
        return attachProcessToFile(file,p,input);
      case Ok:{
        rewindStr(O_STRING(ptr->str)); /* Rewind the string file for reading */
          
        res = decodeICM(ptr->str,&el,verifyCode);
        closeFile(ptr->str);
        free(ptr);
          
        args[0]=el;
        gcRemoveRoot(root);
          
        switch(res){
        case Eof:
          return liberror("__decode",1,"unexpected end of file",eeof);
        case Ok:
          return Ok;
        default:
          return liberror("__decode",1,"unexpected error",einval);
        }
      }
      }
    }
    case Suspend:
    case Interrupt:
    case Fail:
      return attachProcessToFile(file,p,input);
    default:
      return liberror("__decode",1,"error in accessing",eio); 
    }
  }
}

/*
 * outch(file,ch)
 * 
 * write a single character on file controlled by process
 */

retCode m_outch(processpo p,objPo *args)
{
  objPo t1 = args[1];

  if(!p->priveleged)
    return liberror("__outch",2,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__outch",2,"invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);
    objPo t2 = args[0];

    if(isWritingFile(file)!=Ok)
      return liberror("__outch",2,"permission denied",eprivileged);
    else if(!isSymb(t2))
      return liberror("__outch",2,"argument should be a symbol",einval);
    else{
      uniChar *text = (uniChar *)ps_client(p);

      ps_set_client(p,NULL);
      detachProcessFromFile(file,p);

      if(text==NULL)
	text = SymVal(t2);

      while(*text!='\0'){
	switch(outChar(file,*text)){
	case Ok:
	  text++;
	  continue;
	case Interrupt:
	case Fail:
	  ps_set_client(p,(void*)text);
	  return attachProcessToFile(file,p,output);
	default:
	  return liberror("__outch",2,"Problem in writing",eio);
	}
      }
      return Ok;
    }
  }
}

/*
 * outchar(file,ch)
 * 
 * write a string on file controlled by process
 */

retCode m_outchar(processpo p,objPo *args)
{
  objPo t1 = args[1];

  if(!p->priveleged)
    return liberror("__outchar",2,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__outchar",2,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);
    objPo t2 = args[0];

    if(isWritingFile(file)!=Ok)
      return liberror("__outchar",2,"permission denied",eprivileged);
    else if(!isListOfChars(t2))
      return liberror("__outchar",2,"argument should be a string",einval);
    else{
      unsigned WORD32 off = (unsigned WORD32)ps_client(p);
      unsigned WORD32 len = ListLen(t2);
      uniChar buffer[MAX_SYMB_LEN];
      uniChar *buff = (len<NumberOf(buffer)?buffer:(uniChar*)malloc(sizeof(uniChar)*(len+1)));
      uniChar *text = StringText(t2,buff,len+1);

      ps_set_client(p,(void*)0);
      detachProcessFromFile(file,p);

      while(off<len){
	switch(outChar(file,text[off])){
	case Ok:
	  off++;
	  continue;
	case Interrupt:
	case Fail:
	  ps_set_client(p,(void*)off);
	  if(buff!=buffer)
            free(buff);
	  return attachProcessToFile(file,p,output);
	default:
	  if(buff!=buffer)
            free(buff);
	  return liberror("__outchar",2,"Problem in writing",eio);
	}
      }
      if(buff!=buffer)
            free(buff);
      return Ok;
    }
  }
}

/*
 * outbytes(file,ch)
 * 
 * write a list of bytes on file controlled by process
 */

retCode m_outbytes(processpo p,objPo *args)
{
  objPo t1 = args[1];

  if(!p->priveleged)
    return liberror("__outbytes",2,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__outbytes",2,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);
    objPo t2 = args[0];

    if(isWritingFile(file)!=Ok)
      return liberror("__outbytes",2,"permission denied",eprivileged);
    else if(!isListOfInts(t2))
      return liberror("__outbytes",2,"argument should be a string",einval);
    else{
      unsigned WORD32 off = (unsigned WORD32)ps_client(p);
      unsigned WORD32 len = ListLen(t2);
      uniChar buffer[MAX_SYMB_LEN];
      uniChar *buff = (len<NumberOf(buffer)?buffer:(uniChar*)malloc(sizeof(uniChar)*(len+1)));
      
      {
        uniChar *txt = buff;
        objPo pt = t2;
    
        while(isNonEmptyList(pt)){
          *txt++=IntVal(ListHead(pt));
          pt = ListTail(pt);
        }
  
        *txt=0;
      }

      ps_set_client(p,(void*)0);
      detachProcessFromFile(file,p);

      while(off<len){
	switch(outChar(file,buff[off])){
	case Ok:
	  off++;
	  continue;
	case Interrupt:
	case Fail:
	  ps_set_client(p,(void*)off);
	  if(buff!=buffer)
	    free(buff);
	  return attachProcessToFile(file,p,output);
	default:
	  if(buff!=buffer)
	    free(buff);
	  return liberror("__outbytes",2,"Problem in writing",eio);
	}
      }
      if(buff!=buffer)
        free(buff);
      return Ok;
    }
  }
}

/* Write an encoded term onto a file channel */
retCode m_encode(processpo p,objPo *args)
{
  objPo t1 = args[1];

  if(!p->priveleged)
    return liberror("__encode",2,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__encode",2,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    if(isWritingFile(file)!=Ok)
      return liberror("__encode",2,"permission denied",eprivileged);
    else{
      ioPo tmp = openOutStr(rawEncoding);
      retCode ret = encodeICM(tmp,args[0]);
      WORD32 blen;
      uniChar *buffer = getStrText(O_STRING(tmp),&blen); /* access the block written so far */

      detachProcessFromFile(file,p);
      
      if(ret==Ok)
        ret = encodeInt(file,blen,trmString); /* How big is our code? */
      if(ret==Ok)
        ret = outText(file,buffer,blen);	/* complete package */

      closeFile(tmp);	                /* we are done with the temporary string file */

      if(ret!=Ok)
        return liberror("__encode",1,"error in encoding",efail);
      return Ok;			/* return Ok flag */
    }
  }
}

/*
 * ftell(file) -- report position in file
 */

retCode m_ftell(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__tell",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__tell",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);

    if(isFileOpen(file)!=Ok)
      return liberror("__tell",1,"permission denied",eprivileged);

    if(isReadingFile(file)==Ok)
      args[0] = allocateInteger(inBPos(file));
    else{
      flushFile(file);
      args[0] = allocateInteger(outBPos(file));
    }
    return Ok;
  }
}


/*
 * flush(file)
 * flush remaining buffered output of an output stream. 
 */

retCode m_flush(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__flush",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__flush",1,"Invalid argument",einval);
  else{
    ioPo file = opaqueFilePtr(t1);
    retCode ret;

    if(isFileOpen(file)!=Ok)
      return liberror("__flush",1,"permission denied",eprivileged);

    if(isWritingFile(file)!=Ok)
      return liberror("__flush",1,"file not open",efail);

    while((ret=flushFile(file))==Fail||ret==Interrupt)
      ;
    if(ret==Error)
      return liberror("__flush",1,"write error",eio);
    else
      return Ok;
  }
}

retCode m_flushall(processpo p,objPo *args)
{
  if(!p->priveleged)
    return liberror("__flushall",0,"permission denied",eprivileged);
  flushOut();
  return Ok;
}
  


/* Write a message to the April log file */
retCode m_log_msg(processpo p,objPo *args)
{
  uniChar buff[MAX_SYMB_LEN*4];

  if(!p->priveleged)
    return liberror("__log_msg",1,"permission denied",eprivileged);

  if(!isListOfChars(args[0]))
    return liberror("__log_msg",1,"argument should be a string",einval);

  logMsg(logFile,"%U",StringText(args[0],buff,NumberOf(buff)));
  return Ok;
}

/*
 * Try to load a code module from a named file
 */
retCode load_code_file(uniChar *sys,uniChar *url,objPo *tgt,logical verify)
{
  ioPo in = openURL(sys,url,rawEncoding);
  uniChar ch;

  if(in==NULL)
    return Error;
  else{
    configureIo(O_FILE(in),turnOnBlocking);

    ch = inCh(in);

    if(ch=='#'){			/* look for standard #!/.... header */
      if((ch=inCh(in))=='!'){
        while((ch=inCh(in))!=uniEOF && ch!=NEW_LINE)
  	  ;			        /* consume the interpreter statement */
      }
      else{
        unGetChar(in,ch);
        unGetChar(in,'#');
      }
    }
    else
      unGetChar(in,ch);
      
    {
      retCode ret = decodeICM(in,tgt,verify);

      closeFile(in);
 
      return ret;
    }
  }
}

retCode m_load(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!isListOfChars(t1))
    return liberror("_load_code", 1, "argument should be string",einval);
  else if(!p->priveleged)
    return liberror("_load_code", 1, "permission denied",eprivileged);
  else{
    uniChar url[MAX_SYMB_LEN];
    uniChar em[MAX_SYMB_LEN];
    
    StringText(t1,url,NumberOf(url));

    switch(load_code_file(aprilSysPath,url,&args[0],verifyCode)){
    case Ok:
      return Ok;
    case Error:
      if(uniStrLen(errorMsg)!=0)
        strMsg(em,NumberOf(em),"code in %U has a problem: %U",url,errorMsg);
      else
        strMsg(em,NumberOf(em),"cant load program from %U",url);
      return Uliberror("_load_code", 1, em,efail);
    case Eof:
      strMsg(em,NumberOf(em),"%U is empty or contains non-april code",url);
      return Uliberror("_load_code", 1, em,efail);
    case Switch:
      return Switch;		/* not very likely */
    case Suspend:
      return Suspend;		/* This could happen */
    case Space:
      return liberror("_load_code", 1, "no heap space",esystem);
    default:
      return liberror("_load_code",1,"problem in loading",eio);
    }
  }
}

/* This must be installed into outMsg */
static retCode cellMsg(ioPo f,void *data,WORD32 width,WORD32 precision,logical alt)
{
  objPo ptr = (objPo)data;
        
  if(ptr!=NULL){
    if(precision<=0)
      precision=32767;
    outCell(f,ptr,width,precision,alt);
  }
  else
    outStr(f,"(NULL)");
  return Ok;
}

static retCode typeMsg(ioPo f,void *data,WORD32 width,WORD32 precision,logical alt)
{
  objPo ptr = (objPo)data;
        
  if(ptr!=NULL){
    if(precision<=0)
      precision=32767;
    displayType(f,ptr,width,precision,alt);
  }
  else
    outStr(f,"(NULL)");
  return Ok;
}

static retCode listMsg(ioPo f,void *data,WORD32 width,WORD32 precision,logical alt)
{
  objPo ptr = (objPo)data;
        
  if(ptr!=NULL){
    if(precision<=0)
      precision=32767;
      
    if(!alt)
      outChar(f,'"');
    while(isNonEmptyList(ptr)){
      if(isChr(ListHead(ptr))){
        if(!alt)
          wStringChr(f,CharVal(ListHead(ptr)));
        else
          outChar(f,CharVal(ListHead(ptr)));
      }
      else
        return Error;
      ptr = ListTail(ptr);
    }
    if(!alt)
      outChar(f,'"');
  }
  else
    outStr(f,"(NULL)");
  return Ok;
}

void initFiles(void)
{
  atPool = newPool(sizeof(AttachRec),16);

  installMsgProc('w',cellMsg);	/* extend outMsg to cope with cell structures */
  installMsgProc('L',listMsg); // extend to allow lists of chars to be shown
  installMsgProc('t',typeMsg);     // Show structures as types

  setupSIGIO();
  registerOpaqueType(fileOpaqueHdlr,_F_OPAQUE_,NULL);
}

static retCode fileOpaqueHdlr(opaqueEvalCode code,void *p,void *cd,void *cl)
{
  switch(code){
  case showOpaque:{
    ioPo f = (ioPo)cd;
    ioPo ff = (ioPo)p;
    outMsg(f,"<<file %U>>",fileName(ff));
    return Ok;
  }
  default:
    return Error;
  }
}

objPo allocOpaqueFilePtr(ioPo file)
{
  return allocateOpaque(_F_OPAQUE_,(void *)file);
}

ioPo opaqueFilePtr(objPo p)
{
  return (ioPo)OpaqueVal(p);
}

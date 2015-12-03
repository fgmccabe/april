/* 
   Directory handling functions
   (c) 1994-2002 Imperial College and F.G. McCabe

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
#include <string.h>
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

/*
 *************************
 * Files and Directories *
 *************************
 */

/*
 * frm : remove a file
 */

retCode m_frm(processpo p,objPo *args)
{
  if(!p->priveleged)
    return liberror("__rm",1, "permission denied",eprivileged);

  if(!isListOfChars(args[0]))
    return liberror("__rm", 1, "argument should be a string",einval);
  else{
    WORD32 len = ListLen(args[0])+1;
    uniChar buffer[len];
    
    StringText(args[0],buffer,len);
    
    switch(rmURL(aprilSysPath,buffer)){
      case Ok:
        return Ok;
      default:
        return liberror("__rm",1,"cant remove file",efail);
    }
  }
}


/*
 * fmv : move or rename a file
 */

retCode m_fmv(processpo p,objPo *args)
{
  if(!p->priveleged)
    return liberror("__mv",2, "permission denied",eprivileged);

  if(!isListOfChars(args[0]))
    return liberror("__mv", 2, "argument should be a string",einval);
  else{
    WORD32 len1 = ListLen(args[1])+1;
    uniChar buffer1[len1];
    WORD32 len2 = ListLen(args[0])+1;
    uniChar buffer2[len2];
    
    StringText(args[1],buffer1,len1);
    StringText(args[0],buffer2,len2);
    
    switch(mvURL(aprilSysPath,buffer1,buffer2)){
      case Ok:
        return Ok;
      default:
        return liberror("__mv",2,"cant move file",efail);
    }
  }
}


/* file mode handling */
static unsigned int accessMode(objPo modes)
{
  unsigned int acmode = 0;		/* access mode */

  while(isNonEmptyList(modes)){
    objPo mode = ListHead(modes);

    if(mode==ksetuid)
      acmode |=S_ISUID;
    else if(mode==ksetgid)
      acmode |=S_ISGID;
    else if(mode==ksticky)
      acmode |=S_ISVTX;
    else if(mode==krusr)
      acmode |=S_IRUSR;
    else if(mode==kwusr)
      acmode |=S_IWUSR;
    else if(mode==kxusr)
      acmode |=S_IXUSR;
    else if(mode==krgrp)
      acmode |=S_IRGRP;
    else if(mode==kwgrp)
      acmode |=S_IWGRP;
    else if(mode==kxgrp)
      acmode |=S_IXGRP;
    else if(mode==kroth)
      acmode |=S_IROTH;
    else if(mode==kwoth)
      acmode |=S_IWOTH;
    else if(mode==kxoth)
      acmode |=S_IXOTH;

    modes = ListTail(modes);
  }
  return acmode;
}

static objPo modeAccess(unsigned int acmode)
{
  objPo modes = emptyList;
  void *root = gcAddRoot(&modes);

  if(acmode&S_ISUID)
    modes = allocatePair(&ksetuid,&modes);

  if(acmode&S_ISGID)
    modes = allocatePair(&ksetgid,&modes);

  if(acmode&S_ISVTX)
    modes = allocatePair(&ksticky,&modes);

  if(acmode&S_IRUSR)
    modes = allocatePair(&krusr,&modes);

  if(acmode&S_IWUSR)
    modes = allocatePair(&kwusr,&modes);

  if(acmode&S_IXUSR)
    modes = allocatePair(&kxusr,&modes);

  if(acmode&S_IRGRP)
    modes = allocatePair(&krgrp,&modes);

  if(acmode&S_IWGRP)
    modes = allocatePair(&kwgrp,&modes);

  if(acmode&S_IXGRP)
    modes = allocatePair(&kxgrp,&modes);

  if(acmode&S_IROTH)
    modes = allocatePair(&kroth,&modes);

  if(acmode&S_IWOTH)
    modes = allocatePair(&kwoth,&modes);

  if(acmode&S_IXOTH)
    modes = allocatePair(&kxoth,&modes);
  gcRemoveRoot(root);
  return modes;
}

/*
 * fmkdir : create a new directory
 */

retCode m_fmkdir(processpo p,objPo *args)
{
  objPo t1 = args[1];
  objPo t2 = args[0];

  if(!p->priveleged)
    return liberror("__mkdir",2,"permission denied",eprivileged);
  else if(!isListOfChars(t1))
    return liberror("__mkdir",2,"1st argument should be a string",einval);
  else{
    WORD32 len = ListLen(t1)+1;
    uniChar ufn[len];
    uniChar user[MAX_SYMB_LEN],pass[MAX_SYMB_LEN];
    uniChar host[MAX_SYMB_LEN],path[MAX_SYMB_LEN];
    uniChar query[MAX_SYMB_LEN];
    WORD32 port;
    urlScheme scheme;

    StringText(args[1],ufn,len);

    if(parseURI(ufn,&scheme,user,NumberOf(user),pass,NumberOf(pass),
                host,NumberOf(host),&port,path,NumberOf(path),query,NumberOf(query))!=Ok||
       scheme!=fileUrl)
      return liberror("__mkdir",2,"invalid directory name",einval);
    else{
      char fn[MAX_SYMB_LEN];
      
      _utf(path,(unsigned char*)fn,NumberOf(fn)); // Convert unicode string to regular chars 

      if(mkdir(fn,accessMode(t2)) == -1){
        switch(errno){
          case EEXIST:
	    return liberror("__mkdir",2,"directory exists",efail);
          default:
	    return liberror("__mkdir",2,"cant create directory",efail);
	}
      }
    }
    return Ok;
  }
}

/*
 * frmdir : remove a directory
 */
retCode m_frmdir(processpo p,objPo *args)
{
  if(!p->priveleged)
    return liberror("__rmdir",1,"permission denied",eprivileged);
  else if(!isListOfChars(args[0]))
    return liberror("__rmdir",1,"argument should be a string",einval);
  else{
    WORD32 len = ListLen(args[0])+1;
    uniChar ufn[len];
    uniChar user[MAX_SYMB_LEN],pass[MAX_SYMB_LEN];
    uniChar host[MAX_SYMB_LEN],path[MAX_SYMB_LEN];
    uniChar query[MAX_SYMB_LEN];
    WORD32 port;
    urlScheme scheme;

    StringText(args[0],ufn,len);

    if(parseURI(ufn,&scheme,user,NumberOf(user),pass,NumberOf(pass),
                host,NumberOf(host),&port,path,NumberOf(path),query,NumberOf(query))!=Ok||
       scheme!=fileUrl)
      return liberror("__rmdir",1,"invalid directory name",einval);
    else{
      char dir[MAX_SYMB_LEN];
      
      _utf(path,(unsigned char *)dir,NumberOf(dir)); // Convert unicode string to regular chars

tryAgain:
      if(rmdir(dir)==0)
        return Ok;
      else
        switch(errno){
          case EINTR:
            goto tryAgain;
          case EEXIST:
            return liberror("__rmdir",1,"directory not empty",efail);
          case EACCES:
            return liberror("__rmdir",1,"not owner",eprivileged);
          case EBUSY:
            return liberror("__rmdir",1,"file busy",efail);
          case ENOENT:
            return liberror("__rmdir",1,"non existent file",efail);
          case EPERM:
            return liberror("__rmdir",1,"not super user",eprivileged);
          default:
            return liberror("__rmdir",1,"cant remove directory",efail);
        }
    }
  }
}

/*
 * fchmod : set permissions on a file or directory
 */

retCode m_chmod(processpo p,objPo *args)
{
  objPo t1 = args[1];

  if(!p->priveleged)
    return liberror("__chmod",2,"permission denied",eprivileged);
  else if(!isListOfChars(t1))
    return liberror("__chmod",2,"1st argument should be a string",einval);
  else{
    WORD32 len = ListLen(t1)+1;
    uniChar ufn[len];
    uniChar user[MAX_SYMB_LEN],pass[MAX_SYMB_LEN];
    uniChar host[MAX_SYMB_LEN],path[MAX_SYMB_LEN];
    uniChar query[MAX_SYMB_LEN];
    WORD32 port;
    urlScheme scheme;

    StringText(t1,ufn,len);

    if(parseURI(ufn,&scheme,user,NumberOf(user),pass,NumberOf(pass),
                host,NumberOf(host),&port,path,NumberOf(path),query,NumberOf(query))!=Ok||
       scheme!=fileUrl)
      return liberror("__chmod",2,"invalid name",einval);
    else{
      char fn[MAX_SYMB_LEN];
      
      _utf(path,(unsigned char *)fn,NumberOf(fn)); // Convert unicode string to regular chars

  tryAgain:
      if(chmod(fn,accessMode(args[0])) == -1)
        switch(errno){
        case EINTR:
	  goto tryAgain;		/* A mega hack */
        case EACCES:
	  return liberror("__chmod",2,"access denied",eprivileged);
        case EPERM:
	  return liberror("__chmod",2,"not owner",eprivileged);
        default:
	  return liberror("__chmod",2,"cant change permission",eprivileged);
      }
    }
  }
  return Ok;
}

/*
 * fmode : gets permissions of a file or directory as a mode string
 */
 
/* check the permission bits on a file */
static retCode filePerms(char *file,unsigned WORD32 *mode)
{
  struct stat buf;

  if(stat(file, &buf) == -1)
    return Error;
  else{
    if(mode!=NULL)
      *mode = buf.st_mode;
    return Ok;
  }
}

retCode m_fmode(processpo p,objPo *args)
{
  if(!p->priveleged)
    return liberror("__fmode",1, "permission denied",eprivileged);
  else if(!isListOfChars(args[0]))
    return liberror("__fmode", 1, "argument should be a string",einval);
  else{
    WORD32 len = ListLen(args[0])+1;
    uniChar ufn[len];
    uniChar user[MAX_SYMB_LEN],pass[MAX_SYMB_LEN];
    uniChar host[MAX_SYMB_LEN],path[MAX_SYMB_LEN];
    uniChar query[MAX_SYMB_LEN];
    WORD32 port;
    urlScheme scheme;

    StringText(args[0],ufn,len);

    if(parseURI(ufn,&scheme,user,NumberOf(user),pass,NumberOf(pass),
                host,NumberOf(host),&port,path,NumberOf(path),query,NumberOf(query))!=Ok||
       scheme!=fileUrl)
      return liberror("__fmode",1,"invalid name",einval);
    else{
      char fn[MAX_SYMB_LEN];
      unsigned WORD32 acmode = 0;	                /* access mode */
      
      _utf(path,(unsigned char *)fn,NumberOf(fn)); // Convert unicode string to regular chars

      if(filePerms(fn,&acmode)!=Ok){
        uniChar msg[MAX_SYMB_LEN];
        strMsg(msg,NumberOf(msg),"cant stat file: `%U'",path);
        return Uliberror("__fmode",1,msg,efail);
      }
      else{
        args[0]=modeAccess(acmode);
        return Ok;
      }
    }
  }
}

/*
 * m_fls lists files in a directory
 */

retCode m_fls(processpo p,objPo *args)
{
  if(!p->priveleged)
    return liberror("__ls",1,"permission denied",eprivileged);
  else if(!isListOfChars(args[0]))
    return liberror("__ls",1,"argument should be a string",einval);
  else{
    DIR *directory;
    struct dirent  *ent;
    WORD32 len = ListLen(args[0])+1;
    uniChar ufn[len];
    uniChar user[MAX_SYMB_LEN],pass[MAX_SYMB_LEN];
    uniChar host[MAX_SYMB_LEN],path[MAX_SYMB_LEN];
    uniChar query[MAX_SYMB_LEN];
    WORD32 port;
    urlScheme scheme;

    StringText(args[0],ufn,len);

    if(parseURI(ufn,&scheme,user,NumberOf(user),pass,NumberOf(pass),
                host,NumberOf(host),&port,path,NumberOf(path),query,NumberOf(query))!=Ok||
       scheme!=fileUrl)
      return liberror("__ls",1,"invalid directory name",einval);
    else{
      char dir[MAX_SYMB_LEN];
      
      _utf(path,(unsigned char *)dir,NumberOf(dir)); // Convert unicode string to regular chars

      if((directory=opendir(dir)) == NULL)
        return liberror("__ls", 1,"cant access directory",efail);
      else{
        objPo last = emptyList;
        objPo dirEntry = emptyList;
        void *root = gcAddRoot(&last);

        gcAddRoot(&dirEntry);
    
        while((ent=readdir(directory)) != NULL){
        /* skip special entries "." and ".." */
          if(strcmp(ent->d_name, ".")!=0 && strcmp(ent->d_name, "..")!=0){
	    dirEntry = allocateCString(ent->d_name);
	    if(last==emptyList)
	      last = args[0] = allocatePair(&dirEntry,&emptyList);
	    else{
	      objPo tail = allocatePair(&dirEntry,&emptyList);
	      updateListTail(last,tail);
	      last = tail;
	    }
          }
        }

        closedir(directory);	/* Close the directory stream */

        gcRemoveRoot(root);
        return Ok;
      }
    }
  }
}


/*
 * m_file_type check out the type of the file
 */

retCode m_file_type(processpo p,objPo *args)
{
  if(!p->priveleged)
    return liberror("__file_type",1,"permission denied",eprivileged);
  else if(isListOfChars(args[0])){
    struct stat buf;
    WORD32 len = ListLen(args[0])+1;
    uniChar ufn[len];
    uniChar user[MAX_SYMB_LEN],pass[MAX_SYMB_LEN];
    uniChar host[MAX_SYMB_LEN],path[MAX_SYMB_LEN];
    uniChar query[MAX_SYMB_LEN];
    WORD32 port;
    urlScheme scheme;

    StringText(args[0],ufn,len);

    if(parseURI(ufn,&scheme,user,NumberOf(user),pass,NumberOf(pass),
                host,NumberOf(host),&port,path,NumberOf(path),query,NumberOf(query))!=Ok||
       scheme!=fileUrl)
      return liberror("__file_type",1,"invalid name",einval);
    else{
      char fn[MAX_SYMB_LEN];
      
      _utf(path,(unsigned char *)fn,NumberOf(fn)); // Convert unicode string to regular chars

      if(stat(fn, &buf) == -1)
        args[0]=newSymbol("_not_found");
      else if(S_ISFIFO(buf.st_mode))
        args[0]=newSymbol("_fifo_special");
      else if(S_ISCHR(buf.st_mode))
        args[0]=newSymbol("_char_special");
      else if(S_ISDIR(buf.st_mode))
        args[0]=newSymbol("_directory");
      else if(S_ISBLK(buf.st_mode))
        args[0]=newSymbol("_block_special");
      else if(S_ISREG(buf.st_mode))
        args[0]=newSymbol("_plain_file");
      else if(S_ISLNK(buf.st_mode))
        args[0]=newSymbol("_sym_link");
      else
        args[0]=newSymbol("_not_found");
      return Ok;
    }
  }
  else
    return liberror("__file_type",1,"argument should be string",einval);
}


/*
 * m_fstat() - return file status 
 */

retCode m_fstat(processpo p,objPo *args)
{
  struct stat buf;

  if(!p->priveleged)
    return liberror("__stat",1,"permission denied",eprivileged);
  else if(!isListOfChars(args[0]))
    return liberror("__stat",1,"argument should be string",einval);
  else{
    WORD32 len = ListLen(args[0])+1;
    uniChar ufn[len];
    uniChar user[MAX_SYMB_LEN],pass[MAX_SYMB_LEN];
    uniChar host[MAX_SYMB_LEN],path[MAX_SYMB_LEN];
    uniChar query[MAX_SYMB_LEN];
    WORD32 port;
    urlScheme scheme;

    StringText(args[0],ufn,len);

    if(parseURI(ufn,&scheme,user,NumberOf(user),pass,NumberOf(pass),
                host,NumberOf(host),&port,path,NumberOf(path),query,NumberOf(query))!=Ok||
       scheme!=fileUrl)
      return liberror("__stat",1,"invalid name",einval);
    else{
      char fn[MAX_SYMB_LEN];
      
      _utf(path,(unsigned char *)fn,NumberOf(fn)); // Convert unicode string to regular chars

      if(stat(fn, &buf) == -1){
        uniChar msg[MAX_SYMB_LEN];
        strMsg(msg,NumberOf(msg),"cant stat file: `%U'",path);
        return Uliberror("__stat",1,msg,efail);
      }
      else{
        objPo fstat = allocateConstructor(13);
        void *root = gcAddRoot(&fstat);
        objPo val = kvoid;

        gcAddRoot(&val);
      
        updateConsFn(fstat,kfstat);

        /* creation of the tuple of information about the file */

        val = allocateInteger(buf.st_dev); /* device file resides on */
        updateConsEl(fstat,0,val);
        val = allocateInteger(buf.st_ino); /* the file serial number */
        updateConsEl(fstat,1,val);
        val = allocateInteger(buf.st_mode); /* file mode */
        updateConsEl(fstat,2,val);
        val = allocateInteger(buf.st_nlink); /* number of hard links */
        updateConsEl(fstat,3,val);
        val = allocateInteger(buf.st_uid); /* user ID of owner */
        updateConsEl(fstat,4,val);
        val = allocateInteger(buf.st_gid); /* group ID of owner */
        updateConsEl(fstat,5,val);
        val = allocateInteger(buf.st_rdev); /* the device identifier*/
        updateConsEl(fstat,6,val);
        val = allocateInteger(buf.st_size); /* file size, in bytes */
        updateConsEl(fstat,7,val);
        val = allocateInteger(buf.st_atime); /* file last access time */
        updateConsEl(fstat,8,val);
        val = allocateInteger(buf.st_mtime); /* file last modify time */
        updateConsEl(fstat,9,val);
        val = allocateInteger(buf.st_ctime); /* file status change time */
        updateConsEl(fstat,10,val);
        val = allocateInteger(buf.st_blksize); /* preferred blocksize */
        updateConsEl(fstat,11,val);
        val = allocateInteger(buf.st_blocks); /* blocks allocated */
        updateConsEl(fstat,12,val);

        args[0] = fstat;

        gcRemoveRoot(root);
        return Ok;
      }
    }
  }
}

/*
 * ffile(file)
 * True if file is present, false otherwise 
 */

retCode m_ffile(processpo p,objPo *args)
{
  WORD32 len = ListLen(args[0]);
  uniChar fn[len+1];

  if(!p->priveleged)
    return liberror("__ffile",1,"permission denied",eprivileged);

  if(urlPresent(aprilSysPath,StringText(args[0],fn,len+1)))
    args[0]=ktrue;
  else
    args[0]=kfalse;

  return Ok;
}


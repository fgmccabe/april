/*
  Dynamic load of a function or procedure
  (c) 1994-2002 Imperial College and F.G.McCabe

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
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_LIBDL
#include <dlfcn.h>		/* dynamic load functions */
#endif

#include "april.h"
#include "process.h"
#include "dict.h"
#include "symbols.h"
#include "astring.h"		/* String handling interface */
#include "sign.h"		/* signature handling interface */
#include "debug.h"
#include "dll.h"		/* dynamic loader interface */

#define DL_OPAQUE 'D'

#ifdef HAVE_LIBDL
static logical opaque_registered = False;

static retCode dllOpaqueHdlr(opaqueEvalCode code,void *p,void *cd,void *cl);
#endif

retCode m_dll_load(processpo p,objPo *args)
{
#ifdef HAVE_LIBDL
  objPo t1 = args[1];		/* The file to load */
  objPo t2 = args[0];		/* The type signature to verify */

  if(!isListOfChars(t1))
    return liberror("__dll_load_", 2, "1st argument should be string",einval);
  else if(!isSymb(t2))
    return liberror("__dll_load_", 2, "2nd argument should be symbol",einval);
  else if(!p->priveleged)
    return liberror("__dll_load_", 2, "permission denied",eprivileged);
  else{
    uniChar url[MAX_SYMB_LEN];
    uniChar user[MAX_SYMB_LEN],pass[MAX_SYMB_LEN];
    uniChar host[MAX_SYMB_LEN],path[MAX_SYMB_LEN];
    uniChar query[MAX_SYMB_LEN];
    WORD32 port;
    urlScheme scheme;

    StringText(t1,url,NumberOf(url));

    if(parseURI(url,&scheme,user,NumberOf(user),pass,NumberOf(pass),
                host,NumberOf(host),&port,path,NumberOf(path),query,NumberOf(query))!=Ok)
      return liberror("__dll_load",2,"invalid url",einval);
    else{
      char fname[MAX_SYMB_LEN];

      switch(scheme){
      case fileUrl:
        break;
      case sysUrl:{
        uniChar sname[MAX_SYMB_LEN];
        strMsg(sname,NumberOf(sname),"%U/%U",aprilSysPath,path);

        if(parseURI(sname,&scheme,user,NumberOf(user),pass,NumberOf(pass),
                    host,NumberOf(host),&port,path,NumberOf(path),
                    query,NumberOf(query))!=Ok ||
           scheme!=fileUrl)
          return liberror("__dll_load",2,"invalid url",einval);
        break;
      }
      default:
        return liberror("__dll_load",2,"invalid url",einval);
      }

      if(regularFile(path,fname,NumberOf(fname))==Ok){
        void *h = dlopen(fname,RTLD_LAZY); /* open the file */

        if(h==NULL){
          const char *err = dlerror();
          return liberror("__dll_load_", 2,(char *)err,efail);
        }
        else{
          SignatureRec *dllSig = dlsym(h,"signature");
          const char *err = dlerror();

          if(err!=NULL)
            return liberror("__dll_load_",2,(char*)err,efail);
          else{
            if(uniCmp(SymVal(t2),dllSig->sig)==0){
              if(!opaque_registered){
                opaque_registered = True;
                registerOpaqueType(dllOpaqueHdlr,DL_OPAQUE,NULL);
              }
              dllSig->handle = h;	/* store the handle */
              args[1] = allocateOpaque(DL_OPAQUE,(void*)dllSig);
              return Ok;
            }
            else
              return liberror("__dll_load_", 2,"type signature doesnt match",efail);
          }
        }
      }
      else
        return liberror("__dll_load_", 2,"file not found",efail);
    }
  }
#else
  return liberror("__dll_load_", 2,"not implemented",efail);
#endif
}

#ifdef HAVE_LIBDL
static retCode dllOpaqueHdlr(opaqueEvalCode code,void *p,void *cd,void *cl)
{
  switch(code){
  case showOpaque:{
    ioPo f = (ioPo)cd;
    SignatureRec *info = *((SignatureRec**)p);

    outMsg(f,"<<dll %$>>",info->sig);
    return Ok;
  }
  default:
    return Error;
  }
}
#endif

retCode m_dll_call(processpo p,objPo *args)
{
#ifdef HAVE_LIBDL
  objPo t1 = args[2];		/* The loaded module to access */
  objPo t2 = args[1];		/* The function to call */
  objPo t3 = args[0];		/* The arguments to the call */
  WORD32 off;

  if(!IsOpaque(t1) ||
     OpaqueType(t1)!=DL_OPAQUE)
    return liberror("__dll_call_",3,"Invalid 1st argument",einval);
  else if(!p->priveleged)
    return liberror("__dll_call_", 3, "permission denied",eprivileged);
  else if(!IsInteger(t2))
    return liberror("__dll_call_", 3, "2nd argument should be an integer",einval);
  else{
    SignatureRec *info = (SignatureRec*)OpaqueVal(t1);

    if((off=IntVal(t2))<0 || off>=info->modLen)
      return liberror("__dll_call_", 3, "2nd argument invalid",einval);
    else{
      WORD32 ar = info->funs[off].ar;
      WORD32 i;
      WORD32 arg = 3;

      if(ar!=1){
	for(i=0;i<ar;i++)
	  args[--arg]=tupleArg(t3,i); /* push arguments on the stack */
      }
      else
	args[--arg]=t3;

      p->sp=&args[arg];
      return info->funs[off].fun(p,p->sp);
    }
  }
#else
  return liberror("__dll_call_", 3, "not implemented",efail);
#endif
}

retCode m_dll_close(processpo p,objPo *args)
{
#ifdef HAVE_LIBDL
  objPo t1 = args[0];

  if(!IsOpaque(t1) || OpaqueType(t1)!=DL_OPAQUE)
    return liberror("__dll_close_",3,"Invalid 1st argument",einval);
  else if(!p->priveleged)
    return liberror("__dll_close_", 3, "permission denied",eprivileged);
  else{
    SignatureRec *info = (SignatureRec*)OpaqueVal(t1);

    dlclose(info->handle);
    return Ok;
  }
#else
  return liberror("__dll_close_", 3, "not implemented",efail);
#endif
}


  

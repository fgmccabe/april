/*
  Handle functions for the April run-time system
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
#include "april.h"		/* Main april header file */
#include "unix.h"
#include "handle.h"		/* Handle interface */
#include "msg.h"
#include "symbols.h"
#include "astring.h"		/* april string functions */
#include "sort.h"
#include "term.h"
#include "hash.h"		/* access the hash functions */
#include "setops.h"

hashPo handles = NULL;

void initHandles(void)
{
  handles = NewHash(64,NULL,(compFun)uniCmp,NULL);
}

void verifyHandles(void)
{
  extern logical verifyHash(hashPo);

  if(handles!=NULL && !verifyHash(handles))
    syserr("problem in handle hash table");
}

objPo locateHandle(uniChar *tgt,uniChar *name,processpo p)
{
  handlePo h;
  uniChar pname[MAX_SYMB_LEN];
  
  strMsg(pname,NumberOf(pname),"%U:%U",tgt,name);
  
  h = (handlePo)Search(pname,handles);

  if(h==NULL){
    h = (handlePo)allocateHdl(p,pname);

    Install(h->name,h,handles);
  }
  else if(p!=NULL && h->p==NULL)
    h->p = p;			/* set in the process ID */
  return (objPo)h;
}

retCode m_make_handle(processpo p,objPo *args)
{
  objPo htpl = allocateConstructor(HANDLE_ARITY);
  void *root = gcAddRoot(&htpl);
  objPo tmp;

  updateConsFn(htpl,khandle);
  updateConsEl(htpl,TGT_OFFSET,args[1]); /* The handle target */
  updateConsEl(htpl,NAME_OFFSET,args[0]); /* The handle name */
  tmp=locateHandle(SymText(args[1]),SymText(args[0]),NULL);
  updateConsEl(htpl,PROCESS_OFFSET,tmp);
  
  args[1] = htpl;
  gcRemoveRoot(root);
  return Ok;
}

objPo parseHandle(processpo p,uniChar *text)
{
  uniChar *pt = uniSearch(text,uniStrLen(text),':');
  uniChar tgt[MAX_SYMB_LEN];

  if(pt!=NULL){
    int i=0;
    while(text!=pt)
      tgt[i++]=*text++;
    tgt[i]='\0';
    text++;
  }
  else
    uniCpy(tgt,NumberOf(tgt),text);           /* We MUST have a target field */
    
  return buildHandle(p,tgt,text);
}


objPo buildHandle(processpo p,uniChar *tgt,uniChar *name)
{
  objPo hndl = allocateConstructor(HANDLE_ARITY);
  objPo ttmp = kvoid;
  void *root = gcAddRoot(&hndl);
  WORD32 tlen = uniStrLen(tgt)+2;
  uniChar tbuff[tlen];
  WORD32 nlen = uniStrLen(name)+2;
  uniChar nbuff[nlen];

  tbuff[0] = '\'';
  nbuff[0] = '\'';

  uniCpy(&tbuff[1],tlen-1,tgt);
  uniCpy(&nbuff[1],nlen-1,name);

  gcAddRoot(&ttmp);
  
  updateConsFn(hndl,khandle);

  ttmp = newUniSymbol(tbuff);	        /* we need this order for G/C */
  updateConsEl(hndl,TGT_OFFSET,ttmp); /* target identifier */
    
  ttmp = newUniSymbol(nbuff);
  updateConsEl(hndl,NAME_OFFSET,ttmp);   /* name identifier */
  
  ttmp=locateHandle(tgt,name,p);           // This must be last
  updateConsEl(hndl,PROCESS_OFFSET,ttmp);
  
  gcRemoveRoot(root);
  return hndl;
}


inline objPo handleTgt(objPo h1)
{
  assert(IsHandle(h1));

  if(h1==knullhandle)
    return kvoid;
  else
    return consEl(h1,TGT_OFFSET);
}

inline objPo handleName(objPo h1)
{
  assert(IsHandle(h1));

  if(h1==knullhandle)
    return kvoid;
  else
    return consEl(h1,NAME_OFFSET);
}

inline processpo handleProc(objPo h1)
{
  if(IsHandle(h1)){
    if(h1==knullhandle)
      return NULL;
    else
      return HdlVal(consEl(h1,PROCESS_OFFSET))->p;
  }
  else
    return NULL;
}

logical sameHandle(objPo h1,objPo h2)
{
  if(h1==h2)
    return True;
  else if(IsHandle(h1) && IsHandle(h2)){
    processpo H1 = handleProc(h1);
    processpo H2 = handleProc(h2);

    if(H1!=NULL || H2!=NULL)
      /* we only need to look at the hdl structure */
      return H1==H2;
    else if(h1!=knullhandle&&h2!=knullhandle)
      return equalcell(consEl(h1,0),consEl(h2,0))==Ok &&
             equalcell(consEl(h1,1),consEl(h2,1))==Ok;
    else
      return False;
  }
  else
    return False;
}

logical sameAgent(objPo h1,objPo h2)
{
  if(h1==h2)
    return True;
  else if(IsHandle(h1) && IsHandle(h2))
    return handleName(h1)==handleName(h2);
  else
    return False;
}

retCode displayHandle(ioPo out,void *H,int width,int prec,logical alt)
{
  objPo h = (objPo)H;

  if(h==knullhandle)
    return outMsg(out,"%U",SymText(knullhandle));
  else if(IsHandle(h))
    return outMsg(out,"%U:%U",SymText(handleTgt(h)),SymText(handleName(h)));
  else
    return outCell(out,h,width,prec,alt);
}

retCode displayHdl(ioPo out,handlePo h)
{
  return outMsg(out,"$%U$",&h->name[0]);
}


/*
 * Compute `canonical handle' -- may be invoked during type-caste
 */

retCode m_canonical(processpo p,objPo *args)
{
  uniChar pname[MAX_SYMB_LEN];
  objPo h = args[0];

  args[0] = parseHandle(NULL,StringText(h,pname,NumberOf(pname)));

  return Ok;
}

/* Support for process mgt and handles */

void bindHandle(objPo H,processpo p)
{
  if(IsHandle(H) && H!=knullhandle){
    handlePo h = HdlVal(consEl(H,PROCESS_OFFSET));

    h->p = p;
  }
}

inline logical IsHandle(objPo T)
{
  if(T==knullhandle)
    return True;
  else
    return isConstructor(T,khandle) && consArity(T)==HANDLE_ARITY;
}

/* Support for GC of handles */
/* copy-style GC support */
static hashPo nhandles;

void scanHandle(handlePo H)
{
  Install(H->name,H,nhandles); /* put it back in the table */
}

typedef struct {
  objPo base;
  objPo limit;
} scanInfo;

static retCode preScanH(void *n,void *r,void *c)
{
  handlePo h = (handlePo)r;
  scanInfo *info = (scanInfo*)c;

  if(h->p!=NULL)
    scanProcess(h->p);

  if(!((objPo)h>=info->base && (objPo)h<info->limit)){
    if(h->p!=NULL)
      scanCell((objPo)h);
  }
  else
    Install(h->name,h,nhandles);

  return Ok;
}

void scanHandles(objPo base,objPo limit)
{
  scanInfo info = {base,limit};

  nhandles = NewHash(64,NULL,(compFun)uniCmp,NULL);

  ProcessTable(preScanH,handles,&info);

  DelHash(handles);
  handles = nhandles;
}

/* compact-style GC support */
void installHandle(handlePo H)
{
  Install(H->name,H,handles); /* put it back in the table */
}

void removeHandle(handlePo H)
{
  Uninstall(H,handles);
}

static retCode markH(void *n,void *r,void *c)
{
  handlePo h = (handlePo)r;

  if(h->p!=NULL)
    markProcess(h->p);

  Uninstall(n,handles);
  return Ok;
}

void markHandles(void)
{
  ProcessTable(markH,handles,NULL);
}

void adjustHandles(void)
{
}

typedef struct {
  procProc proc;
  void *cl;
} procInfoRec;

retCode pProc(void *n,void *r,void *c)
{
  handlePo h = (handlePo)r;
  procInfoRec *info = (procInfoRec*)c;

  if(h->p!=NULL)
    info->proc(h->p,info->cl);

  return Ok;
}

void processProcesses(procProc proc,void *cl)
{
  if(handles!=NULL){
    procInfoRec info = {proc,cl};

    ProcessTable(pProc,handles,&info);
  }
}

static retCode dH(void *n,void *r,void *c)
{
  handlePo h = (handlePo)r;

  if(h->p==NULL)
    outMsg(logFile,"%U\n",h->name);
  else{
    outMsg(logFile,"%U->",h->name);
    displayProcess(h->p);
  }
  return Ok;
}

void dispHdls(void)
{
  ProcessTable(dH,handles,NULL);
  flushFile(logFile);
}

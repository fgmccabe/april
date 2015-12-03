/* 
  Code management such as storing and loading code modules
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

#include "april.h"
#include "dict.h"
#include "sign.h"		/* type signature handling */
#include "astring.h"
#include "symbols.h"		/* We need some standard symbols */
#include "process.h"

/* Call a program out of an any structure and a list of strings  */
/* Used to invoke the user's main program */
retCode m_call(processpo P,objPo *args)
{
  objPo pr = args[1];
  objPo ags = args[0];

  if(IsAny(pr)){
    objPo tps = AnySig(pr);

    pr = AnyData(pr);

    if(!isClosure(pr))
      return liberror("__call",2,"procedure expected",einval);
    else{
      objPo cp=codeOfClosure(pr);


      while(isBinCall(tps=deRefVar(tps),kallQ,NULL,&tps))
        ;
      
      if(!isConstructor(tps,kprocTp))
	return liberror("__call",2,"invalid type signature",einval);
      else if(!IsList(ags))
	return liberror("__call",2,"list of arguments expected",einval);
      else{
        unsigned WORD32 ar = consArity(tps);
        unsigned WORD16 i;
	void *root = gcAddRoot(&ags); /* let gc know about this root */
        objPo *ptr = P->sp;	/* point into the locals */
	static uniChar msg[MAX_SYMB_LEN];
	retCode ret = Ok;
	
	gcAddRoot(&tps);

        for(i=0;ret==Ok && isNonEmptyList(ags) && i<ar;i++){
	  if((ret=force(consEl(tps,i),ListHead(ags),P->sp = --ptr))!=Ok){
	    strMsg(msg,NumberOf(msg),"mismatch in arguments, expecting `%t'",tps);
	    return Uliberror("__call",2,msg,einval);
	  }
	  else
	    ags = ListTail(ags);
	}
	gcRemoveRoot(root);

	if(i<ar){
	  strMsg(msg,NumberOf(msg),"insufficient number of arguments, "
		   "expecting `%t'",tps);
	  return Uliberror("__call",2,msg,einval);
	}


	/* Build the frame record */
	*--P->sp = (objPo)(P->pc);
	*--P->sp = P->e;
	P->e = pr;
	P->pc = CodeCode(cp);

	*--P->sp=(objPo)P->fp; /* Store existing frame pointer */
	P->fp=P->sp;		/* Redefine frame pointer */

	return Ok;
      }
    }
  }
  else
    return liberror("__call",2,"invalid arguments",einval);
}


retCode m_voidfun(processpo p,objPo *args)
{
  p->errval=kfailed;
  return Error;
}

retCode m_fail(processpo p,objPo *args)
{
  p->errval=kfailed;
  return Error;
}

inline logical isClosure(objPo t)
{
  return isCons(t) && IsCode(consFn(t));
}

inline objPo codeOfClosure(objPo t)
{
  assert(isClosure(t));
  
  return consFn(t);
}

inline objPo *codeFreeVector(objPo p)
{
  assert(isClosure(p));
  
  return consData(p);
}

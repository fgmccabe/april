/*
  Test and skeleton of a dynamically loadable module
  (c) 1994-1999 Imperial College and F.G.McCabe

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
#include "astring.h"		/* String handling interface */
#include "sign.h"		/* signature handling interface */
#include "debug.h"
#include "dll.h"		/* dynamic loader interface */

static retCode d_listlen(processpo p,objPo *args);
static retCode d_front(processpo p,objPo *args);

/* This is used by the loader */
/* This type signature string defines the type:
   (%x-(%x[]=>number), %x-((%x[],number)=>%x[]))
*/
#define LIST_SIG "T\2:\0FL$\0N:\0FT\2L$\0NL$\0"

uniChar listSig[] = {'T',2,':',0,'F','L','$',0,'N',':',0,'F','T',2,'L','$',0,'N','L','$',0,0};

SignatureRec signature = {
  NULL,				/* Used by April */
  listSig,                      /* The type signature of the list module */
  NumberOf(listSig),
  2,                            /* Two functions in module */
  {
    { d_listlen,1},		/* the listlen function */
    { d_front,2}                /* the front function */
  }
};

retCode d_listlen(processpo p,objPo *args)
{
  objPo lst = args[0];
  WORD32 len = 0;

  while(isNonEmptyList(lst)){
    lst = ListTail(lst);
    len++;
  }
  args[0] = allocateInteger(len);
  return Ok;
}

retCode d_front(processpo p,objPo *args)
{
  objPo t1 = args[1];
  WORD32 len = IntVal(args[0]);
  WORD32 pos=0;
  objPo last = args[1] = emptyList;
  objPo elmnt = emptyList;
  void *root = gcAddRoot(&last);

  gcAddRoot(&t1);
  gcAddRoot(&elmnt);

  while(pos++<len && !isEmptyList(t1)){
    elmnt = ListHead(t1);
    elmnt = allocatePair(&elmnt,&emptyList);

    if(last==emptyList)
      last = args[1] = elmnt;
    else{
      updateListTail(last,elmnt);
      last = elmnt;
    }
    t1 = ListTail(t1);
  }

  gcRemoveRoot(root);
  return Ok;
}

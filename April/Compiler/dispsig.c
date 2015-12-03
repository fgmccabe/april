/* 
   Display type signatures
   (c) 1994-2000 Imperial College and F.G. McCabe

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

#include "config.h"
#include "compile.h"
#include "keywords.h"
#include "makesig.h"
#include "operators.h"
#include "ioP.h"		/* access to private IO functions */
#include <stdlib.h>

/* Display a type signature properly */
static uniChar *outSig(inPo f,uniChar *sig)
{
  if(sig!=NULL){
    switch(*sig++){
    case number_sig:
      outStr(f,"number");
      return sig;
    case char_sig:
      outStr(f,"char");
      return sig;
    case symbol_sig:
      outStr(f,"symbol");
      return sig;
    case string_sig:
      outStr(f,"string");
      return sig;
    case handle_sig: 
      outStr(f,"handle");
      return sig;
    case logical_sig: 
      outStr(f,"logical");
      return sig;
    case unknown_sig: 
      outStr(f,"any");
      return sig;
    case opaque_sig: 
      outMsg(f,"opaque");
      return sig;
    case var_sig:		/* type variable */
      outMsg(f,"%c%d",'%',*sig);
      return sig+1;

    case forall_sig:{		/* universally quantified variable */
      outMsg(f,"%%%d-",*sig);
      return outSig(f,sig+1);
    }

    case query_sig:{
      char name[MAXSTR], *n=name;
      char delim = *sig++;	/* pick up the delimiter character */

      /* look for the delimiter */
      while(*sig!=delim)
	*n++=*sig++;

      *n = '\0';

      sig = outSig(f,sig+1);
      outMsg(f,"?%s",name);
      return sig;
    }

    case list_sig:		/* List closure type signature*/
      sig = outSig(f,sig);
      outStr(f,"[]");
      return sig;
      
    case empty_sig:
      outStr(f,"()");
      return sig;

    case tuple_sig:{
      int count = *sig++;	/* number of elements in the tuple */

      outCh(f,'(');
      while(sig!=NULL && count-->0){
	sig = outSig(f,sig);
	if(count>0)
	  outCh(f,',');
      }
      outCh(f,')');
      return sig;
    }

    case usr_sig:{		/* <<< fix to cope with hash */
      char name[MAXSTR], *n=name;
      char delim = *sig++;	/* pick up the delimiter character */

      /* look for the delimiter */
      while(*sig!=delim)
	*n++=*sig++;

      *n = '\0';

      outStr(f,name);
      return sig+1;
    }

    case poly_sig:{		/* <<< fix to cope with hash */
      char name[MAXSTR], *n=name;
      char delim = *sig++;	/* pick up the delimiter character */

      /* look for the delimiter */
      while(*sig!=delim)
	*n++=*sig++;

      *n = '\0';

      outStr(f,name);
      return outSig(f,sig+1);
    }

    case funct_sig:		/* function type signature */
      sig = outSig(f,sig);
      outStr(f,"=>");
      return outSig(f,sig);
      
    case proc_sig:		/* procedure type signature */
      sig = outSig(f,sig);
      outStr(f,"{}");
      return sig;

    default:
      outStr(f,"invalid type signature");
      return NULL;		/* We have an invalid type signature */
    }
  }
  else
    return NULL;
}

static retCode outSignature(ioPo f,void *str,long width,long precision,logical alt)
{
  if(outSig(f,(uniChar*)str)!=NULL)
    return Ok;
  else
    return Error;
}

void initSigDisplay(void)
{
  _installMsgProc('$',outSignature);	/* extend outMsg to cope with type signatures */
}


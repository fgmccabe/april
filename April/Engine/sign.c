/*
  Type signature verification and matching
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
#include <assert.h>
#include <string.h>
#include "april.h"
#include "ioP.h"
#include "astring.h"
#include "sign.h"
#include "symbols.h"
#include "process.h"

uniChar *skipName(uniChar *sig,WORD32 *remaining)
{
  uniChar delim = *sig++;	/* pick up the delimiter character */

  (*remaining)--;

  /* look for the delimiter */
  while((*remaining)-->0 && *sig++!=delim)
    ;
      
  if(*remaining<0)
    return NULL;
  else
    return sig;
}

/* Move over a complete type signature */
uniChar *skipSig(uniChar *sig,WORD32 *remaining)
{
  if(sig!=NULL && *remaining>0){
    switch((aprilTypeSignature)*sig++){
    case number_sig:
    case symbol_sig:
    case string_sig:
    case handle_sig:
    case logical_sig:
    case unknown_sig:
    case opaque_sig:
    case empty_sig:
      (*remaining)--;		/* decrement remaining length count */
      return sig;		/* Just one character for these signatures */
    case var_sig:		/* type variable */
      (*remaining)-=2;		/* skip of the type variable number */
      return sig+1;

    case usr_sig:{		/* user-defined type */
      (*remaining)--;
      return skipName(sig,remaining);
    }

    case poly_sig:{		/* polymorphic user-defined type */
      (*remaining)--;
      sig = skipName(sig,remaining);
      return skipSig(sig,remaining);
    }

    case list_sig:		/* List closure type signature*/
      (*remaining)--;		/* decrement remaining count */
      return skipSig(sig,remaining);
      
    case tuple_sig:{
      int count = *sig++;	/* number of elements in the tuple */
      (*remaining)-=2;

      while(sig!=NULL && count-->0)
	sig = skipSig(sig,remaining);

      return sig;
    }

    case query_sig:{
      (*remaining)--;

      sig = skipName(sig,remaining);

      return skipSig(sig,remaining);
    }

    case funct_sig:		/* function type signature */
      (*remaining)--;
      return skipSig(skipSig(sig,remaining),remaining);

    case proc_sig:		/* procedure type signature */
      (*remaining)--;
      return skipSig(sig,remaining); /* skip over the argument types */

    case forall_sig:		/* Quantified variable */
      (*remaining)-=2;		/* decrement remaining count */
      return skipSig(sig+1,remaining);

    default:
      return NULL;		/* We have an invalid type signature */
    }
  }
  return NULL;
}


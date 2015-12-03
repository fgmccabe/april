/*
  Type signature generator for April compiler
  (c) 1994-1998 Imperial College and F.G.McCabe

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
#include <stdlib.h>		/* Standard functions */
#include <assert.h>		/* Debugging assertion support */
#include <string.h>		/* String handling functions */
#include "compile.h"		/* Compiler structures */
#include "cellP.h"
#include "types.h"
#include "dict.h"
#include "keywords.h"		/* Standard April keywords */
#include "opcodes.h"		/* April machine opcodes */
#include "makesig.h"		/* Compiler's signature access */
#include "signature.h"

#define MAXVAR 1024

static struct{
  cellpo var;			/* The type variable */
  int varNo;			/* Its numeric code number */
  logical free;			/* Is this a free variable? */
}vars[MAXVAR];			/* Table of type variables */

static char *genSymbol(char *sig,cellpo tgt)
{
  uniChar delim = *sig++;	/* pick up the delimiter character */
  uniChar name[1024];	/* Maximum length of a user field name */
  uniChar *n = name;

  /* look for the field name delimiter */
  while(*sig!=delim)
    *n++=*sig++;
  *n='\0';

  mkSymb(tgt,locateU(name));
  return sig+1;
}

static char *genType(char *sig,cellpo tgt)
{
  if(sig!=NULL){
    switch(*sig++){
    case number_sig:
      copyCell(tgt,numberTp);	/* reconstruct the number type */
      return sig;
    case symbol_sig:
      copyCell(tgt,symbolTp);	/* reconstruct the symbol type */
      return sig;
    case char_sig:
      copyCell(tgt,charTp);
      return sig;
    case string_sig:
      copyCell(tgt,stringTp);	/* reconstruct the string type */
      return sig;
    case logical_sig: 
      copyCell(tgt,logicalTp);	/* reconstruct the logical type */
      return sig;
    case handle_sig:
      copyCell(tgt,handleTp);
      return sig;
    case unknown_sig:
      copyCell(tgt,anyTp);		/* reconstruct the any type */
      return sig;
    case opaque_sig:
      copyCell(tgt,opaqueTp);	/* reconstruct the opaque type */
      return sig;
    case var_sig:{		/* type variable */
      int var = *sig++;		/* pick up the the actual type variable */

      if(vars[var].var==NULL)
	vars[var].var=NewTypeVar(tgt);
      else
	copyCell(tgt,vars[var].var);
      return sig;
    }
    
    case query_sig:{
      BuildBinCall(kquery,voidcell,voidcell,tgt);
      sig = genSymbol(sig,callArg(tgt,1));
      return genType(sig,callArg(tgt,0));
    }

    case forall_sig:{		/* We are going to be constructing V:T */
      int var = *sig++;		/* pick up the the actual type variable */

      vars[var].var=NewTypeVar(NULL);

      BuildForAll(vars[var].var,voidcell,tgt);

      return genType(sig,callArg(tgt,1));
    }

    case list_sig:{		/* List closure type signature*/
      cellpo elem;

      BuildListType(voidcell,tgt);
      isListType(tgt,&elem);
      return genType(sig,elem);
    }
    
    case empty_sig:
      mkTpl(tgt,allocTuple(0));
      return sig;

    case tuple_sig:{
      int count = *sig++;	/* number of elements in the tuple */
      int i;

      mkTpl(tgt,allocTuple(count));

      for(i=0;i<count && sig!=NULL;i++)
	sig = genType(sig,tplArg(tgt,i));
      return sig;
    }

    case funct_sig:{		/* function type signature */
      BuildFunType(voidcell,voidcell,tgt);
      
      assert(*sig==tuple_sig||*sig==empty_sig);

      sig = genType(sig,callArg(tgt,0));

      if(sig!=NULL)		/* should never happen */
	sig = genType(sig,callArg(tgt,1));

      return sig;
    }
      
    case proc_sig:{		/* procedure type signature */
      assert(*sig==tuple_sig||*sig==empty_sig);
      
      if(*sig==tuple_sig){
        int ar = sig[1];
        int i;
        
        sig+=2;

        BuildProcType(ar,tgt);

        for(i=0;i<ar && sig!=NULL;i++)
	  sig = genType(sig,consEl(tgt,i));
        return sig;
      }
      else if(*sig==empty_sig){
        BuildProcType(0,tgt);
        return sig+1;
      }
    }

    case usr_sig:		/* user type - non polymorphic */
      return genSymbol(sig,tgt);

    case poly_sig:{		/* user type - polymorphic type */
      cellpo form = allocSingle();
            
      sig = genSymbol(sig,form);
      
      if(*sig==tuple_sig){
        int ar = sig[1];
        int i;

        sig+=2;

        BuildStruct(form,ar,tgt);

        for(i=0;i<ar && sig!=NULL;i++)
	  sig = genType(sig,consEl(tgt,i));
        return sig;
      }
      else if(*sig==empty_sig){
        BuildStruct(form,0,tgt);
        return sig+1;
      }
    }

    default:
      syserr("invalid type signature");
      return NULL;		/* We have an invalid type signature */
    }
  }
  return NULL;
}

void regenType(char *sig,cellpo tgt,logical inH)
{
  int i;

  for(i=0;i<MAXVAR;i++)
    vars[i].var=NULL;

  genType(sig,tgt);		/* regenerate a type description */
}

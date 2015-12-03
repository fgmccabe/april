/*
  Install standard escapes into April system
  (c) 1993-1998 F.G.McCabe and Imperial College

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
#include <string.h>		/* String functions */
#include <limits.h>		/* Limits and stuff */
#include "april.h"
#include "astring.h"
#include "dict.h"
#include "sign.h"
#include "process.h"
#include "msg.h"		/* pick up the definition of MSG_OPTS */

#define MAXESC 256		/* size of the escape table */

typedef struct {
  funpo escape_code;		/* What is the escape function code? */
  int arity;			/* What is the arity of the escape? */
  entrytype mode;		/* What kind of object is this? */
  logical priv;			/* Execute in privilidged mode only? */
#ifdef EXECTRACE
  char *name;		        /* Name of this escape function */
#endif
} EscapeTemplate, *escpo;

static EscapeTemplate escFuns[MAXESC]; /* The escape tables themselves */

static void install_fescape(char *escape_fn,funpo escape_code,int code,
			   logical pr,char *spec);
static void install_pescape(char *escape_fn,funpo escape_code,int code,
			   logical pr,char *spec);

#define fescape(name,cname,code,pr,spec) {\
  extern retCode cname(processpo,objPo *); \
  install_fescape(name,cname,code,pr,spec);\
};

#define pescape(name,cname,code,pr,spec) {\
  extern retCode cname(processpo,objPo *); \
  install_pescape(name,cname,code,pr,spec);\
}

/* Set up the escape table */
void install_escapes(void)
{
  unsigned int i;

  for(i=0;i<NumberOf(escFuns);i++)
    escFuns[i].escape_code = NULL; /* Clear the table */

#include "escapes.h"
}

WORD32 sigAr(char *sig)
{
  switch(sig[0]){
  case funct_sig:
  case proc_sig:
    if(sig[1]==tuple_sig)
      return sig[2];
    else if(sig[1]==empty_sig)
      return 0;
    else
      return 1;
  case forall_sig:
    return sigAr(sig+2);
  default:
    return -1;
  }
}


/* Install a symbol into the procedure escapes table */
static void install_pescape(char *escape_fn,funpo escape_code,int code,
			   logical pr,char *spec)
{
  escpo e = &escFuns[code];

  if(escFuns[code].escape_code==NULL){ /* Verify escape unused */
    e->escape_code = escape_code;
    e->priv = pr;
    e->mode = procedure;
#ifdef EXECTRACE
    e->name = strdup(escape_fn);
#endif
    e->arity = sigAr(spec);
  }
}

/* Install a symbol into the function escapes table */
static void install_fescape(char *escape_fn,funpo escape_code,int code,
			   logical pr,char *spec)
{
  escpo e = &escFuns[code];

  if(escFuns[code].escape_code==NULL){ /* Verify escape unused */
    e->escape_code = escape_code;
    e->priv = pr;
    e->mode = function;
#ifdef EXECTRACE
    e->name = strdup(escape_fn);
#endif
    e->arity = sigAr(spec);
  }
}

funpo escapeCode(unsigned int code)
{
  return escFuns[code].escape_code;
}

#ifdef EXECTRACE
char *escapeName(int code)
{
  if(code<0 || code>=MAXESC)
    return "unknown";
  else
    return escFuns[code].name;
}

void showEscape(processpo P,WORD32 code,objPo *args)
{
  int i;
  escpo e = &escFuns[code];

  outMsg(logFile,"%#w: %s(",P->handle,escapeName(code));

  args += e->arity-1;

  for(i=0;i<e->arity;i++)
    outMsg(logFile,"%.3w%s",*args--,(i<e->arity-1?",":""));

  outMsg(logFile,")\n");
}
#endif

retCode privileged_escape(int escape,entrytype *mode,int *arity)
{
  if(escape<0 || escape>MAXESC)
    return Error;
  else{
    escpo e = &escFuns[escape];

    if(e->escape_code==NULL)
      return Error;		/* Un-implemented escape */

    if(arity!=NULL)
      *arity = e->arity;
    if(mode!=NULL)
      *mode = e->mode;
    if(e->priv)
      return Ok;
    else
      return Fail;
  }
}

/* garbage collection function to handle type structures of escape funs */
void scanEscapes(void)
{
}

void markEscapes(void)
{
}

void adjustEscapes(void)
{
}

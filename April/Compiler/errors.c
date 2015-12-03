/*
 * Error and display management for the April compiler
 * (c) 1994-1998 Imperial College and F.G.McCabe
 * All rights reserved
 */
#include "config.h"
#include <stdlib.h>		/* Standard functions */
#include <stdarg.h>
#include <string.h>
#include "compile.h"
#include "formioP.h"

/* 
 * Standard compiler error reporting functions 
 */

long comperrflag = 0;		/* we wont generate code if set to non-zero */
long compwarnflag = 0;		/* Number of warnings */
long synerrors = 0;
logical suppressWarnings=False;	/* true if we do not display warnings */

errProc error_handler = NULL;
void *errorClientData = NULL;

static void reportFilePos(ioPo outFile,int line);

void resetErrors(errProc handler,void *client)
{
  error_handler = handler;
  errorClientData = client;
}

void reportErr(int line,char *msg,...)
{
  va_list args;			/* access the generic arguments */
  va_start(args,msg);		/* start the variable argument sequence */

  reportFilePos(logFile,line);
  outStr(logFile,RED_ESC_ON "syntax error: " RED_ESC_OFF);

  __voutMsg(logFile,(unsigned char*)msg,args);

  va_end(args);

  outChar(logFile,'\n');
  flushFile(logFile);

  comperrflag++;
}


void reportWarning(int line,char *msg,...)
{
  va_list args;			/* access the generic arguments */
  va_start(args,msg);		/* start the variable argument sequence */

  if(!suppressWarnings){
    reportFilePos(logFile,line);
    outStr(logFile,RED_ESC_ON "warning: " RED_ESC_OFF);

    __voutMsg(logFile,(unsigned char*)msg,args);

    va_end(args);
    
    outChar(logFile,'\n');
    flushFile(logFile);

    compwarnflag++;
  }
}

void reportParseErr(int line,char *msg,...)
{
  va_list args;			/* access the generic arguments */
  va_start(args,msg);		/* start the variable argument sequence */

  reportFilePos(logFile,line);
  outMsg(logFile, RED_ESC_ON "parse error: " RED_ESC_OFF);

  __voutMsg(logFile,(unsigned char*)msg,args);

  va_end(args);

  outChar(logFile,'\n');

  synerrors++;
  comperrflag++;
}

static logical formatErrorsLikeGcc(void)
{
  /* True iff we should format errors like gcc. 
     At the moment, this is true if we are running
     under EMACS which expects gcc style errors */

  char *emacs = getenv("EMACS");
  return (emacs != NULL && strcmp(emacs, "t") == 0);
}

static uniChar *stripFilePrefix(uniChar *s) {
  /* Strip the file:// prefix */
  if(uniIsLitPrefix(s,"file://"))
    return s+strlen("file://");
  else
    return s;
}

static void reportFilePos(ioPo outFile,int line)
{
  cellpo file = sourceFile(line);
  long line_no = sourceLine(line);

  if(formatErrorsLikeGcc()) {
    if(file!=NULL)
      outMsg(outFile,"%U:%d: ",stripFilePrefix(strVal(file)),line_no);
    else
      outMsg(outFile,": ");
  }
  else {
  if(file!=NULL)
    outMsg(outFile,"%U:%d: ",strVal(file),line_no);
  else
    outMsg(outFile,":: ");
  }
}

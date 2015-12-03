/*
  Support the April machine's remote debugging interface
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
#include <stdlib.h>
#include <string.h>
#include "april.h"
#include "opcodes.h"
#include "dict.h"
#include "symbols.h"
#include "process.h"
#include "msg.h"
#include "astring.h"		/* String handling interface */
#include "debug.h"

WORD32 traceCount = 0;
static ioPo debugIn = NULL;
static ioPo debugOut = NULL;

extern logical interactive;
static retCode showAltValue(ioPo out,objPo x);
static retCode showValue(ioPo out,objPo x);

void initDebugger(uniChar *host,int port)
{
  if(debugIn!=NULL){
    closeFile(debugIn);
    closeFile(debugOut);
    debugIn = debugOut =NULL;
  }
  
  {
    retCode ret = connectRemote(host,port,utf8Encoding,True,
				&debugIn,&debugOut);
    
    if(ret!=Ok){
      logMsg(logFile,"Unable to connect to debugger %U/%d\n",host,port);
      debugIn = debugOut = NULL;
      interactive = SymbolDebug = False;
    }
    else{
      SymbolDebug = True;
      outMsg(debugOut,"<model language=\"april\" version=\"5.0\"/>\n");
      flushFile(debugOut);
    }
  }
}
      
static void dump_regs(insPo pc,objPo *fp,objPo env)
{
  outMsg(logFile,"Registers\nPC = 0x%x\nFP = 0x%x\n",pc,fp);
  outMsg(logFile,"E = %.5w\n",env);
  outMsg(logFile,"%d%% free space in heap\n",createRoom());
}

static void dump_var(objPo *fp,char *t,int off)
{
  outMsg(logFile,"%s[%d] = %#.10w\n",t,off,fp[off]);
}

#ifdef EXECTRACE

void debug_stop(WORD32 pcCount,processpo p,insPo pc,objPo *sp,objPo *fp,objPo env,
		objPo *Lits,objPo prefix)
{
  int ch;
  static uniChar line[256] = {'n',0};
  objPo *e = codeFreeVector(env);
  static processpo focus = NULL; /* non-null implies only interested in this */

  if(focus==NULL || focus==p){
    /* decrement process's click counter */
    taxiFare(current_process); 
 
    outMsg(logFile,"%#w %d ",prefix,pcCount);
    dissass(pc,CodeCode(codeOfClosure(env)),fp,e,Lits);
    if(!interactive || traceCount>0){
      if(traceCount==0)
	outMsg(logFile,"\n");
      else{
	traceCount--;
	if(traceCount>0)
	  outMsg(logFile,"\n");
	else if(!interactive)
	  debugging=False;
      }
      flushFile(logFile);
    }

    while(interactive && traceCount==0){
      uniChar *ln = line;
      outMsg(logFile," => ");
      flushFile(logFile);

      reset_stdin();

      if((ch=inCh(stdIn))!='\n' && ch!=uniEOF){
	do{
	  *ln++=ch;
	  ch = inCh(stdIn);
	} while(ch!='\n' && ch!=uniEOF);
	*ln++='\0';
      }

      setup_stdin();
    
      switch(line[0]){
      case ' ':
      case 'n':
      case '\n':
	break;
      case 'f':
	focus = p;
	uniLit(line,NumberOf(line),"n\n");
	break;
      case 'u':
	focus = NULL;
	uniLit(line,NumberOf(line),"n\n");
	break;
      case 'q':
	syserr("terminating april session");
      case 't':
	interactive = False;
	break;
      case 'S':
	SymbolDebug = True;
	TCPDebug = False;
	debugging = False;
	interactive = True;
	uniLit(line,NumberOf(line),"n\n");
	break;
      case uniEOF:
      case 'c':
	debugging=False;
	break;
      case 'r':			/* dump the registers */
	dump_regs(pc,fp,env);
	uniLit(line,NumberOf(line),"n\n");
	continue;
      case 'l':{		/* dump a local variable */
	integer off=parseInteger(line+1,uniStrLen(line+1));

	dump_var(fp,"fp",off);
	uniLit(line,NumberOf(line),"n\n");
	continue;
      }
      case 'e':{		/* dump an environment variable */
	integer off=parseInteger(line+1,uniStrLen(line+1));

	dump_var(e,"e",off);
	uniLit(line,NumberOf(line),"n\n");
	continue;
      }
      case 'm':{		/* dump outstanding messages */
	msgpo m=current_process->mfront;
	if(m!=NULL)
	  while(m){
	    outMsg(logFile,"Message %d from %w `%.10w'\n",m->sequence,
		   m->sender,m->msg);
	    m=m->next;
	  }
	else
	  outMsg(logFile,"No messages\n");
	uniLit(line,NumberOf(line),"n\n");
	continue;
      }
      case 'p':{		/* Display run Q */
	displayRunQ();
	uniLit(line,NumberOf(line),"n\n");
	continue;
      }

      case 'P':{		/* Display all processes */
	displayProcesses();
	uniLit(line,NumberOf(line),"n\n");
	continue;
      }

      case 's':			/* Show a stack trace of this process */
	p->sp = sp;
	p->fp = fp;
	p->e = env;		/* save registers ... */
	p->pc = pc;

	stackTrace(p);
	uniLit(line,NumberOf(line),"n\n");
	continue;

      case '0': case '1': case '2': case '3': case '4': case '5': 
      case '6': case '7': case '8': case '9': {
	traceCount = parseInteger(line,uniStrLen(line));
	continue;
      }
      
      case 'i':{
	integer off=parseInteger(line+1,uniStrLen(line+1));
	integer i;
	insPo p = pc;
	insPo base = CodeCode(codeOfClosure(env));
	
	for(i=0;i<off;i++){
	  p = dissass(p,base,fp,e,Lits);
	  outChar(logFile,'\n');
	}
	uniLit(line,NumberOf(line),"n\n");
	continue;
      }
        
      default:
	outMsg(logFile,"'n' = step, 'c' = continue, 't' = trace mode, 'q' = stop\n");
	outMsg(logFile,"'<n>' = step <n>\n");
	outMsg(logFile,"'S' = symbolic mode\n");
	outMsg(logFile,"'r' = registers, 'l <n>' = local, 'e <n>' = env var\n");
	outMsg(logFile,"'i'<n> = list n instructions, 's' = stack trace\n");
	outMsg(logFile,"'m' = messages, 'p' = active processes, 'P' = all processes\n");
	outMsg(logFile,"'f' = focus on this process, 'u' = unfocus \n");
	outMsg(logFile,"'D <n>' = set trace depth\n");
	continue;
      }
      taxiFlag();		/* restart the clock */
      return;
    }

    taxiFlag();		/* restart the clock */
  }
}

#endif

/* display a debug message to the debugger */
retCode m_debug(processpo p,objPo *args)
{
  objPo msg = args[0];
  objPo h = p->handle;

  if(SymbolDebug){
    if(TCPDebug){
      if(debugOut!=NULL){
        outMsg(debugOut,"<debugmsg tId=\"");
        showAltValue(debugOut,p->handle);
        outMsg(debugOut,"\">");
        showValue(debugOut,args[0]);
        outMsg(debugOut,"</debugmsg>\n");
        flushFile(debugOut);
      }
      return Ok;
    }
    else if(IsTuple(msg)){
      WORD32 ar = tupleArity(msg);
      int i;
      objPo *el = tupleData(msg);

      outMsg(logFile,"[%#w] %.10w",h,*el);

      for(i=1;i<ar;i++)
	outMsg(logFile," %.5w",el[i]);
      outMsg(logFile,"\n");
    }
    else
      outMsg(logFile,"[%#w] %#10w\n",h,msg);
  }
  return Ok;
}

/* wait for a reply from the debugger */
retCode m_debug_wait(processpo p,objPo *args)
{
  if(SymbolDebug){
    if(TCPDebug){
      if(debugIn!=NULL && isInReady(debugIn)==Ok){
        uniChar reply[256];
        uniChar term[]={'\n',0};
        long rLen;
	retCode ret;

      again:
	ret = inLine(debugIn,reply,NumberOf(reply),&rLen,term);
      
        if(ret==Ok){
          if(uniIsLit(reply,"")||uniIsLit(reply,"<step/>"))
            return Ok;
          else if(uniIsLit(reply,"<fail/>"))
            return Fail;
          else if(uniIsLit(reply,"<abort/>"))
	    return liberror("_debug_wait",0,"termination by debugger",eabort);
          else if(uniIsLit(reply,"<quit/>")){
            syserr("terminating april session");
            return Error;
          }
          else
	    return liberror("_debug_wait",0,"invalid message from debugger",einval);
        }
	else if(ret==Interrupt)
	  goto again;
        else
          return ret;
      }
      else
        return Ok;            // Ignore the remote debugger
    }
    else if(interactive)
      return wait_for_user(p,p->pc,p->fp,p->e);
  }
  return Ok;			/* otherwise we are Ok */
}

retCode m_break(processpo p,objPo *args)
{
  if(!SymbolDebug || !TCPDebug){
    flushOut();
    outMsg(logFile,"%#w --  user break %w\n",p->handle,args[0]);

#ifdef EXECTRACE
    if(!SymbolDebug)
      interactive=debugging=True;
#else
    SymbolDebug = True;
    interactive = !TCPDebug;
#endif
  }
  else if(debugOut!=NULL){
    outMsg(debugOut,"<break tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\">");
    showValue(debugOut,args[0]);
    outMsg(debugOut,"</break>\n");
    flushFile(debugOut);
  }
  return Ok;
}

retCode wait_for_user(processpo p,insPo pc,objPo *fp,objPo env)
{
  int ch;
  static char line[256] = "n";
  objPo *e = codeFreeVector(env);
  static processpo focus = NULL; /* non-null implies only interested in this */

  if(focus==NULL || focus==p){
    /* decrement the process's click counter */
    taxiFare(p); 

    flushOut();

    if(traceCount>0){
      if(--traceCount>0){
	outMsg(logFile,"\n");
	return Ok;
      }
    }

    while(traceCount==0 && interactive){
      outMsg(logFile,"[%#w] > ",p->handle);
      flushFile(logFile);

      if((ch=inCh(stdIn))!='\n' && ch!=uniEOF){
	char *ptr = line;
	do{
	  *ptr++=ch;
	  ch = inCh(stdIn);
	} while(ch!='\n' && ch!=uniEOF);
	*ptr++='\0';
      }
      
      switch(line[0]){
      case ' ':
      case 'n':
      case '\n':
	break;
      case 'q':
	syserr("terminating april session");
      case 'c':
	SymbolDebug=False;
	strcpy(line,"n\n");
	break;
      case 'i':			/* Instruction level debugging */
#ifdef EXECTRACE
	debugging=interactive=True;
	strcpy(line,"n\n");
	break;			/* go into instruction level */
#else
	outMsg(logFile,"Instruction level debugging not supported\n");
	strcpy(line,"n\n");
	continue;
#endif
      case 't':
	interactive=False;
	strcpy(line,"n\n");
	break;
      case 'r':			/* dump the registers */
	dump_regs(pc,fp,env);
	continue;
      case 'l':{		/* dump a local variable */
	int off=atoi(line+1);
	
	dump_var(fp,"FP",off);

	continue;
      }
      case 'e':{		/* dump an environment variable */
	int off=atoi(line+1);
      
	dump_var(e,"E",off);

	continue;
      }
      case 'm':{		/* display the messages for this process */
	msgpo m=p->mfront;
	if(m!=NULL)
	  while(m){
	    outMsg(logFile,"Message %d from [%#w] `%.10w'\n",
		   m->sequence,m->sender,m->msg);
	    m=m->next;
	  }
	else
	  outMsg(logFile,"No messages\n");

	continue;
      }

      case 'p':{		/* Display run Q */
	displayRunQ();
	continue;
      }

      case 'P':{		/* Display all processes */
	displayProcesses();
	continue;
      }

      case 's':			/* Show a stack traqce of this process */
	stackTrace(p);
	continue;

      case 'f':
	focus = p;
	strcpy(line,"n\n");
	break;
      case 'u':
	focus = NULL;
	strcpy(line,"n\n");
	break;

      case '0': case '1': case '2': case '3': case '4': case '5': 
      case '6': case '7': case '8': case '9': {
	traceCount = atoi(line);
	continue;
      }

      default:
#ifdef EXECTRACE
	outMsg(logFile,"'i' = instruction-level\n");
#endif
	outMsg(logFile,"'t' = trace mode\n");
	outMsg(logFile,"'r' = dump registers, 'm' = dump messages\n");
	outMsg(logFile,"'l off' = local var, 'e off' = env var\n");
	outMsg(logFile,"' ' = step, 'c' = continue, 'q' = stop\n");
	strcpy(line,"n\n");

	continue;
      }
      taxiFlag();		/* restart the clock */
      return Ok;
    }
  }
  return Ok;
}

static retCode wStringChr(ioPo f,uniChar ch)
{
  switch(ch){
  case '&':
    return outStr(f,"&amp;");
  case '<':
    return outStr(f,"&lt;");
  case '>':
    return outStr(f,"&gt;");
  case '\"':
    return outStr(f,"&quot;");
  case '\'':
    return outStr(f,"&apos;");
  case '\n': 
  case '\r': 
  case '\t': 
    return outChar(f,ch);
  default:
    if(ch<' '||!isChar(ch))
      return outMsg(f,"&#%x;",ch);
    else
      return outChar(f,ch);
  }
}

retCode xmlQuoteString(ioPo f,uniChar *s,WORD32 len)
{
  WORD32 i;
  retCode ret = Ok;
  
  for(i=0;ret==Ok && i<len;i++)
   ret = wStringChr(f,*s++);
   
  return ret;
}

static retCode showAltValue(ioPo out,objPo x)
{
  ioPo str = openOutStr(utf8Encoding);
  retCode ret = outMsg(str,"%#5w",x);
  
  if(ret==Ok){
    long len;
    uniChar *text = getStrText(O_STRING(str),&len);
    
    ret = xmlQuoteString(out,text,len);
    
    closeFile(str);
  }
  return ret;
}
  
static retCode showValue(ioPo out,objPo x)
{
  ioPo str = openOutStr(utf8Encoding);
  retCode ret = outMsg(str,"%5w",x);
  
  if(ret==Ok){
    long len;
    uniChar *text = getStrText(O_STRING(str),&len);
    
    ret = xmlQuoteString(out,text,len);
    
    closeFile(str);
  }
  return ret;
}
  

/* Called when a #line instruction is executed */
/* To record the fact that the program is at a particular place */
retCode line_debug(processpo p,objPo file, int lineno)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<line tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\" line=\"%d\" url=\"%X\"/>\n",lineno,SymText(file));
    flushFile(debugOut);
  }
  return Ok;
}

retCode entry_debug(processpo p,objPo proc)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<call tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\" about=\"%X\">%X()</call>\n",SymText(proc),SymText(proc));
    flushFile(debugOut);
  }
  return Ok;
}

retCode exit_debug(processpo p,objPo proc)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<exit tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\" about=\"%X\">%X()</exit>\n",SymText(proc),SymText(proc));
    flushFile(debugOut);
  }
  return Ok;
}

/* record that a variable has been assigned */
retCode assign_debug(processpo p,objPo var,objPo val)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<asgn tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\" var=\"%X\">",SymText(var));
    showValue(debugOut,val);
    outMsg(debugOut,"</asgn>\n");
    flushFile(debugOut);
  }
  return Ok;
}

/* record that a function is returning a value */
retCode return_debug(processpo p,objPo fun,objPo val)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<return tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\" about=\"%X\">",SymText(fun));
    showValue(debugOut,val);
    outMsg(debugOut,"</return>\n");
    flushFile(debugOut);
  }
  return Ok;
}

/* record that a message has been sent */
retCode send_debug(processpo p,objPo m,objPo from,objPo to)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<send tId=\"");
    showAltValue(debugOut,from);
    outMsg(debugOut,"\" rId=\"");
    showAltValue(debugOut,to);
    outMsg(debugOut,"\">");
    showValue(debugOut,m);
    outMsg(debugOut,"</send>\n");
  }
  return Ok;
}

/* record that a message has been received */
retCode accept_debug(processpo p,objPo m,objPo from)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<accept tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\" rId=\"");
    showAltValue(debugOut,from);
    outMsg(debugOut,"\">");
    showValue(debugOut,m);
    outMsg(debugOut,"</accept>\n");
  }
  return Ok;
}

/* record that a process has been forked */
retCode fork_debug(processpo p,objPo child)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<fork tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\" child=\"");
    showAltValue(debugOut,child);
    outMsg(debugOut,"/>\n");
    flushFile(debugOut);
  }
  return Ok;
}

/* record that a process has died */
retCode die_debug(processpo p)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<die tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\"/>\n");
    flushFile(debugOut);
  }
  return Ok;
}

/* record that a variable scope has been entered */
retCode scope_debug(processpo p,int level)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<scope tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\" level=\"%d\"/>\n",level);
    flushFile(debugOut);
  }
  return Ok;
}

retCode suspend_debug(processpo p)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<suspend tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\"/>\n");
    flushFile(debugOut);
  }
  return Ok;
}

/* record that a run-time error has been raised */
retCode error_debug(processpo p,objPo errmsg)
{
  if(debugOut!=NULL){
    outMsg(debugOut,"<exception tId=\"");
    showAltValue(debugOut,p->handle);
    outMsg(debugOut,"\">");
    showValue(debugOut,errmsg);
    outMsg(debugOut,"</exception>\n");
    flushFile(debugOut);
  }
  return Ok;
}


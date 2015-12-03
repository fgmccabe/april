/* 
   Main program for April compiler -- version 4.4
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
#include "config.h"		/* Pick up our configuration parameters */
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "compile.h"		/* access compiler functions */
#include "types.h"		/* Type cheking interface */
#include "tree.h"		/* Tree parsing and dumping */
#include "dict.h"
#include "keywords.h"
#include "operators.h"		/* operator definition etc. */
#include "macro.h"		/* access macro functions */
#include "unix.h"		/* Unix specific stuff */
#include "ooio.h"                       /* pick up the io system */

extern logical traceMacro;	/* tracing of macro processor */
extern logical traceCompile;	/* tracing preprocessor */
extern logical traceType;	/* tracing of the type checker */
extern logical traceMemory;	/* Tracing of GC */
extern logical traceParse;      // Tracing the parse process

static logical quiet = False;	/* true if not to display version info */

#include "version.h"		/* The identification of this version */
char copyRight[]="(c) 1994-1998 Imperial College and F.G.McCabe\n"
                 "All rights reserved";

static void markExecutable(ioPo outFile);
static uniChar *currentWD(void);
static uniChar *defaultHeader(void);
static uniChar *defaultIncludePath(uniChar *);
static uniChar *defaultSysIncludePath(char *include);

#define INCLUDE DIR_SEP "include" DIR_SEP

static int errflag = 0;
int maxerrors = MAXERRORS;	/* how many errors will we accept? */

int getOptions(int argc, char **argv,OptInfoRec *info)
{
  int opt;
  extern char *optarg;
  extern int optind;

  while((opt=getopt(argc,argv,"vETqd:gxo:s:P:WI:i:#:VM"))>=0){
    switch(opt){
    case 'd':{			/* turn on various debugging options */
      char *c = optarg;

      while(*c)
	switch(*c++){
	case 't':
	  traceType = True;	/* enable tracing of the type checker */
	  break;
	case 'c':
	  traceCompile = True;
	  break;
	case 'm':
	  traceMacro = True;
	  break;
	case 'M':
	  traceMemory = True;
	  break;
	case 'p':
	  traceParse = True;
	  break;
	case '*':
	  traceType = True;	/* enable tracing of the type checker */
	  traceCompile = True;
	  traceMacro = True;
	  traceMemory = True;
	  traceParse = True;
	  break;
	  
	default:
	  errflag++;		/* illegal compiler option */
	}
      continue;
    }

    case 'E':
      info->preProcOnly = True;	/* We are not generating code here ... */
      continue;

    case 'g':
      info->dLvl = 1;		/* Generate symbolic debugging code */
      continue;

    case 'I':{			/* change the system include path */
      long len = strlen(optarg)+1;
      uniChar buff[len];
      
      _uni((unsigned char*)optarg,buff,len);
      
      info->sysinclude = uniDuplicate(buff);
      continue;
    }

    case 'i':{			/* change the user include path */
      long len = strlen(optarg)+1;
      uniChar buff[len];
      
      _uni((unsigned char*)optarg,buff,len);
      
      info->include = uniDuplicate(buff);
      continue;
    }

    case 'o':{
      long len = strlen(optarg)+1;
      uniChar buff[len];
      
      _uni((unsigned char*)optarg,buff,len);
      
      info->out_name = uniDuplicate(buff);
      continue;
    }

    case 'W':
      info->noWarnings=True;	/* do not display warnings */
      outMsg(logFile,"warnings suppressed\n");
      continue;

    case 'x':			/* Make the output an executable file */
      info->executable = True;
      continue;

    case '#':{			/* Override standard macro join string */
      long len = strlen(optarg)+1;
      uniChar buff[len];
      
      _uni((unsigned char*)optarg,buff,len);
      
      info->macJoin = uniDuplicate(buff);
      continue;
    }

    case 'q':
      quiet = True;
      continue;

    case 'v':
      if(logFile==NULL){
        uniChar fn[]={'-',0};
        initLogfile(fn);		/* direct log messages to stderr */
      }

      outMsg(logFile,"%s\n",version);
      flushFile(logFile);
      quiet = True;
      continue;

    case 'V':			/* switch on verbose mode */
      info->verbose = True;
      continue;

    case 'M':			/* generate a module header */
      info->makeAf = True;
      continue;

    default:
      return -1;
    }
  }
  return optind;
}

/*
 * Main program of compiler 
*/
int main(int argc, char **argv)
{
  int farg;
  OptInfoRec info;
  int exit_mode = 0;
  
  if(logFile==NULL){
      uniChar fn[]={'-',0};
      initLogfile(fn);		/* direct log messages to stderr */
  }

  initCompiler();
				/* Set up for processing options */
  info.header =defaultHeader();
  info.sysinclude = defaultSysIncludePath("file:///" APRILDIR);
  info.include = defaultIncludePath(currentWD());
  info.out_name = NULL;
  info.macJoin = khash;		/* default macro generation prefix */
  info.dLvl = 0;		/* symbolic debugging? */
  info.noWarnings = False;	/* true if we do not display warnings */
  info.preProcOnly = False;	/* True if only displaying result of macro output */
  info.executable = False;	/* True if output is marked as executable */
  info.verbose = False;		/* True for verbose output from compiler */
  info.makeAf = False;		/* assume we are not making an .af file */

  if((farg=getOptions(argc,argv,&info))==-1){
    if(logFile==NULL){
      uniChar fn[]={'-',0};
      initLogfile(fn);		/* direct log messages to stderr */
    }
      
    outMsg(logFile,"usage: %s [-v] [-d [ctmME]] [-g] "
	    "[-s serverName] [-P port] [-E] "
	    "[-x] [-X header] [-I include] [-q] [-# join] [-V] [-M] "
	    "file...\n",argv[0]);
    comp_exit(1);
  }

  if(logFile==NULL){
      uniChar fn[]={'-',0};
      initLogfile(fn);		/* direct log messages to stderr */
  }

  if(!quiet && !info.preProcOnly){	/* Display banner */
    outMsg(logFile,"%s\n",version);
    flushFile(logFile);
  }

  if(exit_mode==0 && farg<argc){		/* Set up the file to read */
    uniChar inName[MAXSTR];
    uniChar outName[MAXSTR];
    ioPo inFile=NULL;
    ioPo outFile;

    _uni((unsigned char*)argv[farg],inName,NumberOf(inName));
      
    if((inFile=findURL(info.sysinclude,info.include,inName,unknownEncoding))==NULL){
      outMsg(logFile,"file not found: %s\n",argv[farg]);
      comp_exit(1);
    }

    info.include = fileName(inFile);	/* Set the `real' include path */

    if(info.out_name==NULL)
      fileNameSuffix(inName,".aam",outName,NumberOf(outName));
    else
      uniCpy(outName,NumberOf(outName),info.out_name);

    if(info.preProcOnly)
      outFile = logFile;
    else if((outFile=newOutFile(outName,rawEncoding)) == NULL){
      outMsg(logFile,"failed to create output file [%U]\n",outName);
      comp_exit(1);
    }

    if(info.executable)
      markExecutable(outFile);

    compile(inFile,outFile,&info);

    closeFile(outFile);
    closeFile(inFile);

    if(comperrflag){
      outMsg(logFile,"%d errors, %d warnings found\n",comperrflag,compwarnflag);
      if(!uniIsLit(outName,"")){
        long len = uniStrLen(outName);
        char name[len*4];
        
        _utf(outName,(unsigned char*)name,len*4);
	if(unlink(name)!=-1)
	  outMsg(logFile,"*** %s removed\n",name);
      }
      exit_mode = 1;		/* We are going to report a failure */
    }
    farg++;			/* Look at the next file argument */
  }

  comp_exit(exit_mode);
  return 0;
}

void comp_exit(int code)
{
  exit(code);
}

/* Fatal system error */
void syserr(char *msg)
{
  outMsg(logFile,"Run-time error: %s\n", msg);
  comp_exit(1);
}

static void markExecutable(ioPo outFile)
{
#if !defined(_WIN32) && !defined(__MWERKS__)
  char *preamble = getenv("APRIL_PREAMBLE");

  if(preamble==NULL){
    char *april_dir = getenv("APRIL_DIR");

    if(april_dir==NULL)
      april_dir = APRILDIR;	/* default installation point */
      
    if(strncmp(april_dir,"file:///",NumberOf("file:///"))==0)
      april_dir+=NumberOf("file:///");

    outMsg(outFile,"#!%s/bin/april\n",april_dir);
  }
  else
    outMsg(outFile,"%s\n",preamble);

  fchmod(fileNumber(O_FILE(outFile)),
	 S_IXUSR|S_IXGRP|S_IXOTH|S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR);
#endif
}

static uniChar *currentWD(void)
{
  char cbuff[MAXPATHLEN];
  char *cwd = getcwd(cbuff,NumberOf(cbuff)); /* compute current starting directory */

  if(cwd==NULL){
    syserr("cant determine current directory");
    return NULL;
  }
  else{
    long len = strlen(cwd)+10;
    uniChar buff[len];
    
    strMsg(buff,len,"file:///%s/",cwd);
    return uniDuplicate(buff);
  }
}

static uniChar *defaultIncludePath(uniChar *deflt)
{
  char *path = getenv("APRIL_INCLUDE_PATH");
  if(path==NULL)
    return uniDuplicate(deflt);
  else{
    long len = strlen(path)+1;
    uniChar uPath[len];
    
    strMsg(uPath,len,"%s",path);
    return uniDuplicate(uPath);
  }
}

static uniChar *defaultSysIncludePath(char *deflt)
{
  char *path = getenv("APRIL_DIR");
  if(path==NULL){
    long len = strlen(deflt)+strlen("/include/")+1;
    uniChar uPath[len];
    
    strMsg(uPath,len,"%s/include/",deflt);
    return uniDuplicate(uPath);
  }
  else{
    long len = strlen(path)+strlen("/include/")+1;
    uniChar uPath[len];
    
    strMsg(uPath,len,"%s/include/",path);
    return uniDuplicate(uPath);
  }
}

static uniChar *defaultHeader(void)
{
  char *april_dir = getenv("APRIL_DIR");

  if(april_dir==NULL)
    april_dir = "file:///" APRILDIR;
    
  {
    uniChar buff[MAXSTR];
    
    strMsg(buff,NumberOf(buff),"%s/include/stdlib.ah",april_dir);
    
    return uniDuplicate(buff);
  }
}



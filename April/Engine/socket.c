/*
  Socket interface library for April
  (c) 1994-1997 Imperial College and F.G. McCabe

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
#include <string.h>
#include <assert.h>

#include "april.h"
#include "process.h"
#include "dict.h"
#include "symbols.h"
#include "astring.h"		/* String interface functions */
#include "fileio.h"		/* Interface to file functions */

retCode m_listen(processpo p,objPo *args)
{
  if(!IsInteger(args[0]))
    return liberror("_listen",1,"argument should be an integer",einval);
  else if(!p->priveleged)
    return liberror("_listen", 1, "permission denied",eprivileged);
  else{
    uniChar name[256];
    sockPo listen;

    strMsg(name,NumberOf(name),"listen@%ld",IntVal(args[0]));
    listen = listeningPort(name,IntVal(args[0]));

    if(listen==NULL)
      return liberror("_listen",1,"permission denied",eprivileged);
    else{
      if(configureIo(O_FILE(listen),turnOffBlocking)==Error ||
	 configureIo(O_FILE(listen),enableAsynch)==Error)
	logMsg(logFile,"failed to configure listener for april");

      args[0] = allocOpaqueFilePtr(O_IO(listen));
      return Ok;
    }
  }
}

/* accept allows a connection from a connect socket and returns the
   socket number of the connection and information about the connecting
   host */
retCode m_accept(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__accept",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__accept",1,"Invalid argument",einval);
  else{
    ioPo listen = opaqueFilePtr(t1);
    ioPo inC,outC;
    retCode res = acceptConnection(O_SOCK(listen),unknownEncoding,&inC,&outC);

    switch(res){
    case Ok:{
      objPo io = allocOpaqueFilePtr(inC);
      void *root = gcAddRoot(&io);
      args[0] = allocateTuple(2);
      
      updateTuple(args[0],0,io);

      io = allocOpaqueFilePtr(outC);
      updateTuple(args[0],1,io);
      gcRemoveRoot(root);
      
      if(configureIo(O_FILE(inC),turnOffBlocking)==Error ||
	 configureIo(O_FILE(inC),enableAsynch)==Error||
	 configureIo(O_FILE(outC),turnOffBlocking)==Error ||
	 configureIo(O_FILE(outC),enableAsynch)==Error)
	logMsg(logFile,"failed to properly configure new connection: %U",ioMessage(outC));

      detachProcessFromFile(listen,p);
      return Ok;
    }
    case Interrupt:
    case Fail:
      return attachProcessToFile(listen,p,input);
    case Error:
      return liberror("_accept",1,"failed to acquire new connection",enet);
    default:
      return liberror("_accept",1,"problem",enet);
    }
  }
}


/* return the peername of a connection */
retCode m_peername(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__peername",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__peername",1,"Invalid argument",einval);
  else{
    ioPo stream = opaqueFilePtr(t1);
    int port;
    uniChar *name = peerName(O_SOCK(stream),&port);

    if(name!=NULL){
      objPo val = allocateString(name);
      void *root = gcAddRoot(&val);

      args[0] = allocateTuple(2);

      updateTuple(args[0],0,val);
      val = allocateInteger(port);
      updateTuple(args[0],1,val);

      gcRemoveRoot(root);
      return Ok;
    }
    else
      return liberror("__peername",1,"cant determine connecting host",enet);
  }
}

/* return the peer's IP name of a connection */
retCode m_peerip(processpo p,objPo *args)
{
  objPo t1 = args[0];

  if(!p->priveleged)
    return liberror("__peerip",1,"permission denied",eprivileged);
  else if(!IsOpaque(t1) || OpaqueType(t1)!=_F_OPAQUE_)
    return liberror("__peerip",1,"Invalid argument",einval);
  else{
    ioPo stream = opaqueFilePtr(t1);
    int port;
    uniChar name[MAX_SYMB_LEN];
    uniChar *IP = peerIP(O_SOCK(stream),&port,name,NumberOf(name));

    if(IP!=NULL){
      objPo val = allocateString(IP);
      void *root = gcAddRoot(&val);

      args[0] = allocateTuple(2);

      updateTuple(args[0],0,val);
      val = allocateInteger(port);
      updateTuple(args[0],1,val);

      gcRemoveRoot(root);
      return Ok;
    }
    else
      return liberror("__peerip",1,"cant determine connecting host",enet);
  }
}

/* Attempt a connection with a server 
   specified as a pair: hostname(or ip address)/port
*/
retCode m_connect(processpo p,objPo *args)
{
  objPo t1 = args[1];
  objPo t2 = args[0];

  if(!isListOfChars(t1))
    return liberror("_connect",2,"1st argument should be a string",einval);
  else if(!IsInteger(t2))
    return liberror("_connect",2,"2nd argument should be an integer",einval);
  else if(!p->priveleged)
    return liberror("_connect",2,"permission denied",eprivileged);
  else{
    int port = IntVal(t2);
    WORD32 hlen = ListLen(t1);
    uniChar hBuf[hlen+1];
    uniChar *host = StringText(t1,hBuf,hlen+1);
    retCode ret = Ok;
    ioPo remIn = O_IO(ps_client(p));
    ioPo remOut;

    if(remIn==NULL)
      ret = connectRemote(host,port,unknownEncoding,False,&remIn,&remOut);
    else
      ret = resumeRemoteConnect(O_SOCK(remIn),host,&remOut);

    switch(ret){
    case Ok:
      if(configureIo(O_FILE(remIn),turnOffBlocking)==Error ||
	 configureIo(O_FILE(remIn),enableAsynch)==Error ||
	 configureIo(O_FILE(remOut),turnOffBlocking)==Error ||
	 configureIo(O_FILE(remOut),enableAsynch)==Error)
	logMsg(logFile,"failed to properly configure new connection");


      objPo io = allocOpaqueFilePtr(remIn);
      void *root = gcAddRoot(&io);
      args[1] = allocateTuple(2);
      
      updateTuple(args[1],0,io);

      io = allocOpaqueFilePtr(remOut);
      updateTuple(args[1],1,io);
      gcRemoveRoot(root);

      return Ok;

    case Interrupt:
    case Fail:
      ps_set_client(p,O_IO(remIn));
      return attachProcessToFile(O_IO(remIn),p,output);

    case Error:
      return Uliberror("_connect",2,ioMessage(O_IO(remIn)),efail);
    default:
      return liberror("_connect",2,"problem",efail);
    }
  }
}

/* Access host name functions */
/* return IP addresses of a host */
retCode m_hosttoip(processpo p,objPo *args)
{
  if(!isListOfChars(args[0]))
    return liberror("hosttoip",1,"argument should be a string",einval);
  else{
    WORD32 i=0;
    uniChar ip[MAX_SYMB_LEN];
    WORD32 hlen = ListLen(args[0]);
    uniChar hBuf[hlen+1];
    uniChar *host = StringText(args[0],hBuf,hlen+1);
    objPo last = emptyList;
    objPo ipstr = emptyList;
    void *root = gcAddRoot(&last);

    gcAddRoot(&ipstr);

    while(getNthHostIP(host,i,ip,NumberOf(ip))!=NULL){
      ipstr = allocateString(ip);

      if(last==emptyList)
	last = args[0] = allocatePair(&ipstr,&emptyList);
      else{
	objPo tail = allocatePair(&ipstr,&emptyList);
	updateListTail(last,tail);
	last = tail;
      }
      i++;
    }

    gcRemoveRoot(root);
    return Ok;
  }
}

/* Access host name from IP address */
retCode m_iptohost(processpo p,objPo *args)
{
  if(!isListOfChars(args[0]))
    return liberror("iptohost",1,"argument should be a string",einval);
  else{
    WORD32 hlen = ListLen(args[0]);
    uniChar hBuf[hlen+1];
    uniChar *host = getHostname(StringText(args[0],hBuf,hlen+1));

    if(host!=NULL){
      args[0]=allocateString(host);
      return Ok;
    }
    else
      return liberror("iptohost",1,"get find host",enet);
  }
}

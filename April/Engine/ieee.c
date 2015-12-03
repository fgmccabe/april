/*
  Special functions to convert fp into IEEE format 
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
#include <math.h>
#ifdef HAVE_LIBDL
#include <dlfcn.h>		/* dynamic load functions */
#endif

#include "april.h"
#include "astring.h"		/* String handling interface */
#include "sign.h"		/* signature handling interface */
#include "debug.h"
#include "dll.h"		/* dynamic loader interface */

static retCode m_str2ieee(processpo p,objPo *args);
static retCode m_ieee2str(processpo p,objPo *args);

/* This is used by the loader */

uniChar moduleSig[] = {'T',2,'F','S','N','F','N','S',0};

SignatureRec signature = {
  NULL,				/* Used by April */
  moduleSig,			/* two functions  */
  NumberOf(moduleSig),
  2,				/* one function is defined */
  {
    { m_str2ieee,1},		/* the string to fp conversion function */
    { m_ieee2str,1}		/* the fp to string conversion function */
  }
};

/*
 * Special functions to convert fp into IEEE format 
 */

retCode m_ieee2str(processpo p,objPo *args)
{
  Number f,ip;
  uniChar buff[16];
  int exp,i,sign=0;
  register objPo e1 = args[0];
      
  switch(Tag(e1)){
  case floatMarker:
    f = FloatVal(*args);
    break;
  case integerMarker:
    f = IntVal(*args);
    break;
  default:
    return liberror("ieee2str",1,"argument should be a number",einval);
  }

  if(f<0.0){
    sign = 0x80;
    f = -f;
  }

  f = frexp(f, &exp);

  buff[0] = sign|(((exp+1023)>>4)&0x7f);

  f = modf(f*16,&ip);		/* The most significant nibble */

  buff[1] = (((exp+1023)&0xf)<<4)| (((int)ip)&0xf);
    
  for(i=2;i<=8;i++){
    f = modf(f*256,&ip);
    buff[i] = (int)ip;
  }
  
  args[0] = allocateSubString(buff,8);
  return Ok;
}

static retCode m_str2ieee(processpo p,objPo *args)
{
  objPo e1 = args[0];

  if(!isListOfChars(e1) || ListLen(e1)!=8)
    return liberror("str2ieee",1,"argument should be an 8-byte string",einval);
  else{
    char bf[MAX_SYMB_LEN];
    unsigned char *buff = (unsigned char*)Cstring(e1,bf,NumberOf(bf));
    int sign = buff[0]>>7;
    int exp = (((buff[0]&0x7f)<<4)|(buff[1]>>4))-1023;
    double f = 0.0;
    int len = 8;

    while(len-->2)
      f = (f+buff[len])/0x100;
    f = (f+(buff[1]&0xf))/0x10;
    if(sign)
      f = -ldexp(f,exp);
    else
      f = ldexp(f,exp);
    args[0] = allocateNumber(f);
    return Ok;
  }
}



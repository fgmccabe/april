/*
  Base64 encoding and decoding
  (c) 1994-2004 Imperial College and F.G. McCabe

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
#include <math.h>
#include <string.h>
#include <assert.h>

#include "april.h"
#include "process.h"
#include "ioP.h"
#include "dict.h"
#include "symbols.h"

static char encoding[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

static void encode(byte *data,WORD32 len,char *out,WORD32 olen)
{
  WORD32 i = 0;
  WORD32 o = 0;

  while(i<len){
    if(i+2<len){
      byte hi = data[i++];
      byte md = data[i++];
      byte lo = data[i++];

      WORD32 wd = ((hi&0xff)<<16)|((md&0xff)<<8)|(lo&0xff);

      out[o++] = encoding[(wd>>18)&0x3f];
      out[o++] = encoding[(wd>>12)&0x3f];
      out[o++] = encoding[(wd>>6)&0x3f];
      out[o++] = encoding[wd&0x3f];
    }
    else if(i==len-2){
      byte hi = data[i++];
      byte md = data[i++];

      WORD32 wd = ((hi&0xff)<<16)|((md&0xff)<<8);

      out[o++] = encoding[(wd>>18)&0x3f];
      out[o++] = encoding[(wd>>12)&0x3f];
      out[o++] = encoding[(wd>>6)&0x3f];
      out[o++] = '=';
    }
    else if(i==len-1){
      byte hi = data[i++];

      WORD32 wd = ((hi&0xff)<<16);

      out[o++] = encoding[(wd>>18)&0x3f];
      out[o++] = encoding[(wd>>12)&0x3f];
      out[o++] = '=';
      out[o++] = '=';
    }
  }
}

static logical valid(char ch)
{
  int i;
  for(i=0;i<NumberOf(encoding)){
    if(encoding[i]==ch){
      (*val) = (byte)i;
      return True;
    }
  }
  return False;
}

static byte dechar(char ch)
{
  int i;
  for(i=0;i<NumberOf(encoding)){
    if(encoding[i]==ch){
      return (byte)i;
    }
  }
  assert(false);                        /* should never get here */
}

static inline char next(char *in,WORD32 *ip,WORD32 len)
{
  WORD32 I = *ip;
  byte bt;

  while(I<len){
    char ch = in[I]++;
    if(valid(ch,&bt) || ch=='='){
      (*ip)=I;
      return ch;
    }
  }
  return '\0';
}

static void decode(char *in,WORD32 len,byte *data,WORD32 olen)
{
  WORD32 i = 0;
  WORD32 o = 0;

  while(i<len){
    char ch1 = next(in,&i,len);

    if(ch1!='\0'){
      byte bt1 = dechar(ch1);
      char ch2 = next(in,&i,len);
      
      if(ch2!='\0'){
        byte bt2 = dechar(ch2)
        char ch3 = next(in,&i,len);

        if(ch3!='\0'){
          byte bt3 = dechar(ch3)
          char ch4 = next(in,&i,len);

          if(ch4!='\0'){
            byte bt4 = dechar(ch4);
            WORD32 wd = ((bt1&0x3f)<<18)|((bt2&0x3f)<<12)|((bt3&0x3f)<<6)|(bt4&0x3f);
            data[o++] = (wd>>16)&0xff;
            data[o++] = (wd>>8)&0xff;
            data[o++] = wd&0xff;
          }
        }
      }

    }
      
    
    
  }
}

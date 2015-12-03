/*
  Arithmetic functions for the April system
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

#include "config.h"		/* pick up standard configuration header */
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>		/* system error numbers */
#include <ctype.h>
#include <string.h>
#include "april.h"
#include "astring.h"
#include "arith.h"
#include "process.h"
#include "setops.h"

inline objPo allocateNumber(Number f)
{
  if(f==floor(f) && fabs(f)<=MAX_APRIL_INT){ /* is float really an integer? */
    register integer i = (integer)f;

    return allocateInteger(i);
  }
  return allocateFloat(f);
}

retCode m_integral(processpo p,objPo *args)
{
  register objPo e1 = args[0];

  if(IsInteger(e1))
    args[0]=ktrue;
  else
    args[0]=kfalse;
  return Ok;
}


/* Bitwise arithmetic operators */
retCode m_band(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];

  if(!IsInteger(e1) || !IsInteger(e2))
    return liberror("band",2,"arguments should be integer",einval);
  else{
    args[1]=allocateInteger(IntVal(e1)&IntVal(e2));
    return Ok;
  }
}

retCode m_bor(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];

  if(!IsInteger(e1) || !IsInteger(e2))
    return liberror("bor",2,"arguments should be integer",einval);
  else{
    args[1]=allocateInteger(IntVal(e1)|IntVal(e2));
    return Ok;
  }
}

retCode m_bxor(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];

  if(!IsInteger(e1) || !IsInteger(e2))
    return liberror("bxor",2,"arguments should be integer",einval);
  else{
    args[1]=allocateInteger(IntVal(e1)^IntVal(e2));
    return Ok;
  }
}

retCode m_bleft(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];

  if(!IsInteger(e1) || !IsInteger(e2))
    return liberror("bleft",2,"arguments should be integer",einval);
  else{
    integer shift = IntVal(e2);

    if(shift>=0)
      args[1]=allocateInteger(IntVal(e1)<<shift);
    else
      args[1]=allocateInteger(IntVal(e1)>>-shift); /* same as right shift */
    return Ok;
  }
}

retCode m_bright(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];

  if(!IsInteger(e1) || !IsInteger(e2))
    return liberror("bright",2,"arguments should be integer",einval);
  else{
    integer shift = IntVal(e2);

    if(shift>=0)
      args[1]=allocateInteger(IntVal(e1)>>shift);
    else
      args[1]=allocateInteger(IntVal(e1)<<-shift); /* same as left shift */
    return Ok;
  }
}

retCode m_bnot(processpo p,objPo *args)
{
  register objPo e1 = args[0];

  if(!IsInteger(e1))
    return liberror("bnot",1,"argument should be integer",einval);
  else{
    args[0]=allocateInteger(~IntVal(e1));
    return Ok;
  }
}


/* Escape versions of standard arithmetic functions */
retCode m_plus(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];
  Number result;
      
  switch(TwoTag(Tag(e1),Tag(e2))){
  case TwoTag(integerMarker,integerMarker):
    args[1] = allocateInteger(IntVal(e1)+IntVal(e2));
    return Ok;
  case TwoTag(integerMarker,floatMarker):
    result = FloatVal(e2)+IntVal(e1); /* Integer+Float */
    break;
  case TwoTag(floatMarker,integerMarker):
    result = IntVal(e2)+FloatVal(e1);
    break;
  case TwoTag(floatMarker,floatMarker):
    result = FloatVal(e2)+FloatVal(e1); /* Float+Float */
    break;
  default:
    return liberror("+",2,"incorrect type(s) of arguments",einval);
  }

  args[1] = allocateNumber(result);
  return Ok;
}

/* Integer only plus */
retCode m_iplus(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];
  
  if(Tag(e1)==integerMarker && Tag(e2)==integerMarker){
    args[1] = allocateInteger(IntVal(e1)+IntVal(e2));
    return Ok;
  }
  else
    return liberror(".+",2,"incorrect type(s) of arguments",einval);
}

retCode m_minus(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];
  Number result;
      
  switch(TwoTag(Tag(e1),Tag(e2))){
  case TwoTag(integerMarker,integerMarker):
    args[1] = allocateInteger(IntVal(e1)-IntVal(e2));
    return Ok;
  case TwoTag(integerMarker,floatMarker):
    result = IntVal(e1)-FloatVal(e2); /* Integer-Float */
    break;
  case TwoTag(floatMarker,integerMarker):
    result = FloatVal(e1)-IntVal(e2);
    break;
  case TwoTag(floatMarker,floatMarker):
    result = FloatVal(e1)-FloatVal(e2); /* Float-Float */
    break;
  default:
    return liberror("-",2,"incorrect type(s) of arguments",einval);
  }

  args[1] = allocateNumber(result);
  return Ok;
}

/* Integer only minus */
retCode m_iminus(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];
  
  if(Tag(e1)==integerMarker && Tag(e2)==integerMarker){
    args[1] = allocateInteger(IntVal(e1)-IntVal(e2));
    return Ok;
  }
  else
    return liberror(".-",2,"incorrect type(s) of arguments",einval);
}

retCode m_times(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];
  Number result;
      
  switch(TwoTag(Tag(e1),Tag(e2))){
  case TwoTag(integerMarker,integerMarker):
    args[1] = allocateInteger(IntVal(e1)*IntVal(e2));
    return Ok;
  case TwoTag(integerMarker,floatMarker):
    result = FloatVal(e2)*IntVal(e1); /* Integer*Float */
    break;
  case TwoTag(floatMarker,integerMarker):
    result = IntVal(e2)*FloatVal(e1);
    break;
  case TwoTag(floatMarker,floatMarker):
    result = FloatVal(e2)*FloatVal(e1); /* Float*Float */
    break;
  default:
    return liberror("*",2,"incorrect type(s) of arguments",einval);
  }

  args[1] = allocateNumber(result);
  return Ok;
}

/* Integer only multiplication */
retCode m_itimes(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];
  
  if(Tag(e1)==integerMarker && Tag(e2)==integerMarker){
    args[1] = allocateInteger(IntVal(e1)*IntVal(e2));
    return Ok;
  }
  else
    return liberror(".*",2,"incorrect type(s) of arguments",einval);
}

retCode m_div(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];

  switch(TwoTag(Tag(e1),Tag(e2))){
  case TwoTag(integerMarker,integerMarker):{
    integer divisor = IntVal(e2);

    if(divisor==0)
      return liberror("/",2,"division by zero",earith);
    else{
      args[1] = allocateInteger(IntVal(e1)/divisor);
      return Ok;
    }
  }
  case TwoTag(integerMarker,floatMarker):{
    Number divisor = FloatVal(e2);

    if(divisor==0.0)
      return liberror("/",2,"division by zero",earith);
    else{
      args[1] = allocateNumber(IntVal(e1)/divisor);
      return Ok;
    }
  }
  case TwoTag(floatMarker,integerMarker):{
    integer divisor = IntVal(e2);

    if(divisor==0)
      return liberror("/",2,"division by zero",earith);
    else{
      args[1] = allocateNumber(FloatVal(e1)/divisor);
      return Ok;
    }
  }
  case TwoTag(floatMarker,floatMarker):{
    Number divisor = FloatVal(e2);

    if(divisor==0.0)
      return liberror("/",2,"division by zero",earith);
    else{
      args[1] = allocateNumber(FloatVal(e1)/divisor);
      return Ok;
    }
  }
  default:
    return liberror("/",2,"incorrect type(s) of arguments",earith);
  }
}

/* Integer only division */
retCode m_idiv(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];
  
  if(Tag(e1)==integerMarker && Tag(e2)==integerMarker){
    integer Div = IntVal(e2);
    
    if(Div==0)
      return liberror("./",2,"division by zero",earith);
      
    args[1] = allocateInteger(IntVal(e1)+Div);
    return Ok;
  }
  else
    return liberror("./",2,"incorrect type(s) of arguments",einval);
}

static inline Number roundNum(register Number f)
{
  if(f>=0.0)
    return floor(f);
  else
    return ceil(f);
}

retCode m_quo(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];

  switch(TwoTag(Tag(e1),Tag(e2))){
  case TwoTag(integerMarker,integerMarker):{
    integer divisor = IntVal(e2);

    if(divisor==0)
      return liberror("quot",2,"division by zero",earith);
    else{
      args[1] = allocateInteger(IntVal(e1)/divisor);
      return Ok;
    }
  }
  case TwoTag(integerMarker,floatMarker):{
    Number divisor = FloatVal(e2);

    if(divisor==0.0)
      return liberror("quot",2,"division by zero",earith);
    else{
      args[1] = allocateNumber(roundNum((Number)IntVal(e1)/divisor));
      return Ok;
    }
  }
  case TwoTag(floatMarker,integerMarker):{
    integer divisor = IntVal(e2);

    if(divisor==0)
      return liberror("quot",2,"division by zero",earith);
    else{
      args[1] = allocateNumber(roundNum(FloatVal(e1)/divisor));
      return Ok;
    }
  }
  case TwoTag(floatMarker,floatMarker):{
    Number divisor = FloatVal(e2);

    if(divisor==0.0)
      return liberror("quot",2,"division by zero",earith);
    else{
      args[1] = allocateNumber(roundNum(FloatVal(e1)/divisor));
      return Ok;
    }
  }
  default:
    return liberror("quot",2,"incorrect type(s) of arguments",einval);
  }
}

retCode m_rem(processpo p,objPo *args)
{
  register objPo e1 = args[1];
  register objPo e2 = args[0];

  switch(TwoTag(Tag(e1),Tag(e2))){
  case TwoTag(integerMarker,integerMarker):{
    integer divisor = IntVal(e2);

    if(divisor==0)
      return liberror("rem",2,"division by zero",earith);
    else{
      args[1] = allocateInteger(IntVal(e1)%divisor);
      return Ok;
    }
  }
  case TwoTag(integerMarker,floatMarker):{
    Number divisor = FloatVal(e2);

    if(divisor==0.0)
      return liberror("rem",2,"division by zero",earith);
    else{
      args[1] = allocateNumber(fmod((Number)IntVal(e1),divisor));
      return Ok;
    }
  }
  case TwoTag(floatMarker,integerMarker):{
    integer divisor = IntVal(e2);

    if(divisor==0)
      return liberror("rem",2,"division by zero",earith);
    else{
      args[1] = allocateNumber(fmod(FloatVal(e1),divisor));
      return Ok;
    }
  }
  case TwoTag(floatMarker,floatMarker):{
    Number divisor = FloatVal(e2);

    if(divisor==0.0)
      return liberror("rem",2,"division by zero",earith);
    else{
      args[1] = allocateNumber(fmod(FloatVal(e1),divisor));
      return Ok;
    }
  }
  default:
    return liberror("rem",2,"incorrect type(s) of arguments",einval);
  }
}

retCode m_abs(processpo p,objPo *args)
{
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:{
      integer rslt = IntVal(e1);        /* We do this the hard way 'cos abs don't work */
      
      if(rslt<0)
        rslt = -rslt;
        
      args[0] = allocateInteger(rslt);
      return Ok;
    }
  case floatMarker:
    args[0] = allocateFloat(fabs(FloatVal(e1)));
    return Ok;
  default:
    return liberror("abs",1,"argument should be integer or float",einval);
  }
}

static inline int char2digit(uniChar ch,int base)
{
  if(ch>='0'&&ch<='9')
    return ch-'0';
  else if(ch>='A'&&ch<='F')
    return ch-'A'+10;
  else if(ch>='a'&&ch<='f')
    return ch-'a'+10;
  else
    return -1;
}
    
static integer str2integer(uniChar *s,uniChar **end,int base)
{
  integer number = 0;
  int sign = 1;
  uniChar ch;
  int dgt;

  while(isspace((int)(ch=*s)))		/* consume space from the string */
    s++;

  if(ch=='-'){
    sign=-1;
    ch=*s++;
  }

  if(base==0){
    if(uniIsLitPrefix(s,"0x"))
      base = 16;		/* hexadecimal base */
    else if(*s=='0')
      base = 8;			/* octal base */
    else
      base = 10;		/* decimal base */
  }

  if(base==16 && uniIsLitPrefix(s,"0x"))
    s+=2;

  while((dgt=char2digit(ch=*s,base))>=0){
    number=number*base+dgt;
    s++;
  }

  if(end!=NULL)
    *end = s;

  return sign*number;
}


retCode m_itrunc(processpo p,objPo *args)
{
  register objPo el = args[0];
  
  if(IsInteger(el))
    return Ok;
  else if(IsFloat(el)){
    args[0] = allocateInteger(roundNum(FloatVal(el)));
    return Ok;
  }
  else if(isSymb(el)){
    uniChar *rest;
    integer i = str2integer(SymText(el),&rest,0);

    if(*rest=='\0'){
      args[0] = allocateInteger(i);
      return Ok;
    }
    else
      return liberror("itrunc",1,"argument cannot be coerced to integer",einval);
  }
  else if(isListOfChars(el)){
    WORD32 len = ListLen(el)+1;
    uniChar buff[len];
    
    StringText(el,buff,len);
    
    {
      uniChar *rest;
      integer i = str2integer(buff,&rest,0);

      if(*rest=='\0'){
        args[0] = allocateInteger(i);
        return Ok;
      }
      else
        return liberror("itrunc",1,"argument cannot be coerced to integer",einval);
    }
  }
  else
    return liberror("itrunc",1,"argument cannot be coerced to integer",einval);
}

static inline logical safeEnd(uniChar *p,uniChar *e)
{
  while(p<e && isZsChar(*p))
    p++;

  if(p==e)
    return True;
  else
    return False;
}

objPo str2num(uniChar *buff,WORD32 len)
{
  uniChar *in = buff;
  uniChar buffer[256],*tk=buffer;
  uniChar *end = &buff[len];
  integer mant = 0;

  if(in<end-1 && *in=='0'){
    if(in<end && (*++in=='x'||*in=='X')){
      unsigned integer X=0;
      int dgt;

      in++;			/* skip over the hex marker */

      while(in<end && (dgt=char2digit(*in++,16))>=0)
	X=X*16+dgt;

      if(safeEnd(in,end))	/* check that we reached the end */
	return allocateInteger(X);
      else
	return NULL;
    }
    else if(in<end && *in!='.'){		/* Octal number */
      unsigned integer O=0;
      while(in<end && *in>='0' && *in <= '7')
	O=(O<<3)+((*in++)&0xf);

      if(safeEnd(in,end))	/* check that we reached the end */
	return allocateInteger(O);
      else
	return NULL;
    }
  }

  while(in<end && isNdChar((int)*in)){
    *tk++=*in;
    mant = mant*10+(((int)*in++)&0xf);
  }
    
  if(in<end-1 && *in=='.' && isNdChar((int)in[1])){ /* floating point number */
    *tk++=*in++;

    while(in<end && isNdChar((int)*in))
      *tk++=*in++;

    if(in<end && (*in=='e'||*in=='E')){	/* read in the exponent part */
      *tk++=*in++;
      if(*in=='-'||*in=='+')	/* copy the sign character */
	*tk++=*in++;

      while(in<end && isNdChar((int)*in))
	*tk++=*in++;
    }
    *tk='\0';			/* end of floating point number */
      
    if(safeEnd(in,end))		/* check that we reached the end */
      return allocateNumber(parseNumber(buffer,uniStrLen(buffer)));
    else
      return NULL;
  }
  else{				/* regular decimal number */
    *tk='\0';
    if(safeEnd(in,end))		/* check that we reached the end */
      return allocateInteger(mant);
    else
      return NULL;
  }
}

/*
 * number
 * 
 * any scalar into a number 
 */

retCode m_number(processpo p,objPo *args)
{
  register objPo el = args[0];
  
  if(IsInteger(el))
    return Ok;
  else if(IsFloat(el)){
    return Ok;
  }
  else if(isSymb(el)){
    objPo num = str2num(SymText(el),uniStrLen(SymText(el)));

    if(num!=NULL)
      args[0] = num;
    else
      return liberror("_number",1,"argument cannot be coerced to number",einval);
    return Ok;
  }
  else if(isListOfChars(el)){
    WORD32 len = ListLen(el)+1;
    uniChar buff[len];
    
    StringText(el,buff,len);
    
    {
      objPo num = str2num(buff,len-1);

      if(num!=NULL)
        args[0] = num;
      else
        return liberror("_number",1,"argument cannot be coerced to number",einval);
      return Ok;
    }
  }
  else
    return liberror("_number",1,"argument cannot be coerced to number",einval);
}

retCode m_trunc(processpo p,objPo *args)
{
  register objPo el = args[0];

  switch(Tag(el)){
  case integerMarker:
    return Ok;			/* no operation in this case */
  case floatMarker:
    args[0] = allocateInteger((integer)roundNum(FloatVal(el)));
    return Ok;
  default:
    return liberror("trunc",1,"Argument should be numeric",einval);
  }
}

retCode m_ceil(processpo p,objPo *args)
{
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    return Ok;			/* no operation in this case */
  case floatMarker:
    args[0] = allocateFloat(ceil(FloatVal(e1)));
    return Ok;
  default:
    return liberror("ceil",1,"Argument should be numeric",einval);
  }
}

retCode m_floor(processpo p,objPo *args)
{
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    return Ok;			/* no operation in this case */
  case floatMarker:
    args[0] = allocateFloat(floor(FloatVal(e1)));
    return Ok;
  default:
    return liberror("floor",1,"Argument should be numeric",einval);
  }
}

retCode m_sqrt(processpo p,objPo *args)
{
  register objPo e1 = args[0];
  Number num;

  switch(Tag(e1)){
  case integerMarker:
    num = (Number)IntVal(e1);
    break;
  case floatMarker:
    num = FloatVal(e1);
    break;
  default:
    return liberror("sqrt",1,"Argument should be numeric",einval);
  }

  if(num<0)
    return liberror("sqrt",1,"argument should be positive number",einval);

  args[0] = allocateNumber(sqrt(num));
  return Ok;
}

retCode m_cbrt(processpo p,objPo *args)
{
  Number third = (Number)1/3;
  register objPo e1 = args[0];
  Number num;

  switch(Tag(e1)){
  case integerMarker:
    num = (Number)IntVal(e1);
    break;
  case floatMarker:
    num = FloatVal(e1);
    break;
  default:
    return liberror("cbrt",1,"Argument should be numeric",einval);
  }

  errno = 0;
  if(num>=0)
    num = pow(num,third);
  else
    num = -pow(-num,third);

  if(errno!=0){
    if(errno==EDOM)
      return liberror("cbrt",1,"domain error or singularity",earith);
    else
      return liberror("cbrt",1,"unknown arithmetic error",earith);
  }

  args[0] = allocateNumber(num);
  return Ok;
}

retCode m_pow(processpo p,objPo *args)
{
  Number num1, num2;
  register objPo e1 = args[1];
  register objPo e2 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("pow",2,"Argument should be numeric",einval);
  }

  switch(Tag(e2)){
  case integerMarker:
    num2 = (Number)IntVal(e2);
    break;
  case floatMarker:
    num2 = FloatVal(e2);
    break;
  default:
    return liberror("pow",2,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to computation */
  num1 = pow(num1,num2);	/* allow for checks of the answer */
  if(errno!=0){
    if(errno==EDOM)
      return liberror("pow",2,"domain error or singularity",earith);
    else if(errno==ERANGE)
      return liberror("pow",2,"floating point overflow or underflow",earith);
    else
      return liberror("pow",2,"unknown arithmetic error",earith);
  }

  args[1] = allocateNumber(num1);
  return Ok;
}

retCode m_exp(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("exp",1,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to computation */
  num1 = exp(num1);		/* allow for checks of the answer */
  if(errno!=0){
    if(errno==EDOM)
      return liberror("exp",1,"domain error or singularity",earith);
    else if(errno==ERANGE)
      return liberror("exp",1,"floating point overflow or underflow",earith);
    else
      return liberror("exp",1,"unknown arithmetic error",earith);
  }
  args[0] = allocateNumber(num1);
  return Ok;
}

retCode m_log(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("exp",1,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to computation */
  num1 = log(num1);		/* allow for checks of the answer */
  if(errno!=0){
    if(errno==EDOM)
      return liberror("log",1,"domain error or singularity",earith);
    else if(errno==ERANGE)
      return liberror("log",1,"floating point overflow or underflow",earith);
    else
      return liberror("log",1,"unknown arithmetic error",earith);
  }
  args[0] = allocateNumber(num1);
  return Ok;
}

retCode m_log10(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("exp",1,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to computation */
  num1 = log10(num1);		/* allow for checks of the answer */
  if(errno!=0){
    if(errno==EDOM)
      return liberror("log10",1,"domain error or singularity",earith);
    else if(errno==ERANGE)
      return liberror("log10",1,"floating point overflow or underflow",earith);
    else
      return liberror("log10",1,"unknown arithmetic error",earith);
  }
  args[0] = allocateNumber(num1);
  return Ok;
}

retCode m_pi(processpo p,objPo *args)
{
  args[-1] = allocateFloat(PI);
  p->sp = args-1;		/* adjust sp in case of gc */
  return Ok;
}

retCode m_sin(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("sin",1,"Argument should be numeric",einval);
  }

  args[0] = allocateNumber(sin(num1));
  return Ok;
}

retCode m_cos(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("cos",1,"Argument should be numeric",einval);
  }

  args[0] = allocateNumber(cos(num1));
  return Ok;
}

retCode m_tan(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("tan",1,"Argument should be numeric",einval);
  }

  args[0] = allocateNumber(tan(num1));
  return Ok;
}

retCode m_asin(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("asin",1,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to asin computation */
  num1 = asin(num1);		/* allow for checks of the answer */
  if(errno!=0){
    if(errno==EDOM)
      return liberror("asin",1,"domain error or singularity",earith);
    else
      return liberror("asin",1,"unknown arithmetic error",earith);
  }

  args[0] = allocateNumber(num1);
  return Ok;
}

retCode m_acos(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("acos",1,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to acos computation */
  num1 = acos(num1);		/* allow for checks of the answer */
  if(errno!=0){
    if(errno==EDOM)
      return liberror("acos",1,"domain error or singularity",earith);
    else
      return liberror("acos",1,"unknown arithmetic error",earith);
  }

  args[0] = allocateNumber(num1);
  return Ok;
}

retCode m_atan(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("atan",1,"Argument should be numeric",einval);
  }

  args[0] = allocateNumber(atan(num1));
  return Ok;
}

retCode m_sinh(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("sinh",1,"Argument should be numeric",einval);
  }

  num1 = sinh(num1);		/* allow for checks of the answer */

  if(errno!=0){
    if(errno==EDOM)
      return liberror("sinh",1,"domain error or singularity",earith);
    else if(errno==ERANGE)
      return liberror("sinh",1,"floating point overflow or underflow",earith);
    else
      return liberror("sinh",1,"unknown arithmetic error",earith);
  }
  args[0] = allocateNumber(num1);
  return Ok;
}

retCode m_cosh(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("cosh",1,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to asin computation */
  num1 = cosh(num1);		/* allow for checks of the answer */
  if(errno!=0){
    if(errno==EDOM)
      return liberror("cosh",1,"domain error or singularity",earith);
    else if(errno==ERANGE)
      return liberror("cosh",1,"floating point overflow or underflow",earith);
    else
      return liberror("cosh",1,"unknown arithmetic error",earith);
  }
  args[0] = allocateNumber(num1);
  return Ok;
}

retCode m_tanh(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("tanh",1,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to asin computation */
  num1 = tanh(num1);		/* allow for checks of the answer */

  if(errno!=0){
    if(errno==EDOM)
      return liberror("tanh",1,"domain error or singularity",earith);
    else if(errno==ERANGE)
      return liberror("tanh",1,"floating point overflow or underflow",earith);
    else
      return liberror("tanh",1,"unknown arithmetic error",earith);
  }
  args[0] = allocateNumber(num1);
  return Ok;
}

retCode m_asinh(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("asinh",1,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to asin computation */
  num1 = asinh(num1);		/* allow for checks of the answer */

  if(errno!=0){
    if(errno==EDOM)
      return liberror("asinh",1,"domain error or singularity",earith);
    else if(errno==ERANGE)
      return liberror("asinh",1,"floating point overflow or underflow",earith);
    else
      return liberror("asinh",1,"unknown arithmetic error",earith);
  }
  args[0] = allocateNumber(num1);
  return Ok;
}

retCode m_acosh(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("acosh",1,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to asin computation */
  num1 = acosh(num1);		/* allow for checks of the answer */

  if(errno!=0){
    if(errno==EDOM)
      return liberror("acosh",1,"domain error or singularity",earith);
    else if(errno==ERANGE)
      return liberror("acosh",1,"floating point overflow or underflow",earith);
    else
      return liberror("acosh",1,"unknown arithmetic error",earith);
  }
  args[0] = allocateNumber(num1);
  return Ok;
}

retCode m_atanh(processpo p,objPo *args)
{
  Number num1;
  register objPo e1 = args[0];

  switch(Tag(e1)){
  case integerMarker:
    num1 = (Number)IntVal(e1);
    break;
  case floatMarker:
    num1 = FloatVal(e1);
    break;
  default:
    return liberror("atanh",1,"Argument should be numeric",einval);
  }

  errno = 0;			/* clear errno prior to asin computation */
  num1 = atanh(num1);		/* allow for checks of the answer */

  if(errno!=0){
    if(errno==EDOM)
      return liberror("atanh",1,"domain error or singularity",earith);
    else if(errno==ERANGE)
      return liberror("atanh",1,"floating point overflow or underflow",earith);
    else
      return liberror("atanh",1,"unknown arithmetic error",earith);
  }
  args[0] = allocateNumber(num1);
  return Ok;
}

  
/*
 * seed(n)
 * 
 * reinitialize the random numbers generator
 */

retCode m_seed(processpo p,objPo *args)
{
  integer i;
  if(!IsInteger(*args) ||(i=IntVal(*args))<0)
    return liberror("seed",1,"argument should be a positive integer",einval);
  else
    srandom(i);
  return Ok;
}


/*
 * rand(n)
 * 
 * generate a new random number
 */
retCode m_rand(processpo p,objPo *args)
{
  Number r;

  switch(Tag(*args)){
  case floatMarker:		/* [0.0, arg) */
    r = FloatVal(args[0]);
    break;
  case integerMarker:		/* [0.0, arg) */
    r = IntVal(args[0]);
    break;
  default:
    return liberror("rand",1,"argument should be a positive number",einval);
  }

  if(r<=0)
    return liberror("rand",1,"argument should be a positive number",einval);

  args[0] = allocateNumber((((Number)random())/RAND_MAX) * r);
  return Ok;
}

/*
 * irand(n)
 * 
 * generate a random integer
 */
retCode m_irand(processpo p,objPo *args)
{
  Number next = (Number)random();

  if(!IsInteger(args[0]))
    return liberror("irand",1,"Argument should be integer",einval);
  else{
    Number num1 = IntVal(args[0]);

    if(num1<0)
      return liberror("irand",1,"argument should be a positive integer",einval);

    args[0] = allocateInteger(next*num1/RAND_MAX);
    return Ok;
  }
}

/*
 * Functions to assist in the manipulations of the fp number 
 */

retCode m_ldexp(processpo p,objPo *args)
{
  Number f;
  register objPo e1 = args[1];
  register objPo e2 = args[0];
      
  switch(Tag(e1)){
  case floatMarker:
    f = FloatVal(*args);
    break;
  case integerMarker:
    f = IntVal(*args);
    break;
  default:
    return liberror("ldexp",2,"argument should be a number",einval);
  }

  if(!IsInteger(e2))
    return liberror("ldexp",2,"2nd argument should be an integer",einval);

  args[1] = allocateNumber(ldexp(f,IntVal(e2)));
  return Ok;
}

retCode m_frexp(processpo p,objPo *args)
{
  Number f;
  int exp;
  register objPo e1 = args[0];
      
  switch(Tag(e1)){
  case floatMarker:
    f = FloatVal(e1);
    break;
  case integerMarker:
    f = IntVal(e1);
    break;
  default:
    return liberror("frexp",1,"argument should be a number",einval);
  }

  f = frexp(f,&exp);

  {
    objPo ff = allocateNumber(f);
    void *root = gcAddRoot(&ff);
    objPo fe = allocateInteger(exp);

    gcAddRoot(&fe);

    args[0] = allocateTuple(2);

    updateTuple(args[0],0,ff);
    updateTuple(args[0],1,fe);

    gcRemoveRoot(root);
  }

  return Ok;
}

retCode m_modf(processpo p,objPo *args) /* split an fp into integer and fraction */
{
  Number f,ip;
  register objPo e1 = args[0];
      
  switch(Tag(e1)){
  case floatMarker:
    f = FloatVal(e1);
    break;
  case integerMarker:
    f = IntVal(e1);
    break;
  default:
    return liberror("modf",1,"argument should be a number",einval);
  }

  f = modf(f,&ip);

  {
    objPo ff = allocateNumber(f);
    void *root = gcAddRoot(&ff);
    objPo fi = allocateInteger(ip);

    gcAddRoot(&fi);

    args[0] = allocateTuple(2);

    updateTuple(args[0],0,fi);
    updateTuple(args[0],1,ff);

    gcRemoveRoot(root);
  }

  return Ok;
}

/* Return true if same pointer value */
retCode m_same(processpo p,objPo *args)
{
  objPo e1 = args[0];
  objPo e2 = args[1];

  args[1] = ktrue;		/* default to true */

  switch(TwoTag(Tag(e1),Tag(e2))){
  case TwoTag(integerMarker,integerMarker):
    if(IntVal(e1)!=IntVal(e2))
      args[1] = kfalse;
    return Ok;
  case TwoTag(integerMarker,floatMarker):
    if(((Number)(IntVal(e1)))!=FloatVal(e2))
      args[1] = kfalse;
    return Ok;
  case TwoTag(floatMarker,integerMarker):
    if(FloatVal(e1)!=((Number)(IntVal(e2))))
      args[1] = kfalse;
    return Ok;
  case TwoTag(floatMarker,floatMarker):
    if(FloatVal(e1)!=FloatVal(e2))
      args[1] = kfalse;
    return Ok;
  case TwoTag(symbolMarker,symbolMarker):
  case TwoTag(tupleMarker,tupleMarker):
  case TwoTag(consMarker,consMarker):
  case TwoTag(listMarker,listMarker):
  case TwoTag(anyMarker,anyMarker):
  case TwoTag(handleMarker,handleMarker):
    if(e1!=e2)
      args[1] = kfalse;
    return Ok;
  case TwoTag(charMarker,charMarker):
    if(CharVal(e1)!=CharVal(e2))
      args[1]=kfalse;
    return Ok;
  default:
    args[1] = kfalse;
    return Ok;
  }
}

/* Return true if NOT same pointer value */
retCode m_notsame(processpo p,objPo *args)
{
  objPo e1 = args[0];
  objPo e2 = args[1];

  args[1] = ktrue;		/* default to true */

  switch(TwoTag(Tag(e1),Tag(e2))){
  case TwoTag(integerMarker,integerMarker):
    if(IntVal(e1)==IntVal(e2))
      args[1] = kfalse;
    return Ok;
  case TwoTag(integerMarker,floatMarker):
    if(((Number)(IntVal(e1)))==FloatVal(e2))
      args[1] = kfalse;
    return Ok;
  case TwoTag(floatMarker,integerMarker):
    if(FloatVal(e1)==((Number)(IntVal(e2))))
      args[1] = kfalse;
    return Ok;
  case TwoTag(floatMarker,floatMarker):
    if(FloatVal(e1)==FloatVal(e2))
      args[1] = kfalse;
    return Ok;
  case TwoTag(symbolMarker,symbolMarker):
  case TwoTag(consMarker,consMarker):
  case TwoTag(tupleMarker,tupleMarker):
  case TwoTag(anyMarker,anyMarker):
  case TwoTag(listMarker,listMarker):
  case TwoTag(handleMarker,handleMarker):
    if(e1==e2)
      args[1] = kfalse;
    return Ok;
  case TwoTag(charMarker,charMarker):
    if(CharVal(e1)==CharVal(e2))
      args[1]=kfalse;
    return Ok;
  default:
    return Ok;
  }
}


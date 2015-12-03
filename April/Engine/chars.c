/*
  Character type handling 
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

  Ascii character table mapping characters to character types
*/

#include "config.h"		/* pick up standard configuration header */
#include <string.h>

#include "april.h"

/* Character type predicates */

// other control char
retCode m_isCcChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isCcChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isCcChar",1,"argument should an symbol",einval);
}

// other format char
retCode m_isCfChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isCfChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isCfChar",1,"argument should an symbol",einval);
}

// other unassigned char
retCode m_isCnChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isCnChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isCnChar",1,"argument should an symbol",einval);
}

// other private char
retCode m_isCoChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isCoChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isCoChar",1,"argument should an symbol",einval);
}

// other surrugate char
retCode m_isCsChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isCsChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isCsChar",1,"argument should an symbol",einval);
}

// Letter, lowercase char
retCode m_isLlChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isLlChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isLlChar",1,"argument should an symbol",einval);
}

// Letter, modifier char
retCode m_isLmChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isLmChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isLmChar",1,"argument should an symbol",einval);
}

// Letter, other char
retCode m_isLoChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isLoChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isLoChar",1,"argument should an symbol",einval);
}

// Letter, title char
retCode m_isLtChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isLtChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isLtChar",1,"argument should an symbol",einval);
}

// Letter, uppercase char
retCode m_isLuChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isLuChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isLuChar",1,"argument should an symbol",einval);
}

// Mark, spacing combining char
retCode m_isMcChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isMcChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isMcChar",1,"argument should an symbol",einval);
}

// Mark, enclosing char
retCode m_isMeChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isMeChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isMeChar",1,"argument should an symbol",einval);
}

// Mark, non-spacing char
retCode m_isMnChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isMnChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isMnChar",1,"argument should an symbol",einval);
}

// Number, decimal digit char
retCode m_isNdChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isNdChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isNdChar",1,"argument should an symbol",einval);
}

// Number, letter char
retCode m_isNlChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isNlChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isNlChar",1,"argument should an symbol",einval);
}

// Number, other char
retCode m_isNoChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isNoChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isNoChar",1,"argument should an symbol",einval);
}

// Punctuation, connector char
retCode m_isPcChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isPcChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isPcChar",1,"argument should an symbol",einval);
}

// Punctuation, dash char
retCode m_isPdChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isPdChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isPdChar",1,"argument should an symbol",einval);
}

// Punctuation, close char
retCode m_isPeChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isPeChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isPeChar",1,"argument should an symbol",einval);
}

// Punctuation, final char
retCode m_isPfChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isPfChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isPfChar",1,"argument should an symbol",einval);
}

// Punctuation, initial quote char
retCode m_isPiChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isPiChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isPiChar",1,"argument should an symbol",einval);
}

// Punctuation, other char
retCode m_isPoChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isPoChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isPoChar",1,"argument should an symbol",einval);
}

// Punctuation, open quote char
retCode m_isPsChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isPsChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isPsChar",1,"argument should an symbol",einval);
}

// Symbol, currency char
retCode m_isScChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isScChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isScChar",1,"argument should an symbol",einval);
}

// Symbol, modifier char
retCode m_isSkChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isSkChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isSkChar",1,"argument should an symbol",einval);
}

// Symbol, math char
retCode m_isSmChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isSmChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isSmChar",1,"argument should an symbol",einval);
}

// Symbol, other char
retCode m_isSoChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isSoChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isSoChar",1,"argument should an symbol",einval);
}

// Separator, line char
retCode m_isZlChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isZlChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isZlChar",1,"argument should an symbol",einval);
}

// Separator, paragraph char
retCode m_isZpChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isZpChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isZpChar",1,"argument should an symbol",einval);
}

// Separator, space char
retCode m_isZsChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isZsChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isZsChar",1,"argument should an symbol",einval);
}

// Letter char
retCode m_isLetterChar(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isLetterChar(ch))
      args[0]=ktrue;
    else
      args[0]=kfalse;
    return Ok;
  }
  else
    return liberror("isZsChar",1,"argument should an symbol",einval);
}


// return digit code
retCode m_digitCode(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    uniChar ch = CharVal(e1);
    
    if(isNdChar(ch)){
      args[0]=allocateInteger(digitValue(ch));
      return Ok;
    }
    else
      return liberror("digitCode",1,"argument should an symbol",einval);
  }
  else
    return liberror("digitCode",1,"argument should an symbol",einval);
}

// return unicode code
retCode m_charCode(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(isChr(e1)){
    args[0]=allocateInteger(CharVal(e1));
    return Ok;
  }
  else
    return liberror("charCode",1,"argument should a character",einval);
}

// return unicode character
retCode m_charOf(processpo p,objPo *args)
{
  objPo e1 = args[0];
  if(IsInteger(e1)){
    args[0]=allocateChar(IntVal(e1));
    return Ok;
  }
  else
    return liberror("charOf",1,"argument should an integer",einval);
}


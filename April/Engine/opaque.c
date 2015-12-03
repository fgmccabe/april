/* 
  Opaque type interface
  (c) 1994-2000 Imperial College and F.G.McCabe

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
#include <assert.h>
#include <limits.h>
#include <string.h>
#include "april.h"
#include "gcP.h"		/* private header info for G/C */
#include "symbols.h"
#include "process.h"
#include "msg.h"

static struct {
  opaqueHdlr h;
  void *cl;
} hdlrs[OPAQUE_MASK];

void registerOpaqueType(opaqueHdlr cb,short int code,void *cl)
{
  assert(hdlrs[code].h==NULL);

  hdlrs[code].h = cb;
  hdlrs[code].cl = cl;
}

retCode displayOpaque(ioPo f,opaquePo p)
{
  short int type = OpaqueType((objPo)p);

  return hdlrs[type].h(showOpaque,OpaqueVal(p),f,hdlrs[type].cl);
}




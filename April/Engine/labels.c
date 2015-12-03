/*
  Label mgt functions
  (c) 1994-2002 Imperial College and F.G. McCabe

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
#include "pool.h"               // We need a pool of labels
#include "labels.h"             // Support for label management

static poolPo lblPool = NULL;

lblPo createLabel(objPo term,int ref,lblPo chain)
{
  if(lblPool==NULL)
    lblPool = newPool(sizeof(LabelRec),16);
    
  {
    lblPo lbl = (lblPo)allocPool(lblPool);
    lbl->term = term;
    lbl->ref = ref;
    lbl->prev = chain;
    return lbl;
  }
}

int searchLabels(objPo term,lblPo chain)
{
  while(chain!=NULL){
    if(chain->term==term)
      return chain->ref;
    chain = chain->prev;
  }
  
  return -1;
}

objPo findObj(int ref,lblPo chain)
{
  while(chain!=NULL){
    if(chain->ref == ref)
      return chain->term;
    chain = chain->prev;
  }
  return NULL;
}

lblPo freeAllLabels(lblPo chain)
{
  while(chain!=NULL){
    lblPo p = chain->prev;
    
    freePool(lblPool,chain);
    chain = p;
  }
  return NULL;
}

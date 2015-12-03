/* 
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

  A special escape to pick up on arguments to a program
*/

#include "config.h"		/* pick up standard configuration header */
#include <string.h>
#include <stdlib.h>		/* Standard C library */
#include "april.h"
#include "process.h"
#include "dict.h"

static char **argsv=NULL;	/* Store the command line list */
static int argcnt=0;

void init_args(char **argv, int argc, int start)
{
  argsv = &argv[start];
  argcnt = argc-start;
}

retCode m_command_line(processpo p,objPo *args)
{
  int i;
  objPo alist=kvoid;
  objPo elist=kvoid;
  objPo ag = kvoid;
  void *root = gcAddRoot(&alist);

  gcAddRoot(&elist);
  gcAddRoot(&ag);  

  for(i=0;i<argcnt;i++){
    ag = allocateCString(argsv[i]);
    ag = allocatePair(&ag,&emptyList); /* construct the argument entry */

    if(alist==kvoid){
      alist = elist = ag;
    }
    else{
      updateListTail(elist,ag);
      elist = ag;
    }
  }

  args[-1] = alist;
  p->sp=args-1;			/* adjust caller's stack pointer */
  gcRemoveRoot(root);		/* clear off additional roots */
  return Ok;
}

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

/* Term display utilities */
void Display(ioPo f, cellpo d);
void dumpInput(ioPo f,char *msg, cellpo input);
void dc(cellpo p);		/* display cell value */
void dC(cellpo p);		/* display cell value in pretty format */
void dA(cellpo p);		/* display cell value in pretty format */

#endif

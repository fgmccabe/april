#ifndef _TREE_H_
#define _TREE_H_
/*
 * Functions to handle tree parsing and dumping ... 
 */

void parseTree(ioPo inFile,cellpo tgt);
void treeDump(ioPo outFile,cellpo p,cellpo file,long lno);

#endif

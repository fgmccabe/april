/*
 * Private header file when we must access cell structures
 */
#ifndef _CELLP_H_
#define _CELLP_H_

/* this is the declaration of the cell structure */
typedef struct _cell_ {		/* This is expanded for the compiler ... */
  short int l_info;		/* Line number index */
  tag_tp tag;			/* The tag info */
  short int visited;		/* Used during macro processing */
  union {
    symbpo s;			/* When the cell is a symbol */
    uniChar ch;                 /* A character literal */
    uniChar *st;		/* A string, or identifier */
    cellpo t;			/* A tuple pointer or type var value */
    floatpo f;			/* A floating point number */
    long i;			/* Integer */
    codepo c;			/* Code segment */
  } d;
  cellpo type;			/* type information */
  cellpo prev;                  /* link list chain of allocated cells */
} cell;


#endif

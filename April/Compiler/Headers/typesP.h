#ifndef _TYPESP_H_
#define _TYPESP_H_

/* Private data structures for maintaining type environments */

typedef struct _type_env_ {
  cell typevar;
  cellpo type;
  envPo tail;
} TypeEnvRec;

typedef struct _non_gen_ {
  cellpo type;
  lstPo tail;
} typeListRec;

/* Structures used in dependency analysis */
typedef struct _Dep_rec_ {
  cellpo name;			/* program in the list */
  cellpo type;			/* Type variable */
  cellpo body;			/* Body of this program */
  int marker;			/* =-1 not yet visited, 0 when done, >1 being*/
  depPo next;			/* Next in the list */
} DependRec;

typedef struct _Dep_stack_ {
  depPo stack[1024];		/* Stack of entries */
  int top;
} DepStack;

#endif



/*
   Public declarations for the dictionary functions for April
*/
#ifndef _DICT_H_
#define _DICT_H_

typedef struct _dict_ *dictpo;	/* Variable dictionary */
typedef long dictTrail;		/* Opaque pointer */

/* type of entry in dictionary */
typedef enum {cmark,cvar,cfun,cproc,cptn,ccons,ctype,cunknown} centry;

/* source type of a variable */
typedef enum {localvar,envvar,indexvar,thetavar} vartype; 

/* whether a variable is inited or not */
typedef enum {notInited,Inited} initStat;

/* Whether a variable is read-write or single assignment */
typedef enum {singleAss,multiAss} assMode;

void init_compiler_dict(void);

dictpo declVar(cellpo name,cellpo type,vartype vt,
	       assMode rw,initStat inited,logical reported,
	       int offset,int indx,dictpo dct);

/* Extract an L-value and extend the dictionary as necessary */
dictpo ExtractLVal(cellpo lhs,int depth,int *dp,cellpo *vr,int *index,int *off,
		   vartype *vt,cellpo *it,dictpo dict,cpo code);

/* Set a scope mark */
dictpo addMarker(dictpo dct);

/* ignore most recent markers */
dictpo stripMarks(dictpo dct);

/* Pick out an outer scope */
dictpo outerDict(dictpo dict);

/* Determine lexical depth */
int countMarkers(dictpo d);

/* Clear down a dictionary down to the mark u */
dictpo clearDict(dictpo dict, dictpo u);

/* Slice out a portion of the dictionary ... */
dictpo chopDict(dictpo top, dictpo chop, dictpo base);

/* Return a marker which represents variables currently initialized */
dictTrail currentTrail(dictpo dict);

/* Uninitialize all variables bound more recently than given marker */
void resetDict(dictTrail trail,dictpo dict);

void InitVar(cellpo v,logical inited,dictpo dict);
logical inittedVar(cellpo v,dictpo dict);

logical IsFunVar(cellpo fn,int *r,cellpo *ft,cellpo *at,int *off,int *index,
		 vartype *vt,dictpo dict);

logical IsProcVar(cellpo fn,int *r,cellpo *at,int *off,int *index,vartype *vt,
		  dictpo dict);

logical IsVar(cellpo v,centry *et,cellpo *t,int *off,int *index,
	      assMode *rw,initStat *inited,vartype *vt,dictpo dict);

logical IsLocalVar(cellpo v,dictpo dict);

logical IsType(cellpo tp,cellpo *head,cellpo *body,dictpo dict);

logical IsRecursiveType(cellpo type,dictpo dict);

cellpo dictName(dictpo entry);
assMode dictRW(dictpo entry,cellpo name);
centry dictCe(dictpo entry,cellpo name);
vartype dictVt(dictpo entry,cellpo name);
int dictOffset(dictpo entry,cellpo name);
int dictIndex(dictpo entry,cellpo name);
cellpo dictType(dictpo entry,cellpo name);
initStat dictInited(dictpo entry,cellpo name);
logical dictReported(dictpo entry,cellpo name);
void setDictReported(dictpo entry,cellpo name,logical reported);
void ReportVar(cellpo v,logical reported,dictpo dict);

typedef void (*dictProc)(dictpo entry,void *cl);
			
void processDict(dictpo top,dictpo bottom,dictProc proc,void *cl);
void processRevDict(dictpo top,dictpo bottom,dictProc proc,void *cl);

void processDictTrail(dictpo top,dictpo bottom,dictTrail trail,dictProc proc,void *cl);

void dump_dict(dictpo d, char * msg,long depth);
void dumpDict(dictpo d);

#endif

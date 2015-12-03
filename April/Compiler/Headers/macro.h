/*
  Definitions for the macro-processor
 */
#ifndef _MACRO_H_
#define _MACRO_H_

#define MAX_MAC_VAR 255		/* Maximum no. of vars in one macro */

typedef struct mac_ins *mac_po;
typedef struct _mac_def_ *macdefpo;
typedef struct _macro_list_ *macPo;

void init_macro(void);		/* initialize the macro processor */
macPo macroList(void);		/* construct a new macro list */

/* clear down the macros for the next source file */
macdefpo clearMacros(macdefpo mac_top);

/* invoke macro-processor */
cellpo macro_replace(cellpo,optionPo opts);

macdefpo define_macro(cellpo,cellpo,macdefpo);	/* define a new macro */
extern macdefpo mac_top;	/* current top macro */

#endif

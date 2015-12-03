#ifndef _FOLD_EXP_H_
#define _FOLD_EXP_H_

/* Interface for expression and statement folding */
cellpo foldExp(cellpo input,cellpo output,dictpo dict);
logical IsGrnd(cellpo input, dictpo dict);

logical SimpleLiteral(cellpo input,dictpo dict);
#endif

/*
 * Abstract syntax structure accessing and building functions
 */
#ifndef _STRUCTURE_H_
#define _STRUCTURE_H_

/* Extract the arguments of a function call */
unsigned long argArity(cellpo type);

cellpo tupleOfArgs(cellpo input,cellpo tgt);
cellpo tupleIze(cellpo input);
cellpo BuildStruct(cellpo fn,unsigned long ar,cellpo tgt);

logical isCall(cellpo input, symbpo fn,unsigned long *ar);
logical IsStruct(cellpo input, cellpo *fn,unsigned long *ar);
cellpo callArg(cellpo input,unsigned long n);

logical IsTernaryCall(cellpo input,cellpo *f,cellpo *a1,cellpo *a2,cellpo *a3);
logical IsTernaryStruct(cellpo input,symbpo f,cellpo *a1,cellpo *a2,cellpo *a3);

logical IsBinaryStruct(cellpo input,cellpo *f,cellpo *a1,cellpo *a2);
logical isBinaryCall(cellpo input,symbpo fn, cellpo *a1,cellpo *a2);
cellpo BuildBinCall(symbpo fn,cellpo a1,cellpo a2,cellpo tgt);
cellpo BuildBinStruct(cellpo fn,cellpo a1,cellpo a2,cellpo tgt);

logical isUnaryCall(cellpo input,symbpo fn,cellpo *a1);
logical IsUnaryStruct(cellpo input, cellpo *fn, cellpo *a1);
cellpo BuildUnaryCall(symbpo fn,cellpo a1,cellpo tgt);
cellpo BuildUnaryStruct(cellpo fn,cellpo a1,cellpo tgt);

logical IsZeroStruct(cellpo t,symbpo fn);
cellpo BuildZeroStruct(cellpo fn,cellpo tgt);

logical IsAnyStruct(cellpo input,cellpo *a1);
cellpo BuildAnyStruct(cellpo a1,cellpo tgt);

logical IsEnumStruct(cellpo input,cellpo *a1);
cellpo BuildEnumStruct(cellpo a1,cellpo tgt);

logical isCodeClosure(cellpo input);

cellpo BuildHashStruct(symbpo fn,cellpo a1,cellpo tgt);

cellpo BuildFunType(cellpo lhs,cellpo rhs,cellpo tgt);
cellpo BuildProcType(unsigned long ar,cellpo tgt);
cellpo BuildListType(cellpo lhs,cellpo tgt);
cellpo BuildForAll(cellpo lhs,cellpo rhs,cellpo tgt);

logical IsTheta(cellpo input,cellpo *body);
cellpo BuildTheta(cellpo input,cellpo tgt);

logical IsVoid(cellpo input);
logical IsSymbol(cellpo in,symbpo sym);

logical IsQuote(cellpo input, cellpo *a1);
logical IsQuery(cellpo input,cellpo *lhs,cellpo *rhs);
cellpo BuildQuery(cellpo lhs,cellpo rhs,cellpo tgt);
logical IsForAll(cellpo input,cellpo *lhs,cellpo *rhs);
logical IsHashStruct(cellpo input, symbpo fn, cellpo *a1);
logical IsTuple(cellpo input,unsigned long *arity);
cellpo BuildTuple(unsigned long arity,cellpo tgt);
logical IsLambda(cellpo input,cellpo *args,cellpo *exp);
logical IsMu(cellpo input,cellpo *args,cellpo *body);
logical IsMacroDef(cellpo input,cellpo *ptn,cellpo *rep);
logical IsShriek(cellpo input,cellpo *exp);
logical IsTvar(cellpo input,cellpo *name);

cellpo BuildForAll(cellpo lhs,cellpo rhs,cellpo tgt);

logical IsLabelledRecord(cellpo input,cellpo *label,cellpo *body);
logical emptyLabelledRecord(cellpo input);
cellpo BuildLabelledRecord(cellpo label,cellpo body,cellpo tgt);
long labelledRecordLength(cellpo input);
logical IsHandleRecord(cellpo input);

cellpo JoinTuples(cellpo a,cellpo b,cellpo tgt);
cellpo Tuple2List(cellpo input,cellpo tgt);
cellpo List2Tuple(cellpo input,cellpo tgt);

logical sameCell(cellpo c1,cellpo c2);

cellpo stripCell(cellpo input);
logical IsProcLiteral(cellpo input);
logical IsFunLiteral(cellpo input);

#endif


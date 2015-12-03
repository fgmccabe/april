/*
 * header giving interface to polymorphic type checker 
 */
#ifndef _TYPES_H_
#define _TYPES_H_

typedef struct _type_env_ *envPo; /* Type dictionary */
typedef struct _non_gen_ *lstPo; /* list pointer */
typedef struct _Dep_rec_ *depPo; /* dependency pointer structure */
typedef struct _Dep_stack_ *depStackPo;

void initChecker(void);
envPo resetChecker(void);

cellpo NewTypeVar(cellpo tgt);
cellpo doubleTypeVar(void);
cellpo lookUpBinding(cellpo t);

cellpo whichType(cellpo exp,envPo *env,envPo outer,cellpo tgt,logical H);
cellpo recordType(cellpo exp,envPo env,cellpo tgt);
envPo defType(cellpo type,envPo env);
envPo decExtCons(cellpo def,cellpo lbl,envPo env);

symbpo typeHash(cellpo name,cellpo type);
void generalizeType(cellpo type,envPo env,envPo outer);
cellpo generalizeTypeExp(cellpo t,envPo env,cellpo tgt);
cellpo typeSkel(cellpo name,envPo *env);
cellpo typeFunSkel(cellpo name,envPo *env);
logical occInType(cellpo name,cellpo value);
cellpo typeIs(cellpo ptn,cellpo type);

cellpo freshType(cellpo s,envPo env,cellpo tgt);
cellpo freshenType(cellpo t,envPo env,cellpo tgt,cellpo *vars,long *count);
logical realType(cellpo exp,envPo env,cellpo tgt);
logical typeExpression(cellpo exp,envPo env,envPo *tvars,cellpo tgt);
cellpo generalize(cellpo t,envPo env,cellpo tgt);

cellpo elementType(cellpo type);

logical IsEnumeration(cellpo fn,envPo env,cellpo *type);
logical IsConstructor(cellpo fn,envPo env,cellpo *type);
logical isFunType(cellpo type,cellpo *at,cellpo *rt);
logical isProcType(cellpo type,unsigned long *ar);
logical isConstructorType(cellpo type,cellpo *lhs,cellpo *rhs);
logical isListType(cellpo type,cellpo *el);
unsigned long arityOfType(cellpo type);

logical IsRecordType(cellpo input);

extern envPo standardTypes;

envPo newEnv(cellpo name,cellpo value,envPo env);
void rebindEnv(cellpo name,cellpo type,envPo env);
envPo markEnv(envPo env);
lstPo extendNonG(cellpo t,lstPo nonG);
void popEnv(envPo top,envPo last);
lstPo newLst(cellpo t,lstPo x);
void popLst(lstPo top,lstPo last);
logical isOnLst(cellpo t,lstPo lst);

typedef logical (*envFilter)(cellpo name,cellpo val,void *cl);

cellpo getType(cellpo name,envPo env,envFilter fil,void *cl);
cellpo getTypeDef(cellpo s,envPo env);
cellpo getTypeFunDef(cellpo s,envPo env);
cellpo getTypeOf(cellpo s,envPo env);

logical knownType(cellpo s,envPo env,cellpo tgt);
logical knownTypeName(cellpo s,envPo env);
cellpo pickupType(cellpo s,envPo env);
cellpo pickupTypeDef(cellpo s,envPo env);
cellpo pickupTypeRename(cellpo s,envPo env);
logical bindType(cellpo var,cellpo type);
cellpo lookUpBinding(cellpo t);
envPo extendRecordEnv(cellpo exp,cellpo type,envPo env);
cellpo setTypeInfo(cellpo exp,cellpo type);

cellpo pttrnType(cellpo ptn,logical islhs,envPo *env,envPo outer,
		 cellpo tgt,logical inH);

logical checkStmt(cellpo stmt,envPo env,envPo outer,cellpo tgt,logical inH);
cellpo thetaType(cellpo body,envPo env,cellpo tgt,logical inH);
logical testType(cellpo input,logical islhs,envPo *env,envPo outer);

envPo analyseDependencies(cellpo body,envPo env,depStackPo defs,
			  depStackPo groups);
void clearDeps(depPo dep);

typedef struct _bound_var_ *bPo;

typedef struct _bound_var_ {
  cellpo var;
  cellpo binding;
  bPo prev;
} BoundVarRec;			/* used in tracing access to bound variables */

logical isBoundVar(cellpo var,cellpo *bind,bPo list);
logical sameType(cellpo t1,cellpo t2);
logical unifyType(cellpo t1,cellpo t2,tracePo lscope,tracePo rscope,
		  logical inForall,cellpo *f1,cellpo *f2);
logical checkType(cellpo t1,cellpo t2,cellpo *f1,cellpo *f2);


#endif

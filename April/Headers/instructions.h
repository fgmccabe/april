/*
 * Specification of the April Machine Instruction set
  (c) 1994-2000 Imperial College and F.G.McCabe

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
*/

/* 4/17/2000 Added instructions to handle apply pairs, cleaned up opcodes */
/* 12/20/1999 Modified instruction templates with explicit operand types */
/* 12/17/1999 Added instruction templates for dynamic variables */
/* These are used to provide a safe type for instruction templates */

instruction(halt,0,"","T\0") /* stop execution */

/* Variable move instructions */
instruction(movl,1,"th","T\2$\0N") /* Move a literal value into variable */
instruction(move,2,"ml","T\2NN") /* Move variable */
instruction(emove,3,"ml","T\2NN") /* Move variable from environment */
instruction(stoe,4,"l","N") /* Store environment in local */
instruction(loade,5,"l","N") /* Load environment from local */

instruction(jmp,6,"B","s")  /* jump */
instruction(ijmp,7,"ho","T\2Ns")  /* indirect jump */
instruction(hjmp,8,"ho","T\2Ns") /* symbol hash table jump */
instruction(cjmp,9,"ho","T\2Ns")        /* Character hash table jump */
instruction(tjmp,10,"ho","T\2Ns") /* tag case jump */
instruction(esc_fun,11,"ho","T\2Ns") /* service functions */
instruction(call,12,"hml","T\3NNN") /* call procedure  */
instruction(ecall,13,"hml","T\3NNN") /* environment procedure  */
  
instruction(ret,15,"","T\0")  /* Return from procedure */
instruction(result,16,"ml","T\2NN") /* return from function */
instruction(allocv,17,"o","N") /* Establish a new frame pointer */
instruction(gc,18,"ho","T\2NN") /* reserve space, and invoke GC */

instruction(initv,19,"l","N") /* Initialize a variable */

/* pattern matching instructions */
instruction(mlit,20,"ht","T\2N$\1") /* Match a literal */
instruction(mfloat,21,"ht","T\2NN") /* Match a floating-point */
instruction(mnil,22,"hb","T\2Ns")  /* Match empty list */
instruction(mlist,23,"hml","T\3NNN") /* Match non-empty list */
instruction(mcons,24,"ho","T\2NN") /* Match constructor */
instruction(mtpl,25,"ho","T\2NN") /* Match an n-tuple */
instruction(mhdl,26,"ht","T\2Nh") /* Match a literal handle */
instruction(mchar,27,"ho","T\2Nc")      /* Match literal character */
instruction(many,28,"hml","T\3NNN")/* Check an ANY value */

instruction(ivar,29,"hl","T\2NN")       // Create a new type variable
instruction(isvr,30,"hb","T\2Ns")       // Test for type variable
instruction(uvar,31,"ml","T\2NN")       // Unify subsequent occ of type variable
instruction(ulit,32,"ht","T\2Ns")       // Unify against type literal value
instruction(urst,33,"h","T\1N")         // reset unification tables
instruction(undo,34,"h","T\1N")         // undo recent type unification
instruction(utpl,35,"ho","T\2NN")       // Unify type against a tuple
instruction(ucns,36,"ho","T\2NN")       // Unify against type constructor

/* list matching instructions */

instruction(prefix,44,"hml","T\3NNN")  /* List prefix */
instruction(snip,45,"hml","T\3NNN")  /* Snip count prefix off list */

/*Constructor manipulation instructions */
instruction(loc2cns,46,"hml","T\3NNN") /* create a constructor from locals */
instruction(consfld,47,"hml","T\3NNN")  /* access constructor fld */
instruction(mcnsfun,48,"ht","T\2Ns")       // Match the constructor of a constructor
instruction(conscns,49,"ml","T\2NN")    // Access constructor symbol
instruction(cnupdte,50,"hml","T\3NNN")  /* update a tuple */

/* Tuple manipulation instructions */
instruction(loc2tpl,54,"hml","T\3NNN") /* create a tuple from locals */
instruction(indxfld,55,"hml","T\3NNN")  /* access fld */
instruction(tpupdte,56,"hml","T\3NNN")  /* update a tuple */

/* List manipulation instructions */
instruction(lstpr,60,"hml","T\3NNN") /* Construct a list pair */
instruction(ulst,61,"hml","T\3NNN") /* Unpack a list pair */
instruction(nthel,62,"hml","T\3NNN") /* Extract the nth element of list */

  /* Set construction instructions */
instruction(add2lst,63,"hml","T\3NNN") /* Add new element to end of list */
instruction(lsupdte,64,"hml","T\3NNN")  // Update collect list

instruction(loc2any,66,"hml","T\3NNN")

/* Error block handling instructions */
instruction(errblk,67,"hb","T\2Ns") /* start a new error block */
instruction(errend,68,"l","N")	/* end an error block */
instruction(generr,69,"l","N") /* generate an error explicitly */
instruction(moverr,70,"l","N") /* pick up the current error value */

/* Predicate testing instructions */

/* General comparison instructions */
instruction(eq,75,"ml","T\2NN")  /* equality */
instruction(neq,76,"ml","T\2NN")  /* inequality */
instruction(gt,77,"ml","T\2NN")  /* gt than */
instruction(le,78,"ml","T\2NN")  /* less than or equal */

/* Pointer comparison instructions */
instruction(eqq,79,"ml","T\2NN")  /* pointer equality */
instruction(neqq,80,"ml","T\2NN")  /* pointer inequality */

/* Numeric comparison instructions */
instruction(feq,81,"ml","T\2NN")  /* equality */
instruction(fneq,82,"ml","T\2NN")  /* inequality */
instruction(fgt,83,"ml","T\2NN")  /* gt than */
instruction(fle,84,"ml","T\2NN")  /* less than or equal */

instruction(ieq,85,"ml","T\2NN")  /* equality */
instruction(ineq,86,"ml","T\2NN")  /* inequality */
instruction(igt,87,"ml","T\2NN")  /* gt than */
instruction(ile,88,"ml","T\2NN")  /* less than or equal */

instruction(iftrue,89,"hb","T\2Ns")	/* test for trueval */
instruction(iffalse,90,"hb","T\2Ns") /* test for not trueval */

/* Arithmetic expression instructions */
instruction(plus,91,"hml","T\3NNN")  /* addition */
instruction(minus,92,"hml","T\3NNN")  /* subtraction */
instruction(times,93,"hml","T\3NNN")  /* multiply */
instruction(divide,94,"hml","T\3NNN")  /* divide */
instruction(incr,95,"hml","T\3NNN") /* increment */

/* Special instructions */

instruction(snd,96,"hml","T\3NhA") /* message send */
instruction(self,97,"l","N") /* id of current process */

instruction(die,98,"","T\0") /* kill off current sub-process */

instruction(use,99,"ml","T\2NN")	/* use a new click counter */

instruction(line_d,100,"hyo","T\3NSN") /* report debugging info */
instruction(entry_d,101,"ht","T\2NS") /* report entry into code */
instruction(exit_d,102,"ht","T\2NS") /* report exit from procedure */
instruction(assign_d,103,"hml","T\3NNN") /* assign variable */
instruction(return_d,104,"hml","T\3NNN") /* return from function */
instruction(accept_d,105,"hml","T\3NAN") /* accept message */
instruction(die_d,106,"l","N") /* process dies */
instruction(send_d,107,"hml","T\3NNN") /* send a message */
instruction(fork_d,108,"ml","T\2NN") /* fork process */
instruction(scope_d,110,"ml","T\2NN") /* set variable scope */
instruction(suspend_d,111,"l","N") /* suspend process */
instruction(error_d,112,"ml","T\2NN") /* report error */
instruction(debug_d,113,"b","T\1s") /* are we debugging? */



/* Access teh opcodes of the April machine
 * (c) 1994-1997 Imperial College and F.G.McCabe
 * all rights reserved
 *
 * $Id: opcodes.h,v 1.2 2003/05/07 18:30:10 fmccabe Exp $
 * $Log: opcodes.h,v $
 * Revision 1.2  2003/05/07 18:30:10  fmccabe
 * My version
 *
 * Revision 1.2  2000/06/22 15:51:06  fmccabe
 * A few more attempts to allow code to be larger than 32K. Mostly done now
 *
 * Revision 1.1.1.1  2000/05/04 20:44:40  fmccabe
 * Initial import of April in sourceforge's CVS
 *
 *
 * Revision 1.6  1996/07/31 05:42:39  fgm
 * Removed delay and sleep instructions
 *
 * Revision 1.5  1996/06/07 08:55:54  fgm
 * Added new instruction to update lists
 **/
#ifndef _OPCODES_H_
#define _OPCODES_H_

/* All these assume a 32-bit word; anything else must be re-engineered */

#define op_mask  0x000000ff	/* Mask for the opcode of an instruction */
#define vl_mask  0xffffff00	/* Mask for the value field */
#define vl_l_mask 0x0000ff00	/* Low byte for the value field */
#define vl_m_mask 0x00ff0000	/* Middle byte for the value field */
#define vl_h_mask 0xff000000	/* High byte for the value field */
#define vl_o_mask 0x00ffff00	/* Label offset operand */
#define vl_O_mask 0xffffff00	/* Long label offset operand */
#define op_cde(c) ((opCode)(c&op_mask))	/* construct an opcode from a number */

#define op_l_val(v) ((unsigned)(((v)&vl_l_mask)>>8)) /* Get low-order operand */
#define op_sl_val(v) ((WORD32)((v)<<16>>24)) /* Get signed low-order operand */
#define op_m_val(v) ((unsigned)((v)&vl_m_mask)>>16) /* Get middle order operand */
#define op_sm_val(v) ((WORD32)((v)<<8>>24)) /* Get signed middle operand */
#define op_o_val(v) (unsigned)(((v)&vl_o_mask)>>8) /* Get offset operand */
#define op_so_val(v) ((WORD32)((v)<<8>>16)) /* Get signed low/middle */
#define op_lo_val(v) ((WORD32)((v)>>8)) /* get signed long offset */
#define op_h_val(v) ((unsigned)((v)>>24)) /* Get hi-order operand */
#define op_sh_val(v) ((WORD32)((v)>>24)) /* Get signed hi-order operand */


#undef instruction
#define instruction(mnem,op,sig,tp) mnem=op,

typedef enum {
#include "instructions.h"	/* Pick up the instructions specification */
  illegalOp } opCode;

#undef instruction
#endif


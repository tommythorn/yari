/* src/vm/jit/m68k/linux/md-abi.h - defines for m68k Linux ABI

   Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
   C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
   E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
   J. Wenninger, Institut f. Computersprachen - TU Wien

   This file is part of CACAO.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Contact: cacao@cacaojvm.org

   Authors: Roland Lezuo

   Changes:

   $Id: md-abi.h 5940 2006-11-09 09:59:28Z tbfg $

*/


#ifndef _MD_ABI_H
#define _MD_ABI_H

/* preallocated registers *****************************************************/

/* integer registers */

#define REG_RESULT       0   /* to deliver method results                     */
#define REG_RESULT2      1   /* to deliver long method results                */

#define REG_D0		 0   /* register may be trashed by callee */
#define REG_D1		 1



#define REG_ITMP1        2   /* temporary register                            */
#define REG_ITMP2        3   /* temporary register and method pointer         */
#define REG_ITMP3        4   /* temporary register                            */



/* floating point registers */

#define REG_F0		 0   /* registers may be trashed by callee */
#define REG_F1		 1

#define REG_FRESULT      0   /* to deliver floating point method results      */
#define REG_FTMP1        2   /* temporary floating point register             */
#define REG_FTMP2        3   /* temporary floating point register             */
#define REG_FTMP3        4   /* temporary floating point register             */

#define REG_IFTMP        2   /* temporary integer and floating point register */


/* address registers */

#define REG_A0		 0   /* registers may be trashed by callee */
#define REG_A1		 1

#define REG_ATMP1        2
#define REG_ATMP2        3
#define REG_METHODPTR    REG_ATMP2   /* pointer to the place from where the procedure */
#define REG_ATMP3        4

#define REG_ATMP1_XPTR   REG_ATMP1   /* exception pointer = temporary register 1      */
#define REG_ATMP2_XPC    REG_ATMP2   /* exception pc = temporary register 2           */

#define	REG_FP		 6   /* frame pointer                                 */
#define REG_SP           7   /* stack pointer                                 */

/* number for the register allocator */

#define INT_REG_CNT      8   /* number of integer registers                   */
#define INT_SAV_CNT      3   /* number of int callee saved registers          */
#define INT_ARG_CNT      0   /* number of int argument registers              */
#define INT_TMP_CNT      2   /* number of integer temporary registers         */
#define INT_RES_CNT      3   /* number of integer reserved registers          */

#if !defined(ENABLE_SOFTFLOAT)
	#define FLT_REG_CNT      8   /* number of float registers                     */
	#define FLT_SAV_CNT      3   /* number of float callee saved registers        */
	#define FLT_ARG_CNT      0   /* number of float argument registers            */
	#define FLT_TMP_CNT      2   /* number of float temporary registers           */
	#define FLT_RES_CNT      3   /* number of float reserved registers            */
#else
	#define FLT_REG_CNT      8   /* number of float registers                     */
	#define FLT_SAV_CNT      0   /* number of float callee saved registers        */
	#define FLT_ARG_CNT      0   /* number of float argument registers            */
	#define FLT_TMP_CNT      0   /* number of float temporary registers           */
	#define FLT_RES_CNT      8   /* number of float reserved registers            */
#endif

#define ADR_REG_CNT      8
#define ADR_SAV_CNT      1
#define ADR_ARG_CNT      0
#define ADR_TMP_CNT      2
#define ADR_RES_CNT      5


/* packed register defines ***************************************************/

#define REG_RESULT_PACKED	PACK_REGS(REG_RESULT2, REG_RESULT)
#define REG_ITMP12_PACKED	PACK_REGS(REG_ITMP1, REG_ITMP2)
#define REG_ITMP23_PACKED	PACK_REGS(REG_ITMP2, REG_ITMP3)

/* ABI defines ****************************************************************/

/* TODO */

#define LA_SIZE         48   /* linkage area size                             */
#define LA_SIZE_ALIGNED 16   /* linkage area size aligned to 16-byte          */
#define LA_SIZE_IN_POINTERS	(LA_SIZE / SIZEOF_VOID_P)
#define LA_LR_OFFSET    16   /* link register offset in linkage area          */
#define PA_SIZE		(PA_SIZE_IN_POINTERS*8)	
#define PA_SIZE_IN_POINTERS	8 /* linux/ppc64 has a minimun parameter save area size, XXX:darwin? */

/* #define ALIGN_FRAME_SIZE(sp)       (sp) */

#endif /* _MD_ABI_H */


/*
 * These are local overrides for various environment variables in Emacs.
 * Please do not remove this and leave it at the end of the file, where
 * Emacs will automagically detect them.
 * ---------------------------------------------------------------------
 * Local variables:
 * mode: c
 * indent-tabs-mode: t
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 */

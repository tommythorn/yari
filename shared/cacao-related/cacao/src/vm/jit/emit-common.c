/* src/vm/jit/emit-common.c - common code emitter functions

   Copyright (C) 2006, 2007 R. Grafl, A. Krall, C. Kruegel, C. Oates,
   R. Obermaisser, M. Platter, M. Probst, S. Ring, E. Steiner,
   C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich, J. Wenninger,
   Institut f. Computersprachen - TU Wien

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

   $Id: emitfuncs.c 4398 2006-01-31 23:43:08Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "arch.h"
#include "codegen.h"

#include "vm/jit/emit-common.h"
#include "vm/jit/jit.h"

#include "vmcore/options.h"
#include "vmcore/statistics.h"


/* emit_load_s1 ****************************************************************

   Emits a possible load of the first source operand.

*******************************************************************************/

s4 emit_load_s1(jitdata *jd, instruction *iptr, s4 tempreg)
{
	varinfo *src;
	s4       reg;

	src = VAROP(iptr->s1);

	reg = emit_load(jd, iptr, src, tempreg);

	return reg;
}


/* emit_load_s2 ****************************************************************

   Emits a possible load of the second source operand.

*******************************************************************************/

s4 emit_load_s2(jitdata *jd, instruction *iptr, s4 tempreg)
{
	varinfo *src;
	s4       reg;

	src = VAROP(iptr->sx.s23.s2);

	reg = emit_load(jd, iptr, src, tempreg);

	return reg;
}


/* emit_load_s3 ****************************************************************

   Emits a possible load of the third source operand.

*******************************************************************************/

s4 emit_load_s3(jitdata *jd, instruction *iptr, s4 tempreg)
{
	varinfo *src;
	s4       reg;

	src = VAROP(iptr->sx.s23.s3);

	reg = emit_load(jd, iptr, src, tempreg);

	return reg;
}


/* emit_load_s1_low ************************************************************

   Emits a possible load of the low 32-bits of the first long source
   operand.

*******************************************************************************/

#if SIZEOF_VOID_P == 4
s4 emit_load_s1_low(jitdata *jd, instruction *iptr, s4 tempreg)
{
	varinfo *src;
	s4       reg;

	src = VAROP(iptr->s1);

	reg = emit_load_low(jd, iptr, src, tempreg);

	return reg;
}
#endif


/* emit_load_s2_low ************************************************************

   Emits a possible load of the low 32-bits of the second long source
   operand.

*******************************************************************************/

#if SIZEOF_VOID_P == 4
s4 emit_load_s2_low(jitdata *jd, instruction *iptr, s4 tempreg)
{
	varinfo *src;
	s4       reg;

	src = VAROP(iptr->sx.s23.s2);

	reg = emit_load_low(jd, iptr, src, tempreg);

	return reg;
}
#endif


/* emit_load_s3_low ************************************************************

   Emits a possible load of the low 32-bits of the third long source
   operand.

*******************************************************************************/

#if SIZEOF_VOID_P == 4
s4 emit_load_s3_low(jitdata *jd, instruction *iptr, s4 tempreg)
{
	varinfo *src;
	s4       reg;

	src = VAROP(iptr->sx.s23.s3);

	reg = emit_load_low(jd, iptr, src, tempreg);

	return reg;
}
#endif


/* emit_load_s1_high ***********************************************************

   Emits a possible load of the high 32-bits of the first long source
   operand.

*******************************************************************************/

#if SIZEOF_VOID_P == 4
s4 emit_load_s1_high(jitdata *jd, instruction *iptr, s4 tempreg)
{
	varinfo *src;
	s4       reg;

	src = VAROP(iptr->s1);

	reg = emit_load_high(jd, iptr, src, tempreg);

	return reg;
}
#endif


/* emit_load_s2_high ***********************************************************

   Emits a possible load of the high 32-bits of the second long source
   operand.

*******************************************************************************/

#if SIZEOF_VOID_P == 4
s4 emit_load_s2_high(jitdata *jd, instruction *iptr, s4 tempreg)
{
	varinfo *src;
	s4       reg;

	src = VAROP(iptr->sx.s23.s2);

	reg = emit_load_high(jd, iptr, src, tempreg);

	return reg;
}
#endif


/* emit_load_s3_high ***********************************************************

   Emits a possible load of the high 32-bits of the third long source
   operand.

*******************************************************************************/

#if SIZEOF_VOID_P == 4
s4 emit_load_s3_high(jitdata *jd, instruction *iptr, s4 tempreg)
{
	varinfo *src;
	s4       reg;

	src = VAROP(iptr->sx.s23.s3);

	reg = emit_load_high(jd, iptr, src, tempreg);

	return reg;
}
#endif


/* emit_store_dst **************************************************************

   This function generates the code to store the result of an
   operation back into a spilled pseudo-variable.  If the
   pseudo-variable has not been spilled in the first place, this
   function will generate nothing.
    
*******************************************************************************/

void emit_store_dst(jitdata *jd, instruction *iptr, s4 d)
{
	emit_store(jd, iptr, VAROP(iptr->dst), d);
}


/* emit_bccz *******************************************************************

   Emit conditional and unconditional branch instructions on integer
   regiseters.

*******************************************************************************/

void emit_bccz(codegendata *cd, basicblock *target, s4 condition, s4 reg, u4 options)
{
	s4 branchmpc;
	s4 disp;

	/* Target basic block already has an PC, so we can generate the
	   branch immediately. */

	if ((target->mpc >= 0)) {
		STATISTICS(count_branches_resolved++);

		/* calculate the mpc of the branch instruction */

		branchmpc = cd->mcodeptr - cd->mcodebase;
		disp      = target->mpc - branchmpc;

		emit_branch(cd, disp, condition, reg, options);
	}
	else {
		/* current mcodeptr is the correct position,
		   afterwards emit the NOPs */

		codegen_add_branch_ref(cd, target, condition, reg, options);

		/* generate NOPs as placeholder for branch code */

		BRANCH_NOPS;
	}
}


/* emit_bcc ********************************************************************

   Emit conditional and unconditional branch instructions on condition
   codes.

*******************************************************************************/

void emit_bcc(codegendata *cd, basicblock *target, s4 condition, u4 options)
{
	emit_bccz(cd, target, condition, -1, options);
}


/* emit_br *********************************************************************

   Wrapper for unconditional branches.

*******************************************************************************/

void emit_br(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_UNCONDITIONAL, BRANCH_OPT_NONE);
}


/* emit_bxxz *******************************************************************

   Wrappers for branches on one integer register.

*******************************************************************************/

#if SUPPORT_BRANCH_CONDITIONAL_ONE_INTEGER_REGISTER

void emit_beqz(codegendata *cd, basicblock *target, s4 reg)
{
	emit_bccz(cd, target, BRANCH_EQ, reg, BRANCH_OPT_NONE);
}

void emit_bnez(codegendata *cd, basicblock *target, s4 reg)
{
	emit_bccz(cd, target, BRANCH_NE, reg, BRANCH_OPT_NONE);
}

void emit_bltz(codegendata *cd, basicblock *target, s4 reg)
{
	emit_bccz(cd, target, BRANCH_LT, reg, BRANCH_OPT_NONE);
}

void emit_bgez(codegendata *cd, basicblock *target, s4 reg)
{
	emit_bccz(cd, target, BRANCH_GE, reg, BRANCH_OPT_NONE);
}

void emit_bgtz(codegendata *cd, basicblock *target, s4 reg)
{
	emit_bccz(cd, target, BRANCH_GT, reg, BRANCH_OPT_NONE);
}

void emit_blez(codegendata *cd, basicblock *target, s4 reg)
{
	emit_bccz(cd, target, BRANCH_LE, reg, BRANCH_OPT_NONE);
}

#endif /* SUPPORT_BRANCH_CONDITIONAL_ONE_INTEGER_REGISTER */


/* emit_bxx ********************************************************************

   Wrappers for branches on two integer registers.

   We use PACK_REGS here, so we don't have to change the branchref
   data structure and the emit_bccz function.

*******************************************************************************/

#if SUPPORT_BRANCH_CONDITIONAL_TWO_INTEGER_REGISTERS

void emit_beq(codegendata *cd, basicblock *target, s4 s1, s4 s2)
{
	emit_bccz(cd, target, BRANCH_EQ, PACK_REGS(s1, s2), BRANCH_OPT_NONE);
}

void emit_bne(codegendata *cd, basicblock *target, s4 s1, s4 s2)
{
	emit_bccz(cd, target, BRANCH_NE, PACK_REGS(s1, s2), BRANCH_OPT_NONE);
}

#endif /* SUPPORT_BRANCH_CONDITIONAL_TWO_INTEGER_REGISTERS */


/* emit_bxx ********************************************************************

   Wrappers for branches on condition codes.

*******************************************************************************/

#if SUPPORT_BRANCH_CONDITIONAL_CONDITION_REGISTER

void emit_beq(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_EQ, BRANCH_OPT_NONE);
}

void emit_bne(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_NE, BRANCH_OPT_NONE);
}

void emit_blt(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_LT, BRANCH_OPT_NONE);
}

void emit_bge(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_GE, BRANCH_OPT_NONE);
}

void emit_bgt(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_GT, BRANCH_OPT_NONE);
}

void emit_ble(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_LE, BRANCH_OPT_NONE);
}

#if SUPPORT_BRANCH_CONDITIONAL_UNSIGNED_CONDITIONS
void emit_bult(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_ULT, BRANCH_OPT_NONE);
}

void emit_bule(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_ULE, BRANCH_OPT_NONE);
}

void emit_buge(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_UGE, BRANCH_OPT_NONE);
}

void emit_bugt(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_UGT, BRANCH_OPT_NONE);
}
#endif

#if defined(__POWERPC__) || defined(__POWERPC64__)
void emit_bnan(codegendata *cd, basicblock *target)
{
	emit_bcc(cd, target, BRANCH_NAN, BRANCH_OPT_NONE);
}
#endif

#endif /* SUPPORT_BRANCH_CONDITIONAL_CONDITION_REGISTER */


/* emit_label_bccz *************************************************************

   Emit a branch to a label.  Possibly emit the branch, if it is a
   backward branch.

*******************************************************************************/

void emit_label_bccz(codegendata *cd, s4 label, s4 condition, s4 reg, u4 options)
{
	list_t             *list;
	branch_label_ref_t *br;
	s4                  mpc;
	s4                  disp;

	/* get the label list */

	list = cd->brancheslabel;

	/* search if the label is already in the list */

	for (br = list_first_unsynced(list); br != NULL;
		 br = list_next_unsynced(list, br)) {
		/* is this entry the correct label? */

		if (br->label == label)
			break;
	}

	/* a branch reference was found */

	if (br != NULL) {
		/* calculate the mpc of the branch instruction */

		mpc  = cd->mcodeptr - cd->mcodebase;
		disp = br->mpc - mpc;

		emit_branch(cd, disp, condition, reg, options);

		/* now remove the branch reference */

		list_remove_unsynced(list, br);
	}
	else {
		/* current mcodeptr is the correct position,
		   afterwards emit the NOPs */

		codegen_branch_label_add(cd, label, condition, reg, options);

		/* generate NOPs as placeholder for branch code */

		BRANCH_NOPS;
	}
}


/* emit_label ******************************************************************

   Emit a label for a branch.  Possibly emit the branch, if it is a
   forward branch.

*******************************************************************************/

void emit_label(codegendata *cd, s4 label)
{
	list_t             *list;
	branch_label_ref_t *br;
	s4                  mpc;
	s4                  disp;
	u1                 *mcodeptr;

	/* get the label list */

	list = cd->brancheslabel;

	/* search if the label is already in the list */

	for (br = list_first_unsynced(list); br != NULL;
		 br = list_next_unsynced(list, br)) {
		/* is this entry the correct label? */

		if (br->label == label)
			break;
	}

	/* a branch reference was found */

	if (br != NULL) {
		/* calculate the mpc of the branch instruction */

		mpc  = cd->mcodeptr - cd->mcodebase;
		disp = mpc - br->mpc;

		/* temporary set the mcodeptr */

		mcodeptr     = cd->mcodeptr;
		cd->mcodeptr = cd->mcodebase + br->mpc;

		emit_branch(cd, disp, br->condition, br->reg, br->options);

		/* restore mcodeptr */

		cd->mcodeptr = mcodeptr;

		/* now remove the branch reference */

		list_remove_unsynced(list, br);
	}
	else {
		/* add the label to the list (use invalid values for condition
		   and register) */

		codegen_branch_label_add(cd, label, -1, -1, BRANCH_OPT_NONE );
	}
}


/* emit_label_bcc **************************************************************

   Emit conditional and unconditional label-branch instructions on
   condition codes.

*******************************************************************************/

void emit_label_bcc(codegendata *cd, s4 label, s4 condition, u4 options)
{
	emit_label_bccz(cd, label, condition, -1, options);
}


/* emit_label_br ***************************************************************

   Wrapper for unconditional label-branches.

*******************************************************************************/

void emit_label_br(codegendata *cd, s4 label)
{
	emit_label_bcc(cd, label, BRANCH_UNCONDITIONAL, BRANCH_OPT_NONE);
}


/* emit_label_bxxz *************************************************************

   Wrappers for label-branches on one integer register.

*******************************************************************************/

#if SUPPORT_BRANCH_CONDITIONAL_ONE_INTEGER_REGISTER

void emit_label_beqz(codegendata *cd, s4 label, s4 reg)
{
	emit_label_bccz(cd, label, BRANCH_EQ, reg, BRANCH_OPT_NONE);
}

#endif /* SUPPORT_BRANCH_CONDITIONAL_ONE_INTEGER_REGISTER */


/* emit_label_bxx **************************************************************

   Wrappers for label-branches on condition codes.

*******************************************************************************/

#if SUPPORT_BRANCH_CONDITIONAL_CONDITION_REGISTER

void emit_label_beq(codegendata *cd, s4 label)
{
	emit_label_bcc(cd, label, BRANCH_EQ, BRANCH_OPT_NONE);
}

void emit_label_bne(codegendata *cd, s4 label)
{
	emit_label_bcc(cd, label, BRANCH_NE, BRANCH_OPT_NONE);
}

void emit_label_blt(codegendata *cd, s4 label)
{
	emit_label_bcc(cd, label, BRANCH_LT, BRANCH_OPT_NONE);
}

void emit_label_bge(codegendata *cd, s4 label)
{
	emit_label_bcc(cd, label, BRANCH_GE, BRANCH_OPT_NONE);
}

void emit_label_bgt(codegendata *cd, s4 label)
{
	emit_label_bcc(cd, label, BRANCH_GT, BRANCH_OPT_NONE);
}

void emit_label_ble(codegendata *cd, s4 label)
{
	emit_label_bcc(cd, label, BRANCH_LE, BRANCH_OPT_NONE);
}

#endif /* SUPPORT_BRANCH_CONDITIONAL_CONDITION_REGISTER */


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
 * vim:noexpandtab:sw=4:ts=4:
 */

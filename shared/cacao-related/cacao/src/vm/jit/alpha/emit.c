/* src/vm/jit/alpha/emit.c - Alpha code emitter functions

   Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
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

   $Id: emit.c 7821 2007-04-25 19:56:30Z twisti $

*/


#include "config.h"
#include "vm/types.h"

#include <assert.h>

#include "md-abi.h"

#include "vm/jit/alpha/codegen.h"

#include "mm/memory.h"

#include "threads/lock-common.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"

#include "vm/jit/abi.h"
#include "vm/jit/abi-asm.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/dseg.h"
#include "vm/jit/emit-common.h"
#include "vm/jit/jit.h"
#include "vm/jit/replace.h"

#include "vmcore/options.h"


/* emit_load *******************************************************************

   Emits a possible load of an operand.

*******************************************************************************/

s4 emit_load(jitdata *jd, instruction *iptr, varinfo *src, s4 tempreg)
{
	codegendata  *cd;
	s4            disp;
	s4            reg;

	/* get required compiler data */

	cd = jd->cd;

	if (IS_INMEMORY(src->flags)) {
		COUNT_SPILLS;

		disp = src->vv.regoff * 8;

		switch (src->type) {
		case TYPE_INT:
		case TYPE_LNG:
		case TYPE_ADR:
			M_LLD(tempreg, REG_SP, disp);
			break;
		case TYPE_FLT:
		case TYPE_DBL:
			M_DLD(tempreg, REG_SP, disp);
			break;
		default:
			vm_abort("emit_load: unknown type %d", src->type);
		}

		reg = tempreg;
	}
	else
		reg = src->vv.regoff;

	return reg;
}


/* emit_store ******************************************************************

   Emit a possible store for the given variable.

*******************************************************************************/

void emit_store(jitdata *jd, instruction *iptr, varinfo *dst, s4 d)
{
	codegendata  *cd;
	s4            disp;

	/* get required compiler data */

	cd = jd->cd;

	if (IS_INMEMORY(dst->flags)) {
		COUNT_SPILLS;

		disp = dst->vv.regoff * 8;

		switch (dst->type) {
		case TYPE_INT:
		case TYPE_LNG:
		case TYPE_ADR:
			M_LST(d, REG_SP, disp);
			break;
		case TYPE_FLT:
		case TYPE_DBL:
			M_DST(d, REG_SP, disp);
			break;
		default:
			vm_abort("emit_store: unknown type %d", dst->type);
		}
	}
}


/* emit_copy *******************************************************************

   Generates a register/memory to register/memory copy.

*******************************************************************************/

void emit_copy(jitdata *jd, instruction *iptr)
{
	codegendata *cd;
	varinfo     *src;
	varinfo     *dst;
	s4           s1, d;

	/* get required compiler data */

	cd = jd->cd;

	/* get source and destination variables */

	src = VAROP(iptr->s1);
	dst = VAROP(iptr->dst);

	if ((src->vv.regoff != dst->vv.regoff) ||
		((src->flags ^ dst->flags) & INMEMORY)) {

		if ((src->type == TYPE_RET) || (dst->type == TYPE_RET)) {
			/* emit nothing, as the value won't be used anyway */
			return;
		}

		/* If one of the variables resides in memory, we can eliminate
		   the register move from/to the temporary register with the
		   order of getting the destination register and the load. */

		if (IS_INMEMORY(src->flags)) {
			d  = codegen_reg_of_var(iptr->opc, dst, REG_IFTMP);
			s1 = emit_load(jd, iptr, src, d);
		}
		else {
			s1 = emit_load(jd, iptr, src, REG_IFTMP);
			d  = codegen_reg_of_var(iptr->opc, dst, s1);
		}

		if (s1 != d) {
			switch (src->type) {
			case TYPE_INT:
			case TYPE_LNG:
			case TYPE_ADR:
				M_MOV(s1, d);
				break;
			case TYPE_FLT:
			case TYPE_DBL:
				M_FMOV(s1, d);
				break;
			default:
				vm_abort("emit_copy: unknown type %d", src->type);
			}
		}

		emit_store(jd, iptr, dst, d);
	}
}


/* emit_iconst *****************************************************************

   XXX

*******************************************************************************/

void emit_iconst(codegendata *cd, s4 d, s4 value)
{
	s4 disp;

	if ((value >= -32768) && (value <= 32767))
		M_LDA_INTERN(d, REG_ZERO, value);
	else {
		disp = dseg_add_s4(cd, value);
		M_ILD(d, REG_PV, disp);
	}
}


/* emit_lconst *****************************************************************

   XXX

*******************************************************************************/

void emit_lconst(codegendata *cd, s4 d, s8 value)
{
	s4 disp;

	if ((value >= -32768) && (value <= 32767))
		M_LDA_INTERN(d, REG_ZERO, value);
	else {
		disp = dseg_add_s8(cd, value);
		M_LLD(d, REG_PV, disp);
	}
}


/* emit_branch *****************************************************************

   Emits the code for conditional and unconditional branchs.

*******************************************************************************/

void emit_branch(codegendata *cd, s4 disp, s4 condition, s4 reg, u4 opt)
{
	s4 checkdisp;
	s4 branchdisp;

	/* calculate the different displacements */

	checkdisp  = (disp - 4);
	branchdisp = (disp - 4) >> 2;

	/* check which branch to generate */

	if (condition == BRANCH_UNCONDITIONAL) {
		/* check displacement for overflow */

		if ((checkdisp < (s4) 0xffe00000) || (checkdisp > (s4) 0x001fffff)) {
			/* if the long-branches flag isn't set yet, do it */

			if (!CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd)) {
				log_println("setting error");
				cd->flags |= (CODEGENDATA_FLAG_ERROR |
							  CODEGENDATA_FLAG_LONGBRANCHES);
			}

			vm_abort("emit_branch: emit unconditional long-branch code");
		}
		else {
			M_BR(branchdisp);
		}
	}
	else {
		/* and displacement for overflow */

		if ((checkdisp < (s4) 0xffe00000) || (checkdisp > (s4) 0x001fffff)) {
			/* if the long-branches flag isn't set yet, do it */

			if (!CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd)) {
				log_println("setting error");
				cd->flags |= (CODEGENDATA_FLAG_ERROR |
							  CODEGENDATA_FLAG_LONGBRANCHES);
			}

			vm_abort("emit_branch: emit conditional long-branch code");
		}
		else {
			switch (condition) {
			case BRANCH_EQ:
				M_BEQZ(reg, branchdisp);
				break;
			case BRANCH_NE:
				M_BNEZ(reg, branchdisp);
				break;
			case BRANCH_LT:
				M_BLTZ(reg, branchdisp);
				break;
			case BRANCH_GE:
				M_BGEZ(reg, branchdisp);
				break;
			case BRANCH_GT:
				M_BGTZ(reg, branchdisp);
				break;
			case BRANCH_LE:
				M_BLEZ(reg, branchdisp);
				break;
			default:
				vm_abort("emit_branch: unknown condition %d", condition);
			}
		}
	}
}


/* emit_arithmetic_check *******************************************************

   Emit an ArithmeticException check.

*******************************************************************************/

void emit_arithmetic_check(codegendata *cd, instruction *iptr, s4 reg)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		M_BNEZ(reg, 1);
		/* Destination register must not be REG_ZERO, because then no
		   SIGSEGV is thrown. */
		M_ALD_INTERN(reg, REG_ZERO, EXCEPTION_HARDWARE_ARITHMETIC);
	}
}


/* emit_arrayindexoutofbounds_check ********************************************

   Emit an ArrayIndexOutOfBoundsException check.

*******************************************************************************/

void emit_arrayindexoutofbounds_check(codegendata *cd, instruction *iptr, s4 s1, s4 s2)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		M_ILD(REG_ITMP3, s1, OFFSET(java_arrayheader, size));
		M_CMPULT(s2, REG_ITMP3, REG_ITMP3);
		M_BNEZ(REG_ITMP3, 1);
		M_ALD_INTERN(s2, REG_ZERO, EXCEPTION_HARDWARE_ARRAYINDEXOUTOFBOUNDS);
	}
}


/* emit_classcast_check ********************************************************

   Emit a ClassCastException check.

*******************************************************************************/

void emit_classcast_check(codegendata *cd, instruction *iptr, s4 condition, s4 reg, s4 s1)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		switch (condition) {
		case BRANCH_EQ:
			M_BNEZ(reg, 1);
			break;
		case BRANCH_LE:
			M_BGTZ(reg, 1);
			break;
		default:
			vm_abort("emit_classcast_check: unknown condition %d", condition);
		}
		M_ALD_INTERN(s1, REG_ZERO, EXCEPTION_HARDWARE_CLASSCAST);
	}
}


/* emit_nullpointer_check ******************************************************

   Emit a NullPointerException check.

*******************************************************************************/

void emit_nullpointer_check(codegendata *cd, instruction *iptr, s4 reg)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		M_BNEZ(reg, 1);
		/* Destination register must not be REG_ZERO, because then no
		   SIGSEGV is thrown. */
		M_ALD_INTERN(reg, REG_ZERO, EXCEPTION_HARDWARE_NULLPOINTER);
	}
}


/* emit_exception_check ********************************************************

   Emit an Exception check.

*******************************************************************************/

void emit_exception_check(codegendata *cd, instruction *iptr)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		M_BNEZ(REG_RESULT, 1);
		/* Destination register must not be REG_ZERO, because then no
		   SIGSEGV is thrown. */
		M_ALD_INTERN(REG_RESULT, REG_ZERO, EXCEPTION_HARDWARE_EXCEPTION);
	}
}


/* emit_patcher_stubs **********************************************************

   Generates the code for the patcher stubs.

*******************************************************************************/

void emit_patcher_stubs(jitdata *jd)
{
	codegendata *cd;
	patchref    *pref;
	u4           mcode;
	u1          *savedmcodeptr;
	u1          *tmpmcodeptr;
	s4           targetdisp;
	s4           disp;

	/* get required compiler data */

	cd = jd->cd;

	/* generate code patching stub call code */

	targetdisp = 0;

	for (pref = cd->patchrefs; pref != NULL; pref = pref->next) {
		/* check code segment size */

		MCODECHECK(100);

		/* Get machine code which is patched back in later. The
		   call is 1 instruction word long. */

		tmpmcodeptr = (u1 *) (cd->mcodebase + pref->branchpos);

		mcode = *((u4 *) tmpmcodeptr);

		/* Patch in the call to call the following code (done at
		   compile time). */

		savedmcodeptr = cd->mcodeptr;   /* save current mcodeptr              */
		cd->mcodeptr  = tmpmcodeptr;    /* set mcodeptr to patch position     */

		disp = ((u4 *) savedmcodeptr) - (((u4 *) tmpmcodeptr) + 1);
		M_BSR(REG_ITMP3, disp);

		cd->mcodeptr = savedmcodeptr;   /* restore the current mcodeptr       */

		/* create stack frame */

		M_LSUB_IMM(REG_SP, 6 * 8, REG_SP);

		/* move return address onto stack */

		M_AST(REG_ITMP3, REG_SP, 5 * 8);

		/* move pointer to java_objectheader onto stack */

#if defined(ENABLE_THREADS)
		/* create a virtual java_objectheader */

		(void) dseg_add_unique_address(cd, NULL);                  /* flcword */
		(void) dseg_add_unique_address(cd, lock_get_initial_lock_word());
		disp = dseg_add_unique_address(cd, NULL);                  /* vftbl   */

		M_LDA(REG_ITMP3, REG_PV, disp);
		M_AST(REG_ITMP3, REG_SP, 4 * 8);
#else
		/* nothing to do */
#endif

		/* move machine code onto stack */

		disp = dseg_add_s4(cd, mcode);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST(REG_ITMP3, REG_SP, 3 * 8);

		/* move class/method/field reference onto stack */

		disp = dseg_add_address(cd, pref->ref);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST(REG_ITMP3, REG_SP, 2 * 8);

		/* move data segment displacement onto stack */

		disp = dseg_add_s4(cd, pref->disp);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST(REG_ITMP3, REG_SP, 1 * 8);

		/* move patcher function pointer onto stack */

		disp = dseg_add_functionptr(cd, pref->patcher);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST(REG_ITMP3, REG_SP, 0 * 8);

		if (targetdisp == 0) {
			targetdisp = ((u4 *) cd->mcodeptr) - ((u4 *) cd->mcodebase);

			disp = dseg_add_functionptr(cd, asm_patcher_wrapper);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_JMP(REG_ZERO, REG_ITMP3);
		}
		else {
			disp = (((u4 *) cd->mcodebase) + targetdisp) -
				(((u4 *) cd->mcodeptr) + 1);

			M_BR(disp);
		}
	}
}


/* emit_replacement_stubs ******************************************************

   Generates the code for the replacement stubs.

*******************************************************************************/

#if defined(ENABLE_REPLACEMENT)
void emit_replacement_stubs(jitdata *jd)
{
	codegendata *cd;
	codeinfo    *code;
	rplpoint    *rplp;
	s4           disp;
	s4           i;
#if !defined(NDEBUG)
	u1          *savedmcodeptr;
#endif

	/* get required compiler data */

	cd   = jd->cd;
	code = jd->code;

	rplp = code->rplpoints;

	/* store beginning of replacement stubs */

	code->replacementstubs = (u1*) (cd->mcodeptr - cd->mcodebase);

	for (i = 0; i < code->rplpointcount; ++i, ++rplp) {
		/* do not generate stubs for non-trappable points */

		if (rplp->flags & RPLPOINT_FLAG_NOTRAP)
			continue;

		/* check code segment size */

		MCODECHECK(100);

#if !defined(NDEBUG)
		savedmcodeptr = cd->mcodeptr;
#endif

		/* create stack frame - 16-byte aligned */

		M_LSUB_IMM(REG_SP, 2 * 8, REG_SP);

		/* push address of `rplpoint` struct */

		disp = dseg_add_address(cd, rplp);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST(REG_ITMP3, REG_SP, 0 * 8);

		/* jump to replacement function */

		disp = dseg_add_functionptr(cd, asm_replacement_out);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_JMP(REG_ZERO, REG_ITMP3);

		assert((cd->mcodeptr - savedmcodeptr) == 4*REPLACEMENT_STUB_SIZE);
	}
}
#endif /* defined(ENABLE_REPLACEMENT) */


/* emit_verbosecall_enter ******************************************************

   Generates the code for the call trace.

*******************************************************************************/

#if !defined(NDEBUG)
void emit_verbosecall_enter(jitdata *jd)
{
	methodinfo   *m;
	codegendata  *cd;
	registerdata *rd;
	methoddesc   *md;
	s4            disp;
	s4            i, t;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;
	rd = jd->rd;

	md = m->parseddesc;

	/* mark trace code */

	M_NOP;

	M_LDA(REG_SP, REG_SP, -((ARG_CNT + TMP_CNT + 2) * 8));
	M_AST(REG_RA, REG_SP, 1 * 8);

	/* save argument registers */

	for (i = 0; i < INT_ARG_CNT; i++)
		M_LST(abi_registers_integer_argument[i], REG_SP, (2 + i) * 8);

	for (i = 0; i < FLT_ARG_CNT; i++)
		M_DST(abi_registers_float_argument[i], REG_SP, (2 + INT_ARG_CNT + i) * 8);

	/* save temporary registers for leaf methods */

	if (jd->isleafmethod) {
		for (i = 0; i < INT_TMP_CNT; i++)
			M_LST(rd->tmpintregs[i], REG_SP, (2 + ARG_CNT + i) * 8);

		for (i = 0; i < FLT_TMP_CNT; i++)
			M_DST(rd->tmpfltregs[i], REG_SP, (2 + ARG_CNT + INT_TMP_CNT + i) * 8);
	}

	/* load float arguments into integer registers */

	for (i = 0; i < md->paramcount && i < INT_ARG_CNT; i++) {
		t = md->paramtypes[i].type;

		if (IS_FLT_DBL_TYPE(t)) {
			if (IS_2_WORD_TYPE(t)) {
				M_DST(abi_registers_float_argument[i], REG_SP, 0 * 8);
				M_LLD(abi_registers_integer_argument[i], REG_SP, 0 * 8);
			}
			else {
				M_FST(abi_registers_float_argument[i], REG_SP, 0 * 8);
				M_ILD(abi_registers_integer_argument[i], REG_SP, 0 * 8);
			}
		}
	}

	disp = dseg_add_address(cd, m);
	M_ALD(REG_ITMP1, REG_PV, disp);
	M_AST(REG_ITMP1, REG_SP, 0 * 8);
	disp = dseg_add_functionptr(cd, builtin_verbosecall_enter);
	M_ALD(REG_PV, REG_PV, disp);
	M_JSR(REG_RA, REG_PV);
	disp = (s4) (cd->mcodeptr - cd->mcodebase);
	M_LDA(REG_PV, REG_RA, -disp);
	M_ALD(REG_RA, REG_SP, 1 * 8);

	/* restore argument registers */

	for (i = 0; i < INT_ARG_CNT; i++)
		M_LLD(abi_registers_integer_argument[i], REG_SP, (2 + i) * 8);

	for (i = 0; i < FLT_ARG_CNT; i++)
		M_DLD(abi_registers_float_argument[i], REG_SP, (2 + INT_ARG_CNT + i) * 8);

	/* restore temporary registers for leaf methods */

	if (jd->isleafmethod) {
		for (i = 0; i < INT_TMP_CNT; i++)
			M_LLD(rd->tmpintregs[i], REG_SP, (2 + ARG_CNT + i) * 8);

		for (i = 0; i < FLT_TMP_CNT; i++)
			M_DLD(rd->tmpfltregs[i], REG_SP, (2 + ARG_CNT + INT_TMP_CNT + i) * 8);
	}

	M_LDA(REG_SP, REG_SP, (ARG_CNT + TMP_CNT + 2) * 8);

	/* mark trace code */

	M_NOP;
}
#endif /* !defined(NDEBUG) */


/* emit_verbosecall_exit *******************************************************

   Generates the code for the call trace.

   void builtin_verbosecall_exit(s8 l, double d, float f, methodinfo *m);

*******************************************************************************/

#if !defined(NDEBUG)
void emit_verbosecall_exit(jitdata *jd)
{
	methodinfo   *m;
	codegendata  *cd;
	registerdata *rd;
	s4            disp;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;
	rd = jd->rd;

	/* mark trace code */

	M_NOP;

	M_ASUB_IMM(REG_SP, 4 * 8, REG_SP);
	M_AST(REG_RA, REG_SP, 0 * 8);

	M_LST(REG_RESULT, REG_SP, 1 * 8);
	M_DST(REG_FRESULT, REG_SP, 2 * 8);

	M_MOV(REG_RESULT, REG_A0);
	M_FMOV(REG_FRESULT, REG_FA1);
	M_FMOV(REG_FRESULT, REG_FA2);

	disp = dseg_add_address(cd, m);
	M_ALD(REG_A3, REG_PV, disp);

	disp = dseg_add_functionptr(cd, builtin_verbosecall_exit);
	M_ALD(REG_PV, REG_PV, disp);
	M_JSR(REG_RA, REG_PV);
	disp = (cd->mcodeptr - cd->mcodebase);
	M_LDA(REG_PV, REG_RA, -disp);

	M_DLD(REG_FRESULT, REG_SP, 2 * 8);
	M_LLD(REG_RESULT, REG_SP, 1 * 8);

	M_ALD(REG_RA, REG_SP, 0 * 8);
	M_AADD_IMM(REG_SP, 4 * 8, REG_SP);

	/* mark trace code */

	M_NOP;
}
#endif /* !defined(NDEBUG) */


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

/* src/vm/jit/mips/emit.c - MIPS code emitter functions

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

   $Id: emit.c 4398 2006-01-31 23:43:08Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "vm/jit/mips/codegen.h"
#include "vm/jit/mips/md-abi.h"

#include "mm/memory.h"

#include "threads/lock-common.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/stringlocal.h" /* XXX for gen_resolvebranch */

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

	if (src->flags & INMEMORY) {
		COUNT_SPILLS;

		disp = src->vv.regoff * 8;

		switch (src->type) {
#if SIZEOF_VOID_P == 8
		case TYPE_INT:
		case TYPE_LNG:
		case TYPE_ADR:
			M_LLD(tempreg, REG_SP, disp);
			break;
#else
		case TYPE_INT:
		case TYPE_ADR:
			M_ILD(tempreg, REG_SP, disp);
			break;
		case TYPE_LNG:
			M_LLD(tempreg, REG_SP, disp);
			break;
#endif
		case TYPE_FLT:
			M_FLD(tempreg, REG_SP, disp);
			break;
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


/* emit_load_low ***************************************************************

   Emits a possible load of the low 32-bits of an operand.

*******************************************************************************/

#if SIZEOF_VOID_P == 4
s4 emit_load_low(jitdata *jd, instruction *iptr, varinfo *src, s4 tempreg)
{
	codegendata  *cd;
	s4            disp;
	s4            reg;

	assert(src->type == TYPE_LNG);

	/* get required compiler data */

	cd = jd->cd;

	if (src->flags & INMEMORY) {
		COUNT_SPILLS;

		disp = src->vv.regoff * 8;

#if WORDS_BIGENDIAN == 1
		M_ILD(tempreg, REG_SP, disp + 4);
#else
		M_ILD(tempreg, REG_SP, disp);
#endif

		reg = tempreg;
	}
	else
		reg = GET_LOW_REG(src->vv.regoff);

	return reg;
}
#endif /* SIZEOF_VOID_P == 4 */


/* emit_load_high **************************************************************

   Emits a possible load of the high 32-bits of an operand.

*******************************************************************************/

#if SIZEOF_VOID_P == 4
s4 emit_load_high(jitdata *jd, instruction *iptr, varinfo *src, s4 tempreg)
{
	codegendata  *cd;
	s4            disp;
	s4            reg;

	assert(src->type == TYPE_LNG);

	/* get required compiler data */

	cd = jd->cd;

	if (src->flags & INMEMORY) {
		COUNT_SPILLS;

		disp = src->vv.regoff * 8;

#if WORDS_BIGENDIAN == 1
		M_ILD(tempreg, REG_SP, disp);
#else
		M_ILD(tempreg, REG_SP, disp + 4);
#endif

		reg = tempreg;
	}
	else
		reg = GET_HIGH_REG(src->vv.regoff);

	return reg;
}
#endif /* SIZEOF_VOID_P == 4 */


/* emit_store ******************************************************************

   Emits a possible store to variable.

*******************************************************************************/

void emit_store(jitdata *jd, instruction *iptr, varinfo *dst, s4 d)
{
	codegendata  *cd;
	s4            disp;

	/* get required compiler data */

	cd = jd->cd;

	if (dst->flags & INMEMORY) {
		COUNT_SPILLS;

		disp = dst->vv.regoff * 8;

		switch (dst->type) {
#if SIZEOF_VOID_P == 8
		case TYPE_INT:
		case TYPE_LNG:
		case TYPE_ADR:
			M_LST(d, REG_SP, disp);
			break;
#else
		case TYPE_INT:
		case TYPE_ADR:
			M_IST(d, REG_SP, disp);
			break;
		case TYPE_LNG:
			M_LST(d, REG_SP, disp);
			break;
#endif
		case TYPE_FLT:
			M_FST(d, REG_SP, disp);
			break;
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
#if SIZEOF_VOID_P == 4
			if (IS_2_WORD_TYPE(src->type))
				d = codegen_reg_of_var(iptr->opc, dst, REG_ITMP12_PACKED);
			else
#endif
				d = codegen_reg_of_var(iptr->opc, dst, REG_IFTMP);
			s1 = emit_load(jd, iptr, src, d);
		}
		else {
			s1 = emit_load(jd, iptr, src, REG_IFTMP);
#if SIZEOF_VOID_P == 4
			if (IS_2_WORD_TYPE(src->type))
				d = codegen_reg_of_var(iptr->opc, dst, REG_ITMP12_PACKED);
			else
#endif
				d = codegen_reg_of_var(iptr->opc, dst, s1);
		}

		if (s1 != d) {
			switch (dst->type) {
#if SIZEOF_VOID_P == 8
			case TYPE_INT:
			case TYPE_LNG:
			case TYPE_ADR:
				M_MOV(s1, d);
				break;
#else
			case TYPE_INT:
			case TYPE_ADR:
				M_MOV(s1, d);
				break;
			case TYPE_LNG:
				M_LNGMOVE(s1, d);
				break;
#endif
			case TYPE_FLT:
				M_FMOV(s1, d);
				break;
			case TYPE_DBL:
				M_DMOV(s1, d);
				break;
			default:
				vm_abort("emit_copy: unknown type %d", dst->type);
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
        M_IADD_IMM(REG_ZERO, value, d);
	else if ((value >= 0) && (value <= 0xffff))
        M_OR_IMM(REG_ZERO, value, d);
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

#if SIZEOF_VOID_P == 8
	if ((value >= -32768) && (value <= 32767))
		M_LADD_IMM(REG_ZERO, value, d);
	else if ((value >= 0) && (value <= 0xffff))
		M_OR_IMM(REG_ZERO, value, d);
	else {
		disp = dseg_add_s8(cd, value);
		M_LLD(d, REG_PV, disp);
	}
#else
	disp = dseg_add_s8(cd, value);
	M_LLD(d, REG_PV, disp);
#endif
}


/* emit_branch *****************************************************************

   Emits the code for conditional and unconditional branchs.

   NOTE: The reg argument may contain two packed registers.

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

		if ((checkdisp < (s4) 0xffff8000) || (checkdisp > (s4) 0x00007fff)) {
			/* if the long-branches flag isn't set yet, do it */

			if (!CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd)) {
				cd->flags |= (CODEGENDATA_FLAG_ERROR |
							  CODEGENDATA_FLAG_LONGBRANCHES);
			}

			vm_abort("emit_branch: emit unconditional long-branch code");
		}
		else {
			M_BR(branchdisp);
			M_NOP;
		}
	}
	else {
		/* and displacement for overflow */

		if ((checkdisp < (s4) 0xffff8000) || (checkdisp > (s4) 0x00007fff)) {
			/* if the long-branches flag isn't set yet, do it */

			if (!CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd)) {
				cd->flags |= (CODEGENDATA_FLAG_ERROR |
							  CODEGENDATA_FLAG_LONGBRANCHES);
			}

			switch (condition) {
			case BRANCH_EQ:
				M_BNE(GET_HIGH_REG(reg), GET_LOW_REG(reg), 5);
				break;
			case BRANCH_NE:
				M_BEQ(GET_HIGH_REG(reg), GET_LOW_REG(reg), 5);
				break;
			case BRANCH_LT:
				M_BGEZ(reg, 5);
				break;
			case BRANCH_GE:
				M_BLTZ(reg, 5);
				break;
			case BRANCH_GT:
				M_BLEZ(reg, 5);
				break;
			case BRANCH_LE:
				M_BGTZ(reg, 5);
				break;
			default:
				vm_abort("emit_branch: unknown condition %d", condition);
			}

			/* The actual branch code which is over-jumped (NOTE: we
			   don't use a branch delay slot here). */

			M_LUI(REG_ITMP3, branchdisp >> 16);
			M_OR_IMM(REG_ITMP3, branchdisp, REG_ITMP3);
			M_AADD(REG_PV, REG_ITMP3, REG_ITMP3);
			M_JMP(REG_ITMP3);
			M_NOP;

		}
		else {
			switch (condition) {
			case BRANCH_EQ:
				M_BEQ(GET_HIGH_REG(reg), GET_LOW_REG(reg), branchdisp);
				break;
			case BRANCH_NE:
				M_BNE(GET_HIGH_REG(reg), GET_LOW_REG(reg), branchdisp);
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

			/* branch delay */
			M_NOP;
		}
	}
}


/* emit_arithmetic_check *******************************************************

   Emit an ArithmeticException check.

*******************************************************************************/

void emit_arithmetic_check(codegendata *cd, instruction *iptr, s4 reg)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		M_BNEZ(reg, 2);
		M_NOP;
		M_ALD_INTERN(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_ARITHMETIC);
	}
}


/* emit_arrayindexoutofbounds_check ********************************************

   Emit an ArrayIndexOutOfBoundsException check.

*******************************************************************************/

void emit_arrayindexoutofbounds_check(codegendata *cd, instruction *iptr, s4 s1, s4 s2)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		M_ILD_INTERN(REG_ITMP3, s1, OFFSET(java_arrayheader, size));
		M_CMPULT(s2, REG_ITMP3, REG_ITMP3);
		M_BNEZ(REG_ITMP3, 2);
		M_NOP;
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
		case ICMD_IFEQ:
			M_BNEZ(reg, 2);
			break;

		case ICMD_IFNE:
			M_BEQZ(reg, 2);
			break;

		case ICMD_IFLE:
			M_BGTZ(reg, 2);
			break;

		default:
			vm_abort("emit_classcast_check: unknown condition %d", condition);
		}

		M_NOP;
		M_ALD_INTERN(s1, REG_ZERO, EXCEPTION_HARDWARE_CLASSCAST);
	}
}


/* emit_nullpointer_check ******************************************************

   Emit a NullPointerException check.

*******************************************************************************/

void emit_nullpointer_check(codegendata *cd, instruction *iptr, s4 reg)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		M_BNEZ(reg, 2);
		M_NOP;
		M_ALD_INTERN(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_NULLPOINTER);
	}
}


/* emit_exception_check ********************************************************

   Emit an Exception check.

*******************************************************************************/

void emit_exception_check(codegendata *cd, instruction *iptr)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		M_BNEZ(REG_RESULT, 2);
		M_NOP;
		M_ALD_INTERN(REG_RESULT, REG_ZERO, EXCEPTION_HARDWARE_EXCEPTION);
	}
}


/* emit_patcher_stubs **********************************************************

   Generates the code for the patcher stubs.

*******************************************************************************/

void emit_patcher_stubs(jitdata *jd)
{
	codegendata *cd;
	patchref    *pr;
	u4           mcode[5];
	u1          *savedmcodeptr;
	u1          *tmpmcodeptr;
	s4           targetdisp;
	s4           disp;

	/* get required compiler data */

	cd = jd->cd;

	/* generate code patching stub call code */

	targetdisp = 0;

/* 	for (pr = list_first_unsynced(cd->patchrefs); pr != NULL; */
/* 		 pr = list_next_unsynced(cd->patchrefs, pr)) { */
	for (pr = cd->patchrefs; pr != NULL; pr = pr->next) {
		/* check code segment size */

		MCODECHECK(100);

		/* Get machine code which is patched back in later. The
		   call is 2 instruction words long. */

		tmpmcodeptr = (u1 *) (cd->mcodebase + pr->branchpos);

		/* We use 2 loads here as an unaligned 8-byte read on 64-bit
		   MIPS causes a SIGSEGV and using the same code for both
		   architectures is much better. */

		mcode[0] = ((u4 *) tmpmcodeptr)[0];
		mcode[1] = ((u4 *) tmpmcodeptr)[1];

		mcode[2] = ((u4 *) tmpmcodeptr)[2];
		mcode[3] = ((u4 *) tmpmcodeptr)[3];
		mcode[4] = ((u4 *) tmpmcodeptr)[4];

		/* Patch in the call to call the following code (done at
		   compile time). */

		savedmcodeptr = cd->mcodeptr;   /* save current mcodeptr              */
		cd->mcodeptr  = tmpmcodeptr;    /* set mcodeptr to patch position     */

		disp = ((u4 *) savedmcodeptr) - (((u4 *) tmpmcodeptr) + 1);

/* 		if ((disp < (s4) 0xffff8000) || (disp > (s4) 0x00007fff)) { */
			/* Recalculate the displacement to be relative to PV. */

			disp = savedmcodeptr - cd->mcodebase;

			M_LUI(REG_ITMP3, disp >> 16);
			M_OR_IMM(REG_ITMP3, disp, REG_ITMP3);
			M_AADD(REG_PV, REG_ITMP3, REG_ITMP3);
			M_JMP(REG_ITMP3);
			M_NOP;
/* 		} */
/* 		else { */
/* 			M_BR(disp); */
/* 			M_NOP; */
/* 			M_NOP; */
/* 			M_NOP; */
/* 			M_NOP; */
/* 		} */

		cd->mcodeptr = savedmcodeptr;   /* restore the current mcodeptr   */

		/* create stack frame */

		M_ASUB_IMM(REG_SP, 8 * 8, REG_SP);

		/* calculate return address and move it onto the stack */

		M_LDA(REG_ITMP3, REG_PV, pr->branchpos);
		M_AST(REG_ITMP3, REG_SP, 7 * 8);

		/* move pointer to java_objectheader onto stack */

#if defined(ENABLE_THREADS)
		/* create a virtual java_objectheader */

		(void) dseg_add_unique_address(cd, NULL);                  /* flcword */
		(void) dseg_add_unique_address(cd, lock_get_initial_lock_word());
		disp = dseg_add_unique_address(cd, NULL);                  /* vftbl   */

		M_LDA(REG_ITMP3, REG_PV, disp);
		M_AST(REG_ITMP3, REG_SP, 6 * 8);
#else
		/* do nothing */
#endif

		/* move machine code onto stack */

		disp = dseg_add_s4(cd, mcode[0]);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST(REG_ITMP3, REG_SP, 3 * 8 + 0);

		disp = dseg_add_s4(cd, mcode[1]);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST(REG_ITMP3, REG_SP, 3 * 8 + 4);

		disp = dseg_add_s4(cd, mcode[2]);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST(REG_ITMP3, REG_SP, 4 * 8 + 0);

		disp = dseg_add_s4(cd, mcode[3]);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST(REG_ITMP3, REG_SP, 4 * 8 + 4);

		disp = dseg_add_s4(cd, mcode[4]);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST(REG_ITMP3, REG_SP, 5 * 8 + 0);

		/* move class/method/field reference onto stack */

		disp = dseg_add_address(cd, pr->ref);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST(REG_ITMP3, REG_SP, 2 * 8);

		/* move data segment displacement onto stack */

		disp = dseg_add_s4(cd, pr->disp);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST(REG_ITMP3, REG_SP, 1 * 8);

		/* move patcher function pointer onto stack */

		disp = dseg_add_functionptr(cd, pr->patcher);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST(REG_ITMP3, REG_SP, 0 * 8);

		if (targetdisp == 0) {
			targetdisp = ((u4 *) cd->mcodeptr) - ((u4 *) cd->mcodebase);

			disp = dseg_add_functionptr(cd, asm_patcher_wrapper);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_JMP(REG_ITMP3);
			M_NOP;
		}
		else {
			disp = (((u4 *) cd->mcodebase) + targetdisp) -
				(((u4 *) cd->mcodeptr) + 1);

			M_BR(disp);
			M_NOP;
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

		M_ASUB_IMM(REG_SP, 2 * 8, REG_SP);

		/* push address of `rplpoint` struct */

		disp = dseg_add_address(cd, rplp);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST(REG_ITMP3, REG_SP, 0 * 8);

		/* jump to replacement function */

		disp = dseg_add_functionptr(cd, asm_replacement_out);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_JMP(REG_ITMP3);
		M_NOP; /* delay slot */

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
	s4            i, j, t;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;
	rd = jd->rd;

	md = m->parseddesc;

	/* mark trace code */

	M_NOP;

	M_LDA(REG_SP, REG_SP, -(PA_SIZE + (2 + ARG_CNT + TMP_CNT) * 8));
	M_AST(REG_RA, REG_SP, PA_SIZE + 1 * 8);

	/* save argument registers (we store the registers as address
	   types, so it's correct for MIPS32 too) */

	for (i = 0; i < INT_ARG_CNT; i++)
		M_AST(abi_registers_integer_argument[i], REG_SP, PA_SIZE + (2 + i) * 8);

	for (i = 0; i < FLT_ARG_CNT; i++)
		M_DST(abi_registers_float_argument[i], REG_SP, PA_SIZE + (2 + INT_ARG_CNT + i) * 8);

	/* save temporary registers for leaf methods */

	if (jd->isleafmethod) {
		for (i = 0; i < INT_TMP_CNT; i++)
			M_AST(rd->tmpintregs[i], REG_SP, PA_SIZE + (2 + ARG_CNT + i) * 8);

		for (i = 0; i < FLT_TMP_CNT; i++)
			M_DST(rd->tmpfltregs[i], REG_SP, PA_SIZE + (2 + ARG_CNT + INT_TMP_CNT + i) * 8);
	}

	/* Load float arguments into integer registers.  MIPS32 has less
	   float argument registers than integer ones, we need to check
	   that. */

	for (i = 0; i < md->paramcount && i < INT_ARG_CNT && i < FLT_ARG_CNT; i++) {
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

#if SIZEOF_VOID_P == 4
		for (i = 0, j = 0; i < md->paramcount && i < TRACE_ARGS_NUM; i++) {
			t = md->paramtypes[i].type;

			if (IS_INT_LNG_TYPE(t)) {
				if (IS_2_WORD_TYPE(t)) {
					M_ILD(abi_registers_integer_argument[j], REG_SP, PA_SIZE + (2 + i) * 8);
					M_ILD(abi_registers_integer_argument[j + 1], REG_SP, PA_SIZE + (2 + i) * 8 + 4);
				}
				else {
# if WORDS_BIGENDIAN == 1
					M_MOV(REG_ZERO, abi_registers_integer_argument[j]);
					M_ILD(abi_registers_integer_argument[j + 1], REG_SP, PA_SIZE + (2 + i) * 8);
# else
					M_ILD(abi_registers_integer_argument[j], REG_SP, PA_SIZE + (2 + i) * 8);
					M_MOV(REG_ZERO, abi_registers_integer_argument[j + 1]);
# endif
				}
				j += 2;
			}
		}
#endif

	disp = dseg_add_address(cd, m);
	M_ALD(REG_ITMP1, REG_PV, disp);
	M_AST(REG_ITMP1, REG_SP, PA_SIZE + 0 * 8);
	disp = dseg_add_functionptr(cd, builtin_verbosecall_enter);
	M_ALD(REG_ITMP3, REG_PV, disp);
	M_JSR(REG_RA, REG_ITMP3);
	M_NOP;

	/* restore argument registers */

	for (i = 0; i < INT_ARG_CNT; i++)
		M_ALD(abi_registers_integer_argument[i], REG_SP, PA_SIZE + (2 + i) * 8);

	for (i = 0; i < FLT_ARG_CNT; i++)
		M_DLD(abi_registers_float_argument[i], REG_SP, PA_SIZE + (2 + INT_ARG_CNT + i) * 8);

	/* restore temporary registers for leaf methods */

	if (jd->isleafmethod) {
		for (i = 0; i < INT_TMP_CNT; i++)
			M_ALD(rd->tmpintregs[i], REG_SP, PA_SIZE + (2 + ARG_CNT + i) * 8);

		for (i = 0; i < FLT_TMP_CNT; i++)
			M_DLD(rd->tmpfltregs[i], REG_SP, PA_SIZE + (2 + ARG_CNT + INT_TMP_CNT + i) * 8);
	}

	M_ALD(REG_RA, REG_SP, PA_SIZE + 1 * 8);
	M_LDA(REG_SP, REG_SP, PA_SIZE + (2 + ARG_CNT + TMP_CNT) * 8);

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
	methoddesc   *md;
	s4            disp;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;
	rd = jd->rd;

	md = m->parseddesc;

	/* mark trace code */

	M_NOP;

#if SIZEOF_VOID_P == 8
	M_ASUB_IMM(REG_SP, 4 * 8, REG_SP);          /* keep stack 16-byte aligned */
	M_AST(REG_RA, REG_SP, 0 * 8);

	M_LST(REG_RESULT, REG_SP, 1 * 8);
	M_DST(REG_FRESULT, REG_SP, 2 * 8);

	M_MOV(REG_RESULT, REG_A0);
	M_DMOV(REG_FRESULT, REG_FA1);
	M_FMOV(REG_FRESULT, REG_FA2);

	disp = dseg_add_address(cd, m);
	M_ALD(REG_A4, REG_PV, disp);
#else
	M_ASUB_IMM(REG_SP, (8*4 + 4 * 8), REG_SP);
	M_AST(REG_RA, REG_SP, 8*4 + 0 * 8);

	M_LST(REG_RESULT_PACKED, REG_SP, 8*4 + 1 * 8);
	M_DST(REG_FRESULT, REG_SP, 8*4 + 2 * 8);

	switch (md->returntype.type) {
	case TYPE_LNG:
		M_LNGMOVE(REG_RESULT_PACKED, REG_A0_A1_PACKED);
		break;

	default:
# if WORDS_BIGENDIAN == 1
		M_MOV(REG_ZERO, REG_A0);
		M_MOV(REG_RESULT, REG_A1);
# else
		M_MOV(REG_RESULT, REG_A0);
		M_MOV(REG_ZERO, REG_A1);
# endif
	}

	M_LLD(REG_A2_A3_PACKED, REG_SP, 8*4 + 2 * 8);
	M_FST(REG_FRESULT, REG_SP, 4*4 + 0 * 4);

	disp = dseg_add_address(cd, m);
	M_ALD(REG_ITMP1, REG_PV, disp);
	M_AST(REG_ITMP1, REG_SP, 4*4 + 1 * 4);
#endif

	disp = dseg_add_functionptr(cd, builtin_verbosecall_exit);
	M_ALD(REG_ITMP3, REG_PV, disp);
	M_JSR(REG_RA, REG_ITMP3);
	M_NOP;

#if SIZEOF_VOID_P == 8
	M_DLD(REG_FRESULT, REG_SP, 2 * 8);
	M_LLD(REG_RESULT, REG_SP, 1 * 8);

	M_ALD(REG_RA, REG_SP, 0 * 8);
	M_AADD_IMM(REG_SP, 4 * 8, REG_SP);
#else
	M_DLD(REG_FRESULT, REG_SP, 8*4 + 2 * 8);
	M_LLD(REG_RESULT_PACKED, REG_SP, 8*4 + 1 * 8);

	M_ALD(REG_RA, REG_SP, 8*4 + 0 * 8);
	M_AADD_IMM(REG_SP, 8*4 + 4 * 8, REG_SP);
#endif

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

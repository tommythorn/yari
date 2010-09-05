/* src/vm/jit/powerpc/emit.c - PowerPC code emitter functions

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

   $Id: emit.c 7816 2007-04-25 19:38:46Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "md-abi.h"

#include "vm/jit/powerpc/codegen.h"

#include "mm/memory.h"

#include "threads/lock-common.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"

#include "vm/jit/abi.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/codegen-common.h"
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
	codegendata *cd;
	s4           disp;
	s4           reg;

	/* get required compiler data */

	cd = jd->cd;

	if (IS_INMEMORY(src->flags)) {
		COUNT_SPILLS;

		disp = src->vv.regoff * 4;

		switch (src->type) {
		case TYPE_INT:
		case TYPE_ADR:
			M_ILD(tempreg, REG_SP, disp);
			break;
		case TYPE_LNG:
			M_LLD(tempreg, REG_SP, disp);
			break;
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

s4 emit_load_low(jitdata *jd, instruction *iptr, varinfo *src, s4 tempreg)
{
	codegendata  *cd;
	s4            disp;
	s4            reg;

	assert(src->type == TYPE_LNG);

	/* get required compiler data */

	cd = jd->cd;

	if (IS_INMEMORY(src->flags)) {
		COUNT_SPILLS;

		disp = src->vv.regoff * 4;

		M_ILD(tempreg, REG_SP, disp + 4);

		reg = tempreg;
	}
	else
		reg = GET_LOW_REG(src->vv.regoff);

	return reg;
}


/* emit_load_high **************************************************************

   Emits a possible load of the high 32-bits of an operand.

*******************************************************************************/

s4 emit_load_high(jitdata *jd, instruction *iptr, varinfo *src, s4 tempreg)
{
	codegendata  *cd;
	s4            disp;
	s4            reg;

	assert(src->type == TYPE_LNG);

	/* get required compiler data */

	cd = jd->cd;

	if (IS_INMEMORY(src->flags)) {
		COUNT_SPILLS;

		disp = src->vv.regoff * 4;

		M_ILD(tempreg, REG_SP, disp);

		reg = tempreg;
	}
	else
		reg = GET_HIGH_REG(src->vv.regoff);

	return reg;
}


/* emit_store ******************************************************************

   Emit a possible store for the given variable.

*******************************************************************************/

void emit_store(jitdata *jd, instruction *iptr, varinfo *dst, s4 d)
{
	codegendata *cd;
	s4           disp;

	/* get required compiler data */

	cd = jd->cd;

	if (IS_INMEMORY(dst->flags)) {
		COUNT_SPILLS;

		disp = dst->vv.regoff * 4;

		switch (dst->type) {
		case TYPE_INT:
		case TYPE_ADR:
			M_IST(d, REG_SP, disp);
			break;
		case TYPE_LNG:
			M_LST(d, REG_SP, disp);
			break;
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
		(IS_INMEMORY(src->flags ^ dst->flags))) {

		if ((src->type == TYPE_RET) || (dst->type == TYPE_RET)) {
			/* emit nothing, as the value won't be used anyway */
			return;
		}

		/* If one of the variables resides in memory, we can eliminate
		   the register move from/to the temporary register with the
		   order of getting the destination register and the load. */

		if (IS_INMEMORY(src->flags)) {
			if (IS_LNG_TYPE(src->type))
				d = codegen_reg_of_var(iptr->opc, dst, REG_ITMP12_PACKED);
			else
				d = codegen_reg_of_var(iptr->opc, dst, REG_IFTMP);

			s1 = emit_load(jd, iptr, src, d);
		}
		else {
			if (IS_LNG_TYPE(src->type))
				s1 = emit_load(jd, iptr, src, REG_ITMP12_PACKED);
			else
				s1 = emit_load(jd, iptr, src, REG_IFTMP);

			d = codegen_reg_of_var(iptr->opc, dst, s1);
		}

		if (s1 != d) {
			switch (src->type) {
			case TYPE_INT:
			case TYPE_ADR:
				M_MOV(s1, d);
				break;
			case TYPE_LNG:
				M_MOV(GET_LOW_REG(s1), GET_LOW_REG(d));
				M_MOV(GET_HIGH_REG(s1), GET_HIGH_REG(d));
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


/* emit_branch *****************************************************************

   Emits the code for conditional and unconditional branchs.

*******************************************************************************/

void emit_branch(codegendata *cd, s4 disp, s4 condition, s4 reg, u4 opt)
{
	s4 checkdisp;
	s4 branchdisp;

	/* calculate the different displacements */

	checkdisp  =  disp + 4;
	branchdisp = (disp - 4) >> 2;

	/* check which branch to generate */

	if (condition == BRANCH_UNCONDITIONAL) {
		/* check displacement for overflow */

		if ((checkdisp < (s4) 0xfe000000) || (checkdisp > (s4) 0x01fffffc)) {
			/* if the long-branches flag isn't set yet, do it */

			if (!CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd)) {
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

		if ((checkdisp < (s4) 0xffff8000) || (checkdisp > (s4) 0x00007fff)) {
			/* if the long-branches flag isn't set yet, do it */

			if (!CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd)) {
				cd->flags |= (CODEGENDATA_FLAG_ERROR |
							  CODEGENDATA_FLAG_LONGBRANCHES);
			}

			switch (condition) {
			case BRANCH_EQ:
				M_BNE(1);
				M_BR(branchdisp);
				break;
			case BRANCH_NE:
				M_BEQ(1);
				M_BR(branchdisp);
				break;
			case BRANCH_LT:
				M_BGE(1);
				M_BR(branchdisp);
				break;
			case BRANCH_GE:
				M_BLT(1);
				M_BR(branchdisp);
				break;
			case BRANCH_GT:
				M_BLE(1);
				M_BR(branchdisp);
				break;
			case BRANCH_LE:
				M_BGT(1);
				M_BR(branchdisp);
				break;
			case BRANCH_NAN:
				vm_abort("emit_branch: long BRANCH_NAN");
				break;
			default:
				vm_abort("emit_branch: unknown condition %d", condition);
			}
		}
		else {
			switch (condition) {
			case BRANCH_EQ:
				M_BEQ(branchdisp);
				break;
			case BRANCH_NE:
				M_BNE(branchdisp);
				break;
			case BRANCH_LT:
				M_BLT(branchdisp);
				break;
			case BRANCH_GE:
				M_BGE(branchdisp);
				break;
			case BRANCH_GT:
				M_BGT(branchdisp);
				break;
			case BRANCH_LE:
				M_BLE(branchdisp);
				break;
			case BRANCH_NAN:
				M_BNAN(branchdisp);
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
		M_TST(reg);
		M_BNE(1);
		M_ALD_INTERN(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_ARITHMETIC);
	}
}


/* emit_arrayindexoutofbounds_check ********************************************

   Emit a ArrayIndexOutOfBoundsException check.

*******************************************************************************/

void emit_arrayindexoutofbounds_check(codegendata *cd, instruction *iptr, s4 s1, s4 s2)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		M_ILD(REG_ITMP3, s1, OFFSET(java_arrayheader, size));
		M_TRAPGEU(s2, REG_ITMP3);
	}
}


/* emit_classcast_check ********************************************************

   Emit a ClassCastException check.

*******************************************************************************/

void emit_classcast_check(codegendata *cd, instruction *iptr, s4 condition, s4 reg, s4 s1)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		switch (condition) {
		case BRANCH_LE:
			M_BGT(1);
			break;
		case BRANCH_EQ:
			M_BNE(1);
			break;
		case BRANCH_GT:
			M_BLE(1);
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
		M_TST(reg);
		M_BNE(1);
		M_ALD_INTERN(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_NULLPOINTER);
	}
}


/* emit_exception_check ********************************************************

   Emit an Exception check.

*******************************************************************************/

void emit_exception_check(codegendata *cd, instruction *iptr)
{
	if (INSTRUCTION_MUST_CHECK(iptr)) {
		M_TST(REG_RESULT);
		M_BNE(1);
		M_ALD_INTERN(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_EXCEPTION);
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

		savedmcodeptr = cd->mcodeptr;   /* save current mcodeptr          */
		cd->mcodeptr  = tmpmcodeptr;    /* set mcodeptr to patch position */

		disp = ((u4 *) savedmcodeptr) - (((u4 *) tmpmcodeptr) + 1);
		M_BR(disp);

		cd->mcodeptr = savedmcodeptr;   /* restore the current mcodeptr   */

		/* create stack frame - keep stack 16-byte aligned */

		M_AADD_IMM(REG_SP, -8 * 4, REG_SP);

		/* calculate return address and move it onto the stack */

		M_LDA(REG_ITMP3, REG_PV, pref->branchpos);
		M_AST_INTERN(REG_ITMP3, REG_SP, 5 * 4);

		/* move pointer to java_objectheader onto stack */

#if defined(ENABLE_THREADS)
		/* order reversed because of data segment layout */

		(void) dseg_add_unique_address(cd, NULL);                  /* flcword */
		(void) dseg_add_unique_address(cd, lock_get_initial_lock_word());
		disp = dseg_add_unique_address(cd, NULL);                  /* vftbl   */

		M_LDA(REG_ITMP3, REG_PV, disp);
		M_AST_INTERN(REG_ITMP3, REG_SP, 4 * 4);
#else
		/* do nothing */
#endif

		/* move machine code onto stack */

		disp = dseg_add_s4(cd, mcode);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST_INTERN(REG_ITMP3, REG_SP, 3 * 4);

		/* move class/method/field reference onto stack */

		disp = dseg_add_address(cd, pref->ref);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST_INTERN(REG_ITMP3, REG_SP, 2 * 4);

		/* move data segment displacement onto stack */

		disp = dseg_add_s4(cd, pref->disp);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST_INTERN(REG_ITMP3, REG_SP, 1 * 4);

		/* move patcher function pointer onto stack */

		disp = dseg_add_functionptr(cd, pref->patcher);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST_INTERN(REG_ITMP3, REG_SP, 0 * 4);

		if (targetdisp == 0) {
			targetdisp = ((u4 *) cd->mcodeptr) - ((u4 *) cd->mcodebase);

			disp = dseg_add_functionptr(cd, asm_patcher_wrapper);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_MTCTR(REG_ITMP3);
			M_RTS;
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

		/* create stack frame - keep 16-byte aligned */

		M_AADD_IMM(REG_SP, -4 * 4, REG_SP);

		/* push address of `rplpoint` struct */

		disp = dseg_add_address(cd, rplp);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST_INTERN(REG_ITMP3, REG_SP, 0 * 4);

		/* jump to replacement function */

		disp = dseg_add_functionptr(cd, asm_replacement_out);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_MTCTR(REG_ITMP3);
		M_RTS;

		assert((cd->mcodeptr - savedmcodeptr) == 4*REPLACEMENT_STUB_SIZE);
	}
}
#endif /* defined(ENABLE_REPLACEMENT) */


/* emit_verbosecall_enter ******************************************************

   Generates the code for the call trace.

*******************************************************************************/

void emit_verbosecall_enter(jitdata *jd)
{
#if !defined(NDEBUG)
	methodinfo   *m;
	codegendata  *cd;
	registerdata *rd;
	s4 s1, p, t, d;
	int stack_off;
	int stack_size;
	methoddesc *md;

	if (!JITDATA_HAS_FLAG_VERBOSECALL(jd))
		return;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;
	rd = jd->rd;

	md = m->parseddesc;
	
	/* Build up Stackframe for builtin_trace_args call (a multiple of 16) */
	/* For Darwin:                                                        */
	/* LA + TRACE_ARGS_NUM u8 args + methodinfo + LR                      */
	/* LA_SIZE(=6*4) + 8*8         + 4          + 4  + 0(Padding)         */
	/* 6 * 4 + 8 * 8 + 2 * 4 = 12 * 8 = 6 * 16                            */
	/* For Linux:                                                         */
	/* LA + (TRACE_ARGS_NUM - INT_ARG_CNT/2) u8 args + methodinfo         */
	/* + INT_ARG_CNT * 4 ( save integer registers) + LR + 8 + 8 (Padding) */
	/* LA_SIZE(=2*4) + 4 * 8 + 4 + 8 * 4 + 4 + 8                          */
	/* 2 * 4 + 4 * 8 + 10 * 4 + 1 * 8 + 8= 12 * 8 = 6 * 16                */
	
	/* in nativestubs no Place to save the LR (Link Register) would be needed */
	/* but since the stack frame has to be aligned the 4 Bytes would have to  */
	/* be padded again */

#if defined(__DARWIN__)
	stack_size = LA_SIZE + (TRACE_ARGS_NUM + 1) * 8;
#else
	stack_size = 6 * 16;
#endif

	/* mark trace code */

	M_NOP;

	M_MFLR(REG_ZERO);
	M_AST(REG_ZERO, REG_SP, LA_LR_OFFSET);
	M_STWU(REG_SP, REG_SP, -stack_size);

	M_CLR(REG_ITMP1);    /* clear help register */

	/* save up to TRACE_ARGS_NUM arguments into the reserved stack space */
#if defined(__DARWIN__)
	/* Copy Params starting from first to Stack                          */
	/* since TRACE_ARGS == INT_ARG_CNT all used integer argument regs    */ 
	/* are saved                                                         */
	p = 0;
#else
	/* Copy Params starting from fifth to Stack (INT_ARG_CNT/2) are in   */
	/* integer argument regs                                             */
	/* all integer argument registers have to be saved                   */
	for (p = 0; p < 8; p++) {
		d = abi_registers_integer_argument[p];
		/* save integer argument registers */
		M_IST(d, REG_SP, LA_SIZE + 4 * 8 + 4 + p * 4);
	}
	p = 4;
#endif
	stack_off = LA_SIZE;

	for (; p < md->paramcount && p < TRACE_ARGS_NUM; p++, stack_off += 8) {
		t = md->paramtypes[p].type;

		if (IS_INT_LNG_TYPE(t)) {
			if (!md->params[p].inmemory) {
				s1 = md->params[p].regoff;

				if (IS_2_WORD_TYPE(t)) {
					M_IST(GET_HIGH_REG(s1), REG_SP, stack_off);
					M_IST(GET_LOW_REG(s1), REG_SP, stack_off + 4);
				}
				else {
					M_IST(REG_ITMP1, REG_SP, stack_off);
					M_IST(s1, REG_SP, stack_off + 4);
				}
			}
			else {
				s1 = (md->params[p].regoff + cd->stackframesize) * 4 
					+ stack_size;
				if (IS_2_WORD_TYPE(t)) {
					M_ILD(REG_ITMP2, REG_SP, s1);
					M_IST(REG_ITMP2, REG_SP, stack_off);
					M_ILD(REG_ITMP2, REG_SP, s1 + 4);
					M_IST(REG_ITMP2, REG_SP, stack_off + 4);
				}
				else {
					M_IST(REG_ITMP1, REG_SP, stack_off);
					M_ILD(REG_ITMP2, REG_SP, s1);
					M_IST(REG_ITMP2, REG_SP, stack_off + 4);
				}
			}
		}
		else {
			if (!md->params[p].inmemory) {
				s1 = md->params[p].regoff;

				if (!IS_2_WORD_TYPE(t)) {
					M_IST(REG_ITMP1, REG_SP, stack_off);
					M_FST(s1, REG_SP, stack_off + 4);
				}
				else
					M_DST(s1, REG_SP, stack_off);
			}
			else {
				/* this should not happen */
			}
		}
	}

	/* load first 4 (==INT_ARG_CNT/2) arguments into integer registers */
#if defined(__DARWIN__)
	for (p = 0; p < 8; p++) {
		d = abi_registers_integer_argument[p];
		M_ILD(d, REG_SP, LA_SIZE + p * 4);
	}
#else
	/* LINUX */
	/* Set integer and float argument registers vor trace_args call */
	/* offset to saved integer argument registers                   */

	stack_off = LA_SIZE + 4 * 8 + 4;

	for (p = 0; (p < 4) && (p < md->paramcount); p++) {
		t = md->paramtypes[p].type;

		if (IS_INT_LNG_TYPE(t)) {
			/* "stretch" int types */
			if (!IS_2_WORD_TYPE(t)) {
				M_CLR(abi_registers_integer_argument[2 * p]);
				M_ILD(abi_registers_integer_argument[2 * p + 1], REG_SP,stack_off);
				stack_off += 4;
			}
			else {
				M_ILD(abi_registers_integer_argument[2 * p + 1], REG_SP,stack_off + 4);
				M_ILD(abi_registers_integer_argument[2 * p], REG_SP,stack_off);
				stack_off += 8;
			}
		}
		else {
			if (!md->params[p].inmemory) {
				/* use reserved Place on Stack (sp + 5 * 16) to copy  */
				/* float/double arg reg to int reg                    */

				s1 = md->params[p].regoff;

				if (!IS_2_WORD_TYPE(t)) {
					M_FST(s1, REG_SP, 5 * 16);
					M_ILD(abi_registers_integer_argument[2 * p + 1], REG_SP, 5 * 16);
					M_CLR(abi_registers_integer_argument[2 * p]);
				}
				else {
					M_DST(s1, REG_SP, 5 * 16);
					M_ILD(abi_registers_integer_argument[2 * p + 1], REG_SP,  5 * 16 + 4);
					M_ILD(abi_registers_integer_argument[2 * p], REG_SP, 5 * 16);
				}
			}
		}
	}
#endif

	/* put methodinfo pointer on Stackframe */
	p = dseg_add_address(cd, m);
	M_ALD(REG_ITMP1, REG_PV, p);
#if defined(__DARWIN__)
	M_AST(REG_ITMP1, REG_SP, LA_SIZE + TRACE_ARGS_NUM * 8); 
#else
	M_AST(REG_ITMP1, REG_SP, LA_SIZE + 4 * 8);
#endif
	p = dseg_add_functionptr(cd, builtin_verbosecall_enter);
	M_ALD(REG_ITMP2, REG_PV, p);
	M_MTCTR(REG_ITMP2);
	M_JSR;

#if defined(__DARWIN__)
	/* restore integer argument registers from the reserved stack space */

	stack_off = LA_SIZE;

	for (p = 0; p < md->paramcount && p < TRACE_ARGS_NUM; p++, stack_off += 8) {
		t = md->paramtypes[p].type;

		if (IS_INT_LNG_TYPE(t)) {
			if (!md->params[p].inmemory) {
				s1 = md->params[p].regoff;

				if (IS_2_WORD_TYPE(t)) {
					M_ILD(GET_HIGH_REG(s1), REG_SP, stack_off);
					M_ILD(GET_LOW_REG(s1), REG_SP, stack_off + 4);
				}
				else
					M_ILD(s1, REG_SP, stack_off + 4);
			}
		}
	}
#else
	/* LINUX */
	for (p = 0; p < 8; p++) {
		d = abi_registers_integer_argument[p];
		/* save integer argument registers */
		M_ILD(d, REG_SP, LA_SIZE + 4 * 8 + 4 + p * 4);
	}
#endif

	M_ALD(REG_ZERO, REG_SP, stack_size + LA_LR_OFFSET);
	M_MTLR(REG_ZERO);
	M_LDA(REG_SP, REG_SP, stack_size);

	/* mark trace code */

	M_NOP;
#endif /* !defined(NDEBUG) */
}


/* emit_verbosecall_exit *******************************************************

   Generates the code for the call trace.

   void builtin_verbosecall_exit(s8 l, double d, float f, methodinfo *m);

*******************************************************************************/

void emit_verbosecall_exit(jitdata *jd)
{
#if !defined(NDEBUG)
	methodinfo   *m;
	codegendata  *cd;
	registerdata *rd;
	methoddesc   *md;
	s4            disp;

	if (!JITDATA_HAS_FLAG_VERBOSECALL(jd))
		return;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;
	rd = jd->rd;

	md = m->parseddesc;
	
	/* mark trace code */

	M_NOP;

	M_MFLR(REG_ZERO);
	M_AST(REG_ZERO, REG_SP, LA_LR_OFFSET);
	M_STWU(REG_SP, REG_SP, -(LA_SIZE + (1 + 2 + 2 + 1 + 4) * 4));

	/* save return registers */

	M_LST(REG_RESULT_PACKED, REG_SP, LA_SIZE + (1 + 2 + 2 + 1 + 0) * 4);
	M_DST(REG_FRESULT, REG_SP, LA_SIZE + (1 + 2 + 2 + 1 + 2) * 4);

	/* keep this order */
	switch (md->returntype.type) {
	case TYPE_INT:
	case TYPE_ADR:
		M_INTMOVE(REG_RESULT, REG_A1);
		M_CLR(REG_A0);
		break;

	case TYPE_LNG:
		M_LNGMOVE(REG_RESULT_PACKED, REG_A0_A1_PACKED);
		break;
	}

	M_FLTMOVE(REG_FRESULT, REG_FA0);
	M_FLTMOVE(REG_FRESULT, REG_FA1);

	disp = dseg_add_address(cd, m);
	M_ALD(REG_A2, REG_PV, disp);

	disp = dseg_add_functionptr(cd, builtin_verbosecall_exit);
	M_ALD(REG_ITMP2, REG_PV, disp);
	M_MTCTR(REG_ITMP2);
	M_JSR;

	/* restore return registers */

	M_LLD(REG_RESULT_PACKED, REG_SP, LA_SIZE + (1 + 2 + 2 + 1 + 0) * 4);
	M_DLD(REG_FRESULT, REG_SP, LA_SIZE + (1 + 2 + 2 + 1 + 2) * 4);

	M_ALD(REG_ZERO, REG_SP, LA_SIZE + (1 + 2 + 2 + 1 + 4) * 4 + LA_LR_OFFSET);
	M_MTLR(REG_ZERO);
	M_LDA(REG_SP, REG_SP, LA_SIZE + (1 + 2 + 2 + 1 + 4) * 4);

	/* mark trace code */

	M_NOP;
#endif /* !defined(NDEBUG) */
}


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

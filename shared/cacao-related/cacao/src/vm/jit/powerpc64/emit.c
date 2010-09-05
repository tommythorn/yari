/* src/vm/jit/powerpc64/emit.c - PowerPC64 code emitter functions

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

   $Id: emitfuncs.c 4398 2006-01-31 23:43:08Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "md-abi.h"
#include "vm/jit/powerpc64/codegen.h"

#include "threads/lock-common.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/vm.h"

#include "vm/jit/abi.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/emit-common.h"
#include "vm/jit/jit.h"

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

		if (IS_FLT_DBL_TYPE(src->type)) {
			M_DLD(tempreg, REG_SP, disp);
		}
		else {
			M_LLD(tempreg, REG_SP, disp);
		}

		reg = tempreg;
	}
	else
		reg = src->vv.regoff;

	return reg;
}


/* emit_store ******************************************************************

   Emits a possible store to a variable.

*******************************************************************************/

void emit_store(jitdata *jd, instruction *iptr, varinfo *dst, s4 d)
{
	codegendata  *cd;

	/* get required compiler data */

	cd = jd->cd;

	if (dst->flags & INMEMORY) {
		COUNT_SPILLS;

		if (IS_FLT_DBL_TYPE(dst->type)) {
			M_DST(d, REG_SP, dst->vv.regoff * 8);
		}
		else {
			M_LST(d, REG_SP, dst->vv.regoff * 8);
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
			if (IS_FLT_DBL_TYPE(src->type))
				M_FMOV(s1, d);
			else
				M_MOV(s1, d);
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

	if ((value >= -32768) && (value <= 32767)) {
		M_LDA_INTERN(d, REG_ZERO, value);
	} else {
		disp = dseg_add_s4(cd, value);
		M_ILD(d, REG_PV, disp);
	}
}

void emit_lconst(codegendata *cd, s4 d, s8 value)
{
	s4 disp;
	if ((value >= -32768) && (value <= 32767)) {
		M_LDA_INTERN(d, REG_ZERO, value);
	} else {
		disp = dseg_add_s8(cd, value);
		M_LLD(d, REG_PV, disp);
	}
}


/* emit_verbosecall_enter ******************************************************

   Generates the code for the call trace.

*******************************************************************************/

void emit_verbosecall_enter (jitdata *jd)
{
	methodinfo   *m;
	codegendata  *cd;
	registerdata *rd;
	s4 s1, p, t;
	int stack_size;
	methoddesc *md;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;
	rd = jd->rd;

	md = m->parseddesc;
	
	/* Build up Stackframe for builtin_trace_args call (a multiple of 16) */
	/* For Darwin:                                                        */
	/* TODO                                                               */
	/* For Linux:                                                         */
	/* setup stack for TRACE_ARGS_NUM registers                           */
	/* == LA_SIZE + PA_SIZE + 8 (methodinfo argument) + TRACE_ARGS_NUM*8 + 8 (itmp1)              */
	
	/* in nativestubs no Place to save the LR (Link Register) would be needed */
	/* but since the stack frame has to be aligned the 4 Bytes would have to  */
	/* be padded again */

#if defined(__DARWIN__)
	stack_size = LA_SIZE + (TRACE_ARGS_NUM + 1) * 8;
#else
	stack_size = LA_SIZE + PA_SIZE + 8 + TRACE_ARGS_NUM * 8 + 8;
#endif

	/* mark trace code */
	M_NOP;

	M_MFLR(REG_ZERO);
	M_AST(REG_ZERO, REG_SP, LA_LR_OFFSET);
	M_STDU(REG_SP, REG_SP, -stack_size);

	for (p = 0; p < md->paramcount && p < TRACE_ARGS_NUM; p++) {
		t = md->paramtypes[p].type;
		if (IS_INT_LNG_TYPE(t)) {
			if (!md->params[p].inmemory) { /* Param in Arg Reg */
				M_LST(md->params[p].regoff, REG_SP, LA_SIZE + PA_SIZE + 8 + p * 8);
			} else { /* Param on Stack */
				s1 = (md->params[p].regoff + cd->stackframesize) * 8 + stack_size;
				M_LLD(REG_ITMP2, REG_SP, s1);
				M_LST(REG_ITMP2, REG_SP, LA_SIZE + PA_SIZE + 8 + p * 8);
			}
		} else { /* IS_FLT_DBL_TYPE(t) */
			if (!md->params[p].inmemory) { /* in Arg Reg */
				s1 = md->params[p].regoff;
				M_DST(s1, REG_SP, LA_SIZE + PA_SIZE + 8 + p * 8);
			} else { /* on Stack */
				/* this should not happen */
				assert(0);
			}
		}
	}

#if defined(__DARWIN__)
	#warning "emit_verbosecall_enter not implemented"
#else
	/* LINUX */
	/* Set integer and float argument registers for trace_args call */
	/* offset to saved integer argument registers                   */
	for (p = 0; (p < TRACE_ARGS_NUM) && (p < md->paramcount); p++) {
		t = md->paramtypes[p].type;
		if (IS_INT_LNG_TYPE(t)) {
			M_LLD(abi_registers_integer_argument[p], REG_SP,LA_SIZE + PA_SIZE + 8 + p * 8);
		} else { /* Float/Dbl */
			if (!md->params[p].inmemory) { /* Param in Arg Reg */
				/* use reserved Place on Stack (sp + 5 * 16) to copy  */
				/* float/double arg reg to int reg                    */
				s1 = md->params[p].regoff;
				M_MOV(s1, abi_registers_integer_argument[p]);
			} else	{
				assert(0);
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
	if (TRACE_ARGS_NUM == 8)	{
		/* need to pass via stack */
		M_AST(REG_ITMP1, REG_SP, LA_SIZE + PA_SIZE);
	} else {
		/* pass via register, reg 3 is the first  */
		M_MOV(REG_ITMP1, 3 + TRACE_ARGS_NUM);
	}
#endif
	/* call via function descriptor */
	/* XXX: what about TOC? */
	p = dseg_add_functionptr(cd, builtin_verbosecall_enter);
	M_ALD(REG_ITMP2, REG_PV, p);
	M_ALD(REG_ITMP1, REG_ITMP2, 0);
	M_MTCTR(REG_ITMP1);
	M_JSR;

#if defined(__DARWIN__)
	#warning "emit_verbosecall_enter not implemented"
#else
	/* LINUX */
	for (p = 0; p < md->paramcount && p < TRACE_ARGS_NUM; p++) {
		t = md->paramtypes[p].type;
		if (IS_INT_LNG_TYPE(t))	{
			if (!md->params[p].inmemory) { /* Param in Arg Reg */
				/* restore integer argument registers */
				M_LLD(abi_registers_integer_argument[p], REG_SP, LA_SIZE + PA_SIZE + 8 + p * 8);
			} else {
				assert(0);	/* TODO: implement this */
			}
		} else { /* FLT/DBL */
			if (!md->params[p].inmemory) { /* Param in Arg Reg */
				M_DLD(md->params[p].regoff, REG_SP, LA_SIZE + PA_SIZE + 8 + p * 8);
			} else {
				assert(0); /* this shoudl never happen */
			}
			
		}
	}
#endif
	M_ALD(REG_ZERO, REG_SP, stack_size + LA_LR_OFFSET);
	M_MTLR(REG_ZERO);
	M_LDA(REG_SP, REG_SP, stack_size);

	/* mark trace code */
	M_NOP;
}


/* emit_verbosecall_exit ******************************************************

   Generates the code for the call trace.

   void builtin_verbosecall_exit(s8 l, double d, float f, methodinfo *m);

*******************************************************************************/

void emit_verbosecall_exit(jitdata *jd)
{
	methodinfo   *m;
	codegendata  *cd;
	s4            disp;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;

	/* mark trace code */

	M_NOP;

	M_MFLR(REG_ZERO);
	M_LDA(REG_SP, REG_SP, -(LA_SIZE+PA_SIZE+10*8));
	M_DST(REG_FRESULT, REG_SP, LA_SIZE+PA_SIZE+0*8);
	M_LST(REG_RESULT, REG_SP, LA_SIZE+PA_SIZE+1*8);
	M_AST(REG_ZERO, REG_SP, LA_SIZE+PA_SIZE+2*8);

	M_MOV(REG_RESULT, REG_A0);

	M_FLTMOVE(REG_FRESULT, REG_FA0);
	M_FLTMOVE(REG_FRESULT, REG_FA1);

	disp = dseg_add_address(cd, m);
	M_ALD(REG_A3, REG_PV, disp);

	disp = dseg_add_functionptr(cd, builtin_verbosecall_exit);
	/* call via function descriptor, XXX: what about TOC ? */
	M_ALD(REG_ITMP2, REG_PV, disp);
	M_ALD(REG_ITMP2, REG_ITMP2, 0);
	M_MTCTR(REG_ITMP2);
	M_JSR;

	M_DLD(REG_FRESULT, REG_SP, LA_SIZE+PA_SIZE+0*8);
	M_LLD(REG_RESULT, REG_SP, LA_SIZE+PA_SIZE+1*8);
	M_ALD(REG_ZERO, REG_SP, LA_SIZE+PA_SIZE+2*8);
	M_LDA(REG_SP, REG_SP, LA_SIZE+PA_SIZE+10*8);
	M_MTLR(REG_ZERO);

	/* mark trace code */

	M_NOP;
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

		if ((checkdisp < (s4) 0xffff8000) || (checkdisp > (s4) 0x00007fff)) {
			/* if the long-branches flag isn't set yet, do it */

			if (!CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd)) {
				log_println("setting error");
				cd->flags |= (CODEGENDATA_FLAG_ERROR |
							  CODEGENDATA_FLAG_LONGBRANCHES);
			}
			log_println("generating long-branch");

			branchdisp --;		/* we jump from the second instruction */
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

/* emit_arrayindexoutofbounds_check ********************************************

   Emit a ArrayIndexOutOfBoundsException check.

*******************************************************************************/

void emit_arrayindexoutofbounds_check(codegendata *cd, instruction *iptr, s4 s1, s4 s2)
{
	if (checkbounds) {
#define SOFTEX 0
#if SOFTEX
		M_ILD(REG_ITMP3, s1, OFFSET(java_arrayheader, size));
		M_CMPU(s2, REG_ITMP3);
		codegen_add_arrayindexoutofboundsexception_ref(cd, s2);
		BRANCH_NOPS;
#else
		M_ILD(REG_ITMP3, s1, OFFSET(java_arrayheader, size));
		M_CMPU(s2, REG_ITMP3);
		M_BLT(1);
		/* ALD is 4 byte aligned, ILD 2, onyl LWZ is byte aligned */
		M_LWZ(s2, REG_ZERO, EXCEPTION_HARDWARE_ARRAYINDEXOUTOFBOUNDS);
#endif
	}
}


/* emit_arithmetic_check *******************************************************

   Emit an ArithmeticException check.

*******************************************************************************/

void emit_arithmetic_check(codegendata *cd, instruction *iptr, s4 reg)
{
	if (INSTRUCTION_MUST_CHECK(iptr))	{
	#if SOFTEX
		M_TST(reg);
		codegen_add_arithmeticexception_ref(cd);
		BRANCH_NOPS;
	#else
		M_TST(reg);
		M_BNE(1);
		/* ALD is 4 byte aligned, ILD 2, onyl LWZ is byte aligned */
		M_LWZ(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_ARITHMETIC);
	#endif
	}
}

#if 0
/* emit_arraystore_check *******************************************************

   Emit an ArrayStoreException check.

*******************************************************************************/

void emit_arraystore_check(codegendata *cd, instruction *iptr, s4 reg)
{
	if (INSTRUCTION_MUST_CHECK(iptr))	{
		M_TST(REG_RESULT);
		codegen_add_arraystoreexception_ref(cd);
		BRANCH_NOPS;
	}
}
#endif

/* emit_classcast_check ********************************************************

   Emit a ClassCastException check.

*******************************************************************************/

void emit_classcast_check(codegendata *cd, instruction *iptr, s4 condition, s4 reg, s4 s1)
{
	if (INSTRUCTION_MUST_CHECK(iptr))	{
	#if SOFTEX
		codegen_add_classcastexception_ref(cd, condition, s1);
		BRANCH_NOPS;
		M_NOP;
	#else
		switch(condition)	{
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
		/* ALD is 4 byte aligned, ILD 2, onyl LWZ is byte aligned */
		M_LWZ(s1, REG_ZERO, EXCEPTION_HARDWARE_CLASSCAST);
	#endif
	}
}


/* emit_nullpointer_check ******************************************************

   Emit a NullPointerException check.

*******************************************************************************/

void emit_nullpointer_check(codegendata *cd, instruction *iptr, s4 reg)
{
	if (INSTRUCTION_MUST_CHECK(iptr))	{
		M_TST(reg);
		M_BNE(1);
		/* ALD is 4 byte aligned, ILD 2, onyl LWZ is byte aligned */
		M_LWZ(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_NULLPOINTER);
	}
}

/* emit_exception_check ********************************************************

   Emit an Exception check.

*******************************************************************************/

void emit_exception_check(codegendata *cd, instruction *iptr)
{
	if (INSTRUCTION_MUST_CHECK(iptr))	{
	#if SOFTEX
		M_CMPI(REG_RESULT, 0);
		codegen_add_fillinstacktrace_ref(cd);
		BRANCH_NOPS;
	#else
		M_TST(REG_RESULT);
		M_BNE(1);
		/* ALD is 4 byte aligned, ILD 2, onyl LWZ is byte aligned */
		M_LWZ(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_EXCEPTION);
	#endif
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

	cd = jd->cd;

	/* generate code patching stub call code */

	targetdisp = 0;

	for (pref = cd->patchrefs; pref != NULL; pref = pref->next) {
		/* check code segment size */

		MCODECHECK(16);

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
		M_AADD_IMM(REG_SP, -8 * 8, REG_SP);

		/* calculate return address and move it onto the stack */
		M_LDA(REG_ITMP3, REG_PV, pref->branchpos);
		M_AST_INTERN(REG_ITMP3, REG_SP, 5 * 8);

		/* move pointer to java_objectheader onto stack */

#if defined(ENABLE_THREADS)
		/* order reversed because of data segment layout */

		(void) dseg_add_unique_address(cd, NULL);                         /* flcword    */
		(void) dseg_add_unique_address(cd, lock_get_initial_lock_word()); /* monitorPtr */
		disp = dseg_add_unique_address(cd, NULL);                         /* vftbl      */

		M_LDA(REG_ITMP3, REG_PV, disp);
		M_AST_INTERN(REG_ITMP3, REG_SP, 4 * 8);
#else
		/* do nothing */
#endif

		/* move machine code onto stack */
		disp = dseg_add_s4(cd, mcode);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST_INTERN(REG_ITMP3, REG_SP, 3 * 8);

		/* move class/method/field reference onto stack */
		disp = dseg_add_address(cd, pref->ref);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST_INTERN(REG_ITMP3, REG_SP, 2 * 8);

		/* move data segment displacement onto stack */
		disp = dseg_add_s4(cd, pref->disp);
		M_ILD(REG_ITMP3, REG_PV, disp);
		M_IST_INTERN(REG_ITMP3, REG_SP, 1 * 8);

		/* move patcher function pointer onto stack */
		disp = dseg_add_functionptr(cd, pref->patcher);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST_INTERN(REG_ITMP3, REG_SP, 0 * 8);

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
	rplpoint    *replacementpoint;
	s4           disp;
	s4           i;
#if !defined(NDEBUG)
	u1          *savedmcodeptr;
#endif

	/* get required compiler data */

	cd   = jd->cd;
	code = jd->code;

	replacementpoint = jd->code->rplpoints;

	for (i = 0; i < code->rplpointcount; ++i, ++replacementpoint) {
		/* do not generate stubs for non-trappable points */

		if (replacementpoint->flags & RPLPOINT_FLAG_NOTRAP)
			continue;


		/* check code segment size */

		MCODECHECK(100);

#if !defined(NDEBUG)
		savedmcodeptr = cd->mcodeptr;
#endif
		/* create stack frame - keep 16-byte aligned */

		M_AADD_IMM(REG_SP, -4 * 8, REG_SP);

		/* push address of `rplpoint` struct */

		disp = dseg_add_address(cd, replacementpoint);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST_INTERN(REG_ITMP3, REG_SP, 0 * 8);

		/* jump to replacement function */

		disp = dseg_add_functionptr(cd, asm_replacement_out);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_MTCTR(REG_ITMP3);
		M_RTS;

		assert((cd->mcodeptr - savedmcodeptr) == 4*REPLACEMENT_STUB_SIZE);

#if 0
		/* note start of stub code */

		replacementpoint->outcode = (u1 *) (cd->mcodeptr - cd->mcodebase);

		/* make machine code for patching */

		savedmcodeptr  = cd->mcodeptr;
		cd->mcodeptr = (u1 *) &(replacementpoint->mcode) + 1 /* big-endian */;

		disp = (ptrint)((s4*)replacementpoint->outcode - (s4*)replacementpoint->pc) - 1;
		M_BR(disp);

		cd->mcodeptr = savedmcodeptr;

		/* create stack frame - keep 16-byte aligned */

		M_AADD_IMM(REG_SP, -4 * 4, REG_SP);

		/* push address of `rplpoint` struct */

		disp = dseg_add_unique_address(cd, replacementpoint);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_AST_INTERN(REG_ITMP3, REG_SP, 0 * 4);

		/* jump to replacement function */

		disp = dseg_add_functionptr(cd, asm_replacement_out);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_MTCTR(REG_ITMP3);
		M_RTS;
#endif
	}
}
#endif /* define(ENABLE_REPLACEMENT) */

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

/* src/vm/jit/powerpc/codegen.c - machine code generator for 32-bit PowerPC

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

   $Id: codegen.c 7864 2007-05-03 21:17:26Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <signal.h>

#include "vm/types.h"

#include "md-abi.h"

#include "vm/jit/powerpc/arch.h"
#include "vm/jit/powerpc/codegen.h"

#include "mm/memory.h"

#include "native/native.h"

#include "threads/lock-common.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit/abi.h"
#include "vm/jit/abi-asm.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/codegen-common.h"
#include "vm/jit/dseg.h"
#include "vm/jit/emit-common.h"
#include "vm/jit/jit.h"
#include "vm/jit/md.h"
#include "vm/jit/methodheader.h"
#include "vm/jit/parse.h"
#include "vm/jit/patcher.h"
#include "vm/jit/reg.h"
#include "vm/jit/replace.h"
#include "vm/jit/stacktrace.h"

#if defined(ENABLE_LSRA)
# include "vm/jit/allocator/lsra.h"
#endif

#include "vmcore/loader.h"
#include "vmcore/options.h"


/* codegen *********************************************************************

   Generates machine code.

*******************************************************************************/

bool codegen_emit(jitdata *jd)
{
	methodinfo         *m;
	codeinfo           *code;
	codegendata        *cd;
	registerdata       *rd;
	s4                  len, s1, s2, s3, d, disp;
	ptrint              a;
	varinfo            *var;
	basicblock         *bptr;
	instruction        *iptr;
	exception_entry    *ex;
	u2                  currentline;
	methodinfo         *lm;             /* local methodinfo for ICMD_INVOKE*  */
	unresolved_method  *um;
	builtintable_entry *bte;
	methoddesc         *md;
	fieldinfo          *fi;
	unresolved_field   *uf;
	s4                  fieldtype;
	s4                 varindex;

	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;
	rd   = jd->rd;

	/* prevent compiler warnings */

	d         = 0;
	fieldtype = 0;
	lm        = NULL;
	um        = NULL;
	uf        = NULL;
	bte       = NULL;

	{
	s4 i, p, t, l;
	s4 savedregs_num;

	savedregs_num = 0;

	/* space to save used callee saved registers */

	savedregs_num += (INT_SAV_CNT - rd->savintreguse);
	savedregs_num += (FLT_SAV_CNT - rd->savfltreguse) * 2;

	cd->stackframesize = rd->memuse + savedregs_num;

#if defined(ENABLE_THREADS)
	/* Space to save argument of monitor_enter and Return Values to
	   survive monitor_exit. The stack position for the argument can
	   not be shared with place to save the return register on PPC,
	   since both values reside in R3. */

	if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
		/* reserve 2 slots for long/double return values for monitorexit */

		if (IS_2_WORD_TYPE(m->parseddesc->returntype.type))
			cd->stackframesize += 3;
		else
			cd->stackframesize += 2;
	}

#endif

	/* create method header */

	/* align stack to 16-bytes */

	if (!jd->isleafmethod || JITDATA_HAS_FLAG_VERBOSECALL(jd))
		cd->stackframesize = (cd->stackframesize + 3) & ~3;

	else if (jd->isleafmethod && (cd->stackframesize == LA_SIZE_IN_POINTERS))
		cd->stackframesize = 0;

	(void) dseg_add_unique_address(cd, code);              /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 4); /* FrameSize       */

#if defined(ENABLE_THREADS)
	/* IsSync contains the offset relative to the stack pointer for the
	   argument of monitor_exit used in the exception handler. Since the
	   offset could be zero and give a wrong meaning of the flag it is
	   offset by one.
	*/

	if (checksync && (m->flags & ACC_SYNCHRONIZED))
		(void) dseg_add_unique_s4(cd, (rd->memuse + 1) * 4);/* IsSync         */
	else
#endif
		(void) dseg_add_unique_s4(cd, 0);                  /* IsSync          */
	                                       
	(void) dseg_add_unique_s4(cd, jd->isleafmethod);       /* IsLeaf          */
	(void) dseg_add_unique_s4(cd, INT_SAV_CNT - rd->savintreguse); /* IntSave */
	(void) dseg_add_unique_s4(cd, FLT_SAV_CNT - rd->savfltreguse); /* FltSave */

	dseg_addlinenumbertablesize(cd);

	(void) dseg_add_unique_s4(cd, jd->exceptiontablelength); /* ExTableSize   */

	/* create exception table */

	for (ex = jd->exceptiontable; ex != NULL; ex = ex->down) {
		dseg_add_target(cd, ex->start);
   		dseg_add_target(cd, ex->end);
		dseg_add_target(cd, ex->handler);
		(void) dseg_add_unique_address(cd, ex->catchtype.any);
	}

#if defined(ENABLE_PROFILING)
	/* generate method profiling code */

	if (JITDATA_HAS_FLAG_INSTRUMENT(jd)) {
		/* count frequency */

		M_ALD(REG_ITMP1, REG_PV, CodeinfoPointer);
		M_ALD(REG_ITMP2, REG_ITMP1, OFFSET(codeinfo, frequency));
		M_IADD_IMM(REG_ITMP2, 1, REG_ITMP2);
		M_AST(REG_ITMP2, REG_ITMP1, OFFSET(codeinfo, frequency));

/* 		PROFILE_CYCLE_START; */
	}
#endif

	/* create stack frame (if necessary) */

	if (!jd->isleafmethod) {
		M_MFLR(REG_ZERO);
		M_AST(REG_ZERO, REG_SP, LA_LR_OFFSET);
	}

	if (cd->stackframesize)
		M_STWU(REG_SP, REG_SP, -(cd->stackframesize * 4));

	/* save return address and used callee saved registers */

	p = cd->stackframesize;
	for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
		p--; M_IST(rd->savintregs[i], REG_SP, p * 4);
	}
	for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
		p -= 2; M_DST(rd->savfltregs[i], REG_SP, p * 4);
	}

	/* take arguments out of register or stack frame */

	md = m->parseddesc;

 	for (p = 0, l = 0; p < md->paramcount; p++) {
 		t = md->paramtypes[p].type;
		varindex = jd->local_map[l * 5 + t];

 		l++;
 		if (IS_2_WORD_TYPE(t))    /* increment local counter for 2 word types */
 			l++;

		if (varindex == UNUSED)
			continue;

		var = VAR(varindex);
		s1  = md->params[p].regoff;

		if (IS_INT_LNG_TYPE(t)) {
 			if (!md->params[p].inmemory) {
 				if (!IS_INMEMORY(var->flags)) {
					if (IS_2_WORD_TYPE(t))
						M_LNGMOVE(s1, var->vv.regoff);
					else
						M_INTMOVE(s1, var->vv.regoff);
				}
				else {
					if (IS_2_WORD_TYPE(t))
						M_LST(s1, REG_SP, var->vv.regoff * 4);
					else
						M_IST(s1, REG_SP, var->vv.regoff * 4);
				}
			}
			else {
 				if (!IS_INMEMORY(var->flags)) {
					if (IS_2_WORD_TYPE(t))
						M_LLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 4);
					else
						M_ILD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 4);
				}
				else {
#if 1
 					M_ILD(REG_ITMP1, REG_SP, (cd->stackframesize + s1) * 4);
 					M_IST(REG_ITMP1, REG_SP, var->vv.regoff * 4);
					if (IS_2_WORD_TYPE(t)) {
						M_ILD(REG_ITMP1, REG_SP, (cd->stackframesize + s1) * 4 +4);
						M_IST(REG_ITMP1, REG_SP, var->vv.regoff * 4 + 4);
					}
#else
					/* Reuse Memory Position on Caller Stack */
					var->vv.regoff = cd->stackframesize + s1;
#endif
				}
			}
		}
		else {
 			if (!md->params[p].inmemory) {
 				if (!IS_INMEMORY(var->flags))
 					M_FLTMOVE(s1, var->vv.regoff);
				else {
					if (IS_2_WORD_TYPE(t))
						M_DST(s1, REG_SP, var->vv.regoff * 4);
					else
						M_FST(s1, REG_SP, var->vv.regoff * 4);
 				}
 			}
			else {
 				if (!IS_INMEMORY(var->flags)) {
					if (IS_2_WORD_TYPE(t))
						M_DLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 4);
					else
						M_FLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 4);
 				}
				else {
#if 1
					if (IS_2_WORD_TYPE(t)) {
						M_DLD(REG_FTMP1, REG_SP, (cd->stackframesize + s1) * 4);
						M_DST(REG_FTMP1, REG_SP, var->vv.regoff * 4);
						var->vv.regoff = cd->stackframesize + s1;

					} else {
						M_FLD(REG_FTMP1, REG_SP, (cd->stackframesize + s1) * 4);
						M_FST(REG_FTMP1, REG_SP, var->vv.regoff * 4);
					}
#else
					/* Reuse Memory Position on Caller Stack */
					var->vv.regoff = cd->stackframesize + s1;
#endif
				}
			}
		}
	}

#if defined(ENABLE_THREADS)
	/* call monitorenter function */

	if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
		/* stack offset for monitor argument */

		s1 = rd->memuse;

# if !defined(NDEBUG)
		if (JITDATA_HAS_FLAG_VERBOSECALL(jd)) {
			M_AADD_IMM(REG_SP, -((LA_SIZE_IN_POINTERS + ARG_CNT) * 8), REG_SP);

			for (p = 0; p < INT_ARG_CNT; p++)
				M_IST(abi_registers_integer_argument[p], REG_SP, LA_SIZE + p * 8);

			for (p = 0; p < FLT_ARG_CNT; p++)
				M_DST(abi_registers_float_argument[p], REG_SP, LA_SIZE + (INT_ARG_CNT + p) * 8);

			/* ATTENTION: We multiply here with 2, because we use * 8
			   above for simplicity and below * 4! */

			s1 += (LA_SIZE_IN_POINTERS + ARG_CNT) * 2;
		}
# endif

		disp = dseg_add_functionptr(cd, LOCK_monitor_enter);
		M_ALD(REG_ITMP3, REG_PV, disp);
		M_MTCTR(REG_ITMP3);

		/* get or test the lock object */

		if (m->flags & ACC_STATIC) {
			disp = dseg_add_address(cd, &m->class->object.header);
			M_ALD(REG_A0, REG_PV, disp);
		}
		else {
			M_TST(REG_A0);
			M_BNE(1);
			M_ALD_INTERN(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_NULLPOINTER);
		}

		M_AST(REG_A0, REG_SP, s1 * 4);
		M_JSR;

# if !defined(NDEBUG)
		if (JITDATA_HAS_FLAG_VERBOSECALL(jd)) {
			for (p = 0; p < INT_ARG_CNT; p++)
				M_ILD(abi_registers_integer_argument[p], REG_SP, LA_SIZE + p * 8);

			for (p = 0; p < FLT_ARG_CNT; p++)
				M_DLD(abi_registers_float_argument[p], REG_SP, LA_SIZE + (INT_ARG_CNT + p) * 8);

			M_AADD_IMM(REG_SP, (LA_SIZE_IN_POINTERS + ARG_CNT) * 8, REG_SP);
		}
# endif
	}
#endif /* defined(ENABLE_THREADS) */

	/* call trace function */

	emit_verbosecall_enter(jd);
	}

	/* end of header generation */

	/* create replacement points */

	REPLACEMENT_POINTS_INIT(cd, jd);

	/* walk through all basic blocks */

	for (bptr = jd->basicblocks; bptr != NULL; bptr = bptr->next) {

		bptr->mpc = (s4) (cd->mcodeptr - cd->mcodebase);

		if (bptr->flags >= BBREACHED) {
		/* branch resolving */

		codegen_resolve_branchrefs(cd, bptr);

		/* handle replacement points */

		REPLACEMENT_POINT_BLOCK_START(cd, bptr);

#if defined(ENABLE_PROFILING)
		/* generate basicblock profiling code */

		if (JITDATA_HAS_FLAG_INSTRUMENT(jd)) {
			/* count frequency */

			disp = dseg_add_address(cd, code->bbfrequency);
			M_ALD(REG_ITMP2, REG_PV, disp);
			M_ALD(REG_ITMP3, REG_ITMP2, bptr->nr * 4);
			M_IADD_IMM(REG_ITMP3, 1, REG_ITMP3);
			M_AST(REG_ITMP3, REG_ITMP2, bptr->nr * 4);

			/* if this is an exception handler, start profiling again */

/* 			if (bptr->type == BBTYPE_EXH) */
/* 				PROFILE_CYCLE_START; */
		}
#endif

		/* copy interface registers to their destination */

		len = bptr->indepth;
		MCODECHECK(64+len);

#if defined(ENABLE_LSRA)
		if (opt_lsra) {
			while (src != NULL) {
				len--;
				if ((len == bptr->indepth-1) && (bptr->type == BBTYPE_EXH)) {
					/* d = reg_of_var(m, src, REG_ITMP1); */
					if (!IS_INMEMORY(src->flags))
						d = src->vv.regoff;
					else
						d = REG_ITMP1;
					M_INTMOVE(REG_ITMP1, d);
					emit_store(jd, NULL, src, d);
				}
				src = src->prev;
			}
		} else {
#endif
		while (len > 0) {
			len--;
			var = VAR(bptr->invars[len]);
			if ((len == bptr->indepth-1) && (bptr->type == BBTYPE_EXH)) {
				d = codegen_reg_of_var(0, var, REG_ITMP1);
				M_INTMOVE(REG_ITMP1, d);
				emit_store(jd, NULL, var, d);
			}
			else {
				assert((var->flags & INOUT));
			}
		}

#if defined(ENABLE_LSRA)
		}
#endif
		/* walk through all instructions */
		
		len = bptr->icount;
		currentline = 0;

		for (iptr = bptr->iinstr; len > 0; len--, iptr++) {
			if (iptr->line != currentline) {
				dseg_addlinenumber(cd, iptr->line);
				currentline = iptr->line;
			}

			MCODECHECK(64);   /* an instruction usually needs < 64 words      */

		switch (iptr->opc) {
		case ICMD_NOP:        /* ...  ==> ...                                 */
		case ICMD_POP:        /* ..., value  ==> ...                          */
		case ICMD_POP2:       /* ..., value, value  ==> ...                   */
			break;

		case ICMD_INLINE_START:

			REPLACEMENT_POINT_INLINE_START(cd, iptr);
			break;

		case ICMD_INLINE_BODY:

			REPLACEMENT_POINT_INLINE_BODY(cd, iptr);
			dseg_addlinenumber_inline_start(cd, iptr);
			dseg_addlinenumber(cd, iptr->line);
			break;

		case ICMD_INLINE_END:

			dseg_addlinenumber_inline_end(cd, iptr);
			dseg_addlinenumber(cd, iptr->line);
			break;

		case ICMD_CHECKNULL:  /* ..., objectref  ==> ..., objectref           */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			emit_nullpointer_check(cd, iptr, s1);
			break;

		/* constant operations ************************************************/

		case ICMD_ICONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			ICONST(d, iptr->sx.val.i);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LCONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			LCONST(d, iptr->sx.val.l);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			a = dseg_add_float(cd, iptr->sx.val.f);
			M_FLD(d, REG_PV, a);
			emit_store_dst(jd, iptr, d);
			break;
			
		case ICMD_DCONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			a = dseg_add_double(cd, iptr->sx.val.d);
			M_DLD(d, REG_PV, a);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ACONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				constant_classref *cr = iptr->sx.val.c.ref;;

				disp = dseg_add_unique_address(cd, cr);

				codegen_addpatchref(cd, PATCHER_resolve_classref_to_classinfo,
									cr, disp);
			}
			else
				disp = dseg_add_address(cd, iptr->sx.val.anyptr);

			M_ALD(d, REG_PV, disp);
			emit_store_dst(jd, iptr, d);
			break;


		/* load/store/copy/move operations ************************************/

		case ICMD_ILOAD:      /* ...  ==> ..., content of local variable      */
		case ICMD_ALOAD:      /* s1 = local variable                          */
		case ICMD_LLOAD:
		case ICMD_FLOAD:  
		case ICMD_DLOAD:  
		case ICMD_ISTORE:     /* ..., value  ==> ...                          */
		case ICMD_LSTORE:
		case ICMD_FSTORE:
		case ICMD_DSTORE: 
		case ICMD_COPY:
		case ICMD_MOVE:
			
			emit_copy(jd, iptr);
			break;
	
		case ICMD_ASTORE:

			if (!(iptr->flags.bits & INS_FLAG_RETADDR))
				emit_copy(jd, iptr);
			break;


		/* integer operations *************************************************/

		case ICMD_INEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1); 
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_NEG(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_SUBFIC(GET_LOW_REG(s1), 0, GET_LOW_REG(d));
			M_SUBFZE(GET_HIGH_REG(s1), GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2L:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_INTMOVE(s1, GET_LOW_REG(d));
			M_SRA_IMM(GET_LOW_REG(d), 31, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_L2I:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2BYTE:   /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_BSEXT(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2CHAR:   /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_CZEXT(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2SHORT:  /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SSEXT(s1, d);
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_IADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_IADD(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		                      /* s1.localindex = variable, sx.val.i = constant*/

		case ICMD_IINC:
		case ICMD_IADDCONST:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			if ((iptr->sx.val.i >= -32768) && (iptr->sx.val.i <= 32767)) {
				M_IADD_IMM(s1, iptr->sx.val.i, d);
			} else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_IADD(s1, REG_ITMP2, d);
			}
			/* XXX the old code for ICMD_IINC was as follows:
			{
				u4 m = iptr->sx.val.i;
				if (m & 0x8000)
					m += 65536;
				if (m & 0xffff0000)
					M_ADDIS(s1, m >> 16, d);
				if (m & 0xffff)
					M_IADD_IMM(s1, m & 0xffff, d);
			}
			*/
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_ADDC(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);/* don't use REG_ITMP2*/
			M_ADDE(s1, s2, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADDCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.l = constant                          */

			s3 = iptr->sx.val.l & 0xffffffff;
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if ((s3 >= -32768) && (s3 <= 32767))
				M_ADDIC(s1, s3, GET_LOW_REG(d));
			else {
				ICONST(REG_ITMP2, s3);
				M_ADDC(s1, REG_ITMP2, GET_LOW_REG(d));
			}
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s3 = iptr->sx.val.l >> 32;
			if (s3 == -1)
				M_ADDME(s1, GET_HIGH_REG(d));
			else if (s3 == 0)
				M_ADDZE(s1, GET_HIGH_REG(d));
			else {
				ICONST(REG_ITMP3, s3);                 /* don't use REG_ITMP2 */
				M_ADDE(s1, REG_ITMP3, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_ISUB(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISUBCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= -32767) && (iptr->sx.val.i <= 32768))
				M_IADD_IMM(s1, -iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_ISUB(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_SUBC(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);/* don't use REG_ITMP2*/
			M_SUBE(s1, s2, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUBCONST:  /* ..., value  ==> ..., value - constant        */
		                      /* sx.val.l = constant                          */

			s3 = (-iptr->sx.val.l) & 0xffffffff;
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if ((s3 >= -32768) && (s3 <= 32767)) {
				M_ADDIC(s1, s3, GET_LOW_REG(d));
			} else {
				ICONST(REG_ITMP2, s3);
				M_ADDC(s1, REG_ITMP2, GET_LOW_REG(d));
			}
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s3 = (-iptr->sx.val.l) >> 32;
			if (s3 == -1)
				M_ADDME(s1, GET_HIGH_REG(d));
			else if (s3 == 0)
				M_ADDZE(s1, GET_HIGH_REG(d));
			else {
				ICONST(REG_ITMP3, s3);                 /* don't use REG_ITMP2 */
				M_ADDE(s1, REG_ITMP3, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			emit_arithmetic_check(cd, iptr, s2);
			M_LDAH(REG_ITMP3, REG_ZERO, 0x8000);
			M_CMP(REG_ITMP3, s1);
			M_BNE(3 + (s1 != d));
			M_CMPI(s2, -1);
			M_BNE(1 + (s1 != d));
			M_INTMOVE(s1, d);
			M_BR(1);
			M_IDIV(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arithmetic_check(cd, iptr, s2);
			M_LDAH(REG_ITMP3, REG_ZERO, 0x8000);
			M_CMP(REG_ITMP3, s1);
			M_BNE(4);
			M_CMPI(s2, -1);
			M_BNE(2);
			M_CLR(d);
			M_BR(3);
			M_IDIV(s1, s2, REG_ITMP3);
			M_IMUL(REG_ITMP3, s2, REG_ITMP3);
			M_ISUB(s1, REG_ITMP3, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
		case ICMD_LREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_A0_A1_PACKED);
			s2 = emit_load_s2(jd, iptr, REG_A2_A3_PACKED);

			/* XXX TODO: only do this if arithmetic check is really done! */
			M_OR_TST(GET_HIGH_REG(s2), GET_LOW_REG(s2), REG_ITMP3);
			/* XXX could be optimized */
			emit_arithmetic_check(cd, iptr, REG_ITMP3);

			bte = iptr->sx.s23.s3.bte;
			disp = dseg_add_functionptr(cd, bte->fp);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_MTCTR(REG_ITMP3);

			M_LNGMOVE(s1, REG_A0_A1_PACKED);
			M_LNGMOVE(s2, REG_A2_A3_PACKED);

			M_JSR;

			d = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(REG_RESULT_PACKED, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_IMUL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMULCONST:  /* ..., value  ==> ..., value * constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= -32768) && (iptr->sx.val.i <= 32767))
				M_IMUL_IMM(s1, iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP3, iptr->sx.val.i);
				M_IMUL(s1, REG_ITMP3, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIVPOW2:   /* ..., value  ==> ..., value << constant       */
		                      
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_SRA_IMM(s1, iptr->sx.val.i, d);
			M_ADDZE(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND_IMM(s2, 0x1f, REG_ITMP3);
			M_SLL(s1, REG_ITMP3, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHLCONST:  /* ..., value  ==> ..., value << constant       */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SLL_IMM(s1, iptr->sx.val.i & 0x1f, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHR:       /* ..., val1, val2  ==> ..., val1 >> val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND_IMM(s2, 0x1f, REG_ITMP3);
			M_SRA(s1, REG_ITMP3, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHRCONST:  /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SRA_IMM(s1, iptr->sx.val.i & 0x1f, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHR:      /* ..., val1, val2  ==> ..., val1 >>> val2      */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND_IMM(s2, 0x1f, REG_ITMP2);
			M_SRL(s1, REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.i & 0x1f)
				M_SRL_IMM(s1, iptr->sx.val.i & 0x1f, d);
			else {
				M_INTMOVE(s1, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IAND:       /* ..., val1, val2  ==> ..., val1 & val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 65535))
				M_AND_IMM(s1, iptr->sx.val.i, d);
			/*
			else if (iptr->sx.val.i == 0xffffff) {
				M_RLWINM(s1, 0, 8, 31, d);
				}
			*/
			else {
				ICONST(REG_ITMP3, iptr->sx.val.i);
				M_AND(s1, REG_ITMP3, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LAND:       /* ..., val1, val2  ==> ..., val1 & val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_AND(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);/* don't use REG_ITMP2*/
			M_AND(s1, s2, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.l = constant                          */

			s3 = iptr->sx.val.l & 0xffffffff;
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if ((s3 >= 0) && (s3 <= 65535))
				M_AND_IMM(s1, s3, GET_LOW_REG(d));
			else {
				ICONST(REG_ITMP3, s3);
				M_AND(s1, REG_ITMP3, GET_LOW_REG(d));
			}
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s3 = iptr->sx.val.l >> 32;
			if ((s3 >= 0) && (s3 <= 65535))
				M_AND_IMM(s1, s3, GET_HIGH_REG(d));
			else {
				ICONST(REG_ITMP3, s3);                 /* don't use REG_ITMP2 */
				M_AND(s1, REG_ITMP3, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_MOV(s1, REG_ITMP2);
			M_CMPI(s1, 0);
			M_BGE(1 + 2*(iptr->sx.val.i >= 32768));
			if (iptr->sx.val.i >= 32768) {
				M_ADDIS(REG_ZERO, iptr->sx.val.i >> 16, REG_ITMP2);
				M_OR_IMM(REG_ITMP2, iptr->sx.val.i, REG_ITMP2);
				M_IADD(s1, REG_ITMP2, REG_ITMP2);
			}
			else {
				M_IADD_IMM(s1, iptr->sx.val.i, REG_ITMP2);
			}
			{
				int b=0, m = iptr->sx.val.i;
				while (m >>= 1)
					++b;
				M_RLWINM(REG_ITMP2, 0, 0, 30-b, REG_ITMP2);
			}
			M_ISUB(s1, REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IOR:        /* ..., val1, val2  ==> ..., val1 | val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_OR(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 65535))
				M_OR_IMM(s1, iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP3, iptr->sx.val.i);
				M_OR(s1, REG_ITMP3, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LOR:       /* ..., val1, val2  ==> ..., val1 | val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_OR(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);/* don't use REG_ITMP2*/
			M_OR(s1, s2, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.l = constant                          */

			s3 = iptr->sx.val.l & 0xffffffff;
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if ((s3 >= 0) && (s3 <= 65535))
				M_OR_IMM(s1, s3, GET_LOW_REG(d));
			else {
				ICONST(REG_ITMP3, s3);
				M_OR(s1, REG_ITMP3, GET_LOW_REG(d));
			}
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s3 = iptr->sx.val.l >> 32;
			if ((s3 >= 0) && (s3 <= 65535))
				M_OR_IMM(s1, s3, GET_HIGH_REG(d));
			else {
				ICONST(REG_ITMP3, s3);                 /* don't use REG_ITMP2 */
				M_OR(s1, REG_ITMP3, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IXOR:       /* ..., val1, val2  ==> ..., val1 ^ val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_XOR(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IXORCONST:  /* ..., value  ==> ..., value ^ constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 65535))
				M_XOR_IMM(s1, iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP3, iptr->sx.val.i);
				M_XOR(s1, REG_ITMP3, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXOR:       /* ..., val1, val2  ==> ..., val1 ^ val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_XOR(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);/* don't use REG_ITMP2*/
			M_XOR(s1, s2, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXORCONST:  /* ..., value  ==> ..., value ^ constant        */
		                      /* sx.val.l = constant                          */

			s3 = iptr->sx.val.l & 0xffffffff;
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if ((s3 >= 0) && (s3 <= 65535))
				M_XOR_IMM(s1, s3, GET_LOW_REG(d));
			else {
				ICONST(REG_ITMP3, s3);
				M_XOR(s1, REG_ITMP3, GET_LOW_REG(d));
			}
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s3 = iptr->sx.val.l >> 32;
			if ((s3 >= 0) && (s3 <= 65535))
				M_XOR_IMM(s1, s3, GET_HIGH_REG(d));
			else {
				ICONST(REG_ITMP3, s3);                 /* don't use REG_ITMP2 */
				M_XOR(s1, REG_ITMP3, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LCMP:       /* ..., val1, val2  ==> ..., val1 cmp val2      */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			vm_abort("codegen: implement ICMD_LCMP!");
			emit_store_dst(jd, iptr, d);
			break;


		/* floating operations ************************************************/

		case ICMD_FNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_FMOVN(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_FMOVN(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_FADD(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_DADD(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_FSUB(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_DSUB(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_FMUL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_DMUL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_FDIV(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_DDIV(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;
		
		case ICMD_F2I:       /* ..., value  ==> ..., (int) value              */
		case ICMD_D2I:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_CLR(d);
			disp = dseg_add_float(cd, 0.0);
			M_FLD(REG_FTMP2, REG_PV, disp);
			M_FCMPU(s1, REG_FTMP2);
			M_BNAN(4);
			disp = dseg_add_unique_s4(cd, 0);
			M_CVTDL_C(s1, REG_FTMP1);
			M_LDA(REG_ITMP1, REG_PV, disp);
			M_STFIWX(REG_FTMP1, 0, REG_ITMP1);
			M_ILD(d, REG_PV, disp);
			emit_store_dst(jd, iptr, d);
			break;
		
		case ICMD_F2D:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_FLTMOVE(s1, d);
			emit_store_dst(jd, iptr, d);
			break;
					
		case ICMD_D2F:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_CVTDF(s1, d);
			emit_store_dst(jd, iptr, d);
			break;
		
		case ICMD_FCMPL:      /* ..., val1, val2  ==> ..., val1 fcmpg val2    */
		case ICMD_DCMPL:      /* == => 0, < => 1, > => -1                     */


			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_FCMPU(s2, s1);
			M_IADD_IMM(REG_ZERO, -1, d);
			M_BNAN(4);
			M_BGT(3);
			M_IADD_IMM(REG_ZERO, 0, d);
			M_BGE(1);
			M_IADD_IMM(REG_ZERO, 1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCMPG:      /* ..., val1, val2  ==> ..., val1 fcmpl val2    */
		case ICMD_DCMPG:      /* == => 0, < => 1, > => -1                     */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_FCMPU(s1, s2);
			M_IADD_IMM(REG_ZERO, 1, d);
			M_BNAN(4);
			M_BGT(3);
			M_IADD_IMM(REG_ZERO, 0, d);
			M_BGE(1);
			M_IADD_IMM(REG_ZERO, -1, d);
			emit_store_dst(jd, iptr, d);
			break;
			
		case ICMD_IF_FCMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_DCMPEQ:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			M_FCMPU(s1, s2);
			M_BNAN(1);
			emit_beq(cd, iptr->dst.block);
			break;

		case ICMD_IF_FCMPNE:    /* ..., value, value ==> ...                  */
		case ICMD_IF_DCMPNE:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			M_FCMPU(s1, s2);
			emit_bnan(cd, iptr->dst.block);
			emit_bne(cd, iptr->dst.block);
			break;


		case ICMD_IF_FCMPL_LT:  /* ..., value, value ==> ...                  */
		case ICMD_IF_DCMPL_LT:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			M_FCMPU(s1, s2);
			emit_bnan(cd, iptr->dst.block);
			emit_blt(cd, iptr->dst.block);
			break;

		case ICMD_IF_FCMPL_GT:  /* ..., value, value ==> ...                  */
		case ICMD_IF_DCMPL_GT:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			M_FCMPU(s1, s2);
			M_BNAN(1);
			emit_bgt(cd, iptr->dst.block);
			break;

		case ICMD_IF_FCMPL_LE:  /* ..., value, value ==> ...                  */
		case ICMD_IF_DCMPL_LE:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			M_FCMPU(s1, s2);
			emit_bnan(cd, iptr->dst.block);
			emit_ble(cd, iptr->dst.block);
			break;

		case ICMD_IF_FCMPL_GE:  /* ..., value, value ==> ...                  */
		case ICMD_IF_DCMPL_GE:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			M_FCMPU(s1, s2);
			M_BNAN(1);
			emit_bge(cd, iptr->dst.block);
			break;

		case ICMD_IF_FCMPG_LT:  /* ..., value, value ==> ...                  */
		case ICMD_IF_DCMPG_LT:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			M_FCMPU(s1, s2);
			M_BNAN(1);
			emit_blt(cd, iptr->dst.block);
			break;

		case ICMD_IF_FCMPG_GT:  /* ..., value, value ==> ...                  */
		case ICMD_IF_DCMPG_GT:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			M_FCMPU(s1, s2);
			emit_bnan(cd, iptr->dst.block);
			emit_bgt(cd, iptr->dst.block);
			break;

		case ICMD_IF_FCMPG_LE:  /* ..., value, value ==> ...                  */
		case ICMD_IF_DCMPG_LE:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			M_FCMPU(s1, s2);
			M_BNAN(1);
			emit_ble(cd, iptr->dst.block);
			break;

		case ICMD_IF_FCMPG_GE:  /* ..., value, value ==> ...                  */
		case ICMD_IF_DCMPG_GE:

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			M_FCMPU(s1, s2);
			emit_bnan(cd, iptr->dst.block);
			emit_bge(cd, iptr->dst.block);
			break;


		/* memory operations **************************************************/

		case ICMD_ARRAYLENGTH: /* ..., arrayref  ==> ..., length              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			M_ILD(d, s1, OFFSET(java_arrayheader, size));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_BALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_IADD_IMM(s2, OFFSET(java_bytearray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_LBZX(d, s1, REG_ITMP2);
			M_BSEXT(d, d);
			emit_store_dst(jd, iptr, d);
			break;			

		case ICMD_CALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_SLL_IMM(s2, 1, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_chararray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_LHZX(d, s1, REG_ITMP2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_SALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_SLL_IMM(s2, 1, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_shortarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_LHAX(d, s1, REG_ITMP2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_SLL_IMM(s2, 2, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_intarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_LWZX(d, s1, REG_ITMP2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_SLL_IMM(s2, 3, REG_ITMP2);
			M_IADD(s1, REG_ITMP2, REG_ITMP2);
			M_LLD_INTERN(d, REG_ITMP2, OFFSET(java_longarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_SLL_IMM(s2, 2, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_floatarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_LFSX(d, s1, REG_ITMP2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_SLL_IMM(s2, 3, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_doublearray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_LFDX(d, s1, REG_ITMP2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_AALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_SLL_IMM(s2, 2, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_objectarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_LWZX(d, s1, REG_ITMP2);
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_BASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_IADD_IMM(s2, OFFSET(java_bytearray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_STBX(s3, s1, REG_ITMP2);
			break;

		case ICMD_CASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_SLL_IMM(s2, 1, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_chararray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_STHX(s3, s1, REG_ITMP2);
			break;

		case ICMD_SASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_SLL_IMM(s2, 1, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_shortarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_STHX(s3, s1, REG_ITMP2);
			break;

		case ICMD_IASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_SLL_IMM(s2, 2, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_intarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_STWX(s3, s1, REG_ITMP2);
			break;

		case ICMD_LASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3_high(jd, iptr, REG_ITMP3);
			M_SLL_IMM(s2, 3, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_longarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_STWX(s3, s1, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, 4, REG_ITMP2);
			s3 = emit_load_s3_low(jd, iptr, REG_ITMP3);
			M_STWX(s3, s1, REG_ITMP2);
			break;

		case ICMD_FASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_FTMP3);
			M_SLL_IMM(s2, 2, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_floatarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_STFSX(s3, s1, REG_ITMP2);
			break;

		case ICMD_DASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_FTMP3);
			M_SLL_IMM(s2, 3, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_doublearray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_STFDX(s3, s1, REG_ITMP2);
			break;

		case ICMD_AASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_A0);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_A1);

			/* XXX what if array is NULL */
			disp = dseg_add_functionptr(cd, BUILTIN_canstore);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_MTCTR(REG_ITMP3);

			M_INTMOVE(s1, REG_A0);
			M_INTMOVE(s3, REG_A1);

			M_JSR;
			emit_exception_check(cd, iptr);

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_SLL_IMM(s2, 2, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_objectarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_STWX(s3, s1, REG_ITMP2);
			break;


		case ICMD_GETSTATIC:  /* ...  ==> ..., value                          */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = dseg_add_unique_address(cd, uf);

				codegen_addpatchref(cd, PATCHER_get_putstatic, uf, disp);
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = dseg_add_address(cd, &(fi->value));

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class))
					codegen_addpatchref(cd, PATCHER_initialize_class,
										fi->class, disp);
  			}

			M_ALD(REG_ITMP1, REG_PV, disp);
			switch (fieldtype) {
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD_INTERN(d, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
				M_ILD_INTERN(GET_LOW_REG(d), REG_ITMP1, 4);/* keep this order */
				M_ILD_INTERN(GET_HIGH_REG(d), REG_ITMP1, 0);/*keep this order */
				break;
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ALD_INTERN(d, REG_ITMP1, 0);
				break;
			case TYPE_FLT:
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_FLD_INTERN(d, REG_ITMP1, 0);
				break;
			case TYPE_DBL:				
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_DLD_INTERN(d, REG_ITMP1, 0);
				break;
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_PUTSTATIC:  /* ..., value  ==> ...                          */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = dseg_add_unique_address(cd, uf);

				codegen_addpatchref(cd, PATCHER_get_putstatic, uf, disp);
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = dseg_add_address(cd, &(fi->value));

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class))
					codegen_addpatchref(cd, PATCHER_initialize_class,
										fi->class, disp);
  			}

			M_ALD(REG_ITMP1, REG_PV, disp);
			switch (fieldtype) {
			case TYPE_INT:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_IST_INTERN(s1, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				s1 = emit_load_s1(jd, iptr, REG_ITMP23_PACKED);
				M_LST_INTERN(s1, REG_ITMP1, 0);
				break;
			case TYPE_ADR:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_AST_INTERN(s1, REG_ITMP1, 0);
				break;
			case TYPE_FLT:
				s1 = emit_load_s1(jd, iptr, REG_FTMP2);
				M_FST_INTERN(s1, REG_ITMP1, 0);
				break;
			case TYPE_DBL:
				s1 = emit_load_s1(jd, iptr, REG_FTMP2);
				M_DST_INTERN(s1, REG_ITMP1, 0);
				break;
			}
			break;


		case ICMD_GETFIELD:   /* ...  ==> ..., value                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = 0;

				codegen_addpatchref(cd, PATCHER_get_putfield, uf, 0);
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = fi->offset;
			}

			/* implicit null-pointer check */
			switch (fieldtype) {
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD(d, s1, disp);
				break;
			case TYPE_LNG:
   				d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
				if (GET_HIGH_REG(d) == s1) {
					M_ILD(GET_LOW_REG(d), s1, disp + 4);
					M_ILD(GET_HIGH_REG(d), s1, disp);
				}
				else {
					M_ILD(GET_HIGH_REG(d), s1, disp);
					M_ILD(GET_LOW_REG(d), s1, disp + 4);
				}
				break;
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ALD(d, s1, disp);
				break;
			case TYPE_FLT:
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_FLD(d, s1, disp);
				break;
			case TYPE_DBL:				
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_DLD(d, s1, disp);
				break;
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_PUTFIELD:   /* ..., value  ==> ...                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = 0;
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = fi->offset;
			}

			if (IS_INT_LNG_TYPE(fieldtype)) {
				if (IS_2_WORD_TYPE(fieldtype))
					s2 = emit_load_s2(jd, iptr, REG_ITMP23_PACKED);
				else
					s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			}
			else
				s2 = emit_load_s2(jd, iptr, REG_FTMP2);

			if (INSTRUCTION_IS_UNRESOLVED(iptr))
				codegen_addpatchref(cd, PATCHER_get_putfield, uf, 0);

			/* implicit null-pointer check */
			switch (fieldtype) {
			case TYPE_INT:
				M_IST(s2, s1, disp);
				break;
			case TYPE_LNG:
				M_IST(GET_LOW_REG(s2), s1, disp + 4);      /* keep this order */
				M_IST(GET_HIGH_REG(s2), s1, disp);         /* keep this order */
				break;
			case TYPE_ADR:
				M_AST(s2, s1, disp);
				break;
			case TYPE_FLT:
				M_FST(s2, s1, disp);
				break;
			case TYPE_DBL:
				M_DST(s2, s1, disp);
				break;
			}
			break;


		/* branch operations **************************************************/

		case ICMD_ATHROW:       /* ..., objectref ==> ... (, objectref)       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1_XPTR);

#ifdef ENABLE_VERIFIER
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_class *uc = iptr->sx.s23.s2.uc;

				codegen_addpatchref(cd, PATCHER_resolve_class, uc, 0);
			}
#endif /* ENABLE_VERIFIER */

			disp = dseg_add_functionptr(cd, asm_handle_exception);
			M_ALD(REG_ITMP2, REG_PV, disp);
			M_MTCTR(REG_ITMP2);

			if (jd->isleafmethod)
				M_MFLR(REG_ITMP3);                          /* save LR        */

			M_BL(0);                                        /* get current PC */
			M_MFLR(REG_ITMP2_XPC);

			if (jd->isleafmethod)
				M_MTLR(REG_ITMP3);                          /* restore LR     */

			M_RTS;                                          /* jump to CTR    */
			ALIGNCODENOP;
			break;

		case ICMD_GOTO:         /* ... ==> ...                                */
		case ICMD_RET:          /* ... ==> ...                                */

			emit_br(cd, iptr->dst.block);
			ALIGNCODENOP;
			break;

		case ICMD_JSR:          /* ... ==> ...                                */

			emit_br(cd, iptr->sx.s23.s3.jsrtarget.block);
			ALIGNCODENOP;
			break;

		case ICMD_IFNULL:       /* ..., value ==> ...                         */
		case ICMD_IFNONNULL:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_TST(s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IFNULL, BRANCH_OPT_NONE);
			break;

		case ICMD_IFLT:
		case ICMD_IFLE:
		case ICMD_IFNE:
		case ICMD_IFGT:
		case ICMD_IFGE:
		case ICMD_IFEQ:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if ((iptr->sx.val.i >= -32768) && (iptr->sx.val.i <= 32767))
				M_CMPI(s1, iptr->sx.val.i);
			else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_CMP(s1, REG_ITMP2);
			}
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IFEQ, BRANCH_OPT_NONE);
			break;


		case ICMD_IF_LEQ:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.l == 0) {
				M_OR_TST(s1, s2, REG_ITMP3);
  			}
			else if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff)) {
				M_XOR_IMM(s2, 0, REG_ITMP2);
				M_XOR_IMM(s1, iptr->sx.val.l & 0xffff, REG_ITMP1);
				M_OR_TST(REG_ITMP1, REG_ITMP2, REG_ITMP3);
  			}
			else {
				ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_XOR(s1, REG_ITMP3, REG_ITMP1);
				ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
				M_XOR(s2, REG_ITMP3, REG_ITMP2);
				M_OR_TST(REG_ITMP1, REG_ITMP2, REG_ITMP3);
			}
			emit_beq(cd, iptr->dst.block);
			break;
			
		case ICMD_IF_LLT:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.l == 0) {
				/* if high word is less than zero, the whole long is too */
				M_CMPI(s2, 0);
				emit_blt(cd, iptr->dst.block);
			}
			else if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff)) {
  				M_CMPI(s2, 0);
				emit_blt(cd, iptr->dst.block);
				emit_label_bgt(cd, BRANCH_LABEL_1);
  				M_CMPUI(s1, iptr->sx.val.l & 0xffff);
				emit_blt(cd, iptr->dst.block);
				emit_label(cd, BRANCH_LABEL_1);
  			}
			else {
  				ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
  				M_CMP(s2, REG_ITMP3);
				emit_blt(cd, iptr->dst.block);
				emit_label_bgt(cd, BRANCH_LABEL_1);
  				ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_CMPU(s1, REG_ITMP3);
				emit_blt(cd, iptr->dst.block);
				emit_label(cd, BRANCH_LABEL_1);
			}
			break;
			
		case ICMD_IF_LLE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_high(jd, iptr, REG_ITMP2);
/*  			if (iptr->sx.val.l == 0) { */
/*  				M_OR(s1, s2, REG_ITMP3); */
/*  				M_CMPI(REG_ITMP3, 0); */

/*    			} else  */
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff)) {
  				M_CMPI(s2, 0);
				emit_blt(cd, iptr->dst.block);
				emit_label_bgt(cd, BRANCH_LABEL_1);
  				M_CMPUI(s1, iptr->sx.val.l & 0xffff);
  			}
			else {
  				ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
  				M_CMP(s2, REG_ITMP3);
				emit_blt(cd, iptr->dst.block);
				emit_label_bgt(cd, BRANCH_LABEL_1);
  				ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_CMPU(s1, REG_ITMP3);
			}
			emit_ble(cd, iptr->dst.block);
			emit_label(cd, BRANCH_LABEL_1);
			break;
			
		case ICMD_IF_LNE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.l == 0) {
				M_OR_TST(s1, s2, REG_ITMP3);
			}
			else if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff)) {
				M_XOR_IMM(s2, 0, REG_ITMP2);
				M_XOR_IMM(s1, iptr->sx.val.l & 0xffff, REG_ITMP1);
				M_OR_TST(REG_ITMP1, REG_ITMP2, REG_ITMP3);
  			}
			else {
				ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_XOR(s1, REG_ITMP3, REG_ITMP1);
				ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
				M_XOR(s2, REG_ITMP3, REG_ITMP2);
				M_OR_TST(REG_ITMP1, REG_ITMP2, REG_ITMP3);
			}
			emit_bne(cd, iptr->dst.block);
			break;
			
		case ICMD_IF_LGT:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_high(jd, iptr, REG_ITMP2);
/*  			if (iptr->sx.val.l == 0) { */
/*  				M_OR(s1, s2, REG_ITMP3); */
/*  				M_CMPI(REG_ITMP3, 0); */

/*    			} else  */
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff)) {
  				M_CMPI(s2, 0);
				emit_bgt(cd, iptr->dst.block);
				emit_label_blt(cd, BRANCH_LABEL_1);
  				M_CMPUI(s1, iptr->sx.val.l & 0xffff);
  			}
			else {
  				ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
  				M_CMP(s2, REG_ITMP3);
				emit_bgt(cd, iptr->dst.block);
				emit_label_blt(cd, BRANCH_LABEL_1);
  				ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_CMPU(s1, REG_ITMP3);
			}
			emit_bgt(cd, iptr->dst.block);
			emit_label(cd, BRANCH_LABEL_1);
			break;
			
		case ICMD_IF_LGE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.l == 0) {
				/* if high word is greater equal zero, the whole long is too */
				M_CMPI(s2, 0);
				emit_bge(cd, iptr->dst.block);
			}
			else if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff)) {
  				M_CMPI(s2, 0);
				emit_bgt(cd, iptr->dst.block);
				emit_label_blt(cd, BRANCH_LABEL_1);
  				M_CMPUI(s1, iptr->sx.val.l & 0xffff);
				emit_bge(cd, iptr->dst.block);
				emit_label(cd, BRANCH_LABEL_1);
  			}
			else {
  				ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
  				M_CMP(s2, REG_ITMP3);
				emit_bgt(cd, iptr->dst.block);
				emit_label_blt(cd, BRANCH_LABEL_1);
  				ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_CMPU(s1, REG_ITMP3);
				emit_bge(cd, iptr->dst.block);
				emit_label(cd, BRANCH_LABEL_1);
			}
			break;

		case ICMD_IF_ICMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ICMPNE:
		case ICMD_IF_ICMPLT:
		case ICMD_IF_ICMPGT:
		case ICMD_IF_ICMPLE:
		case ICMD_IF_ICMPGE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_ICMPEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_ACMPEQ:    /* op1 = target JavaVM pc                     */
		case ICMD_IF_ACMPNE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_ACMPEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_LCMPEQ:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_label_bne(cd, BRANCH_LABEL_1);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_beq(cd, iptr->dst.block);
			emit_label(cd, BRANCH_LABEL_1);
			break;

		case ICMD_IF_LCMPNE:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bne(cd, iptr->dst.block);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bne(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPLT:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_blt(cd, iptr->dst.block);
			emit_label_bgt(cd, BRANCH_LABEL_1);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMPU(s1, s2);
			emit_blt(cd, iptr->dst.block);
			emit_label(cd, BRANCH_LABEL_1);
			break;

		case ICMD_IF_LCMPGT:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bgt(cd, iptr->dst.block);
			emit_label_blt(cd, BRANCH_LABEL_1);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMPU(s1, s2);
			emit_bgt(cd, iptr->dst.block);
			emit_label(cd, BRANCH_LABEL_1);
			break;

		case ICMD_IF_LCMPLE:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_blt(cd, iptr->dst.block);
			emit_label_bgt(cd, BRANCH_LABEL_1);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMPU(s1, s2);
			emit_ble(cd, iptr->dst.block);
			emit_label(cd, BRANCH_LABEL_1);
			break;

		case ICMD_IF_LCMPGE:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bgt(cd, iptr->dst.block);
			emit_label_blt(cd, BRANCH_LABEL_1);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMPU(s1, s2);
			emit_bge(cd, iptr->dst.block);
			emit_label(cd, BRANCH_LABEL_1);
			break;

		case ICMD_IRETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT);
			M_INTMOVE(s1, REG_RESULT);
			goto nowperformreturn;

		case ICMD_ARETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT);
			M_INTMOVE(s1, REG_RESULT);

#ifdef ENABLE_VERIFIER
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_class *uc = iptr->sx.s23.s2.uc;

				codegen_addpatchref(cd, PATCHER_resolve_class, uc, 0);
			}
#endif /* ENABLE_VERIFIER */
			goto nowperformreturn;

		case ICMD_LRETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(s1, REG_RESULT_PACKED);
			goto nowperformreturn;

		case ICMD_FRETURN:      /* ..., retvalue ==> ...                      */
		case ICMD_DRETURN:

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_FRESULT);
			M_FLTMOVE(s1, REG_FRESULT);
			goto nowperformreturn;

		case ICMD_RETURN:      /* ...  ==> ...                                */

			REPLACEMENT_POINT_RETURN(cd, iptr);

nowperformreturn:
			{
			s4 i, p;
			
			p = cd->stackframesize;

			/* call trace function */

			emit_verbosecall_exit(jd);

#if defined(ENABLE_THREADS)
			if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
				disp = dseg_add_functionptr(cd, LOCK_monitor_exit);
				M_ALD(REG_ITMP3, REG_PV, disp);
				M_MTCTR(REG_ITMP3);

				/* we need to save the proper return value */

				switch (iptr->opc) {
				case ICMD_LRETURN:
					M_IST(REG_RESULT2, REG_SP, rd->memuse * 4 + 8);
					/* fall through */
				case ICMD_IRETURN:
				case ICMD_ARETURN:
					M_IST(REG_RESULT , REG_SP, rd->memuse * 4 + 4);
					break;
				case ICMD_FRETURN:
					M_FST(REG_FRESULT, REG_SP, rd->memuse * 4 + 4);
					break;
				case ICMD_DRETURN:
					M_DST(REG_FRESULT, REG_SP, rd->memuse * 4 + 4);
					break;
				}

				M_ALD(REG_A0, REG_SP, rd->memuse * 4);
				M_JSR;

				/* and now restore the proper return value */

				switch (iptr->opc) {
				case ICMD_LRETURN:
					M_ILD(REG_RESULT2, REG_SP, rd->memuse * 4 + 8);
					/* fall through */
				case ICMD_IRETURN:
				case ICMD_ARETURN:
					M_ILD(REG_RESULT , REG_SP, rd->memuse * 4 + 4);
					break;
				case ICMD_FRETURN:
					M_FLD(REG_FRESULT, REG_SP, rd->memuse * 4 + 4);
					break;
				case ICMD_DRETURN:
					M_DLD(REG_FRESULT, REG_SP, rd->memuse * 4 + 4);
					break;
				}
			}
#endif

			/* restore return address                                         */

			if (!jd->isleafmethod) {
				/* ATTENTION: Don't use REG_ZERO (r0) here, as M_ALD
				   may have a displacement overflow. */

				M_ALD(REG_ITMP1, REG_SP, p * 4 + LA_LR_OFFSET);
				M_MTLR(REG_ITMP1);
			}

			/* restore saved registers                                        */

			for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
				p--; M_ILD(rd->savintregs[i], REG_SP, p * 4);
			}
			for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
				p -= 2; M_DLD(rd->savfltregs[i], REG_SP, p * 4);
			}

			/* deallocate stack                                               */

			if (cd->stackframesize)
				M_LDA(REG_SP, REG_SP, cd->stackframesize * 4);

			M_RET;
			ALIGNCODENOP;
			}
			break;


		case ICMD_TABLESWITCH:  /* ..., index ==> ...                         */
			{
			s4 i, l;
			branch_target_t *table;

			table = iptr->dst.table;

			l = iptr->sx.s23.s2.tablelow;
			i = iptr->sx.s23.s3.tablehigh;
			
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (l == 0)
				M_INTMOVE(s1, REG_ITMP1);
			else if (l <= 32768)
				M_LDA(REG_ITMP1, s1, -l);
			else {
				ICONST(REG_ITMP2, l);
				M_ISUB(s1, REG_ITMP2, REG_ITMP1);
			}

			i = i - l + 1;

			/* range check */

			M_CMPUI(REG_ITMP1, i - 1);
			emit_bgt(cd, table[0].block);

			/* build jump table top down and use address of lowest entry */

			table += i;

			while (--i >= 0) {
				dseg_add_target(cd, table->block); 
				--table;
			}

			/* length of dataseg after last dseg_add_target is used by load */

			M_SLL_IMM(REG_ITMP1, 2, REG_ITMP1);
			M_IADD(REG_ITMP1, REG_PV, REG_ITMP2);
			M_ALD(REG_ITMP2, REG_ITMP2, -(cd->dseglen));
			M_MTCTR(REG_ITMP2);
			M_RTS;
			ALIGNCODENOP;
			}
			break;


		case ICMD_LOOKUPSWITCH: /* ..., key ==> ...                           */
			{
			s4 i;
			lookup_target_t *lookup;

			lookup = iptr->dst.lookup;

			i = iptr->sx.s23.s2.lookupcount;
			
			MCODECHECK((i<<2)+8);
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

			while (--i >= 0) {
				if ((lookup->value >= -32768) && (lookup->value <= 32767)) {
					M_CMPI(s1, lookup->value);
				}
				else {
					disp = dseg_add_s4(cd, lookup->value);
					M_ILD(REG_ITMP2, REG_PV, disp);
					M_CMP(s1, REG_ITMP2);
				}
				emit_beq(cd, lookup->target.block);
				lookup++;
			}

			emit_br(cd, iptr->sx.s23.s3.lookupdefault.block);
			ALIGNCODENOP;
			break;
			}


		case ICMD_BUILTIN:      /* ..., [arg1, [arg2 ...]] ==> ...            */

			bte = iptr->sx.s23.s3.bte;
			md  = bte->md;
			goto gen_method;

		case ICMD_INVOKESTATIC: /* ..., [arg1, [arg2 ...]] ==> ...            */

		case ICMD_INVOKESPECIAL:/* ..., objectref, [arg1, [arg2 ...]] ==> ... */
		case ICMD_INVOKEVIRTUAL:/* op1 = arg count, val.a = method pointer    */
		case ICMD_INVOKEINTERFACE:

			REPLACEMENT_POINT_INVOKE(cd, iptr);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				lm = NULL;
				um = iptr->sx.s23.s3.um;
				md = um->methodref->parseddesc.md;
			}
			else {
				lm = iptr->sx.s23.s3.fmiref->p.method;
				um = NULL;
				md = lm->parseddesc;
			}

gen_method:
			s3 = md->paramcount;

			MCODECHECK((s3 << 1) + 64);

			/* copy arguments to registers or stack location */

			for (s3 = s3 - 1; s3 >= 0; s3--) {
				var = VAR(iptr->sx.s23.s2.args[s3]);
				d   = md->params[s3].regoff;

				/* Already Preallocated? */
				if (var->flags & PREALLOC)
					continue;

				if (IS_INT_LNG_TYPE(var->type)) {
					if (!md->params[s3].inmemory) {
						if (IS_2_WORD_TYPE(var->type)) {
							s1 = emit_load(jd, iptr, var, d);
							M_LNGMOVE(s1, d);
						}
						else {
							s1 = emit_load(jd, iptr, var, d);
							M_INTMOVE(s1, d);
						}
					}
					else {
						if (IS_2_WORD_TYPE(var->type)) {
							s1 = emit_load(jd, iptr, var, REG_ITMP12_PACKED);
							M_LST(s1, REG_SP, d * 4);
						}
						else {
							s1 = emit_load(jd, iptr, var, REG_ITMP1);
							M_IST(s1, REG_SP, d * 4);
						}
					}
				}
				else {
					if (!md->params[s3].inmemory) {
						s1 = emit_load(jd, iptr, var, d);
						M_FLTMOVE(s1, d);
					}
					else {
						s1 = emit_load(jd, iptr, var, REG_FTMP1);
						if (IS_2_WORD_TYPE(var->type))
							M_DST(s1, REG_SP, d * 4);
						else
							M_FST(s1, REG_SP, d * 4);
					}
				}
			}

			switch (iptr->opc) {
			case ICMD_BUILTIN:
				disp = dseg_add_functionptr(cd, bte->fp);

				M_ALD(REG_PV, REG_PV, disp);  /* pointer to built-in-function */

				/* generate the actual call */

				M_MTCTR(REG_PV);
				M_JSR;
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_MFLR(REG_ITMP1);
				M_LDA(REG_PV, REG_ITMP1, -disp);

				emit_exception_check(cd, iptr);
				break;

			case ICMD_INVOKESPECIAL:
				emit_nullpointer_check(cd, iptr, REG_A0);
				/* fall-through */

			case ICMD_INVOKESTATIC:
				if (lm == NULL) {
					disp = dseg_add_unique_address(cd, um);

					codegen_addpatchref(cd, PATCHER_invokestatic_special,
										um, disp);
				}
				else
					disp = dseg_add_address(cd, lm->stubroutine);

				M_ALD(REG_PV, REG_PV, disp);

				/* generate the actual call */

				M_MTCTR(REG_PV);
				M_JSR;
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_MFLR(REG_ITMP1);
				M_LDA(REG_PV, REG_ITMP1, -disp);
				break;

			case ICMD_INVOKEVIRTUAL:
				if (lm == NULL) {
					codegen_addpatchref(cd, PATCHER_invokevirtual, um, 0);

					s1 = 0;
				}
				else {
					s1 = OFFSET(vftbl_t, table[0]) +
						sizeof(methodptr) * lm->vftblindex;
				}

				/* implicit null-pointer check */
				M_ALD(REG_METHODPTR, REG_A0, OFFSET(java_objectheader, vftbl));
				M_ALD(REG_PV, REG_METHODPTR, s1);

				/* generate the actual call */

				M_MTCTR(REG_PV);
				M_JSR;
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_MFLR(REG_ITMP1);
				M_LDA(REG_PV, REG_ITMP1, -disp);
				break;

			case ICMD_INVOKEINTERFACE:
				if (lm == NULL) {
					codegen_addpatchref(cd, PATCHER_invokeinterface, um, 0);

					s1 = 0;
					s2 = 0;
				}
				else {
					s1 = OFFSET(vftbl_t, interfacetable[0]) -
						sizeof(methodptr*) * lm->class->index;

					s2 = sizeof(methodptr) * (lm - lm->class->methods);
				}

				/* implicit null-pointer check */
				M_ALD(REG_METHODPTR, REG_A0, OFFSET(java_objectheader, vftbl));
				M_ALD(REG_METHODPTR, REG_METHODPTR, s1);
				M_ALD(REG_PV, REG_METHODPTR, s2);

				/* generate the actual call */

				M_MTCTR(REG_PV);
				M_JSR;
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_MFLR(REG_ITMP1);
				M_LDA(REG_PV, REG_ITMP1, -disp);
				break;
			}

			/* store return value */

			d = md->returntype.type;

			if (d != TYPE_VOID) {
				if (IS_INT_LNG_TYPE(d)) {
					if (IS_2_WORD_TYPE(d)) {
						s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);
						M_LNGMOVE(REG_RESULT_PACKED, s1);
					}
					else {
						s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT);
						M_INTMOVE(REG_RESULT, s1);
					}
				}
				else {
					s1 = codegen_reg_of_dst(jd, iptr, REG_FRESULT);
					M_FLTMOVE(REG_FRESULT, s1);
				}
				emit_store_dst(jd, iptr, s1);
			}
			break;


		case ICMD_CHECKCAST:  /* ..., objectref ==> ..., objectref            */

			if (!(iptr->flags.bits & INS_FLAG_ARRAY)) {
				/* object type cast-check */

				classinfo *super;
				s4         superindex;

				if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
					super      = NULL;
					superindex = 0;
				}
				else {
					super      = iptr->sx.s23.s3.c.cls;
					superindex = super->index;
				}

				if ((super == NULL) || !(super->flags & ACC_INTERFACE))
					CODEGEN_CRITICAL_SECTION_NEW;

				s1 = emit_load_s1(jd, iptr, REG_ITMP1);

				/* if class is not resolved, check which code to call */

				if (super == NULL) {
					M_TST(s1);
					emit_label_beq(cd, BRANCH_LABEL_1);

					disp = dseg_add_unique_s4(cd, 0);         /* super->flags */

					codegen_addpatchref(cd,
										PATCHER_resolve_classref_to_flags,
										iptr->sx.s23.s3.c.ref,
										disp);

					M_ILD(REG_ITMP2, REG_PV, disp);
					M_AND_IMM(REG_ITMP2, ACC_INTERFACE, REG_ITMP2);
					emit_label_beq(cd, BRANCH_LABEL_2);
				}

				/* interface checkcast code */

				if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
					if (super == NULL) {
						codegen_addpatchref(cd,
											PATCHER_checkcast_interface,
											iptr->sx.s23.s3.c.ref,
											0);
					}
					else {
						M_TST(s1);
						emit_label_beq(cd, BRANCH_LABEL_3);
					}

					M_ALD(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));
					M_ILD(REG_ITMP3, REG_ITMP2, OFFSET(vftbl_t, interfacetablelength));
					M_LDATST(REG_ITMP3, REG_ITMP3, -superindex);
					emit_classcast_check(cd, iptr, BRANCH_LE, REG_ITMP3, s1);

					M_ALD(REG_ITMP3, REG_ITMP2,
						  OFFSET(vftbl_t, interfacetable[0]) -
						  superindex * sizeof(methodptr*));
					M_TST(REG_ITMP3);
					emit_classcast_check(cd, iptr, BRANCH_EQ, REG_ITMP3, s1);

					if (super == NULL)
						emit_label_br(cd, BRANCH_LABEL_4);
					else
						emit_label(cd, BRANCH_LABEL_3);
				}

				/* class checkcast code */

				if ((super == NULL) || !(super->flags & ACC_INTERFACE)) {
					if (super == NULL) {
						emit_label(cd, BRANCH_LABEL_2);

						disp = dseg_add_unique_address(cd, NULL);

						codegen_addpatchref(cd, PATCHER_resolve_classref_to_vftbl,
											iptr->sx.s23.s3.c.ref,
											disp);
					}
					else {
						disp = dseg_add_address(cd, super->vftbl);

						M_TST(s1);
						emit_label_beq(cd, BRANCH_LABEL_5);
					}

					M_ALD(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));

					CODEGEN_CRITICAL_SECTION_START;

					M_ILD(REG_ITMP3, REG_ITMP2, OFFSET(vftbl_t, baseval));
					M_ALD(REG_ITMP2, REG_PV, disp);
					if (s1 != REG_ITMP1) {
						M_ILD(REG_ITMP1, REG_ITMP2, OFFSET(vftbl_t, baseval));
						M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, diffval));

						CODEGEN_CRITICAL_SECTION_END;

						M_ISUB(REG_ITMP3, REG_ITMP1, REG_ITMP3);
					}
					else {
						M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, baseval));
						M_ISUB(REG_ITMP3, REG_ITMP2, REG_ITMP3);
						M_ALD(REG_ITMP2, REG_PV, disp);
						M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, diffval));

						CODEGEN_CRITICAL_SECTION_END;
					}
					M_CMPU(REG_ITMP3, REG_ITMP2);
					emit_classcast_check(cd, iptr, BRANCH_GT, REG_ITMP3, s1);

					if (super != NULL)
						emit_label(cd, BRANCH_LABEL_5);
				}

				if (super == NULL) {
					emit_label(cd, BRANCH_LABEL_1);
					emit_label(cd, BRANCH_LABEL_4);
				}

				d = codegen_reg_of_dst(jd, iptr, s1);
			}
			else {
				/* array type cast-check */

				s1 = emit_load_s1(jd, iptr, REG_A0);
				M_INTMOVE(s1, REG_A0);

				if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
					disp = dseg_add_unique_address(cd, NULL);

					codegen_addpatchref(cd, PATCHER_resolve_classref_to_classinfo,
										iptr->sx.s23.s3.c.ref,
										disp);
				}
				else
					disp = dseg_add_address(cd, iptr->sx.s23.s3.c.cls);

				M_ALD(REG_A1, REG_PV, disp);
				disp = dseg_add_functionptr(cd, BUILTIN_arraycheckcast);
				M_ALD(REG_ITMP2, REG_PV, disp);
				M_MTCTR(REG_ITMP2);
				M_JSR;
				M_TST(REG_RESULT);
				emit_classcast_check(cd, iptr, BRANCH_EQ, REG_RESULT, s1);

				s1 = emit_load_s1(jd, iptr, REG_ITMP1);
				d = codegen_reg_of_dst(jd, iptr, s1);
			}
			M_INTMOVE(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INSTANCEOF: /* ..., objectref ==> ..., intresult            */

			{
			classinfo *super;
			s4         superindex;

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				super      = NULL;
				superindex = 0;
			}
			else {
				super      = iptr->sx.s23.s3.c.cls;
				superindex = super->index;
			}
			
			if ((super == NULL) || !(super->flags & ACC_INTERFACE))
				CODEGEN_CRITICAL_SECTION_NEW;

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s1 == d) {
				M_MOV(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			}

			M_CLR(d);

			/* if class is not resolved, check which code to call */

			if (super == NULL) {
				M_TST(s1);
				emit_label_beq(cd, BRANCH_LABEL_1);

				disp = dseg_add_unique_s4(cd, 0);             /* super->flags */

				codegen_addpatchref(cd, PATCHER_resolve_classref_to_flags,
									iptr->sx.s23.s3.c.ref, disp);

				M_ILD(REG_ITMP3, REG_PV, disp);
				M_AND_IMM(REG_ITMP3, ACC_INTERFACE, REG_ITMP3);
				emit_label_beq(cd, BRANCH_LABEL_2);
			}

			/* interface instanceof code */

			if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
				if (super == NULL) {
					codegen_addpatchref(cd,
										PATCHER_instanceof_interface,
										iptr->sx.s23.s3.c.ref, 0);
				}
				else {
					M_TST(s1);
					emit_label_beq(cd, BRANCH_LABEL_3);
				}

				M_ALD(REG_ITMP1, s1, OFFSET(java_objectheader, vftbl));
				M_ILD(REG_ITMP3, REG_ITMP1, OFFSET(vftbl_t, interfacetablelength));
				M_LDATST(REG_ITMP3, REG_ITMP3, -superindex);
				M_BLE(4);
				M_ALD(REG_ITMP1, REG_ITMP1,
					  OFFSET(vftbl_t, interfacetable[0]) -
					  superindex * sizeof(methodptr*));
				M_TST(REG_ITMP1);
				M_BEQ(1);
				M_IADD_IMM(REG_ZERO, 1, d);

				if (super == NULL)
					emit_label_br(cd, BRANCH_LABEL_4);
				else
					emit_label(cd, BRANCH_LABEL_3);
			}

			/* class instanceof code */

			if ((super == NULL) || !(super->flags & ACC_INTERFACE)) {
				if (super == NULL) {
					emit_label(cd, BRANCH_LABEL_2);

					disp = dseg_add_unique_address(cd, NULL);

					codegen_addpatchref(cd, PATCHER_resolve_classref_to_vftbl,
										iptr->sx.s23.s3.c.ref,
										disp);
				}
				else {
					disp = dseg_add_address(cd, super->vftbl);

					M_TST(s1);
					emit_label_beq(cd, BRANCH_LABEL_5);
				}

				M_ALD(REG_ITMP1, s1, OFFSET(java_objectheader, vftbl));
				M_ALD(REG_ITMP2, REG_PV, disp);

				CODEGEN_CRITICAL_SECTION_START;

				M_ILD(REG_ITMP1, REG_ITMP1, OFFSET(vftbl_t, baseval));
				M_ILD(REG_ITMP3, REG_ITMP2, OFFSET(vftbl_t, baseval));
				M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, diffval));

				CODEGEN_CRITICAL_SECTION_END;

				M_ISUB(REG_ITMP1, REG_ITMP3, REG_ITMP1);
				M_CMPU(REG_ITMP1, REG_ITMP2);
				M_CLR(d);
				M_BGT(1);
				M_IADD_IMM(REG_ZERO, 1, d);

				if (super != NULL)
					emit_label(cd, BRANCH_LABEL_5);
			}

			if (super == NULL) {
				emit_label(cd, BRANCH_LABEL_1);
				emit_label(cd, BRANCH_LABEL_4);
			}

			emit_store_dst(jd, iptr, d);
			}
			break;

		case ICMD_MULTIANEWARRAY:/* ..., cnt1, [cnt2, ...] ==> ..., arrayref  */

			/* check for negative sizes and copy sizes to stack if necessary  */

			MCODECHECK((iptr->s1.argcount << 1) + 64);

			for (s1 = iptr->s1.argcount; --s1 >= 0;) {
				var = VAR(iptr->sx.s23.s2.args[s1]);

				/* copy SAVEDVAR sizes to stack */

				/* Already Preallocated? */
				if (!(var->flags & PREALLOC)) {
					s2 = emit_load(jd, iptr, var, REG_ITMP1);
#if defined(__DARWIN__)
					M_IST(s2, REG_SP, LA_SIZE + (s1 + INT_ARG_CNT) * 4);
#else
					M_IST(s2, REG_SP, LA_SIZE + (s1 + 3) * 4);
#endif
				}
			}

			/* a0 = dimension count */

			ICONST(REG_A0, iptr->s1.argcount);

			/* is patcher function set? */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				disp = dseg_add_unique_address(cd, NULL);

				codegen_addpatchref(cd, PATCHER_resolve_classref_to_classinfo,
									iptr->sx.s23.s3.c.ref, disp);
			}
			else
				disp = dseg_add_address(cd, iptr->sx.s23.s3.c.cls);

			/* a1 = arraydescriptor */

			M_ALD(REG_A1, REG_PV, disp);

			/* a2 = pointer to dimensions = stack pointer */

#if defined(__DARWIN__)
			M_LDA(REG_A2, REG_SP, LA_SIZE + INT_ARG_CNT * 4);
#else
			M_LDA(REG_A2, REG_SP, LA_SIZE + 3 * 4);
#endif

			disp = dseg_add_functionptr(cd, BUILTIN_multianewarray);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_MTCTR(REG_ITMP3);
			M_JSR;

			/* check for exception before result assignment */

			emit_exception_check(cd, iptr);

			d = codegen_reg_of_dst(jd, iptr, REG_RESULT);
			M_INTMOVE(REG_RESULT, d);
			emit_store_dst(jd, iptr, d);
			break;

		default:
			exceptions_throw_internalerror("Unknown ICMD %d during code generation",
										   iptr->opc);
			return false;
	} /* switch */
		
	} /* for instruction */
		
	} /* if (bptr -> flags >= BBREACHED) */
	} /* for basic block */

	dseg_createlinenumbertable(cd);

	/* generate stubs */

	emit_patcher_stubs(jd);
	REPLACEMENT_EMIT_STUBS(jd);

	/* everything's ok */

	return true;
}


/* codegen_emit_stub_compiler **************************************************

   Emits a stub routine which calls the compiler.
	
*******************************************************************************/

void codegen_emit_stub_compiler(jitdata *jd)
{
	methodinfo  *m;
	codegendata *cd;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;

	/* code for the stub */

	M_ALD_INTERN(REG_ITMP1, REG_PV, -2 * SIZEOF_VOID_P);
	M_ALD_INTERN(REG_PV, REG_PV, -3 * SIZEOF_VOID_P);
	M_MTCTR(REG_PV);
	M_RTS;
}


/* codegen_emit_stub_native ****************************************************

   Emits a stub routine which calls a native method.

*******************************************************************************/

void codegen_emit_stub_native(jitdata *jd, methoddesc *nmd, functionptr f)
{
	methodinfo  *m;
	codeinfo    *code;
	codegendata *cd;
	methoddesc  *md;
	s4           nativeparams;
	s4           i, j;                 /* count variables                    */
	s4           t;
	s4           s1, s2, disp;
	s4           funcdisp;

	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;

	/* set some variables */

	md = m->parseddesc;
	nativeparams = (m->flags & ACC_STATIC) ? 2 : 1;

	/* calculate stackframe size */

	cd->stackframesize =
		sizeof(stackframeinfo) / SIZEOF_VOID_P +
		sizeof(localref_table) / SIZEOF_VOID_P +
		4 +                             /* 4 stackframeinfo arguments (darwin)*/
		nmd->paramcount * 2 +           /* assume all arguments are doubles   */
		nmd->memuse;

	/* keep stack 16-byte aligned */

	cd->stackframesize = (cd->stackframesize + 3) & ~3;

	/* create method header */

	(void) dseg_add_unique_address(cd, code);              /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 4); /* FrameSize       */
	(void) dseg_add_unique_s4(cd, 0);                      /* IsSync          */
	(void) dseg_add_unique_s4(cd, 0);                      /* IsLeaf          */
	(void) dseg_add_unique_s4(cd, 0);                      /* IntSave         */
	(void) dseg_add_unique_s4(cd, 0);                      /* FltSave         */
	(void) dseg_addlinenumbertablesize(cd);
	(void) dseg_add_unique_s4(cd, 0);                      /* ExTableSize     */

	/* generate code */

	M_MFLR(REG_ZERO);
	M_AST_INTERN(REG_ZERO, REG_SP, LA_LR_OFFSET);
	M_STWU(REG_SP, REG_SP, -(cd->stackframesize * 4));

	/* get function address (this must happen before the stackframeinfo) */

	funcdisp = dseg_add_functionptr(cd, f);

#if !defined(WITH_STATIC_CLASSPATH)
	if (f == NULL)
		codegen_addpatchref(cd, PATCHER_resolve_native_function, m, funcdisp);
#endif

	/* emit trace code */

	emit_verbosecall_enter(jd);

	/* save integer and float argument registers */

	j = 0;

	for (i = 0; i < md->paramcount; i++) {
		t = md->paramtypes[i].type;

		if (IS_INT_LNG_TYPE(t)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;

				if (IS_2_WORD_TYPE(t)) {
					M_IST(GET_HIGH_REG(s1), REG_SP, LA_SIZE + 4 * 4 + j * 4);
					j++;
					M_IST(GET_LOW_REG(s1), REG_SP, LA_SIZE + 4 * 4 + j * 4);
				}
				else
					M_IST(s1, REG_SP, LA_SIZE + 4 * 4 + j * 4);

				j++;
			}
		}
	}

	for (i = 0; i < md->paramcount; i++) {
		if (IS_FLT_DBL_TYPE(md->paramtypes[i].type)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;
				M_DST(s1, REG_SP, LA_SIZE + 4 * 4 + j * 8);
				j++;
			}
		}
	}

	/* create native stack info */

	M_AADD_IMM(REG_SP, cd->stackframesize * 4, REG_A0);
	M_MOV(REG_PV, REG_A1);
	M_AADD_IMM(REG_SP, cd->stackframesize * 4, REG_A2);
	M_ALD(REG_A3, REG_SP, cd->stackframesize * 4 + LA_LR_OFFSET);
	disp = dseg_add_functionptr(cd, codegen_start_native_call);
	M_ALD(REG_ITMP1, REG_PV, disp);
	M_MTCTR(REG_ITMP1);
	M_JSR;

	/* restore integer and float argument registers */

	j = 0;

	for (i = 0; i < md->paramcount; i++) {
		t = md->paramtypes[i].type;

		if (IS_INT_LNG_TYPE(t)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;

				if (IS_2_WORD_TYPE(t)) {
					M_ILD(GET_HIGH_REG(s1), REG_SP, LA_SIZE + 4 * 4 + j * 4);
					j++;
					M_ILD(GET_LOW_REG(s1), REG_SP, LA_SIZE + 4 * 4 + j * 4);
				}
				else
					M_ILD(s1, REG_SP, LA_SIZE + 4 * 4 + j * 4);

				j++;
			}
		}
	}

	for (i = 0; i < md->paramcount; i++) {
		if (IS_FLT_DBL_TYPE(md->paramtypes[i].type)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;
				M_DLD(s1, REG_SP, LA_SIZE + 4 * 4 + j * 8);
				j++;
			}
		}
	}
	
	/* copy or spill arguments to new locations */

	for (i = md->paramcount - 1, j = i + nativeparams; i >= 0; i--, j--) {
		t = md->paramtypes[i].type;

		if (IS_INT_LNG_TYPE(t)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;
				s2 = nmd->params[j].regoff;

				if (!nmd->params[j].inmemory) {
					if (IS_2_WORD_TYPE(t))
						M_LNGMOVE(s1, s2);
					else
						M_INTMOVE(s1, s2);
				}
				else {
					if (IS_2_WORD_TYPE(t))
						M_LST(s1, REG_SP, s2 * 4);
					else
						M_IST(s1, REG_SP, s2 * 4);
				}
			}
			else {
				s1 = md->params[i].regoff + cd->stackframesize;
				s2 = nmd->params[j].regoff;

				M_ILD(REG_ITMP1, REG_SP, s1 * 4);
				if (IS_2_WORD_TYPE(t))
					M_ILD(REG_ITMP2, REG_SP, s1 * 4 + 4);

				M_IST(REG_ITMP1, REG_SP, s2 * 4);
				if (IS_2_WORD_TYPE(t))
					M_IST(REG_ITMP2, REG_SP, s2 * 4 + 4);
			}
		}
		else {
			/* We only copy spilled float arguments, as the float
			   argument registers keep unchanged. */

			if (md->params[i].inmemory) {
				s1 = md->params[i].regoff + cd->stackframesize;
				s2 = nmd->params[j].regoff;

				if (IS_2_WORD_TYPE(t)) {
					M_DLD(REG_FTMP1, REG_SP, s1 * 4);
					M_DST(REG_FTMP1, REG_SP, s2 * 4);
				}
				else {
					M_FLD(REG_FTMP1, REG_SP, s1 * 4);
					M_FST(REG_FTMP1, REG_SP, s2 * 4);
				}
			}
		}
	}

	/* put class into second argument register */

	if (m->flags & ACC_STATIC) {
		disp = dseg_add_address(cd, m->class);
		M_ALD(REG_A1, REG_PV, disp);
	}

	/* put env into first argument register */

	disp = dseg_add_address(cd, _Jv_env);
	M_ALD(REG_A0, REG_PV, disp);

	/* generate the actual native call */

	M_ALD(REG_ITMP3, REG_PV, funcdisp);
	M_MTCTR(REG_ITMP3);
	M_JSR;

	/* print call trace */

	emit_verbosecall_exit(jd);

	/* save return value */

	switch (md->returntype.type) {
	case TYPE_INT:
	case TYPE_ADR:
		M_IST(REG_RESULT, REG_SP, LA_SIZE + 1 * 4);
		break;
	case TYPE_LNG:
		M_LST(REG_RESULT_PACKED, REG_SP, LA_SIZE + 1 * 4);
		break;
	case TYPE_FLT:
		M_FST(REG_FRESULT, REG_SP, LA_SIZE + 1 * 4);
		break;
	case TYPE_DBL:
		M_DST(REG_FRESULT, REG_SP, LA_SIZE + 1 * 4);
		break;
	case TYPE_VOID:
		break;
	}

	/* remove native stackframe info */

	M_AADD_IMM(REG_SP, cd->stackframesize * 4, REG_A0);
	disp = dseg_add_functionptr(cd, codegen_finish_native_call);
	M_ALD(REG_ITMP1, REG_PV, disp);
	M_MTCTR(REG_ITMP1);
	M_JSR;
	M_MOV(REG_RESULT, REG_ITMP1_XPTR);

	/* restore return value */

	switch (md->returntype.type) {
	case TYPE_INT:
	case TYPE_ADR:
		M_ILD(REG_RESULT, REG_SP, LA_SIZE + 1 * 4);
		break;
	case TYPE_LNG:
		M_LLD(REG_RESULT_PACKED, REG_SP, LA_SIZE + 1 * 4);
		break;
	case TYPE_FLT:
		M_FLD(REG_FRESULT, REG_SP, LA_SIZE + 1 * 4);
		break;
	case TYPE_DBL:
		M_DLD(REG_FRESULT, REG_SP, LA_SIZE + 1 * 4);
		break;
	case TYPE_VOID:
		break;
	}

	M_ALD(REG_ITMP2_XPC, REG_SP, cd->stackframesize * 4 + LA_LR_OFFSET);
	M_MTLR(REG_ITMP2_XPC);
	M_LDA(REG_SP, REG_SP, cd->stackframesize * 4); /* remove stackframe       */

	/* check for exception */

	M_TST(REG_ITMP1_XPTR);
	M_BNE(1);                           /* if no exception then return        */

	M_RET;

	/* handle exception */

	M_IADD_IMM(REG_ITMP2_XPC, -4, REG_ITMP2_XPC);  /* exception address       */

	disp = dseg_add_functionptr(cd, asm_handle_nat_exception);
	M_ALD(REG_ITMP3, REG_PV, disp);
	M_MTCTR(REG_ITMP3);
	M_RTS;

	/* generate patcher stubs */

	emit_patcher_stubs(jd);
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

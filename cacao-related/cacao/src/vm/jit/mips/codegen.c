/* src/vm/jit/mips/codegen.c - machine code generator for MIPS

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

   $Id: codegen.c 7880 2007-05-07 14:13:45Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>

#include "vm/types.h"

#include "md-abi.h"

#include "vm/jit/mips/arch.h"
#include "vm/jit/mips/codegen.h"

#include "mm/memory.h"

#include "native/native.h"

#include "threads/lock-common.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/vm.h"

#include "vm/jit/abi.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/codegen-common.h"
#include "vm/jit/dseg.h"
#include "vm/jit/emit-common.h"
#include "vm/jit/jit.h"
#include "vm/jit/md.h"
#include "vm/jit/patcher.h"
#include "vm/jit/reg.h"
#include "vm/jit/replace.h"

#if defined(ENABLE_LSRA)
# include "vm/jit/allocator/lsra.h"
#endif

#include "vmcore/class.h"
#include "vmcore/options.h"


/* codegen_emit ****************************************************************

   Generates machine code.

*******************************************************************************/

bool codegen_emit(jitdata *jd)
{
	methodinfo         *m;
	codeinfo           *code;
	codegendata        *cd;
	registerdata       *rd;
	s4                  len, s1, s2, s3, d, disp;
	varinfo            *var;
	basicblock         *bptr;
	instruction        *iptr;
	exception_entry    *ex;
	u2                  currentline;
	constant_classref  *cr;
	unresolved_class   *uc;
	methodinfo         *lm;             /* local methodinfo for ICMD_INVOKE*  */
	unresolved_method  *um;
	builtintable_entry *bte;
	methoddesc         *md;
	fieldinfo          *fi;
	unresolved_field   *uf;
	s4                  fieldtype;
	s4                  varindex;

	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;
	rd   = jd->rd;

	/* prevent compiler warnings */

	s1          = 0;
	d           = 0;
	fieldtype   = 0;
	lm          = NULL;
	um          = NULL;
	bte         = NULL;
	currentline = 0;
	uf          = NULL;

	{
	s4 i, p, t, l;
	s4 savedregs_num;

	savedregs_num = (jd->isleafmethod) ? 0 : 1;       /* space to save the RA */

	/* space to save used callee saved registers */

	savedregs_num += (INT_SAV_CNT - rd->savintreguse);
	savedregs_num += (FLT_SAV_CNT - rd->savfltreguse);

	cd->stackframesize = rd->memuse + savedregs_num;

#if defined(ENABLE_THREADS)
	/* space to save argument of monitor_enter */

	if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
# if SIZEOF_VOID_P == 8
		cd->stackframesize++;
# else
		rd->memuse++;
		cd->stackframesize += 2;
# endif
	}
#endif

	/* keep stack 16-byte aligned */

	if (cd->stackframesize & 1)
		cd->stackframesize++;

	/* create method header */

	(void) dseg_add_unique_address(cd, code);              /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 8); /* FrameSize       */

#if defined(ENABLE_THREADS)
	/* IsSync contains the offset relative to the stack pointer for the
	   argument of monitor_exit used in the exception handler. Since the
	   offset could be zero and give a wrong meaning of the flag it is
	   offset by one.
	*/

	if (checksync && (m->flags & ACC_SYNCHRONIZED))
		(void) dseg_add_unique_s4(cd, (rd->memuse + 1) * 8); /* IsSync        */
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
	
	/* create stack frame (if necessary) */

	if (cd->stackframesize)
		M_LDA(REG_SP, REG_SP, -cd->stackframesize * 8);

	/* save return address and used callee saved registers */

	p = cd->stackframesize;
	if (!jd->isleafmethod) {
		p--; M_AST(REG_RA, REG_SP, p * 8);
	}
	for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
		p--; M_AST(rd->savintregs[i], REG_SP, p * 8);
	}
	for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
		p--; M_DST(rd->savfltregs[i], REG_SP, p * 8);
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

		if (IS_INT_LNG_TYPE(t)) {                    /* integer args          */
 			if (!md->params[p].inmemory) {           /* register arguments    */
#if SIZEOF_VOID_P == 8
 				if (!(var->flags & INMEMORY))
 					M_INTMOVE(s1, var->vv.regoff);
 				else
 					M_LST(s1, REG_SP, var->vv.regoff * 8);
#else
				if (IS_2_WORD_TYPE(t)) {
					if (!(var->flags & INMEMORY))
						M_LNGMOVE(s1, var->vv.regoff);
					else
						M_LST(s1, REG_SP, var->vv.regoff * 8);
				}
				else {
					if (!(var->flags & INMEMORY))
						M_INTMOVE(s1, var->vv.regoff);
					else
						M_IST(s1, REG_SP, var->vv.regoff * 8);
				}
#endif
 			}
			else {                                   /* stack arguments       */
 				if (!(var->flags & INMEMORY)) {
#if SIZEOF_VOID_P == 8
 					M_LLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8);
#else
					if (IS_2_WORD_TYPE(t))
						M_LLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8);
					else
						M_ILD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8);
#endif
 				}
				else
					var->vv.regoff = cd->stackframesize + s1;
			}
 		}
		else {                                       /* floating args         */
 			if (!md->params[p].inmemory) {
#if SIZEOF_VOID_P == 8
 				if (!(var->flags & INMEMORY)) {
					if (IS_2_WORD_TYPE(t))
						M_DMOV(s1, var->vv.regoff);
					else
						M_FMOV(s1, var->vv.regoff);
 				}
				else {
					if (IS_2_WORD_TYPE(t))
						M_DST(s1, REG_SP, var->vv.regoff * 8);
					else
						M_FST(s1, REG_SP, var->vv.regoff * 8);
 				}
#else
				if ((p == 0) ||
					((p == 1) && IS_FLT_DBL_TYPE(md->paramtypes[0].type))) {
					if (!(var->flags & INMEMORY)) {
						if (IS_2_WORD_TYPE(t))
							M_DBLMOVE(s1, var->vv.regoff);
						else
							M_FLTMOVE(s1, var->vv.regoff);
					}
					else {
						if (IS_2_WORD_TYPE(t))
							M_DST(s1, REG_SP, var->vv.regoff * 8);
						else
							M_FST(s1, REG_SP, var->vv.regoff * 8);
					}
				}
				else {
					if (IS_2_WORD_TYPE(t)) {
						if (!(var->flags & INMEMORY)) {
							M_MTC1(GET_LOW_REG(s1), var->vv.regoff);
							M_MTC1(GET_HIGH_REG(s1), var->vv.regoff + 1);
							M_NOP;
						}
						else
							M_LST(s1, REG_SP, var->vv.regoff * 8);
					}
					else {
						if (!(var->flags & INMEMORY)) {
							M_MTC1(s1, var->vv.regoff);
							M_NOP;
						}
						else
							M_IST(s1, REG_SP, var->vv.regoff * 8);
					}
				}
#endif
 			}
			else {
 				if (!(var->flags & INMEMORY)) {
					if (IS_2_WORD_TYPE(t))
						M_DLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8);
					else
						M_FLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8);
				}
				else
					var->vv.regoff = cd->stackframesize + s1;
			}
		}
	}

	/* call monitorenter function */

#if defined(ENABLE_THREADS)
	if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
		/* stack offset for monitor argument */

		s1 = rd->memuse;

# if !defined(NDEBUG)
		if (opt_verbosecall) {
			M_LDA(REG_SP, REG_SP, -(INT_ARG_CNT + FLT_ARG_CNT) * 8);

			for (p = 0; p < INT_ARG_CNT; p++)
				M_AST(abi_registers_integer_argument[p], REG_SP, p * 8);

			for (p = 0; p < FLT_ARG_CNT; p++)
				M_DST(abi_registers_float_argument[p], REG_SP, (INT_ARG_CNT + p) * 8);

			s1 += INT_ARG_CNT + FLT_ARG_CNT;
		}
# endif

		/* get correct lock object */

		if (m->flags & ACC_STATIC) {
			disp = dseg_add_address(cd, &m->class->object.header);
			M_ALD(REG_A0, REG_PV, disp);
			disp = dseg_add_functionptr(cd, LOCK_monitor_enter);
			M_ALD(REG_ITMP3, REG_PV, disp);
		}
		else {
/* 			emit_nullpointer_check(cd, iptr, REG_A0); */
			M_BNEZ(REG_A0, 2);
			disp = dseg_add_functionptr(cd, LOCK_monitor_enter);
			M_ALD(REG_ITMP3, REG_PV, disp);                   /* branch delay */
			M_ALD_INTERN(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_NULLPOINTER);
		}

		M_JSR(REG_RA, REG_ITMP3);
		M_AST(REG_A0, REG_SP, s1 * 8);                        /* branch delay */

# if !defined(NDEBUG)
		if (opt_verbosecall) {
			for (p = 0; p < INT_ARG_CNT; p++)
				M_ALD(abi_registers_integer_argument[p], REG_SP, p * 8);

			for (p = 0; p < FLT_ARG_CNT; p++)
				M_DLD(abi_registers_float_argument[p], REG_SP, (INT_ARG_CNT + p) * 8);


			M_LDA(REG_SP, REG_SP, (INT_ARG_CNT + FLT_ARG_CNT) * 8);
		}
# endif
	}
#endif
	}

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_enter(jd);
#endif

	/* end of header generation */

	/* create replacement points */

	REPLACEMENT_POINTS_INIT(cd, jd);

	/* walk through all basic blocks */

	for (bptr = jd->basicblocks; bptr != NULL; bptr = bptr->next) {

		/* handle replacement points */

		REPLACEMENT_POINT_BLOCK_START(cd, bptr);

		/* store relative start of block */

		bptr->mpc = (s4) (cd->mcodeptr - cd->mcodebase);

		if (bptr->flags >= BBREACHED) {
			/* branch resolving */

			codegen_resolve_branchrefs(cd, bptr);

		/* copy interface registers to their destination */

		len = bptr->indepth;
		MCODECHECK(64+len);
#if defined(ENABLE_LSRA)
		if (opt_lsra) {
		while (len) {
			len--;
			src = bptr->invars[len];
			if ((len == bptr->indepth-1) && (bptr->type == BBTYPE_EXH)) {
					/* 				d = reg_of_var(m, src, REG_ITMP1); */
					if (!(src->flags & INMEMORY))
						d = src->vv.regoff;
					else
						d = REG_ITMP1;
					M_INTMOVE(REG_ITMP1, d);
					emit_store(jd, NULL, src, d);
				}
			}
		} else {
#endif
		while (len) {
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
/* 		currentline = 0; */

		for (iptr = bptr->iinstr; len > 0; len--, iptr++) {
			if (iptr->line != currentline) {
				dseg_addlinenumber(cd, iptr->line);
				currentline = iptr->line;
			}

		MCODECHECK(64);       /* an instruction usually needs < 64 words      */

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

#if SIZEOF_VOID_P == 8
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
#endif
			LCONST(d, iptr->sx.val.l);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			disp = dseg_add_float(cd, iptr->sx.val.f);
			M_FLD(d, REG_PV, disp);
			emit_store_dst(jd, iptr, d);
			break;
			
		case ICMD_DCONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			disp = dseg_add_double(cd, iptr->sx.val.d);
			M_DLD(d, REG_PV, disp);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ACONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				cr   = iptr->sx.val.c.ref;
				disp = dseg_add_unique_address(cd, cr);

				codegen_add_patch_ref(cd, PATCHER_aconst, cr, disp);

				M_ALD(d, REG_PV, disp);
			}
			else {
				if (iptr->sx.val.anyptr == NULL)
					M_INTMOVE(REG_ZERO, d);
				else {
					disp = dseg_add_address(cd, iptr->sx.val.anyptr);
					M_ALD(d, REG_PV, disp);
				}
			}
			emit_store_dst(jd, iptr, d);
			break;


		/* load/store/copy/move operations ************************************/

		case ICMD_ILOAD:      /* ...  ==> ..., content of local variable      */
		case ICMD_LLOAD:
		case ICMD_ALOAD:
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
			M_ISUB(REG_ZERO, s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LNEG:       /* ..., value  ==> ..., - value                 */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSUB(REG_ZERO, s1, d);
#else
			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_ISUB(REG_ZERO, GET_LOW_REG(s1), GET_LOW_REG(d));
			M_ISUB(REG_ZERO, GET_HIGH_REG(s1), GET_HIGH_REG(d));
			M_CMPULT(REG_ZERO, GET_LOW_REG(d), REG_ITMP3);
			M_ISUB(GET_HIGH_REG(d), REG_ITMP3, GET_HIGH_REG(d));
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2L:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
#if SIZEOF_VOID_P == 8
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(s1, d);
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_INTMOVE(s1, GET_LOW_REG(d));
			M_ISRA_IMM(GET_LOW_REG(d), 31, GET_HIGH_REG(d));
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_L2I:        /* ..., value  ==> ..., value                   */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_ISLL_IMM(s1, 0, d);
#else
			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(GET_LOW_REG(s1), d);
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2BYTE:   /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
#if SIZEOF_VOID_P == 8
			M_LSLL_IMM(s1, 56, d);
			M_LSRA_IMM( d, 56, d);
#else
			M_ISLL_IMM(s1, 24, d);
			M_ISRA_IMM( d, 24, d);
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2CHAR:   /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND_IMM(s1, 0xffff, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2SHORT:  /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
#if SIZEOF_VOID_P == 8
			M_LSLL_IMM(s1, 48, d);
			M_LSRA_IMM( d, 48, d);
#else
			M_ISLL_IMM(s1, 16, d);
			M_ISRA_IMM( d, 16, d);
#endif
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_IADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_IADD(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IINC:
		case ICMD_IADDCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= -32768) && (iptr->sx.val.i <= 32767))
				M_IADD_IMM(s1, iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_IADD(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LADD(s1, s2, d);
#else
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_IADD(s1, s2, GET_HIGH_REG(d));
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_low(jd, iptr, GET_LOW_REG(REG_ITMP12_PACKED));
			if (s1 == GET_LOW_REG(d)) {
				M_MOV(s1, REG_ITMP3);
				s1 = REG_ITMP3;
			}
			M_IADD(s1, s2, GET_LOW_REG(d));
			M_CMPULT(GET_LOW_REG(d), s1, REG_ITMP3);
			M_IADD(GET_HIGH_REG(d), REG_ITMP3, GET_HIGH_REG(d));
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADDCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.l = constant                          */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= -32768) && (iptr->sx.val.l <= 32767))
				M_LADD_IMM(s1, iptr->sx.val.l, d);
			else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_LADD(s1, REG_ITMP2, d);
			}
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 32767)) {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_IADD_IMM(GET_LOW_REG(s1), iptr->sx.val.l, GET_LOW_REG(d));
				M_CMPULT_IMM(GET_LOW_REG(d), iptr->sx.val.l, REG_ITMP3);
				M_IADD(GET_HIGH_REG(s1), REG_ITMP3, GET_HIGH_REG(d));
			}
			else if ((iptr->sx.val.l >= (-32768 + 1)) && (iptr->sx.val.l < 0)) {
				s1 = emit_load_s1_low(jd, iptr, REG_ITMP2);
				M_ISUB_IMM(s1, -(iptr->sx.val.l), GET_LOW_REG(d));
				M_CMPULT_IMM(GET_LOW_REG(d), iptr->sx.val.l, REG_ITMP3);
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
				M_ISUB_IMM(s1, 1, GET_HIGH_REG(d));
				M_IADD(GET_HIGH_REG(d), REG_ITMP3, GET_HIGH_REG(d));
			}
			else {
				ICONST(REG_ITMP2, iptr->sx.val.l & 0xffffffff);
				s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
				M_IADD(s1, REG_ITMP2, GET_LOW_REG(d));
				M_CMPULT(GET_LOW_REG(d), s1, REG_ITMP3);
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
				M_IADD(s1, REG_ITMP3, GET_HIGH_REG(d));
				ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
				M_IADD(GET_HIGH_REG(d), REG_ITMP3, GET_HIGH_REG(d));
			}
#endif
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

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSUB(s1, s2, d);
#else
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_ISUB(s1, s2, GET_HIGH_REG(d));
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP1);
			M_CMPULT(s1, s2, REG_ITMP3);
			M_ISUB(GET_HIGH_REG(d), REG_ITMP3, GET_HIGH_REG(d));
			/* if s1 is equal to REG_ITMP3 we have to reload it, since
			   the CMPULT instruction destroyed it */
			if (s1 == REG_ITMP3)
				s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
			M_ISUB(s1, s2, GET_LOW_REG(d));

#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUBCONST:  /* ..., value  ==> ..., value - constant        */
		                      /* sx.val.l = constant                          */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= -32767) && (iptr->sx.val.l <= 32768))
				M_LADD_IMM(s1, -iptr->sx.val.l, d);
			else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_LSUB(s1, REG_ITMP2, d);
			}
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 32768)) {
				s1 = emit_load_s1_low(jd, iptr, REG_ITMP2);
				M_ISUB_IMM(s1, iptr->sx.val.l, GET_LOW_REG(d));
				M_CMPULT_IMM(GET_LOW_REG(d), -(iptr->sx.val.l), REG_ITMP3);
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
				M_ISUB_IMM(s1, 1, GET_HIGH_REG(d));
				M_IADD(GET_HIGH_REG(d), REG_ITMP3, GET_HIGH_REG(d));
			}
			else if ((iptr->sx.val.l >= -32767) && (iptr->sx.val.l < 0)) {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_IADD_IMM(GET_LOW_REG(s1), -(iptr->sx.val.l), GET_LOW_REG(d));
				M_CMPULT_IMM(GET_LOW_REG(d), -(iptr->sx.val.l), REG_ITMP3);
				M_IADD(GET_HIGH_REG(s1), REG_ITMP3, GET_HIGH_REG(d));
			}
			else {
				ICONST(REG_ITMP2, iptr->sx.val.l & 0xffffffff);
				s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
				M_ISUB(s1, REG_ITMP2, GET_LOW_REG(d));
				M_CMPULT(s1, REG_ITMP2, REG_ITMP3);
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
				M_ISUB(s1, REG_ITMP3, GET_HIGH_REG(d));
				ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
				M_ISUB(GET_HIGH_REG(d), REG_ITMP3, GET_HIGH_REG(d));
			}
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_IMUL(s1, s2);
			M_MFLO(d);
			M_NOP;
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMULCONST:  /* ..., value  ==> ..., value * constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			ICONST(REG_ITMP2, iptr->sx.val.i);
			M_IMUL(s1, REG_ITMP2);
			M_MFLO(d);
			M_NOP;
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LMUL(s1, s2);
			M_MFLO(d);
			M_NOP;
			M_NOP;
#else
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_IMUL(s2, s1);
			M_MFLO(REG_ITMP3);
			M_NOP;
			M_NOP;
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_IMULU(s2, s1);
			M_MFHI(GET_HIGH_REG(d));
			M_MFLO(GET_LOW_REG(d));
			M_NOP;
			M_NOP;
			M_IADD(GET_HIGH_REG(d), REG_ITMP3, REG_ITMP3);

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_IMUL(s1, s2);
			M_MFLO(s2);
			/* XXX do we need nops here? */
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LMULCONST:  /* ..., value  ==> ..., value * constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			LCONST(REG_ITMP2, iptr->sx.val.l);
			M_LMUL(s1, REG_ITMP2);
			M_MFLO(d);
			M_NOP;
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arithmetic_check(cd, iptr, s2);
			M_IDIV(s1, s2);
			M_MFLO(d);
			M_NOP;
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arithmetic_check(cd, iptr, s2);
			M_IDIV(s1, s2);
			M_MFHI(d);
			M_NOP;
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;

#if SIZEOF_VOID_P == 8

		case ICMD_LDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arithmetic_check(cd, iptr, s2);
			M_LDIV(s1, s2);
			M_MFLO(d);
			M_NOP;
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arithmetic_check(cd, iptr, s2);
			M_LDIV(s1, s2);
			M_MFHI(d);
			M_NOP;
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;

#else /* SIZEOF_VOID_P == 8 */

		case ICMD_LDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
		case ICMD_LREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_A0_A1_PACKED);
			s2 = emit_load_s2(jd, iptr, REG_A2_A3_PACKED);

			/* XXX TODO: only do this if arithmetic check is really done! */
			M_OR(GET_HIGH_REG(s2), GET_LOW_REG(s2), REG_ITMP3);
			emit_arithmetic_check(cd, iptr, REG_ITMP3);

			M_LNGMOVE(s1, REG_A0_A1_PACKED);
			M_LNGMOVE(s2, REG_A2_A3_PACKED);

			bte = iptr->sx.s23.s3.bte;
			disp = dseg_add_functionptr(cd, bte->fp);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_JSR(REG_RA, REG_ITMP3);
			M_NOP;

			d = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(REG_RESULT_PACKED, d);
			emit_store_dst(jd, iptr, d);
			break;

#endif /* SIZEOF_VOID_P == 8 */

		case ICMD_IDIVPOW2:   /* ..., value  ==> ..., value << constant       */
		                      /* val.i = constant                             */
		                      
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
#if SIZEOF_VOID_P == 8
			M_LSRA_IMM(s1, 63, REG_ITMP2);
			M_LSRL_IMM(REG_ITMP2, 64 - iptr->sx.val.i, REG_ITMP2);
			M_LADD(s1, REG_ITMP2, REG_ITMP2);
			M_LSRA_IMM(REG_ITMP2, iptr->sx.val.i, d);
#else
			M_ISRA_IMM(s1, 31, REG_ITMP2);
			M_ISRL_IMM(REG_ITMP2, 32 - iptr->sx.val.i, REG_ITMP2);
			M_IADD(s1, REG_ITMP2, REG_ITMP2);
			M_ISRA_IMM(REG_ITMP2, iptr->sx.val.i, d);
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s1 == d) {
				M_MOV(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			}
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 0xffff)) {
				M_AND_IMM(s1, iptr->sx.val.i, d);
				M_BGEZ(s1, 4);
				M_NOP;
				M_ISUB(REG_ZERO, s1, d);
				M_AND_IMM(d, iptr->sx.val.i, d);
			}
			else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_AND(s1, REG_ITMP2, d);
				M_BGEZ(s1, 4);
				M_NOP;
				M_ISUB(REG_ZERO, s1, d);
				M_AND(d, REG_ITMP2, d);
			}
			M_ISUB(REG_ZERO, d, d);
			emit_store_dst(jd, iptr, d);
			break;

#if SIZEOF_VOID_P == 8

		case ICMD_LDIVPOW2:   /* ..., value  ==> ..., value << constant       */
		                      /* val.i = constant                             */
		                      
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSRA_IMM(s1, 63, REG_ITMP2);
			M_LSRL_IMM(REG_ITMP2, 64 - iptr->sx.val.i, REG_ITMP2);
			M_LADD(s1, REG_ITMP2, REG_ITMP2);
			M_LSRA_IMM(REG_ITMP2, iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s1 == d) {
				M_MOV(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			}
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff)) {
				M_AND_IMM(s1, iptr->sx.val.l, d);
				M_BGEZ(s1, 4);
				M_NOP;
				M_LSUB(REG_ZERO, s1, d);
				M_AND_IMM(d, iptr->sx.val.l, d);
			}
			else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_AND(s1, REG_ITMP2, d);
				M_BGEZ(s1, 4);
				M_NOP;
				M_LSUB(REG_ZERO, s1, d);
				M_AND(d, REG_ITMP2, d);
			}
			M_LSUB(REG_ZERO, d, d);
			emit_store_dst(jd, iptr, d);
			break;

#endif /* SIZEOF_VOID_P == 8 */

		case ICMD_ISHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_ISLL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHLCONST:  /* ..., value  ==> ..., value << constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_ISLL_IMM(s1, iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHR:       /* ..., val1, val2  ==> ..., val1 >> val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_ISRA(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHRCONST:  /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_ISRA_IMM(s1, iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHR:      /* ..., val1, val2  ==> ..., val1 >>> val2      */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_ISRL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_ISRL_IMM(s1, iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSLL(s1, s2, d);
#else
			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			s2 = emit_load_s2(jd, iptr, REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);

			M_ISLL(s2, 26, REG_ITMP1);
			M_BGEZ(REG_ITMP1, 3);
			M_NOP;

			M_ISLL(GET_LOW_REG(s1), s2, GET_HIGH_REG(d));
			M_BR(7);
			M_MOV(REG_ZERO, GET_LOW_REG(d));                    /* delay slot */

#if 1
			M_ISLL(GET_LOW_REG(s1), s2, GET_LOW_REG(d));
#endif
			M_BEQZ(REG_ITMP1, 4);
			M_ISLL(GET_HIGH_REG(s1), s2, GET_HIGH_REG(d));      /* delay slot */

			M_ISUB(s2, REG_ZERO, REG_ITMP3);
			M_ISRL(GET_LOW_REG(s1), REG_ITMP3, REG_ITMP3);
			M_OR(GET_HIGH_REG(d), REG_ITMP3, GET_HIGH_REG(d));

#if 0
			M_ISLL(GET_LOW_REG(s1), s2, GET_LOW_REG(d));
#endif
#endif
			emit_store_dst(jd, iptr, d);
			break;

#if SIZEOF_VOID_P == 8

		case ICMD_LSHLCONST:  /* ..., value  ==> ..., value << constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSLL_IMM(s1, iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHR:       /* ..., val1, val2  ==> ..., val1 >> val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSRA(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHRCONST:  /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSRA_IMM(s1, iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LUSHR:      /* ..., val1, val2  ==> ..., val1 >>> val2      */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSRL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSRL_IMM(s1, iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

#endif /* SIZEOF_VOID_P == 8 */

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
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 0xffff))
				M_AND_IMM(s1, iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_AND(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LAND:       /* ..., val1, val2  ==> ..., val1 & val2        */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_AND(s1, s2, d);
#else
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_AND(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);
			M_AND(s1, s2, GET_HIGH_REG(d));
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.l = constant                          */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff))
				M_AND_IMM(s1, iptr->sx.val.l, d);
			else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_AND(s1, REG_ITMP2, d);
			}
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff)) {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_AND_IMM(GET_LOW_REG(s1), iptr->sx.val.l, GET_LOW_REG(d));
				M_AND_IMM(GET_HIGH_REG(s1), 0, GET_HIGH_REG(d));
			}
			else {
				LCONST(REG_ITMP12_PACKED, iptr->sx.val.l);
				s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
				M_AND(s1, GET_LOW_REG(REG_ITMP12_PACKED), GET_LOW_REG(d));
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP3);
				M_AND(s1, GET_HIGH_REG(REG_ITMP12_PACKED), GET_HIGH_REG(d));
			}
#endif
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
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 0xffff))
				M_OR_IMM(s1, iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_OR(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LOR:        /* ..., val1, val2  ==> ..., val1 | val2        */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_OR(s1, s2, d);
#else
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_OR(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);
			M_OR(s1, s2, GET_HIGH_REG(d));
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.l = constant                          */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff))
				M_OR_IMM(s1, iptr->sx.val.l, d);
			else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_OR(s1, REG_ITMP2, d);
			}
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff)) {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_OR_IMM(GET_LOW_REG(s1), iptr->sx.val.l, GET_LOW_REG(d));
				M_OR_IMM(GET_HIGH_REG(s1), 0, GET_HIGH_REG(d));
			}
			else {
				LCONST(REG_ITMP12_PACKED, iptr->sx.val.l);
				s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
				M_OR(s1, GET_LOW_REG(REG_ITMP12_PACKED), GET_LOW_REG(d));
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP3);
				M_OR(s1, GET_HIGH_REG(REG_ITMP12_PACKED), GET_HIGH_REG(d));
			}
#endif
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
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 0xffff))
				M_XOR_IMM(s1, iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_XOR(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXOR:       /* ..., val1, val2  ==> ..., val1 ^ val2        */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_XOR(s1, s2, d);
#else
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_XOR(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);
			M_XOR(s1, s2, GET_HIGH_REG(d));
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXORCONST:  /* ..., value  ==> ..., value ^ constant        */
		                      /* sx.val.l = constant                          */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff))
				M_XOR_IMM(s1, iptr->sx.val.l, d);
			else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_XOR(s1, REG_ITMP2, d);
			}
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 0xffff)) {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_XOR_IMM(GET_LOW_REG(s1), iptr->sx.val.l, GET_LOW_REG(d));
				M_XOR_IMM(GET_HIGH_REG(s1), 0, GET_HIGH_REG(d));
			}
			else {
				LCONST(REG_ITMP12_PACKED, iptr->sx.val.l);
				s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
				M_XOR(s1, GET_LOW_REG(REG_ITMP12_PACKED), GET_LOW_REG(d));
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP3);
				M_XOR(s1, GET_HIGH_REG(REG_ITMP12_PACKED), GET_HIGH_REG(d));
			}
#endif
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_LCMP:       /* ..., val1, val2  ==> ..., val1 cmp val2      */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_CMPLT(s1, s2, REG_ITMP3);
			M_CMPLT(s2, s1, REG_ITMP1);
			M_LSUB(REG_ITMP1, REG_ITMP3, d);
#else
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_CMPLT(s1, s2, REG_ITMP3);
			M_CMPLT(s2, s1, REG_ITMP1);
			M_ISUB(REG_ITMP1, REG_ITMP3, d);
			M_BNEZ(d, 4);
			M_NOP;
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMPULT(s1, s2, REG_ITMP3);
			M_CMPULT(s2, s1, REG_ITMP1);
			M_ISUB(REG_ITMP1, REG_ITMP3, d);
#endif
			emit_store_dst(jd, iptr, d);
			break;


		/* floating operations ************************************************/

		case ICMD_FNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_FNEG(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_DNEG(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_FADD(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_DADD(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_FSUB(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_DSUB(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_FMUL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DMUL:       /* ..., val1, val2  ==> ..., val1 *** val2      */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_DMUL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_FDIV(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_DDIV(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

#if 0		
		case ICMD_FREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_FDIV(s1,s2, REG_FTMP3);
			M_FLOORFL(REG_FTMP3, REG_FTMP3);
			M_CVTLF(REG_FTMP3, REG_FTMP3);
			M_FMUL(REG_FTMP3, s2, REG_FTMP3);
			M_FSUB(s1, REG_FTMP3, d);
			emit_store_dst(jd, iptr, d);
		    break;

		case ICMD_DREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_DDIV(s1,s2, REG_FTMP3);
			M_FLOORDL(REG_FTMP3, REG_FTMP3);
			M_CVTLD(REG_FTMP3, REG_FTMP3);
			M_DMUL(REG_FTMP3, s2, REG_FTMP3);
			M_DSUB(s1, REG_FTMP3, d);
			emit_store_dst(jd, iptr, d);
		    break;
#endif

		case ICMD_I2F:       /* ..., value  ==> ..., (float) value            */
		case ICMD_L2F:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_MOVLD(s1, d);
			M_CVTLF(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2D:       /* ..., value  ==> ..., (double) value           */
		case ICMD_L2D:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_MOVLD(s1, d);
			M_CVTLD(d, d);
			emit_store_dst(jd, iptr, d);
			break;

#if 0
		/* XXX these do not work correctly */

		case ICMD_F2I:       /* ..., (float) value  ==> ..., (int) value      */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_TRUNCFI(s1, REG_FTMP1);
			M_MOVDI(REG_FTMP1, d);
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;
		
		case ICMD_D2I:       /* ..., (double) value  ==> ..., (int) value     */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_TRUNCDI(s1, REG_FTMP1);
			M_MOVDI(REG_FTMP1, d);
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;
		
		case ICMD_F2L:       /* ..., (float) value  ==> ..., (long) value     */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_TRUNCFL(s1, REG_FTMP1);
			M_MOVDL(REG_FTMP1, d);
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_D2L:       /* ..., (double) value  ==> ..., (long) value    */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_TRUNCDL(s1, REG_FTMP1);
			M_MOVDL(REG_FTMP1, d);
			M_NOP;
			emit_store_dst(jd, iptr, d);
			break;
#endif

		case ICMD_F2D:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_CVTFD(s1, d);
			emit_store_dst(jd, iptr, d);
			break;
					
		case ICMD_D2F:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_CVTDF(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

#if SUPPORT_FLOAT_CMP
		case ICMD_FCMPL:      /* ..., val1, val2  ==> ..., val1 fcmpl val2    */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_FCMPULEF(s1, s2);
			M_FBT(3);
			M_AADD_IMM(REG_ZERO, 1, d);
			M_BR(4);
			M_NOP;
			M_FCMPEQF(s1, s2);
			M_ASUB_IMM(REG_ZERO, 1, d);
			M_CMOVT(REG_ZERO, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCMPG:      /* ..., val1, val2  ==> ..., val1 fcmpg val2    */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_FCMPOLTF(s1, s2);
			M_FBF(3);
			M_ASUB_IMM(REG_ZERO, 1, d);
			M_BR(4);
			M_NOP;
			M_FCMPEQF(s1, s2);
			M_AADD_IMM(REG_ZERO, 1, d);
			M_CMOVT(REG_ZERO, d);
			emit_store_dst(jd, iptr, d);
			break;
#endif

#if SUPPORT_DOUBLE_CMP
		case ICMD_DCMPL:      /* ..., val1, val2  ==> ..., val1 fcmpl val2    */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_FCMPULED(s1, s2);
			M_FBT(3);
			M_AADD_IMM(REG_ZERO, 1, d);
			M_BR(4);
			M_NOP;
			M_FCMPEQD(s1, s2);
			M_ASUB_IMM(REG_ZERO, 1, d);
			M_CMOVT(REG_ZERO, d);
			emit_store_dst(jd, iptr, d);
			break;
			
		case ICMD_DCMPG:      /* ..., val1, val2  ==> ..., val1 fcmpg val2    */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_FCMPOLTD(s1, s2);
			M_FBF(3);
			M_ASUB_IMM(REG_ZERO, 1, d);
			M_BR(4);
			M_NOP;
			M_FCMPEQD(s1, s2);
			M_AADD_IMM(REG_ZERO, 1, d);
			M_CMOVT(REG_ZERO, d);
			emit_store_dst(jd, iptr, d);
			break;
#endif


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
			M_AADD(s2, s1, REG_ITMP3);
			/* implicit null-pointer check */
			M_BLDS(d, REG_ITMP3, OFFSET(java_bytearray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_CALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_AADD(s2, s1, REG_ITMP3);
			M_AADD(s2, REG_ITMP3, REG_ITMP3);
			/* implicit null-pointer check */
			M_SLDU(d, REG_ITMP3, OFFSET(java_chararray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;			

		case ICMD_SALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_AADD(s2, s1, REG_ITMP3);
			M_AADD(s2, REG_ITMP3, REG_ITMP3);
			/* implicit null-pointer check */
			M_SLDS(d, REG_ITMP3, OFFSET(java_shortarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, 2, REG_ITMP3);
			M_AADD(REG_ITMP3, s1, REG_ITMP3);
			/* implicit null-pointer check */
			M_ILD_INTERN(d, REG_ITMP3, OFFSET(java_intarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
#if SIZEOF_VOID_P == 8
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
#endif
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, 3, REG_ITMP3);
			M_AADD(REG_ITMP3, s1, REG_ITMP3);
			/* implicit null-pointer check */
			M_LLD_INTERN(d, REG_ITMP3, OFFSET(java_longarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, 2, REG_ITMP3);
			M_AADD(REG_ITMP3, s1, REG_ITMP3);
			/* implicit null-pointer check */
			M_FLD_INTERN(d, REG_ITMP3, OFFSET(java_floatarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, 3, REG_ITMP3);
			M_AADD(REG_ITMP3, s1, REG_ITMP3);
			/* implicit null-pointer check */
			M_DLD_INTERN(d, REG_ITMP3, OFFSET(java_doublearray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_AALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, POINTERSHIFT, REG_ITMP3);
			M_AADD(REG_ITMP3, s1, REG_ITMP3);
			/* implicit null-pointer check */
			M_ALD_INTERN(d, REG_ITMP3, OFFSET(java_objectarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_BASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_AADD(s2, s1, REG_ITMP1);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			M_BST(s3, REG_ITMP1, OFFSET(java_bytearray, data[0]));
			break;

		case ICMD_CASTORE:    /* ..., arrayref, index, value  ==> ...         */
		case ICMD_SASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_AADD(s2, s1, REG_ITMP1);
			M_AADD(s2, REG_ITMP1, REG_ITMP1);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			M_SST(s3, REG_ITMP1, OFFSET(java_chararray, data[0]));
			break;

		case ICMD_IASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, 2, REG_ITMP2);
			M_AADD(REG_ITMP2, s1, REG_ITMP1);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			M_IST_INTERN(s3, REG_ITMP1, OFFSET(java_intarray, data[0]));
			break;

		case ICMD_LASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, 3, REG_ITMP2);
			M_AADD(REG_ITMP2, s1, REG_ITMP1);
#if SIZEOF_VOID_P == 8
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
#else
			s3 = emit_load_s3(jd, iptr, REG_ITMP23_PACKED);
#endif
			/* implicit null-pointer check */
			M_LST_INTERN(s3, REG_ITMP1, OFFSET(java_longarray, data[0]));
			break;

		case ICMD_FASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, 2, REG_ITMP2);
			M_AADD(REG_ITMP2, s1, REG_ITMP1);
			s3 = emit_load_s3(jd, iptr, REG_FTMP1);
			/* implicit null-pointer check */
			M_FST_INTERN(s3, REG_ITMP1, OFFSET(java_floatarray, data[0]));
			break;

		case ICMD_DASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, 3, REG_ITMP2);
			M_AADD(REG_ITMP2, s1, REG_ITMP1);
			s3 = emit_load_s3(jd, iptr, REG_FTMP1);
			/* implicit null-pointer check */
			M_DST_INTERN(s3, REG_ITMP1, OFFSET(java_doublearray, data[0]));
			break;


		case ICMD_AASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);

			M_INTMOVE(s1, REG_A0);
			M_INTMOVE(s3, REG_A1);
			disp = dseg_add_functionptr(cd, BUILTIN_canstore);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_JSR(REG_RA, REG_ITMP3);
			M_NOP;
			emit_exception_check(cd, iptr);

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_ASLL_IMM(s2, POINTERSHIFT, REG_ITMP2);
			M_AADD(REG_ITMP2, s1, REG_ITMP1);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			M_AST_INTERN(s3, REG_ITMP1, OFFSET(java_objectarray, data[0]));
			break;


		case ICMD_BASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_AADD(s2, s1, REG_ITMP1);
			/* implicit null-pointer check */
			M_BST(REG_ZERO, REG_ITMP1, OFFSET(java_bytearray, data[0]));
			break;

		case ICMD_CASTORECONST:   /* ..., arrayref, index  ==> ...            */
		case ICMD_SASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_AADD(s2, s1, REG_ITMP1);
			M_AADD(s2, REG_ITMP1, REG_ITMP1);
			/* implicit null-pointer check */
			M_SST(REG_ZERO, REG_ITMP1, OFFSET(java_chararray, data[0]));
			break;

		case ICMD_IASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, 2, REG_ITMP2);
			M_AADD(REG_ITMP2, s1, REG_ITMP1);
			/* implicit null-pointer check */
			M_IST_INTERN(REG_ZERO, REG_ITMP1, OFFSET(java_intarray, data[0]));
			break;

		case ICMD_LASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, 3, REG_ITMP2);
			M_AADD(REG_ITMP2, s1, REG_ITMP1);
			/* implicit null-pointer check */
#if SIZEOF_VOID_P == 8
			M_LST_INTERN(REG_ZERO, REG_ITMP1, OFFSET(java_longarray, data[0]));
#else
			M_LST_INTERN(PACK_REGS(REG_ZERO, REG_ZERO), REG_ITMP1, OFFSET(java_longarray, data[0]));
#endif
			break;

		case ICMD_AASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ASLL_IMM(s2, POINTERSHIFT, REG_ITMP2);
			M_AADD(REG_ITMP2, s1, REG_ITMP1);
			/* implicit null-pointer check */
			M_AST_INTERN(REG_ZERO, REG_ITMP1, OFFSET(java_objectarray, data[0]));
			break;


		case ICMD_GETSTATIC:  /* ...  ==> ..., value                          */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = dseg_add_unique_address(cd, uf);

				codegen_add_patch_ref(cd, PATCHER_get_putstatic, uf, disp);
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = dseg_add_address(cd, &(fi->value));

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class))
					codegen_add_patch_ref(cd, PATCHER_clinit, fi->class, disp);
  			}

			M_ALD(REG_ITMP1, REG_PV, disp);

			switch (fieldtype) {
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD_INTERN(d, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
#if SIZEOF_VOID_P == 8
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
#else
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP23_PACKED);
#endif
				M_LLD_INTERN(d, REG_ITMP1, 0);
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

				codegen_add_patch_ref(cd, PATCHER_get_putstatic, uf, disp);
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = dseg_add_address(cd, &(fi->value));

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class))
					codegen_add_patch_ref(cd, PATCHER_clinit, fi->class, disp);
  			}

			M_ALD(REG_ITMP1, REG_PV, disp);

			switch (fieldtype) {
			case TYPE_INT:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_IST_INTERN(s1, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
#if SIZEOF_VOID_P == 8
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
#else
				s1 = emit_load_s1(jd, iptr, REG_ITMP23_PACKED);
#endif
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

		case ICMD_PUTSTATICCONST: /* ...  ==> ...                             */
		                          /* val = value (in current instruction)     */
		                          /* following NOP)                           */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = dseg_add_unique_address(cd, uf);

				codegen_add_patch_ref(cd, PATCHER_get_putstatic, uf, disp);
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = dseg_add_address(cd, &(fi->value));

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class))
					codegen_add_patch_ref(cd, PATCHER_clinit, fi->class, disp);
  			}

			M_ALD(REG_ITMP1, REG_PV, disp);

			switch (fieldtype) {
			case TYPE_INT:
				M_IST_INTERN(REG_ZERO, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				M_LST_INTERN(REG_ZERO, REG_ITMP1, 0);
				break;
			case TYPE_ADR:
				M_AST_INTERN(REG_ZERO, REG_ITMP1, 0);
				break;
			case TYPE_FLT:
				M_FST_INTERN(REG_ZERO, REG_ITMP1, 0);
				break;
			case TYPE_DBL:
				M_DST_INTERN(REG_ZERO, REG_ITMP1, 0);
				break;
			}
			break;


		case ICMD_GETFIELD:   /* ...  ==> ..., value                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			emit_nullpointer_check(cd, iptr, s1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = 0;

				codegen_add_patch_ref(cd, PATCHER_get_putfield, uf, 0);
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = fi->offset;
			}

			switch (fieldtype) {
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD(d, s1, disp);
				break;
			case TYPE_LNG:
#if SIZEOF_VOID_P == 8
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_LLD(d, s1, disp);
#else
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP23_PACKED);
				M_LLD_GETFIELD(d, s1, disp);
#endif
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

		case ICMD_PUTFIELD:   /* ..., objectref, value  ==> ...               */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			emit_nullpointer_check(cd, iptr, s1);

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

#if SIZEOF_VOID_P == 8
			if (IS_INT_LNG_TYPE(fieldtype))
				s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			else
				s2 = emit_load_s2(jd, iptr, REG_FTMP1);
#else
			if (IS_INT_LNG_TYPE(fieldtype)) {
				if (IS_2_WORD_TYPE(fieldtype))
					s2 = emit_load_s2(jd, iptr, REG_ITMP23_PACKED);
				else
					s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			}
			else
				s2 = emit_load_s2(jd, iptr, REG_FTMP2);
#endif

			if (INSTRUCTION_IS_UNRESOLVED(iptr))
				codegen_add_patch_ref(cd, PATCHER_get_putfield, uf, 0);

			switch (fieldtype) {
			case TYPE_INT:
				M_IST(s2, s1, disp);
				break;
			case TYPE_LNG:
				M_LST(s2, s1, disp);
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

		case ICMD_PUTFIELDCONST:  /* ..., objectref  ==> ...                  */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			emit_nullpointer_check(cd, iptr, s1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = 0;

				codegen_add_patch_ref(cd, PATCHER_get_putfield, uf, 0);
			}
			else {
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = fi->offset;
			}

			switch (fieldtype) {
			case TYPE_INT:
				M_IST(REG_ZERO, s1, disp);
				break;
			case TYPE_LNG:
				M_LST(REG_ZERO, s1, disp);
				break;
			case TYPE_ADR:
				M_AST(REG_ZERO, s1, disp);
				break;
			case TYPE_FLT:
				M_FST(REG_ZERO, s1, disp);
				break;
			case TYPE_DBL:
				M_DST(REG_ZERO, s1, disp);
				break;
			}
			break;


		/* branch operations **************************************************/

		case ICMD_ATHROW:       /* ..., objectref ==> ... (, objectref)       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1_XPTR);
			
#ifdef ENABLE_VERIFIER
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uc = iptr->sx.s23.s2.uc;

				codegen_add_patch_ref(cd, PATCHER_athrow_areturn, uc, 0);
			}
#endif /* ENABLE_VERIFIER */

			disp = dseg_add_functionptr(cd, asm_handle_exception);
			M_ALD(REG_ITMP2, REG_PV, disp);
			M_JSR(REG_ITMP2_XPC, REG_ITMP2);
			M_NOP;
			M_NOP;              /* nop ensures that XPC is less than the end */
			                    /* of basic block                            */
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
			emit_bccz(cd, iptr->dst.block, iptr->opc - ICMD_IFNULL, s1, BRANCH_OPT_NONE);
			break;

		case ICMD_IFEQ:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 0)
				emit_beqz(cd, iptr->dst.block, s1);
			else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				emit_beq(cd, iptr->dst.block, s1, REG_ITMP2);
			}
			break;

		case ICMD_IFLT:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 0)
				emit_bltz(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.i >= -32768) && (iptr->sx.val.i <= 32767))
					M_CMPLT_IMM(s1, iptr->sx.val.i, REG_ITMP1);
				else {
					ICONST(REG_ITMP2, iptr->sx.val.i);
					M_CMPLT(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_bnez(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IFLE:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 0)
				emit_blez(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.i >= -32769) && (iptr->sx.val.i <= 32766)) {
					M_CMPLT_IMM(s1, iptr->sx.val.i + 1, REG_ITMP1);
					emit_bnez(cd, iptr->dst.block, REG_ITMP1);
				}
				else {
					ICONST(REG_ITMP2, iptr->sx.val.i);
					M_CMPGT(s1, REG_ITMP2, REG_ITMP1);
					emit_beqz(cd, iptr->dst.block, REG_ITMP1);
				}
			}
			break;

		case ICMD_IFNE:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 0)
				emit_bnez(cd, iptr->dst.block, s1);
			else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				emit_bne(cd, iptr->dst.block, s1, REG_ITMP2);
			}
			break;

		case ICMD_IFGT:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 0)
				emit_bgtz(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.i >= -32769) && (iptr->sx.val.i <= 32766)) {
					M_CMPLT_IMM(s1, iptr->sx.val.i + 1, REG_ITMP1);
					emit_beqz(cd, iptr->dst.block, REG_ITMP1);
				}
				else {
					ICONST(REG_ITMP2, iptr->sx.val.i);
					M_CMPGT(s1, REG_ITMP2, REG_ITMP1);
					emit_bnez(cd, iptr->dst.block, REG_ITMP1);
				}
			}
			break;

		case ICMD_IFGE:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 0)
				emit_bgez(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.i >= -32768) && (iptr->sx.val.i <= 32767))
					M_CMPLT_IMM(s1, iptr->sx.val.i, REG_ITMP1);
				else {
					ICONST(REG_ITMP2, iptr->sx.val.i);
					M_CMPLT(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_beqz(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IF_LEQ:       /* ..., value ==> ...                         */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_beqz(cd, iptr->dst.block, s1);
			else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				emit_beq(cd, iptr->dst.block, s1, REG_ITMP2);
			}
#else
			if (iptr->sx.val.l == 0) {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_OR(GET_LOW_REG(s1), GET_HIGH_REG(s1), REG_ITMP3);
				emit_beqz(cd, iptr->dst.block, REG_ITMP3);
			}
			else {
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
				ICONST(REG_ITMP2, iptr->sx.val.l >> 32);
				M_XOR(s1, REG_ITMP2, REG_ITMP2);
				s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
				ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_XOR(s1, REG_ITMP3, REG_ITMP3);
				M_OR(REG_ITMP2, REG_ITMP3, REG_ITMP3);
				emit_beqz(cd, iptr->dst.block, REG_ITMP3);
			}
#endif
			break;

		case ICMD_IF_LLT:       /* ..., value ==> ...                         */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_bltz(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.l >= -32768) && (iptr->sx.val.l <= 32767))
					M_CMPLT_IMM(s1, iptr->sx.val.l, REG_ITMP3);
				else {
					LCONST(REG_ITMP2, iptr->sx.val.l);
					M_CMPLT(s1, REG_ITMP2, REG_ITMP3);
				}
				emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			}
#else
			if (iptr->sx.val.l == 0) {
				/* if high word is less than zero, the whole long is too */
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
				emit_bltz(cd, iptr->dst.block, s1);
			}
			else {
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
				ICONST(REG_ITMP2, iptr->sx.val.l >> 32);
				M_CMPLT(s1, REG_ITMP2, REG_ITMP3);
				emit_bnez(cd, iptr->dst.block, REG_ITMP3);
				s2 = emit_load_s1_low(jd, iptr, REG_ITMP3);
				M_BNE(s1, REG_ITMP2, 5); /* XXX */
				M_NOP;
				ICONST(REG_ITMP2, iptr->sx.val.l & 0xffffffff);
				M_CMPULT(s2, REG_ITMP2, REG_ITMP3);
				emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			}
#endif
			break;

		case ICMD_IF_LLE:       /* ..., value ==> ...                         */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_blez(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.l >= -32769) && (iptr->sx.val.l <= 32766)) {
					M_CMPLT_IMM(s1, iptr->sx.val.l + 1, REG_ITMP2);
					emit_bnez(cd, iptr->dst.block, REG_ITMP2);
				}
				else {
					LCONST(REG_ITMP2, iptr->sx.val.l);
					M_CMPGT(s1, REG_ITMP2, REG_ITMP3);
					emit_beqz(cd, iptr->dst.block, REG_ITMP3);
				}
			}
#else
			if (iptr->sx.val.l == 0) {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_BGTZ(GET_HIGH_REG(s1), 5); /* XXX */
				M_NOP;
				emit_bltz(cd, iptr->dst.block, GET_HIGH_REG(s1));
				emit_beqz(cd, iptr->dst.block, GET_LOW_REG(s1));
			}
			else {
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
				ICONST(REG_ITMP2, iptr->sx.val.l >> 32);
				M_CMPLT(s1, REG_ITMP2, REG_ITMP3);
				emit_bnez(cd, iptr->dst.block, REG_ITMP3);
				s2 = emit_load_s1_low(jd, iptr, REG_ITMP3);
				M_BNE(s1, REG_ITMP2, 5); /* XXX */
				M_NOP;
				ICONST(REG_ITMP2, iptr->sx.val.l & 0xffffffff);
				M_CMPUGT(s2, REG_ITMP2, REG_ITMP3);
				emit_beqz(cd, iptr->dst.block, REG_ITMP3);
			}
#endif
			break;

		case ICMD_IF_LNE:       /* ..., value ==> ...                         */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_bnez(cd, iptr->dst.block, s1);
			else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				emit_bne(cd, iptr->dst.block, s1, REG_ITMP2);
			}
#else
			if (iptr->sx.val.l == 0) {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_OR(GET_LOW_REG(s1), GET_HIGH_REG(s1), REG_ITMP3);
				emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			}
			else {
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
				ICONST(REG_ITMP2, iptr->sx.val.l >> 32);
				M_XOR(s1, REG_ITMP2, REG_ITMP2);
				s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
				ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_XOR(s1, REG_ITMP3, REG_ITMP3);
				M_OR(REG_ITMP2, REG_ITMP3, REG_ITMP3);
				emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			}
#endif
			break;

		case ICMD_IF_LGT:       /* ..., value ==> ...                         */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_bgtz(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.l >= -32769) && (iptr->sx.val.l <= 32766)) {
					M_CMPLT_IMM(s1, iptr->sx.val.l + 1, REG_ITMP2);
					emit_beqz(cd, iptr->dst.block, REG_ITMP2);
				}
				else {
					LCONST(REG_ITMP2, iptr->sx.val.l);
					M_CMPGT(s1, REG_ITMP2, REG_ITMP3);
					emit_bnez(cd, iptr->dst.block, REG_ITMP3);
				}
			}
#else
			if (iptr->sx.val.l == 0) {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				emit_bgtz(cd, iptr->dst.block, GET_HIGH_REG(s1));
				M_BLTZ(GET_HIGH_REG(s1), 3); /* XXX */
				M_NOP;
				emit_bnez(cd, iptr->dst.block, GET_LOW_REG(s1));
			}
			else {
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
				ICONST(REG_ITMP2, iptr->sx.val.l >> 32);
				M_CMPGT(s1, REG_ITMP2, REG_ITMP3);
				emit_bnez(cd, iptr->dst.block, REG_ITMP3);
				s2 = emit_load_s1_low(jd, iptr, REG_ITMP3);
				M_BNE(s1, REG_ITMP2, 5); /* XXX */
				M_NOP;
				ICONST(REG_ITMP2, iptr->sx.val.l & 0xffffffff);
				M_CMPUGT(s2, REG_ITMP2, REG_ITMP3);
				emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			}
#endif
			break;

		case ICMD_IF_LGE:       /* ..., value ==> ...                         */

#if SIZEOF_VOID_P == 8
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_bgez(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.l >= -32768) && (iptr->sx.val.l <= 32767)) {
					M_CMPLT_IMM(s1, iptr->sx.val.l, REG_ITMP3);
				}
				else {
					LCONST(REG_ITMP2, iptr->sx.val.l);
					M_CMPLT(s1, REG_ITMP2, REG_ITMP3);
				}
				emit_beqz(cd, iptr->dst.block, REG_ITMP3);
			}
#else
			if (iptr->sx.val.l == 0) {
				/* if high word is greater equal zero, the whole long is too */
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
				emit_bgez(cd, iptr->dst.block, s1);
			}
			else {
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
				ICONST(REG_ITMP2, iptr->sx.val.l >> 32);
				M_CMPGT(s1, REG_ITMP2, REG_ITMP3);
				emit_bnez(cd, iptr->dst.block, REG_ITMP3);
				s2 = emit_load_s1_low(jd, iptr, REG_ITMP3);
				M_BNE(s1, REG_ITMP2, 5); /* XXX */
				M_NOP;
				ICONST(REG_ITMP2, iptr->sx.val.l & 0xffffffff);
				M_CMPULT(s2, REG_ITMP2, REG_ITMP3);
				emit_beqz(cd, iptr->dst.block, REG_ITMP3);
			}
#endif
			break;

		case ICMD_IF_ICMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ACMPEQ:    /* op1 = target JavaVM pc                     */
#if SIZEOF_VOID_P == 8
		case ICMD_IF_LCMPEQ:
#endif

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_beq(cd, iptr->dst.block, s1, s2);
			break;

#if SIZEOF_VOID_P == 4
		case ICMD_IF_LCMPEQ:    /* ..., value, value ==> ...                  */
		                        /* op1 = target JavaVM pc                     */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_BNE(s1, s2, 3); /* XXX TWISTI: uff, that is a problem */
			M_NOP;
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			emit_beq(cd, iptr->dst.block, s1, s2);
			break;
#endif

		case ICMD_IF_ICMPNE:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ACMPNE:    /* op1 = target JavaVM pc                     */
#if SIZEOF_VOID_P == 8
		case ICMD_IF_LCMPNE:
#endif

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_bne(cd, iptr->dst.block, s1, s2);
			break;

#if SIZEOF_VOID_P == 4
		case ICMD_IF_LCMPNE:    /* ..., value, value ==> ...                  */

			/* TODO: could be optimized (XOR or SUB) */
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			emit_bne(cd, iptr->dst.block, s1, s2);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			emit_bne(cd, iptr->dst.block, s1, s2);
			break;
#endif

		case ICMD_IF_ICMPLT:    /* ..., value, value ==> ...                  */
#if SIZEOF_VOID_P == 8
		case ICMD_IF_LCMPLT:    /* op1 = target JavaVM pc                     */
#endif

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMPLT(s1, s2, REG_ITMP3);
			emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			break;

#if SIZEOF_VOID_P == 4
		case ICMD_IF_LCMPLT:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMPLT(s1, s2, REG_ITMP3);
			emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			M_CMPGT(s1, s2, REG_ITMP3);
			/* load low-bits before the branch, so we know the distance */
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_BNEZ(REG_ITMP3, 4); /* XXX */
			M_NOP;
			M_CMPULT(s1, s2, REG_ITMP3);
			emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			break;
#endif

		case ICMD_IF_ICMPGT:    /* ..., value, value ==> ...                  */
#if SIZEOF_VOID_P == 8
		case ICMD_IF_LCMPGT:    /* op1 = target JavaVM pc                     */
#endif

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMPGT(s1, s2, REG_ITMP3);
			emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			break;

#if SIZEOF_VOID_P == 4
		case ICMD_IF_LCMPGT:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMPGT(s1, s2, REG_ITMP3);
			emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			M_CMPLT(s1, s2, REG_ITMP3);
			/* load low-bits before the branch, so we know the distance */
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_BNEZ(REG_ITMP3, 4); /* XXX */
			M_NOP;
			M_CMPUGT(s1, s2, REG_ITMP3);
			emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			break;
#endif

		case ICMD_IF_ICMPLE:    /* ..., value, value ==> ...                  */
#if SIZEOF_VOID_P == 8
		case ICMD_IF_LCMPLE:    /* op1 = target JavaVM pc                     */
#endif

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMPGT(s1, s2, REG_ITMP3);
			emit_beqz(cd, iptr->dst.block, REG_ITMP3);
			break;

#if SIZEOF_VOID_P == 4
		case ICMD_IF_LCMPLE:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMPLT(s1, s2, REG_ITMP3);
			emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			M_CMPGT(s1, s2, REG_ITMP3);
			/* load low-bits before the branch, so we know the distance */
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_BNEZ(REG_ITMP3, 4); /* XXX */
			M_NOP;
			M_CMPUGT(s1, s2, REG_ITMP3);
			emit_beqz(cd, iptr->dst.block, REG_ITMP3);
			break;
#endif

		case ICMD_IF_ICMPGE:    /* ..., value, value ==> ...                  */
#if SIZEOF_VOID_P == 8
		case ICMD_IF_LCMPGE:    /* op1 = target JavaVM pc                     */
#endif

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMPLT(s1, s2, REG_ITMP3);
			emit_beqz(cd, iptr->dst.block, REG_ITMP3);
			break;

#if SIZEOF_VOID_P == 4
		case ICMD_IF_LCMPGE:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMPGT(s1, s2, REG_ITMP3);
			emit_bnez(cd, iptr->dst.block, REG_ITMP3);
			M_CMPLT(s1, s2, REG_ITMP3);
			/* load low-bits before the branch, so we know the distance */
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_BNEZ(REG_ITMP3, 4); /* XXX */
			M_NOP;
			M_CMPULT(s1, s2, REG_ITMP3);
			emit_beqz(cd, iptr->dst.block, REG_ITMP3);
			break;
#endif

		case ICMD_IRETURN:      /* ..., retvalue ==> ...                      */
#if SIZEOF_VOID_P == 8
		case ICMD_LRETURN:
#endif

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
				uc = iptr->sx.s23.s2.uc;

				codegen_add_patch_ref(cd, PATCHER_athrow_areturn, uc, 0);
			}
#endif /* ENABLE_VERIFIER */
			goto nowperformreturn;

#if SIZEOF_VOID_P == 4
		case ICMD_LRETURN:      /* ..., retvalue ==> ...                      */

			s1 = emit_load_s1(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(s1, REG_RESULT_PACKED);
			goto nowperformreturn;
#endif

	    case ICMD_FRETURN:      /* ..., retvalue ==> ...                      */
			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_FRESULT);
			M_FLTMOVE(s1, REG_FRESULT);
			goto nowperformreturn;

	    case ICMD_DRETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_FRESULT);
			M_DBLMOVE(s1, REG_FRESULT);
			goto nowperformreturn;

		case ICMD_RETURN:      /* ...  ==> ...                                */

			REPLACEMENT_POINT_RETURN(cd, iptr);

nowperformreturn:
			{
			s4 i, p;
			
			p = cd->stackframesize;

#if !defined(NDEBUG)
			if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
				emit_verbosecall_exit(jd);
#endif

#if defined(ENABLE_THREADS)
			if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
				disp = dseg_add_functionptr(cd, LOCK_monitor_exit);
				M_ALD(REG_ITMP3, REG_PV, disp);

				/* we need to save the proper return value */

				switch (iptr->opc) {
				case ICMD_IRETURN:
				case ICMD_ARETURN:
#if SIZEOF_VOID_P == 8
				case ICMD_LRETURN:
#endif
					M_ALD(REG_A0, REG_SP, rd->memuse * 8);
					M_JSR(REG_RA, REG_ITMP3);
					M_AST(REG_RESULT, REG_SP, rd->memuse * 8);  /* delay slot */
					break;
#if SIZEOF_VOID_P == 4
				case ICMD_LRETURN:
					M_ALD(REG_A0, REG_SP, rd->memuse * 8);
					M_LST(REG_RESULT_PACKED, REG_SP, rd->memuse * 8);
					M_JSR(REG_RA, REG_ITMP3);
					M_NOP;
					break;
#endif
				case ICMD_FRETURN:
				case ICMD_DRETURN:
					M_ALD(REG_A0, REG_SP, rd->memuse * 8);
					M_JSR(REG_RA, REG_ITMP3);
					M_DST(REG_FRESULT, REG_SP, rd->memuse * 8); /* delay slot */
					break;
				case ICMD_RETURN:
					M_JSR(REG_RA, REG_ITMP3);
					M_ALD(REG_A0, REG_SP, rd->memuse * 8); /* delay*/
					break;
				}

				/* and now restore the proper return value */

				switch (iptr->opc) {
				case ICMD_IRETURN:
				case ICMD_ARETURN:
#if SIZEOF_VOID_P == 8
				case ICMD_LRETURN:
#endif
					M_ALD(REG_RESULT, REG_SP, rd->memuse * 8);
					break;
#if SIZEOF_VOID_P == 4
				case ICMD_LRETURN:
					M_LLD(REG_RESULT_PACKED, REG_SP, rd->memuse * 8);
					break;
#endif
				case ICMD_FRETURN:
				case ICMD_DRETURN:
					M_DLD(REG_FRESULT, REG_SP, rd->memuse * 8);
					break;
				}
			}
#endif

			/* restore return address                                         */

			if (!jd->isleafmethod) {
				p--; M_ALD(REG_RA, REG_SP, p * 8);
			}

			/* restore saved registers                                        */

			for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
				p--; M_ALD(rd->savintregs[i], REG_SP, p * 8);
			}
			for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
				p--; M_DLD(rd->savfltregs[i], REG_SP, p * 8);
			}

			/* deallocate stack and return                                    */

			if (cd->stackframesize) {
				s4 lo, hi;

				disp = cd->stackframesize * 8;
				lo = (short) (disp);
				hi = (short) (((disp) - lo) >> 16);

				if (hi == 0) {
					M_RET(REG_RA);
					M_AADD_IMM(REG_SP, lo, REG_SP);             /* delay slot */
				} else {
					M_LUI(REG_ITMP3,hi);
					M_AADD_IMM(REG_ITMP3,lo,REG_ITMP3);
					M_RET(REG_RA);
					M_AADD(REG_ITMP3,REG_SP,REG_SP);            /* delay slot */
				}

			} else {
				M_RET(REG_RA);
				M_NOP;
			}

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
				{M_INTMOVE(s1, REG_ITMP1);}
			else if (l <= 32768) {
				M_IADD_IMM(s1, -l, REG_ITMP1);
				}
			else {
				ICONST(REG_ITMP2, l);
				M_ISUB(s1, REG_ITMP2, REG_ITMP1);
				}

			/* number of targets */
			i = i - l + 1;

			/* range check */

			M_CMPULT_IMM(REG_ITMP1, i, REG_ITMP2);
			emit_beqz(cd, table[0].block, REG_ITMP2);

			/* build jump table top down and use address of lowest entry */

			table += i;

			while (--i >= 0) {
				dseg_add_target(cd, table->block); 
				--table;
			}
			}

			/* length of dataseg after last dseg_add_target is used by load */

			M_ASLL_IMM(REG_ITMP1, POINTERSHIFT, REG_ITMP1);
			M_AADD(REG_ITMP1, REG_PV, REG_ITMP2);
			M_ALD(REG_ITMP2, REG_ITMP2, -(cd->dseglen));
			M_JMP(REG_ITMP2);
			M_NOP;
			ALIGNCODENOP;
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
				ICONST(REG_ITMP2, lookup->value);
				emit_beq(cd, lookup->target.block, s1, REG_ITMP2);
				++lookup;
			}

			emit_br(cd, iptr->sx.s23.s3.lookupdefault.block);
			ALIGNCODENOP;
			break;
			}


		case ICMD_BUILTIN:      /* ..., arg1 ==> ...                          */

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

			/* copy arguments to registers or stack location                  */

			for (s3 = s3 - 1; s3 >= 0; s3--) {
				var = VAR(iptr->sx.s23.s2.args[s3]);
				d   = md->params[s3].regoff;

				if (var->flags & PREALLOC)
					continue;

				if (IS_INT_LNG_TYPE(var->type)) {
#if SIZEOF_VOID_P == 8
					if (!md->params[s3].inmemory) {
						s1 = emit_load(jd, iptr, var, d);
						M_INTMOVE(s1, d);
					}
					else  {
						s1 = emit_load(jd, iptr, var, REG_ITMP1);
						M_LST(s1, REG_SP, d * 8);
					}
#else
					if (!md->params[s3].inmemory) {
						s1 = emit_load(jd, iptr, var, d);

						if (IS_2_WORD_TYPE(var->type))
							M_LNGMOVE(s1, d);
						else
							M_INTMOVE(s1, d);
					}
					else {
						if (IS_2_WORD_TYPE(var->type)) {
							s1 = emit_load(jd, iptr, var, REG_ITMP12_PACKED);
							M_LST(s1, REG_SP, d * 8);
						}
						else {
							s1 = emit_load(jd, iptr, var, REG_ITMP1);
							M_IST(s1, REG_SP, d * 8);
						}
					}
#endif
				}
				else {
					if (!md->params[s3].inmemory) {
#if SIZEOF_VOID_P == 8
						s1 = emit_load(jd, iptr, var, d);
						if (IS_2_WORD_TYPE(var->type))
							M_DMOV(s1, d);
						else
							M_FMOV(s1, d);
#else
						if ((s3 == 0) ||
							((s3 == 1) && IS_FLT_DBL_TYPE(md->paramtypes[0].type))) {
							s1 = emit_load(jd, iptr, var, d);
							if (IS_2_WORD_TYPE(var->type))
								M_DBLMOVE(s1, d);
							else
								M_FLTMOVE(s1, d);
						}
						else {
							if (IS_2_WORD_TYPE(var->type)) {
								s1 = emit_load(jd, iptr, var, REG_FTMP1);
								M_MFC1(GET_LOW_REG(d), s1);
								M_MFC1(GET_HIGH_REG(d), s1 + 1);
								M_NOP;
							}
							else {
								s1 = emit_load(jd, iptr, var, d);
								M_MFC1(d, s1);
								M_NOP;
							}
						}	
#endif
					}
					else {
						s1 = emit_load(jd, iptr, var, REG_FTMP1);
						if (IS_2_WORD_TYPE(var->type))
							M_DST(s1, REG_SP, d * 8);
						else
							M_FST(s1, REG_SP, d * 8);
					}
				}
			}

			switch (iptr->opc) {
			case ICMD_BUILTIN:
				disp = dseg_add_functionptr(cd, bte->fp);

				M_ALD(REG_ITMP3, REG_PV, disp);  /* built-in-function pointer */

				/* generate the actual call */

				/* TWISTI: i actually don't know the reason for using
				   REG_ITMP3 here instead of REG_PV. */

				M_JSR(REG_RA, REG_ITMP3);
				M_NOP;
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_LDA(REG_PV, REG_RA, -disp);

				emit_exception_check(cd, iptr);
				break;

			case ICMD_INVOKESPECIAL:
				emit_nullpointer_check(cd, iptr, REG_A0);
				/* fall through */

			case ICMD_INVOKESTATIC:
				if (lm == NULL) {
					disp = dseg_add_unique_address(cd, um);

					codegen_add_patch_ref(cd, PATCHER_invokestatic_special, um,
										  disp);
				}
				else
					disp = dseg_add_address(cd, lm->stubroutine);

				M_ALD(REG_PV, REG_PV, disp);          /* method pointer in pv */

				/* generate the actual call */

				M_JSR(REG_RA, REG_PV);
				M_NOP;
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_LDA(REG_PV, REG_RA, -disp);
				break;

			case ICMD_INVOKEVIRTUAL:
				emit_nullpointer_check(cd, iptr, REG_A0);

				if (lm == NULL) {
					codegen_add_patch_ref(cd, PATCHER_invokevirtual, um, 0);

					s1 = 0;
				}
				else
					s1 = OFFSET(vftbl_t, table[0]) +
						sizeof(methodptr) * lm->vftblindex;

				/* implicit null-pointer check */
				M_ALD(REG_METHODPTR, REG_A0, OFFSET(java_objectheader, vftbl));
				M_ALD(REG_PV, REG_METHODPTR, s1);

				/* generate the actual call */

				M_JSR(REG_RA, REG_PV);
				M_NOP;
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_LDA(REG_PV, REG_RA, -disp);
				break;

			case ICMD_INVOKEINTERFACE:
				emit_nullpointer_check(cd, iptr, REG_A0);

				if (lm == NULL) {
					codegen_add_patch_ref(cd, PATCHER_invokeinterface, um, 0);

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

				M_JSR(REG_RA, REG_PV);
				M_NOP;
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_LDA(REG_PV, REG_RA, -disp);
				break;
			}

			/* store return value */

			d = md->returntype.type;

			if (d != TYPE_VOID) {
				if (IS_INT_LNG_TYPE(d)) {
#if SIZEOF_VOID_P == 8
					s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT);
					M_INTMOVE(REG_RESULT, s1);
#else
					if (IS_2_WORD_TYPE(d)) {
						s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);
						M_LNGMOVE(REG_RESULT_PACKED, s1);
					}
					else {
						s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT);
						M_INTMOVE(REG_RESULT, s1);
					}
#endif
				}
				else {
					s1 = codegen_reg_of_dst(jd, iptr, REG_FRESULT);
					if (IS_2_WORD_TYPE(d))
						M_DMOV(REG_FRESULT, s1);
					else
						M_FMOV(REG_FRESULT, s1);
				}
				emit_store_dst(jd, iptr, s1);
			}
			break;


		case ICMD_CHECKCAST:  /* ..., objectref ==> ..., objectref            */

			if (!(iptr->flags.bits & INS_FLAG_ARRAY)) {
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
					emit_label_beqz(cd, BRANCH_LABEL_1, s1);

					cr   = iptr->sx.s23.s3.c.ref;
					disp = dseg_add_unique_s4(cd, 0);         /* super->flags */

					codegen_add_patch_ref(cd, PATCHER_checkcast_instanceof_flags,
										  cr, disp);

					M_ILD(REG_ITMP2, REG_PV, disp);
					M_AND_IMM(REG_ITMP2, ACC_INTERFACE, REG_ITMP2);
					emit_label_beqz(cd, BRANCH_LABEL_2, REG_ITMP2);
				}

				/* interface checkcast code */

				if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
					if (super == NULL) {
						cr = iptr->sx.s23.s3.c.ref;

						codegen_add_patch_ref(cd, PATCHER_checkcast_interface,
											  cr, 0);
					}
					else {
						emit_label_beqz(cd, BRANCH_LABEL_3, s1);
					}

					M_ALD(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));
					M_ILD(REG_ITMP3, REG_ITMP2,
						  OFFSET(vftbl_t, interfacetablelength));
					M_IADD_IMM(REG_ITMP3, -superindex, REG_ITMP3);
					emit_classcast_check(cd, iptr, ICMD_IFLE, REG_ITMP3, s1);

					M_ALD(REG_ITMP3, REG_ITMP2,
						  OFFSET(vftbl_t, interfacetable[0]) -
						  superindex * sizeof(methodptr*));
					emit_classcast_check(cd, iptr, ICMD_IFEQ, REG_ITMP3, s1);

					if (super == NULL)
						emit_label_br(cd, BRANCH_LABEL_4);
					else
						emit_label(cd, BRANCH_LABEL_3);
				}

				/* class checkcast code */

				if ((super == NULL) || !(super->flags & ACC_INTERFACE)) {
					if (super == NULL) {
						emit_label(cd, BRANCH_LABEL_2);

						cr   = iptr->sx.s23.s3.c.ref;
						disp = dseg_add_unique_address(cd, NULL);

						codegen_add_patch_ref(cd,
											  PATCHER_checkcast_instanceof_class,
											  cr, disp);
					}
					else {
						disp = dseg_add_address(cd, super->vftbl);

						emit_label_beqz(cd, BRANCH_LABEL_5, s1);
					}

					M_ALD(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));
					M_ALD(REG_ITMP3, REG_PV, disp);

					CODEGEN_CRITICAL_SECTION_START;

					M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, baseval));
					/* 				if (s1 != REG_ITMP1) { */
					/* 					M_ILD(REG_ITMP1, REG_ITMP3, OFFSET(vftbl_t, baseval)); */
					/* 					M_ILD(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, diffval)); */
					/* #if defined(ENABLE_THREADS) */
					/* 					codegen_threadcritstop(cd, cd->mcodeptr - cd->mcodebase); */
					/* #endif */
					/* 					M_ISUB(REG_ITMP2, REG_ITMP1, REG_ITMP2); */
					/* 				} else { */
					M_ILD(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, baseval));
					M_ISUB(REG_ITMP2, REG_ITMP3, REG_ITMP2); 
					M_ALD(REG_ITMP3, REG_PV, disp);
					M_ILD(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, diffval));

					CODEGEN_CRITICAL_SECTION_END;

					/* 				} */
					M_CMPULT(REG_ITMP3, REG_ITMP2, REG_ITMP3);
					emit_classcast_check(cd, iptr, ICMD_IFNE, REG_ITMP3, s1);

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
				s1 = emit_load_s1(jd, iptr, REG_A0);
				M_INTMOVE(s1, REG_A0);

				if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
					cr   = iptr->sx.s23.s3.c.ref;
					disp = dseg_add_unique_address(cd, NULL);

					codegen_add_patch_ref(cd, PATCHER_builtin_arraycheckcast,
										  cr, disp);
				}
				else
					disp = dseg_add_address(cd, iptr->sx.s23.s3.c.cls);

				M_ALD(REG_A1, REG_PV, disp);
				disp = dseg_add_functionptr(cd, BUILTIN_arraycheckcast);
				M_ALD(REG_ITMP3, REG_PV, disp);
				M_JSR(REG_RA, REG_ITMP3);
				M_NOP;

				s1 = emit_load_s1(jd, iptr, REG_ITMP1);
				emit_classcast_check(cd, iptr, ICMD_IFEQ, REG_RESULT, s1);

				d = codegen_reg_of_dst(jd, iptr, s1);
			}

			M_INTMOVE(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INSTANCEOF: /* ..., objectref ==> ..., intresult            */

			{
			classinfo *super;
			s4         superindex;

			super = iptr->sx.s23.s3.c.cls;

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
				emit_label_beqz(cd, BRANCH_LABEL_1, s1);

				cr   = iptr->sx.s23.s3.c.ref;
				disp = dseg_add_unique_s4(cd, 0);             /* super->flags */

				codegen_add_patch_ref(cd, PATCHER_checkcast_instanceof_flags,
									  cr, disp);

				M_ILD(REG_ITMP3, REG_PV, disp);
				M_AND_IMM(REG_ITMP3, ACC_INTERFACE, REG_ITMP3);
				emit_label_beqz(cd, BRANCH_LABEL_2, REG_ITMP3);
			}

			/* interface instanceof code */

			if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
				if (super == NULL) {
					cr = iptr->sx.s23.s3.c.ref;

					codegen_add_patch_ref(cd, PATCHER_instanceof_interface,
										  cr, 0);
				}
				else {
					emit_label_beqz(cd, BRANCH_LABEL_3, s1);
				}

				M_ALD(REG_ITMP1, s1, OFFSET(java_objectheader, vftbl));
				M_ILD(REG_ITMP3, REG_ITMP1,
					  OFFSET(vftbl_t, interfacetablelength));
				M_IADD_IMM(REG_ITMP3, -superindex, REG_ITMP3);
				M_BLEZ(REG_ITMP3, 3);
				M_NOP;
				M_ALD(REG_ITMP1, REG_ITMP1,
					  OFFSET(vftbl_t, interfacetable[0]) -
					  superindex * sizeof(methodptr*));
				M_CMPULT(REG_ZERO, REG_ITMP1, d);      /* REG_ITMP1 != 0  */

				if (super == NULL)
					emit_label_br(cd, BRANCH_LABEL_4);
				else
					emit_label(cd, BRANCH_LABEL_3);
			}

			/* class instanceof code */

			if ((super == NULL) || !(super->flags & ACC_INTERFACE)) {
				if (super == NULL) {
					emit_label(cd, BRANCH_LABEL_2);

					cr   = iptr->sx.s23.s3.c.ref;
					disp = dseg_add_unique_address(cd, NULL);

					codegen_add_patch_ref(cd, PATCHER_checkcast_instanceof_class,
										  cr, disp);
				}
				else {
					disp = dseg_add_address(cd, super->vftbl);

					emit_label_beqz(cd, BRANCH_LABEL_5, s1);
				}

				M_ALD(REG_ITMP1, s1, OFFSET(java_objectheader, vftbl));
				M_ALD(REG_ITMP2, REG_PV, disp);

				CODEGEN_CRITICAL_SECTION_START;

				M_ILD(REG_ITMP1, REG_ITMP1, OFFSET(vftbl_t, baseval));
				M_ILD(REG_ITMP3, REG_ITMP2, OFFSET(vftbl_t, baseval));
				M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, diffval));

				CODEGEN_CRITICAL_SECTION_END;

				M_ISUB(REG_ITMP1, REG_ITMP3, REG_ITMP1); 
				M_CMPULT(REG_ITMP2, REG_ITMP1, d);
				M_XOR_IMM(d, 1, d);

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

			for (s1 = iptr->s1.argcount; --s1 >= 0; ) {

				var = VAR(iptr->sx.s23.s2.args[s1]);

				/* copy SAVEDVAR sizes to stack */

				if (!(var->flags & PREALLOC)) {
					s2 = emit_load(jd, iptr, var, REG_ITMP1);
#if SIZEOF_VOID_P == 8
					M_LST(s2, REG_SP, s1 * 8);
#else
					M_IST(s2, REG_SP, (s1 + 2) * 8);
#endif
				}
			}

			/* a0 = dimension count */

			ICONST(REG_A0, iptr->s1.argcount);

			/* is patcher function set? */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				cr   = iptr->sx.s23.s3.c.ref;
				disp = dseg_add_unique_address(cd, NULL);

				codegen_add_patch_ref(cd, PATCHER_builtin_multianewarray,
									  cr, disp);
			}
			else
				disp = dseg_add_address(cd, iptr->sx.s23.s3.c.cls);

			/* a1 = arraydescriptor */

			M_ALD(REG_A1, REG_PV, disp);

			/* a2 = pointer to dimensions = stack pointer */

#if SIZEOF_VOID_P == 8
			M_MOV(REG_SP, REG_A2);
#else
			M_AADD_IMM(REG_SP, 4*4, REG_A2);
#endif

			disp = dseg_add_functionptr(cd, BUILTIN_multianewarray);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_JSR(REG_RA, REG_ITMP3);
			M_NOP;

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
		
	MCODECHECK(64); /* XXX require smaller number? */

	/* At the end of a basic block we may have to append some nops,
	   because the patcher stub calling code might be longer than the
	   actual instruction. So codepatching does not change the
	   following block unintentionally. */

	if (cd->mcodeptr < cd->lastmcodeptr) {
		while (cd->mcodeptr < cd->lastmcodeptr)
			M_NOP;
	}

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

	M_ALD_INTERN(REG_ITMP1, REG_PV, -2 * SIZEOF_VOID_P);  /* codeinfo pointer */
	M_ALD_INTERN(REG_PV, REG_PV, -3 * SIZEOF_VOID_P);  /* pointer to compiler */
	M_JMP(REG_PV);
	M_NOP;
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
	s4           i, j;
	s4           t;
	s4           s1, s2, disp;
	s4           funcdisp;              /* displacement of the function       */

	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;

	/* initialize variables */

	md = m->parseddesc;
	nativeparams = (m->flags & ACC_STATIC) ? 2 : 1;

	/* calculate stack frame size */

	cd->stackframesize =
		1 +                             /* return address                     */
		sizeof(stackframeinfo) / SIZEOF_VOID_P +
		sizeof(localref_table) / SIZEOF_VOID_P +
		md->paramcount +                /* for saving arguments over calls    */
#if SIZEOF_VOID_P == 4
		5 +                             /* additional save space (MIPS32)     */
#endif
		1 +                             /* for saving return address          */
		nmd->memuse;

	/* adjust stackframe size for 16-byte alignment */

	if (cd->stackframesize & 1)
		cd->stackframesize++;

	/* create method header */

	(void) dseg_add_unique_address(cd, code);              /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 8); /* FrameSize       */
	(void) dseg_add_unique_s4(cd, 0);                      /* IsSync          */
	(void) dseg_add_unique_s4(cd, 0);                      /* IsLeaf          */
	(void) dseg_add_unique_s4(cd, 0);                      /* IntSave         */
	(void) dseg_add_unique_s4(cd, 0);                      /* FltSave         */
	(void) dseg_addlinenumbertablesize(cd);
	(void) dseg_add_unique_s4(cd, 0);                      /* ExTableSize     */

	/* generate stub code */

	M_LDA(REG_SP, REG_SP, -cd->stackframesize * 8); /* build up stackframe    */
	M_AST(REG_RA, REG_SP, (cd->stackframesize - 1) * 8); /* store RA          */

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_enter(jd);
#endif

	/* get function address (this must happen before the stackframeinfo) */

	funcdisp = dseg_add_functionptr(cd, f);

#if !defined(WITH_STATIC_CLASSPATH)
	if (f == NULL)
		codegen_add_patch_ref(cd, PATCHER_resolve_native, m, funcdisp);
#endif

	/* save integer and float argument registers */

#if SIZEOF_VOID_P == 8
	for (i = 0, j = 0; i < md->paramcount && i < INT_ARG_CNT; i++) {
		if (IS_INT_LNG_TYPE(md->params[i].type)) {
			s1 = md->params[i].regoff;
			M_AST(s1, REG_SP, j * 8);
			j++;
		}
	}
#else
	for (i = 0, j = 5; i < md->paramcount && i < INT_ARG_CNT; i++) {
		if (IS_INT_LNG_TYPE(md->params[i].type)) {
			if (!md->params[i].inmemory) {
 				s1 = md->params[i].regoff;

				if (IS_2_WORD_TYPE(md->params[i].type))
					M_LST(s1, REG_SP, j * 8);
				else
					M_IST(s1, REG_SP, j * 8);

				j++;
			}
		}
	}
#endif

	for (i = 0; i < md->paramcount && i < FLT_ARG_CNT; i++) {
		if (IS_FLT_DBL_TYPE(md->params[i].type)) {
			s1 = md->params[i].regoff;

			if (IS_2_WORD_TYPE(md->params[i].type))
				M_DST(s1, REG_SP, j * 8);
			else
				M_FST(s1, REG_SP, j * 8);

			j++;
		}
	}

	/* prepare data structures for native function call */

	M_AADD_IMM(REG_SP, (cd->stackframesize - 1) * 8, REG_A0);
	M_MOV(REG_PV, REG_A1);
	M_AADD_IMM(REG_SP, cd->stackframesize * 8, REG_A2);
	M_ALD(REG_A3, REG_SP, (cd->stackframesize - 1) * 8);
	disp = dseg_add_functionptr(cd, codegen_start_native_call);
	M_ALD(REG_ITMP3, REG_PV, disp);
	M_JSR(REG_RA, REG_ITMP3);
	M_NOP; /* XXX fill me! */

	/* restore integer and float argument registers */

#if SIZEOF_VOID_P == 8
	for (i = 0, j = 0; i < md->paramcount && i < INT_ARG_CNT; i++) {
		if (IS_INT_LNG_TYPE(md->params[i].type)) {
			s1 = md->params[i].regoff;
			M_LLD(s1, REG_SP, j * 8);
			j++;
		}
	}
#else
	for (i = 0, j = 5; i < md->paramcount && i < INT_ARG_CNT; i++) {
		if (IS_INT_LNG_TYPE(md->params[i].type)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;

				if (IS_2_WORD_TYPE(md->params[i].type))
					M_LLD(s1, REG_SP, j * 8);
				else
					M_ILD(s1, REG_SP, j * 8);

				j++;
			}
		}
	}
#endif

	for (i = 0; i < md->paramcount && i < FLT_ARG_CNT; i++) {
		if (IS_FLT_DBL_TYPE(md->params[i].type)) {
			s1 = md->params[i].regoff;

			if (IS_2_WORD_TYPE(md->params[i].type))
				M_DLD(s1, REG_SP, j * 8);
			else
				M_FLD(s1, REG_SP, j * 8);

			j++;
		}
	}

	/* copy or spill arguments to new locations */

	for (i = md->paramcount - 1, j = i + nativeparams; i >= 0; i--, j--) {
		t = md->params[i].type;

		if (IS_INT_LNG_TYPE(t)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;
				s2 = nmd->params[j].regoff;

				if (!nmd->params[j].inmemory) {
#if SIZEOF_VOID_P == 8
					M_INTMOVE(s1, s2);
#else
					if (IS_2_WORD_TYPE(t))
						M_LNGMOVE(s1, s2);
					else
						M_INTMOVE(s1, s2);
#endif
				}
				else {
#if SIZEOF_VOID_P == 8
					M_LST(s1, REG_SP, s2 * 8);
#else
					if (IS_2_WORD_TYPE(t))
						M_LST(s1, REG_SP, s2 * 4);
					else
						M_IST(s1, REG_SP, s2 * 4);
#endif
				}
			}
			else {
				s1 = md->params[i].regoff + cd->stackframesize;
				s2 = nmd->params[j].regoff;

#if SIZEOF_VOID_P == 8
				M_LLD(REG_ITMP1, REG_SP, s1 * 8);
				M_LST(REG_ITMP1, REG_SP, s2 * 8);
#else
				if (IS_2_WORD_TYPE(t)) {
					M_LLD(REG_ITMP12_PACKED, REG_SP, s1 * 8);
					M_LST(REG_ITMP12_PACKED, REG_SP, s2 * 4);
				}
				else {
					M_ILD(REG_ITMP1, REG_SP, s1 * 8);
					M_IST(REG_ITMP1, REG_SP, s2 * 4);
				}
#endif
			}
		}
		else {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;
				s2 = nmd->params[j].regoff;

				if (!nmd->params[j].inmemory) {
#if SIZEOF_VOID_P == 8
					if (IS_2_WORD_TYPE(t))
						M_DMOV(s1, s2);
					else
						M_FMOV(s1, s2);
#else
					/* On MIPS32 float arguments for native functions
					   can never be in float argument registers, since
					   the first argument is _always_ an integer
					   argument (JNIenv) */

					if (IS_2_WORD_TYPE(t)) {
						/* double high/low order is endian
						   independent: even numbered holds low
						   32-bits, odd numbered high 32-bits */

						M_MFC1(GET_LOW_REG(s2), s1);           /* low 32-bits */
						M_MFC1(GET_HIGH_REG(s2), s1 + 1);     /* high 32-bits */
					}
					else
						M_MFC1(s2, s1);
#endif
				}
				else {
#if SIZEOF_VOID_P == 8
					if (IS_2_WORD_TYPE(t))
						M_DST(s1, REG_SP, s2 * 8);
					else
						M_FST(s1, REG_SP, s2 * 8);
#else
					/* s1 may have been originally in 2 int registers,
					   but was moved out by the native function
					   argument(s), just get low register */

					if (IS_2_WORD_TYPE(t))
						M_DST(GET_LOW_REG(s1), REG_SP, s2 * 4);
					else
						M_FST(GET_LOW_REG(s1), REG_SP, s2 * 4);
#endif
				}
			}
			else {
				s1 = md->params[i].regoff + cd->stackframesize;
				s2 = nmd->params[j].regoff;

				if (IS_2_WORD_TYPE(t)) {
					M_DLD(REG_FTMP1, REG_SP, s1 * 8);
					M_DST(REG_FTMP1, REG_SP, s2 * 8);
				}
				else {
					M_FLD(REG_FTMP1, REG_SP, s1 * 8);
					M_FST(REG_FTMP1, REG_SP, s2 * 8);
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

	/* do the native function call */

	M_ALD(REG_ITMP3, REG_PV, funcdisp); /* load adress of native method       */
	M_JSR(REG_RA, REG_ITMP3);           /* call native method                 */
	M_NOP;                              /* delay slot                         */

	/* save return value */

	switch (md->returntype.type) {
#if SIZEOF_VOID_P == 8
	case TYPE_INT:
	case TYPE_LNG:
	case TYPE_ADR:
		M_LST(REG_RESULT, REG_SP, 0 * 8);
		break;
	case TYPE_FLT:
	case TYPE_DBL:
		M_DST(REG_FRESULT, REG_SP, 0 * 8);
		break;
#else
	case TYPE_INT:
	case TYPE_ADR:
		M_IST(REG_RESULT, REG_SP, 1*4 + 0 * 8);
		break;
	case TYPE_LNG:
		M_LST(REG_RESULT_PACKED, REG_SP, 1*4 + 0 * 8);
		break;
	case TYPE_FLT:
	case TYPE_DBL:
		M_DST(REG_FRESULT, REG_SP, 1*4 + 0 * 8);
		break;
#endif
	case TYPE_VOID:
		break;
	}

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_exit(jd);
#endif

	/* remove native stackframe info */

	M_AADD_IMM(REG_SP, (cd->stackframesize - 1) * 8, REG_A0);
	disp = dseg_add_functionptr(cd, codegen_finish_native_call);
	M_ALD(REG_ITMP3, REG_PV, disp);
	M_JSR(REG_RA, REG_ITMP3);
	M_NOP; /* XXX fill me! */
	M_MOV(REG_RESULT, REG_ITMP1_XPTR);

	/* restore return value */

	switch (md->returntype.type) {
#if SIZEOF_VOID_P == 8
	case TYPE_INT:
	case TYPE_LNG:
	case TYPE_ADR:
		M_LLD(REG_RESULT, REG_SP, 0 * 8);
		break;
	case TYPE_FLT:
	case TYPE_DBL:
		M_DLD(REG_FRESULT, REG_SP, 0 * 8);
		break;
#else
	case TYPE_INT:
	case TYPE_ADR:
		M_ILD(REG_RESULT, REG_SP, 1*4 + 0 * 8);
		break;
	case TYPE_LNG:
		M_LLD(REG_RESULT_PACKED, REG_SP, 1*4 + 0 * 8);
		break;
	case TYPE_FLT:
	case TYPE_DBL:
		M_DLD(REG_FRESULT, REG_SP, 1*4 + 0 * 8);
		break;
#endif
	case TYPE_VOID:
		break;
	}

	M_ALD(REG_RA, REG_SP, (cd->stackframesize - 1) * 8); /* load RA           */

	/* check for exception */

	M_BNEZ(REG_ITMP1_XPTR, 2);          /* if no exception then return        */
	M_LDA(REG_SP, REG_SP, cd->stackframesize * 8); /* DELAY SLOT              */

	M_RET(REG_RA);                      /* return to caller                   */
	M_NOP;                              /* DELAY SLOT                         */

	/* handle exception */
	
	disp = dseg_add_functionptr(cd, asm_handle_nat_exception);
	M_ALD(REG_ITMP3, REG_PV, disp);     /* load asm exception handler address */
	M_JMP(REG_ITMP3);                   /* jump to asm exception handler      */
	M_ASUB_IMM(REG_RA, 4, REG_ITMP2_XPC); /* get exception address (DELAY)    */

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

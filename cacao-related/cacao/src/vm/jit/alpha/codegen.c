/* src/vm/jit/alpha/codegen.c - machine code generator for Alpha

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

   $Id: codegen.c 7886 2007-05-07 21:34:01Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>

#include "vm/types.h"

#include "md.h"
#include "md-abi.h"

#include "vm/jit/alpha/arch.h"
#include "vm/jit/alpha/codegen.h"

#include "mm/memory.h"

#include "native/jni.h"
#include "native/native.h"

#include "threads/lock-common.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/vm.h"

#include "vm/jit/abi.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/codegen-common.h"
#include "vm/jit/dseg.h"
#include "vm/jit/emit-common.h"
#include "vm/jit/jit.h"
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

	d           = 0;
	fieldtype   = 0;
	lm          = NULL;
	um          = NULL;
	bte         = NULL;
	currentline = 0;

	{
	s4 i, p, t, l;
	s4 savedregs_num;

	savedregs_num = (jd->isleafmethod) ? 0 : 1;       /* space to save the RA */

	/* space to save used callee saved registers */

	savedregs_num += (INT_SAV_CNT - rd->savintreguse);
	savedregs_num += (FLT_SAV_CNT - rd->savfltreguse);

	cd->stackframesize = rd->memuse + savedregs_num;

#if defined(ENABLE_THREADS)        /* space to save argument of monitor_enter */
	if (checksync && (m->flags & ACC_SYNCHRONIZED))
		cd->stackframesize++;
#endif

	/* create method header */

#if 0
	cd->stackframesize = (cd->stackframesize + 1) & ~1; /* align stack to 16-bytes */
#endif

	(void) dseg_add_unique_address(cd, code);              /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 8); /* FrameSize       */

#if defined(ENABLE_THREADS)
	/* IsSync contains the offset relative to the stack pointer for the
	   argument of monitor_exit used in the exception handler. Since the
	   offset could be zero and give a wrong meaning of the flag it is
	   offset by one.
	*/

	if (checksync && (m->flags & ACC_SYNCHRONIZED))
		(void) dseg_add_unique_s4(cd, (rd->memuse + 1) * 8);       /* IsSync  */
	else
#endif
		(void) dseg_add_unique_s4(cd, 0);                          /* IsSync  */

	(void) dseg_add_unique_s4(cd, jd->isleafmethod);               /* IsLeaf  */
	(void) dseg_add_unique_s4(cd, INT_SAV_CNT - rd->savintreguse); /* IntSave */
	(void) dseg_add_unique_s4(cd, FLT_SAV_CNT - rd->savfltreguse); /* FltSave */

	dseg_addlinenumbertablesize(cd);

	(void) dseg_add_unique_s4(cd, jd->exceptiontablelength);   /* ExTableSize */

	/* create exception table */

	for (ex = jd->exceptiontable; ex != NULL; ex = ex->down) {
		dseg_add_target(cd, ex->start);
   		dseg_add_target(cd, ex->end);
		dseg_add_target(cd, ex->handler);
		(void) dseg_add_unique_address(cd, ex->catchtype.any);
	}
	
	/* create stack frame (if necessary) */

	if (cd->stackframesize)
		M_LDA(REG_SP, REG_SP, -(cd->stackframesize * 8));

	/* save return address and used callee saved registers */

	p = cd->stackframesize;
	if (!jd->isleafmethod) {
		p--; M_AST(REG_RA, REG_SP, p * 8);
	}
	for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
		p--; M_LST(rd->savintregs[i], REG_SP, p * 8);
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

		s1 = md->params[p].regoff;

		if (IS_INT_LNG_TYPE(t)) {                    /* integer args          */
 			if (!md->params[p].inmemory) {           /* register arguments    */
 				if (!IS_INMEMORY(var->flags))
 					M_INTMOVE(s1, var->vv.regoff);
				else
 					M_LST(s1, REG_SP, var->vv.regoff * 8);
			}
			else {                                   /* stack arguments       */
 				if (!IS_INMEMORY(var->flags))
 					M_LLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) *8);
				else
					var->vv.regoff = cd->stackframesize + s1;
			}
		}
		else {                                       /* floating args         */
 			if (!md->params[p].inmemory) {           /* register arguments    */
 				if (!IS_INMEMORY(var->flags))
 					M_FLTMOVE(s1, var->vv.regoff);
 				else
 					M_DST(s1, REG_SP, var->vv.regoff * 8);
			}
			else {                                   /* stack arguments       */
 				if (!(var->flags & INMEMORY))
 					M_DLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8);
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

#if !defined(NDEBUG)
		if (opt_verbosecall) {
			M_LDA(REG_SP, REG_SP, -(INT_ARG_CNT + FLT_ARG_CNT) * 8);

			for (p = 0; p < INT_ARG_CNT; p++)
				M_LST(abi_registers_integer_argument[p], REG_SP, p * 8);

			for (p = 0; p < FLT_ARG_CNT; p++)
				M_DST(abi_registers_float_argument[p], REG_SP, (INT_ARG_CNT + p) * 8);

			s1 += INT_ARG_CNT + FLT_ARG_CNT;
		}
#endif /* !defined(NDEBUG) */

		/* decide which monitor enter function to call */

		if (m->flags & ACC_STATIC) {
			disp = dseg_add_address(cd, &m->class->object.header);
			M_ALD(REG_A0, REG_PV, disp);
		}
		else {
			M_BNEZ(REG_A0, 1);
			M_ALD_INTERN(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_NULLPOINTER);
		}

		M_AST(REG_A0, REG_SP, s1 * 8);
		disp = dseg_add_functionptr(cd, LOCK_monitor_enter);
		M_ALD(REG_PV, REG_PV, disp);
		M_JSR(REG_RA, REG_PV);
		disp = (s4) (cd->mcodeptr - cd->mcodebase);
		M_LDA(REG_PV, REG_RA, -disp);

#if !defined(NDEBUG)
		if (opt_verbosecall) {
			for (p = 0; p < INT_ARG_CNT; p++)
				M_LLD(abi_registers_integer_argument[p], REG_SP, p * 8);

			for (p = 0; p < FLT_ARG_CNT; p++)
				M_DLD(abi_registers_float_argument[p], REG_SP, (INT_ARG_CNT + p) * 8);

			M_LDA(REG_SP, REG_SP, (INT_ARG_CNT + FLT_ARG_CNT) * 8);
		}
#endif /* !defined(NDEBUG) */
	}			
#endif

	/* call trace function */

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_enter(jd);
#endif

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

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
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
				constant_classref *cr = iptr->sx.val.c.ref;

				disp = dseg_add_unique_address(cd, cr);

				/* XXX Only add the patcher, if this position needs to
				   be patched.  If there was a previous position which
				   resolved the same class, the returned displacement
				   of dseg_add_address is ok to use. */

				codegen_add_patch_ref(cd, PATCHER_resolve_classref_to_classinfo,
									  cr, disp);

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


		/* load/store/move/copy operations ************************************/

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
			M_ISUB(REG_ZERO, s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSUB(REG_ZERO, s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2L:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_L2I:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_IADD(s1, REG_ZERO, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2BYTE:   /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (has_ext_instr_set) {
				M_BSEXT(s1, d);
			} else {
				M_SLL_IMM(s1, 56, d);
				M_SRA_IMM( d, 56, d);
			}
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
			if (has_ext_instr_set) {
				M_SSEXT(s1, d);
			} else {
				M_SLL_IMM(s1, 48, d);
				M_SRA_IMM( d, 48, d);
			}
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
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 255)) {
				M_IADD_IMM(s1, iptr->sx.val.i, d);
			} else if ((iptr->sx.val.i > -256) && (iptr->sx.val.i < 0)) {
				M_ISUB_IMM(s1, (-iptr->sx.val.i), d);
			} else {
				/* XXX maybe use M_LDA? */
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_IADD(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LADD(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADDCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 255)) {
				M_LADD_IMM(s1, iptr->sx.val.l, d);
			} else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_LADD(s1, REG_ITMP2, d);
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
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 255)) {
				M_ISUB_IMM(s1, iptr->sx.val.i, d);
			} else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_ISUB(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LSUB(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUBCONST:  /* ..., value  ==> ..., value - constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 255)) {
				M_LSUB_IMM(s1, iptr->sx.val.l, d);
			} else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_LSUB(s1, REG_ITMP2, d);
			}
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
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 255)) {
				M_IMUL_IMM(s1, iptr->sx.val.i, d);
			} else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_IMUL(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_LMUL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LMULCONST:  /* ..., value  ==> ..., value * constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 255)) {
				M_LMUL_IMM(s1, iptr->sx.val.l, d);
			} else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_LMUL(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
		case ICMD_IREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_A0);
			s2 = emit_load_s2(jd, iptr, REG_A1);
			d = codegen_reg_of_dst(jd, iptr, REG_RESULT);
			emit_arithmetic_check(cd, iptr, s2);

			M_INTMOVE(s1, REG_A0);
			M_INTMOVE(s2, REG_A1);
			bte = iptr->sx.s23.s3.bte;
			disp = dseg_add_functionptr(cd, bte->fp);
			M_ALD(REG_PV, REG_PV, disp);
			M_JSR(REG_RA, REG_PV);
			disp = (s4) (cd->mcodeptr - cd->mcodebase);
			M_LDA(REG_PV, REG_RA, -disp);

			M_IADD(REG_RESULT, REG_ZERO, d); /* sign extend (bugfix for gcc -O2) */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
		case ICMD_LREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_A0);
			s2 = emit_load_s2(jd, iptr, REG_A1);
			d = codegen_reg_of_dst(jd, iptr, REG_RESULT);
			emit_arithmetic_check(cd, iptr, s2);

			M_INTMOVE(s1, REG_A0);
			M_INTMOVE(s2, REG_A1);
			bte = iptr->sx.s23.s3.bte;
			disp = dseg_add_functionptr(cd, bte->fp);
			M_ALD(REG_PV, REG_PV, disp);
			M_JSR(REG_RA, REG_PV);
			disp = (s4) (cd->mcodeptr - cd->mcodebase);
			M_LDA(REG_PV, REG_RA, -disp);

			M_INTMOVE(REG_RESULT, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIVPOW2:   /* ..., value  ==> ..., value << constant       */
		case ICMD_LDIVPOW2:   /* val.i = constant                             */
		                      
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.i <= 15) {
				M_LDA(REG_ITMP2, s1, (1 << iptr->sx.val.i) -1);
				M_CMOVGE(s1, s1, REG_ITMP2);
			} else {
				M_SRA_IMM(s1, 63, REG_ITMP2);
				M_SRL_IMM(REG_ITMP2, 64 - iptr->sx.val.i, REG_ITMP2);
				M_LADD(s1, REG_ITMP2, REG_ITMP2);
			}
			M_SRA_IMM(REG_ITMP2, iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND_IMM(s2, 0x1f, REG_ITMP3);
			M_SLL(s1, REG_ITMP3, d);
			M_IADD(d, REG_ZERO, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHLCONST:  /* ..., value  ==> ..., value << constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SLL_IMM(s1, iptr->sx.val.i & 0x1f, d);
			M_IADD(d, REG_ZERO, d);
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
		                      /* sx.val.i = constant                             */

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
            M_IZEXT(s1, d);
			M_SRL(d, REG_ITMP2, d);
			M_IADD(d, REG_ZERO, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
            M_IZEXT(s1, d);
			M_SRL_IMM(d, iptr->sx.val.i & 0x1f, d);
			M_IADD(d, REG_ZERO, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SLL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHLCONST:  /* ..., value  ==> ..., value << constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SLL_IMM(s1, iptr->sx.val.i & 0x3f, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHR:       /* ..., val1, val2  ==> ..., val1 >> val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SRA(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHRCONST:  /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SRA_IMM(s1, iptr->sx.val.i & 0x3f, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LUSHR:      /* ..., val1, val2  ==> ..., val1 >>> val2      */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SRL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SRL_IMM(s1, iptr->sx.val.i & 0x3f, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IAND:       /* ..., val1, val2  ==> ..., val1 & val2        */
		case ICMD_LAND:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 255)) {
				M_AND_IMM(s1, iptr->sx.val.i, d);
			} else if (iptr->sx.val.i == 0xffff) {
				M_CZEXT(s1, d);
			} else if (iptr->sx.val.i == 0xffffff) {
				M_ZAPNOT_IMM(s1, 0x07, d);
			} else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_AND(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s1 == d) {
				M_MOV(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			}
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 255)) {
				M_AND_IMM(s1, iptr->sx.val.i, d);
				M_BGEZ(s1, 3);
				M_ISUB(REG_ZERO, s1, d);
				M_AND_IMM(d, iptr->sx.val.i, d);
			} else if (iptr->sx.val.i == 0xffff) {
				M_CZEXT(s1, d);
				M_BGEZ(s1, 3);
				M_ISUB(REG_ZERO, s1, d);
				M_CZEXT(d, d);
			} else if (iptr->sx.val.i == 0xffffff) {
				M_ZAPNOT_IMM(s1, 0x07, d);
				M_BGEZ(s1, 3);
				M_ISUB(REG_ZERO, s1, d);
				M_ZAPNOT_IMM(d, 0x07, d);
			} else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_AND(s1, REG_ITMP2, d);
				M_BGEZ(s1, 3);
				M_ISUB(REG_ZERO, s1, d);
				M_AND(d, REG_ITMP2, d);
			}
			M_ISUB(REG_ZERO, d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 255)) {
				M_AND_IMM(s1, iptr->sx.val.l, d);
			} else if (iptr->sx.val.l == 0xffffL) {
				M_CZEXT(s1, d);
			} else if (iptr->sx.val.l == 0xffffffL) {
				M_ZAPNOT_IMM(s1, 0x07, d);
			} else if (iptr->sx.val.l == 0xffffffffL) {
				M_IZEXT(s1, d);
			} else if (iptr->sx.val.l == 0xffffffffffL) {
				M_ZAPNOT_IMM(s1, 0x1f, d);
			} else if (iptr->sx.val.l == 0xffffffffffffL) {
				M_ZAPNOT_IMM(s1, 0x3f, d);
			} else if (iptr->sx.val.l == 0xffffffffffffffL) {
				M_ZAPNOT_IMM(s1, 0x7f, d);
			} else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_AND(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s1 == d) {
				M_MOV(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			}
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 255)) {
				M_AND_IMM(s1, iptr->sx.val.l, d);
				M_BGEZ(s1, 3);
				M_LSUB(REG_ZERO, s1, d);
				M_AND_IMM(d, iptr->sx.val.l, d);
			} else if (iptr->sx.val.l == 0xffffL) {
				M_CZEXT(s1, d);
				M_BGEZ(s1, 3);
				M_LSUB(REG_ZERO, s1, d);
				M_CZEXT(d, d);
			} else if (iptr->sx.val.l == 0xffffffL) {
				M_ZAPNOT_IMM(s1, 0x07, d);
				M_BGEZ(s1, 3);
				M_LSUB(REG_ZERO, s1, d);
				M_ZAPNOT_IMM(d, 0x07, d);
			} else if (iptr->sx.val.l == 0xffffffffL) {
				M_IZEXT(s1, d);
				M_BGEZ(s1, 3);
				M_LSUB(REG_ZERO, s1, d);
				M_IZEXT(d, d);
			} else if (iptr->sx.val.l == 0xffffffffffL) {
 				M_ZAPNOT_IMM(s1, 0x1f, d);
				M_BGEZ(s1, 3);
				M_LSUB(REG_ZERO, s1, d);
				M_ZAPNOT_IMM(d, 0x1f, d);
			} else if (iptr->sx.val.l == 0xffffffffffffL) {
				M_ZAPNOT_IMM(s1, 0x3f, d);
				M_BGEZ(s1, 3);
				M_LSUB(REG_ZERO, s1, d);
				M_ZAPNOT_IMM(d, 0x3f, d);
			} else if (iptr->sx.val.l == 0xffffffffffffffL) {
				M_ZAPNOT_IMM(s1, 0x7f, d);
				M_BGEZ(s1, 3);
				M_LSUB(REG_ZERO, s1, d);
				M_ZAPNOT_IMM(d, 0x7f, d);
			} else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_AND(s1, REG_ITMP2, d);
				M_BGEZ(s1, 3);
				M_LSUB(REG_ZERO, s1, d);
				M_AND(d, REG_ITMP2, d);
			}
			M_LSUB(REG_ZERO, d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IOR:        /* ..., val1, val2  ==> ..., val1 | val2        */
		case ICMD_LOR:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_OR( s1,s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 255)) {
				M_OR_IMM(s1, iptr->sx.val.i, d);
			} else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_OR(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 255)) {
				M_OR_IMM(s1, iptr->sx.val.l, d);
			} else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_OR(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IXOR:       /* ..., val1, val2  ==> ..., val1 ^ val2        */
		case ICMD_LXOR:

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
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 255)) {
				M_XOR_IMM(s1, iptr->sx.val.i, d);
			} else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_XOR(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXORCONST:  /* ..., value  ==> ..., value ^ constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 255)) {
				M_XOR_IMM(s1, iptr->sx.val.l, d);
			} else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_XOR(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_LCMP:       /* ..., val1, val2  ==> ..., val1 cmp val2      */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_CMPLT(s1, s2, REG_ITMP3);
			M_CMPLT(s2, s1, REG_ITMP1);
			M_LSUB(REG_ITMP1, REG_ITMP3, d);
			emit_store_dst(jd, iptr, d);
			break;


		/* floating operations ************************************************/

		case ICMD_FNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_FMOVN(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_FMOVN(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (opt_noieee) {
				M_FADD(s1, s2, d);
			} else {
				if (d == s1 || d == s2) {
					M_FADDS(s1, s2, REG_FTMP3);
					M_TRAPB;
					M_FMOV(REG_FTMP3, d);
				} else {
					M_FADDS(s1, s2, d);
					M_TRAPB;
				}
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (opt_noieee) {
				M_DADD(s1, s2, d);
			} else {
				if (d == s1 || d == s2) {
					M_DADDS(s1, s2, REG_FTMP3);
					M_TRAPB;
					M_FMOV(REG_FTMP3, d);
				} else {
					M_DADDS(s1, s2, d);
					M_TRAPB;
				}
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (opt_noieee) {
				M_FSUB(s1, s2, d);
			} else {
				if (d == s1 || d == s2) {
					M_FSUBS(s1, s2, REG_FTMP3);
					M_TRAPB;
					M_FMOV(REG_FTMP3, d);
				} else {
					M_FSUBS(s1, s2, d);
					M_TRAPB;
				}
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (opt_noieee) {
				M_DSUB(s1, s2, d);
			} else {
				if (d == s1 || d == s2) {
					M_DSUBS(s1, s2, REG_FTMP3);
					M_TRAPB;
					M_FMOV(REG_FTMP3, d);
				} else {
					M_DSUBS(s1, s2, d);
					M_TRAPB;
				}
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (opt_noieee) {
				M_FMUL(s1, s2, d);
			} else {
				if (d == s1 || d == s2) {
					M_FMULS(s1, s2, REG_FTMP3);
					M_TRAPB;
					M_FMOV(REG_FTMP3, d);
				} else {
					M_FMULS(s1, s2, d);
					M_TRAPB;
				}
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DMUL:       /* ..., val1, val2  ==> ..., val1 *** val2      */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (opt_noieee) {
				M_DMUL(s1, s2, d);
			} else {
				if (d == s1 || d == s2) {
					M_DMULS(s1, s2, REG_FTMP3);
					M_TRAPB;
					M_FMOV(REG_FTMP3, d);
				} else {
					M_DMULS(s1, s2, d);
					M_TRAPB;
				}
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (opt_noieee) {
				M_FDIV(s1, s2, d);
			} else {
				if (d == s1 || d == s2) {
					M_FDIVS(s1, s2, REG_FTMP3);
					M_TRAPB;
					M_FMOV(REG_FTMP3, d);
				} else {
					M_FDIVS(s1, s2, d);
					M_TRAPB;
				}
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (opt_noieee) {
				M_DDIV(s1, s2, d);
			} else {
				if (d == s1 || d == s2) {
					M_DDIVS(s1, s2, REG_FTMP3);
					M_TRAPB;
					M_FMOV(REG_FTMP3, d);
				} else {
					M_DDIVS(s1, s2, d);
					M_TRAPB;
				}
			}
			emit_store_dst(jd, iptr, d);
			break;
		
		case ICMD_I2F:       /* ..., value  ==> ..., (float) value            */
		case ICMD_L2F:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			disp = dseg_add_unique_double(cd, 0.0);
			M_LST(s1, REG_PV, disp);
			M_DLD(d, REG_PV, disp);
			M_CVTLF(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2D:       /* ..., value  ==> ..., (double) value           */
		case ICMD_L2D:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			disp = dseg_add_unique_double(cd, 0.0);
			M_LST(s1, REG_PV, disp);
			M_DLD(d, REG_PV, disp);
			M_CVTLD(d, d);
			emit_store_dst(jd, iptr, d);
			break;
			
		case ICMD_F2I:       /* ..., value  ==> ..., (int) value              */
		case ICMD_D2I:
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			disp = dseg_add_unique_double(cd, 0.0);
			M_CVTDL_C(s1, REG_FTMP2);
			M_CVTLI(REG_FTMP2, REG_FTMP3);
			M_DST(REG_FTMP3, REG_PV, disp);
			M_ILD(d, REG_PV, disp);
			emit_store_dst(jd, iptr, d);
			break;
		
		case ICMD_F2L:       /* ..., value  ==> ..., (long) value             */
		case ICMD_D2L:
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			disp = dseg_add_unique_double(cd, 0.0);
			M_CVTDL_C(s1, REG_FTMP2);
			M_DST(REG_FTMP2, REG_PV, disp);
			M_LLD(d, REG_PV, disp);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_F2D:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_CVTFDS(s1, d);
			M_TRAPB;
			emit_store_dst(jd, iptr, d);
			break;
					
		case ICMD_D2F:       /* ..., value  ==> ..., (float) value            */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (opt_noieee) {
				M_CVTDF(s1, d);
			} else {
				M_CVTDFS(s1, d);
				M_TRAPB;
			}
			emit_store_dst(jd, iptr, d);
			break;
		
		case ICMD_FCMPL:      /* ..., val1, val2  ==> ..., val1 fcmpl val2    */
		case ICMD_DCMPL:
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			if (opt_noieee) {
				M_LSUB_IMM(REG_ZERO, 1, d);
				M_FCMPEQ(s1, s2, REG_FTMP3);
				M_FBEQZ (REG_FTMP3, 1);        /* jump over next instructions */
				M_CLR   (d);
				M_FCMPLT(s2, s1, REG_FTMP3);
				M_FBEQZ (REG_FTMP3, 1);        /* jump over next instruction  */
				M_LADD_IMM(REG_ZERO, 1, d);
			} else {
				M_LSUB_IMM(REG_ZERO, 1, d);
				M_FCMPEQS(s1, s2, REG_FTMP3);
				M_TRAPB;
				M_FBEQZ (REG_FTMP3, 1);        /* jump over next instructions */
				M_CLR   (d);
				M_FCMPLTS(s2, s1, REG_FTMP3);
				M_TRAPB;
				M_FBEQZ (REG_FTMP3, 1);        /* jump over next instruction  */
				M_LADD_IMM(REG_ZERO, 1, d);
			}
			emit_store_dst(jd, iptr, d);
			break;
			
		case ICMD_FCMPG:      /* ..., val1, val2  ==> ..., val1 fcmpg val2    */
		case ICMD_DCMPG:
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			if (opt_noieee) {
				M_LADD_IMM(REG_ZERO, 1, d);
				M_FCMPEQ(s1, s2, REG_FTMP3);
				M_FBEQZ (REG_FTMP3, 1);        /* jump over next instruction  */
				M_CLR   (d);
				M_FCMPLT(s1, s2, REG_FTMP3);
				M_FBEQZ (REG_FTMP3, 1);        /* jump over next instruction  */
				M_LSUB_IMM(REG_ZERO, 1, d);
			} else {
				M_LADD_IMM(REG_ZERO, 1, d);
				M_FCMPEQS(s1, s2, REG_FTMP3);
				M_TRAPB;
				M_FBEQZ (REG_FTMP3, 1);        /* jump over next instruction  */
				M_CLR   (d);
				M_FCMPLTS(s1, s2, REG_FTMP3);
				M_TRAPB;
				M_FBEQZ (REG_FTMP3, 1);        /* jump over next instruction  */
				M_LSUB_IMM(REG_ZERO, 1, d);
			}
			emit_store_dst(jd, iptr, d);
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
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			if (has_ext_instr_set) {
				M_LADD(s2, s1, REG_ITMP1);
				M_BLDU(d, REG_ITMP1, OFFSET (java_bytearray, data[0]));
				M_BSEXT(d, d);
			}
			else {
				M_LADD(s2, s1, REG_ITMP1);
				M_LLD_U(REG_ITMP2, REG_ITMP1, OFFSET(java_bytearray, data[0]));
				M_LDA(REG_ITMP1, REG_ITMP1, OFFSET(java_bytearray, data[0])+1);
				M_EXTQH(REG_ITMP2, REG_ITMP1, d);
				M_SRA_IMM(d, 56, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_CALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			if (has_ext_instr_set) {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_SLDU(d, REG_ITMP1, OFFSET(java_chararray, data[0]));
			}
			else {
				M_LADD (s2, s1, REG_ITMP1);
				M_LADD (s2, REG_ITMP1, REG_ITMP1);
				M_LLD_U(REG_ITMP2, REG_ITMP1, OFFSET(java_chararray, data[0]));
				M_LDA  (REG_ITMP1, REG_ITMP1, OFFSET(java_chararray, data[0]));
				M_EXTWL(REG_ITMP2, REG_ITMP1, d);
			}
			emit_store_dst(jd, iptr, d);
			break;			

		case ICMD_SALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			if (has_ext_instr_set) {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_SLDU( d, REG_ITMP1, OFFSET (java_shortarray, data[0]));
				M_SSEXT(d, d);
			} else {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_LLD_U(REG_ITMP2, REG_ITMP1, OFFSET(java_shortarray, data[0]));
				M_LDA(REG_ITMP1, REG_ITMP1, OFFSET(java_shortarray, data[0])+2);
				M_EXTQH(REG_ITMP2, REG_ITMP1, d);
				M_SRA_IMM(d, 48, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_S4ADDQ(s2, s1, REG_ITMP1);
			M_ILD(d, REG_ITMP1, OFFSET(java_intarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_S8ADDQ(s2, s1, REG_ITMP1);
			M_LLD(d, REG_ITMP1, OFFSET(java_longarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_S4ADDQ(s2, s1, REG_ITMP1);
			M_FLD(d, REG_ITMP1, OFFSET(java_floatarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_S8ADDQ(s2, s1, REG_ITMP1);
			M_DLD(d, REG_ITMP1, OFFSET(java_doublearray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_AALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_SAADDQ(s2, s1, REG_ITMP1);
			M_ALD(d, REG_ITMP1, OFFSET(java_objectarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_BASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			if (has_ext_instr_set) {
				M_LADD(s2, s1, REG_ITMP1);
				M_BST(s3, REG_ITMP1, OFFSET(java_bytearray, data[0]));
			}
			else {
				M_LADD(s2, s1, REG_ITMP1);
				M_LLD_U(REG_ITMP2, REG_ITMP1, OFFSET(java_bytearray, data[0]));
				M_LDA(REG_ITMP1, REG_ITMP1, OFFSET(java_bytearray, data[0]));
				M_INSBL(s3, REG_ITMP1, REG_ITMP3);
				M_MSKBL(REG_ITMP2, REG_ITMP1, REG_ITMP2);
				M_OR(REG_ITMP2, REG_ITMP3, REG_ITMP2);
				M_LST_U(REG_ITMP2, REG_ITMP1, 0);
			}
			break;

		case ICMD_CASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			if (has_ext_instr_set) {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_SST(s3, REG_ITMP1, OFFSET(java_chararray, data[0]));
			}
			else {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_LLD_U(REG_ITMP2, REG_ITMP1, OFFSET(java_chararray, data[0]));
				M_LDA(REG_ITMP1, REG_ITMP1, OFFSET(java_chararray, data[0]));
				M_INSWL(s3, REG_ITMP1, REG_ITMP3);
				M_MSKWL(REG_ITMP2, REG_ITMP1, REG_ITMP2);
				M_OR(REG_ITMP2, REG_ITMP3, REG_ITMP2);
				M_LST_U(REG_ITMP2, REG_ITMP1, 0);
			}
			break;

		case ICMD_SASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			if (has_ext_instr_set) {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_SST(s3, REG_ITMP1, OFFSET(java_shortarray, data[0]));
			}
			else {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_LLD_U(REG_ITMP2, REG_ITMP1, OFFSET(java_shortarray, data[0]));
				M_LDA(REG_ITMP1, REG_ITMP1, OFFSET(java_shortarray, data[0]));
				M_INSWL(s3, REG_ITMP1, REG_ITMP3);
				M_MSKWL(REG_ITMP2, REG_ITMP1, REG_ITMP2);
				M_OR(REG_ITMP2, REG_ITMP3, REG_ITMP2);
				M_LST_U(REG_ITMP2, REG_ITMP1, 0);
			}
			break;

		case ICMD_IASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_S4ADDQ(s2, s1, REG_ITMP1);
			M_IST(s3, REG_ITMP1, OFFSET(java_intarray, data[0]));
			break;

		case ICMD_LASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_S8ADDQ(s2, s1, REG_ITMP1);
			M_LST(s3, REG_ITMP1, OFFSET(java_longarray, data[0]));
			break;

		case ICMD_FASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_FTMP3);
			M_S4ADDQ(s2, s1, REG_ITMP1);
			M_FST(s3, REG_ITMP1, OFFSET(java_floatarray, data[0]));
			break;

		case ICMD_DASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_FTMP3);
			M_S8ADDQ(s2, s1, REG_ITMP1);
			M_DST(s3, REG_ITMP1, OFFSET(java_doublearray, data[0]));
			break;

		case ICMD_AASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_A0);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_A1);

			M_INTMOVE(s1, REG_A0);
			M_INTMOVE(s3, REG_A1);

			disp = dseg_add_functionptr(cd, BUILTIN_canstore);
			M_ALD(REG_PV, REG_PV, disp);
			M_JSR(REG_RA, REG_PV);
			disp = (s4) (cd->mcodeptr - cd->mcodebase);
			M_LDA(REG_PV, REG_RA, -disp);
			emit_exception_check(cd, iptr);

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_SAADDQ(s2, s1, REG_ITMP1);
			M_AST(s3, REG_ITMP1, OFFSET(java_objectarray, data[0]));
			break;


		case ICMD_BASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			if (has_ext_instr_set) {
				M_LADD(s2, s1, REG_ITMP1);
				M_BST(REG_ZERO, REG_ITMP1, OFFSET(java_bytearray, data[0]));
			}
			else {
				M_LADD(s2, s1, REG_ITMP1);
				M_LLD_U(REG_ITMP2, REG_ITMP1, OFFSET(java_bytearray, data[0]));
				M_LDA(REG_ITMP1, REG_ITMP1, OFFSET(java_bytearray, data[0]));
				M_INSBL(REG_ZERO, REG_ITMP1, REG_ITMP3);
				M_MSKBL(REG_ITMP2, REG_ITMP1, REG_ITMP2);
				M_OR(REG_ITMP2, REG_ITMP3, REG_ITMP2);
				M_LST_U(REG_ITMP2, REG_ITMP1, 0);
			}
			break;

		case ICMD_CASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			if (has_ext_instr_set) {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_SST(REG_ZERO, REG_ITMP1, OFFSET(java_chararray, data[0]));
			}
			else {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_LLD_U(REG_ITMP2, REG_ITMP1, OFFSET(java_chararray, data[0]));
				M_LDA(REG_ITMP1, REG_ITMP1, OFFSET(java_chararray, data[0]));
				M_INSWL(REG_ZERO, REG_ITMP1, REG_ITMP3);
				M_MSKWL(REG_ITMP2, REG_ITMP1, REG_ITMP2);
				M_OR(REG_ITMP2, REG_ITMP3, REG_ITMP2);
				M_LST_U(REG_ITMP2, REG_ITMP1, 0);
			}
			break;

		case ICMD_SASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			if (has_ext_instr_set) {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_SST(REG_ZERO, REG_ITMP1, OFFSET(java_shortarray, data[0]));
			}
			else {
				M_LADD(s2, s1, REG_ITMP1);
				M_LADD(s2, REG_ITMP1, REG_ITMP1);
				M_LLD_U(REG_ITMP2, REG_ITMP1, OFFSET(java_shortarray, data[0]));
				M_LDA(REG_ITMP1, REG_ITMP1, OFFSET(java_shortarray, data[0]));
				M_INSWL(REG_ZERO, REG_ITMP1, REG_ITMP3);
				M_MSKWL(REG_ITMP2, REG_ITMP1, REG_ITMP2);
				M_OR(REG_ITMP2, REG_ITMP3, REG_ITMP2);
				M_LST_U(REG_ITMP2, REG_ITMP1, 0);
			}
			break;

		case ICMD_IASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_S4ADDQ(s2, s1, REG_ITMP1);
			M_IST(REG_ZERO, REG_ITMP1, OFFSET(java_intarray, data[0]));
			break;

		case ICMD_LASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_S8ADDQ(s2, s1, REG_ITMP1);
			M_LST(REG_ZERO, REG_ITMP1, OFFSET(java_longarray, data[0]));
			break;

		case ICMD_AASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_SAADDQ(s2, s1, REG_ITMP1);
			M_AST(REG_ZERO, REG_ITMP1, OFFSET(java_objectarray, data[0]));
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
					codegen_add_patch_ref(cd, PATCHER_initialize_class, fi->class,
										  0);
  			}

			M_ALD(REG_ITMP1, REG_PV, disp);
			switch (fieldtype) {
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD(d, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_LLD(d, REG_ITMP1, 0);
				break;
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ALD(d, REG_ITMP1, 0);
				break;
			case TYPE_FLT:
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_FLD(d, REG_ITMP1, 0);
				break;
			case TYPE_DBL:				
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_DLD(d, REG_ITMP1, 0);
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
					codegen_add_patch_ref(cd, PATCHER_initialize_class, fi->class,
										  0);
  			}

			M_ALD(REG_ITMP1, REG_PV, disp);
			switch (fieldtype) {
			case TYPE_INT:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_IST(s1, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_LST(s1, REG_ITMP1, 0);
				break;
			case TYPE_ADR:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_AST(s1, REG_ITMP1, 0);
				break;
			case TYPE_FLT:
				s1 = emit_load_s1(jd, iptr, REG_FTMP2);
				M_FST(s1, REG_ITMP1, 0);
				break;
			case TYPE_DBL:
				s1 = emit_load_s1(jd, iptr, REG_FTMP2);
				M_DST(s1, REG_ITMP1, 0);
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
					codegen_add_patch_ref(cd, PATCHER_initialize_class, fi->class,
										  0);
  			}
			
			M_ALD(REG_ITMP1, REG_PV, disp);
			switch (fieldtype) {
			case TYPE_INT:
				M_IST(REG_ZERO, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				M_LST(REG_ZERO, REG_ITMP1, 0);
				break;
			case TYPE_ADR:
				M_AST(REG_ZERO, REG_ITMP1, 0);
				break;
			case TYPE_FLT:
				M_FST(REG_ZERO, REG_ITMP1, 0);
				break;
			case TYPE_DBL:
				M_DST(REG_ZERO, REG_ITMP1, 0);
				break;
			}
			break;


		case ICMD_GETFIELD:   /* ...  ==> ..., value                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

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

			/* implicit null-pointer check */
			switch (fieldtype) {
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD(d, s1, disp);
				break;
			case TYPE_LNG:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_LLD(d, s1, disp);
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

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = 0;
			}
			else {
				uf        = NULL;
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = fi->offset;
			}

			if (IS_INT_LNG_TYPE(fieldtype))
				s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			else
				s2 = emit_load_s2(jd, iptr, REG_FTMP2);

			if (INSTRUCTION_IS_UNRESOLVED(iptr))
				codegen_add_patch_ref(cd, PATCHER_get_putfield, uf, 0);

			/* implicit null-pointer check */
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
		                          /* val = value (in current instruction)     */
		                          /* following NOP)                           */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

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

			/* implicit null-pointer check */
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
				unresolved_class *uc = iptr->sx.s23.s2.uc;

				codegen_add_patch_ref(cd, PATCHER_resolve_class, uc, 0);
			}
#endif /* ENABLE_VERIFIER */

			disp = dseg_add_functionptr(cd, asm_handle_exception);
			M_ALD(REG_ITMP2, REG_PV, disp);
			M_JMP(REG_ITMP2_XPC, REG_ITMP2);
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
				if ((iptr->sx.val.i > 0) && (iptr->sx.val.i <= 255))
					M_CMPEQ_IMM(s1, iptr->sx.val.i, REG_ITMP1);
				else {
					ICONST(REG_ITMP2, iptr->sx.val.i);
					M_CMPEQ(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_bnez(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IFLT:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 0)
				emit_bltz(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.i > 0) && (iptr->sx.val.i <= 255))
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
				if ((iptr->sx.val.i > 0) && (iptr->sx.val.i <= 255))
					M_CMPLE_IMM(s1, iptr->sx.val.i, REG_ITMP1);
				else {
					ICONST(REG_ITMP2, iptr->sx.val.i);
					M_CMPLE(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_bnez(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IFNE:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 0)
				emit_bnez(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.i > 0) && (iptr->sx.val.i <= 255))
					M_CMPEQ_IMM(s1, iptr->sx.val.i, REG_ITMP1);
				else {
					ICONST(REG_ITMP2, iptr->sx.val.i);
					M_CMPEQ(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_beqz(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IFGT:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 0)
				emit_bgtz(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.i > 0) && (iptr->sx.val.i <= 255))
					M_CMPLE_IMM(s1, iptr->sx.val.i, REG_ITMP1);
				else {
					ICONST(REG_ITMP2, iptr->sx.val.i);
					M_CMPLE(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_beqz(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IFGE:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 0)
				emit_bgez(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.i > 0) && (iptr->sx.val.i <= 255))
					M_CMPLT_IMM(s1, iptr->sx.val.i, REG_ITMP1);
				else {
					ICONST(REG_ITMP2, iptr->sx.val.i);
					M_CMPLT(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_beqz(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IF_LEQ:       /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_beqz(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.l > 0) && (iptr->sx.val.l <= 255))
					M_CMPEQ_IMM(s1, iptr->sx.val.l, REG_ITMP1);
				else {
					LCONST(REG_ITMP2, iptr->sx.val.l);
					M_CMPEQ(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_bnez(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IF_LLT:       /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_bltz(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.l > 0) && (iptr->sx.val.l <= 255))
					M_CMPLT_IMM(s1, iptr->sx.val.l, REG_ITMP1);
				else {
					LCONST(REG_ITMP2, iptr->sx.val.l);
					M_CMPLT(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_bnez(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IF_LLE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_blez(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.l > 0) && (iptr->sx.val.l <= 255))
					M_CMPLE_IMM(s1, iptr->sx.val.l, REG_ITMP1);
				else {
					LCONST(REG_ITMP2, iptr->sx.val.l);
					M_CMPLE(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_bnez(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IF_LNE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_bnez(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.l > 0) && (iptr->sx.val.l <= 255))
					M_CMPEQ_IMM(s1, iptr->sx.val.l, REG_ITMP1);
				else {
					LCONST(REG_ITMP2, iptr->sx.val.l);
					M_CMPEQ(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_beqz(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IF_LGT:       /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_bgtz(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.l > 0) && (iptr->sx.val.l <= 255))
					M_CMPLE_IMM(s1, iptr->sx.val.l, REG_ITMP1);
				else {
					LCONST(REG_ITMP2, iptr->sx.val.l);
					M_CMPLE(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_beqz(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IF_LGE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.l == 0)
				emit_bgez(cd, iptr->dst.block, s1);
			else {
				if ((iptr->sx.val.l > 0) && (iptr->sx.val.l <= 255))
					M_CMPLT_IMM(s1, iptr->sx.val.l, REG_ITMP1);
				else {
					LCONST(REG_ITMP2, iptr->sx.val.l);
					M_CMPLT(s1, REG_ITMP2, REG_ITMP1);
				}
				emit_beqz(cd, iptr->dst.block, REG_ITMP1);
			}
			break;

		case ICMD_IF_ICMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPEQ:    /* op1 = target JavaVM pc                     */
		case ICMD_IF_ACMPEQ:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMPEQ(s1, s2, REG_ITMP1);
			emit_bnez(cd, iptr->dst.block, REG_ITMP1);
			break;

		case ICMD_IF_ICMPNE:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPNE:    /* op1 = target JavaVM pc                     */
		case ICMD_IF_ACMPNE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMPEQ(s1, s2, REG_ITMP1);
			emit_beqz(cd, iptr->dst.block, REG_ITMP1);
			break;

		case ICMD_IF_ICMPLT:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPLT:    /* op1 = target JavaVM pc                     */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMPLT(s1, s2, REG_ITMP1);
			emit_bnez(cd, iptr->dst.block, REG_ITMP1);
			break;

		case ICMD_IF_ICMPGT:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPGT:    /* op1 = target JavaVM pc                     */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMPLE(s1, s2, REG_ITMP1);
			emit_beqz(cd, iptr->dst.block, REG_ITMP1);
			break;

		case ICMD_IF_ICMPLE:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPLE:    /* op1 = target JavaVM pc                     */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMPLE(s1, s2, REG_ITMP1);
			emit_bnez(cd, iptr->dst.block, REG_ITMP1);
			break;

		case ICMD_IF_ICMPGE:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPGE:    /* op1 = target JavaVM pc                     */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMPLT(s1, s2, REG_ITMP1);
			emit_beqz(cd, iptr->dst.block, REG_ITMP1);
			break;


		case ICMD_IRETURN:      /* ..., retvalue ==> ...                      */
		case ICMD_LRETURN:

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

				codegen_add_patch_ref(cd, PATCHER_resolve_class, uc, 0);
			}
#endif /* ENABLE_VERIFIER */
			goto nowperformreturn;

		case ICMD_FRETURN:      /* ..., retvalue ==> ...                      */
		case ICMD_DRETURN:

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_FRESULT);
			M_FLTMOVE(s1, REG_FRESULT);
			goto nowperformreturn;

		case ICMD_RETURN:       /* ...  ==> ...                               */

			REPLACEMENT_POINT_RETURN(cd, iptr);

nowperformreturn:
			{
			s4 i, p;
			
			p = cd->stackframesize;
			
			/* call trace function */

#if !defined(NDEBUG)
			if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
				emit_verbosecall_exit(jd);
#endif

#if defined(ENABLE_THREADS)
			if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
				M_ALD(REG_A0, REG_SP, rd->memuse * 8);

				switch (iptr->opc) {
				case ICMD_IRETURN:
				case ICMD_LRETURN:
				case ICMD_ARETURN:
					M_LST(REG_RESULT, REG_SP, rd->memuse * 8);
					break;
				case ICMD_FRETURN:
				case ICMD_DRETURN:
					M_DST(REG_FRESULT, REG_SP, rd->memuse * 8);
					break;
				}

				disp = dseg_add_functionptr(cd, LOCK_monitor_exit);
				M_ALD(REG_PV, REG_PV, disp);
				M_JSR(REG_RA, REG_PV);
				disp = -(s4) (cd->mcodeptr - cd->mcodebase);
				M_LDA(REG_PV, REG_RA, disp);

				switch (iptr->opc) {
				case ICMD_IRETURN:
				case ICMD_LRETURN:
				case ICMD_ARETURN:
					M_LLD(REG_RESULT, REG_SP, rd->memuse * 8);
					break;
				case ICMD_FRETURN:
				case ICMD_DRETURN:
					M_DLD(REG_FRESULT, REG_SP, rd->memuse * 8);
					break;
				}
			}
#endif

			/* restore return address                                         */

			if (!jd->isleafmethod) {
				p--; M_LLD(REG_RA, REG_SP, p * 8);
			}

			/* restore saved registers                                        */

			for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
				p--; M_LLD(rd->savintregs[i], REG_SP, p * 8);
			}
			for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
				p--; M_DLD(rd->savfltregs[i], REG_SP, p * 8);
			}

			/* deallocate stack                                               */

			if (cd->stackframesize)
				M_LDA(REG_SP, REG_SP, cd->stackframesize * 8);

			M_RET(REG_ZERO, REG_RA);
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
			if (l == 0) {
				M_INTMOVE(s1, REG_ITMP1);
			} else if (l <= 32768) {
				M_LDA(REG_ITMP1, s1, -l);
			} else {
				ICONST(REG_ITMP2, l);
				M_ISUB(s1, REG_ITMP2, REG_ITMP1);
			}

			/* number of targets */
			i = i - l + 1;

			/* range check */

			if (i <= 256)
				M_CMPULE_IMM(REG_ITMP1, i - 1, REG_ITMP2);
			else {
				M_LDA(REG_ITMP2, REG_ZERO, i - 1);
				M_CMPULE(REG_ITMP1, REG_ITMP2, REG_ITMP2);
			}
			emit_beqz(cd, table[0].block, REG_ITMP2);

			/* build jump table top down and use address of lowest entry */

			table += i;

			while (--i >= 0) {
				dseg_add_target(cd, table->block); 
				--table;
			}
			}

			/* length of dataseg after last dseg_add_target is used by load */

			M_SAADDQ(REG_ITMP1, REG_PV, REG_ITMP2);
			M_ALD(REG_ITMP2, REG_ITMP2, -(cd->dseglen));
			M_JMP(REG_ZERO, REG_ITMP2);
			ALIGNCODENOP;
			break;


		case ICMD_LOOKUPSWITCH: /* ..., key ==> ...                           */
			{
			s4 i, val;
			lookup_target_t *lookup;

			lookup = iptr->dst.lookup;

			i = iptr->sx.s23.s2.lookupcount;
			
			MCODECHECK((i<<2)+8);
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

			while (--i >= 0) {
				val = lookup->value;
				if ((val >= 0) && (val <= 255)) {
					M_CMPEQ_IMM(s1, val, REG_ITMP2);
				} else {
					if ((val >= -32768) && (val <= 32767)) {
						M_LDA(REG_ITMP2, REG_ZERO, val);
					} else {
						disp = dseg_add_s4(cd, val);
						M_ILD(REG_ITMP2, REG_PV, disp);
					}
					M_CMPEQ(s1, REG_ITMP2, REG_ITMP2);
				}
				emit_bnez(cd, lookup->target.block, REG_ITMP2);
				lookup++;
			}

			emit_br(cd, iptr->sx.s23.s3.lookupdefault.block);
			ALIGNCODENOP;
			break;
			}


		case ICMD_BUILTIN:      /* ..., arg1, arg2, arg3 ==> ...              */

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

				/* already preallocated (ARGVAR)? */

				if (var->flags & PREALLOC)
					continue;

				if (IS_INT_LNG_TYPE(var->type)) {
					if (!md->params[s3].inmemory) {
						s1 = emit_load(jd, iptr, var, d);
						M_INTMOVE(s1, d);
					}
					else {
						s1 = emit_load(jd, iptr, var, REG_ITMP1);
						M_LST(s1, REG_SP, d * 8);
					}
				}
				else {
					if (!md->params[s3].inmemory) {
						s1 = emit_load(jd, iptr, var, d);
						M_FLTMOVE(s1, d);
					}
					else {
						s1 = emit_load(jd, iptr, var, REG_FTMP1);
						M_DST(s1, REG_SP, d * 8);
					}
				}
			}

			switch (iptr->opc) {
			case ICMD_BUILTIN:
				disp = dseg_add_functionptr(cd, bte->fp);

				M_ALD(REG_PV, REG_PV, disp);  /* Pointer to built-in-function */

				/* generate the actual call */

				M_JSR(REG_RA, REG_PV);
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_LDA(REG_PV, REG_RA, -disp);

				emit_exception_check(cd, iptr);
				break;

			case ICMD_INVOKESPECIAL:
				emit_nullpointer_check(cd, iptr, REG_A0);
				/* fall-through */

			case ICMD_INVOKESTATIC:
				if (lm == NULL) {
					disp = dseg_add_unique_address(cd, um);

					codegen_add_patch_ref(cd, PATCHER_invokestatic_special,
										  um, disp);
				}
				else
					disp = dseg_add_address(cd, lm->stubroutine);

				M_ALD(REG_PV, REG_PV, disp);         /* method pointer in r27 */

				/* generate the actual call */

				M_JSR(REG_RA, REG_PV);
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_LDA(REG_PV, REG_RA, -disp);
				break;

			case ICMD_INVOKEVIRTUAL:
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
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_LDA(REG_PV, REG_RA, -disp);
				break;

			case ICMD_INVOKEINTERFACE:
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
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_LDA(REG_PV, REG_RA, -disp);
				break;
			}

			/* store the return value */

			d = md->returntype.type;

			if (d != TYPE_VOID) {
				if (IS_INT_LNG_TYPE(d)) {
					s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT);
					M_INTMOVE(REG_RESULT, s1);
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
					emit_label_beqz(cd, BRANCH_LABEL_1, s1);

					disp = dseg_add_unique_s4(cd, 0);         /* super->flags */

					codegen_add_patch_ref(cd, PATCHER_resolve_classref_to_flags,
										  iptr->sx.s23.s3.c.ref,
										  disp);

					M_ILD(REG_ITMP2, REG_PV, disp);
					disp = dseg_add_s4(cd, ACC_INTERFACE);
					M_ILD(REG_ITMP3, REG_PV, disp);
					M_AND(REG_ITMP2, REG_ITMP3, REG_ITMP2);
					emit_label_beqz(cd, BRANCH_LABEL_2, REG_ITMP2);
				}

				/* interface checkcast code */

				if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
					if (super == NULL) {
						codegen_add_patch_ref(cd,
											  PATCHER_checkcast_interface,
											  iptr->sx.s23.s3.c.ref,
											  0);
					}
					else
						emit_label_beqz(cd, BRANCH_LABEL_3, s1);

					M_ALD(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));
					M_ILD(REG_ITMP3, REG_ITMP2,
						  OFFSET(vftbl_t, interfacetablelength));
					M_LDA(REG_ITMP3, REG_ITMP3, -superindex);
					emit_classcast_check(cd, iptr, BRANCH_LE, REG_ITMP3, s1);

					M_ALD(REG_ITMP3, REG_ITMP2,
						  (s4) (OFFSET(vftbl_t, interfacetable[0]) -
								superindex * sizeof(methodptr*)));
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

						codegen_add_patch_ref(cd,
											  PATCHER_resolve_classref_to_vftbl,
											  iptr->sx.s23.s3.c.ref,
											  disp);
					}
					else {
						disp = dseg_add_address(cd, super->vftbl);

						emit_label_beqz(cd, BRANCH_LABEL_5, s1);
					}

					M_ALD(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));
					M_ALD(REG_ITMP3, REG_PV, disp);

					CODEGEN_CRITICAL_SECTION_START;

					M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, baseval));
					/*  				if (s1 != REG_ITMP1) { */
					/*  					M_ILD(REG_ITMP1, REG_ITMP3, OFFSET(vftbl_t, baseval)); */
					/*  					M_ILD(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, diffval)); */
					/*  #if defined(ENABLE_THREADS) */
					/*  					codegen_threadcritstop(cd, (u1 *) mcodeptr - cd->mcodebase); */
					/*  #endif */
					/*  					M_ISUB(REG_ITMP2, REG_ITMP1, REG_ITMP2); */

					/*  				} else { */
					M_ILD(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, baseval));
					M_ISUB(REG_ITMP2, REG_ITMP3, REG_ITMP2);
					M_ALD(REG_ITMP3, REG_PV, disp);
					M_ILD(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, diffval));

					CODEGEN_CRITICAL_SECTION_END;

					/*  				} */
					M_CMPULE(REG_ITMP2, REG_ITMP3, REG_ITMP3);
					emit_classcast_check(cd, iptr, BRANCH_EQ, REG_ITMP3, s1);

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

					codegen_add_patch_ref(cd,
										  PATCHER_resolve_classref_to_classinfo,
										  iptr->sx.s23.s3.c.ref,
										  disp);
				}
				else
					disp = dseg_add_address(cd, iptr->sx.s23.s3.c.cls);

				M_ALD(REG_A1, REG_PV, disp);
				disp = dseg_add_functionptr(cd, BUILTIN_arraycheckcast);
				M_ALD(REG_PV, REG_PV, disp);
				M_JSR(REG_RA, REG_PV);
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_LDA(REG_PV, REG_RA, -disp);

				s1 = emit_load_s1(jd, iptr, REG_ITMP1);
				emit_classcast_check(cd, iptr, BRANCH_EQ, REG_RESULT, s1);

				d = codegen_reg_of_dst(jd, iptr, s1);
			}

			M_INTMOVE(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INSTANCEOF: /* ..., objectref ==> ..., intresult            */

			{
			classinfo *super;
			vftbl_t   *supervftbl;
			s4         superindex;

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				super = NULL;
				superindex = 0;
				supervftbl = NULL;

			} else {
				super = iptr->sx.s23.s3.c.cls;
				superindex = super->index;
				supervftbl = super->vftbl;
			}

			if ((super == NULL) || !(super->flags & ACC_INTERFACE))
				CODEGEN_CRITICAL_SECTION_NEW;

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);

			if (s1 == d) {
				M_MOV(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			}

			/* if class is not resolved, check which code to call */

			if (super == NULL) {
				M_CLR(d);
				emit_label_beqz(cd, BRANCH_LABEL_1, s1);

				disp = dseg_add_unique_s4(cd, 0);             /* super->flags */

				codegen_add_patch_ref(cd, PATCHER_resolve_classref_to_flags,
									  iptr->sx.s23.s3.c.ref, disp);

				M_ILD(REG_ITMP3, REG_PV, disp);

				disp = dseg_add_s4(cd, ACC_INTERFACE);
				M_ILD(REG_ITMP2, REG_PV, disp);
				M_AND(REG_ITMP3, REG_ITMP2, REG_ITMP3);
				emit_label_beqz(cd, BRANCH_LABEL_2, REG_ITMP3);
			}

			/* interface instanceof code */

			if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
				if (super == NULL) {
					/* If d == REG_ITMP2, then it's destroyed in check
					   code above. */
					if (d == REG_ITMP2)
						M_CLR(d);

					codegen_add_patch_ref(cd,
										  PATCHER_instanceof_interface,
										  iptr->sx.s23.s3.c.ref, 0);
				}
				else {
					M_CLR(d);
					emit_label_beqz(cd, BRANCH_LABEL_3, s1);
				}

				M_ALD(REG_ITMP1, s1, OFFSET(java_objectheader, vftbl));
				M_ILD(REG_ITMP3, REG_ITMP1, OFFSET(vftbl_t, interfacetablelength));
				M_LDA(REG_ITMP3, REG_ITMP3, -superindex);
				M_BLEZ(REG_ITMP3, 2);
				M_ALD(REG_ITMP1, REG_ITMP1,
					  (s4) (OFFSET(vftbl_t, interfacetable[0]) -
							superindex * sizeof(methodptr*)));
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

					disp = dseg_add_unique_address(cd, NULL);

					codegen_add_patch_ref(cd, PATCHER_resolve_classref_to_vftbl,
										  iptr->sx.s23.s3.c.ref,
										  disp);
				}
				else {
					disp = dseg_add_address(cd, supervftbl);

					M_CLR(d);
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
				M_CMPULE(REG_ITMP1, REG_ITMP2, d);

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

				/* Already Preallocated? */

				if (!(var->flags & PREALLOC)) {
					s2 = emit_load(jd, iptr, var, REG_ITMP1);
					M_LST(s2, REG_SP, s1 * 8);
				}
			}

			/* a0 = dimension count */

			ICONST(REG_A0, iptr->s1.argcount);

			/* is patcher function set? */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				disp = dseg_add_unique_address(cd, 0);

				codegen_add_patch_ref(cd, PATCHER_resolve_classref_to_classinfo,
									  iptr->sx.s23.s3.c.ref,
									  disp);
			}
			else
				disp = dseg_add_address(cd, iptr->sx.s23.s3.c.cls);

			/* a1 = arraydescriptor */

			M_ALD(REG_A1, REG_PV, disp);

			/* a2 = pointer to dimensions = stack pointer */

			M_INTMOVE(REG_SP, REG_A2);

			disp = dseg_add_functionptr(cd, BUILTIN_multianewarray);
			M_ALD(REG_PV, REG_PV, disp);
			M_JSR(REG_RA, REG_PV);
			disp = (s4) (cd->mcodeptr - cd->mcodebase);
			M_LDA(REG_PV, REG_RA, -disp);

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

	M_ALD(REG_ITMP1, REG_PV, -2 * 8);   /* load codeinfo pointer              */
	M_ALD(REG_PV, REG_PV, -3 * 8);      /* load pointer to the compiler       */
	M_JMP(REG_ZERO, REG_PV);            /* jump to the compiler               */
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
	s4           funcdisp;             /* displacement of the function       */

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
		1 +                             /* methodinfo for call trace          */
		md->paramcount +
		nmd->memuse;

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

	M_LDA(REG_SP, REG_SP, -(cd->stackframesize * 8));
	M_AST(REG_RA, REG_SP, cd->stackframesize * 8 - SIZEOF_VOID_P);

	/* call trace function */

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_enter(jd);
#endif

	/* get function address (this must happen before the stackframeinfo) */

	funcdisp = dseg_add_functionptr(cd, f);

#if !defined(WITH_STATIC_CLASSPATH)
	if (f == NULL)
		codegen_add_patch_ref(cd, PATCHER_resolve_native_function, m, funcdisp);
#endif

	/* save integer and float argument registers */

	for (i = 0; i < md->paramcount; i++) {
		if (!md->params[i].inmemory) {
			s1 = md->params[i].regoff;

			switch (md->paramtypes[i].type) {
			case TYPE_INT:
			case TYPE_LNG:
			case TYPE_ADR:
				M_LST(s1, REG_SP, i * 8);
				break;
			case TYPE_FLT:
			case TYPE_DBL:
				M_DST(s1, REG_SP, i * 8);
				break;
			}
		}
	}

	/* prepare data structures for native function call */

	M_LDA(REG_A0, REG_SP, cd->stackframesize * 8 - SIZEOF_VOID_P);
	M_MOV(REG_PV, REG_A1);
	M_LDA(REG_A2, REG_SP, cd->stackframesize * 8);
	M_ALD(REG_A3, REG_SP, cd->stackframesize * 8 - SIZEOF_VOID_P);
	disp = dseg_add_functionptr(cd, codegen_start_native_call);
	M_ALD(REG_PV, REG_PV, disp);
	M_JSR(REG_RA, REG_PV);
	disp = (s4) (cd->mcodeptr - cd->mcodebase);
	M_LDA(REG_PV, REG_RA, -disp);

	/* restore integer and float argument registers */

	for (i = 0; i < md->paramcount; i++) {
		if (!md->params[i].inmemory) {
			s1 = md->params[i].regoff;

			switch (md->paramtypes[i].type) {
			case TYPE_INT:
			case TYPE_LNG:
			case TYPE_ADR:
				M_LLD(s1, REG_SP, i * 8);
				break;
			case TYPE_FLT:
			case TYPE_DBL:
				M_DLD(s1, REG_SP, i * 8);
				break;
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

				if (!nmd->params[j].inmemory)
					M_INTMOVE(s1, s2);
				else
					M_LST(s1, REG_SP, s2 * 8);
			}
			else {
				s1 = md->params[i].regoff + cd->stackframesize;
				s2 = nmd->params[j].regoff;
				M_LLD(REG_ITMP1, REG_SP, s1 * 8);
				M_LST(REG_ITMP1, REG_SP, s2 * 8);
			}
		}
		else {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;
				s2 = nmd->params[j].regoff;

				if (!nmd->params[j].inmemory)
					M_FLTMOVE(s1, s2);
				else {
					if (IS_2_WORD_TYPE(t))
						M_DST(s1, REG_SP, s2 * 8);
					else
						M_FST(s1, REG_SP, s2 * 8);
				}
			}
			else {
				s1 = md->params[i].regoff + cd->stackframesize;
				s2 = nmd->params[j].regoff;
				M_DLD(REG_FTMP1, REG_SP, s1 * 8);
				if (IS_2_WORD_TYPE(t))
					M_DST(REG_FTMP1, REG_SP, s2 * 8);
				else
					M_FST(REG_FTMP1, REG_SP, s2 * 8);
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

	M_ALD(REG_PV, REG_PV, funcdisp);
	M_JSR(REG_RA, REG_PV);              /* call native method                 */
	disp = (s4) (cd->mcodeptr - cd->mcodebase);
	M_LDA(REG_PV, REG_RA, -disp);       /* recompute pv from ra               */

	/* save return value */

	switch (md->returntype.type) {
	case TYPE_INT:
	case TYPE_LNG:
	case TYPE_ADR:
		M_LST(REG_RESULT, REG_SP, 0 * 8);
		break;
	case TYPE_FLT:
	case TYPE_DBL:
		M_DST(REG_FRESULT, REG_SP, 0 * 8);
		break;
	case TYPE_VOID:
		break;
	}

	/* call finished trace */

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_exit(jd);
#endif

	/* remove native stackframe info */

	M_LDA(REG_A0, REG_SP, cd->stackframesize * 8 - SIZEOF_VOID_P);
	disp = dseg_add_functionptr(cd, codegen_finish_native_call);
	M_ALD(REG_PV, REG_PV, disp);
	M_JSR(REG_RA, REG_PV);
	disp = (s4) (cd->mcodeptr - cd->mcodebase);
	M_LDA(REG_PV, REG_RA, -disp);
	M_MOV(REG_RESULT, REG_ITMP1_XPTR);

	/* restore return value */

	switch (md->returntype.type) {
	case TYPE_INT:
	case TYPE_LNG:
	case TYPE_ADR:
		M_LLD(REG_RESULT, REG_SP, 0 * 8);
		break;
	case TYPE_FLT:
	case TYPE_DBL:
		M_DLD(REG_FRESULT, REG_SP, 0 * 8);
		break;
	case TYPE_VOID:
		break;
	}

	M_ALD(REG_RA, REG_SP, (cd->stackframesize - 1) * 8); /* get RA            */
	M_LDA(REG_SP, REG_SP, cd->stackframesize * 8);

	/* check for exception */

	M_BNEZ(REG_ITMP1_XPTR, 1);          /* if no exception then return        */
	M_RET(REG_ZERO, REG_RA);            /* return to caller                   */

	/* handle exception */

	M_ASUB_IMM(REG_RA, 4, REG_ITMP2_XPC); /* get exception address            */

	disp = dseg_add_functionptr(cd, asm_handle_nat_exception);
	M_ALD(REG_ITMP3, REG_PV, disp);     /* load asm exception handler address */
	M_JMP(REG_ZERO, REG_ITMP3);         /* jump to asm exception handler      */
	
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

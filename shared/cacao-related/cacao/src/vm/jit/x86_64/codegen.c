/* src/vm/jit/x86_64/codegen.c - machine code generator for x86_64

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

   $Id: codegen.c 7866 2007-05-03 22:32:11Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>

#include "vm/types.h"

#include "md-abi.h"

#include "vm/jit/x86_64/arch.h"
#include "vm/jit/x86_64/codegen.h"
#include "vm/jit/x86_64/emit.h"

#include "mm/memory.h"

#include "native/jni.h"
#include "native/native.h"

#include "threads/lock-common.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit/abi.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/codegen-common.h"
#include "vm/jit/dseg.h"
#include "vm/jit/emit-common.h"
#include "vm/jit/jit.h"
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
#include "vmcore/statistics.h"


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
	u2                  currentline;
	ptrint              a;
	varinfo            *var, *dst;
	basicblock         *bptr;
	instruction        *iptr;
	exception_entry    *ex;
	constant_classref  *cr;
	unresolved_class   *uc;
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

	d   = 0;
	lm  = NULL;
	um  = NULL;
	bte = NULL;

	{
	s4 i, p, t, l;
	s4 savedregs_num;

  	savedregs_num = 0;

	/* space to save used callee saved registers */

	savedregs_num += (INT_SAV_CNT - rd->savintreguse);
	savedregs_num += (FLT_SAV_CNT - rd->savfltreguse);

	cd->stackframesize = rd->memuse + savedregs_num;

#if defined(ENABLE_THREADS)
	/* space to save argument of monitor_enter */

	if (checksync && (m->flags & ACC_SYNCHRONIZED))
		cd->stackframesize++;
#endif

	/* Keep stack of non-leaf functions 16-byte aligned for calls into
	   native code e.g. libc or jni (alignment problems with
	   movaps). */

	if (!jd->isleafmethod || opt_verbosecall)
		cd->stackframesize |= 0x1;

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
	                                       
	(void) dseg_add_unique_s4(cd, jd->isleafmethod);               /* IsLeaf  */
	(void) dseg_add_unique_s4(cd, INT_SAV_CNT - rd->savintreguse); /* IntSave */
	(void) dseg_add_unique_s4(cd, FLT_SAV_CNT - rd->savfltreguse); /* FltSave */

	(void) dseg_addlinenumbertablesize(cd);

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

		M_MOV_IMM(code, REG_ITMP3);
		M_IINC_MEMBASE(REG_ITMP3, OFFSET(codeinfo, frequency));

		PROFILE_CYCLE_START;
	}
#endif

	/* create stack frame (if necessary) */

	if (cd->stackframesize)
		M_ASUB_IMM(cd->stackframesize * 8, REG_SP);

	/* save used callee saved registers */

  	p = cd->stackframesize;
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
			else {                                 /* stack arguments       */
 				if (!IS_INMEMORY(var->flags))
					/* + 8 for return address */
 					M_LLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8 + 8);
				else
					var->vv.regoff = cd->stackframesize + s1 + 1;
			}
		}
		else {                                     /* floating args         */
 			if (!md->params[p].inmemory) {           /* register arguments    */
 				if (!IS_INMEMORY(var->flags))
					M_FLTMOVE(s1, var->vv.regoff);
				else
					M_DST(s1, REG_SP, var->vv.regoff * 8);
			}
			else {                                 /* stack arguments       */
 				if (!IS_INMEMORY(var->flags))
					M_DLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8 + 8);
				else
					var->vv.regoff = cd->stackframesize + s1 + 1;
			}
		}
	}

	/* save monitorenter argument */

#if defined(ENABLE_THREADS)
	if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
		/* stack offset for monitor argument */

		s1 = rd->memuse;

		if (opt_verbosecall) {
			M_LSUB_IMM((INT_ARG_CNT + FLT_ARG_CNT) * 8, REG_SP);

			for (p = 0; p < INT_ARG_CNT; p++)
				M_LST(abi_registers_integer_argument[p], REG_SP, p * 8);

			for (p = 0; p < FLT_ARG_CNT; p++)
				M_DST(abi_registers_float_argument[p], REG_SP, (INT_ARG_CNT + p) * 8);

			s1 += INT_ARG_CNT + FLT_ARG_CNT;
		}

		/* decide which monitor enter function to call */

		if (m->flags & ACC_STATIC) {
			M_MOV_IMM(&m->class->object.header, REG_A0);
		}
		else {
			M_TEST(REG_A0);
			M_BNE(8);
			M_ALD_MEM(REG_A0, EXCEPTION_HARDWARE_NULLPOINTER);
		}

		M_AST(REG_A0, REG_SP, s1 * 8);
		M_MOV_IMM(LOCK_monitor_enter, REG_ITMP1);
		M_CALL(REG_ITMP1);

		if (opt_verbosecall) {
			for (p = 0; p < INT_ARG_CNT; p++)
				M_LLD(abi_registers_integer_argument[p], REG_SP, p * 8);

			for (p = 0; p < FLT_ARG_CNT; p++)
				M_DLD(abi_registers_float_argument[p], REG_SP, (INT_ARG_CNT + p) * 8);

			M_LADD_IMM((INT_ARG_CNT + FLT_ARG_CNT) * 8, REG_SP);
		}
	}
#endif

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_enter(jd);
#endif /* !defined(NDEBUG) */

	}

	/* end of header generation */

	/* create replacement points */

	REPLACEMENT_POINTS_INIT(cd, jd);

	/* walk through all basic blocks */

	for (bptr = jd->basicblocks; bptr != NULL; bptr = bptr->next) {

		bptr->mpc = (u4) ((u1 *) cd->mcodeptr - cd->mcodebase);

		if (bptr->flags >= BBREACHED) {

		/* branch resolving */

		codegen_resolve_branchrefs(cd, bptr);

		/* handle replacement points */

		REPLACEMENT_POINT_BLOCK_START(cd, bptr);

		/* copy interface registers to their destination */

		len = bptr->indepth;
		MCODECHECK(512);

#if defined(ENABLE_PROFILING)
		/* generate basicblock profiling code */

		if (JITDATA_HAS_FLAG_INSTRUMENT(jd)) {
			/* count frequency */

			M_MOV_IMM(code->bbfrequency, REG_ITMP3);
			M_IINC_MEMBASE(REG_ITMP3, bptr->nr * 4);

			/* if this is an exception handler, start profiling again */

			if (bptr->type == BBTYPE_EXH)
				PROFILE_CYCLE_START;
		}
#endif

#if defined(ENABLE_LSRA)
		if (opt_lsra) {
			while (len) {
				len--;
				src = bptr->invars[len];
				if ((len == bptr->indepth-1) && (bptr->type != BBTYPE_STD)) {
					if (bptr->type == BBTYPE_EXH) {
/*  					d = reg_of_var(rd, src, REG_ITMP1); */
						if (!IS_INMEMORY(src->flags))
							d= src->vv.regoff;
						else
							d=REG_ITMP1;
						M_INTMOVE(REG_ITMP1, d);
						emit_store(jd, NULL, src, d);
					}
				}
			}
			
		} else {
#endif

		while (len) {
			len--;
			var = VAR(bptr->invars[len]);
  			if ((len ==  bptr->indepth-1) && (bptr->type != BBTYPE_STD)) {
				if (bptr->type == BBTYPE_EXH) {
					d = codegen_reg_of_var(0, var, REG_ITMP1);
					M_INTMOVE(REG_ITMP1, d);
					emit_store(jd, NULL, var, d);
				}
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

			MCODECHECK(1024);                         /* 1KB should be enough */

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
			emit_movdl_membase_reg(cd, RIP, -((cd->mcodeptr + ((d > 7) ? 9 : 8)) - cd->mcodebase) + disp, d);
			emit_store_dst(jd, iptr, d);
			break;
		
		case ICMD_DCONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			disp = dseg_add_double(cd, iptr->sx.val.d);
			emit_movd_membase_reg(cd, RIP, -((cd->mcodeptr + 9) - cd->mcodebase) + disp, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ACONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				cr = iptr->sx.val.c.ref;

/* 				PROFILE_CYCLE_STOP; */

				codegen_add_patch_ref(cd, PATCHER_aconst, cr, 0);

/* 				PROFILE_CYCLE_START; */

				M_MOV_IMM(NULL, d);

			} else {
				if (iptr->sx.val.anyptr == 0)
					M_CLR(d);
				else
					M_MOV_IMM(iptr->sx.val.anyptr, d);
			}
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
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_INEG(d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1); 
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_LNEG(d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2L:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_ISEXT(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_L2I:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_IMOV(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2BYTE:   /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_BSEXT(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2CHAR:   /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_CZEXT(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2SHORT:  /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_SSEXT(s1, d);
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_IADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_IADD(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_IADD(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IINC:
		case ICMD_IADDCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);

			/* Using inc and dec is not faster than add (tested with
			   sieve). */

			M_INTMOVE(s1, d);
			M_IADD_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_LADD(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_LADD(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADDCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			if (IS_IMM32(iptr->sx.val.l))
				M_LADD_IMM(iptr->sx.val.l, d);
			else {
				M_MOV_IMM(iptr->sx.val.l, REG_ITMP2);
				M_LADD(REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d) {
				M_INTMOVE(s1, REG_ITMP1);
				M_ISUB(s2, REG_ITMP1);
				M_INTMOVE(REG_ITMP1, d);
			} else {
				M_INTMOVE(s1, d);
				M_ISUB(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISUBCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_ISUB_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d) {
				M_INTMOVE(s1, REG_ITMP1);
				M_LSUB(s2, REG_ITMP1);
				M_INTMOVE(REG_ITMP1, d);
			} else {
				M_INTMOVE(s1, d);
				M_LSUB(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUBCONST:  /* ..., value  ==> ..., value - constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			if (IS_IMM32(iptr->sx.val.l))
				M_LSUB_IMM(iptr->sx.val.l, d);
			else {
				M_MOV_IMM(iptr->sx.val.l, REG_ITMP2);
				M_LSUB(REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_IMUL(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_IMUL(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMULCONST:  /* ..., value  ==> ..., value * constant        */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i == 2) {
				M_INTMOVE(s1, d);
				M_ISLL_IMM(1, d);
			} else
				M_IMUL_IMM(s1, iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_LMUL(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_LMUL(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LMULCONST:  /* ..., value  ==> ..., value * constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			if (IS_IMM32(iptr->sx.val.l))
				M_LMUL_IMM(s1, iptr->sx.val.l, d);
			else {
				M_MOV_IMM(iptr->sx.val.l, REG_ITMP2);
				M_INTMOVE(s1, d);
				M_LMUL(REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, RAX);
			s2 = emit_load_s2(jd, iptr, REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, RAX);

			M_INTMOVE(s1, RAX);
			M_INTMOVE(s2, REG_ITMP3);
			emit_arithmetic_check(cd, iptr, REG_ITMP3);

			M_MOV(RDX, REG_ITMP2);    /* save RDX (it's an argument register) */

			M_ICMP_IMM(0x80000000, RAX);    /* check as described in jvm spec */
			M_BNE(4 + 6);
			M_ICMP_IMM(-1, REG_ITMP3);                             /* 4 bytes */
			M_BEQ(1 + 3);                                          /* 6 bytes */

  			emit_cltd(cd);                                         /* 1 byte  */
			emit_idivl_reg(cd, REG_ITMP3);                         /* 3 bytes */

			M_INTMOVE(RAX, d);
			emit_store_dst(jd, iptr, d);
			dst = VAROP(iptr->dst);
			if (IS_INMEMORY(dst->flags) || (dst->vv.regoff != RDX))
				M_MOV(REG_ITMP2, RDX);                         /* restore RDX */
			break;

		case ICMD_IREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, RAX);
			s2 = emit_load_s2(jd, iptr, REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, RDX);

			M_INTMOVE(s1, RAX);
			M_INTMOVE(s2, REG_ITMP3);
			emit_arithmetic_check(cd, iptr, REG_ITMP3);

			M_MOV(RDX, REG_ITMP2);    /* save RDX (it's an argument register) */

			M_ICMP_IMM(0x80000000, RAX);    /* check as described in jvm spec */
			M_BNE(3 + 4 + 6);
			M_CLR(RDX);                                            /* 3 bytes */
			M_ICMP_IMM(-1, REG_ITMP3);                             /* 4 bytes */
			M_BEQ(1 + 3);                                          /* 6 bytes */

  			emit_cltd(cd);                                         /* 1 byte  */
			emit_idivl_reg(cd, REG_ITMP3);                         /* 3 byte  */

			M_INTMOVE(RDX, d);
			emit_store_dst(jd, iptr, d);
			dst = VAROP(iptr->dst);
			if (IS_INMEMORY(dst->flags) || (dst->vv.regoff != RDX))
				M_MOV(REG_ITMP2, RDX);                         /* restore RDX */
			break;

		case ICMD_IDIVPOW2:   /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_INTMOVE(s1, REG_ITMP1);
			emit_alul_imm_reg(cd, ALU_CMP, -1, REG_ITMP1);
			emit_leal_membase_reg(cd, REG_ITMP1, (1 << iptr->sx.val.i) - 1, REG_ITMP2);
			emit_cmovccl_reg_reg(cd, CC_LE, REG_ITMP2, REG_ITMP1);
			emit_shiftl_imm_reg(cd, SHIFT_SAR, iptr->sx.val.i, REG_ITMP1);
			emit_mov_reg_reg(cd, REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_INTMOVE(s1, REG_ITMP1);
			emit_alul_imm_reg(cd, ALU_CMP, -1, REG_ITMP1);
			emit_leal_membase_reg(cd, REG_ITMP1, iptr->sx.val.i, REG_ITMP2);
			emit_cmovccl_reg_reg(cd, CC_G, REG_ITMP1, REG_ITMP2);
			emit_alul_imm_reg(cd, ALU_AND, -1 - (iptr->sx.val.i), REG_ITMP2);
			emit_alul_reg_reg(cd, ALU_SUB, REG_ITMP2, REG_ITMP1);
			emit_mov_reg_reg(cd, REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_LDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, RAX);
			s2 = emit_load_s2(jd, iptr, REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, RAX);

			M_INTMOVE(s1, RAX);
			M_INTMOVE(s2, REG_ITMP3);
			emit_arithmetic_check(cd, iptr, REG_ITMP3);

			M_MOV(RDX, REG_ITMP2);    /* save RDX (it's an argument register) */

			/* check as described in jvm spec */
			disp = dseg_add_s8(cd, 0x8000000000000000LL);
  			M_LCMP_MEMBASE(RIP, -((cd->mcodeptr + 7) - cd->mcodebase) + disp, RAX);
			M_BNE(4 + 6);
			M_LCMP_IMM(-1, REG_ITMP3);                             /* 4 bytes */
			M_BEQ(2 + 3);                                          /* 6 bytes */

  			emit_cqto(cd);                                         /* 2 bytes */
			emit_idiv_reg(cd, REG_ITMP3);                          /* 3 bytes */

			M_INTMOVE(RAX, d);
			emit_store_dst(jd, iptr, d);
			dst = VAROP(iptr->dst);
			if (IS_INMEMORY(dst->flags) || (dst->vv.regoff != RDX))
				M_MOV(REG_ITMP2, RDX);                         /* restore RDX */
			break;

		case ICMD_LREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, RAX);
			s2 = emit_load_s2(jd, iptr, REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, RDX);

			M_INTMOVE(s1, RAX);
			M_INTMOVE(s2, REG_ITMP3);
			emit_arithmetic_check(cd, iptr, REG_ITMP3);

			M_MOV(RDX, REG_ITMP2);    /* save RDX (it's an argument register) */

			/* check as described in jvm spec */
			disp = dseg_add_s8(cd, 0x8000000000000000LL);
  			M_LCMP_MEMBASE(RIP, -((cd->mcodeptr + 7) - cd->mcodebase) + disp, REG_ITMP1);
			M_BNE(3 + 4 + 6);
			M_LXOR(RDX, RDX);                                      /* 3 bytes */
			M_LCMP_IMM(-1, REG_ITMP3);                             /* 4 bytes */
			M_BEQ(2 + 3);                                          /* 6 bytes */

  			emit_cqto(cd);                                         /* 2 bytes */
			emit_idiv_reg(cd, REG_ITMP3);                          /* 3 bytes */

			M_INTMOVE(RDX, d);
			emit_store_dst(jd, iptr, d);
			dst = VAROP(iptr->dst);
			if (IS_INMEMORY(dst->flags) || (dst->vv.regoff != RDX))
				M_MOV(REG_ITMP2, RDX);                         /* restore RDX */
			break;

		case ICMD_LDIVPOW2:   /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_INTMOVE(s1, REG_ITMP1);
			emit_alu_imm_reg(cd, ALU_CMP, -1, REG_ITMP1);
			emit_lea_membase_reg(cd, REG_ITMP1, (1 << iptr->sx.val.i) - 1, REG_ITMP2);
			emit_cmovcc_reg_reg(cd, CC_LE, REG_ITMP2, REG_ITMP1);
			emit_shift_imm_reg(cd, SHIFT_SAR, iptr->sx.val.i, REG_ITMP1);
			emit_mov_reg_reg(cd, REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_INTMOVE(s1, REG_ITMP1);
			emit_alu_imm_reg(cd, ALU_CMP, -1, REG_ITMP1);
			emit_lea_membase_reg(cd, REG_ITMP1, iptr->sx.val.i, REG_ITMP2);
			emit_cmovcc_reg_reg(cd, CC_G, REG_ITMP1, REG_ITMP2);
			emit_alu_imm_reg(cd, ALU_AND, -1 - (iptr->sx.val.i), REG_ITMP2);
			emit_alu_reg_reg(cd, ALU_SUB, REG_ITMP2, REG_ITMP1);
			emit_mov_reg_reg(cd, REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			d = codegen_reg_of_dst(jd, iptr, REG_NULL);
			emit_ishift(jd, SHIFT_SHL, iptr);
			break;

		case ICMD_ISHLCONST:  /* ..., value  ==> ..., value << constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_ISLL_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHR:       /* ..., val1, val2  ==> ..., val1 >> val2       */

			d = codegen_reg_of_dst(jd, iptr, REG_NULL);
			emit_ishift(jd, SHIFT_SAR, iptr);
			break;

		case ICMD_ISHRCONST:  /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(s1, d);
			M_ISRA_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHR:      /* ..., val1, val2  ==> ..., val1 >>> val2      */

			d = codegen_reg_of_dst(jd, iptr, REG_NULL);
			emit_ishift(jd, SHIFT_SHR, iptr);
			break;

		case ICMD_IUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(s1, d);
			M_ISRL_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			d = codegen_reg_of_dst(jd, iptr, REG_NULL);
			emit_lshift(jd, SHIFT_SHL, iptr);
			break;

        case ICMD_LSHLCONST:  /* ..., value  ==> ..., value << constant       */
 			                  /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_LSLL_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHR:       /* ..., val1, val2  ==> ..., val1 >> val2       */

			d = codegen_reg_of_dst(jd, iptr, REG_NULL);
			emit_lshift(jd, SHIFT_SAR, iptr);
			break;

		case ICMD_LSHRCONST:  /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(s1, d);
			M_LSRA_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LUSHR:      /* ..., val1, val2  ==> ..., val1 >>> val2      */

			d = codegen_reg_of_dst(jd, iptr, REG_NULL);
			emit_lshift(jd, SHIFT_SHR, iptr);
			break;

  		case ICMD_LUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
  		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(s1, d);
			M_LSRL_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IAND:       /* ..., val1, val2  ==> ..., val1 & val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_IAND(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_IAND(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_IAND_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LAND:       /* ..., val1, val2  ==> ..., val1 & val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_LAND(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_LAND(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			if (IS_IMM32(iptr->sx.val.l))
				M_LAND_IMM(iptr->sx.val.l, d);
			else {
				M_MOV_IMM(iptr->sx.val.l, REG_ITMP2);
				M_LAND(REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IOR:        /* ..., val1, val2  ==> ..., val1 | val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_IOR(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_IOR(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_IOR_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LOR:        /* ..., val1, val2  ==> ..., val1 | val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_LOR(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_LOR(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			if (IS_IMM32(iptr->sx.val.l))
				M_LOR_IMM(iptr->sx.val.l, d);
			else {
				M_MOV_IMM(iptr->sx.val.l, REG_ITMP2);
				M_LOR(REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IXOR:       /* ..., val1, val2  ==> ..., val1 ^ val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_IXOR(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_IXOR(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IXORCONST:  /* ..., value  ==> ..., value ^ constant        */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_IXOR_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXOR:       /* ..., val1, val2  ==> ..., val1 ^ val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_LXOR(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_LXOR(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXORCONST:  /* ..., value  ==> ..., value ^ constant        */
		                      /* sx.val.l = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			if (IS_IMM32(iptr->sx.val.l))
				M_LXOR_IMM(iptr->sx.val.l, d);
			else {
				M_MOV_IMM(iptr->sx.val.l, REG_ITMP2);
				M_LXOR(REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;


		/* floating operations ************************************************/

		case ICMD_FNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			disp = dseg_add_s4(cd, 0x80000000);
			M_FLTMOVE(s1, d);
			emit_movss_membase_reg(cd, RIP, -((cd->mcodeptr + 9) - cd->mcodebase) + disp, REG_FTMP2);
			emit_xorps_reg_reg(cd, REG_FTMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			disp = dseg_add_s8(cd, 0x8000000000000000);
			M_FLTMOVE(s1, d);
			emit_movd_membase_reg(cd, RIP, -((cd->mcodeptr + 9) - cd->mcodebase) + disp, REG_FTMP2);
			emit_xorpd_reg_reg(cd, REG_FTMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (s2 == d)
				M_FADD(s1, d);
			else {
				M_FLTMOVE(s1, d);
				M_FADD(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (s2 == d)
				M_DADD(s1, d);
			else {
				M_FLTMOVE(s1, d);
				M_DADD(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (s2 == d) {
				M_FLTMOVE(s2, REG_FTMP2);
				s2 = REG_FTMP2;
			}
			M_FLTMOVE(s1, d);
			M_FSUB(s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (s2 == d) {
				M_FLTMOVE(s2, REG_FTMP2);
				s2 = REG_FTMP2;
			}
			M_FLTMOVE(s1, d);
			M_DSUB(s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (s2 == d)
				M_FMUL(s1, d);
			else {
				M_FLTMOVE(s1, d);
				M_FMUL(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (s2 == d)
				M_DMUL(s1, d);
			else {
				M_FLTMOVE(s1, d);
				M_DMUL(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (s2 == d) {
				M_FLTMOVE(s2, REG_FTMP2);
				s2 = REG_FTMP2;
			}
			M_FLTMOVE(s1, d);
			M_FDIV(s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			if (s2 == d) {
				M_FLTMOVE(s2, REG_FTMP2);
				s2 = REG_FTMP2;
			}
			M_FLTMOVE(s1, d);
			M_DDIV(s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2F:       /* ..., value  ==> ..., (float) value            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_CVTIF(s1, d);
  			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2D:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_CVTID(s1, d);
  			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_L2F:       /* ..., value  ==> ..., (float) value            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_CVTLF(s1, d);
  			emit_store_dst(jd, iptr, d);
			break;
			
		case ICMD_L2D:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_CVTLD(s1, d);
  			emit_store_dst(jd, iptr, d);
			break;
			
		case ICMD_F2I:       /* ..., value  ==> ..., (int) value              */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_CVTFI(s1, d);
			M_ICMP_IMM(0x80000000, d);                        /* corner cases */
			disp = ((s1 == REG_FTMP1) ? 0 : 5) + 10 + 3 +
				((REG_RESULT == d) ? 0 : 3);
			M_BNE(disp);
			M_FLTMOVE(s1, REG_FTMP1);
			M_MOV_IMM(asm_builtin_f2i, REG_ITMP2);
			M_CALL(REG_ITMP2);
			M_INTMOVE(REG_RESULT, d);
  			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_D2I:       /* ..., value  ==> ..., (int) value              */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_CVTDI(s1, d);
			M_ICMP_IMM(0x80000000, d);                        /* corner cases */
			disp = ((s1 == REG_FTMP1) ? 0 : 5) + 10 + 3 +
				((REG_RESULT == d) ? 0 : 3);
			M_BNE(disp);
			M_FLTMOVE(s1, REG_FTMP1);
			M_MOV_IMM(asm_builtin_d2i, REG_ITMP2);
			M_CALL(REG_ITMP2);
			M_INTMOVE(REG_RESULT, d);
  			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_F2L:       /* ..., value  ==> ..., (long) value             */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_CVTFL(s1, d);
			M_MOV_IMM(0x8000000000000000, REG_ITMP2);
			M_LCMP(REG_ITMP2, d);                             /* corner cases */
			disp = ((s1 == REG_FTMP1) ? 0 : 5) + 10 + 3 +
				((REG_RESULT == d) ? 0 : 3);
			M_BNE(disp);
			M_FLTMOVE(s1, REG_FTMP1);
			M_MOV_IMM(asm_builtin_f2l, REG_ITMP2);
			M_CALL(REG_ITMP2);
			M_INTMOVE(REG_RESULT, d);
  			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_D2L:       /* ..., value  ==> ..., (long) value             */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_CVTDL(s1, d);
			M_MOV_IMM(0x8000000000000000, REG_ITMP2);
			M_LCMP(REG_ITMP2, d);                             /* corner cases */
			disp = ((s1 == REG_FTMP1) ? 0 : 5) + 10 + 3 +
				((REG_RESULT == d) ? 0 : 3);
			M_BNE(disp);
			M_FLTMOVE(s1, REG_FTMP1);
			M_MOV_IMM(asm_builtin_d2l, REG_ITMP2);
			M_CALL(REG_ITMP2);
			M_INTMOVE(REG_RESULT, d);
  			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_F2D:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_CVTFD(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_D2F:       /* ..., value  ==> ..., (float) value            */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			M_CVTDF(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCMPL:      /* ..., val1, val2  ==> ..., val1 fcmpl val2    */
 			                  /* == => 0, < => 1, > => -1 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_CLR(d);
			M_MOV_IMM(1, REG_ITMP1);
			M_MOV_IMM(-1, REG_ITMP2);
			emit_ucomiss_reg_reg(cd, s1, s2);
			M_CMOVULT(REG_ITMP1, d);
			M_CMOVUGT(REG_ITMP2, d);
			M_CMOVP(REG_ITMP2, d);                   /* treat unordered as GT */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCMPG:      /* ..., val1, val2  ==> ..., val1 fcmpg val2    */
 			                  /* == => 0, < => 1, > => -1 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_CLR(d);
			M_MOV_IMM(1, REG_ITMP1);
			M_MOV_IMM(-1, REG_ITMP2);
			emit_ucomiss_reg_reg(cd, s1, s2);
			M_CMOVULT(REG_ITMP1, d);
			M_CMOVUGT(REG_ITMP2, d);
			M_CMOVP(REG_ITMP1, d);                   /* treat unordered as LT */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DCMPL:      /* ..., val1, val2  ==> ..., val1 fcmpl val2    */
 			                  /* == => 0, < => 1, > => -1 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_CLR(d);
			M_MOV_IMM(1, REG_ITMP1);
			M_MOV_IMM(-1, REG_ITMP2);
			emit_ucomisd_reg_reg(cd, s1, s2);
			M_CMOVULT(REG_ITMP1, d);
			M_CMOVUGT(REG_ITMP2, d);
			M_CMOVP(REG_ITMP2, d);                   /* treat unordered as GT */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DCMPG:      /* ..., val1, val2  ==> ..., val1 fcmpg val2    */
 			                  /* == => 0, < => 1, > => -1 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_CLR(d);
			M_MOV_IMM(1, REG_ITMP1);
			M_MOV_IMM(-1, REG_ITMP2);
			emit_ucomisd_reg_reg(cd, s1, s2);
			M_CMOVULT(REG_ITMP1, d);
			M_CMOVUGT(REG_ITMP2, d);
			M_CMOVP(REG_ITMP1, d);                   /* treat unordered as LT */
			emit_store_dst(jd, iptr, d);
			break;


		/* memory operations **************************************************/

		case ICMD_ARRAYLENGTH: /* ..., arrayref  ==> ..., (int) length        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			M_ILD(d, s1, OFFSET(java_arrayheader, size));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_BALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
   			emit_movsbq_memindex_reg(cd, OFFSET(java_bytearray, data[0]), s1, s2, 0, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_CALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movzwq_memindex_reg(cd, OFFSET(java_chararray, data[0]), s1, s2, 1, d);
			emit_store_dst(jd, iptr, d);
			break;			

		case ICMD_SALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movswq_memindex_reg(cd, OFFSET(java_shortarray, data[0]), s1, s2, 1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movl_memindex_reg(cd, OFFSET(java_intarray, data[0]), s1, s2, 2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_mov_memindex_reg(cd, OFFSET(java_longarray, data[0]), s1, s2, 3, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movss_memindex_reg(cd, OFFSET(java_floatarray, data[0]), s1, s2, 2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movsd_memindex_reg(cd, OFFSET(java_doublearray, data[0]), s1, s2, 3, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_AALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_mov_memindex_reg(cd, OFFSET(java_objectarray, data[0]), s1, s2, 3, d);
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_BASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			emit_movb_reg_memindex(cd, s3, OFFSET(java_bytearray, data[0]), s1, s2, 0);
			break;

		case ICMD_CASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			emit_movw_reg_memindex(cd, s3, OFFSET(java_chararray, data[0]), s1, s2, 1);
			break;

		case ICMD_SASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			emit_movw_reg_memindex(cd, s3, OFFSET(java_shortarray, data[0]), s1, s2, 1);
			break;

		case ICMD_IASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			emit_movl_reg_memindex(cd, s3, OFFSET(java_intarray, data[0]), s1, s2, 2);
			break;

		case ICMD_LASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			emit_mov_reg_memindex(cd, s3, OFFSET(java_longarray, data[0]), s1, s2, 3);
			break;

		case ICMD_FASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_FTMP3);
			emit_movss_reg_memindex(cd, s3, OFFSET(java_floatarray, data[0]), s1, s2, 2);
			break;

		case ICMD_DASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_FTMP3);
			emit_movsd_reg_memindex(cd, s3, OFFSET(java_doublearray, data[0]), s1, s2, 3);
			break;

		case ICMD_AASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);

			M_MOV(s1, REG_A0);
			M_MOV(s3, REG_A1);
			M_MOV_IMM(BUILTIN_canstore, REG_ITMP1);
			M_CALL(REG_ITMP1);
			emit_exception_check(cd, iptr);

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			emit_mov_reg_memindex(cd, s3, OFFSET(java_objectarray, data[0]), s1, s2, 3);
			break;


		case ICMD_BASTORECONST: /* ..., arrayref, index  ==> ...              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movb_imm_memindex(cd, iptr->sx.s23.s3.constval, OFFSET(java_bytearray, data[0]), s1, s2, 0);
			break;

		case ICMD_CASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movw_imm_memindex(cd, iptr->sx.s23.s3.constval, OFFSET(java_chararray, data[0]), s1, s2, 1);
			break;

		case ICMD_SASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movw_imm_memindex(cd, iptr->sx.s23.s3.constval, OFFSET(java_shortarray, data[0]), s1, s2, 1);
			break;

		case ICMD_IASTORECONST: /* ..., arrayref, index  ==> ...              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movl_imm_memindex(cd, iptr->sx.s23.s3.constval, OFFSET(java_intarray, data[0]), s1, s2, 2);
			break;

		case ICMD_LASTORECONST: /* ..., arrayref, index  ==> ...              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);

			if (IS_IMM32(iptr->sx.s23.s3.constval)) {
				emit_mov_imm_memindex(cd, (u4) (iptr->sx.s23.s3.constval & 0x00000000ffffffff), OFFSET(java_longarray, data[0]), s1, s2, 3);
			}
			else {
				emit_movl_imm_memindex(cd, (u4) (iptr->sx.s23.s3.constval & 0x00000000ffffffff), OFFSET(java_longarray, data[0]), s1, s2, 3);
				emit_movl_imm_memindex(cd, (u4) (iptr->sx.s23.s3.constval >> 32), OFFSET(java_longarray, data[0]) + 4, s1, s2, 3);
			}
			break;

		case ICMD_AASTORECONST: /* ..., arrayref, index  ==> ...              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_mov_imm_memindex(cd, 0, OFFSET(java_objectarray, data[0]), s1, s2, 3);
			break;


		case ICMD_GETSTATIC:  /* ...  ==> ..., value                          */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = dseg_add_unique_address(cd, NULL);
				disp      = disp + -((cd->mcodeptr + 7) - cd->mcodebase);

				/* must be calculated before codegen_add_patch_ref */

				if (opt_shownops)
					disp -= PATCHER_CALL_SIZE;

/* 				PROFILE_CYCLE_STOP; */

				codegen_add_patch_ref(cd, PATCHER_get_putstatic, uf, disp);

/* 				PROFILE_CYCLE_START; */
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = dseg_add_address(cd, &(fi->value));
				disp      = disp + -((cd->mcodeptr + 7) - cd->mcodebase);

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class)) {
					PROFILE_CYCLE_STOP;

					codegen_add_patch_ref(cd, PATCHER_clinit, fi->class, 0);

					PROFILE_CYCLE_START;

					if (opt_shownops)
						disp -= PATCHER_CALL_SIZE;
				}
  			}

			/* This approach is much faster than moving the field
			   address inline into a register. */

  			M_ALD(REG_ITMP1, RIP, disp);

			switch (fieldtype) {
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD(d, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_LLD(d, REG_ITMP1, 0);
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
				disp      = dseg_add_unique_address(cd, NULL);
				disp      = disp + -((cd->mcodeptr + 7) - cd->mcodebase);

				/* must be calculated before codegen_add_patch_ref */

				if (opt_shownops)
					disp -= PATCHER_CALL_SIZE;

/* 				PROFILE_CYCLE_STOP; */

				codegen_add_patch_ref(cd, PATCHER_get_putstatic, uf, disp);

/* 				PROFILE_CYCLE_START; */
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = dseg_add_address(cd, &(fi->value));
				disp      = disp + -((cd->mcodeptr + 7) - cd->mcodebase);

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class)) {
					PROFILE_CYCLE_STOP;

					codegen_add_patch_ref(cd, PATCHER_clinit, fi->class, 0);

					PROFILE_CYCLE_START;

					if (opt_shownops)
						disp -= PATCHER_CALL_SIZE;
				}
  			}

			/* This approach is much faster than moving the field
			   address inline into a register. */

  			M_ALD(REG_ITMP1, RIP, disp);

			switch (fieldtype) {
			case TYPE_INT:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_IST(s1, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
			case TYPE_ADR:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_LST(s1, REG_ITMP1, 0);
				break;
			case TYPE_FLT:
				s1 = emit_load_s1(jd, iptr, REG_FTMP1);
				M_FST(s1, REG_ITMP1, 0);
				break;
			case TYPE_DBL:
				s1 = emit_load_s1(jd, iptr, REG_FTMP1);
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
				disp      = dseg_add_unique_address(cd, NULL);
				disp      = disp + -((cd->mcodeptr + 7) - cd->mcodebase);

				/* must be calculated before codegen_add_patch_ref */

				if (opt_shownops)
					disp -= PATCHER_CALL_SIZE;

/* 				PROFILE_CYCLE_STOP; */

				codegen_add_patch_ref(cd, PATCHER_get_putstatic, uf, disp);

/* 				PROFILE_CYCLE_START; */
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = dseg_add_address(cd, &(fi->value));
				disp      = disp + -((cd->mcodeptr + 7) - cd->mcodebase);

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class)) {
					PROFILE_CYCLE_STOP;

					codegen_add_patch_ref(cd, PATCHER_clinit, fi->class, 0);

					PROFILE_CYCLE_START;

					if (opt_shownops)
						disp -= PATCHER_CALL_SIZE;
				}
  			}

			/* This approach is much faster than moving the field
			   address inline into a register. */

  			M_ALD(REG_ITMP1, RIP, disp);

			switch (fieldtype) {
			case TYPE_INT:
			case TYPE_FLT:
				M_IST_IMM(iptr->sx.s23.s2.constval, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
			case TYPE_ADR:
			case TYPE_DBL:
				if (IS_IMM32(iptr->sx.s23.s2.constval))
					M_LST_IMM32(iptr->sx.s23.s2.constval, REG_ITMP1, 0);
				else {
					M_IST_IMM(iptr->sx.s23.s2.constval, REG_ITMP1, 0);
					M_IST_IMM(iptr->sx.s23.s2.constval >> 32, REG_ITMP1, 4);
				}
				break;
			}
			break;

		case ICMD_GETFIELD:   /* ...  ==> ..., value                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = 0;

/* 				PROFILE_CYCLE_STOP; */

				codegen_add_patch_ref(cd, PATCHER_get_putfield, uf, 0);

/* 				PROFILE_CYCLE_START; */
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = fi->offset;
			}

			/* implicit null-pointer check */
			switch (fieldtype) {
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
				M_ILD32(d, s1, disp);
				break;
			case TYPE_LNG:
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
				M_LLD32(d, s1, disp);
				break;
			case TYPE_FLT:
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_FLD32(d, s1, disp);
				break;
			case TYPE_DBL:				
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_DLD32(d, s1, disp);
				break;
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_PUTFIELD:   /* ..., objectref, value  ==> ...               */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_IFTMP); /* REG_IFTMP == REG_ITMP2 */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = 0;

/* 				PROFILE_CYCLE_STOP; */

				codegen_add_patch_ref(cd, PATCHER_get_putfield, uf, 0);

/* 				PROFILE_CYCLE_START; */
			} 
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = fi->offset;
			}

			/* implicit null-pointer check */
			switch (fieldtype) {
			case TYPE_INT:
				M_IST32(s2, s1, disp);
				break;
			case TYPE_LNG:
			case TYPE_ADR:
				M_LST32(s2, s1, disp);
				break;
			case TYPE_FLT:
				M_FST32(s2, s1, disp);
				break;
			case TYPE_DBL:
				M_DST32(s2, s1, disp);
				break;
			}
			break;

		case ICMD_PUTFIELDCONST:  /* ..., objectref, value  ==> ...           */
		                          /* val = value (in current instruction)     */
		                          /* following NOP)                           */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = 0;

/* 				PROFILE_CYCLE_STOP; */

				codegen_add_patch_ref(cd, PATCHER_putfieldconst, uf, 0);

/* 				PROFILE_CYCLE_START; */
			} 
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = fi->offset;
			}

			/* implicit null-pointer check */
			switch (fieldtype) {
			case TYPE_INT:
			case TYPE_FLT:
				M_IST32_IMM(iptr->sx.s23.s2.constval, s1, disp);
				break;
			case TYPE_LNG:
			case TYPE_ADR:
			case TYPE_DBL:
				/* XXX why no check for IS_IMM32? */
				M_IST32_IMM(iptr->sx.s23.s2.constval, s1, disp);
				M_IST32_IMM(iptr->sx.s23.s2.constval >> 32, s1, disp + 4);
				break;
			}
			break;


		/* branch operations **************************************************/

		case ICMD_ATHROW:       /* ..., objectref ==> ... (, objectref)       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1_XPTR);

			PROFILE_CYCLE_STOP;

#ifdef ENABLE_VERIFIER
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uc = iptr->sx.s23.s2.uc;

				codegen_add_patch_ref(cd, PATCHER_athrow_areturn, uc, 0);
			}
#endif /* ENABLE_VERIFIER */

			M_CALL_IMM(0);                            /* passing exception pc */
			M_POP(REG_ITMP2_XPC);

  			M_MOV_IMM(asm_handle_exception, REG_ITMP3);
  			M_JMP(REG_ITMP3);
			break;

		case ICMD_GOTO:         /* ... ==> ...                                */
		case ICMD_RET:

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
			M_TEST(s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IFNULL, BRANCH_OPT_NONE);
			break;

		case ICMD_IFEQ:         /* ..., value ==> ...                         */
		case ICMD_IFLT:
		case ICMD_IFLE:
		case ICMD_IFNE:
		case ICMD_IFGT:
		case ICMD_IFGE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_ICMP_IMM(iptr->sx.val.i, s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IFEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_LEQ:       /* ..., value ==> ...                         */
		case ICMD_IF_LNE:
		case ICMD_IF_LLT:
		case ICMD_IF_LGE:
		case ICMD_IF_LGT:
		case ICMD_IF_LLE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (IS_IMM32(iptr->sx.val.l))
				M_LCMP_IMM(iptr->sx.val.l, s1);
			else {
				M_MOV_IMM(iptr->sx.val.l, REG_ITMP2);
				M_LCMP(REG_ITMP2, s1);
			}
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_LEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_ICMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ICMPNE:
		case ICMD_IF_ICMPLT:
		case ICMD_IF_ICMPGE:
		case ICMD_IF_ICMPGT:
		case ICMD_IF_ICMPLE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_ICMP(s2, s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_ICMPEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_ACMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ACMPNE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_LCMP(s2, s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_ACMPEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_LCMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPNE:
		case ICMD_IF_LCMPLT:
		case ICMD_IF_LCMPGE:
		case ICMD_IF_LCMPGT:
		case ICMD_IF_LCMPLE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_LCMP(s2, s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_LCMPEQ, BRANCH_OPT_NONE);
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
				uc = iptr->sx.s23.s2.uc;

				PROFILE_CYCLE_STOP;

				codegen_add_patch_ref(cd, PATCHER_athrow_areturn, uc, 0);

				PROFILE_CYCLE_START;
			}
#endif /* ENABLE_VERIFIER */
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

#if !defined(NDEBUG)
			if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
				emit_verbosecall_exit(jd);
#endif /* !defined(NDEBUG) */

#if defined(ENABLE_THREADS)
			if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
				M_ALD(REG_A0, REG_SP, rd->memuse * 8);
	
				/* we need to save the proper return value */
				switch (iptr->opc) {
				case ICMD_IRETURN:
				case ICMD_ARETURN:
				case ICMD_LRETURN:
					M_LST(REG_RESULT, REG_SP, rd->memuse * 8);
					break;
				case ICMD_FRETURN:
				case ICMD_DRETURN:
					M_DST(REG_FRESULT, REG_SP, rd->memuse * 8);
					break;
				}

				M_MOV_IMM(LOCK_monitor_exit, REG_ITMP1);
				M_CALL(REG_ITMP1);

				/* and now restore the proper return value */
				switch (iptr->opc) {
				case ICMD_IRETURN:
				case ICMD_ARETURN:
				case ICMD_LRETURN:
					M_LLD(REG_RESULT, REG_SP, rd->memuse * 8);
					break;
				case ICMD_FRETURN:
				case ICMD_DRETURN:
					M_DLD(REG_FRESULT, REG_SP, rd->memuse * 8);
					break;
				}
			}
#endif

			/* restore saved registers */

			for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
				p--; M_LLD(rd->savintregs[i], REG_SP, p * 8);
			}
			for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
  				p--; M_DLD(rd->savfltregs[i], REG_SP, p * 8);
			}

			/* deallocate stack */

			if (cd->stackframesize)
				M_AADD_IMM(cd->stackframesize * 8, REG_SP);

			/* generate method profiling code */

			PROFILE_CYCLE_STOP;

			M_RET;
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
				M_INTMOVE(s1, REG_ITMP1);

				if (l != 0)
					M_ISUB_IMM(l, REG_ITMP1);

				/* number of targets */
				i = i - l + 1;

                /* range check */

				M_ICMP_IMM(i - 1, REG_ITMP1);
				emit_bugt(cd, table[0].block);

				/* build jump table top down and use address of lowest entry */

				table += i;

				while (--i >= 0) {
					dseg_add_target(cd, table->block);
					--table;
				}

				/* length of dataseg after last dseg_add_target is used
				   by load */

				M_MOV_IMM(0, REG_ITMP2);
				dseg_adddata(cd);
				emit_mov_memindex_reg(cd, -(cd->dseglen), REG_ITMP2, REG_ITMP1, 3, REG_ITMP1);
				M_JMP(REG_ITMP1);
			}
			break;


		case ICMD_LOOKUPSWITCH: /* ..., key ==> ...                           */
			{
				s4 i;
				lookup_target_t *lookup;

				lookup = iptr->dst.lookup;

				i = iptr->sx.s23.s2.lookupcount;
			
				MCODECHECK(8 + ((7 + 6) * i) + 5);
				s1 = emit_load_s1(jd, iptr, REG_ITMP1);

				while (--i >= 0) {
					M_ICMP_IMM(lookup->value, s1);
					emit_beq(cd, lookup->target.block);
					lookup++;
				}

				emit_br(cd, iptr->sx.s23.s3.lookupdefault.block);
				ALIGNCODENOP;
			}
			break;


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

			MCODECHECK((20 * s3) + 128);

			/* copy arguments to registers or stack location */

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

						if (IS_2_WORD_TYPE(var->type))
							M_DST(s1, REG_SP, d * 8);
						else
							M_FST(s1, REG_SP, d * 8);
					}
				}
			}

			/* generate method profiling code */

			PROFILE_CYCLE_STOP;

			switch (iptr->opc) {
			case ICMD_BUILTIN:
				M_MOV_IMM(bte->fp, REG_ITMP1);
				M_CALL(REG_ITMP1);

				emit_exception_check(cd, iptr);
				break;

			case ICMD_INVOKESPECIAL:
				emit_nullpointer_check(cd, iptr, REG_A0);
				/* fall through */

			case ICMD_INVOKESTATIC:
				if (lm == NULL) {
					disp = dseg_add_unique_address(cd, NULL);
					disp = disp + -((cd->mcodeptr + 7) - cd->mcodebase);

					/* must be calculated before codegen_add_patch_ref */

					if (opt_shownops)
						disp -= PATCHER_CALL_SIZE;

					codegen_add_patch_ref(cd, PATCHER_invokestatic_special,
										  um, disp);

/* 					a = 0; */
				}
				else {
					disp = dseg_add_functionptr(cd, lm->stubroutine);
					disp = disp + -((cd->mcodeptr + 7) - cd->mcodebase);

/* 					a = (ptrint) lm->stubroutine; */
				}

/* 				M_MOV_IMM(a, REG_ITMP2); */
				M_ALD(REG_ITMP2, RIP, disp);
				M_CALL(REG_ITMP2);
				break;

			case ICMD_INVOKEVIRTUAL:
				if (lm == NULL) {
					codegen_add_patch_ref(cd, PATCHER_invokevirtual, um, 0);

					s1 = 0;
				}
				else {
					s1 = OFFSET(vftbl_t, table[0]) +
						sizeof(methodptr) * lm->vftblindex;
				}

				/* implicit null-pointer check */
				M_ALD(REG_METHODPTR, REG_A0, OFFSET(java_objectheader, vftbl));
				M_ALD32(REG_ITMP3, REG_METHODPTR, s1);
				M_CALL(REG_ITMP3);
				break;

			case ICMD_INVOKEINTERFACE:
				if (lm == NULL) {
					codegen_add_patch_ref(cd, PATCHER_invokeinterface, um, 0);

					s1 = 0;
					s2 = 0;
				}
				else {
					s1 = OFFSET(vftbl_t, interfacetable[0]) -
						sizeof(methodptr) * lm->class->index;

					s2 = sizeof(methodptr) * (lm - lm->class->methods);
				}

				/* implicit null-pointer check */
				M_ALD(REG_METHODPTR, REG_A0, OFFSET(java_objectheader, vftbl));
				M_ALD32(REG_METHODPTR, REG_METHODPTR, s1);
				M_ALD32(REG_ITMP3, REG_METHODPTR, s2);
				M_CALL(REG_ITMP3);
				break;
			}

			/* generate method profiling code */

			PROFILE_CYCLE_START;

			/* store size of call code in replacement point */

			REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);

			/* store return value */

			switch (md->returntype.type) {
			case TYPE_INT:
			case TYPE_LNG:
			case TYPE_ADR:
				s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT);
				M_INTMOVE(REG_RESULT, s1);
				emit_store_dst(jd, iptr, s1);
				break;
			case TYPE_FLT:
			case TYPE_DBL:
				s1 = codegen_reg_of_dst(jd, iptr, REG_FRESULT);
				M_FLTMOVE(REG_FRESULT, s1);
				emit_store_dst(jd, iptr, s1);
				break;
			default:
				/* TYPE_VOID */
				break;
			}
			break;


		case ICMD_CHECKCAST:  /* ..., objectref ==> ..., objectref            */

			if (!(iptr->flags.bits & INS_FLAG_ARRAY)) {
				/* object type cast-check */

				classinfo *super;
				vftbl_t   *supervftbl;
				s4         superindex;

				if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
					super      = NULL;
					superindex = 0;
					supervftbl = NULL;
				}
				else {
					super      = iptr->sx.s23.s3.c.cls;
					superindex = super->index;
					supervftbl = super->vftbl;
				}

				if ((super == NULL) || !(super->flags & ACC_INTERFACE))
					CODEGEN_CRITICAL_SECTION_NEW;

				s1 = emit_load_s1(jd, iptr, REG_ITMP1);

				/* if class is not resolved, check which code to call */

				if (super == NULL) {
					M_TEST(s1);
					emit_label_beq(cd, BRANCH_LABEL_1);

					codegen_add_patch_ref(cd, PATCHER_checkcast_instanceof_flags,
										  iptr->sx.s23.s3.c.ref, 0);

					M_IMOV_IMM(0, REG_ITMP2);                 /* super->flags */
					M_IAND_IMM(ACC_INTERFACE, REG_ITMP2);
					emit_label_beq(cd, BRANCH_LABEL_2);
				}

				/* interface checkcast code */

				if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
					if (super != NULL) {
						M_TEST(s1);
						emit_label_beq(cd, BRANCH_LABEL_3);
					}

					M_ALD(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));

					if (super == NULL) {
						codegen_add_patch_ref(cd, PATCHER_checkcast_interface,
											  iptr->sx.s23.s3.c.ref,
											  0);
					}

					M_ILD32(REG_ITMP3,
							REG_ITMP2, OFFSET(vftbl_t, interfacetablelength));
					M_ICMP_IMM32(superindex, REG_ITMP3);
					emit_classcast_check(cd, iptr, BRANCH_LE, REG_ITMP3, s1);

					M_ALD32(REG_ITMP3, REG_ITMP2, 
							OFFSET(vftbl_t, interfacetable[0]) -
							superindex * sizeof(methodptr*));
					M_TEST(REG_ITMP3);
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
					}
					else {
						M_TEST(s1);
						emit_label_beq(cd, BRANCH_LABEL_5);
					}

					M_ALD(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));

					if (super == NULL) {
						codegen_add_patch_ref(cd, PATCHER_checkcast_class,
											  iptr->sx.s23.s3.c.ref,
											  0);
					}

					M_MOV_IMM(supervftbl, REG_ITMP3);

					CODEGEN_CRITICAL_SECTION_START;

					M_ILD32(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, baseval));

					/*  					if (s1 != REG_ITMP1) { */
					/*  						emit_movl_membase_reg(cd, REG_ITMP3, */
					/*  												OFFSET(vftbl_t, baseval), */
					/*  												REG_ITMP1); */
					/*  						emit_movl_membase_reg(cd, REG_ITMP3, */
					/*  												OFFSET(vftbl_t, diffval), */
					/*  												REG_ITMP3); */
					/*  #if defined(ENABLE_THREADS) */
					/*  						codegen_threadcritstop(cd, cd->mcodeptr - cd->mcodebase); */
					/*  #endif */
					/*  						emit_alu_reg_reg(cd, ALU_SUB, REG_ITMP1, REG_ITMP2); */

					/*  					} else { */

					M_ILD32(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, baseval));
					M_ISUB(REG_ITMP3, REG_ITMP2);
					M_MOV_IMM(supervftbl, REG_ITMP3);
					M_ILD(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, diffval));
					/*  					} */

					CODEGEN_CRITICAL_SECTION_END;

					M_ICMP(REG_ITMP3, REG_ITMP2);
					emit_classcast_check(cd, iptr, BRANCH_UGT, REG_ITMP3, s1);

					if (super != NULL)
						emit_label(cd, BRANCH_LABEL_5);
				}

				if (super == NULL) {
					emit_label(cd, BRANCH_LABEL_1);
					emit_label(cd, BRANCH_LABEL_4);
				}

				d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			}
			else {
				/* array type cast-check */

				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_INTMOVE(s1, REG_A0);

				if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
					codegen_add_patch_ref(cd, PATCHER_builtin_arraycheckcast,
										  iptr->sx.s23.s3.c.ref, 0);
				}

				M_MOV_IMM(iptr->sx.s23.s3.c.cls, REG_A1);
				M_MOV_IMM(BUILTIN_arraycheckcast, REG_ITMP1);
				M_CALL(REG_ITMP1);

				/* s1 may have been destroyed over the function call */
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_TEST(REG_RESULT);
				emit_classcast_check(cd, iptr, BRANCH_EQ, REG_RESULT, s1);

				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
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
				super      = NULL;
				superindex = 0;
				supervftbl = NULL;
			}
			else {
				super      = iptr->sx.s23.s3.c.cls;
				superindex = super->index;
				supervftbl = super->vftbl;
			}

			if ((super == NULL) || !(super->flags & ACC_INTERFACE))
				CODEGEN_CRITICAL_SECTION_NEW;

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);

			if (s1 == d) {
				M_INTMOVE(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			}

			M_CLR(d);

			/* if class is not resolved, check which code to call */

			if (super == NULL) {
				M_TEST(s1);
				emit_label_beq(cd, BRANCH_LABEL_1);

				codegen_add_patch_ref(cd, PATCHER_checkcast_instanceof_flags,
									  iptr->sx.s23.s3.c.ref, 0);

				M_IMOV_IMM(0, REG_ITMP3);                     /* super->flags */
				M_IAND_IMM(ACC_INTERFACE, REG_ITMP3);
				emit_label_beq(cd, BRANCH_LABEL_2);
			}

			/* interface instanceof code */

			if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
				if (super != NULL) {
					M_TEST(s1);
					emit_label_beq(cd, BRANCH_LABEL_3);
				}

				M_ALD(REG_ITMP1, s1, OFFSET(java_objectheader, vftbl));

				if (super == NULL) {
					codegen_add_patch_ref(cd, PATCHER_instanceof_interface,
										  iptr->sx.s23.s3.c.ref, 0);
				}

				M_ILD32(REG_ITMP3,
						REG_ITMP1, OFFSET(vftbl_t, interfacetablelength));
				M_ICMP_IMM32(superindex, REG_ITMP3);

				a = 3 + 4 /* mov_membase32_reg */ + 3 /* test */ + 4 /* setcc */;

				M_BLE(a);
				M_ALD32(REG_ITMP1, REG_ITMP1,
						OFFSET(vftbl_t, interfacetable[0]) -
						superindex * sizeof(methodptr*));
				M_TEST(REG_ITMP1);
				M_SETNE(d);

				if (super == NULL)
					emit_label_br(cd, BRANCH_LABEL_4);
				else
					emit_label(cd, BRANCH_LABEL_3);
			}

			/* class instanceof code */

			if ((super == NULL) || !(super->flags & ACC_INTERFACE)) {
				if (super == NULL) {
					emit_label(cd, BRANCH_LABEL_2);
				}
				else {
					M_TEST(s1);
					emit_label_beq(cd, BRANCH_LABEL_5);
				}

				M_ALD(REG_ITMP1, s1, OFFSET(java_objectheader, vftbl));

				if (super == NULL) {
					codegen_add_patch_ref(cd, PATCHER_instanceof_class,
										  iptr->sx.s23.s3.c.ref, 0);
				}

				M_MOV_IMM(supervftbl, REG_ITMP2);

				CODEGEN_CRITICAL_SECTION_START;

				M_ILD(REG_ITMP1, REG_ITMP1, OFFSET(vftbl_t, baseval));
				M_ILD(REG_ITMP3, REG_ITMP2, OFFSET(vftbl_t, diffval));
				M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, baseval));

				CODEGEN_CRITICAL_SECTION_END;

				M_ISUB(REG_ITMP2, REG_ITMP1);
				M_CLR(d); /* may be REG_ITMP2 */
				M_ICMP(REG_ITMP3, REG_ITMP1);
				M_SETULE(d);

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

  			MCODECHECK((10 * 4 * iptr->s1.argcount) + 5 + 10 * 8);

			for (s1 = iptr->s1.argcount; --s1 >= 0; ) {

				/* copy SAVEDVAR sizes to stack */
				var = VAR(iptr->sx.s23.s2.args[s1]);

				/* Already Preallocated? */
				if (!(var->flags & PREALLOC)) {
					s2 = emit_load(jd, iptr, var, REG_ITMP1);
					M_LST(s2, REG_SP, s1 * 8);
				}
			}

			/* is a patcher function set? */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				codegen_add_patch_ref(cd, PATCHER_builtin_multianewarray,
									  iptr->sx.s23.s3.c.ref, 0);
			}

			/* a0 = dimension count */

			M_MOV_IMM(iptr->s1.argcount, REG_A0);

			/* a1 = classinfo */

			M_MOV_IMM(iptr->sx.s23.s3.c.cls, REG_A1);

			/* a2 = pointer to dimensions = stack pointer */

			M_MOV(REG_SP, REG_A2);

			M_MOV_IMM(BUILTIN_multianewarray, REG_ITMP1);
			M_CALL(REG_ITMP1);

			/* check for exception before result assignment */

			emit_exception_check(cd, iptr);

			s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT);
			M_INTMOVE(REG_RESULT, s1);
			emit_store_dst(jd, iptr, s1);
			break;

		default:
			exceptions_throw_internalerror("Unknown ICMD %d during code generation",
										   iptr->opc);
			return false;
	} /* switch */

	} /* for instruction */
		
	MCODECHECK(512); /* XXX require a lower number? */

	/* At the end of a basic block we may have to append some nops,
	   because the patcher stub calling code might be longer than the
	   actual instruction. So codepatching does not change the
	   following block unintentionally. */

	if (cd->mcodeptr < cd->lastmcodeptr) {
		while (cd->mcodeptr < cd->lastmcodeptr) {
			M_NOP;
		}
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

   Emit a stub routine which calls the compiler.
	
*******************************************************************************/

void codegen_emit_stub_compiler(jitdata *jd)
{
	methodinfo  *m;
	codegendata *cd;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;

	/* code for the stub */

	M_ALD(REG_ITMP1, RIP, -(7 * 1 + 2 * SIZEOF_VOID_P));       /* methodinfo  */
	M_ALD(REG_ITMP3, RIP, -(7 * 2 + 3 * SIZEOF_VOID_P));  /* compiler pointer */
	M_JMP(REG_ITMP3);
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
	s4           s1, s2;

	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;

	/* initialize variables */

	md = m->parseddesc;
	nativeparams = (m->flags & ACC_STATIC) ? 2 : 1;

	/* calculate stack frame size */

	cd->stackframesize =
		sizeof(stackframeinfo) / SIZEOF_VOID_P +
		sizeof(localref_table) / SIZEOF_VOID_P +
		md->paramcount +
		1 +                       /* functionptr, TODO: store in data segment */
		nmd->memuse;

	cd->stackframesize |= 0x1;                  /* keep stack 16-byte aligned */

	/* create method header */

	(void) dseg_add_unique_address(cd, code);              /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 8); /* FrameSize       */
	(void) dseg_add_unique_s4(cd, 0);                      /* IsSync          */
	(void) dseg_add_unique_s4(cd, 0);                      /* IsLeaf          */
	(void) dseg_add_unique_s4(cd, 0);                      /* IntSave         */
	(void) dseg_add_unique_s4(cd, 0);                      /* FltSave         */
	(void) dseg_addlinenumbertablesize(cd);
	(void) dseg_add_unique_s4(cd, 0);                      /* ExTableSize     */

#if defined(ENABLE_PROFILING)
	/* generate native method profiling code */

	if (JITDATA_HAS_FLAG_INSTRUMENT(jd)) {
		/* count frequency */

		M_MOV_IMM(code, REG_ITMP3);
		M_IINC_MEMBASE(REG_ITMP3, OFFSET(codeinfo, frequency));
	}
#endif

	/* generate stub code */

	M_ASUB_IMM(cd->stackframesize * 8, REG_SP);

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_enter(jd);
#endif

	/* get function address (this must happen before the stackframeinfo) */

#if !defined(WITH_STATIC_CLASSPATH)
	if (f == NULL)
		codegen_add_patch_ref(cd, PATCHER_resolve_native, m, 0);
#endif

	M_MOV_IMM(f, REG_ITMP3);

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

	M_AST(REG_ITMP3, REG_SP, md->paramcount * 8);

	/* create dynamic stack info */

	M_ALEA(REG_SP, cd->stackframesize * 8, REG_A0);
	emit_lea_membase_reg(cd, RIP, -((cd->mcodeptr + 7) - cd->mcodebase), REG_A1);
	M_ALEA(REG_SP, cd->stackframesize * 8 + SIZEOF_VOID_P, REG_A2);
	M_ALD(REG_A3, REG_SP, cd->stackframesize * 8);
	M_MOV_IMM(codegen_start_native_call, REG_ITMP1);
	M_CALL(REG_ITMP1);

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

	M_ALD(REG_ITMP3, REG_SP, md->paramcount * 8);

	/* copy or spill arguments to new locations */

	for (i = md->paramcount - 1, j = i + nativeparams; i >= 0; i--, j--) {
		t  = md->paramtypes[i].type;
		s2 = nmd->params[j].regoff;

		if (IS_INT_LNG_TYPE(t)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;

				if (!nmd->params[j].inmemory)
					M_INTMOVE(s1, s2);
				else
					M_LST(s1, REG_SP, s2 * 8);
			}
			else {
				s1 = md->params[i].regoff + cd->stackframesize + 1;/* +1 (RA) */
				M_LLD(REG_ITMP1, REG_SP, s1 * 8);
				M_LST(REG_ITMP1, REG_SP, s2 * 8);
			}
		}
		else {
			/* We only copy spilled float arguments, as the float
			   argument registers keep unchanged. */

			if (md->params[i].inmemory) {
				s1 = md->params[i].regoff + cd->stackframesize + 1;/* +1 (RA) */

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

	if (m->flags & ACC_STATIC)
		M_MOV_IMM(m->class, REG_A1);

	/* put env into first argument register */

	M_MOV_IMM(_Jv_env, REG_A0);

	/* do the native function call */

	M_CALL(REG_ITMP3);

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

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_exit(jd);
#endif

	/* remove native stackframe info */

	M_ALEA(REG_SP, cd->stackframesize * 8, REG_A0);
	M_MOV_IMM(codegen_finish_native_call, REG_ITMP1);
	M_CALL(REG_ITMP1);
	M_MOV(REG_RESULT, REG_ITMP3);

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

	/* remove stackframe */

	M_AADD_IMM(cd->stackframesize * 8, REG_SP);

	/* test for exception */

	M_TEST(REG_ITMP3);
	M_BNE(1);
	M_RET;

	/* handle exception */

	M_MOV(REG_ITMP3, REG_ITMP1_XPTR);
	M_ALD(REG_ITMP2_XPC, REG_SP, 0 * 8);     /* get return address from stack */
	M_ASUB_IMM(3, REG_ITMP2_XPC);                                    /* callq */

	M_MOV_IMM(asm_handle_nat_exception, REG_ITMP3);
	M_JMP(REG_ITMP3);

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

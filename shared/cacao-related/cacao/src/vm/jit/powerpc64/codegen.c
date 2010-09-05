/* src/vm/jit/powerpc64/codegen.c - machine code generator for 64-bit PowerPC

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

   $Id: codegen.c 7817 2007-04-25 19:42:50Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <signal.h>

#include "vm/types.h"

#include "md-abi.h"

#include "vm/jit/powerpc64/arch.h"
#include "vm/jit/powerpc64/codegen.h"

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
#include "vm/jit/md.h"
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

#include "vmcore/loader.h"
#include "vmcore/options.h"

#if defined(ENABLE_LSRA)
# include "vm/jit/allocator/lsra.h"
#endif


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
	s4                  fieldtype;
	s4                  varindex;
	unresolved_field   *uf;
	fieldinfo          *fi;

	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;
	rd   = jd->rd;

	/* prevent compiler warnings */

	d = 0;
	lm = NULL;
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
	/* space to save argument of monitor_enter and Return Values to survive */
    /* monitor_exit. The stack position for the argument can not be shared  */
	/* with place to save the return register on PPC64, since both values     */
	/* reside in R3 */
	if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
		/* reserve 2 slots for long/double return values for monitorexit */
		cd->stackframesize += 2;
	}

#endif

	/* create method header */

	/* align stack to 16-bytes */

/* FIXME */
/* 	if (!m->isleafmethod || opt_verbosecall) */
/*		stackframesize = (stackframesize + 3) & ~3;
*/
/* 	else if (m->isleafmethod && (stackframesize == LA_WORD_SIZE)) */
/* 		stackframesize = 0; */

	(void) dseg_add_unique_address(cd, code);                      /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 8);             /* FrameSize       */

#if defined(ENABLE_THREADS)
	/* IsSync contains the offset relative to the stack pointer for the
	   argument of monitor_exit used in the exception handler. Since the
	   offset could be zero and give a wrong meaning of the flag it is
	   offset by one.
	*/

	if (checksync && (m->flags & ACC_SYNCHRONIZED))
		(void) dseg_add_unique_s4(cd, (rd->memuse + 1) * 8);       /* IsSync          */
	else
#endif
		(void) dseg_add_unique_s4(cd, 0);                          /* IsSync          */
	                                       
	(void) dseg_add_unique_s4(cd, jd->isleafmethod);                /* IsLeaf          */
	(void) dseg_add_unique_s4(cd, INT_SAV_CNT - rd->savintreguse); /* IntSave         */
	(void) dseg_add_unique_s4(cd, FLT_SAV_CNT - rd->savfltreguse); /* FltSave         */

	dseg_addlinenumbertablesize(cd);

	(void) dseg_add_unique_s4(cd, jd->exceptiontablelength);       /* ExTableSize     */

	/* create exception table */

	for (ex = jd->exceptiontable; ex != NULL; ex = ex->down) {
		dseg_add_target(cd, ex->start);
   		dseg_add_target(cd, ex->end);
		dseg_add_target(cd, ex->handler);
		(void) dseg_add_unique_address(cd, ex->catchtype.any);
	}
	
	/* create stack frame (if necessary) */

	if (!jd->isleafmethod) {
		M_MFLR(REG_ZERO);
		M_AST(REG_ZERO, REG_SP, LA_LR_OFFSET);
	}

	if (cd->stackframesize)
		M_STDU(REG_SP, REG_SP, -cd->stackframesize * 8);

	/* save return address and used callee saved registers */

	p = cd->stackframesize;
	for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
		p--; M_LST(rd->savintregs[i], REG_SP, p * 8);
	}
	for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
		p --; M_DST(rd->savfltregs[i], REG_SP, p * 8);
	}

	/* take arguments out of register or stack frame */

	md = m->parseddesc;

 	for (p = 0, l = 0; p < md->paramcount; p++) {
 		t = md->paramtypes[p].type;
 		varindex = jd->local_map[l*5 + t];
 		l++;
 		if (IS_2_WORD_TYPE(t))    /* increment local counter for 2 word types */
 			l++;
 		if (varindex == UNUSED)
 			continue;

		var = VAR(varindex);
		s1  = md->params[p].regoff;

		if (IS_INT_LNG_TYPE(t)) {
 			if (!md->params[p].inmemory) {
				if (!IS_INMEMORY(var->flags))
					M_INTMOVE(s1, var->vv.regoff);
				else
					M_LST(s1, REG_SP, var->vv.regoff * 8);
			}
			else {
 				if (!IS_INMEMORY(var->flags))
					M_LLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8);
				else
					var->vv.regoff = cd->stackframesize + s1;
			}
		}
		else {
 			if (!md->params[p].inmemory) {
 				if (!IS_INMEMORY(var->flags))
 					M_FLTMOVE(s1, var->vv.regoff);
				else
					M_DST(s1, REG_SP, var->vv.regoff * 8);
 			}
			else {
 				if (!(var->flags & INMEMORY)) {
					if (IS_2_WORD_TYPE(t))
						M_DLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8);
					else
						M_DLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 8);
 				}
				else
					var->vv.regoff = cd->stackframesize + s1;
			}
		}
	}

	/* save monitorenter argument */

#if defined(ENABLE_THREADS)

	if (checksync && (m->flags & ACC_SYNCHRONIZED)) {

		/* stackoffset for argument used for LOCK_monitor_exit */
		s1 = rd->memuse;

#if !defined (NDEBUG)
		if (JITDATA_HAS_FLAG_VERBOSECALL(jd)) {
			M_AADD_IMM(REG_SP, -((LA_SIZE_IN_POINTERS + PA_SIZE_IN_POINTERS + ARG_CNT) * 8), REG_SP);

			for (p = 0; p < INT_ARG_CNT; p++)
				M_LST(abi_registers_integer_argument[p], REG_SP, LA_SIZE + PA_SIZE + p * 8);

			for (p = 0; p < FLT_ARG_CNT; p++)
				M_DST(abi_registers_float_argument[p], REG_SP, LA_SIZE + PA_SIZE + (INT_ARG_CNT + p) * 8);

			/* used for LOCK_monitor_exit, adopt size because we created another stackframe */
			s1 += (LA_SIZE_IN_POINTERS + PA_SIZE_IN_POINTERS + ARG_CNT);
		}
#endif

		p = dseg_add_functionptr(cd, LOCK_monitor_enter);
		M_ALD(REG_ITMP3, REG_PV, p);
		M_ALD(REG_ITMP3, REG_ITMP3, 0); /* TOC */
		M_MTCTR(REG_ITMP3);

		/* get or test the lock object */

		if (m->flags & ACC_STATIC) {
			p = dseg_add_address(cd, &m->class->object.header);
			M_ALD(REG_A0, REG_PV, p);
		}
		else {
			M_TST(REG_A0);
			M_BNE(1);
			M_ALD_INTERN(REG_ZERO, REG_ZERO, EXCEPTION_HARDWARE_NULLPOINTER);
		}

		M_AST(REG_A0, REG_SP, s1 * 8);                      /* rd->memuse * 8 */
		M_JSR;

#if !defined(NDEBUG)
		if (JITDATA_HAS_FLAG_VERBOSECALL(jd)) {
			for (p = 0; p < INT_ARG_CNT; p++)
				M_LLD(abi_registers_integer_argument[p], REG_SP, LA_SIZE + PA_SIZE + p * 8);

			for (p = 0; p < FLT_ARG_CNT; p++)
				M_DLD(abi_registers_float_argument[p], REG_SP, LA_SIZE + PA_SIZE + (INT_ARG_CNT + p) * 8);

			M_AADD_IMM(REG_SP, (LA_SIZE_IN_POINTERS + PA_SIZE_IN_POINTERS + ARG_CNT) * 8, REG_SP);
		}
#endif
	}
#endif

	/* call trace function */
#if !defined (NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_enter(jd);

	}
#endif

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
				var = VAR(bptr->invars[len]);
				if ((len == bptr->indepth-1) && (bptr->type == BBTYPE_EXH)) {
					/* d = reg_of_var(m, var, REG_ITMP1); */
					if (!(var->flags & INMEMORY))
						d = var->vv.regoff;
					else
						d = REG_ITMP1;
					M_INTMOVE(REG_ITMP1, d);
					emit_store(jd, NULL, var, d);
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
			M_TST(s1);
			M_BEQ(0);
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
				disp = dseg_add_unique_address(cd, iptr->sx.val.c.ref);
				codegen_addpatchref(cd, PATCHER_aconst,
									iptr->sx.val.c.ref,
								    disp);
			} else	{
				disp = dseg_add_address(cd, iptr->sx.val.anyptr);
			}
			M_ALD(d, REG_PV, disp);
			emit_store_dst(jd, iptr, d);
			break;


		/* load/store/copy/move operations ************************************/

		case ICMD_ILOAD:      /* ...  ==> ..., content of local variable      */
		case ICMD_ALOAD:      /* s1.localindex = local variable               */
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
			M_EXTSW(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LNEG:    
			s1 = emit_load_s1(jd, iptr, REG_ITMP1); 
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_NEG(s1, d);
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
			M_ISEXT(s1, d);	
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
			M_EXTSW(d,d);
			emit_store_dst(jd, iptr, d);
			break;

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
			M_EXTSW(d,d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_LADD(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADDCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* XXX check me */
			if ((iptr->sx.val.l >= -32768) && (iptr->sx.val.l <= 32767)) {
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
			M_SUB(s1, s2, d);
			M_EXTSW(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISUBCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= -32767) && (iptr->sx.val.i <= 32768)) {
				M_IADD_IMM(s1, -iptr->sx.val.i, d);
			} else {
				ICONST(REG_ITMP2, iptr->sx.val.i);
				M_SUB(s1, REG_ITMP2, d);
			}
			M_EXTSW(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_SUB(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUBCONST:  /* ..., value  ==> ..., value - constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* XXX check me */
			if ((iptr->sx.val.l >= -32767) && (iptr->sx.val.l <= 32767)) {
				M_LADD_IMM(s1, -iptr->sx.val.l, d);
			} else {
				LCONST(REG_ITMP2, iptr->sx.val.l);
				M_SUB(s1, REG_ITMP2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIV:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			emit_arithmetic_check(cd, iptr, s2);

			M_DIV(s1, s2, d);
			M_EXTSW(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			emit_arithmetic_check(cd, iptr, s2);

			M_DIV(s1, s2, d);
			/* we need to test if divident was 0x8000000000000, bit OV is set in XER in this case */
			/* we only need to check this if we did a LDIV, not for IDIV */
			M_MFXER(REG_ITMP2);
			M_ANDIS(REG_ITMP2, 0x4000, REG_ITMP2);	/* test OV */
			M_BLE(1);
			M_MOV(s1, d);				/* java specs says result == dividend */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREM:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arithmetic_check(cd, iptr, s2);

			M_DIV(s1, s2,  REG_ITMP3);	
			M_MUL(REG_ITMP3, s2, REG_ITMP2);
			M_SUB(s1, REG_ITMP2,  REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);

			M_MOV(REG_ITMP3, d);
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_LREM:       /* ..., val1, val2  ==> ..., val1 % val2        */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arithmetic_check(cd, iptr, s2);

			M_DIV(s1, s2,  REG_ITMP3);	
			/* we need to test if divident was 0x8000000000000, bit OV is set in XER in this case */
			/* we only need to check this if we did a LDIV, not for IDIV */
			M_MFXER(REG_ITMP2);
			M_ANDIS(REG_ITMP2, 0x4000, REG_ITMP2);	/* test OV */
			M_BLE(2); 
			LCONST(REG_ITMP3, 0);			/* result == 0 in this case */
			M_BR(2);
			M_MUL(REG_ITMP3, s2, REG_ITMP2);
			M_SUB(s1, REG_ITMP2,  REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);

			M_MOV(REG_ITMP3, d);
			emit_store_dst(jd, iptr, d);
			break;

		
		case ICMD_IMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_MUL(s1, s2, d);
			M_EXTSW(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LMUL:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_MUL(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMULCONST:  /* ..., value  ==> ..., value * constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= -32768) && (iptr->sx.val.i <= 32767))
				M_MUL_IMM(s1, iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP3, iptr->sx.val.i);
				M_MUL(s1, REG_ITMP3, d);
			}
			M_EXTSW(d, d);
			emit_store_dst(jd, iptr, d);
			break;
		case ICMD_LMULCONST:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= -32767) && (iptr->sx.val.l <= 32767))
				M_MUL_IMM(s1, iptr->sx.val.l, d);
			else {
				LCONST(REG_ITMP3, iptr->sx.val.l);
				M_MUL(s1, REG_ITMP3, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIVPOW2:   /* ..., value  ==> ..., value << constant       */
		      
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_SRA_IMM(s1, iptr->sx.val.i, d);
			M_ADDZE(d, d);
			M_EXTSW(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND_IMM(s2, 0x1f, REG_ITMP3);
			M_SLL(s1, REG_ITMP3, d);
			M_EXTSW(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHLCONST:  /* ..., value  ==> ..., value << constant       */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SLL_IMM(s1, iptr->sx.val.i & 0x1f, d);
			M_EXTSW(d,d);
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
			M_MOV(s1, REG_ITMP1);
			M_CLR_HIGH(REG_ITMP1);
			M_SRL(REG_ITMP1, REG_ITMP2, d);
			M_EXTSW(d,d);	/* for the case it was shift 0 bits */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
		                      /* sx.val.i = constant                             */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.i & 0x1f) {
				M_MOV(s1, REG_ITMP1);
				M_CLR_HIGH(REG_ITMP1);
				M_SRA_IMM(REG_ITMP1, iptr->sx.val.i & 0x1f, d);
			} else {
				M_INTMOVE(s1, d);
			}
			emit_store_dst(jd, iptr, d);
			break;
	
		case ICMD_LSHLCONST:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SLL_IMM(s1, iptr->sx.val.i & 0x3f, d);
			emit_store_dst(jd, iptr, d);
			break;
		case ICMD_LSHL:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND_IMM(s2, 0x3f, REG_ITMP2);
			M_SLL(s1, REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;
		case ICMD_LSHRCONST:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SRA_IMM(s1, iptr->sx.val.i & 0x3f, d);
			emit_store_dst(jd, iptr, d);
			break;
		case ICMD_LSHR:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND_IMM(s2, 0x3f, REG_ITMP2);
			M_SRA(s1, REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;
		case ICMD_LUSHRCONST:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_SRL_IMM(s1, iptr->sx.val.i & 0x3f, d);
			emit_store_dst(jd, iptr, d);
			break;
		case ICMD_LUSHR:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_AND_IMM(s2, 0x3f, REG_ITMP2);
			M_SRL(s1, REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IAND:       /* ..., val1, val2  ==> ..., val1 & val2        */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_AND(s1, s2, d);
/*			M_EXTSW(d, d);*/
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LAND:
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_AND(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 65535)) {
				M_AND_IMM(s1, iptr->sx.val.i, d);
				}
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

		case ICMD_LANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 65535))
				M_AND_IMM(s1, iptr->sx.val.l, d);
			/*
			else if (iptr->sx.val.l == 0xffffff) {
				M_RLWINM(s1, 0, 8, 31, d);
				}
			*/
			else {
				LCONST(REG_ITMP3, iptr->sx.val.l);
				M_AND(s1, REG_ITMP3, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* sx.val.i = constant                             */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
#if 0
			/* fast division, result in REG_ITMP3) */
			M_SRA_IMM(s1, iptr->sx.val.i, REG_ITMP3);
			M_ADDZE(REG_ITMP3, REG_ITMP3);

			M_SUB(s1, REG_ITMP3, d);
			M_EXTSW(d, d);
			emit_store_dst(jd, iptr, d);
			break;
#else
			
			M_MOV(s1, REG_ITMP2);
			M_CMPI(s1, 0);
			M_BGE(1 + 3*(iptr->sx.val.i >= 32768));
			if (iptr->sx.val.i >= 32768) {
				M_ADDIS(REG_ZERO, iptr->sx.val.i >> 16, REG_ITMP2);
				M_EXTSW(REG_ITMP2, REG_ITMP2);
				M_OR_IMM(REG_ITMP2, iptr->sx.val.i, REG_ITMP2);
				M_IADD(s1, REG_ITMP2, REG_ITMP2);
			} else {
				M_IADD_IMM(s1, iptr->sx.val.i, REG_ITMP2);
			}
			{
				int b=0, m = iptr->sx.val.i;
				while (m >>= 1)
					++b;
				M_RLWINM(REG_ITMP2, 0, 0, 30-b, REG_ITMP2);
			}
			M_SUB(s1, REG_ITMP2, d);
			M_EXTSW(d, d);
			emit_store_dst(jd, iptr, d);
			break;
#endif

		case ICMD_IOR:        /* ..., val1, val2  ==> ..., val1 | val2        */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			M_OR(s1, s2, d);
/*			M_EXTSW(d,d);*/
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LOR:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
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

		case ICMD_LORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 65535))
				M_OR_IMM(s1, iptr->sx.val.l, d);
			else {
				LCONST(REG_ITMP3, iptr->sx.val.l);
				M_OR(s1, REG_ITMP3, d);
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
			if ((iptr->sx.val.i >= 0) && (iptr->sx.val.i <= 65535))
				M_XOR_IMM(s1, iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP3, iptr->sx.val.i);
				M_XOR(s1, REG_ITMP3, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXORCONST:  /* ..., value  ==> ..., value ^ constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if ((iptr->sx.val.l >= 0) && (iptr->sx.val.l <= 65535))
				M_XOR_IMM(s1, iptr->sx.val.l, d);
			else {
				LCONST(REG_ITMP3, iptr->sx.val.l);
				M_XOR(s1, REG_ITMP3, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LCMP:       /* ..., val1, val2  ==> ..., val1 cmp val2      */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			/* XXX implement me!!! */
			vm_abort("codegen: implement ICMD_LCMP!");
			emit_store_dst(jd, iptr, d);
			break;
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
			emit_nullpointer_check(cd, iptr, s1);
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
			M_LWAX(d, s1, REG_ITMP2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, PACK_REGS(REG_ITMP2, REG_ITMP1));
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_SLL_IMM(s2, 3, REG_ITMP2);
			M_IADD(s1, REG_ITMP2, REG_ITMP2);
			/* implicit null-pointer check */
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
			M_SLL_IMM(s2, 3, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_objectarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_ALDX(d, s1, REG_ITMP2);
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
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_SLL_IMM(s2, 3, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_longarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_LSTX(s3, s1, REG_ITMP2);
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

			disp = dseg_add_functionptr(cd, BUILTIN_canstore);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_ALD(REG_ITMP3, REG_ITMP3, 0); /* TOC */
			M_MTCTR(REG_ITMP3);

			M_INTMOVE(s1, REG_A0);
			M_INTMOVE(s3, REG_A1);

			M_JSR;
			emit_exception_check(cd, iptr);

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_SLL_IMM(s2, 3, REG_ITMP2);
			M_IADD_IMM(REG_ITMP2, OFFSET(java_objectarray, data[0]), REG_ITMP2);
			/* implicit null-pointer check */
			M_ASTX(s3, s1, REG_ITMP2);
			break;


		case ICMD_GETSTATIC:  /* ...  ==> ..., value                          */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf = iptr->sx.s23.s3.uf;

				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp = dseg_add_unique_address(cd, NULL);

				codegen_addpatchref(cd, PATCHER_get_putstatic,
									iptr->sx.s23.s3.uf, disp);

			} else {
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;

				fieldtype = fi->type;
				disp = dseg_add_address(cd, &(fi->value));

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class)) {
					codegen_addpatchref(cd, PATCHER_clinit, fi->class, disp);
				}
  			}

			M_ALD(REG_ITMP1, REG_PV, disp);
			switch (fieldtype) {
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD_INTERN(d, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_LLD(d, REG_ITMP1, 0);
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
				uf = iptr->sx.s23.s3.uf;

				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp = dseg_add_unique_address(cd, NULL);

				codegen_addpatchref(cd, PATCHER_get_putstatic,
									iptr->sx.s23.s3.uf, disp);
			} else {
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;

				fieldtype = fi->type;
				disp = dseg_add_address(cd, &(fi->value));

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class)) {
					codegen_addpatchref(cd, PATCHER_clinit, fi->class, disp);
				}
  			}

			M_ALD(REG_ITMP1, REG_PV, disp);
			switch (fieldtype) {
			case TYPE_INT:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_IST_INTERN(s1, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
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
				uf = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp = 0;

				codegen_addpatchref(cd, PATCHER_get_putfield, uf, 0);
			} else {
				fi = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp = fi->offset;
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
				s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			}
			else
				s2 = emit_load_s2(jd, iptr, REG_FTMP2);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				codegen_addpatchref(cd, PATCHER_get_putfield, uf, 0);
			}


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


		/* branch operations **************************************************/

		case ICMD_ATHROW:       /* ..., objectref ==> ... (, objectref)       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_LNGMOVE(s1, REG_ITMP1_XPTR);

#ifdef ENABLE_VERIFIER
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				codegen_addpatchref(cd, PATCHER_athrow_areturn,
									iptr->sx.s23.s2.uc, 0);
			}
#endif /* ENABLE_VERIFIER */

			disp = dseg_add_functionptr(cd, asm_handle_exception);
			M_ALD(REG_ITMP2, REG_PV, disp);
			M_MTCTR(REG_ITMP2);

			if (jd->isleafmethod) M_MFLR(REG_ITMP3);         /* save LR        */
			M_BL(0);                                        /* get current PC */
			M_MFLR(REG_ITMP2_XPC);
			if (jd->isleafmethod) M_MTLR(REG_ITMP3);         /* restore LR     */
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

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_TST(s1);
			emit_beq(cd, iptr->dst.block);
			break;

		case ICMD_IFNONNULL:    /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_TST(s1);
			emit_bne(cd, iptr->dst.block);
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
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			LCONST(REG_ITMP2, iptr->sx.val.l);
			M_CMP(s1, REG_ITMP2);
			emit_beq(cd, iptr->dst.block);
			break;
		case ICMD_IF_LLT:       /* ..., value ==> ...                         */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			LCONST(REG_ITMP2, iptr->sx.val.l);
			M_CMP(s1, REG_ITMP2);
			emit_blt(cd, iptr->dst.block);
			break;
		case ICMD_IF_LLE:       /* ..., value ==> ...                         */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			LCONST(REG_ITMP2, iptr->sx.val.l);
			M_CMP(s1, REG_ITMP2);
			emit_ble(cd, iptr->dst.block);
			break;

		case ICMD_IF_LNE:       /* ..., value ==> ... */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			LCONST(REG_ITMP2, iptr->sx.val.l);
			M_CMP(s1, REG_ITMP2);
			emit_bne(cd, iptr->dst.block);
			break;
		case ICMD_IF_LGE:       /* ..., value ==> ... */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			LCONST(REG_ITMP2, iptr->sx.val.l);
			M_CMP(s1, REG_ITMP2);
			emit_bge(cd, iptr->dst.block);
			break;
		case ICMD_IF_LGT:       /* ..., value ==> ...                         */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			LCONST(REG_ITMP2, iptr->sx.val.l);
			M_CMP(s1, REG_ITMP2);
			emit_bgt(cd, iptr->dst.block);
			break;
		case ICMD_IF_ICMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ACMPEQ:    /* op1 = target JavaVM pc                     */
		case ICMD_IF_LCMPEQ: 

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_beq(cd, iptr->dst.block);
			break;

		case ICMD_IF_ICMPNE:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ACMPNE:    /* op1 = target JavaVM pc                     */
		case ICMD_IF_LCMPNE:  

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bne(cd, iptr->dst.block);
			break;


		case ICMD_IF_ICMPLT:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPLT:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_blt(cd, iptr->dst.block);
			break;

		case ICMD_IF_ICMPGT:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPGT:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bgt(cd, iptr->dst.block);
			break;

		case ICMD_IF_ICMPLE:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPLE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_ble(cd, iptr->dst.block);
			break;

		case ICMD_IF_ICMPGE:    /* ..., value, value ==> ...                  */
		case ICMD_IF_LCMPGE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bge(cd, iptr->dst.block);
			break;


		case ICMD_LRETURN:      /* ..., retvalue ==> ...                      */
		case ICMD_IRETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT);
			M_LNGMOVE(s1, REG_RESULT);
			goto nowperformreturn;

		case ICMD_ARETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT);
			M_LNGMOVE(s1, REG_RESULT);

#ifdef ENABLE_VERIFIER
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				codegen_addpatchref(cd, PATCHER_athrow_areturn,
									iptr->sx.s23.s2.uc, 0);
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

			/* call trace function */

#if !defined(NDEBUG)
			if (JITDATA_HAS_FLAG_VERBOSECALL(jd)) {
				emit_verbosecall_exit(jd);
			}
#endif		

#if defined(ENABLE_THREADS)
			if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
				disp = dseg_add_functionptr(cd, LOCK_monitor_exit);
				M_ALD(REG_ITMP3, REG_PV, disp);
				M_ALD(REG_ITMP3, REG_ITMP3, 0); /* TOC */
				M_MTCTR(REG_ITMP3);

				/* we need to save the proper return value */

				switch (iptr->opc) {
				case ICMD_LRETURN:
				case ICMD_IRETURN:
				case ICMD_ARETURN:
					/* fall through */
					M_LST(REG_RESULT , REG_SP, rd->memuse * 8 + 8);
					break;
				case ICMD_FRETURN:
					M_FST(REG_FRESULT, REG_SP, rd->memuse * 8 + 8);
					break;
				case ICMD_DRETURN:
					M_DST(REG_FRESULT, REG_SP, rd->memuse * 8 + 8);
					break;
				}

				M_ALD(REG_A0, REG_SP, rd->memuse * 8);
				M_JSR;

				/* and now restore the proper return value */

				switch (iptr->opc) {
				case ICMD_LRETURN:
				case ICMD_IRETURN:
				case ICMD_ARETURN:
					/* fall through */
					M_LLD(REG_RESULT , REG_SP, rd->memuse * 8 + 8);
					break;
				case ICMD_FRETURN:
					M_FLD(REG_FRESULT, REG_SP, rd->memuse * 8 + 8);
					break;
				case ICMD_DRETURN:
					M_DLD(REG_FRESULT, REG_SP, rd->memuse * 8 + 8);
					break;
				}
			}
#endif

			/* restore return address                                         */

			if (!jd->isleafmethod) {
				/* ATTENTION: Don't use REG_ZERO (r0) here, as M_ALD
				   may have a displacement overflow. */

				M_ALD(REG_ITMP1, REG_SP, p * 8 + LA_LR_OFFSET);
				M_MTLR(REG_ITMP1);
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
			if (l == 0) {
				M_INTMOVE(s1, REG_ITMP1);
			} else if (l <= 32768) {
				M_LDA(REG_ITMP1, s1, -l);
			} else {
				ICONST(REG_ITMP2, l);
				M_SUB(s1, REG_ITMP2, REG_ITMP1);
			}

			/* number of targets */
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

			/* length of dataseg after last dseg_add_unique_target is used by load */

			M_SLL_IMM(REG_ITMP1, 3, REG_ITMP1);
			M_IADD(REG_ITMP1, REG_PV, REG_ITMP2);
			M_ALD(REG_ITMP2, REG_ITMP2, -(cd->dseglen));
			M_MTCTR(REG_ITMP2);
			M_RTS;
			ALIGNCODENOP;
			}
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
				if ((val >= -32768) && (val <= 32767)) {
					M_CMPI(s1, val);
			
				} else {
					a = dseg_add_s4(cd, val);
					M_ILD(REG_ITMP2, REG_PV, a);
					M_CMP(s1, REG_ITMP2);
				}
				emit_beq(cd, lookup->target.block);
				++lookup;
			}

			emit_br(cd, iptr->sx.s23.s3.lookupdefault.block);

			ALIGNCODENOP;
			break;
			}


		case ICMD_BUILTIN:      /* ..., [arg1, [arg2 ...]] ==> ...            */

			bte = iptr->sx.s23.s3.bte;
			md = bte->md;
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

				if (var->flags & PREALLOC)
					continue;

				if (IS_INT_LNG_TYPE(var->type)) {
					if (!md->params[s3].inmemory) {
						s1 = emit_load(jd, iptr, var, d);
						M_LNGMOVE(s1, d);
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
				M_ALD(REG_PV, REG_PV, disp);
				M_ALD(REG_PV, REG_PV, 0);	/* TOC */

				/* generate the actual call */
				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
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
				/* fall through */

			case ICMD_INVOKESTATIC:
				if (lm == NULL) {
					disp = dseg_add_unique_address(cd, um);

					codegen_addpatchref(cd, PATCHER_invokestatic_special,
										um, disp);
				} else {
					disp = dseg_add_address(cd, lm->stubroutine);
				}
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
				} else {
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

				} else {
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
					s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT);
					M_MOV(REG_RESULT, s1);
				}
				else {
					s1 = codegen_reg_of_dst(jd, iptr, REG_FRESULT);
					M_FLTMOVE(REG_FRESULT, s1);
				}
				emit_store_dst(jd, iptr, s1);
			}
			break;

		case ICMD_CHECKCAST:  /* ..., objectref ==> ..., objectref            */
		                      /* val.a: (classinfo*) superclass               */

			/*  superclass is an interface:
			 *
			 *  OK if ((sub == NULL) ||
			 *         (sub->vftbl->interfacetablelength > super->index) &&
			 *         (sub->vftbl->interfacetable[-super->index] != NULL));
			 *
			 *  superclass is a class:
			 *
			 *  OK if ((sub == NULL) || (0
			 *         <= (sub->vftbl->baseval - super->vftbl->baseval) <=
			 *         super->vftbl->diffvall));
			 */

			if (!(iptr->flags.bits & INS_FLAG_ARRAY)) {
				/* object type cast-check */

				classinfo *super;
				s4         superindex;

				if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
					super = NULL;
					superindex = 0;
				} else {
					super = iptr->sx.s23.s3.c.cls;
					superindex = super->index;
				}
			
#if defined(ENABLE_THREADS)
				codegen_threadcritrestart(cd, cd->mcodeptr - cd->mcodebase);
#endif
				s1 = emit_load_s1(jd, iptr, REG_ITMP1);

				/* calculate interface checkcast code size */

				s2 = 9;
#if defined(SOFTEX)
				s2 += CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd) ? 2 : 0;
#endif
				if (super == NULL)
					s2 += (opt_shownops ? 1 : 0);

				/* calculate class checkcast code size */

				s3 = 10 + (s1 == REG_ITMP1);
#if defined(SOFTEX)
				s3 += CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd) ? 1 : 0;
#endif
				if (super == NULL)
					s3 += (opt_shownops ? 1 : 0);

				/* if class is not resolved, check which code to call */

				if (super == NULL) {
					M_TST(s1);
					M_BEQ(3 + (opt_shownops ? 1 : 0) + s2 + 1 + s3);

					disp = dseg_add_unique_s4(cd, 0);                     /* super->flags */

					codegen_addpatchref(cd,
										PATCHER_checkcast_instanceof_flags,
										iptr->sx.s23.s3.c.ref,
										disp);

					M_ILD(REG_ITMP2, REG_PV, disp);
					M_AND_IMM(REG_ITMP2, ACC_INTERFACE, REG_ITMP2);
					M_BEQ(s2 + 1);
				}

				/* interface checkcast code */

				if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
					if (super == NULL) {
						codegen_addpatchref(cd,
											PATCHER_checkcast_interface,
											iptr->sx.s23.s3.c.ref,
											0);
					} else {
						M_TST(s1);
						M_BEQ(s2);
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

					if (!super)
						M_BR(s3);
				}

				/* class checkcast code */

				if ((super == NULL) || !(super->flags & ACC_INTERFACE)) {
					if (super == NULL) {
						disp = dseg_add_unique_address(cd, NULL);
						codegen_addpatchref(cd, PATCHER_checkcast_class,
											iptr->sx.s23.s3.c.ref,
											disp);
					} else {
						disp = dseg_add_address(cd, super->vftbl);
						M_TST(s1);
						M_BEQ(s3);
					}

					M_ALD(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));
#if defined(ENABLE_THREADS)
					codegen_threadcritstart(cd, cd->mcodeptr - cd->mcodebase);
#endif
					M_ILD(REG_ITMP3, REG_ITMP2, OFFSET(vftbl_t, baseval));
					M_ALD(REG_ITMP2, REG_PV, disp);
					if (s1 != REG_ITMP1) {
						M_ILD(REG_ITMP1, REG_ITMP2, OFFSET(vftbl_t, baseval));
						M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, diffval));
#if defined(ENABLE_THREADS)
						codegen_threadcritstop(cd, cd->mcodeptr - cd->mcodebase);
#endif
						M_SUB(REG_ITMP3, REG_ITMP1, REG_ITMP3);
						M_EXTSW(REG_ITMP3, REG_ITMP3);
					} else {
						M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, baseval));
						M_SUB(REG_ITMP3, REG_ITMP2, REG_ITMP3);
						M_EXTSW(REG_ITMP3, REG_ITMP3);
						M_ALD(REG_ITMP2, REG_PV, disp);
						M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, diffval));
#if defined(ENABLE_THREADS)
						codegen_threadcritstop(cd, cd->mcodeptr - cd->mcodebase);
#endif
					}
					M_CMPU(REG_ITMP3, REG_ITMP2);
					emit_classcast_check(cd, iptr, BRANCH_GT, REG_ITMP3, s1);
				}
				d = codegen_reg_of_dst(jd, iptr, s1);

			} else {
				/* array type cast-check */

				s1 = emit_load_s1(jd, iptr, REG_A0);
				M_INTMOVE(s1, REG_A0);


				if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
					disp = dseg_add_unique_address(cd, NULL);
					codegen_addpatchref(cd, PATCHER_builtin_arraycheckcast,
										iptr->sx.s23.s3.c.ref,
										disp);
				} else {
					disp = dseg_add_address(cd, iptr->sx.s23.s3.c.cls);
				}

				M_ALD(REG_A1, REG_PV, disp);
				disp = dseg_add_functionptr(cd, BUILTIN_arraycheckcast);
				M_ALD(REG_ITMP2, REG_PV, disp);
				M_ALD(REG_ITMP2, REG_ITMP2, 0);	/* TOC */
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
		                      /* val.a: (classinfo*) superclass               */

			/*  superclass is an interface:
			 *
			 *  return (sub != NULL) &&
			 *         (sub->vftbl->interfacetablelength > super->index) &&
			 *         (sub->vftbl->interfacetable[-super->index] != NULL);
			 *
			 *  superclass is a class:
			 *
			 *  return ((sub != NULL) && (0
			 *          <= (sub->vftbl->baseval - super->vftbl->baseval) <=
			 *          super->vftbl->diffvall));
			 */

			{
			classinfo *super;
			vftbl_t   *supervftbl;
			s4         superindex;

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				super = NULL;
				superindex = 0;
				supervftbl = NULL;
			}
			else {
				super = iptr->sx.s23.s3.c.cls;
				superindex = super->index;
				supervftbl = super->vftbl;
			}
			
#if defined(ENABLE_THREADS)
            codegen_threadcritrestart(cd, cd->mcodeptr - cd->mcodebase);
#endif
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s1 == d) {
				M_MOV(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			}

			/* calculate interface instanceof code size */

			s2 = 8;
			if (!super)
				s2 += (opt_shownops ? 1 : 0);

			/* calculate class instanceof code size */

			s3 = 11;
			if (super == NULL)
				s3 += (opt_shownops ? 1 : 0);

			M_CLR(d);

			/* if class is not resolved, check which code to call */

			if (super == NULL) {
				M_TST(s1);
				M_BEQ(3 + (opt_shownops ? 1 : 0) + s2 + 1 + s3);

				disp = dseg_add_unique_s4(cd, 0);                     /* super->flags */

				codegen_addpatchref(cd, PATCHER_checkcast_instanceof_flags,
									iptr->sx.s23.s3.c.ref, disp);

				M_ILD(REG_ITMP3, REG_PV, disp);
				M_AND_IMM(REG_ITMP3, ACC_INTERFACE, REG_ITMP3);
				M_BEQ(s2 + 1);
			}

			/* interface instanceof code */

			if (!super || (super->flags & ACC_INTERFACE)) {
				if (super) {
					M_TST(s1);
					M_BEQ(s2);

				} else {
					codegen_addpatchref(cd,
										PATCHER_instanceof_interface,
										iptr->sx.s23.s3.c.ref, 0);
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
					M_BR(s3);
			}

			/* class instanceof code */

			if ((super == NULL) || !(super->flags & ACC_INTERFACE)) {

				if (super) {
					disp = dseg_add_address(cd, supervftbl);
					M_TST(s1);
					M_BEQ(s3);

				} else {
					disp = dseg_add_unique_address(cd, NULL);
					codegen_addpatchref(cd, PATCHER_instanceof_class,
										iptr->sx.s23.s3.c.ref,
										disp);
				}

				M_ALD(REG_ITMP1, s1, OFFSET(java_objectheader, vftbl));
				M_ALD(REG_ITMP2, REG_PV, disp);
#if defined(ENABLE_THREADS)
				codegen_threadcritstart(cd, cd->mcodeptr - cd->mcodebase);
#endif
				M_ILD(REG_ITMP1, REG_ITMP1, OFFSET(vftbl_t, baseval));
				M_ILD(REG_ITMP3, REG_ITMP2, OFFSET(vftbl_t, baseval));
				M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, diffval));
#if defined(ENABLE_THREADS)
				codegen_threadcritstop(cd, cd->mcodeptr - cd->mcodebase);
#endif
				M_SUB(REG_ITMP1, REG_ITMP3, REG_ITMP1);
				M_EXTSW(REG_ITMP1, REG_ITMP1);
				M_CMPU(REG_ITMP1, REG_ITMP2);
				M_CLR(d);
				M_BGT(1);
				M_IADD_IMM(REG_ZERO, 1, d);
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
#if defined(__DARWIN__)
					M_LST(s2, REG_SP, LA_SIZE + (s1 + INT_ARG_CNT) * 8);
#else
					M_LST(s2, REG_SP, LA_SIZE + (s1 + 3) * 8);
#endif
				}
			}

			/* a0 = dimension count */

			ICONST(REG_A0, iptr->s1.argcount);

			/* is patcher function set? */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				disp = dseg_add_unique_address(cd, NULL);

				codegen_addpatchref(cd, PATCHER_builtin_multianewarray,
									iptr->sx.s23.s3.c.ref, disp);
			} else {
				disp = dseg_add_address(cd, iptr->sx.s23.s3.c.cls);
			}

			/* a1 = arraydescriptor */

			M_ALD(REG_A1, REG_PV, disp);

			/* a2 = pointer to dimensions = stack pointer */

#if defined(__DARWIN__)
			M_LDA(REG_A2, REG_SP, LA_SIZE + INT_ARG_CNT * 8);
#else
			M_LDA(REG_A2, REG_SP, LA_SIZE + 3 * 8);
#endif

			disp = dseg_add_functionptr(cd, BUILTIN_multianewarray);
			M_ALD(REG_ITMP3, REG_PV, disp);
			M_ALD(REG_ITMP3, REG_ITMP3, 0); /* TOC */
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
	s4           i, j;
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
		4 +                            /* 4 stackframeinfo arguments (darwin)*/
		nmd->paramcount  + 
		nmd->memuse;

/*	cd->stackframesize = (cd->stackframesize + 3) & ~3;*/ /* keep stack 16-byte aligned */

	/* create method header */

	(void) dseg_add_unique_address(cd, code);                      /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 8);             /* FrameSize       */
	(void) dseg_add_unique_s4(cd, 0);                              /* IsSync          */
	(void) dseg_add_unique_s4(cd, 0);                              /* IsLeaf          */
	(void) dseg_add_unique_s4(cd, 0);                              /* IntSave         */
	(void) dseg_add_unique_s4(cd, 0);                              /* FltSave         */
	(void) dseg_addlinenumbertablesize(cd);
	(void) dseg_add_unique_s4(cd, 0);                              /* ExTableSize     */

	/* generate code */

	M_MFLR(REG_ZERO);
	M_AST_INTERN(REG_ZERO, REG_SP, LA_LR_OFFSET);
	M_STDU(REG_SP, REG_SP, -(cd->stackframesize * 8));

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd)) {
		emit_verbosecall_enter(jd);
	}
#endif
	/* get function address (this must happen before the stackframeinfo) */

	funcdisp = dseg_add_functionptr(cd, f);

#if !defined(WITH_STATIC_CLASSPATH)
	if (f == NULL) {
		codegen_addpatchref(cd, PATCHER_resolve_native, m, funcdisp);
	}
#endif

	/* save integer and float argument registers */

	j = 0;

	for (i = 0; i < md->paramcount; i++) {
		t = md->paramtypes[i].type;

		if (IS_INT_LNG_TYPE(t)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;
				M_LST(s1, REG_SP, LA_SIZE + PA_SIZE + 4*8 + j * 8);
				j++;
			}
		}
	}

	for (i = 0; i < md->paramcount; i++) {
		if (IS_FLT_DBL_TYPE(md->paramtypes[i].type)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;
				M_DST(s1, REG_SP, LA_SIZE + PA_SIZE + 4*8 + j * 8);
				j++;
			}
		}
	}

	/* create native stack info */

	M_AADD_IMM(REG_SP, cd->stackframesize * 8, REG_A0);
	M_MOV(REG_PV, REG_A1);
	M_AADD_IMM(REG_SP, cd->stackframesize * 8, REG_A2);
	M_ALD(REG_A3, REG_SP, cd->stackframesize * 8 + LA_LR_OFFSET);
	disp = dseg_add_functionptr(cd, codegen_start_native_call);

	M_ALD(REG_ITMP1, REG_PV, disp);
	M_ALD(REG_ITMP1, REG_ITMP1, 0);		/* TOC */
	M_MTCTR(REG_ITMP1);
	M_JSR;

	/* restore integer and float argument registers */

	j = 0;

	for (i = 0; i < md->paramcount; i++) {
		t = md->paramtypes[i].type;

		if (IS_INT_LNG_TYPE(t)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;
				M_LLD(s1, REG_SP, LA_SIZE + PA_SIZE + 4 * 8 + j * 8);
				j++;
			}
		}
	}

	for (i = 0; i < md->paramcount; i++) {
		if (IS_FLT_DBL_TYPE(md->paramtypes[i].type)) {
			if (!md->params[i].inmemory) {
				s1 = md->params[i].regoff;
				M_DLD(s1, REG_SP, LA_SIZE + PA_SIZE + 4 * 8 + j * 8);
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
			/* We only copy spilled float arguments, as the float
			   argument registers keep unchanged. */

			if (md->params[i].inmemory) {
				s1 = md->params[i].regoff + cd->stackframesize;
				s2 = nmd->params[j].regoff;

				M_DLD(REG_FTMP1, REG_SP, s1 * 8);

				if (IS_2_WORD_TYPE(t))
					M_DST(REG_FTMP1, REG_SP, s2 * 8);
				else
					M_FST(REG_FTMP1, REG_SP, s2 * 8 + 4);
			}
		}
	}

	/* put class into second argument register */

	if (m->flags & ACC_STATIC) {
		disp = dseg_add_unique_address(cd, m->class);
		M_ALD(REG_A1, REG_PV, disp);
	}

	/* put env into first argument register */

	disp = dseg_add_unique_address(cd, _Jv_env);
	M_ALD(REG_A0, REG_PV, disp);

	/* generate the actual native call */
	/* native functions have a different TOC for sure */

	M_AST(REG_TOC, REG_SP, 40);	/* save old TOC */
	M_ALD(REG_ITMP3, REG_PV, funcdisp);
	M_ALD(REG_TOC, REG_ITMP3, 8);	/* load TOC from func. descriptor */
	M_ALD(REG_ITMP3, REG_ITMP3, 0);		
	M_MTCTR(REG_ITMP3);
	M_JSR;
	M_ALD(REG_TOC, REG_SP, 40);	/* restore TOC */

	/* save return value */

	if (md->returntype.type != TYPE_VOID) {
		if (IS_INT_LNG_TYPE(md->returntype.type)) {
			M_LST(REG_RESULT, REG_SP, LA_SIZE + PA_SIZE + 1 * 8);
		}
		else {
			M_DST(REG_FRESULT, REG_SP, LA_SIZE + PA_SIZE + 1 * 8);
		}
	}

	/* print call trace */
#if ! defined(NDEBGUU)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd)) {
		emit_verbosecall_exit(jd);
	}
#endif
	/* remove native stackframe info */

	M_AADD_IMM(REG_SP, cd->stackframesize * 8, REG_A0);
	disp = dseg_add_functionptr(cd, codegen_finish_native_call);
	M_ALD(REG_ITMP1, REG_PV, disp);
	M_ALD(REG_ITMP1, REG_ITMP1, 0);	/* XXX what about TOC? */
	M_MTCTR(REG_ITMP1);
	M_JSR;
	M_MOV(REG_RESULT, REG_ITMP1_XPTR);

	/* restore return value */

	if (md->returntype.type != TYPE_VOID) {
		if (IS_INT_LNG_TYPE(md->returntype.type)) {
			M_LLD(REG_RESULT, REG_SP, LA_SIZE + PA_SIZE + 1 * 8);
		}
		else {
/*			if (IS_2_WORD_TYPE(md->returntype.type)) */
				M_DLD(REG_FRESULT, REG_SP, LA_SIZE + PA_SIZE + 1 * 8);
/*			else
				M_FLD(REG_FRESULT, REG_SP, LA_SIZE + PA_SIZE + 1 * 8); F XXX
				*/
		}
	}

	M_ALD(REG_ITMP2_XPC, REG_SP, cd->stackframesize * 8 + LA_LR_OFFSET);
	M_MTLR(REG_ITMP2_XPC);
	M_LDA(REG_SP, REG_SP, cd->stackframesize * 8); /* remove stackframe           */

	/* check for exception */

	M_TST(REG_ITMP1_XPTR);
	M_BNE(1);                           /* if no exception then return        */

	M_RET;

	/* handle exception */

	M_LADD_IMM(REG_ITMP2_XPC, -4, REG_ITMP2_XPC);  /* exception address       */

	disp = dseg_add_functionptr(cd, asm_handle_nat_exception);
	M_ALD(REG_ITMP3, REG_PV, disp);
	M_MTCTR(REG_ITMP3);
	M_RTS;

	/* generate patcher stub call code */

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

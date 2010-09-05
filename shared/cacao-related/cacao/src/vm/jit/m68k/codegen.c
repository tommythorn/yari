/* src/vm/jit/m68k/codegen.c

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

   $Id: codegen.c 7564 2007-03-23 23:36:17Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "md-abi.h"
#include "md-os.h"

#include "vm/types.h"
#include "vm/jit/m68k/codegen.h"
#include "vm/jit/m68k/emit.h"

#include "mm/memory.h"
#include "native/jni.h"
#include "native/native.h"

#include "threads/lock-common.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

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
#include "vm/jit/md.h"

#include "vmcore/loader.h"
#include "vmcore/options.h"
#include "vmcore/utf8.h"


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
		/* save calle saved registers */
		s4 savedregs_num = 0;

		savedregs_num += (INT_SAV_CNT - rd->savintreguse);
		savedregs_num += (ADR_SAV_CNT - rd->savadrreguse);
		savedregs_num += (FLT_SAV_CNT - rd->savfltreguse) * 2;

		cd->stackframesize = rd->memuse + savedregs_num;

		/* FIXME: we could need 2 words to move a double result, which gets
		 * passed in %d0, %d1 into a floating point register, this is of 
		 * course onyl true when not using ENABLE_SOFTFLOAT, so this could be
		 * optimized away, for now always use 2 more words. When optimizing watch
		 * the threading code, which stores the lock word, the offset would change */
		cd->stackframesize += 2;

#if defined(ENABLE_THREADS)
		/* we need additional space to save argument of monitor_enter */
		if (checksync && (m->flags & ACC_SYNCHRONIZED))	{
			if (IS_2_WORD_TYPE(m->parseddesc->returntype.type))	{
				cd->stackframesize += 2;
			} else	{
				cd->stackframesize ++;
			}
		}
#endif
		
	
		/* create method header */
		(void) dseg_add_unique_address(cd, code);              /* CodeinfoPointer */
		(void) dseg_add_unique_s4(cd, cd->stackframesize * 4); /* FrameSize       */
#if defined(ENABLE_THREADS)
		if (checksync && (m->flags & ACC_SYNCHRONIZED))
			(void) dseg_add_unique_s4(cd, (rd->memuse + 1) * 4);/* IsSync         */
		else
#endif
		(void) dseg_add_unique_s4(cd, 0);                      /* IsSync          */
		(void) dseg_add_unique_s4(cd, jd->isleafmethod);       /* IsLeaf          */

		/* XXX we use the IntSAce a split field for the adr now */
		(void) dseg_add_unique_s4(cd, (ADR_SAV_CNT - rd->savadrreguse) << 16 | (INT_SAV_CNT - rd->savintreguse)); /* IntSave */
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
		assert(0);
#endif

#if !defined(NDEBUG)
		emit_verbosecall_enter(jd);
#endif
		/* create stack frame */
		M_AADD_IMM(-(cd->stackframesize*4), REG_SP);

		/* save used callee saved registers */
		p = cd->stackframesize;
		for (i=INT_SAV_CNT-1; i>=rd->savintreguse; --i)	{
			p--; M_IST(rd->savintregs[i], REG_SP, p*4);
		}
		for (i=ADR_SAV_CNT-1; i>=rd->savadrreguse; --i)	{
			p--; M_AST(rd->savadrregs[i], REG_SP, p*4);
		}
#if !defined(ENABLE_SOFTFLOAT)
		for (i=FLT_SAV_CNT-1; i>=rd->savfltreguse; --i)	{
			p-=2; M_DST(rd->savfltregs[i], REG_SP, p*4);
		}	
#else
		assert(FLT_SAV_CNT == 0);
		assert(rd->savfltreguse == 0);
#endif
		/* take arguments out of stack frame */
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
			assert(md->params[p].inmemory);			/* all args are on stack */

			switch (t)	{
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
			case TYPE_DBL:
#endif
			case TYPE_LNG:
			case TYPE_INT:
				if (!IS_INMEMORY(var->flags)) {      /* stack arg -> register */
					if (IS_2_WORD_TYPE(t))	{
						M_LLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1 + 1) * 4);
					} else {
						M_ILD(var->vv.regoff, REG_SP, (cd->stackframesize + s1 + 1) * 4);
					}
				} else {                             /* stack arg -> spilled  */
#if 1
 					M_ILD(REG_ITMP1, REG_SP, (cd->stackframesize + s1 + 1) * 4);
 					M_IST(REG_ITMP1, REG_SP, var->vv.regoff * 4);
					if (IS_2_WORD_TYPE(t)) {
						M_ILD(REG_ITMP1, REG_SP, (cd->stackframesize + s1 + 1) * 4 + 4);
						M_IST(REG_ITMP1, REG_SP, var->vv.regoff * 4 + 4);
					}
#else
					/* Reuse Memory Position on Caller Stack */
					var->vv.regoff = cd->stackframesize + s1;
#endif
				} 
				break;
#if !defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
			case TYPE_DBL:
 				if (!IS_INMEMORY(var->flags)) {      /* stack-arg -> register */
					if (IS_2_WORD_TYPE(t))	{
						M_DLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1 + 1) * 4);
					} else {
						M_FLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1 + 1) * 4);
					}
 				} else {                             /* stack-arg -> spilled  */
#if 1
					if (IS_2_WORD_TYPE(t)) {
						M_DLD(REG_FTMP1, REG_SP, (cd->stackframesize + s1 + 1) * 4);
						M_DST(REG_FTMP1, REG_SP, var->vv.regoff * 4);
					} else {
						M_FLD(REG_FTMP1, REG_SP, (cd->stackframesize + s1 + 1) * 4);
						M_FST(REG_FTMP1, REG_SP, var->vv.regoff * 4);
					}
#else
					/* Reuse Memory Position on Caller Stack */
					var->vv.regoff = cd->stackframesize + s1;
#endif
				}
				break;
#endif /* SOFTFLOAT */
			case TYPE_ADR:
 				if (!IS_INMEMORY(var->flags)) {      /* stack-arg -> register */
					M_ALD(var->vv.regoff, REG_SP, (cd->stackframesize + s1 + 1) * 4);
 				} else {                             /* stack-arg -> spilled  */
#if 1
					M_ALD(REG_ATMP1, REG_SP, (cd->stackframesize + s1 + 1) * 4);
					M_AST(REG_ATMP1, REG_SP, var->vv.regoff * 4);
#else
				/* Reuse Memory Position on Caller Stack */
				var->vv.regoff = cd->stackframesize + s1;
#endif
				}
				break;
			default: assert(0);
			}
		} /* end for argument out of stack*/

#if defined(ENABLE_THREADS)
	/* call monitor_enter function */
	if (checksync && (m->flags & ACC_SYNCHRONIZED))	{
		if (m->flags & ACC_STATIC)	{
			M_AMOV_IMM((&m->class->object.header), REG_ATMP1);
		} else	{
			/* for non-static case the first arg is the object */
			M_ALD(REG_ATMP1, REG_SP, cd->stackframesize*4 + 4);
			M_ATST(REG_ATMP1);
			M_BNE(2);
			M_TRAP(M68K_EXCEPTION_HARDWARE_NULLPOINTER);
		}

		M_AST(REG_ATMP1, REG_SP, rd->memuse * 4 + 2*4);
		M_AST(REG_ATMP1, REG_SP, 0 * 4);
		M_JSR_IMM(LOCK_monitor_enter);
	}
#endif

	}

	/* create replacement points */
	REPLACEMENT_POINTS_INIT(cd, jd);

	/* foreach basic block */
	for (bptr = jd->basicblocks; bptr != NULL; bptr = bptr->next) {
	
	bptr->mpc = (s4) (cd->mcodeptr - cd->mcodebase);

	if (bptr->flags >= BBREACHED)	{
	
	/* branch resolving */
	codegen_resolve_branchrefs(cd, bptr);

	/* FIXME there are still some constrcuts to copy in here */

	/* walk through all instructions */
	len = bptr->icount;
	currentline = 0;

	for (iptr = bptr->iinstr; len > 0; len--, iptr++) {
		if (iptr->line != currentline) {
			dseg_addlinenumber(cd, iptr->line);
			currentline = iptr->line;
		}

		MCODECHECK(1024);                         /* 1kB should be enough */

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
			assert(VAROP(iptr->s1)->type == TYPE_ADR);
			emit_nullpointer_check(cd, iptr, s1);
			break;


		/* CONST **************************************************************/
		case ICMD_ICONST:     /* ...  ==> ..., constant                       */
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_IMOV_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LCONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			LCONST(iptr->sx.val.l, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCONST:     /* ...  ==> ..., constant                       */

#if defined(ENABLE_SOFTFLOAT)
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_IMOV_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
#else
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			FCONST(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
#endif
			break;

		case ICMD_DCONST:     /* ...  ==> ..., constant                       */

#if defined(ENABLE_SOFTFLOAT)
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			LCONST(iptr->sx.val.l, d);
			emit_store_dst(jd, iptr, d);
#else
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			disp = dseg_add_double(cd, iptr->sx.val.d);
			M_AMOV_IMM(0, REG_ATMP1);
			dseg_adddata(cd);
			M_DLD(d, REG_ATMP1, disp);
			emit_store_dst(jd, iptr, d);
#endif
			break;



		/* integer operations ************************************************/
		case ICMD_INEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1); 
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);
			M_INEG(REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

#if 0
		case ICMD_LNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_SUBFIC(GET_LOW_REG(s1), 0, GET_LOW_REG(d));
			M_SUBFZE(GET_HIGH_REG(s1), GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;
#endif
		case ICMD_I2L:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP3);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_IMOV(s1, GET_LOW_REG(d));				/* sets negativ bit */
			M_BPL(4);
			M_ISET(GET_HIGH_REG(d));
			M_TPFW;
			M_ICLR(GET_HIGH_REG(d));

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
			M_INTMOVE(s2, REG_ITMP2);
			M_IADD(s1, REG_ITMP2);
			M_INTMOVE(REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		                      /* s1.localindex = variable, sx.val.i = constant*/

		case ICMD_IINC:
		case ICMD_IADDCONST:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);
			M_IADD_IMM(iptr->sx.val.i, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);
			M_ISUB(s2, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISUBCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);
			M_IADD_IMM(-iptr->sx.val.i, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			emit_arithmetic_check(cd, iptr, s2);
			M_INTMOVE(s1, REG_ITMP1);
			M_IDIV(s2, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIVPOW2:		/* ..., value  ==> ..., value << constant       */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);

			M_ITST(REG_ITMP1);
			M_BPL(6);
			M_IADD_IMM((1 << iptr->sx.val.i) - 1, REG_ITMP1);

			M_IMOV_IMM(iptr->sx.val.i, REG_ITMP2);
			M_ISSR(REG_ITMP2, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREM:       /* ..., val1, val2  ==> ..., val1 % val2        */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			emit_arithmetic_check(cd, iptr, s2);

			M_ICMP_IMM(0x80000000, s1);
			M_BNE(4+8);
			M_ICMP_IMM(-1, s2);
			M_BNE(4);
			M_ICLR(REG_ITMP3);
			M_TPFL;					/* hides the next instruction */
			M_IREM(s2, s1, REG_ITMP3);

			M_INTMOVE(REG_ITMP3, d);

			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREMPOW2:		/* ..., value  ==> ..., value << constant       */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s1 == d) {
				M_IMOV(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			} 
			M_INTMOVE(s1, d);
			M_IAND_IMM(iptr->sx.val.i, d);
			M_ITST(s1);
			M_BGE(2 + 2 + 6 + 2);
			M_IMOV(s1, d);  /* don't use M_INTMOVE, so we know the jump offset */
			M_INEG(d);
			M_IAND_IMM(iptr->sx.val.i, d);     /* use 32-bit for jump offset */
			M_INEG(d);

			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_LDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
		case ICMD_LREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			bte = iptr->sx.s23.s3.bte;
			md  = bte->md;

			s2 = emit_load_s2(jd, iptr, REG_ITMP12_PACKED);
			M_INTMOVE(GET_LOW_REG(s2), REG_ITMP3);
			M_IOR(GET_HIGH_REG(s2), REG_ITMP3);
			/* XXX could be optimized */
			emit_arithmetic_check(cd, iptr, REG_ITMP3);

			M_LST(s2, REG_SP, 2 * 4);
			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			M_LST(s1, REG_SP, 0 * 4);

			M_JSR_IMM(bte->fp);

			d = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(REG_RESULT_PACKED, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(s2, REG_ITMP2);
			M_IMUL(s1, REG_ITMP2);
			M_INTMOVE(REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMULCONST:  /* ..., value  ==> ..., value * constant        */
		                      /* sx.val.i = constant                          */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_IMOV_IMM(iptr->sx.val.i, REG_ITMP2);
			M_IMUL(s1, REG_ITMP2);
			M_INTMOVE(REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);
			M_INTMOVE(s2, REG_ITMP2);
			M_IAND_IMM(0x1f, REG_ITMP2);
			M_ISSL(REG_ITMP2, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHLCONST:  /* ..., value  ==> ..., value << constant       */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i & 0x1f)	{
				M_INTMOVE(s1, REG_ITMP1)
				if ((iptr->sx.val.i & 0x1f) <= 7)	{
					M_ISSL_IMM(iptr->sx.val.i & 0x1f, REG_ITMP1);
				} else	{
					M_IMOV_IMM(iptr->sx.val.i & 0x1f, REG_ITMP2);
					M_ISSL(REG_ITMP2, REG_ITMP1);
				}
				M_INTMOVE(REG_ITMP1, d);
			} else	{
				M_INTMOVE(s1, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHR:       /* ..., val1, val2  ==> ..., val1 >> val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);
			M_INTMOVE(s2, REG_ITMP2);
			M_IAND_IMM(0x1f, REG_ITMP2);
			M_ISSR(REG_ITMP2, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHRCONST:  /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i & 0x1f)	{
				M_INTMOVE(s1, REG_ITMP1)
				if ((iptr->sx.val.i & 0x1f) <= 7)	{
					M_ISSR_IMM(iptr->sx.val.i & 0x1f, REG_ITMP1);
				} else	{
					M_IMOV_IMM(iptr->sx.val.i & 0x1f, REG_ITMP2);
					M_ISSR(REG_ITMP2, REG_ITMP1);
				}
				M_INTMOVE(REG_ITMP1, d);
			} else	{
				M_INTMOVE(s1, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHR:      /* ..., val1, val2  ==> ..., val1 >>> val2      */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);
			M_INTMOVE(s2, REG_ITMP2);
			M_IAND_IMM(0x1f, REG_ITMP2);
			M_IUSR(REG_ITMP2, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
		                      /* sx.val.i = constant                          */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			if (iptr->sx.val.i & 0x1f)	{
				M_INTMOVE(s1, REG_ITMP1)
				if ((iptr->sx.val.i & 0x1f) <= 7)	{
					M_IUSR_IMM(iptr->sx.val.i & 0x1f, REG_ITMP1);
				} else	{
					M_IMOV_IMM(iptr->sx.val.i & 0x1f, REG_ITMP2);
					M_IUSR(REG_ITMP2, REG_ITMP1);
				}
				M_INTMOVE(REG_ITMP1, d);
			} else	{
				M_INTMOVE(s1, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IAND:       /* ..., val1, val2  ==> ..., val1 & val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(s2, REG_ITMP2);
			M_IAND(s1, REG_ITMP2);
			M_INTMOVE(REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);
			M_IAND_IMM(iptr->sx.val.i, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IOR:        /* ..., val1, val2  ==> ..., val1 | val2        */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(s2, REG_ITMP2);
			M_IOR(s1, REG_ITMP2);
			M_INTMOVE(REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.i = constant                          */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);
			M_IOR_IMM(iptr->sx.val.i, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IXOR:        /* ..., val1, val2  ==> ..., val1 | val2        */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_INTMOVE(s2, REG_ITMP2);
			M_IXOR(s1, REG_ITMP2);
			M_INTMOVE(REG_ITMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IXORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.i = constant                          */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1);
			M_IXOR_IMM(iptr->sx.val.i, REG_ITMP1);
			M_INTMOVE(REG_ITMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		/* floating point operations ******************************************/
		#if !defined(ENABLE_SOFTFLOAT)
		case ICMD_FCMPL:		/* ..., val1, val2  ==> ..., val1 fcmpl val2  */
		case ICMD_DCMPL:
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_IMOV_IMM(-1, d);
			M_FCMP(s1, s2);
			M_BFUN(14);	/* result is -1, branch to end */
			M_BFLT(10);	/* result is -1, branch to end */
			M_IMOV_IMM(0, d);
			M_BFEQ(4)	/* result is 0, branch to end */
			M_IMOV_IMM(1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCMPG:		/* ..., val1, val2  ==> ..., val1 fcmpg val2  */
		case ICMD_DCMPG:
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_IMOV_IMM(1, d);
			M_FCMP(s1, s2);
			M_BFUN(16);	/* result is +1, branch to end */
			M_BFGT(14);	/* result is +1, branch to end */
			M_IMOV_IMM(0, d);
			M_BFEQ(8)	/* result is 0, branch to end */
			M_IMOV_IMM(-1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_FLTMOVE(s2, REG_FTMP2);
			M_FMUL(s1, REG_FTMP2);
			M_FLTMOVE(REG_FTMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_DBLMOVE(s2, REG_FTMP2);
			M_DMUL(s1, REG_FTMP2);
			M_DBLMOVE(REG_FTMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_FLTMOVE(s1, REG_FTMP1);
			M_FDIV(s2, REG_FTMP1);
			M_FLTMOVE(REG_FTMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_DBLMOVE(s1, REG_FTMP1);
			M_DDIV(s2, REG_FTMP1);
			M_DBLMOVE(REG_FTMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FADD:       /* ..., val1, val2  ==> ..., val1 + val2        */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_FLTMOVE(s2, REG_FTMP2);
			M_FADD(s1, REG_FTMP2);
			M_FLTMOVE(REG_FTMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DADD:       /* ..., val1, val2  ==> ..., val1 + val2        */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_DBLMOVE(s2, REG_FTMP2);
			M_DADD(s1, REG_FTMP2);
			M_DBLMOVE(REG_FTMP2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_FLTMOVE(s1, REG_FTMP1);
			M_FSUB(s2, REG_FTMP1);
			M_FLTMOVE(REG_FTMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_DBLMOVE(s1, REG_FTMP1);
			M_DSUB(s2, REG_FTMP1);
			M_DBLMOVE(REG_FTMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_F2D:       /* ..., value  ==> ..., (double) value           */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_F2D(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_D2F:       /* ..., value  ==> ..., (float) value           */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_D2F(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

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

		#endif

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


		case ICMD_ACONST:     /* ...  ==> ..., constant                       */
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				constant_classref *cr = iptr->sx.val.c.ref;;
				codegen_addpatchref(cd, PATCHER_resolve_classref_to_classinfo, cr, 0);
				M_AMOV_IMM(0, d);
			} else {
				M_AMOV_IMM(iptr->sx.val.anyptr, d);
			}
			emit_store_dst(jd, iptr, d);
			break;
		/* BRANCH *************************************************************/

		case ICMD_ATHROW:       /* ..., objectref ==> ... (, objectref)       */

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			M_ADRMOVE(s1, REG_ATMP1_XPTR);

#ifdef ENABLE_VERIFIER
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_class *uc = iptr->sx.s23.s2.uc;

				codegen_addpatchref(cd, PATCHER_resolve_class, uc, 0);
			}
#endif /* ENABLE_VERIFIER */
			M_JSR_PCREL(2);				/* get current PC */
			M_APOP(REG_ATMP2);		

			M_AMOV_IMM(asm_handle_exception, REG_ATMP3);
			M_JMP(REG_ATMP3);
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
			assert(IS_ADR_TYPE(VAROP(iptr->s1)->type));
			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			M_ATST(s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IFNULL, BRANCH_OPT_NONE);
			break;

		case ICMD_IFLT:
		case ICMD_IFLE:
		case ICMD_IFNE:
		case ICMD_IFGT:
		case ICMD_IFGE:
		case ICMD_IFEQ:         /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			assert (VAROP(iptr->s1)->type == TYPE_INT);
			M_ICMP_IMM(iptr->sx.val.i, s1); 
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IFEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_ICMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ICMPNE:
		case ICMD_IF_ICMPLT:
		case ICMD_IF_ICMPGT:
		case ICMD_IF_ICMPLE:
		case ICMD_IF_ICMPGE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_ICMP(s2, s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_ICMPEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_ACMPEQ:    /* op1 = target JavaVM pc                     */
		case ICMD_IF_ACMPNE:

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ATMP2);
			M_ACMP(s1, s2);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_ACMPEQ, BRANCH_OPT_NONE);
			break;


		/* MEMORY *************************************************************/
		case ICMD_GETSTATIC:
			if (INSTRUCTION_IS_UNRESOLVED(iptr))	{
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				codegen_addpatchref(cd, PATCHER_get_putstatic, uf, 0);
			} else	{
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;

				fieldtype = fi->type;
				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class))	{
					codegen_addpatchref(cd, PATCHER_initialize_class, fi->class, 0);
				}

				disp = (ptrint) &(fi->value);
			}
			M_AMOV_IMM(disp, REG_ATMP1);
			switch (fieldtype) {
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
#endif
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
				M_ILD(d, REG_ATMP1, 0);
				break;
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
				M_ALD(d, REG_ATMP1, 0);
				break;
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_DBL:
#endif
			case TYPE_LNG:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP23_PACKED);
				M_LLD(d, REG_ATMP1, 0);
				break;
#if !defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_FLD(d, REG_ATMP1, 0);
				break;
			case TYPE_DBL:				
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_DLD(d, REG_ATMP1, 0);
				break;
#endif
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_PUTSTATIC:  /* ..., value  ==> ...                          */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;

				codegen_addpatchref(cd, PATCHER_get_putstatic, uf, 0);
			} else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = &(fi->value);

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class))
					codegen_addpatchref(cd, PATCHER_initialize_class, fi->class, 0);
  			}
		
			M_AMOV_IMM(disp, REG_ATMP1);
			switch (fieldtype) {
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
#endif
			case TYPE_INT:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_IST(s1, REG_ATMP1, 0);
				break;
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_DBL:
#endif
			case TYPE_LNG:
				s1 = emit_load_s1(jd, iptr, REG_ITMP23_PACKED);
				M_LST(s1, REG_ATMP1, 0);
				break;
			case TYPE_ADR:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_AST(s1, REG_ATMP1, 0);
				break;
#if !defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
				s1 = emit_load_s1(jd, iptr, REG_FTMP2);
				M_FST(s1, REG_ATMP1, 0);
				break;
			case TYPE_DBL:
				s1 = emit_load_s1(jd, iptr, REG_FTMP2);
				M_DST(s1, REG_ATMP1, 0);
				break;
#endif
			default: assert(0);
			}
			break;

		case ICMD_GETFIELD:   /* ...  ==> ..., value                          */

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);

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
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
#endif
			case TYPE_INT:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD(d, s1, disp);
				break;
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_DBL:
#endif
			case TYPE_LNG:
   				d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
				M_LLD(d, s1, disp);
				break;
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ALD(d, s1, disp);
				break;
#if !defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_FLD(d, s1, disp);
				break;
			case TYPE_DBL:				
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_DLD(d, s1, disp);
				break;
#endif
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_PUTFIELD:   /* ..., value  ==> ...                          */

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);

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
				if (IS_2_WORD_TYPE(fieldtype)) {
					s2 = emit_load_s2(jd, iptr, REG_ITMP23_PACKED);
				} else {
					s2 = emit_load_s2(jd, iptr, REG_ITMP2);
				}
			} else {
				s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			}

			if (INSTRUCTION_IS_UNRESOLVED(iptr))
				codegen_addpatchref(cd, PATCHER_get_putfield, uf, 0);

			/* implicit null-pointer check */
			switch (fieldtype) {
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
#endif
			case TYPE_INT:
				M_IST(s2, s1, disp);
				break;

#if defined(ENABLE_SOFTFLOAT)
			case TYPE_DBL:
#endif
			case TYPE_LNG:
				M_LST(s2, s1, disp);  
				break;
			case TYPE_ADR:
				M_AST(s2, s1, disp);
				break;
#if !defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
				M_FST(s2, s1, disp);
				break;
			case TYPE_DBL:
				M_DST(s2, s1, disp);
				break;
#endif
			}
			break;

		case ICMD_ARRAYLENGTH: /* ..., arrayref  ==> ..., length              */

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			M_ILD(d, s1, OFFSET(java_arrayheader, size));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_BALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_INTMOVE(s2, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_bytearray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
			M_LBZX(REG_ATMP1, d);
			M_BSEXT(d, d);
			emit_store_dst(jd, iptr, d);
			break;			

		case ICMD_CALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(1, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_chararray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
			M_LHZX(REG_ATMP1, d);
			M_CZEXT(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_SALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(1, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_shortarray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
		
			/* implicit null-pointer check */
			M_LHZX(REG_ATMP1, d);
			M_SSEXT(d, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(2, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_intarray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
			M_LWZX(REG_ATMP1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LALOAD:     /* ..., arrayref, index  ==> ..., value         */
			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_INTMOVE(s2, REG_ITMP1);
			M_ISSL_IMM(3, REG_ITMP1);
			M_IADD_IMM(OFFSET(java_longarray, data[0]), REG_ITMP1);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP1, REG_ATMP1);
			/* implicit null-pointer check */
			M_LLD(d, REG_ATMP1, 0);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FALOAD:     /* ..., arrayref, index  ==> ..., value         */
			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(2, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_floatarray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
#if !defined(ENABLE_SOFTFLOAT)
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_FLD(d, REG_ATMP1, 0);
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_LWZX(REG_ATMP1, d);
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DALOAD:     /* ..., arrayref, index  ==> ..., value         */
			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(3, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_doublearray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
#if !defined(ENABLE_SOFTFLOAT)
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_DLD(d, REG_ATMP1, 0);
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LLD(d, REG_ATMP1, 0);
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_AALOAD:     /* ..., arrayref, index  ==> ..., value         */
			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(2, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_objectarray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
	
			/* implicit null-pointer check */
			M_LAX(REG_ATMP1, d);
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_BASTORE:    /* ..., arrayref, index, value  ==> ...         */
			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_INTMOVE(s2, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_bytearray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
			M_STBX(REG_ATMP1, s3);
			break;

		case ICMD_CASTORE:    /* ..., arrayref, index, value  ==> ...         */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(1, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_chararray, data[0]), REG_ITMP2); 
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
			M_STHX(REG_ATMP1, s3);
			break;

		case ICMD_SASTORE:    /* ..., arrayref, index, value  ==> ...         */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(1, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_shortarray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
			M_STHX(REG_ATMP1, s3);
			break;

		case ICMD_IASTORE:    /* ..., arrayref, index, value  ==> ...         */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(2, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_intarray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
			M_STWX(REG_ATMP1, s3);
			break;

		case ICMD_LASTORE:    /* ..., arrayref, index, value  ==> ...         */
			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP1);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);

			M_INTMOVE(s2, REG_ITMP1);
			M_ISSL_IMM(3, REG_ITMP1);
			M_IADD_IMM(OFFSET(java_longarray, data[0]), REG_ITMP1);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP1, REG_ATMP1);
			/* implicit null-pointer check */
			s3 = emit_load_s3(jd, iptr, REG_ITMP12_PACKED);
			M_LST(s3, REG_ATMP1, 0);
			break;

		case ICMD_FASTORE:    /* ..., arrayref, index, value  ==> ...         */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(2, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_floatarray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
#if !defined(ENABLE_SOFTFLOAT)
			s3 = emit_load_s3(jd, iptr, REG_FTMP3);
			M_FST(s3, REG_ATMP1, 0);
#else
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_STWX(REG_ATMP1, s3);
#endif
			break;

		case ICMD_DASTORE:    /* ..., arrayref, index, value  ==> ...         */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_INTMOVE(s2, REG_ITMP2);
			M_ISSL_IMM(3, REG_ITMP2);
			M_IADD_IMM(OFFSET(java_doublearray, data[0]), REG_ITMP2);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP2, REG_ATMP1);
			/* implicit null-pointer check */
#if !defined(ENABLE_SOFTFLOAT)
			s3 = emit_load_s3(jd, iptr, REG_FTMP3);
			M_DST(s3, REG_ATMP1, 0);
#else
			s3 = emit_load_s3(jd, iptr, REG_ITMP12_PACKED);
			/* implicit null-pointer check */
			M_LST(s3, REG_ATMP1, 0);
#endif
			break;

		case ICMD_AASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP1);
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ATMP2);

			/* XXX what if array is NULL */
			disp = dseg_add_functionptr(cd, BUILTIN_canstore);

			M_AST(s1, REG_SP, 0*4);
			M_AST(s3, REG_SP, 1*4);
			M_JSR_IMM(BUILTIN_canstore);	
			emit_exception_check(cd, iptr);

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP1);
			s3 = emit_load_s3(jd, iptr, REG_ATMP2);
			M_INTMOVE(s2, REG_ITMP1);
			M_ISSL_IMM(2, REG_ITMP1);
			M_IADD_IMM(OFFSET(java_objectarray, data[0]), REG_ITMP1);
			M_ADRMOVE(s1, REG_ATMP1);
			M_AADDINT(REG_ITMP1, REG_ATMP1);
			/* implicit null-pointer check */
			M_STAX(REG_ATMP1, s3);
			break;



		/* METHOD INVOCATION *********************************************************/
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

			/* copy arguments to stack */
			for (s3 = s3 - 1; s3 >= 0; s3--)	{
				var = VAR(iptr->sx.s23.s2.args[s3]);
				/* already preallocated */
				if (var->flags & PREALLOC) continue;
		
				if (!md->params[s3].inmemory) assert(0);

				switch (var->type)	{
#if defined(ENABLE_SOFTFLOAT)
					case TYPE_DBL:
#endif
					case TYPE_LNG:
						d = emit_load(jd, iptr, var, REG_ITMP12_PACKED);
						M_LST(d, REG_SP, md->params[s3].regoff*4);
						break;
#if defined(ENABLE_SOFTFLOAT)
					case TYPE_FLT:
#endif
					case TYPE_INT:
						d = emit_load(jd, iptr, var, REG_ITMP1);
						M_IST(d, REG_SP, md->params[s3].regoff*4);
						break;
					case TYPE_ADR:
						d = emit_load(jd, iptr, var, REG_ATMP1);
						M_AST(d, REG_SP, md->params[s3].regoff*4);
						break;
#if !defined(ENABLE_SOFTFLOAT)
					case TYPE_FLT:
						d = emit_load(jd, iptr, var, REG_FTMP1);
						M_FST(d, REG_SP, md->params[s3].regoff*4);
						break;
					case TYPE_DBL:
						d = emit_load(jd, iptr, var, REG_FTMP1);
						M_DST(d, REG_SP, md->params[s3].regoff*4);
						break;
#endif
					default:
						assert(0);
				}
			}

			/* arguments in place now */
			switch(iptr->opc)	{
				case ICMD_BUILTIN: 
					disp = (ptrint) bte->fp;
					d = md->returntype.type;
					M_JSR_IMM(disp);

					REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
					emit_exception_check(cd, iptr);
					break;

				case ICMD_INVOKESPECIAL: 
					/* adress register for sure */
					M_ALD(REG_ATMP1, REG_SP, 0);
					emit_nullpointer_check(cd, iptr, REG_ATMP1);
					/* fall through */
				case ICMD_INVOKESTATIC: 
					if (lm == NULL) {
						codegen_addpatchref(cd, PATCHER_invokestatic_special, um, 0);
						disp = 0;
						M_AMOV_IMM(disp, REG_ATMP1);
					} else	{
						disp = lm->stubroutine;
						M_AMOV_IMM(disp, REG_ATMP1);
					}

					/* generate the actual call */
					M_JSR(REG_ATMP1);
					REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
					break;


				case ICMD_INVOKEVIRTUAL:
					if (lm == NULL) {
						codegen_addpatchref(cd, PATCHER_invokevirtual, um, 0);
						s1 = 0;
					} else {
						s1 = OFFSET(vftbl_t, table[0]) + sizeof(methodptr) * lm->vftblindex;
					}
					/* load object pointer (==argument 0) */
					M_ALD(REG_ATMP1, REG_SP, 0);
					/* implicit null-pointer check */
					M_ALD(REG_METHODPTR, REG_ATMP1, OFFSET(java_objectheader, vftbl));
					M_ALD(REG_ATMP3, REG_METHODPTR, s1);
					/* generate the actual call */
					M_JSR(REG_ATMP3);
					break;
				case ICMD_INVOKEINTERFACE: 
					if (lm == NULL) {
						codegen_addpatchref(cd, PATCHER_invokeinterface, um, 0);

						s1 = 0;
						s2 = 0;
					} else {
						s1 = OFFSET(vftbl_t, interfacetable[0]) - sizeof(methodptr*) * lm->class->index;
						s2 = sizeof(methodptr) * (lm - lm->class->methods);
					}
					/* load object pointer (==argument 0) */
					M_ALD(REG_ATMP1, REG_SP, 0);

					/* implicit null-pointer check */
					M_ALD(REG_METHODPTR, REG_ATMP1, OFFSET(java_objectheader, vftbl));
					M_ALD(REG_METHODPTR, REG_METHODPTR, s1);
					M_ALD(REG_ATMP3, REG_METHODPTR, s2);

					/* generate the actual call */
					M_JSR(REG_ATMP3);
					REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
					break;

				default: assert(0);
				}	/* switch (iptr->opc) */

				REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);
				
				/* store return value */
				d = md->returntype.type;

				switch (d)	{
					case TYPE_VOID:	break;
#if defined(ENABLE_SOFTFLOAT)
					case TYPE_FLT:
#endif
					case TYPE_INT:
						s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT);
						M_INTMOVE(REG_RESULT, s1);
						break;
#if defined(ENABLE_SOFTFLOAT)
					case TYPE_DBL:
#endif
					case TYPE_LNG:
						s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);
						M_LNGMOVE(REG_RESULT_PACKED, s1);
						break;
					case TYPE_ADR:
						s1 = codegen_reg_of_dst(jd, iptr, REG_ATMP1);
						/* all stuff is returned in %d0 */
						M_INT2ADRMOVE(REG_RESULT, s1);
						break;
#if !defined(ENABLE_SOFTFLOAT)
					/*
					 *	for BUILTINS float values are returned in %d0,%d1
					 *	within cacao we use %fp0 for that.
					 */
					case TYPE_FLT:
						s1 = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
						if (iptr->opc == ICMD_BUILTIN)	{
							M_INT2FLTMOVE(REG_FRESULT, s1);
						} else	{
							M_FLTMOVE(REG_FRESULT, s1);
						}
						break;
					case TYPE_DBL:
						s1 = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
						if (iptr->opc == ICMD_BUILTIN)	{
							M_LST(REG_RESULT_PACKED, REG_SP, rd->memuse * 4);
							M_DLD(s1, REG_SP, rd->memuse * 4);
						} else	{
							M_DBLMOVE(REG_FRESULT, s1);
						}
						break;
#endif
					default:
						assert(0);
				}
				if (d != TYPE_VOID) emit_store_dst(jd, iptr, s1);
			break; /* ICMD_INVOKE* */

#if defined(ENABLE_SOFTFLOAT)
		case ICMD_FRETURN:
#endif
		case ICMD_IRETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT);
			M_INTMOVE(s1, REG_RESULT);
			goto nowperformreturn;

		case ICMD_ARETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT);
			assert(VAROP(iptr->s1)->type == TYPE_ADR);
			M_ADR2INTMOVE(s1, REG_RESULT);

#ifdef ENABLE_VERIFIER
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_class *uc = iptr->sx.s23.s2.uc;

				codegen_addpatchref(cd, PATCHER_resolve_class, uc, 0);
			}
#endif /* ENABLE_VERIFIER */
			goto nowperformreturn;

#if defined(ENABLE_SOFTFLOAT)
		case ICMD_DRETURN:
#endif
		case ICMD_LRETURN:      /* ..., retvalue ==> ...                      */
			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(s1, REG_RESULT_PACKED);
			goto nowperformreturn;

#if !defined(ENABLE_SOFTFLOAT)
		case ICMD_FRETURN:      /* ..., retvalue ==> ...                      */
			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_FRESULT);
			M_FLTMOVE(s1, REG_FRESULT);
			goto nowperformreturn;

		case ICMD_DRETURN:
			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_FRESULT);
			M_DBLMOVE(s1, REG_FRESULT);
			goto nowperformreturn;

#endif

		case ICMD_RETURN:      /* ...  ==> ...                                */

			REPLACEMENT_POINT_RETURN(cd, iptr);

nowperformreturn:
			{
			s4 i, p;
			
			p = cd->stackframesize;

			/* call trace function */
#if !defined(NDEBUG)
			emit_verbosecall_exit(jd);
#endif

#if defined(ENABLE_THREADS)
			/* call lock_monitor_exit */
			if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
				M_ILD(REG_ITMP3, REG_SP, rd->memuse * 4 + 2*4);

				/* we need to save the proper return value */
				/* we do not care for the long -> doubel convert space here */
				switch (iptr->opc) {
#if defined(ENABLE_SOFTFLOAT)
				case ICMD_DRETURN:
#endif
				case ICMD_LRETURN:
					M_LST(REG_RESULT_PACKED, REG_SP, rd->memuse * 4);
					break;
#if defined(ENABLE_SOFTFLOAT)
				case ICMD_FRETURN:
#endif
				case ICMD_IRETURN:
				case ICMD_ARETURN:
					M_IST(REG_RESULT , REG_SP, rd->memuse * 4);
					break;
#if !defined(ENABLE_SOFTFLOAT)
				case ICMD_FRETURN:
					M_FST(REG_FRESULT, REG_SP, rd->memuse * 4);
					break;
				case ICMD_DRETURN:
					M_DST(REG_FRESULT, REG_SP, rd->memuse * 4);
					break;
#endif
				}

				M_IST(REG_ITMP3, REG_SP, 0 * 4);
				M_JSR_IMM(LOCK_monitor_exit);

				/* and now restore the proper return value */
				switch (iptr->opc) {

#if defined(ENABLE_SOFTFLOAT)
				case ICMD_DRETURN:
#endif
				case ICMD_LRETURN:
					M_LLD(REG_RESULT_PACKED, REG_SP, rd->memuse * 4);
					break;
#if defined(ENABLE_SOFTFLOAT)
				case ICMD_FRETURN:
#endif
				case ICMD_IRETURN:
				case ICMD_ARETURN:
					M_ILD(REG_RESULT , REG_SP, rd->memuse * 4);
					break;
#if !defined(ENABLE_SOFTFLOAT)
				case ICMD_FRETURN:
					M_FLD(REG_FRESULT, REG_SP, rd->memuse * 4);
					break;
				case ICMD_DRETURN:
					M_DLD(REG_FRESULT, REG_SP, rd->memuse * 4);
					break;
#endif
				}
			}
#endif


			/* restore return address                                         */
#if 0
			if (!jd->isleafmethod) {
				/* ATTENTION: Don't use REG_ZERO (r0) here, as M_ALD
				   may have a displacement overflow. */

				M_ALD(REG_ITMP1, REG_SP, p * 4 + LA_LR_OFFSET);
				M_MTLR(REG_ITMP1);
			}
#endif
			/* restore saved registers                                        */

			for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
				p--; M_ILD(rd->savintregs[i], REG_SP, p * 4);
			}
			for (i=ADR_SAV_CNT-1; i>=rd->savadrreguse; --i)	{
				p--; M_ALD(rd->savadrregs[i], REG_SP, p*4);
			}
#if !defined(ENABLE_SOFTFLOAT)
			for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
				p -= 2; M_DLD(rd->savfltregs[i], REG_SP, p * 4);
			}
#endif
			/* deallocate stack                                               */
			M_AADD_IMM(cd->stackframesize*4, REG_SP);
			M_RET;
			}
			break;

		/* the evil ones */
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

			s1 = emit_load_s1(jd, iptr, REG_ATMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);

			assert(VAROP(iptr->s1 )->type == TYPE_ADR);
			assert(VAROP(iptr->dst)->type == TYPE_INT);

			M_ICLR(d);

			/* if class is not resolved, check which code to call */

			if (super == NULL) {
				M_ATST(s1);
				emit_label_beq(cd, BRANCH_LABEL_1);

				codegen_addpatchref(cd, PATCHER_resolve_classref_to_flags, iptr->sx.s23.s3.c.ref, 0);

				M_IMOV_IMM32(0, REG_ITMP3);
				M_IAND_IMM(ACC_INTERFACE, REG_ITMP3);
				emit_label_beq(cd, BRANCH_LABEL_2);
			}

			/* interface instanceof code */

			if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
				if (super == NULL) {
					codegen_addpatchref(cd, PATCHER_instanceof_interface, iptr->sx.s23.s3.c.ref, 0);
				} else {
					M_ATST(s1);
					emit_label_beq(cd, BRANCH_LABEL_3);
				}

				M_ALD(REG_ATMP1, s1, OFFSET(java_objectheader, vftbl));
				M_ILD(REG_ITMP3, REG_ATMP1, OFFSET(vftbl_t, interfacetablelength));
				M_IADD_IMM(-superindex, REG_ITMP3);	/* -superindex may be patched patched */
				M_ITST(REG_ITMP3);
				M_BLE(10);
				M_ALD(REG_ATMP1, REG_ATMP1, OFFSET(vftbl_t, interfacetable[0]) - superindex * sizeof(methodptr*));	/* patch here too! */
				M_ATST(REG_ATMP1);
				M_BEQ(2);
				M_IMOV_IMM(1, d);

				if (super == NULL)
					emit_label_br(cd, BRANCH_LABEL_4);
				else
					emit_label(cd, BRANCH_LABEL_3);
			}

			/* class instanceof code */

			if ((super == NULL) || !(super->flags & ACC_INTERFACE)) {
				if (super == NULL) {
					emit_label(cd, BRANCH_LABEL_2);

					codegen_addpatchref(cd, PATCHER_resolve_classref_to_vftbl, iptr->sx.s23.s3.c.ref, 0);
					M_AMOV_IMM(0, REG_ATMP2);
				} else {
					M_AMOV_IMM(super->vftbl, REG_ATMP2);
					M_ATST(s1);
					emit_label_beq(cd, BRANCH_LABEL_5);
				}

				M_ALD(REG_ATMP1, s1, OFFSET(java_objectheader, vftbl));

				CODEGEN_CRITICAL_SECTION_START;

				M_ILD(REG_ITMP1, REG_ATMP1, OFFSET(vftbl_t, baseval));
				M_ILD(REG_ITMP3, REG_ATMP2, OFFSET(vftbl_t, baseval));
				M_ILD(REG_ITMP2, REG_ATMP2, OFFSET(vftbl_t, diffval));

				CODEGEN_CRITICAL_SECTION_END;

				M_ISUB(REG_ITMP3, REG_ITMP1);
				M_ICMP(REG_ITMP2, REG_ITMP1);
				M_BHI(4);
				M_IMOV_IMM(1, d);
				M_TPFW;			/* overlaps next instruction */
				M_ICLR(d);

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
					super      = NULL;
					superindex = 0;
				}
				else {
					super      = iptr->sx.s23.s3.c.cls;
					superindex = super->index;
				}

				if ((super == NULL) || !(super->flags & ACC_INTERFACE))
					CODEGEN_CRITICAL_SECTION_NEW;

				s1 = emit_load_s1(jd, iptr, REG_ATMP1);
				assert(VAROP(iptr->s1)->type == TYPE_ADR);

				/* if class is not resolved, check which code to call */

				if (super == NULL) {
					M_ATST(s1);
					emit_label_beq(cd, BRANCH_LABEL_1);

					codegen_addpatchref(cd, PATCHER_resolve_classref_to_flags, iptr->sx.s23.s3.c.ref, 0);
			
					M_IMOV_IMM32(0, REG_ITMP2);
					M_IAND_IMM(ACC_INTERFACE, REG_ITMP2);
					emit_label_beq(cd, BRANCH_LABEL_2);
				}

				/* interface checkcast code */

				if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
					if (super == NULL) {
						codegen_addpatchref(cd, PATCHER_checkcast_interface, iptr->sx.s23.s3.c.ref, 0);
					} else {
						M_ATST(s1);
						emit_label_beq(cd, BRANCH_LABEL_3);
					}

					M_ALD(REG_ATMP2, s1, OFFSET(java_objectheader, vftbl));
					M_ILD(REG_ITMP3, REG_ATMP2, OFFSET(vftbl_t, interfacetablelength));
	
					M_IADD_IMM(-superindex, REG_ITMP3);	/* superindex patched */
					M_ITST(REG_ITMP3);
					emit_classcast_check(cd, iptr, BRANCH_LE, REG_ITMP3, s1);

					M_ALD(REG_ATMP3, REG_ATMP2, OFFSET(vftbl_t, interfacetable[0]) - superindex * sizeof(methodptr*));	/* patched*/
					M_ATST(REG_ATMP3);
					emit_classcast_check(cd, iptr, BRANCH_EQ, REG_ATMP3, s1);

					if (super == NULL)
						emit_label_br(cd, BRANCH_LABEL_4);
					else
						emit_label(cd, BRANCH_LABEL_3);
				}

				/* class checkcast code */

				if ((super == NULL) || !(super->flags & ACC_INTERFACE)) {
					if (super == NULL) {
						emit_label(cd, BRANCH_LABEL_2);

						codegen_addpatchref(cd, PATCHER_resolve_classref_to_vftbl, iptr->sx.s23.s3.c.ref, 0);
						M_AMOV_IMM(0, REG_ATMP3);
					} else {
						M_AMOV_IMM(super->vftbl, REG_ATMP3);
						M_ATST(s1);
						emit_label_beq(cd, BRANCH_LABEL_5);
					}

					M_ALD(REG_ATMP2, s1, OFFSET(java_objectheader, vftbl));

					CODEGEN_CRITICAL_SECTION_START;

					M_ILD(REG_ITMP3, REG_ATMP2, OFFSET(vftbl_t, baseval));	/* REG_ITMP3 == sub->vftbl->baseval */
					M_ILD(REG_ITMP1, REG_ATMP3, OFFSET(vftbl_t, baseval));
					M_ILD(REG_ITMP2, REG_ATMP3, OFFSET(vftbl_t, diffval));

					CODEGEN_CRITICAL_SECTION_END;

					M_ISUB(REG_ITMP1, REG_ITMP3);
					M_ICMP(REG_ITMP2, REG_ITMP3);	/* XXX was CMPU */

					emit_classcast_check(cd, iptr, BRANCH_UGT, REG_ITMP3, s1); /* XXX was BRANCH_GT */

					if (super != NULL)
						emit_label(cd, BRANCH_LABEL_5);
				}

				if (super == NULL) {
					emit_label(cd, BRANCH_LABEL_1);
					emit_label(cd, BRANCH_LABEL_4);
				}

				d = codegen_reg_of_dst(jd, iptr, s1);
			} else {
				/* array type cast-check */

				s1 = emit_load_s1(jd, iptr, REG_ATMP2);

				if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
					codegen_addpatchref(cd, PATCHER_resolve_classref_to_classinfo, iptr->sx.s23.s3.c.ref, 0);
					M_AMOV_IMM(0, REG_ATMP1);
				} else {
					M_AMOV_IMM(iptr->sx.s23.s3.c.cls, REG_ATMP1);
				}
	
				M_APUSH(REG_ATMP1);
				M_APUSH(s1);
				M_JSR_IMM(BUILTIN_arraycheckcast);
				M_AADD_IMM(2*4, REG_SP);		/* pop arguments off stack */
				M_ITST(REG_RESULT);
				emit_classcast_check(cd, iptr, BRANCH_EQ, REG_RESULT, s1);

				s1 = emit_load_s1(jd, iptr, REG_ITMP1);
				d = codegen_reg_of_dst(jd, iptr, s1);
			}
			assert(VAROP(iptr->dst)->type == TYPE_ADR);
			M_ADRMOVE(s1, d);
			emit_store_dst(jd, iptr, d);
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
			if (l != 0) M_ISUB_IMM(l, REG_ITMP1);

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

			/* length of dataseg after last dseg_add_target is used by load */
			M_AMOV_IMM(0, REG_ATMP2);
			dseg_adddata(cd);

			M_ISSL_IMM(2, REG_ITMP1);			/* index * 4 == offset in table */
			M_AADDINT(REG_ITMP1, REG_ATMP2);		/* offset in table */
			M_AADD_IMM(-(cd->dseglen), REG_ATMP2);		/* start of table in dseg */
			M_ALD(REG_ATMP1, REG_ATMP2, 0);

			M_JMP(REG_ATMP1);
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
				M_ICMP_IMM(lookup->value, s1);
				emit_beq(cd, lookup->target.block);
				lookup++;
			}

			emit_br(cd, iptr->sx.s23.s3.lookupdefault.block);
			ALIGNCODENOP;
			break;
			}

		case ICMD_MULTIANEWARRAY:/* ..., cnt1, [cnt2, ...] ==> ..., arrayref  */

			/* check for negative sizes and copy sizes to stack if necessary  */
			MCODECHECK((iptr->s1.argcount << 1) + 64);

			for (s1 = iptr->s1.argcount; --s1 >= 0;) {
				var = VAR(iptr->sx.s23.s2.args[s1]);

				/* Already Preallocated? */
				if (!(var->flags & PREALLOC)) {
					s2 = emit_load(jd, iptr, var, REG_ITMP1);
					M_IST(s2, REG_SP, (s1 + 3) * 4);
				}
			}

			/* a0 = dimension count */
			M_IMOV_IMM(iptr->s1.argcount, REG_ITMP1);
			M_IST(REG_ITMP1, REG_SP, 0*4);

			/* a1 = arraydescriptor */
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				codegen_addpatchref(cd, PATCHER_resolve_classref_to_classinfo, iptr->sx.s23.s3.c.ref, 0);
				M_AMOV_IMM(0, REG_ATMP1);
			} else	{
				M_AMOV_IMM(iptr->sx.s23.s3.c.cls, REG_ATMP1);
			}
			M_AST(REG_ATMP1, REG_SP, 1*4);

			/* a2 = pointer to dimensions = stack pointer */
			M_AMOV(REG_SP, REG_ATMP1);
			M_AADD_IMM(3*4, REG_ATMP1);
			M_AST(REG_ATMP1, REG_SP, 2*4);

			M_JSR_IMM(BUILTIN_multianewarray);

			/* check for exception before result assignment */
			emit_exception_check(cd, iptr);

			assert(VAROP(iptr->dst)->type == TYPE_ADR);
			d = codegen_reg_of_dst(jd, iptr, REG_RESULT);
			M_INT2ADRMOVE(REG_RESULT, d);
			emit_store_dst(jd, iptr, d);
			break;



		default:
			printf("UNKNOWN OPCODE %d\n", iptr->opc);
			exceptions_throw_internalerror("Unknown ICMD %d during code generation", iptr->opc);
			return false;
	} /* switch */
	M_TPF;
	} /* for each instruction */
	} /* if (btpre->flags >= BBREACHED) */
	} /* for each basic block */

	dseg_createlinenumbertable(cd);

	/* generate stubs */
	emit_patcher_stubs(jd);
	REPLACEMENT_EMIT_STUBS(jd);

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

	M_AMOV_IMM(m, REG_ATMP1);
	M_AMOV_IMM(asm_call_jit_compiler, REG_ATMP3);
	M_JMP(REG_ATMP3);
}


/* codegen_emit_stub_native ****************************************************

   Emits a stub routine which calls a native method.

*******************************************************************************/

void codegen_emit_stub_native(jitdata *jd, methoddesc *nmd, functionptr f)
{
	methodinfo   *m;
	codeinfo     *code;
	codegendata  *cd;
	registerdata *rd;
	methoddesc   *md;
	s4 nativeparams, i, j, t, s1, s2;
	
	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;
	rd   = jd->rd;

	md = m->parseddesc;
	nativeparams = (m->flags & ACC_STATIC) ? 2 : 1;

	/* calc stackframe size */
	cd->stackframesize = 	sizeof(stackframeinfo) / SIZEOF_VOID_P +
				sizeof(localref_table) / SIZEOF_VOID_P +
				nmd->memuse +
				1 +						/* functionptr */
				4;						/* args for codegen_start_native_call */

	/* create method header */
	(void) dseg_add_unique_address(cd, code);                      /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 4);         /* FrameSize       */
	(void) dseg_add_unique_s4(cd, 0);                              /* IsSync          */
	(void) dseg_add_unique_s4(cd, 0);                              /* IsLeaf          */
	(void) dseg_add_unique_s4(cd, 0);                              /* IntSave         */
	(void) dseg_add_unique_s4(cd, 0);                              /* FltSave         */
	(void) dseg_addlinenumbertablesize(cd);
	(void) dseg_add_unique_s4(cd, 0);                              /* ExTableSize     */

	/* print call trace */
#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd)) {
		emit_verbosecall_enter(jd);
	}
#endif

	/* generate code */
	M_AADD_IMM(-(cd->stackframesize*4), REG_SP);

	/* get function address (this must happen before the stackframeinfo) */
#if !defined(WITH_STATIC_CLASSPATH)
	if (f == NULL)	{
		codegen_addpatchref(cd, PATCHER_resolve_native_function, m, 0);
	}
#endif
	M_AMOV_IMM(f, REG_ATMP2); /* do not move this line, the patcher is needed */

	M_AST(REG_ATMP2, REG_SP, 4 * 4);

	/* put arguments for codegen_start_native_call onto stack */
	/* void codegen_start_native_call(u1 *datasp, u1 *pv, u1 *sp, u1 *ra) */
	
	M_AMOV(REG_SP, REG_ATMP1);
	M_AADD_IMM(cd->stackframesize * 4, REG_ATMP1);

	M_ALD(REG_ATMP3, REG_ATMP1, 0 * 4);
	M_AST(REG_ATMP3, REG_SP, 3 * 4);		/* ra */

	M_AST(REG_ATMP1, REG_SP, 0 * 4);		/* datasp */

	M_AADD_IMM(1 * 4 , REG_ATMP1);			
	M_AST(REG_ATMP1, REG_SP, 2 * 4);		/* sp */

	M_AMOV_IMM(0, REG_ATMP2);			/* 0 needs to patched */
	dseg_adddata(cd);				    /* this patches it */

	M_AST(REG_ATMP2, REG_SP, 1 * 4);		/* pv */

	M_JSR_IMM(codegen_start_native_call);

	/* load function pointer */
	M_ALD(REG_ATMP2, REG_SP, 4 * 4);

	/* copy arguments into stackframe */
	for (i = md->paramcount -1, j = i + nativeparams; i >= 0; --i, --j)	{
		t = md->paramtypes[i].type;
		/* all arguments via stack */
		assert(md->params[i].inmemory);						

		s1 = (md->params[i].regoff + cd->stackframesize + 1) * 4;
		s2 = nmd->params[j].regoff * 4;

		/* simply copy argument stack */
		M_ILD(REG_ITMP1, REG_SP, s1);
		M_IST(REG_ITMP1, REG_SP, s2);
		if (IS_2_WORD_TYPE(t))	{
			M_ILD(REG_ITMP1, REG_SP, s1 + 4);
			M_IST(REG_ITMP1, REG_SP, s2 + 4);
		}
	}

	/* for static function class as second arg */
	if (m->flags & ACC_STATIC)	{
		M_AMOV_IMM(m->class, REG_ATMP1);
		M_AST(REG_ATMP1, REG_SP, 1 * 4);
	}
	/* env ist first argument */
	M_AMOV_IMM(_Jv_env, REG_ATMP1);
	M_AST(REG_ATMP1, REG_SP, 0 * 4);

	/* call the native function */
	M_JSR(REG_ATMP2);

	/* save return value */
	switch (md->returntype.type)	{
		case TYPE_VOID: break;

#if defined(ENABLE_SOFTFLOAT)
		case TYPE_DBL:
#endif
		case TYPE_LNG:
			M_IST(REG_D1, REG_SP, 2 * 4);
			/* fall through */

#if defined(ENABLE_SOFTFLOAT)
		case TYPE_FLT:
#endif
		case TYPE_INT:
		case TYPE_ADR:
			M_IST(REG_D0, REG_SP, 1 * 4);
			break;

#if !defined(ENABLE_SOFTFLOAT)
		/* natives return float arguments in %d0, %d1, cacao expects them in %fp0 */
		case TYPE_FLT:
			M_INT2FLTMOVE(REG_D0, REG_D0);
			M_FST(REG_D0, REG_SP, 1 * 4);
			break;
		case TYPE_DBL:	
			/* to convert %d0, %d1 to dbl we need 2 memory slots
			 * it is safe reuse argument stack slots here */
			M_IST(REG_D0, REG_SP, 1 * 4);
			M_IST(REG_D1, REG_SP, 2 * 4);
			/*M_DST(REG_D0, REG_SP, 1 * 4);*/
			break;
#endif
		default: assert(0);
	}
	
	/* print call trace */
#if ! defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd)) {
		emit_verbosecall_exit(jd);
	}
#endif
	/* remove native stackframe info */
	/* therefore we call: java_objectheader *codegen_finish_native_call(u1 *datasp) */

	M_AMOV(REG_SP, REG_ATMP3);
	M_AADD_IMM(cd->stackframesize * 4, REG_ATMP3);
	M_AST(REG_ATMP3, REG_SP, 0 * 4);			/* datasp */
	M_JSR_IMM(codegen_finish_native_call);
	
	M_INT2ADRMOVE(REG_RESULT, REG_ATMP1);
	/* restore return value */
	switch (md->returntype.type)	{
		case TYPE_VOID: break;

#if defined(ENABLE_SOFTFLOAT)
		case TYPE_DBL:
#endif
		case TYPE_LNG:
			M_ILD(REG_D1, REG_SP, 2 * 4);
			/* fall through */
#if defined(ENABLE_SOFTFLOAT)
		case TYPE_FLT:
#endif
		case TYPE_INT:
		case TYPE_ADR:
			M_ILD(REG_D0, REG_SP, 1 * 4);
			break;

#if !defined(ENABLE_SOFTFLOAT)
		case TYPE_FLT:
			M_FLD(REG_D0, REG_SP, 1 * 4);
			break;
		case TYPE_DBL:	
			M_DLD(REG_D0, REG_SP, 1 * 4);
			break;
#endif
		default: assert(0);
	}
	/* restore saved registers */

	M_AADD_IMM(cd->stackframesize*4, REG_SP);
	/* check for exception */
	M_ATST(REG_ATMP1);
	M_BNE(2);
	M_RET;

	/* handle exception, REG_ATMP1 already contains exception object, REG_ATMP2 holds address */
	
	M_ALD(REG_ATMP2_XPC, REG_SP, 0);		/* take return address as faulting instruction */
	M_AADD_IMM(-2, REG_ATMP2_XPC);			/* which is off by 2 */
	M_JMP_IMM(asm_handle_nat_exception);

	/* should never be reached from within jit code*/
	M_JSR_IMM(0);

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

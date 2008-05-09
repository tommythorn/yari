/* src/vm/jit/arm/codegen.c - machine code generator for Arm

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

   $Id: codegen.c 7823 2007-04-25 20:03:04Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>

#include "vm/types.h"

#include "md-abi.h"

#include "vm/jit/arm/arch.h"
#include "vm/jit/arm/codegen.h"

#include "mm/memory.h"

#include "native/native.h"

#include "threads/lock-common.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/vm.h"

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

#if defined(ENABLE_LSRA)
#include "vm/jit/allocator/lsra.h"
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
	s4              i, t, len;
	s4              s1, s2, s3, d;
	s4              disp;
	varinfo        *var;
	basicblock     *bptr;
	instruction    *iptr;
	exception_entry *ex;
	s4              fieldtype;
	s4              varindex;

	s4              spilledregs_num;
	s4              savedregs_num;
	u2              savedregs_bitmask;
	u2              currentline;

	methodinfo         *lm;             /* local methodinfo for ICMD_INVOKE* */
	unresolved_method  *um;
	builtintable_entry *bte;
	methoddesc         *md;

	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;
	rd   = jd->rd;

	/* prevent compiler warnings */

	lm  = NULL;
	um  = NULL;
	bte = NULL;

	fieldtype = -1;
	
	/* space to save used callee saved registers */

	savedregs_num = (jd->isleafmethod) ? 0 : 1;       /* space to save the LR */
	savedregs_num += (INT_SAV_CNT - rd->savintreguse);
	savedregs_num += (FLT_SAV_CNT - rd->savfltreguse);

	spilledregs_num = rd->memuse;

#if defined(ENABLE_THREADS)        /* space to save argument of monitor_enter */
	if (checksync && (m->flags & ACC_SYNCHRONIZED))
		spilledregs_num++;
#endif

	cd->stackframesize = spilledregs_num + savedregs_num;

	/* XXX QUICK FIX: We shouldn't align the stack in Java code, but
	   only in native stubs. */
	/* align stack to 8-byte */

	cd->stackframesize = (cd->stackframesize + 1) & ~1;

	/* SECTION: Method Header */
	/* create method header */

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
	(void) dseg_addlinenumbertablesize(cd);
	(void) dseg_add_unique_s4(cd, jd->exceptiontablelength); /* ExTableSize   */

	/* create exception table */

	for (ex = jd->exceptiontable; ex != NULL; ex = ex->down) {
		dseg_add_target(cd, ex->start);
		dseg_add_target(cd, ex->end);
		dseg_add_target(cd, ex->handler);
		(void) dseg_add_unique_address(cd, ex->catchtype.any);
	}

	/* save return address and used callee saved registers */

	savedregs_bitmask = 0;

	if (!jd->isleafmethod)
		savedregs_bitmask = (1<<REG_LR);

	for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--)
		savedregs_bitmask |= (1<<(rd->savintregs[i]));

#if !defined(NDEBUG)
	for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
		log_text("!!! CODEGEN: floating-point callee saved registers are not saved to stack (SEVERE! STACK IS MESSED UP!)");
		/* TODO: floating-point */
	}
#endif

	if (savedregs_bitmask)
		M_STMFD(savedregs_bitmask, REG_SP);

	/* create additional stack frame for spilled variables (if necessary) */

	if ((cd->stackframesize - savedregs_num) > 0)
		M_SUB_IMM_EXT_MUL4(REG_SP, REG_SP, cd->stackframesize - savedregs_num);

	/* take arguments out of register or stack frame */

	md = m->parseddesc;
	for (i = 0, len = 0; i < md->paramcount; i++) {
		s1 = md->params[i].regoff;
		t = md->paramtypes[i].type;

		varindex = jd->local_map[len * 5 + t];

		len += (IS_2_WORD_TYPE(t)) ? 2 : 1;          /* 2 word type arguments */

		if (varindex == UNUSED)
			continue;

		var = VAR(varindex);

		/* ATTENTION: we use interger registers for all arguments (even float) */
#if !defined(ENABLE_SOFTFLOAT)
		if (IS_INT_LNG_TYPE(t)) {                    /* integer args          */
#endif
			if (!md->params[i].inmemory) {           /* register arguments    */
				if (!(var->flags & INMEMORY)) {      /* reg arg -> register   */
					if (GET_LOW_REG(var->vv.regoff) == REG_SPLIT || GET_HIGH_REG(var->vv.regoff) == REG_SPLIT) {
						/* TODO: remove this!!! */
						dolog("SPLIT in local var: %x>%x (%s.%s)", s1, var->vv.regoff, m->class->name->text, m->name->text);
						assert(s1 == var->vv.regoff);
					}
					s3 = var->vv.regoff;
					SPLIT_OPEN(t, s1, REG_ITMP1);
					SPLIT_LOAD(t, s1, cd->stackframesize);
					SPLIT_OPEN(t, s3, REG_ITMP1);

					if (IS_2_WORD_TYPE(t))
						M_LNGMOVE(s1, s3);
					else
						M_INTMOVE(s1, s3);

					SPLIT_STORE_AND_CLOSE(t, s3, cd->stackframesize);
				}
				else {                               /* reg arg -> spilled    */
					SPLIT_OPEN(t, s1, REG_ITMP1);
					SPLIT_LOAD(t, s1, cd->stackframesize);

					if (IS_2_WORD_TYPE(t))
						M_LST(s1, REG_SP, var->vv.regoff * 4);
					else
						M_IST(s1, REG_SP, var->vv.regoff * 4);
					/* no SPLIT_CLOSE here because arg is fully spilled now */
				}
			}
			else {                                   /* stack arguments       */
				if (!(var->flags & INMEMORY)) {      /* stack arg -> register */
					if (IS_2_WORD_TYPE(t))
						M_LLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 4);
					else
						M_ILD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 4);
				}
				else {                               /* stack arg -> spilled  */
					/* Reuse Memory Position on Caller Stack */
					var->vv.regoff = cd->stackframesize + s1;
				}
			}
#if !defined(ENABLE_SOFTFLOAT)
		} else {                                     /* floating args         */
			if (!md->params[i].inmemory) {           /* register arguments    */
				if (!(var->flags & INMEMORY)) {      /* reg arg -> register   */
					SPLIT_OPEN(t, s1, REG_ITMP1);
					SPLIT_LOAD(t, s1, cd->stackframesize);
					M_CAST_INT_TO_FLT_TYPED(t, s1, var->vv.regoff);
				}
				else {                               /* reg arg -> spilled    */
					SPLIT_OPEN(t, s1, REG_ITMP1);
					SPLIT_LOAD(t, s1, cd->stackframesize);

					if (IS_2_WORD_TYPE(t))
						M_LST(s1, REG_SP, var->vv.regoff * 4);
					else
						M_IST(s1, REG_SP, var->vv.regoff * 4);
					/* no SPLIT_CLOSE here because arg is fully spilled now */
				}
			}
			else {                                   /* stack arguments       */
				if (!(var->flags & INMEMORY)) {      /* stack arg -> register */
					if (IS_2_WORD_TYPE(t))
						M_DLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 4);
					else
						M_FLD(var->vv.regoff, REG_SP, (cd->stackframesize + s1) * 4);
				}
				else {                               /* stack arg -> spilled  */
					/* Reuse Memory Position on Caller Stack */
					var->vv.regoff = cd->stackframesize + s1;
				}
			}
		}
#endif /* !defined(ENABLE_SOFTFLOAT) */
	}

#if defined(ENABLE_THREADS)
	/* call monitorenter function */

	if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
		/* stack offset for monitor argument */

		s1 = rd->memuse;

# if !defined(NDEBUG)
		if (JITDATA_HAS_FLAG_VERBOSECALL(jd)) {
			M_STMFD(BITMASK_ARGS, REG_SP);
			s1 += 4;
		}
# endif

		/* get the correct lock object */

		if (m->flags & ACC_STATIC) {
			disp = dseg_add_address(cd, &m->class->object.header);
			M_DSEG_LOAD(REG_A0, disp);
		}
		else {
			emit_nullpointer_check_force(cd, iptr, REG_A0);
		}

		M_STR(REG_A0, REG_SP, s1 * 4);
		disp = dseg_add_functionptr(cd, LOCK_monitor_enter);
		M_DSEG_BRANCH(disp);
		s1 = (s4) (cd->mcodeptr - cd->mcodebase);
		M_RECOMPUTE_PV(s1);

# if !defined(NDEBUG)
		if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
			M_LDMFD(BITMASK_ARGS, REG_SP);
# endif
	}
#endif

#if !defined(NDEBUG)
	/* call trace function */

	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_enter(jd);
#endif

	/* end of header generation */

	/* SECTION: ICMD Code Generation */
	/* for all basic blocks */

	for (bptr = jd->basicblocks; bptr != NULL; bptr = bptr->next) {

		bptr->mpc = (s4) (cd->mcodeptr - cd->mcodebase);

		/* is this basic block reached? */

		if (bptr->flags < BBREACHED)
			continue;

		/* branch resolving */

		codegen_resolve_branchrefs(cd, bptr);

		/* copy interface registers to their destination */

		len = bptr->indepth;

		MCODECHECK(64+len);

#if defined(ENABLE_LSRA)
		if (opt_lsra) {
		while (len) {
			len--;
			var = VAR(bptr->invars[len]);
			if ((len == bptr->indepth-1) && (bptr->type == BBTYPE_EXH)) {
				if (!(var->flags & INMEMORY))
					d= var->vv.regoff;
				else
					d=REG_ITMP1;
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

		/* for all instructions */
		len = bptr->icount;
		currentline = 0;
		for (iptr = bptr->iinstr; len > 0; len--, iptr++) {

			/* add line number */
			if (iptr->line != currentline) {
				dseg_addlinenumber(cd, iptr->line);
				currentline = iptr->line;
			}

			MCODECHECK(64);   /* an instruction usually needs < 64 words      */

			/* the big switch */
			switch (iptr->opc) {
		case ICMD_NOP:        /* ... ==> ...                                  */
			break;

	/* constant operations ************************************************/

		case ICMD_ICONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			ICONST(d, iptr->sx.val.i);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ACONST:     /* ... ==> ..., constant                        */

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				disp = dseg_add_unique_address(cd, NULL);

				codegen_addpatchref(cd, PATCHER_aconst,
				                    iptr->sx.val.c.ref,
									disp);

				if (opt_showdisassemble)
					M_NOP;

				M_DSEG_LOAD(d, disp);
			}
			else {
				ICONST(d, (u4) iptr->sx.val.anyptr);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LCONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			LCONST(d, iptr->sx.val.l);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCONST:     /* ...  ==> ..., constant                       */

#if defined(ENABLE_SOFTFLOAT)
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			ICONST(d, iptr->sx.val.i);
			emit_store_dst(jd, iptr, d);
#else
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			FCONST(d, iptr->sx.val.f);
			emit_store_dst(jd, iptr, d);
#endif
			break;

		case ICMD_DCONST:     /* ...  ==> ..., constant                       */

#if defined(ENABLE_SOFTFLOAT)
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			LCONST(d, iptr->sx.val.l);
			emit_store_dst(jd, iptr, d);
#else
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			DCONST(d, iptr->sx.val.d);
			emit_store_dst(jd, iptr, d);
#endif
			break;


		/* load/store/copy/move operations ************************************/

		case ICMD_ILOAD:      /* ...  ==> ..., content of local variable      */
		case ICMD_ALOAD:      /* op1 = local variable                         */
		case ICMD_FLOAD:
		case ICMD_LLOAD:
		case ICMD_DLOAD:
		case ICMD_ISTORE:     /* ..., value  ==> ...                          */
		case ICMD_FSTORE:
		case ICMD_LSTORE:
		case ICMD_DSTORE:
		case ICMD_COPY:
		case ICMD_MOVE:

			emit_copy(jd, iptr);
			break;

		case ICMD_ASTORE:
			if (!(iptr->flags.bits & INS_FLAG_RETADDR))
				emit_copy(jd, iptr);
			break;

		/* pop operations *****************************************************/

		/* attention: double and longs are only one entry in CACAO ICMDs      */

		case ICMD_POP:        /* ..., value  ==> ...                          */
		case ICMD_POP2:       /* ..., value, value  ==> ...                   */

			break;


		/* integer operations *************************************************/

		case ICMD_INT2BYTE:   /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_MOV(d, REG_LSL(s1, 24));
			M_MOV(d, REG_ASR(d, 24));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2CHAR:   /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_MOV(d, REG_LSL(s1, 16));
			M_MOV(d, REG_LSR(d, 16)); /* ATTENTION: char is unsigned */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INT2SHORT:  /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_MOV(d, REG_LSL(s1, 16));
			M_MOV(d, REG_ASR(d, 16));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2L:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_INTMOVE(s1, GET_LOW_REG(d));
			M_MOV(GET_HIGH_REG(d), REG_ASR(s1, 31));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_L2I:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1); 
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_RSB_IMM(d, s1, 0);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_RSB_IMMS(GET_LOW_REG(d), GET_LOW_REG(s1), 0);
			M_RSC_IMM(GET_HIGH_REG(d), GET_HIGH_REG(s1), 0);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_ADD(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_ADD_S(GET_LOW_REG(d), s1, s2);
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_ADC(GET_HIGH_REG(d), s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IADDCONST:
		case ICMD_IINC:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);

			if (IS_IMM(iptr->sx.val.i)) {
				M_ADD_IMM(d, s1, iptr->sx.val.i);
			} else if (IS_IMM(-iptr->sx.val.i)) {
				M_SUB_IMM(d, s1, (-iptr->sx.val.i));
			} else {
				ICONST(REG_ITMP3, iptr->sx.val.i);
				M_ADD(d, s1, REG_ITMP3);
			}

			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADDCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.l = constant                          */

			s3 = iptr->sx.val.l & 0xffffffff;
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if (IS_IMM(s3))
				M_ADD_IMMS(GET_LOW_REG(d), s1, s3);
			else {
				ICONST(REG_ITMP3, s3);
				M_ADD_S(GET_LOW_REG(d), s1, REG_ITMP3);
			}
			s3 = iptr->sx.val.l >> 32;
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			if (IS_IMM(s3))
				M_ADC_IMM(GET_HIGH_REG(d), s1, s3);
			else {
				ICONST(REG_ITMP3, s3);
				M_ADC(GET_HIGH_REG(d), s1, REG_ITMP3);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_SUB(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_SUB_S(GET_LOW_REG(d), s1, s2);
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_SBC(GET_HIGH_REG(d), s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISUBCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (IS_IMM(iptr->sx.val.i))
				M_SUB_IMM(d, s1, iptr->sx.val.i);
			else {
				ICONST(REG_ITMP3, iptr->sx.val.i);
				M_SUB(d, s1, REG_ITMP3);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUBCONST:  /* ..., value  ==> ..., value - constant        */
		                      /* sx.val.l = constant                          */

			s3 = iptr->sx.val.l & 0xffffffff;
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if (IS_IMM(s3))
				M_SUB_IMMS(GET_LOW_REG(d), s1, s3);
			else {
				ICONST(REG_ITMP3, s3);
				M_SUB_S(GET_LOW_REG(d), s1, REG_ITMP3);
			}
			s3 = iptr->sx.val.l >> 32;
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			if (IS_IMM(s3))
				M_SBC_IMM(GET_HIGH_REG(d), s1, s3);
			else {
				ICONST(REG_ITMP3, s3);
				M_SBC(GET_HIGH_REG(d), s1, REG_ITMP3);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_MUL(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
		case ICMD_IREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_A0);
			s2 = emit_load_s2(jd, iptr, REG_A1);
			emit_arithmetic_check(cd, iptr, s2);

			/* move arguments into argument registers */
			M_INTMOVE(s1, REG_A0);
			M_INTMOVE(s2, REG_A1);

			/* call builtin function */
			bte = iptr->sx.s23.s3.bte;
			disp = dseg_add_functionptr(cd, bte->fp);
			M_DSEG_BRANCH(disp);

			/* recompute pv */
			s1 = (s4) (cd->mcodeptr - cd->mcodebase);
			M_RECOMPUTE_PV(s1);

			/* move result into destination register */
			d = codegen_reg_of_dst(jd, iptr, REG_RESULT);
			M_INTMOVE(REG_RESULT, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
		case ICMD_LREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			/* move arguments into argument registers */

			s1 = emit_load_s1(jd, iptr, REG_A0_A1_PACKED);
			s2 = emit_load_s2(jd, iptr, REG_A2_A3_PACKED);
			/* XXX TODO: only do this if arithmetic check is really done! */
			M_ORR(GET_HIGH_REG(s2), GET_LOW_REG(s2), REG_ITMP3);
			emit_arithmetic_check(cd, iptr, REG_ITMP3);

			M_LNGMOVE(s1, REG_A0_A1_PACKED);
			M_LNGMOVE(s2, REG_A2_A3_PACKED);

			/* call builtin function */
			bte = iptr->sx.s23.s3.bte;
			disp = dseg_add_functionptr(cd, bte->fp);
			M_DSEG_BRANCH(disp);

			/* recompute pv */
			s1 = (s4) (cd->mcodeptr - cd->mcodebase);
			M_RECOMPUTE_PV(s1);

			/* move result into destination register */
			d = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(REG_RESULT_PACKED, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IMULPOW2:   /* ..., value  ==> ..., value * (2 ^ constant)  */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_MOV(d, REG_LSL(s1, iptr->sx.val.i));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIVPOW2:   /* ..., value  ==> ..., value / (2 ^ constant)  */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* this rounds towards 0 as java likes it */
			M_MOV(REG_ITMP3, REG_ASR(s1, 31));
			M_ADD(REG_ITMP3, s1, REG_LSR(REG_ITMP3, 32 - iptr->sx.val.i));
			M_MOV(d, REG_ASR(REG_ITMP3, iptr->sx.val.i));
			/* this rounds towards nearest, not java style */
			/*M_MOV_S(d, REG_ASR(s1, iptr->sx.val.i));
			M_ADCMI_IMM(d, d, 0);*/
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* sx.val.i = constant [ (2 ^ x) - 1 ]          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_MOV_S(REG_ITMP1, s1);
			M_RSBMI_IMM(REG_ITMP1, REG_ITMP1, 0);
			if (IS_IMM(iptr->sx.val.i))
				M_AND_IMM(REG_ITMP1, iptr->sx.val.i, d);
			else {
				ICONST(REG_ITMP3, iptr->sx.val.i);
				M_AND(REG_ITMP1, REG_ITMP3, d);
			}
			M_RSBMI_IMM(d, d, 0);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_AND_IMM(s2, 0x1f, REG_ITMP2);
			M_MOV(d, REG_LSL_REG(s1, REG_ITMP2));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHR:       /* ..., val1, val2  ==> ..., val1 >> val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_AND_IMM(s2, 0x1f, REG_ITMP2);
			M_MOV(d, REG_ASR_REG(s1, REG_ITMP2));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHR:      /* ..., val1, val2  ==> ..., val1 >>> val2      */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_AND_IMM(s2, 0x1f, REG_ITMP2);
			M_MOV(d, REG_LSR_REG(s1, REG_ITMP2));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHLCONST:  /* ..., value  ==> ..., value << constant       */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_MOV(d, REG_LSL(s1, iptr->sx.val.i & 0x1f));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHRCONST:  /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* we need to check for zero here because arm interprets it as SHR by 32 */
			if ((iptr->sx.val.i & 0x1f) == 0) {
				M_INTMOVE(s1, d);
			} else {
				M_MOV(d, REG_ASR(s1, iptr->sx.val.i & 0x1f));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* we need to check for zero here because arm interprets it as SHR by 32 */
			if ((iptr->sx.val.i & 0x1f) == 0)
				M_INTMOVE(s1, d);
			else
				M_MOV(d, REG_LSR(s1, iptr->sx.val.i & 0x1f));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IAND:       /* ..., val1, val2  ==> ..., val1 & val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_AND(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LAND:       /* ..., val1, val2  ==> ..., val1 & val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_AND(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_AND(s1, s2, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IOR:        /* ..., val1, val2  ==> ..., val1 | val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_ORR(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LOR:       /* ..., val1, val2  ==> ..., val1 | val2        */	

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_ORR(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_ORR(s1, s2, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IXOR:       /* ..., val1, val2  ==> ..., val1 ^ val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_EOR(s1, s2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXOR:       /* ..., val1, val2  ==> ..., val1 ^ val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_EOR(s1, s2, GET_LOW_REG(d));
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP3);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_EOR(s1, s2, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;


	/* floating operations ************************************************/

#if !defined(ENABLE_SOFTFLOAT)

		case ICMD_FNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_MNFS(d, s1);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_ADFS(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_SUFS(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_MUFS(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_DVFS(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		/* ATTENTION: Jave does not want IEEE behaviour in FREM, do
		   not use this */

		case ICMD_FREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_RMFS(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_MNFD(d, s1);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_ADFD(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_SUFD(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_MUFD(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_DVFD(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		/* ATTENTION: Jave does not want IEEE behaviour in DREM, do
		   not use this */

		case ICMD_DREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_RMFD(d, s1, s2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2F:       /* ..., value  ==> ..., (float) value            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_FLTS(d, s1);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2D:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_FLTD(d, s1);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_F2I:       /* ..., value  ==> ..., (int) value              */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* this uses round towards zero, as Java likes it */
			M_FIX(d, s1);
			/* this checks for NaN; to return zero as Java likes it */
			M_CMF(s1, 0x8);
			M_MOVVS_IMM(0, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_D2I:       /* ..., value  ==> ..., (int) value              */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* this uses round towards zero, as Java likes it */
			M_FIX(d, s1);
			/* this checks for NaN; to return zero as Java likes it */
			M_CMF(s1, 0x8);
			M_MOVVS_IMM(0, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_D2F:       /* ..., value  ==> ..., (float) value            */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_MVFS(d,s1);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_F2D:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP2);
			M_MVFD(d,s1);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCMPG:      /* ..., val1, val2  ==> ..., val1 fcmpg val2    */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_CMF(s2, s1);
			M_MOV_IMM(d, 0);
			M_SUBGT_IMM(d, d, 1);
			M_ADDLT_IMM(d, d, 1);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DCMPG:      /* ..., val1, val2  ==> ..., val1 dcmpg val2    */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_CMF(s2, s1);
			M_MOV_IMM(d, 0);
			M_SUBGT_IMM(d, d, 1);
			M_ADDLT_IMM(d, d, 1);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCMPL:      /* ..., val1, val2  ==> ..., val1 fcmpl val2    */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_CMF(s1, s2);
			M_MOV_IMM(d, 0);
			M_SUBLT_IMM(d, d, 1);
			M_ADDGT_IMM(d, d, 1);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DCMPL:      /* ..., val1, val2  ==> ..., val1 dcmpl val2    */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_CMF(s1, s2);
			M_MOV_IMM(d, 0);
			M_SUBLT_IMM(d, d, 1);
			M_ADDGT_IMM(d, d, 1);
			emit_store_dst(jd, iptr, d);
			break;

#endif /* !defined(ENABLE_SOFTFLOAT) */


		/* memory operations **************************************************/

		case ICMD_ARRAYLENGTH: /* ..., arrayref  ==> ..., length              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			M_ILD_INTERN(d, s1, OFFSET(java_arrayheader, size));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_BALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP1, s1, s2); /* REG_ITMP1 = s1 + 1 * s2 */
			M_LDRSB(d, REG_ITMP1, OFFSET(java_bytearray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_CALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 1)); /* REG_ITMP1 = s1 + 2 * s2 */
			M_LDRH(d, REG_ITMP1, OFFSET(java_chararray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_SALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 1)); /* REG_ITMP1 = s1 + 2 * s2 */
			M_LDRSH(d, REG_ITMP1, OFFSET(java_shortarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 2)); /* REG_ITMP1 = s1 + 4 * s2 */
			M_ILD_INTERN(d, REG_ITMP1, OFFSET(java_intarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP3, s1, REG_LSL(s2, 3)); /* REG_ITMP3 = s1 + 8 * s2 */
			M_LLD_INTERN(d, REG_ITMP3, OFFSET(java_longarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 2)); /* REG_ITMP1 = s1 + 4 * s2 */
#if !defined(ENABLE_SOFTFLOAT)
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_FLD_INTERN(d, REG_ITMP1, OFFSET(java_floatarray, data[0]));
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_ILD_INTERN(d, REG_ITMP1, OFFSET(java_floatarray, data[0]));
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP3, s1, REG_LSL(s2, 3)); /* REG_ITMP3 = s1 + 8 * s2 */
#if !defined(ENABLE_SOFTFLOAT)
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			M_DLD_INTERN(d, REG_ITMP3, OFFSET(java_doublearray, data[0]));
#else
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LLD_INTERN(d, REG_ITMP3, OFFSET(java_doublearray, data[0]));
#endif
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_AALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 2)); /* REG_ITMP1 = s1 + 4 * s2 */
			M_LDR_INTERN(d, REG_ITMP1, OFFSET(java_objectarray, data[0]));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_BASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_ADD(REG_ITMP1, s1, s2); /* REG_ITMP1 = s1 + 1 * s2 */
			M_STRB(s3, REG_ITMP1, OFFSET(java_bytearray, data[0]));
			break;

		case ICMD_CASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 1)); /* REG_ITMP1 = s1 + 2 * s2 */
			M_STRH(s3, REG_ITMP1, OFFSET(java_chararray, data[0]));
			break;

		case ICMD_SASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 1)); /* REG_ITMP1 = s1 + 2 * s2 */
			M_STRH(s3, REG_ITMP1, OFFSET(java_shortarray, data[0]));
			break;

		case ICMD_IASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 2)); /* REG_ITMP1 = s1 + 4 * s2 */
			M_IST_INTERN(s3, REG_ITMP1, OFFSET(java_intarray, data[0]));
			break;

		case ICMD_LASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP3, s1, REG_LSL(s2, 3)); /* REG_ITMP3 = s1 + 8 * s2 */
			s3 = emit_load_s3(jd, iptr, REG_ITMP12_PACKED);
			M_LST_INTERN(s3, REG_ITMP3, OFFSET(java_longarray, data[0]));
			break;

		case ICMD_FASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 2)); /* REG_ITMP1 = s1 + 4 * s2 */
#if !defined(ENABLE_SOFTFLOAT)
			s3 = emit_load_s3(jd, iptr, REG_FTMP1);
			M_FST_INTERN(s3, REG_ITMP1, OFFSET(java_floatarray, data[0]));
#else
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_IST_INTERN(s3, REG_ITMP1, OFFSET(java_floatarray, data[0]));
#endif
			break;

		case ICMD_DASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 3)); /* REG_ITMP1 = s1 + 8 * s2 */
#if !defined(ENABLE_SOFTFLOAT)
			s3 = emit_load_s3(jd, iptr, REG_FTMP1);
			M_DST_INTERN(s3, REG_ITMP1, OFFSET(java_doublearray, data[0]));
#else
			s3 = emit_load_s3(jd, iptr, REG_ITMP23_PACKED);
			M_LST_INTERN(s3, REG_ITMP1, OFFSET(java_doublearray, data[0]));
#endif
			break;

		case ICMD_AASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_A0);
			s2 = emit_load_s2(jd, iptr, REG_ITMP1);
			s3 = emit_load_s3(jd, iptr, REG_A1);

			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);

			/* move arguments to argument registers */
			M_INTMOVE(s1, REG_A0);
			M_INTMOVE(s3, REG_A1);

			/* call builtin function */
			disp = dseg_add_functionptr(cd, BUILTIN_canstore);
			M_DSEG_BRANCH(disp);

			/* recompute pv */
			s1 = (s4) (cd->mcodeptr - cd->mcodebase);
			M_RECOMPUTE_PV(s1);

			/* check resturn value of builtin */
			emit_exception_check(cd, iptr);

			/* finally store address into array */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			M_ADD(REG_ITMP1, s1, REG_LSL(s2, 2)); /* REG_ITMP1 = s1 + 4 * s2 */
			M_STR_INTERN(s3, REG_ITMP1, OFFSET(java_objectarray, data[0]));
			break;

		case ICMD_GETSTATIC:  /* ...  ==> ..., value                          */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				fieldtype = uf->fieldref->parseddesc.fd->type;

				disp = dseg_add_unique_address(cd, NULL);

				codegen_addpatchref(cd, PATCHER_get_putstatic, uf, disp);

				if (opt_showdisassemble)
					M_NOP;
			}
			else {
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;

				fieldtype = fi->type;

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class)) {
					codegen_addpatchref(cd, PATCHER_clinit, fi->class, 0);

					if (opt_showdisassemble)
						M_NOP;
				}

				disp = dseg_add_address(cd, &(fi->value));
			}

			M_DSEG_LOAD(REG_ITMP3, disp);
			switch (fieldtype) {
			case TYPE_INT:
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
#endif
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
				M_ILD_INTERN(d, REG_ITMP3, 0);
				break;
			case TYPE_LNG:
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_DBL:
#endif
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
				M_LLD_INTERN(d, REG_ITMP3, 0);
				break;
#if !defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_FLD_INTERN(d, REG_ITMP3, 0);
				break;
			case TYPE_DBL:
				d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
				M_DLD_INTERN(d, REG_ITMP3, 0);
				break;
#endif
			default:
				assert(0);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_PUTSTATIC:  /* ..., value  ==> ...                          */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				fieldtype = uf->fieldref->parseddesc.fd->type;

				disp = dseg_add_unique_address(cd, NULL);

				codegen_addpatchref(cd, PATCHER_get_putstatic, uf, disp);

				if (opt_showdisassemble)
					M_NOP;
			}
			else {
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;

				fieldtype = fi->type;

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class)) {
					codegen_addpatchref(cd, PATCHER_clinit, fi->class, 0);

					if (opt_showdisassemble)
						M_NOP;
				}

				disp = dseg_add_address(cd, &(fi->value));
			}

			M_DSEG_LOAD(REG_ITMP3, disp);
			switch (fieldtype) {
			case TYPE_INT:
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
#endif
			case TYPE_ADR:
				s1 = emit_load_s1(jd, iptr, REG_ITMP1);
				M_IST_INTERN(s1, REG_ITMP3, 0);
				break;
			case TYPE_LNG:
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_DBL:
#endif
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_LST_INTERN(s1, REG_ITMP3, 0);
				break;
#if !defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
				s1 = emit_load_s1(jd, iptr, REG_FTMP1);
				M_FST_INTERN(s1, REG_ITMP3, 0);
				break;
			case TYPE_DBL:
				s1 = emit_load_s1(jd, iptr, REG_FTMP1);
				M_DST_INTERN(s1, REG_ITMP3, 0);
				break;
#endif
			default:
				assert(0);
			}
			break;

		case ICMD_GETFIELD:   /* ..., objectref, value  ==> ...               */

			s1 = emit_load_s1(jd, iptr, REG_ITMP3);
			emit_nullpointer_check(cd, iptr, s1);


			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				fieldtype = uf->fieldref->parseddesc.fd->type;
			}
			else {
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;

				fieldtype = fi->type;
				disp      = fi->offset;
			}

#if !defined(ENABLE_SOFTFLOAT)
			/* HACK: softnull checks on floats */
			if (!INSTRUCTION_MUST_CHECK(iptr) && IS_FLT_DBL_TYPE(fieldtype))
				emit_nullpointer_check_force(cd, iptr, s1);
#endif

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				codegen_addpatchref(cd, PATCHER_get_putfield, uf, 0);

				if (opt_showdisassemble)
					M_NOP;

				disp = 0;
			}

			switch (fieldtype) {
			case TYPE_INT:
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
#endif
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
				M_ILD(d, s1, disp);
				break;
			case TYPE_LNG:
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_DBL:
#endif
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
				M_LLD(d, s1, disp);
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
			default:
				assert(0);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_PUTFIELD:   /* ..., objectref, value  ==> ...               */

			s1 = emit_load_s1(jd, iptr, REG_ITMP3);
			emit_nullpointer_check(cd, iptr, s1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				fieldtype = uf->fieldref->parseddesc.fd->type;
			}
			else {
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;

				fieldtype = fi->type;
				disp      = fi->offset;
			}

#if !defined(ENABLE_SOFTFLOAT)
			/* HACK: softnull checks on floats */
			if (!INSTRUCTION_MUST_CHECK(iptr) && IS_FLT_DBL_TYPE(fieldtype))
				emit_nullpointer_check_force(cd, iptr, s1);
#endif

			switch (fieldtype) {
			case TYPE_INT:
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
#endif
			case TYPE_ADR:
				s2 = emit_load_s2(jd, iptr, REG_ITMP1);
				break;
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_DBL: /* fall through */
#endif
			case TYPE_LNG:
				s2 = emit_load_s2(jd, iptr, REG_ITMP12_PACKED);
				break;
#if !defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
			case TYPE_DBL:
				s2 = emit_load_s2(jd, iptr, REG_FTMP1);
				break;
#endif
			default:
				assert(0);
			}

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				codegen_addpatchref(cd, PATCHER_get_putfield, uf, 0);

				if (opt_showdisassemble)
					M_NOP;

				disp = 0;
			}

			switch (fieldtype) {
			case TYPE_INT:
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
#endif
			case TYPE_ADR:
				M_IST(s2, s1, disp);
				break;
			case TYPE_LNG:
#if defined(ENABLE_SOFTFLOAT)
			case TYPE_DBL:
#endif
				M_LST(s2, s1, disp);
				break;
#if !defined(ENABLE_SOFTFLOAT)
			case TYPE_FLT:
				M_FST(s2, s1, disp);
				break;
			case TYPE_DBL:
				M_DST(s2, s1, disp);
				break;
#endif
			default:
				assert(0);
			}
			break;


		/* branch operations **************************************************/

		case ICMD_ATHROW:       /* ..., objectref ==> ... (, objectref)       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1_XPTR);
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				codegen_addpatchref(cd, PATCHER_athrow_areturn,
									iptr->sx.s23.s2.uc, 0);

				if (opt_showdisassemble)
					M_NOP;
			}
			disp = dseg_add_functionptr(cd, asm_handle_exception);
			M_DSEG_LOAD(REG_ITMP3, disp);
			M_MOV(REG_ITMP2_XPC, REG_PC);
			M_MOV(REG_PC, REG_ITMP3);
			M_NOP;              /* nop ensures that XPC is less than the end  */
			                    /* of basic block                             */
			break;

		case ICMD_GOTO:         /* ... ==> ...                                */
		case ICMD_RET:

			emit_br(cd, iptr->dst.block);
			break;

		case ICMD_JSR:          /* ... ==> ...                                */

			emit_br(cd, iptr->sx.s23.s3.jsrtarget.block);
			break;
		
		case ICMD_IFNULL:       /* ..., value ==> ...                         */
		case ICMD_IFNONNULL:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_TEQ_IMM(s1, 0);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IFNULL, BRANCH_OPT_NONE);
			break;

		case ICMD_IFLT:         /* ..., value ==> ...                         */
		case ICMD_IFLE:         /* op1 = target JavaVM pc, val.i = constant   */
		case ICMD_IFGT:
		case ICMD_IFGE:
		case ICMD_IFEQ:
		case ICMD_IFNE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_COMPARE(s1, iptr->sx.val.i);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IFEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_LEQ:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_low(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.l == 0) {
				M_ORR_S(s1, s2, REG_ITMP3);
			}
			else {
				M_COMPARE(s1, (iptr->sx.val.l >> 32));
				/*ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
				M_CMP(s1, REG_ITMP3);*/
				ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_CMPEQ(s2, REG_ITMP3);
			}
			emit_beq(cd, iptr->dst.block);
			break;

		case ICMD_IF_LLT:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_low(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.l == 0) {
				/* if high word is less than zero, the whole long is too */
				M_CMP_IMM(s1, 0);
				emit_blt(cd, iptr->dst.block);
			}
			else {
				/* high compare: x=0(ifLT) ; x=1(ifEQ) ; x=2(ifGT) */
				M_COMPARE(s1, (iptr->sx.val.l >> 32));
				/*ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
				M_CMP(s1, REG_ITMP3);*/
				M_EOR(REG_ITMP1, REG_ITMP1, REG_ITMP1);
				M_MOVGT_IMM(2, REG_ITMP1);
				M_MOVEQ_IMM(1, REG_ITMP1);

				/* low compare: x=x-1(ifLO) */
				M_COMPARE(s2, (iptr->sx.val.l & 0xffffffff));
  				/*ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_CMP(s2, REG_ITMP3);*/
				M_SUBLO_IMM(REG_ITMP1, REG_ITMP1, 1);

				/* branch if (x LT 1) */
				M_CMP_IMM(REG_ITMP1, 1);
				emit_blt(cd, iptr->dst.block);
			}
			break;

		case ICMD_IF_LLE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_low(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.l == 0) {
				/* if high word is less than zero, the whole long is too  */
				M_CMP_IMM(s1, 0);
				emit_blt(cd, iptr->dst.block);

				/* ... otherwise the low word has to be zero (tricky!) */
				M_CMPEQ_IMM(s2, 0);
				emit_beq(cd, iptr->dst.block);
			}
			else {
				/* high compare: x=0(ifLT) ; x=1(ifEQ) ; x=2(ifGT) */
				M_COMPARE(s1, (iptr->sx.val.l >> 32));
				/*ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
				M_CMP(s1, REG_ITMP3);*/
				M_EOR(REG_ITMP1, REG_ITMP1, REG_ITMP1);
				M_MOVGT_IMM(2, REG_ITMP1);
				M_MOVEQ_IMM(1, REG_ITMP1);

				/* low compare: x=x+1(ifHI) */
				M_COMPARE(s2, (iptr->sx.val.l & 0xffffffff));
  				/*ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_CMP(s2, REG_ITMP3);*/
				M_ADDHI_IMM(REG_ITMP1, REG_ITMP1, 1);

				/* branch if (x LE 1) */
				M_CMP_IMM(REG_ITMP1, 1);
				emit_ble(cd, iptr->dst.block);
			}
			break;

		case ICMD_IF_LGE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_low(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.l == 0) {
				/* if high word is greater or equal zero, the whole long is too */
				M_CMP_IMM(s1, 0);
				emit_bge(cd, iptr->dst.block);
			}
			else {
				/* high compare: x=0(ifLT) ; x=1(ifEQ) ; x=2(ifGT) */
				M_COMPARE(s1, (iptr->sx.val.l >> 32));
				/*ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
				M_CMP(s1, REG_ITMP3);*/
				M_EOR(REG_ITMP1, REG_ITMP1, REG_ITMP1);
				M_MOVGT_IMM(2, REG_ITMP1);
				M_MOVEQ_IMM(1, REG_ITMP1);

				/* low compare: x=x-1(ifLO) */
				M_COMPARE(s2, (iptr->sx.val.l & 0xffffffff));
  				/*ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_CMP(s2, REG_ITMP3);*/
				M_SUBLO_IMM(REG_ITMP1, REG_ITMP1, 1);

				/* branch if (x GE 1) */
				M_CMP_IMM(REG_ITMP1, 1);
				emit_bge(cd, iptr->dst.block);
			}
			break;

		case ICMD_IF_LGT:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_low(jd, iptr, REG_ITMP2);
#if 0
			if (iptr->sx.val.l == 0) {
				/* if high word is greater than zero, the whole long is too */
				M_CMP_IMM(s1, 0);
				M_BGT(0);
				codegen_add_branch_ref(cd, iptr->dst.block);

				/* ... or high was zero and low is non zero (tricky!) */
				M_EOR(REG_ITMP3, REG_ITMP3, REG_ITMP3);
				M_MOVLT_IMM(1, REG_ITMP3);
				M_ORR_S(REG_ITMP3, s2, REG_ITMP3);
				M_BNE(0);
				codegen_add_branch_ref(cd, iptr->dst.block);
			}
			else {
#endif
				/* high compare: x=0(ifLT) ; x=1(ifEQ) ; x=2(ifGT) */
				M_COMPARE(s1, (iptr->sx.val.l >> 32));
				/*ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
				M_CMP(s1, REG_ITMP3);*/
				M_EOR(REG_ITMP1, REG_ITMP1, REG_ITMP1);
				M_MOVGT_IMM(2, REG_ITMP1);
				M_MOVEQ_IMM(1, REG_ITMP1);

				/* low compare: x=x+1(ifHI) */
				M_COMPARE(s2, (iptr->sx.val.l & 0xffffffff));
  				/*ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_CMP(s2, REG_ITMP3);*/
				M_ADDHI_IMM(REG_ITMP1, REG_ITMP1, 1);

				/* branch if (x GT 1) */
				M_CMP_IMM(REG_ITMP1, 1);
				emit_bgt(cd, iptr->dst.block);
#if 0
			}
#endif
			break;

		case ICMD_IF_LNE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s1_low(jd, iptr, REG_ITMP2);
			if (iptr->sx.val.l == 0) {
				M_ORR_S(s1, s2, REG_ITMP3);
			}
			else {
				M_COMPARE(s1, (iptr->sx.val.l >> 32));
				/*ICONST(REG_ITMP3, iptr->sx.val.l >> 32);
				M_CMP(s1, REG_ITMP3);*/
				ICONST(REG_ITMP3, iptr->sx.val.l & 0xffffffff);
				M_CMPEQ(s2, REG_ITMP3);
			}
			emit_bne(cd, iptr->dst.block);
			break;
			
		case ICMD_IF_ICMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ICMPNE:
		case ICMD_IF_ICMPLT:
		case ICMD_IF_ICMPLE:
		case ICMD_IF_ICMPGT:
		case ICMD_IF_ICMPGE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_ICMPEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_ACMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ACMPNE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_ACMPEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_LCMPEQ:    /* ..., value, value ==> ...                  */
		                        /* op1 = target JavaVM pc                     */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMPEQ(s1, s2);

			emit_beq(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPNE:    /* ..., value, value ==> ...                  */
		                        /* op1 = target JavaVM pc                     */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMPEQ(s1, s2);

			emit_bne(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPLT:    /* ..., value, value ==> ...                  */
		                        /* op1 = target JavaVM pc                     */

			/* high compare: x=0(ifLT) ; x=1(ifEQ) ; x=2(ifGT) */
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			M_EOR(REG_ITMP3, REG_ITMP3, REG_ITMP3);
			M_MOVGT_IMM(2, REG_ITMP3);
			M_MOVEQ_IMM(1, REG_ITMP3);

			/* low compare: x=x-1(ifLO) */
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			M_SUBLO_IMM(REG_ITMP3, REG_ITMP3, 1);

			/* branch if (x LT 1) */
			M_CMP_IMM(REG_ITMP3, 1);
			emit_blt(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPLE:    /* ..., value, value ==> ...                  */
		                        /* op1 = target JavaVM pc                     */

			/* high compare: x=0(ifLT) ; x=1(ifEQ) ; x=2(ifGT) */
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			M_EOR(REG_ITMP3, REG_ITMP3, REG_ITMP3);
			M_MOVGT_IMM(2, REG_ITMP3);
			M_MOVEQ_IMM(1, REG_ITMP3);

			/* low compare: x=x-1(ifLO) */
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			M_ADDHI_IMM(REG_ITMP3, REG_ITMP3, 1);

			/* branch if (x LE 1) */
			M_CMP_IMM(REG_ITMP3, 1);
			emit_ble(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPGT:    /* ..., value, value ==> ...                  */
		                        /* op1 = target JavaVM pc                     */

			/* high compare: x=0(ifLT) ; x=1(ifEQ) ; x=2(ifGT) */
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			M_EOR(REG_ITMP3, REG_ITMP3, REG_ITMP3);
			M_MOVGT_IMM(2, REG_ITMP3);
			M_MOVEQ_IMM(1, REG_ITMP3);

			/* low compare: x=x-1(ifLO) */
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			M_ADDHI_IMM(REG_ITMP3, REG_ITMP3, 1);

			/* branch if (x GT 1) */
			M_CMP_IMM(REG_ITMP3, 1);
			emit_bgt(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPGE:    /* ..., value, value ==> ...                  */
		                        /* op1 = target JavaVM pc                     */

			/* high compare: x=0(ifLT) ; x=1(ifEQ) ; x=2(ifGT) */
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			M_EOR(REG_ITMP3, REG_ITMP3, REG_ITMP3);
			M_MOVGT_IMM(2, REG_ITMP3);
			M_MOVEQ_IMM(1, REG_ITMP3);

			/* low compare: x=x-1(ifLO) */
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_CMP(s1, s2);
			M_SUBLO_IMM(REG_ITMP3, REG_ITMP3, 1);

			/* branch if (x GE 1) */
			M_CMP_IMM(REG_ITMP3, 1);
			emit_bge(cd, iptr->dst.block);
			break;

		case ICMD_TABLESWITCH:  /* ..., index ==> ...                         */
			{
			s4 i, l;
			branch_target_t *table;

			table = iptr->dst.table;

			l = iptr->sx.s23.s2.tablelow;
			i = iptr->sx.s23.s3.tablehigh;

			/* calculate new index (index - low) */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			if (l  == 0) {
				M_INTMOVE(s1, REG_ITMP1);
			} else if (IS_IMM(l)) {
				M_SUB_IMM(REG_ITMP1, s1, l);
			} else {
				ICONST(REG_ITMP2, l);
				M_SUB(REG_ITMP1, s1, REG_ITMP2);
			}

			/* range check (index <= high-low) */
			i = i - l + 1;
			M_COMPARE(REG_ITMP1, i-1);
			emit_bugt(cd, table[0].block);

			/* build jump table top down and use address of lowest entry */

			table += i;

			while (--i >= 0) {
				dseg_add_target(cd, table->block);
				--table;
			}
			}

			/* length of dataseg after last dseg_add_target is used by load */
			/* TODO: this loads from data-segment */
			M_ADD(REG_ITMP2, REG_PV, REG_LSL(REG_ITMP1, 2));
			M_LDR(REG_PC, REG_ITMP2, -(cd->dseglen));
			break;

		case ICMD_LOOKUPSWITCH: /* ..., key ==> ...                           */
			{
			s4 i;
			lookup_target_t *lookup;

			lookup = iptr->dst.lookup;

			i = iptr->sx.s23.s2.lookupcount;
			
			/* compare keys */
			MCODECHECK((i<<2)+8);
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

			while (--i >= 0) {
				M_COMPARE(s1, lookup->value);
				emit_beq(cd, lookup->target.block);
				lookup++;
			}

			/* default branch */
			emit_br(cd, iptr->sx.s23.s3.lookupdefault.block);
			}
			break;

		case ICMD_FRETURN:      /* ..., retvalue ==> ...                      */

#if !defined(ENABLE_SOFTFLOAT)
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			M_CAST_FLT_TO_INT_TYPED(VAROP(iptr->s1)->type, s1, REG_RESULT);
			goto ICMD_RETURN_do;
#endif

		case ICMD_IRETURN:      /* ..., retvalue ==> ...                      */

			s1 = emit_load_s1(jd, iptr, REG_RESULT);
			M_INTMOVE(s1, REG_RESULT);
			goto ICMD_RETURN_do;

		case ICMD_DRETURN:      /* ..., retvalue ==> ...                      */

#if !defined(ENABLE_SOFTFLOAT)
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			M_CAST_FLT_TO_INT_TYPED(VAROP(iptr->s1)->type, s1, REG_RESULT_PACKED);
			goto ICMD_RETURN_do;
#endif

		case ICMD_LRETURN:      /* ..., retvalue ==> ...                      */

			s1 = emit_load_s1(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(s1, REG_RESULT_PACKED);
			goto ICMD_RETURN_do;

		case ICMD_ARETURN:      /* ..., retvalue ==> ...                      */

			s1 = emit_load_s1(jd, iptr, REG_RESULT);
			M_INTMOVE(s1, REG_RESULT);
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				codegen_addpatchref(cd, PATCHER_athrow_areturn,
									iptr->sx.s23.s2.uc, 0);

				if (opt_showdisassemble)
					M_NOP;
			}
			goto ICMD_RETURN_do;

		case ICMD_RETURN:       /* ...  ==> ...                               */
			ICMD_RETURN_do:

#if !defined(NDEBUG)
			if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
				emit_verbosecall_exit(jd);
#endif

#if defined(ENABLE_THREADS)
			/* call monitorexit function */

			if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
				/* stack offset for monitor argument */

				s1 = rd->memuse;

				/* we need to save the proper return value */

				switch (iptr->opc) {
				case ICMD_IRETURN:
				case ICMD_ARETURN:
				case ICMD_LRETURN:
				case ICMD_FRETURN: /* XXX TWISTI: is that correct? */
				case ICMD_DRETURN:
					M_STMFD(BITMASK_RESULT, REG_SP);
					s1 += 2;
					break;
				}

				M_LDR(REG_A0, REG_SP, s1 * 4);
				disp = dseg_add_functionptr(cd, LOCK_monitor_exit);
				M_DSEG_BRANCH(disp);

				/* we no longer need PV here, no more loading */
				/*s1 = (s4) (cd->mcodeptr - cd->mcodebase);
				M_RECOMPUTE_PV(s1);*/

				switch (iptr->opc) {
				case ICMD_IRETURN:
				case ICMD_ARETURN:
				case ICMD_LRETURN:
				case ICMD_FRETURN: /* XXX TWISTI: is that correct? */
				case ICMD_DRETURN:
					M_LDMFD(BITMASK_RESULT, REG_SP);
					break;
				}
			}
#endif

			/* deallocate stackframe for spilled variables */

			if ((cd->stackframesize - savedregs_num) > 0)
				M_ADD_IMM_EXT_MUL4(REG_SP, REG_SP, cd->stackframesize - savedregs_num);

			/* restore callee saved registers + do return */

			if (savedregs_bitmask) {
				if (!jd->isleafmethod) {
					savedregs_bitmask &= ~(1<<REG_LR);
					savedregs_bitmask |= (1<<REG_PC);
				}
				M_LDMFD(savedregs_bitmask, REG_SP);
			}

			/* if LR was not on stack, we need to return manually */

			if (jd->isleafmethod)
				M_MOV(REG_PC, REG_LR);
			break;

		case ICMD_BUILTIN:      /* ..., arg1, arg2, arg3 ==> ...              */

			bte = iptr->sx.s23.s3.bte;
			md  = bte->md;
			goto ICMD_INVOKE_do;

		case ICMD_INVOKESTATIC: /* ..., [arg1, [arg2 ...]] ==> ...            */
		case ICMD_INVOKESPECIAL:/* ..., objectref, [arg1, [arg2 ...]] ==> ... */
		case ICMD_INVOKEVIRTUAL:/* op1 = arg count, val.a = method pointer    */
		case ICMD_INVOKEINTERFACE:

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

		ICMD_INVOKE_do:
			/* copy arguments to registers or stack location */

			s3 = md->paramcount;

			MCODECHECK((s3 << 1) + 64);

			for (s3 = s3 - 1; s3 >= 0; s3--) {
				var = VAR(iptr->sx.s23.s2.args[s3]);
				d   = md->params[s3].regoff;

				if (var->flags & PREALLOC) /* argument was precolored? */
					continue;

				/* TODO: document me */
#if !defined(ENABLE_SOFTFLOAT)
				if (IS_INT_LNG_TYPE(var->type)) {
#endif /* !defined(ENABLE_SOFTFLOAT) */
					if (!md->params[s3].inmemory) {
						SPLIT_OPEN(var->type, s1, REG_ITMP2);
						s1 = emit_load(jd, iptr, var, d);

						if (IS_2_WORD_TYPE(var->type))
							M_LNGMOVE(s1, d);
						else
							M_INTMOVE(s1, d);

						SPLIT_STORE_AND_CLOSE(var->type, d, 0);
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
#if !defined(ENABLE_SOFTFLOAT)
				}
				else {
					if (!md->params[s3].inmemory) {
						s1 = emit_load(jd, iptr, var, REG_FTMP1);
						SPLIT_OPEN(var->type, d, REG_ITMP1);
						M_CAST_FLT_TO_INT_TYPED(var->type, s1, d);
						SPLIT_STORE_AND_CLOSE(var->type, d, 0);
					}
					else {
						s1 = emit_load(jd, iptr, var, REG_FTMP1);
						if (IS_2_WORD_TYPE(var->type))
							M_DST(s1, REG_SP, d * 4);
						else
							M_FST(s1, REG_SP, d * 4);
					}
				}
#endif /* !defined(ENABLE_SOFTFLOAT) */
			}

			switch (iptr->opc) {
			case ICMD_BUILTIN:
				disp = dseg_add_functionptr(cd, bte->fp);

				M_DSEG_LOAD(REG_PV, disp); /* pointer to built-in-function */

				/* generate the actual call */

				M_MOV(REG_LR, REG_PC);
				M_MOV(REG_PC, REG_PV);
				s1 = (s4) (cd->mcodeptr - cd->mcodebase);
				M_RECOMPUTE_PV(s1);

				emit_exception_check(cd, iptr);
				break;

			case ICMD_INVOKESPECIAL:
				emit_nullpointer_check(cd, iptr, REG_A0);
				/* fall through */

			case ICMD_INVOKESTATIC:
				if (lm == NULL) {
					disp = dseg_add_unique_address(cd, NULL);

					codegen_addpatchref(cd, PATCHER_invokestatic_special,
										um, disp);

					if (opt_showdisassemble)
						M_NOP;
				}
				else
					disp = dseg_add_address(cd, lm->stubroutine);

				M_DSEG_LOAD(REG_PV, disp);            /* Pointer to method */

				/* generate the actual call */

				M_MOV(REG_LR, REG_PC);
				M_MOV(REG_PC, REG_PV);
				s1 = (s4) (cd->mcodeptr - cd->mcodebase);
				M_RECOMPUTE_PV(s1);
				break;

			case ICMD_INVOKEVIRTUAL:
				if (lm == NULL) {
					codegen_addpatchref(cd, PATCHER_invokevirtual, um, 0);

					if (opt_showdisassemble)
						M_NOP;

					s1 = 0;
				}
				else
					s1 = OFFSET(vftbl_t, table[0]) +
						sizeof(methodptr) * lm->vftblindex;

				/* implicit null-pointer check */
				M_LDR_INTERN(REG_METHODPTR, REG_A0,
							 OFFSET(java_objectheader, vftbl));
				M_LDR_INTERN(REG_PV, REG_METHODPTR, s1);

				/* generate the actual call */

				M_MOV(REG_LR, REG_PC);
				M_MOV(REG_PC, REG_PV);
				s1 = (s4) (cd->mcodeptr - cd->mcodebase);
				M_RECOMPUTE_PV(s1);
				break;

			case ICMD_INVOKEINTERFACE:
				if (lm == NULL) {
					codegen_addpatchref(cd, PATCHER_invokeinterface, um, 0);

					if (opt_showdisassemble)
						M_NOP;

					s1 = 0;
					s2 = 0;
				}
				else {
					s1 = OFFSET(vftbl_t, interfacetable[0]) -
						sizeof(methodptr*) * lm->class->index;
					s2 = sizeof(methodptr) * (lm - lm->class->methods);
				}

				/* implicit null-pointer check */
				M_LDR_INTERN(REG_METHODPTR, REG_A0,
							 OFFSET(java_objectheader, vftbl));
				M_LDR_INTERN(REG_METHODPTR, REG_METHODPTR, s1);
				M_LDR_INTERN(REG_PV, REG_METHODPTR, s2);

				/* generate the actual call */

				M_MOV(REG_LR, REG_PC);
				M_MOV(REG_PC, REG_PV);
				s1 = (s4) (cd->mcodeptr - cd->mcodebase);
				M_RECOMPUTE_PV(s1);
				break;
			}

			/* store return value */

			d = md->returntype.type;

#if !defined(__SOFTFP__)
			/* TODO: this is only a hack, since we use R0/R1 for float
			   return!  this depends on gcc; it is independent from
			   our ENABLE_SOFTFLOAT define */
			if (iptr->opc == ICMD_BUILTIN && d != TYPE_VOID && IS_FLT_DBL_TYPE(d)) {
#if 0 && !defined(NDEBUG)
				dolog("BUILTIN that returns float or double (%s.%s)", m->class->name->text, m->name->text);
#endif
				/* we cannot use this macro, since it is not defined
				   in ENABLE_SOFTFLOAT M_CAST_FLT_TO_INT_TYPED(d,
				   REG_FRESULT, REG_RESULT_TYPED(d)); */
				if (IS_2_WORD_TYPE(d)) {
					DCD(0xed2d8102); /* stfd    f0, [sp, #-8]! */
					M_LDRD_UPDATE(REG_RESULT_PACKED, REG_SP, 8);
				} else {
					DCD(0xed2d0101); /* stfs    f0, [sp, #-4]!*/
					M_LDR_UPDATE(REG_RESULT, REG_SP, 4);
				}
			}
#endif

			if (d != TYPE_VOID) {
#if !defined(ENABLE_SOFTFLOAT)
				if (IS_INT_LNG_TYPE(d)) {
#endif /* !defined(ENABLE_SOFTFLOAT) */
					if (IS_2_WORD_TYPE(d)) {
						s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);
						M_LNGMOVE(REG_RESULT_PACKED, s1);
					}
					else {
						s1 = codegen_reg_of_dst(jd, iptr, REG_RESULT);
						M_INTMOVE(REG_RESULT, s1);
					}

#if !defined(ENABLE_SOFTFLOAT)
				} else {
					s1 = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
					M_CAST_INT_TO_FLT_TYPED(VAROP(iptr->dst)->type, REG_RESULT_TYPED(VAROP(iptr->dst)->type), s1);
				}
#endif /* !defined(ENABLE_SOFTFLOAT) */

				emit_store_dst(jd, iptr, s1);
			}
			break;

		case ICMD_CHECKCAST:  /* ..., objectref ==> ..., objectref            */
		                      /* val.a: (classinfo*) superclass               */

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

#if defined(ENABLE_THREADS)
			codegen_threadcritrestart(cd, cd->mcodeptr - cd->mcodebase);
#endif
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);

			/* if class is not resolved, check which code to call */

			if (super == NULL) {
				M_TST(s1, s1);
				emit_label_beq(cd, BRANCH_LABEL_1);

				disp = dseg_add_unique_s4(cd, 0); /* super->flags */
				codegen_addpatchref(cd, PATCHER_checkcast_instanceof_flags,
				                    iptr->sx.s23.s3.c.ref, disp);

				if (opt_showdisassemble)
					M_NOP;

				M_DSEG_LOAD(REG_ITMP2, disp);
				disp = dseg_add_s4(cd, ACC_INTERFACE);
				M_DSEG_LOAD(REG_ITMP3, disp);
				M_TST(REG_ITMP2, REG_ITMP3);
				emit_label_beq(cd, BRANCH_LABEL_2);
			}

			/* interface checkcast code */

			if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
				if (super == NULL) {
					codegen_addpatchref(cd,
					                    PATCHER_checkcast_instanceof_interface,
					                    iptr->sx.s23.s3.c.ref, 0);

					if (opt_showdisassemble)
						M_NOP;
				}
				else {
					M_TST(s1, s1);
					emit_label_beq(cd, BRANCH_LABEL_3);
				}

				M_LDR_INTERN(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));
				M_LDR_INTERN(REG_ITMP3, REG_ITMP2, OFFSET(vftbl_t, interfacetablelength));
				assert(IS_IMM(superindex));
				M_CMP_IMM(REG_ITMP3, superindex);
				emit_classcast_check(cd, iptr, BRANCH_LE, REG_ITMP3, s1);

				s2 = OFFSET(vftbl_t, interfacetable[0]) -
					superindex * sizeof(methodptr*);

				M_LDR_INTERN(REG_ITMP3, REG_ITMP2, s2);
				M_TST(REG_ITMP3, REG_ITMP3);
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

					codegen_addpatchref(cd, PATCHER_checkcast_instanceof_class,
					                    iptr->sx.s23.s3.c.ref,
										disp);

					if (opt_showdisassemble)
						M_NOP;
				}
				else {
					disp = dseg_add_address(cd, super->vftbl);

					M_TST(s1, s1);
					emit_label_beq(cd, BRANCH_LABEL_5);
				}

				M_LDR_INTERN(REG_ITMP2, s1, OFFSET(java_objectheader, vftbl));
				M_DSEG_LOAD(REG_ITMP3, disp);
#if defined(ENABLE_THREADS)
				codegen_threadcritstart(cd, cd->mcodeptr - cd->mcodebase);
#endif
				M_LDR_INTERN(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, baseval));
				M_LDR_INTERN(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, baseval));
				M_SUB(REG_ITMP2, REG_ITMP2, REG_ITMP3);
				M_DSEG_LOAD(REG_ITMP3, disp);
				M_LDR_INTERN(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, diffval));
#if defined(ENABLE_THREADS)
				codegen_threadcritstop(cd, cd->mcodeptr - cd->mcodebase);
#endif
				M_CMP(REG_ITMP2, REG_ITMP3);
				emit_classcast_check(cd, iptr, BRANCH_UGT, 0, s1);

				if (super != NULL)
					emit_label(cd, BRANCH_LABEL_5);
			}

			if (super == NULL) {
				emit_label(cd, BRANCH_LABEL_1);
				emit_label(cd, BRANCH_LABEL_4);
			}

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			}
			else {
				/* array type cast-check */

				s1 = emit_load_s1(jd, iptr, REG_A0);
				M_INTMOVE(s1, REG_A0);

				if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
					disp = dseg_add_unique_address(cd, NULL);

					codegen_addpatchref(cd, PATCHER_builtin_arraycheckcast,
										iptr->sx.s23.s3.c.ref,
										disp);

					if (opt_showdisassemble)
						M_NOP;
				}
				else
					disp = dseg_add_address(cd, iptr->sx.s23.s3.c.cls);

				M_DSEG_LOAD(REG_A1, disp);
				disp = dseg_add_functionptr(cd, BUILTIN_arraycheckcast);
				M_DSEG_BRANCH(disp);

				/* recompute pv */
				disp = (s4) (cd->mcodeptr - cd->mcodebase);
				M_RECOMPUTE_PV(disp);

				s1 = emit_load_s1(jd, iptr, REG_ITMP1);
				M_TST(REG_RESULT, REG_RESULT);
				emit_classcast_check(cd, iptr, BRANCH_EQ, REG_RESULT, s1);

				d = codegen_reg_of_dst(jd, iptr, s1);
			}

			M_INTMOVE(s1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_INSTANCEOF: /* ..., objectref ==> ..., intresult            */
		                      /* val.a: (classinfo*) superclass               */
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

#if defined(ENABLE_THREADS)
			codegen_threadcritrestart(cd, cd->mcodeptr - cd->mcodebase);
#endif
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s1 == d) {
				M_MOV(REG_ITMP1, s1);
				s1 = REG_ITMP1;
			}

			/* if class is not resolved, check which code to call */

			if (super == NULL) {
				M_EOR(d, d, d);

				M_TST(s1, s1);
				emit_label_beq(cd, BRANCH_LABEL_1);

				disp = dseg_add_unique_s4(cd, 0); /* super->flags */
				codegen_addpatchref(cd, PATCHER_checkcast_instanceof_flags,
				                    iptr->sx.s23.s3.c.ref, disp);

				if (opt_showdisassemble)
					M_NOP;

				M_DSEG_LOAD(REG_ITMP2, disp);
				disp = dseg_add_s4(cd, ACC_INTERFACE);
				M_DSEG_LOAD(REG_ITMP3, disp);
				M_TST(REG_ITMP2, REG_ITMP3);
				emit_label_beq(cd, BRANCH_LABEL_2);
			}

			/* interface checkcast code */

			if ((super == NULL) || (super->flags & ACC_INTERFACE)) {
				if (super == NULL) {
					/* If d == REG_ITMP2, then it's destroyed in check
					   code above.  */
					if (d == REG_ITMP2)
						M_EOR(d, d, d);

					codegen_addpatchref(cd,
					                    PATCHER_checkcast_instanceof_interface,
					                    iptr->sx.s23.s3.c.ref, 0);

					if (opt_showdisassemble)
						M_NOP;
				}
				else {
					M_EOR(d, d, d);
					M_TST(s1, s1);
					emit_label_beq(cd, BRANCH_LABEL_3);
				}

				M_LDR_INTERN(REG_ITMP1, s1, OFFSET(java_objectheader, vftbl));
				M_LDR_INTERN(REG_ITMP3,
							 REG_ITMP1, OFFSET(vftbl_t, interfacetablelength));
				assert(IS_IMM(superindex));
				M_CMP_IMM(REG_ITMP3, superindex);
				M_BLE(2);

				s2 = OFFSET(vftbl_t, interfacetable[0]) -
					superindex * sizeof(methodptr*);

				M_LDR_INTERN(REG_ITMP3, REG_ITMP1, s2);
				M_TST(REG_ITMP3, REG_ITMP3);
				M_MOVNE_IMM(1, d);

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

					codegen_addpatchref(cd, PATCHER_checkcast_instanceof_class,
					                    iptr->sx.s23.s3.c.ref,
										disp);

					if (opt_showdisassemble)
						M_NOP;
				}
				else {
					disp = dseg_add_address(cd, super->vftbl);

					M_EOR(d, d, d);
					M_TST(s1, s1);
					emit_label_beq(cd, BRANCH_LABEL_5);
				}

				M_LDR_INTERN(REG_ITMP1, s1, OFFSET(java_objectheader, vftbl));
				M_DSEG_LOAD(REG_ITMP2, disp);
#if defined(ENABLE_THREADS)
				codegen_threadcritstart(cd, cd->mcodeptr - cd->mcodebase);
#endif
				M_LDR_INTERN(REG_ITMP1, REG_ITMP1, OFFSET(vftbl_t, baseval));
				M_LDR_INTERN(REG_ITMP3, REG_ITMP2, OFFSET(vftbl_t, baseval));
				M_LDR_INTERN(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, diffval));
#if defined(ENABLE_THREADS)
				codegen_threadcritstop(cd, cd->mcodeptr - cd->mcodebase);
#endif
				M_SUB(REG_ITMP1, REG_ITMP1, REG_ITMP3);
				M_CMP(REG_ITMP1, REG_ITMP2);
				/* If d == REG_ITMP2, then it's destroyed */
				if (d == REG_ITMP2)
					M_EOR(d, d, d);
				M_MOVLS_IMM(1, d);

				if (super != NULL)
					emit_label(cd, BRANCH_LABEL_5);
			}

			if (super == NULL) {
				emit_label(cd, BRANCH_LABEL_1);
				emit_label(cd, BRANCH_LABEL_4);
			}

			}

			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_MULTIANEWARRAY:/* ..., cnt1, [cnt2, ...] ==> ..., arrayref  */

			/* copy sizes to stack if necessary  */

			MCODECHECK((iptr->s1.argcount << 1) + 64);

			for (s1 = iptr->s1.argcount; --s1 >= 0; ) {

				var = VAR(iptr->sx.s23.s2.args[s1]);
	
				/* copy SAVEDVAR sizes to stack */

				if (!(var->flags & PREALLOC)) {
					s2 = emit_load(jd, iptr, var, REG_ITMP1);
					M_STR(s2, REG_SP, s1 * 4);
				}
			}

			/* a0 = dimension count */

			assert(IS_IMM(iptr->s1.argcount));
			M_MOV_IMM(REG_A0, iptr->s1.argcount);

			/* is patcher function set? */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				disp = dseg_add_unique_address(cd, NULL);

				codegen_addpatchref(cd, PATCHER_builtin_multianewarray,
									iptr->sx.s23.s3.c.ref, disp);

				if (opt_showdisassemble)
					M_NOP;
			}
			else
				disp = dseg_add_address(cd, iptr->sx.s23.s3.c.cls);

			/* a1 = arraydescriptor */

			M_DSEG_LOAD(REG_A1, disp);

			/* a2 = pointer to dimensions = stack pointer */

			M_INTMOVE(REG_SP, REG_A2);

			/* call builtin_multianewarray here */

			disp = dseg_add_functionptr(cd, BUILTIN_multianewarray);
			M_DSEG_BRANCH(disp);

			/* recompute pv */

			s1 = (s4) (cd->mcodeptr - cd->mcodebase);
			M_RECOMPUTE_PV(s1);

			/* check for exception before result assignment */

			emit_exception_check(cd, iptr);

			/* get arrayref */

			d = codegen_reg_of_dst(jd, iptr, REG_RESULT);
			M_INTMOVE(REG_RESULT, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_CHECKNULL:  /* ..., objectref  ==> ..., objectref           */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			emit_nullpointer_check(cd, iptr, s1);
			break;

		default:
			exceptions_throw_internalerror("Unknown ICMD %d during code generation",
										   iptr->opc);
			return false;
		} /* the big switch */

		} /* for all instructions */

	} /* for all basic blocks */

	dseg_createlinenumbertable(cd);


	/* generate stubs */

	emit_patcher_stubs(jd);

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

	M_LDR_INTERN(REG_ITMP1, REG_PC, -(2 * 4 + 2 * SIZEOF_VOID_P));
	M_LDR_INTERN(REG_PC, REG_PC, -(3 * 4 + 3 * SIZEOF_VOID_P));
}


/* codegen_emit_stub_native ****************************************************

   Emits a stub routine which calls a native method.

*******************************************************************************/

void codegen_emit_stub_native(jitdata *jd, methoddesc *nmd, functionptr f)
{
	methodinfo  *m;
	codeinfo    *code;
	codegendata *cd;
	s4           nativeparams;
	methoddesc  *md;
	s4           i, j;
	s4           t;
	s4           disp, funcdisp, s1, s2;

	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;

	/* initialize variables */

	md = m->parseddesc;
	nativeparams = (m->flags & ACC_STATIC) ? 2 : 1;

	/* calculate stackframe size */

	cd->stackframesize = 
		1 +                                                /* return address  */
		sizeof(stackframeinfo) / SIZEOF_VOID_P +           /* stackframeinfo  */
		sizeof(localref_table) / SIZEOF_VOID_P +           /* localref_table  */
		nmd->memuse;                                       /* stack arguments */

	/* align stack to 8-byte */

	cd->stackframesize = (cd->stackframesize + 1) & ~1;

	/* create method header */

	(void) dseg_add_unique_address(cd, code);              /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 4); /* FrameSize       */
	(void) dseg_add_unique_s4(cd, 0);                      /* IsSync          */
	(void) dseg_add_unique_s4(cd, 0);                      /* IsLeaf          */
	(void) dseg_add_unique_s4(cd, 0);                      /* IntSave         */
	(void) dseg_add_unique_s4(cd, 0);                      /* FltSave         */
	(void) dseg_addlinenumbertablesize(cd);
	(void) dseg_add_unique_s4(cd, 0);                      /* ExTableSize     */

	/* generate stub code */

	M_STMFD(1<<REG_LR, REG_SP);
	M_SUB_IMM_EXT_MUL4(REG_SP, REG_SP, cd->stackframesize - 1);

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_enter(jd);
#endif

	/* get function address (this must happen before the stackframeinfo) */

	funcdisp = dseg_add_functionptr(cd, f);

#if !defined(WITH_STATIC_CLASSPATH)
	if (f == NULL) {
		codegen_addpatchref(cd, PATCHER_resolve_native, m, funcdisp);

		if (opt_showdisassemble)
			M_NOP;
	}
#endif

	/* Save integer and float argument registers (these are 4
	   registers, stack is 8-byte aligned). */

	M_STMFD(BITMASK_ARGS, REG_SP);
	/* TODO: floating point */

	/* create native stackframe info */

	assert(IS_IMM(4*4 + cd->stackframesize * 4));
	M_ADD_IMM(REG_A0, REG_SP, 4*4 + cd->stackframesize * 4 - SIZEOF_VOID_P);
	M_MOV(REG_A1, REG_PV);
	M_ADD_IMM(REG_A2, REG_SP, 4*4 + cd->stackframesize * 4);
	M_LDR_INTERN(REG_A3, REG_SP, 4*4 + cd->stackframesize * 4 - SIZEOF_VOID_P);
	disp = dseg_add_functionptr(cd, codegen_start_native_call);
	M_DSEG_BRANCH(disp);

	/* recompute pv */

	s1 = (s4) (cd->mcodeptr - cd->mcodebase);
	M_RECOMPUTE_PV(s1);

	/* Restore integer and float argument registers (these are 4
	   registers, stack is 8-byte aligned). */

	M_LDMFD(BITMASK_ARGS, REG_SP);
	/* TODO: floating point */

	/* copy or spill arguments to new locations */
	/* ATTENTION: the ARM has only integer argument registers! */

	for (i = md->paramcount - 1, j = i + nativeparams; i >= 0; i--, j--) {
		t = md->paramtypes[i].type;

		if (!md->params[i].inmemory) {
			s1 = md->params[i].regoff;
			s2 = nmd->params[j].regoff;

			if (!nmd->params[j].inmemory) {
#if !defined(__ARM_EABI__)
				SPLIT_OPEN(t, s1, REG_ITMP1);
				SPLIT_LOAD(t, s1, cd->stackframesize);
				SPLIT_OPEN(t, s2, REG_ITMP1);
#endif

				if (IS_2_WORD_TYPE(t))
					M_LNGMOVE(s1, s2);
				else
					M_INTMOVE(s1, s2);

#if !defined(__ARM_EABI__)
				SPLIT_STORE_AND_CLOSE(t, s2, 0);
#endif
			}
			else {
#if !defined(__ARM_EABI__)
				SPLIT_OPEN(t, s1, REG_ITMP1);
				SPLIT_LOAD(t, s1, cd->stackframesize);
#endif

				if (IS_2_WORD_TYPE(t))
					M_LST(s1, REG_SP, s2 * 4);
				else
					M_IST(s1, REG_SP, s2 * 4);
				/* no SPLIT_CLOSE here because argument is fully on stack now */
			}
		}
		else {
			s1 = md->params[i].regoff + cd->stackframesize;
			s2 = nmd->params[j].regoff;

			if (IS_2_WORD_TYPE(t)) {
				M_LLD(REG_ITMP12_PACKED, REG_SP, s1 * 4);
				M_LST(REG_ITMP12_PACKED, REG_SP, s2 * 4);
			}
			else {
				M_ILD(REG_ITMP1, REG_SP, s1 * 4);
				M_IST(REG_ITMP1, REG_SP, s2 * 4);
			}
		}
	}

	/* put class into second argument register */

	if (m->flags & ACC_STATIC) {
		disp = dseg_add_address(cd, m->class);
		M_DSEG_LOAD(REG_A1, disp);
	}

	/* put env into first argument register */

	disp = dseg_add_address(cd, _Jv_env);
	M_DSEG_LOAD(REG_A0, disp);

	/* do the native function call */

	M_DSEG_BRANCH(funcdisp);

	/* recompute pv */
	/* TODO: this is only needed because of the tracer ... do we
	   really need it? */

	s1 = (s4) (cd->mcodeptr - cd->mcodebase);
	M_RECOMPUTE_PV(s1);

#if !defined(__SOFTFP__)
	/* TODO: this is only a hack, since we use R0/R1 for float return! */
	/* this depends on gcc; it is independent from our ENABLE_SOFTFLOAT define */
	if (md->returntype.type != TYPE_VOID && IS_FLT_DBL_TYPE(md->returntype.type)) {
#if 0 && !defined(NDEBUG)
		dolog("NATIVESTUB that returns float or double (%s.%s)", m->class->name->text, m->name->text);
#endif
		/* we cannot use this macro, since it is not defined in ENABLE_SOFTFLOAT */
		/* M_CAST_FLT_TO_INT_TYPED(md->returntype.type, REG_FRESULT, REG_RESULT_TYPED(md->returntype.type)); */
		if (IS_2_WORD_TYPE(md->returntype.type)) {
			DCD(0xed2d8102); /* stfd    f0, [sp, #-8]! */
			M_LDRD_UPDATE(REG_RESULT_PACKED, REG_SP, 8);
		} else {
			DCD(0xed2d0101); /* stfs    f0, [sp, #-4]!*/
			M_LDR_UPDATE(REG_RESULT, REG_SP, 4);
		}
	}
#endif

#if !defined(NDEBUG)
	if (JITDATA_HAS_FLAG_VERBOSECALL(jd))
		emit_verbosecall_exit(jd);
#endif

	/* remove native stackframe info */
	/* TODO: improve this store/load */

	M_STMFD(BITMASK_RESULT, REG_SP);

	M_ADD_IMM(REG_A0, REG_SP, 2*4 + cd->stackframesize * 4 - SIZEOF_VOID_P);
	disp = dseg_add_functionptr(cd, codegen_finish_native_call);
	M_DSEG_BRANCH(disp);
	s1 = (s4) (cd->mcodeptr - cd->mcodebase);
	M_RECOMPUTE_PV(s1);

	M_MOV(REG_ITMP1_XPTR, REG_RESULT);
	M_LDMFD(BITMASK_RESULT, REG_SP);

	/* finish stub code, but do not yet return to caller */

	M_ADD_IMM_EXT_MUL4(REG_SP, REG_SP, cd->stackframesize - 1);
	M_LDMFD(1<<REG_LR, REG_SP);

	/* check for exception */

	M_TST(REG_ITMP1_XPTR, REG_ITMP1_XPTR);
	M_MOVEQ(REG_LR, REG_PC);            /* if no exception, return to caller  */

	/* handle exception here */

	M_SUB_IMM(REG_ITMP2_XPC, REG_LR, 4);/* move fault address into xpc        */

	disp = dseg_add_functionptr(cd, asm_handle_nat_exception);
	M_DSEG_LOAD(REG_ITMP3, disp);       /* load asm exception handler address */
	M_MOV(REG_PC, REG_ITMP3);           /* jump to asm exception handler      */

	/* generate patcher stubs */

	emit_patcher_stubs(jd);
}


/* asm_debug *******************************************************************

   Lazy debugger!

*******************************************************************************/

void asm_debug(int a1, int a2, int a3, int a4)
{
	printf("===> i am going to exit after this debugging message!\n");
	printf("got asm_debug(%p, %p, %p, %p)\n",(void*)a1,(void*)a2,(void*)a3,(void*)a4);
	vm_abort("leave you now");
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

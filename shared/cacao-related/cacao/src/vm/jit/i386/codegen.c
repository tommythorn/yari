/* src/vm/jit/i386/codegen.c - machine code generator for i386

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

   $Id: codegen.c 7865 2007-05-03 21:29:40Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>

#include "vm/types.h"

#include "vm/jit/i386/md-abi.h"

#include "vm/jit/i386/codegen.h"
#include "vm/jit/i386/emit.h"

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

#if defined(ENABLE_SSA)
# include "vm/jit/optimizing/lsra.h"
# include "vm/jit/optimizing/ssa.h"
#elif defined(ENABLE_LSRA)
# include "vm/jit/allocator/lsra.h"
#endif

#include "vmcore/loader.h"
#include "vmcore/options.h"
#include "vmcore/utf8.h"


/* codegen_emit ****************************************************************

   Generates machine code.

*******************************************************************************/

#if defined(ENABLE_SSA)
void cg_move(codegendata *cd, s4 type, s4 src_regoff, s4 src_flags,
			 s4 dst_regoff, s4 dst_flags);
void codegen_insert_phi_moves(jitdata *jd, basicblock *bptr);
#endif

bool codegen_emit(jitdata *jd)
{
	methodinfo         *m;
	codeinfo           *code;
	codegendata        *cd;
	registerdata       *rd;
	s4                  len, s1, s2, s3, d, disp;
	varinfo            *var, *var1;
	basicblock         *bptr;
	instruction        *iptr;
	exception_entry    *ex;
	u2                  currentline;
	methodinfo         *lm;             /* local methodinfo for ICMD_INVOKE*  */
	builtintable_entry *bte;
	methoddesc         *md;
	fieldinfo          *fi;
	unresolved_field   *uf;
	s4                  fieldtype;
	s4                  varindex;
#if defined(ENABLE_SSA)
	lsradata *ls;
	bool last_cmd_was_goto;

	last_cmd_was_goto = false;
	ls = jd->ls;
#endif

	/* get required compiler data */

	m    = jd->m;
	code = jd->code;
	cd   = jd->cd;
	rd   = jd->rd;

	/* prevent compiler warnings */

	s1          = 0;
	s2          = 0;
	d           = 0;
	currentline = 0;
	lm          = NULL;
	bte         = NULL;

	{
	s4 i, p, t, l;
  	s4 savedregs_num = 0;
	s4 stack_off = 0;

	/* space to save used callee saved registers */

	savedregs_num += (INT_SAV_CNT - rd->savintreguse);

	/* float register are saved on 2 4-byte stackslots */
	savedregs_num += (FLT_SAV_CNT - rd->savfltreguse) * 2;

	cd->stackframesize = rd->memuse + savedregs_num;

	   
#if defined(ENABLE_THREADS)
	/* space to save argument of monitor_enter */

	if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
		/* reserve 2 slots for long/double return values for monitorexit */

		if (IS_2_WORD_TYPE(m->parseddesc->returntype.type))
			cd->stackframesize += 2;
		else
			cd->stackframesize++;
	}
#endif

	/* create method header */

    /* Keep stack of non-leaf functions 16-byte aligned. */

	if (!jd->isleafmethod)
		cd->stackframesize |= 0x3;

	(void) dseg_add_unique_address(cd, code);              /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 4); /* FrameSize       */

#if defined(ENABLE_THREADS)
	/* IsSync contains the offset relative to the stack pointer for the
	   argument of monitor_exit used in the exception handler. Since the
	   offset could be zero and give a wrong meaning of the flag it is
	   offset by one.
	*/

	if (checksync && (m->flags & ACC_SYNCHRONIZED))
		(void) dseg_add_unique_s4(cd, (rd->memuse + 1) * 4); /* IsSync        */
	else
#endif
		(void) dseg_add_unique_s4(cd, 0);                  /* IsSync          */
	                                       
	(void) dseg_add_unique_s4(cd, jd->isleafmethod);       /* IsLeaf          */
	(void) dseg_add_unique_s4(cd, INT_SAV_CNT - rd->savintreguse); /* IntSave */
	(void) dseg_add_unique_s4(cd, FLT_SAV_CNT - rd->savfltreguse); /* FltSave */

	/* adds a reference for the length of the line number counter. We don't
	   know the size yet, since we evaluate the information during code
	   generation, to save one additional iteration over the whole
	   instructions. During code optimization the position could have changed
	   to the information gotten from the class file */
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
		M_IADD_IMM_MEMBASE(1, REG_ITMP3, OFFSET(codeinfo, frequency));
	}
#endif

	/* create stack frame (if necessary) */

	if (cd->stackframesize)
		M_ASUB_IMM(cd->stackframesize * 4, REG_SP);

	/* save return address and used callee saved registers */

  	p = cd->stackframesize;
	for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
 		p--; M_AST(rd->savintregs[i], REG_SP, p * 4);
	}
	for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
		p-=2; emit_fld_reg(cd, rd->savfltregs[i]); emit_fstpl_membase(cd, REG_SP, p * 4);
	}

	/* take arguments out of register or stack frame */

	md = m->parseddesc;

	stack_off = 0;
 	for (p = 0, l = 0; p < md->paramcount; p++) {
 		t = md->paramtypes[p].type;

#if defined(ENABLE_SSA)
		if ( ls != NULL ) {
			l = ls->local_0[p];
		}
#endif
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
				log_text("integer register argument");
				assert(0);
				if (!(var->flags & INMEMORY)) {      /* reg arg -> register   */
					/* rd->argintregs[md->params[p].regoff -> var->vv.regoff     */
				} 
				else {                               /* reg arg -> spilled    */
					/* rd->argintregs[md->params[p].regoff -> var->vv.regoff * 4 */
				}
			} 
			else {                                   /* stack arguments       */
				if (!(var->flags & INMEMORY)) {      /* stack arg -> register */
					emit_mov_membase_reg(           /* + 4 for return address */
					   cd, REG_SP, (cd->stackframesize + s1) * 4 + 4, var->vv.regoff);
					                                /* + 4 for return address */
				} 
				else {                               /* stack arg -> spilled  */
					if (!IS_2_WORD_TYPE(t)) {
#if defined(ENABLE_SSA)
						/* no copy avoiding by now possible with SSA */
						if (ls != NULL) {
							emit_mov_membase_reg(   /* + 4 for return address */
								 cd, REG_SP, (cd->stackframesize + s1) * 4 + 4,
								 REG_ITMP1);    
							emit_mov_reg_membase(
								 cd, REG_ITMP1, REG_SP, var->vv.regoff * 4);
						}
						else 
#endif /*defined(ENABLE_SSA)*/
						                  /* reuse Stackslotand avoid copying */
							var->vv.regoff = cd->stackframesize + s1 + 1;

					} 
					else {
#if defined(ENABLE_SSA)
						/* no copy avoiding by now possible with SSA */
						if (ls != NULL) {
							emit_mov_membase_reg(  /* + 4 for return address */
								 cd, REG_SP, (cd->stackframesize + s1) * 4 + 4,
								 REG_ITMP1);
							emit_mov_reg_membase(
								 cd, REG_ITMP1, REG_SP, var->vv.regoff * 4);
							emit_mov_membase_reg(   /* + 4 for return address */
								  cd, REG_SP, (cd->stackframesize + s1) * 4 + 4 + 4,
								  REG_ITMP1);             
							emit_mov_reg_membase(
								 cd, REG_ITMP1, REG_SP, var->vv.regoff * 4 + 4);
						}
						else
#endif /*defined(ENABLE_SSA)*/
						                  /* reuse Stackslotand avoid copying */
							var->vv.regoff = cd->stackframesize + s1 + 1;
					}
				}
			}
		}
		else {                                       /* floating args         */
			if (!md->params[p].inmemory) {           /* register arguments    */
				log_text("There are no float argument registers!");
				assert(0);
				if (!(var->flags & INMEMORY)) {  /* reg arg -> register   */
					/* rd->argfltregs[md->params[p].regoff -> var->vv.regoff     */
				} else {			             /* reg arg -> spilled    */
					/* rd->argfltregs[md->params[p].regoff -> var->vv.regoff * 4 */
				}

			} 
			else {                                   /* stack arguments       */
				if (!(var->flags & INMEMORY)) {      /* stack-arg -> register */
					if (t == TYPE_FLT) {
						emit_flds_membase(
                            cd, REG_SP, (cd->stackframesize + s1) * 4 + 4);
						assert(0);
/* 						emit_fstp_reg(cd, var->vv.regoff + fpu_st_offset); */

					} 
					else {
						emit_fldl_membase(
                            cd, REG_SP, (cd->stackframesize + s1) * 4 + 4);
						assert(0);
/* 						emit_fstp_reg(cd, var->vv.regoff + fpu_st_offset); */
					}

				} else {                             /* stack-arg -> spilled  */
#if defined(ENABLE_SSA)
					/* no copy avoiding by now possible with SSA */
					if (ls != NULL) {
						emit_mov_membase_reg(
						 cd, REG_SP, (cd->stackframesize + s1) * 4 + 4, REG_ITMP1);
						emit_mov_reg_membase(
 									 cd, REG_ITMP1, REG_SP, var->vv.regoff * 4);
						if (t == TYPE_FLT) {
							emit_flds_membase(
								  cd, REG_SP, (cd->stackframesize + s1) * 4 + 4);
							emit_fstps_membase(cd, REG_SP, var->vv.regoff * 4);
						} 
						else {
							emit_fldl_membase(
								  cd, REG_SP, (cd->stackframesize + s1) * 4 + 4);
							emit_fstpl_membase(cd, REG_SP, var->vv.regoff * 4);
						}
					}
					else
#endif /*defined(ENABLE_SSA)*/
						                  /* reuse Stackslotand avoid copying */
						var->vv.regoff = cd->stackframesize + s1 + 1;
				}
			}
		}
	}  /* end for */

	/* call monitorenter function */

#if defined(ENABLE_THREADS)
	if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
		s1 = rd->memuse;

		if (m->flags & ACC_STATIC) {
			M_MOV_IMM(&m->class->object.header, REG_ITMP1);
		}
		else {
			M_ALD(REG_ITMP1, REG_SP, cd->stackframesize * 4 + 4);
			M_TEST(REG_ITMP1);
			M_BNE(6);
			M_ALD_MEM(REG_ITMP1, EXCEPTION_HARDWARE_NULLPOINTER);
		}

		M_AST(REG_ITMP1, REG_SP, s1 * 4);
		M_AST(REG_ITMP1, REG_SP, 0 * 4);
		M_MOV_IMM(LOCK_monitor_enter, REG_ITMP3);
		M_CALL(REG_ITMP3);
	}			
#endif

#if !defined(NDEBUG)
	emit_verbosecall_enter(jd);
#endif

	} 

#if defined(ENABLE_SSA)
	/* with SSA Header is Basic Block 0 - insert phi Moves if necessary */
	if ( ls != NULL)
		codegen_insert_phi_moves(jd, ls->basicblocks[0]);
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

#if defined(ENABLE_REPLACEMENT)
		if (bptr->bitflags & BBFLAG_REPLACEMENT) {
			if (cd->replacementpoint[-1].flags & RPLPOINT_FLAG_COUNTDOWN) {
				MCODECHECK(32);
				disp = (s4) &(m->hitcountdown);
				M_ISUB_IMM_MEMABS(1, disp);
				M_BS(0);
			}
		}
#endif

		/* copy interface registers to their destination */

		len = bptr->indepth;
		MCODECHECK(512);

#if defined(ENABLE_PROFILING)
		/* generate basic block profiling code */

		if (JITDATA_HAS_FLAG_INSTRUMENT(jd)) {
			/* count frequency */

			M_MOV_IMM(code->bbfrequency, REG_ITMP3);
			M_IADD_IMM_MEMBASE(1, REG_ITMP3, bptr->nr * 4);
		}
#endif

#if defined(ENABLE_LSRA) || defined(ENABLE_SSA)
# if defined(ENABLE_LSRA) && !defined(ENABLE_SSA)
		if (opt_lsra) {
# endif
# if defined(ENABLE_SSA)
		if (ls != NULL) {
			last_cmd_was_goto = false;
# endif
			if (len > 0) {
				len--;
				var = VAR(bptr->invars[len]);
				if (bptr->type != BBTYPE_STD) {
					if (!IS_2_WORD_TYPE(var->type)) {
						if (bptr->type == BBTYPE_EXH) {
							d = codegen_reg_of_var(0, var, REG_ITMP1);
							M_INTMOVE(REG_ITMP1, d);
							emit_store(jd, NULL, var, d);
						}
					} 
					else {
						log_text("copy interface registers(EXH, SBR): longs \
                                  have to be in memory (begin 1)");
						assert(0);
					}
				}
			}

		} 
		else
#endif /* defined(ENABLE_LSRA) || defined(ENABLE_SSA) */
		{
		while (len) {
			len--;
			var = VAR(bptr->invars[len]);
			if ((len == bptr->indepth-1) && (bptr->type != BBTYPE_STD)) {
				if (!IS_2_WORD_TYPE(var->type)) {
					if (bptr->type == BBTYPE_EXH) {
						d = codegen_reg_of_var(0, var, REG_ITMP1);
						M_INTMOVE(REG_ITMP1, d);
						emit_store(jd, NULL, var, d);
					}
				} 
				else {
					log_text("copy interface registers: longs have to be in \
                               memory (begin 1)");
					assert(0);
				}

			} 
			else {
				assert((var->flags & INOUT));
			}
		} /* while (len) */
		} /* */

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
			if (iptr->sx.val.f == 0.0) {
				emit_fldz(cd);

				/* -0.0 */
				if (iptr->sx.val.i == 0x80000000) {
					emit_fchs(cd);
				}

			} else if (iptr->sx.val.f == 1.0) {
				emit_fld1(cd);

			} else if (iptr->sx.val.f == 2.0) {
				emit_fld1(cd);
				emit_fld1(cd);
				emit_faddp(cd);

			} else {
  				disp = dseg_add_float(cd, iptr->sx.val.f);
				emit_mov_imm_reg(cd, 0, REG_ITMP1);
				dseg_adddata(cd);
				emit_flds_membase(cd, REG_ITMP1, disp);
			}
			emit_store_dst(jd, iptr, d);
			break;
		
		case ICMD_DCONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			if (iptr->sx.val.d == 0.0) {
				emit_fldz(cd);

				/* -0.0 */
				if (iptr->sx.val.l == 0x8000000000000000LL) {
					emit_fchs(cd);
				}

			} else if (iptr->sx.val.d == 1.0) {
				emit_fld1(cd);

			} else if (iptr->sx.val.d == 2.0) {
				emit_fld1(cd);
				emit_fld1(cd);
				emit_faddp(cd);

			} else {
				disp = dseg_add_double(cd, iptr->sx.val.d);
				emit_mov_imm_reg(cd, 0, REG_ITMP1);
				dseg_adddata(cd);
				emit_fldl_membase(cd, REG_ITMP1, disp);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ACONST:     /* ...  ==> ..., constant                       */

			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				codegen_addpatchref(cd, PATCHER_aconst,
									iptr->sx.val.c.ref, 0);

				M_MOV_IMM(NULL, d);

			} else {
				if (iptr->sx.val.anyptr == NULL)
					M_CLR(d);
				else
					M_MOV_IMM(iptr->sx.val.anyptr, d);
			}
			emit_store_dst(jd, iptr, d);
			break;


		/* load/store/copy/move operations ************************************/

		case ICMD_ILOAD:
		case ICMD_ALOAD:
		case ICMD_LLOAD:
		case ICMD_FLOAD:
		case ICMD_DLOAD:
		case ICMD_ISTORE:
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
			M_NEG(d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LNGMOVE(s1, d);
			M_NEG(GET_LOW_REG(d));
			M_IADDC_IMM(0, GET_HIGH_REG(d));
			M_NEG(GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_I2L:        /* ..., value  ==> ..., value                   */

			s1 = emit_load_s1(jd, iptr, EAX);
			d = codegen_reg_of_dst(jd, iptr, EAX_EDX_PACKED);
			M_INTMOVE(s1, EAX);
			M_CLTD;
			M_LNGMOVE(EAX_EDX_PACKED, d);
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
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_SLL_IMM(24, d);
			M_SRA_IMM(24, d);
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
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
				
			/* `inc reg' is slower on p4's (regarding to ia32
			   optimization reference manual and benchmarks) and as
			   fast on athlon's. */

			M_INTMOVE(s1, d);
			M_IADD_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_INTMOVE(s1, GET_LOW_REG(d));
			M_IADD(s2, GET_LOW_REG(d));
			/* don't use REG_ITMP1 */
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);
			M_INTMOVE(s1, GET_HIGH_REG(d));
			M_IADDC(s2, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LADDCONST:  /* ..., value  ==> ..., value + constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LNGMOVE(s1, d);
			M_IADD_IMM(iptr->sx.val.l, GET_LOW_REG(d));
			M_IADDC_IMM(iptr->sx.val.l >> 32, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			if (s2 == d) {
				M_INTMOVE(s1, REG_ITMP1);
				M_ISUB(s2, REG_ITMP1);
				M_INTMOVE(REG_ITMP1, d);
			}
			else {
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

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if (s2 == GET_LOW_REG(d)) {
				M_INTMOVE(s1, REG_ITMP1);
				M_ISUB(s2, REG_ITMP1);
				M_INTMOVE(REG_ITMP1, GET_LOW_REG(d));
			}
			else {
				M_INTMOVE(s1, GET_LOW_REG(d));
				M_ISUB(s2, GET_LOW_REG(d));
			}
			/* don't use REG_ITMP1 */
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);
			if (s2 == GET_HIGH_REG(d)) {
				M_INTMOVE(s1, REG_ITMP2);
				M_ISUBB(s2, REG_ITMP2);
				M_INTMOVE(REG_ITMP2, GET_HIGH_REG(d));
			}
			else {
				M_INTMOVE(s1, GET_HIGH_REG(d));
				M_ISUBB(s2, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSUBCONST:  /* ..., value  ==> ..., value - constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LNGMOVE(s1, d);
			M_ISUB_IMM(iptr->sx.val.l, GET_LOW_REG(d));
			M_ISUBB_IMM(iptr->sx.val.l >> 32, GET_HIGH_REG(d));
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
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			M_IMUL_IMM(s1, iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_low(jd, iptr, EDX);
			d = codegen_reg_of_dst(jd, iptr, EAX_EDX_PACKED);

			M_INTMOVE(s1, REG_ITMP2);
			M_IMUL(s2, REG_ITMP2);

			s1 = emit_load_s1_low(jd, iptr, EAX);
			s2 = emit_load_s2_high(jd, iptr, EDX);
			M_INTMOVE(s2, EDX);
			M_IMUL(s1, EDX);
			M_IADD(EDX, REG_ITMP2);

			s1 = emit_load_s1_low(jd, iptr, EAX);
			s2 = emit_load_s2_low(jd, iptr, EDX);
			M_INTMOVE(s1, EAX);
			M_MUL(s2);
			M_INTMOVE(EAX, GET_LOW_REG(d));
			M_IADD(REG_ITMP2, GET_HIGH_REG(d));

			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LMULCONST:  /* ..., value  ==> ..., value * constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, EAX_EDX_PACKED);
			ICONST(EAX, iptr->sx.val.l);
			M_MUL(s1);
			M_IMUL_IMM(s1, iptr->sx.val.l >> 32, REG_ITMP2);
			M_IADD(REG_ITMP2, EDX);
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			M_IMUL_IMM(s1, iptr->sx.val.l, REG_ITMP2);
			M_IADD(REG_ITMP2, EDX);
			M_LNGMOVE(EAX_EDX_PACKED, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			s1 = emit_load_s1(jd, iptr, EAX);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, EAX);
			emit_arithmetic_check(cd, iptr, s2);

			M_INTMOVE(s1, EAX);           /* we need the first operand in EAX */

			/* check as described in jvm spec */

			M_CMP_IMM(0x80000000, EAX);
			M_BNE(3 + 6);
			M_CMP_IMM(-1, s2);
			M_BEQ(1 + 2);
  			M_CLTD;
			M_IDIV(s2);

			M_INTMOVE(EAX, d);           /* if INMEMORY then d is already EAX */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s1 = emit_load_s1(jd, iptr, EAX);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, EDX);
			emit_arithmetic_check(cd, iptr, s2);

			M_INTMOVE(s1, EAX);           /* we need the first operand in EAX */

			/* check as described in jvm spec */

			M_CMP_IMM(0x80000000, EAX);
			M_BNE(2 + 3 + 6);
			M_CLR(EDX);
			M_CMP_IMM(-1, s2);
			M_BEQ(1 + 2);
  			M_CLTD;
			M_IDIV(s2);

			M_INTMOVE(EDX, d);           /* if INMEMORY then d is already EDX */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IDIVPOW2:   /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                          */

			/* TODO: optimize for `/ 2' */
			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_TEST(d);
			M_BNS(6);
			M_IADD_IMM32((1 << iptr->sx.val.i) - 1, d);/* 32-bit for jump off */
			M_SRA_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s1 == d) {
				M_MOV(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			} 
			M_INTMOVE(s1, d);
			M_AND_IMM(iptr->sx.val.i, d);
			M_TEST(s1);
			M_BGE(2 + 2 + 6 + 2);
			M_MOV(s1, d);  /* don't use M_INTMOVE, so we know the jump offset */
			M_NEG(d);
			M_AND_IMM32(iptr->sx.val.i, d);     /* use 32-bit for jump offset */
			M_NEG(d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */
		case ICMD_LREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			s2 = emit_load_s2(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);

			M_INTMOVE(GET_LOW_REG(s2), REG_ITMP3);
			M_OR(GET_HIGH_REG(s2), REG_ITMP3);
			/* XXX could be optimized */
			emit_arithmetic_check(cd, iptr, REG_ITMP3);

			bte = iptr->sx.s23.s3.bte;
			md = bte->md;

			M_LST(s2, REG_SP, 2 * 4);

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			M_LST(s1, REG_SP, 0 * 4);

			M_MOV_IMM(bte->fp, REG_ITMP3);
			M_CALL(REG_ITMP3);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LDIVPOW2:   /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(s1, d);
			M_TEST(GET_HIGH_REG(d));
			M_BNS(6 + 3);
			M_IADD_IMM32((1 << iptr->sx.val.i) - 1, GET_LOW_REG(d));
			M_IADDC_IMM(0, GET_HIGH_REG(d));
			M_SRLD_IMM(iptr->sx.val.i, GET_HIGH_REG(d), GET_LOW_REG(d));
			M_SRA_IMM(iptr->sx.val.i, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

#if 0
		case ICMD_LREMPOW2:   /* ..., value  ==> ..., value % constant        */
		                      /* sx.val.l = constant                          */

			d = codegen_reg_of_dst(jd, iptr, REG_NULL);
			if (iptr->dst.var->flags & INMEMORY) {
				if (iptr->s1.var->flags & INMEMORY) {
					/* Alpha algorithm */
					disp = 3;
					CALCOFFSETBYTES(disp, REG_SP, iptr->s1.var->vv.regoff * 4);
					disp += 3;
					CALCOFFSETBYTES(disp, REG_SP, iptr->s1.var->vv.regoff * 4 + 4);

					disp += 2;
					disp += 3;
					disp += 2;

					/* TODO: hmm, don't know if this is always correct */
					disp += 2;
					CALCIMMEDIATEBYTES(disp, iptr->sx.val.l & 0x00000000ffffffff);
					disp += 2;
					CALCIMMEDIATEBYTES(disp, iptr->sx.val.l >> 32);

					disp += 2;
					disp += 3;
					disp += 2;

					emit_mov_membase_reg(cd, REG_SP, iptr->s1.var->vv.regoff * 4, REG_ITMP1);
					emit_mov_membase_reg(cd, REG_SP, iptr->s1.var->vv.regoff * 4 + 4, REG_ITMP2);
					
					emit_alu_imm_reg(cd, ALU_AND, iptr->sx.val.l, REG_ITMP1);
					emit_alu_imm_reg(cd, ALU_AND, iptr->sx.val.l >> 32, REG_ITMP2);
					emit_alu_imm_membase(cd, ALU_CMP, 0, REG_SP, iptr->s1.var->vv.regoff * 4 + 4);
					emit_jcc(cd, CC_GE, disp);

					emit_mov_membase_reg(cd, REG_SP, iptr->s1.var->vv.regoff * 4, REG_ITMP1);
					emit_mov_membase_reg(cd, REG_SP, iptr->s1.var->vv.regoff * 4 + 4, REG_ITMP2);
					
					emit_neg_reg(cd, REG_ITMP1);
					emit_alu_imm_reg(cd, ALU_ADC, 0, REG_ITMP2);
					emit_neg_reg(cd, REG_ITMP2);
					
					emit_alu_imm_reg(cd, ALU_AND, iptr->sx.val.l, REG_ITMP1);
					emit_alu_imm_reg(cd, ALU_AND, iptr->sx.val.l >> 32, REG_ITMP2);
					
					emit_neg_reg(cd, REG_ITMP1);
					emit_alu_imm_reg(cd, ALU_ADC, 0, REG_ITMP2);
					emit_neg_reg(cd, REG_ITMP2);

					emit_mov_reg_membase(cd, REG_ITMP1, REG_SP, iptr->dst.var->vv.regoff * 4);
					emit_mov_reg_membase(cd, REG_ITMP2, REG_SP, iptr->dst.var->vv.regoff * 4 + 4);
				}
			}

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(s1, d);
			M_AND_IMM(iptr->sx.val.l, GET_LOW_REG(d));	
			M_AND_IMM(iptr->sx.val.l >> 32, GET_HIGH_REG(d));
			M_TEST(GET_LOW_REG(s1));
			M_BGE(0);
			M_LNGMOVE(s1, d);
		break;
#endif

		case ICMD_ISHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s2, ECX);                       /* s2 may be equal to d */
			M_INTMOVE(s1, d);
			M_SLL(d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHLCONST:  /* ..., value  ==> ..., value << constant       */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_SLL_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHR:       /* ..., val1, val2  ==> ..., val1 >> val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s2, ECX);                       /* s2 may be equal to d */
			M_INTMOVE(s1, d);
			M_SRA(d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_ISHRCONST:  /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_SRA_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHR:      /* ..., val1, val2  ==> ..., val1 >>> val2      */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s2, ECX);                       /* s2 may be equal to d */
			M_INTMOVE(s1, d);
			M_SRL(d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_SRL_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHL:       /* ..., val1, val2  ==> ..., val1 << val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP13_PACKED);
			s2 = emit_load_s2(jd, iptr, ECX);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP13_PACKED);
			M_LNGMOVE(s1, d);
			M_INTMOVE(s2, ECX);
			M_TEST_IMM(32, ECX);
			M_BEQ(2 + 2);
			M_MOV(GET_LOW_REG(d), GET_HIGH_REG(d));
			M_CLR(GET_LOW_REG(d));
			M_SLLD(GET_LOW_REG(d), GET_HIGH_REG(d));
			M_SLL(GET_LOW_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

        case ICMD_LSHLCONST:  /* ..., value  ==> ..., value << constant       */
 			                  /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LNGMOVE(s1, d);
			if (iptr->sx.val.i & 0x20) {
				M_MOV(GET_LOW_REG(d), GET_HIGH_REG(d));
				M_CLR(GET_LOW_REG(d));
				M_SLLD_IMM(iptr->sx.val.i & 0x3f, GET_LOW_REG(d), 
						   GET_HIGH_REG(d));
			}
			else {
				M_SLLD_IMM(iptr->sx.val.i & 0x3f, GET_LOW_REG(d),
						   GET_HIGH_REG(d));
				M_SLL_IMM(iptr->sx.val.i & 0x3f, GET_LOW_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHR:       /* ..., val1, val2  ==> ..., val1 >> val2       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP13_PACKED);
			s2 = emit_load_s2(jd, iptr, ECX);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP13_PACKED);
			M_LNGMOVE(s1, d);
			M_INTMOVE(s2, ECX);
			M_TEST_IMM(32, ECX);
			M_BEQ(2 + 3);
			M_MOV(GET_HIGH_REG(d), GET_LOW_REG(d));
			M_SRA_IMM(31, GET_HIGH_REG(d));
			M_SRLD(GET_HIGH_REG(d), GET_LOW_REG(d));
			M_SRA(GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LSHRCONST:  /* ..., value  ==> ..., value >> constant       */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LNGMOVE(s1, d);
			if (iptr->sx.val.i & 0x20) {
				M_MOV(GET_HIGH_REG(d), GET_LOW_REG(d));
				M_SRA_IMM(31, GET_HIGH_REG(d));
				M_SRLD_IMM(iptr->sx.val.i & 0x3f, GET_HIGH_REG(d), 
						   GET_LOW_REG(d));
			}
			else {
				M_SRLD_IMM(iptr->sx.val.i & 0x3f, GET_HIGH_REG(d), 
						   GET_LOW_REG(d));
				M_SRA_IMM(iptr->sx.val.i & 0x3f, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LUSHR:      /* ..., val1, val2  ==> ..., val1 >>> val2      */

			s1 = emit_load_s1(jd, iptr, REG_ITMP13_PACKED);
			s2 = emit_load_s2(jd, iptr, ECX);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP13_PACKED);
			M_LNGMOVE(s1, d);
			M_INTMOVE(s2, ECX);
			M_TEST_IMM(32, ECX);
			M_BEQ(2 + 2);
			M_MOV(GET_HIGH_REG(d), GET_LOW_REG(d));
			M_CLR(GET_HIGH_REG(d));
			M_SRLD(GET_HIGH_REG(d), GET_LOW_REG(d));
			M_SRL(GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

  		case ICMD_LUSHRCONST: /* ..., value  ==> ..., value >>> constant      */
  		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LNGMOVE(s1, d);
			if (iptr->sx.val.i & 0x20) {
				M_MOV(GET_HIGH_REG(d), GET_LOW_REG(d));
				M_CLR(GET_HIGH_REG(d));
				M_SRLD_IMM(iptr->sx.val.i & 0x3f, GET_HIGH_REG(d), 
						   GET_LOW_REG(d));
			}
			else {
				M_SRLD_IMM(iptr->sx.val.i & 0x3f, GET_HIGH_REG(d), 
						   GET_LOW_REG(d));
				M_SRL_IMM(iptr->sx.val.i & 0x3f, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
  			break;

		case ICMD_IAND:       /* ..., val1, val2  ==> ..., val1 & val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_AND(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_AND(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_AND_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LAND:       /* ..., val1, val2  ==> ..., val1 & val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if (s2 == GET_LOW_REG(d))
				M_AND(s1, GET_LOW_REG(d));
			else {
				M_INTMOVE(s1, GET_LOW_REG(d));
				M_AND(s2, GET_LOW_REG(d));
			}
			/* REG_ITMP1 probably contains low 32-bit of destination */
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);
			if (s2 == GET_HIGH_REG(d))
				M_AND(s1, GET_HIGH_REG(d));
			else {
				M_INTMOVE(s1, GET_HIGH_REG(d));
				M_AND(s2, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LANDCONST:  /* ..., value  ==> ..., value & constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LNGMOVE(s1, d);
			M_AND_IMM(iptr->sx.val.l, GET_LOW_REG(d));
			M_AND_IMM(iptr->sx.val.l >> 32, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IOR:        /* ..., val1, val2  ==> ..., val1 | val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_OR(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_OR(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_OR_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LOR:        /* ..., val1, val2  ==> ..., val1 | val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if (s2 == GET_LOW_REG(d))
				M_OR(s1, GET_LOW_REG(d));
			else {
				M_INTMOVE(s1, GET_LOW_REG(d));
				M_OR(s2, GET_LOW_REG(d));
			}
			/* REG_ITMP1 probably contains low 32-bit of destination */
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);
			if (s2 == GET_HIGH_REG(d))
				M_OR(s1, GET_HIGH_REG(d));
			else {
				M_INTMOVE(s1, GET_HIGH_REG(d));
				M_OR(s2, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LORCONST:   /* ..., value  ==> ..., value | constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LNGMOVE(s1, d);
			M_OR_IMM(iptr->sx.val.l, GET_LOW_REG(d));
			M_OR_IMM(iptr->sx.val.l >> 32, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IXOR:       /* ..., val1, val2  ==> ..., val1 ^ val2        */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
			if (s2 == d)
				M_XOR(s1, d);
			else {
				M_INTMOVE(s1, d);
				M_XOR(s2, d);
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IXORCONST:  /* ..., value  ==> ..., value ^ constant        */
		                      /* sx.val.i = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, d);
			M_XOR_IMM(iptr->sx.val.i, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXOR:       /* ..., val1, val2  ==> ..., val1 ^ val2        */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			if (s2 == GET_LOW_REG(d))
				M_XOR(s1, GET_LOW_REG(d));
			else {
				M_INTMOVE(s1, GET_LOW_REG(d));
				M_XOR(s2, GET_LOW_REG(d));
			}
			/* REG_ITMP1 probably contains low 32-bit of destination */
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);
			if (s2 == GET_HIGH_REG(d))
				M_XOR(s1, GET_HIGH_REG(d));
			else {
				M_INTMOVE(s1, GET_HIGH_REG(d));
				M_XOR(s2, GET_HIGH_REG(d));
			}
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LXORCONST:  /* ..., value  ==> ..., value ^ constant        */
		                      /* sx.val.l = constant                          */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP12_PACKED);
			M_LNGMOVE(s1, d);
			M_XOR_IMM(iptr->sx.val.l, GET_LOW_REG(d));
			M_XOR_IMM(iptr->sx.val.l >> 32, GET_HIGH_REG(d));
			emit_store_dst(jd, iptr, d);
			break;


		/* floating operations ************************************************/

		case ICMD_FNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			emit_fchs(cd);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DNEG:       /* ..., value  ==> ..., - value                 */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			emit_fchs(cd);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			emit_faddp(cd);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DADD:       /* ..., val1, val2  ==> ..., val1 + val2        */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			emit_faddp(cd);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			emit_fsubp(cd);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DSUB:       /* ..., val1, val2  ==> ..., val1 - val2        */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			emit_fsubp(cd);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			emit_fmulp(cd);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DMUL:       /* ..., val1, val2  ==> ..., val1 * val2        */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			emit_fmulp(cd);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			emit_fdivp(cd);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DDIV:       /* ..., val1, val2  ==> ..., val1 / val2        */

			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			emit_fdivp(cd);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			/* exchanged to skip fxch */
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
/*  			emit_fxch(cd); */
			emit_fprem(cd);
			emit_wait(cd);
			emit_fnstsw(cd);
			emit_sahf(cd);
			emit_jcc(cd, CC_P, -(2 + 1 + 2 + 1 + 6));
			emit_store_dst(jd, iptr, d);
			emit_ffree_reg(cd, 0);
			emit_fincstp(cd);
			break;

		case ICMD_DREM:       /* ..., val1, val2  ==> ..., val1 % val2        */

			/* exchanged to skip fxch */
			s2 = emit_load_s2(jd, iptr, REG_FTMP2);
			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
/*  			emit_fxch(cd); */
			emit_fprem(cd);
			emit_wait(cd);
			emit_fnstsw(cd);
			emit_sahf(cd);
			emit_jcc(cd, CC_P, -(2 + 1 + 2 + 1 + 6));
			emit_store_dst(jd, iptr, d);
			emit_ffree_reg(cd, 0);
			emit_fincstp(cd);
			break;

		case ICMD_I2F:       /* ..., value  ==> ..., (float) value            */
		case ICMD_I2D:       /* ..., value  ==> ..., (double) value           */

			var = VAROP(iptr->s1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);

			if (var->flags & INMEMORY) {
				emit_fildl_membase(cd, REG_SP, var->vv.regoff * 4);
			} else {
				/* XXX not thread safe! */
				disp = dseg_add_unique_s4(cd, 0);
				emit_mov_imm_reg(cd, 0, REG_ITMP1);
				dseg_adddata(cd);
				emit_mov_reg_membase(cd, var->vv.regoff, REG_ITMP1, disp);
				emit_fildl_membase(cd, REG_ITMP1, disp);
			}

  			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_L2F:       /* ..., value  ==> ..., (float) value            */
		case ICMD_L2D:       /* ..., value  ==> ..., (double) value           */

			var = VAROP(iptr->s1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			if (var->flags & INMEMORY) {
				emit_fildll_membase(cd, REG_SP, var->vv.regoff * 4);

			} else {
				log_text("L2F: longs have to be in memory");
				assert(0);
			}
  			emit_store_dst(jd, iptr, d);
			break;
			
		case ICMD_F2I:       /* ..., value  ==> ..., (int) value              */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_NULL);

			emit_mov_imm_reg(cd, 0, REG_ITMP1);
			dseg_adddata(cd);

			/* Round to zero, 53-bit mode, exception masked */
			disp = dseg_add_s4(cd, 0x0e7f);
			emit_fldcw_membase(cd, REG_ITMP1, disp);

			var = VAROP(iptr->dst);
			var1 = VAROP(iptr->s1);

			if (var->flags & INMEMORY) {
				emit_fistpl_membase(cd, REG_SP, var->vv.regoff * 4);

				/* Round to nearest, 53-bit mode, exceptions masked */
				disp = dseg_add_s4(cd, 0x027f);
				emit_fldcw_membase(cd, REG_ITMP1, disp);

				emit_alu_imm_membase(cd, ALU_CMP, 0x80000000, 
									 REG_SP, var->vv.regoff * 4);

				disp = 3;
				CALCOFFSETBYTES(disp, REG_SP, var1->vv.regoff * 4);
				disp += 5 + 2 + 3;
				CALCOFFSETBYTES(disp, REG_SP, var->vv.regoff * 4);

			} else {
				/* XXX not thread safe! */
				disp = dseg_add_unique_s4(cd, 0);
				emit_fistpl_membase(cd, REG_ITMP1, disp);
				emit_mov_membase_reg(cd, REG_ITMP1, disp, var->vv.regoff);

				/* Round to nearest, 53-bit mode, exceptions masked */
				disp = dseg_add_s4(cd, 0x027f);
				emit_fldcw_membase(cd, REG_ITMP1, disp);

				emit_alu_imm_reg(cd, ALU_CMP, 0x80000000, var->vv.regoff);

				disp = 3;
				CALCOFFSETBYTES(disp, REG_SP, var1->vv.regoff * 4);
				disp += 5 + 2 + ((REG_RESULT == var->vv.regoff) ? 0 : 2);
			}

			emit_jcc(cd, CC_NE, disp);

			/* XXX: change this when we use registers */
			emit_flds_membase(cd, REG_SP, var1->vv.regoff * 4);
			emit_mov_imm_reg(cd, (ptrint) asm_builtin_f2i, REG_ITMP1);
			emit_call_reg(cd, REG_ITMP1);

			if (var->flags & INMEMORY) {
				emit_mov_reg_membase(cd, REG_RESULT, REG_SP, var->vv.regoff * 4);

			} else {
				M_INTMOVE(REG_RESULT, var->vv.regoff);
			}
			break;

		case ICMD_D2I:       /* ..., value  ==> ..., (int) value              */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_NULL);

			emit_mov_imm_reg(cd, 0, REG_ITMP1);
			dseg_adddata(cd);

			/* Round to zero, 53-bit mode, exception masked */
			disp = dseg_add_s4(cd, 0x0e7f);
			emit_fldcw_membase(cd, REG_ITMP1, disp);

			var  = VAROP(iptr->dst);
			var1 = VAROP(iptr->s1);

			if (var->flags & INMEMORY) {
				emit_fistpl_membase(cd, REG_SP, var->vv.regoff * 4);

				/* Round to nearest, 53-bit mode, exceptions masked */
				disp = dseg_add_s4(cd, 0x027f);
				emit_fldcw_membase(cd, REG_ITMP1, disp);

  				emit_alu_imm_membase(cd, ALU_CMP, 0x80000000, 
									 REG_SP, var->vv.regoff * 4);

				disp = 3;
				CALCOFFSETBYTES(disp, REG_SP, var1->vv.regoff * 4);
				disp += 5 + 2 + 3;
				CALCOFFSETBYTES(disp, REG_SP, var->vv.regoff * 4);

			} else {
				/* XXX not thread safe! */
				disp = dseg_add_unique_s4(cd, 0);
				emit_fistpl_membase(cd, REG_ITMP1, disp);
				emit_mov_membase_reg(cd, REG_ITMP1, disp, var->vv.regoff);

				/* Round to nearest, 53-bit mode, exceptions masked */
				disp = dseg_add_s4(cd, 0x027f);
				emit_fldcw_membase(cd, REG_ITMP1, disp);

				emit_alu_imm_reg(cd, ALU_CMP, 0x80000000, var->vv.regoff);

				disp = 3;
				CALCOFFSETBYTES(disp, REG_SP, var1->vv.regoff * 4);
				disp += 5 + 2 + ((REG_RESULT == var->vv.regoff) ? 0 : 2);
			}

			emit_jcc(cd, CC_NE, disp);

			/* XXX: change this when we use registers */
			emit_fldl_membase(cd, REG_SP, var1->vv.regoff * 4);
			emit_mov_imm_reg(cd, (ptrint) asm_builtin_d2i, REG_ITMP1);
			emit_call_reg(cd, REG_ITMP1);

			if (var->flags & INMEMORY) {
				emit_mov_reg_membase(cd, REG_RESULT, REG_SP, var->vv.regoff * 4);
			} else {
				M_INTMOVE(REG_RESULT, var->vv.regoff);
			}
			break;

		case ICMD_F2L:       /* ..., value  ==> ..., (long) value             */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_NULL);

			emit_mov_imm_reg(cd, 0, REG_ITMP1);
			dseg_adddata(cd);

			/* Round to zero, 53-bit mode, exception masked */
			disp = dseg_add_s4(cd, 0x0e7f);
			emit_fldcw_membase(cd, REG_ITMP1, disp);

			var  = VAROP(iptr->dst);
			var1 = VAROP(iptr->s1);

			if (var->flags & INMEMORY) {
				emit_fistpll_membase(cd, REG_SP, var->vv.regoff * 4);

				/* Round to nearest, 53-bit mode, exceptions masked */
				disp = dseg_add_s4(cd, 0x027f);
				emit_fldcw_membase(cd, REG_ITMP1, disp);

  				emit_alu_imm_membase(cd, ALU_CMP, 0x80000000, 
									 REG_SP, var->vv.regoff * 4 + 4);

				disp = 6 + 4;
				CALCOFFSETBYTES(disp, REG_SP, var->vv.regoff * 4);
				disp += 3;
				CALCOFFSETBYTES(disp, REG_SP, var1->vv.regoff * 4);
				disp += 5 + 2;
				disp += 3;
				CALCOFFSETBYTES(disp, REG_SP, var->vv.regoff * 4);
				disp += 3;
				CALCOFFSETBYTES(disp, REG_SP, var->vv.regoff * 4 + 4);

				emit_jcc(cd, CC_NE, disp);

  				emit_alu_imm_membase(cd, ALU_CMP, 0, 
									 REG_SP, var->vv.regoff * 4);

				disp = 3;
				CALCOFFSETBYTES(disp, REG_SP, var1->vv.regoff * 4);
				disp += 5 + 2 + 3;
				CALCOFFSETBYTES(disp, REG_SP, var->vv.regoff * 4);

				emit_jcc(cd, CC_NE, disp);

				/* XXX: change this when we use registers */
				emit_flds_membase(cd, REG_SP, var1->vv.regoff * 4);
				emit_mov_imm_reg(cd, (ptrint) asm_builtin_f2l, REG_ITMP1);
				emit_call_reg(cd, REG_ITMP1);
				emit_mov_reg_membase(cd, REG_RESULT, REG_SP, var->vv.regoff * 4);
				emit_mov_reg_membase(cd, REG_RESULT2, 
									 REG_SP, var->vv.regoff * 4 + 4);

			} else {
				log_text("F2L: longs have to be in memory");
				assert(0);
			}
			break;

		case ICMD_D2L:       /* ..., value  ==> ..., (long) value             */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_NULL);

			emit_mov_imm_reg(cd, 0, REG_ITMP1);
			dseg_adddata(cd);

			/* Round to zero, 53-bit mode, exception masked */
			disp = dseg_add_s4(cd, 0x0e7f);
			emit_fldcw_membase(cd, REG_ITMP1, disp);

			var  = VAROP(iptr->dst);
			var1 = VAROP(iptr->s1);

			if (var->flags & INMEMORY) {
				emit_fistpll_membase(cd, REG_SP, var->vv.regoff * 4);

				/* Round to nearest, 53-bit mode, exceptions masked */
				disp = dseg_add_s4(cd, 0x027f);
				emit_fldcw_membase(cd, REG_ITMP1, disp);

  				emit_alu_imm_membase(cd, ALU_CMP, 0x80000000, 
									 REG_SP, var->vv.regoff * 4 + 4);

				disp = 6 + 4;
				CALCOFFSETBYTES(disp, REG_SP, var->vv.regoff * 4);
				disp += 3;
				CALCOFFSETBYTES(disp, REG_SP, var1->vv.regoff * 4);
				disp += 5 + 2;
				disp += 3;
				CALCOFFSETBYTES(disp, REG_SP, var->vv.regoff * 4);
				disp += 3;
				CALCOFFSETBYTES(disp, REG_SP, var->vv.regoff * 4 + 4);

				emit_jcc(cd, CC_NE, disp);

  				emit_alu_imm_membase(cd, ALU_CMP, 0, REG_SP, var->vv.regoff * 4);

				disp = 3;
				CALCOFFSETBYTES(disp, REG_SP, var1->vv.regoff * 4);
				disp += 5 + 2 + 3;
				CALCOFFSETBYTES(disp, REG_SP, var->vv.regoff * 4);

				emit_jcc(cd, CC_NE, disp);

				/* XXX: change this when we use registers */
				emit_fldl_membase(cd, REG_SP, var1->vv.regoff * 4);
				emit_mov_imm_reg(cd, (ptrint) asm_builtin_d2l, REG_ITMP1);
				emit_call_reg(cd, REG_ITMP1);
				emit_mov_reg_membase(cd, REG_RESULT, REG_SP, var->vv.regoff * 4);
				emit_mov_reg_membase(cd, REG_RESULT2, 
									 REG_SP, var->vv.regoff * 4 + 4);

			} else {
				log_text("D2L: longs have to be in memory");
				assert(0);
			}
			break;

		case ICMD_F2D:       /* ..., value  ==> ..., (double) value           */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			/* nothing to do */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_D2F:       /* ..., value  ==> ..., (float) value            */

			s1 = emit_load_s1(jd, iptr, REG_FTMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			/* nothing to do */
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCMPL:      /* ..., val1, val2  ==> ..., val1 fcmpl val2    */
		case ICMD_DCMPL:

			/* exchanged to skip fxch */
			s2 = emit_load_s1(jd, iptr, REG_FTMP1);
			s1 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
/*    			emit_fxch(cd); */
			emit_fucompp(cd);
			emit_fnstsw(cd);
			emit_test_imm_reg(cd, 0x400, EAX);    /* unordered treat as GT */
			emit_jcc(cd, CC_E, 6);
			emit_alu_imm_reg(cd, ALU_AND, 0x000000ff, EAX);
 			emit_sahf(cd);
			emit_mov_imm_reg(cd, 0, d);    /* does not affect flags */
  			emit_jcc(cd, CC_E, 6 + 3 + 5 + 3);
  			emit_jcc(cd, CC_B, 3 + 5);
			emit_alu_imm_reg(cd, ALU_SUB, 1, d);
			emit_jmp_imm(cd, 3);
			emit_alu_imm_reg(cd, ALU_ADD, 1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_FCMPG:      /* ..., val1, val2  ==> ..., val1 fcmpg val2    */
		case ICMD_DCMPG:

			/* exchanged to skip fxch */
			s2 = emit_load_s1(jd, iptr, REG_FTMP1);
			s1 = emit_load_s2(jd, iptr, REG_FTMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
/*    			emit_fxch(cd); */
			emit_fucompp(cd);
			emit_fnstsw(cd);
			emit_test_imm_reg(cd, 0x400, EAX);    /* unordered treat as LT */
			emit_jcc(cd, CC_E, 3);
			emit_movb_imm_reg(cd, 1, REG_AH);
 			emit_sahf(cd);
			emit_mov_imm_reg(cd, 0, d);    /* does not affect flags */
  			emit_jcc(cd, CC_E, 6 + 3 + 5 + 3);
  			emit_jcc(cd, CC_B, 3 + 5);
			emit_alu_imm_reg(cd, ALU_SUB, 1, d);
			emit_jmp_imm(cd, 3);
			emit_alu_imm_reg(cd, ALU_ADD, 1, d);
			emit_store_dst(jd, iptr, d);
			break;


		/* memory operations **************************************************/

		case ICMD_ARRAYLENGTH: /* ..., arrayref  ==> ..., length              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			M_ILD(d, s1, OFFSET(java_arrayheader, size));
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_BALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
   			emit_movsbl_memindex_reg(cd, OFFSET(java_bytearray, data[0]), 
									 s1, s2, 0, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_CALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movzwl_memindex_reg(cd, OFFSET(java_chararray, data[0]), 
									 s1, s2, 1, d);
			emit_store_dst(jd, iptr, d);
			break;			

		case ICMD_SALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movswl_memindex_reg(cd, OFFSET(java_shortarray, data[0]), 
									 s1, s2, 1, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_IALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_mov_memindex_reg(cd, OFFSET(java_intarray, data[0]), 
								  s1, s2, 2, d);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_LALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP3);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);

			var = VAROP(iptr->dst);

			assert(var->flags & INMEMORY);
			emit_mov_memindex_reg(cd, OFFSET(java_longarray, data[0]), 
								  s1, s2, 3, REG_ITMP3);
			emit_mov_reg_membase(cd, REG_ITMP3, REG_SP, var->vv.regoff * 4);
			emit_mov_memindex_reg(cd, OFFSET(java_longarray, data[0]) + 4, 
								  s1, s2, 3, REG_ITMP3);
			emit_mov_reg_membase(cd, REG_ITMP3, REG_SP, var->vv.regoff * 4 + 4);
			break;

		case ICMD_FALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_flds_memindex(cd, OFFSET(java_floatarray, data[0]), s1, s2, 2);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_DALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_FTMP3);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_fldl_memindex(cd, OFFSET(java_doublearray, data[0]), s1, s2,3);
			emit_store_dst(jd, iptr, d);
			break;

		case ICMD_AALOAD:     /* ..., arrayref, index  ==> ..., value         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			d = codegen_reg_of_dst(jd, iptr, REG_ITMP1);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_mov_memindex_reg(cd, OFFSET(java_objectarray, data[0]),
								  s1, s2, 2, d);
			emit_store_dst(jd, iptr, d);
			break;


		case ICMD_BASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			if (s3 >= EBP) { 
				/* because EBP, ESI, EDI have no xH and xL nibbles */
				M_INTMOVE(s3, REG_ITMP3);
				s3 = REG_ITMP3;
			}
			emit_movb_reg_memindex(cd, s3, OFFSET(java_bytearray, data[0]),
								   s1, s2, 0);
			break;

		case ICMD_CASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			emit_movw_reg_memindex(cd, s3, OFFSET(java_chararray, data[0]),
								   s1, s2, 1);
			break;

		case ICMD_SASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			emit_movw_reg_memindex(cd, s3, OFFSET(java_shortarray, data[0]),
								   s1, s2, 1);
			break;

		case ICMD_IASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			emit_mov_reg_memindex(cd, s3, OFFSET(java_intarray, data[0]),
								  s1, s2, 2);
			break;

		case ICMD_LASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);

			var = VAROP(iptr->sx.s23.s3);

			assert(var->flags & INMEMORY);
			emit_mov_membase_reg(cd, REG_SP, var->vv.regoff * 4, REG_ITMP3);
			emit_mov_reg_memindex(cd, REG_ITMP3, OFFSET(java_longarray, data[0])
								  , s1, s2, 3);
			emit_mov_membase_reg(cd, REG_SP, var->vv.regoff * 4 + 4, REG_ITMP3);
			emit_mov_reg_memindex(cd, REG_ITMP3,
							    OFFSET(java_longarray, data[0]) + 4, s1, s2, 3);
			break;

		case ICMD_FASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_FTMP1);
			emit_fstps_memindex(cd, OFFSET(java_floatarray, data[0]), s1, s2,2);
			break;

		case ICMD_DASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_FTMP1);
			emit_fstpl_memindex(cd, OFFSET(java_doublearray, data[0]),
								s1, s2, 3);
			break;

		case ICMD_AASTORE:    /* ..., arrayref, index, value  ==> ...         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);

			M_AST(s1, REG_SP, 0 * 4);
			M_AST(s3, REG_SP, 1 * 4);
			M_MOV_IMM(BUILTIN_canstore, REG_ITMP1);
			M_CALL(REG_ITMP1);
			emit_exception_check(cd, iptr);

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			s3 = emit_load_s3(jd, iptr, REG_ITMP3);
			emit_mov_reg_memindex(cd, s3, OFFSET(java_objectarray, data[0]),
								  s1, s2, 2);
			break;

		case ICMD_BASTORECONST: /* ..., arrayref, index  ==> ...              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movb_imm_memindex(cd, iptr->sx.s23.s3.constval,
								   OFFSET(java_bytearray, data[0]), s1, s2, 0);
			break;

		case ICMD_CASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movw_imm_memindex(cd, iptr->sx.s23.s3.constval,
								   OFFSET(java_chararray, data[0]), s1, s2, 1);
			break;

		case ICMD_SASTORECONST:   /* ..., arrayref, index  ==> ...            */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_movw_imm_memindex(cd, iptr->sx.s23.s3.constval,
								   OFFSET(java_shortarray, data[0]), s1, s2, 1);
			break;

		case ICMD_IASTORECONST: /* ..., arrayref, index  ==> ...              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_mov_imm_memindex(cd, iptr->sx.s23.s3.constval,
								  OFFSET(java_intarray, data[0]), s1, s2, 2);
			break;

		case ICMD_LASTORECONST: /* ..., arrayref, index  ==> ...              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_mov_imm_memindex(cd, 
						   (u4) (iptr->sx.s23.s3.constval & 0x00000000ffffffff),
						   OFFSET(java_longarray, data[0]), s1, s2, 3);
			emit_mov_imm_memindex(cd, 
						        ((s4)iptr->sx.s23.s3.constval) >> 31, 
						        OFFSET(java_longarray, data[0]) + 4, s1, s2, 3);
			break;

		case ICMD_AASTORECONST: /* ..., arrayref, index  ==> ...              */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			/* implicit null-pointer check */
			emit_arrayindexoutofbounds_check(cd, iptr, s1, s2);
			emit_mov_imm_memindex(cd, 0, 
								  OFFSET(java_objectarray, data[0]), s1, s2, 2);
			break;


		case ICMD_GETSTATIC:  /* ...  ==> ..., value                          */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = 0;

				codegen_addpatchref(cd, PATCHER_get_putstatic, uf, 0);

			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = (ptrint) &(fi->value);

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class))
					codegen_addpatchref(cd, PATCHER_clinit, fi->class, 0);
  			}

			M_MOV_IMM(disp, REG_ITMP1);
			switch (fieldtype) {
			case TYPE_INT:
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD(d, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP23_PACKED);
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
				disp      = 0;

				codegen_addpatchref(cd, PATCHER_get_putstatic, uf, 0);
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = (ptrint) &(fi->value);

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class))
					codegen_addpatchref(cd, PATCHER_clinit, fi->class, 0);
  			}

			M_MOV_IMM(disp, REG_ITMP1);
			switch (fieldtype) {
			case TYPE_INT:
			case TYPE_ADR:
				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_IST(s1, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				s1 = emit_load_s1(jd, iptr, REG_ITMP23_PACKED);
				M_LST(s1, REG_ITMP1, 0);
				break;
			case TYPE_FLT:
				s1 = emit_load_s1(jd, iptr, REG_FTMP1);
				emit_fstps_membase(cd, REG_ITMP1, 0);
				break;
			case TYPE_DBL:
				s1 = emit_load_s1(jd, iptr, REG_FTMP1);
				emit_fstpl_membase(cd, REG_ITMP1, 0);
				break;
			}
			break;

		case ICMD_PUTSTATICCONST: /* ...  ==> ...                             */
		                          /* val = value (in current instruction)     */
		                          /* following NOP)                           */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				uf        = iptr->sx.s23.s3.uf;
				fieldtype = uf->fieldref->parseddesc.fd->type;
				disp      = 0;

				codegen_addpatchref(cd, PATCHER_get_putstatic, uf, 0);
			}
			else {
				fi        = iptr->sx.s23.s3.fmiref->p.field;
				fieldtype = fi->type;
				disp      = (ptrint) &(fi->value);

				if (!CLASS_IS_OR_ALMOST_INITIALIZED(fi->class))
					codegen_addpatchref(cd, PATCHER_clinit, fi->class, 0);
  			}

			M_MOV_IMM(disp, REG_ITMP1);
			switch (fieldtype) {
			case TYPE_INT:
			case TYPE_ADR:
				M_IST_IMM(iptr->sx.s23.s2.constval, REG_ITMP1, 0);
				break;
			case TYPE_LNG:
				M_IST_IMM(iptr->sx.s23.s2.constval & 0xffffffff, REG_ITMP1, 0);
				M_IST_IMM(((s4)iptr->sx.s23.s2.constval) >> 31, REG_ITMP1, 4);
				break;
			default:
				assert(0);
			}
			break;

		case ICMD_GETFIELD:   /* .., objectref.  ==> ..., value               */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			emit_nullpointer_check(cd, iptr, s1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				fieldtype = uf->fieldref->parseddesc.fd->type;

				codegen_addpatchref(cd, PATCHER_getfield,
									iptr->sx.s23.s3.uf, 0);

				disp = 0;

			}
			else {
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;
				
				fieldtype = fi->type;
				disp = fi->offset;
			}

			switch (fieldtype) {
			case TYPE_INT:
			case TYPE_ADR:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP2);
				M_ILD32(d, s1, disp);
				break;
			case TYPE_LNG:
				d = codegen_reg_of_dst(jd, iptr, REG_ITMP23_PACKED);
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
			emit_nullpointer_check(cd, iptr, s1);

			/* must be done here because of code patching */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				fieldtype = uf->fieldref->parseddesc.fd->type;
			}
			else {
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;

				fieldtype = fi->type;
			}

			if (!IS_FLT_DBL_TYPE(fieldtype)) {
				if (IS_2_WORD_TYPE(fieldtype))
					s2 = emit_load_s2(jd, iptr, REG_ITMP23_PACKED);
				else
					s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			}
			else
				s2 = emit_load_s2(jd, iptr, REG_FTMP2);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				codegen_addpatchref(cd, PATCHER_putfield, uf, 0);

				disp = 0;

			}
			else {
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;

				disp = fi->offset;
			}

			switch (fieldtype) {
			case TYPE_INT:
			case TYPE_ADR:
				M_IST32(s2, s1, disp);
				break;
			case TYPE_LNG:
				M_LST32(s2, s1, disp);
				break;
			case TYPE_FLT:
				emit_fstps_membase32(cd, s1, disp);
				break;
			case TYPE_DBL:
				emit_fstpl_membase32(cd, s1, disp);
				break;
			}
			break;

		case ICMD_PUTFIELDCONST:  /* ..., objectref  ==> ...                  */
		                          /* val = value (in current instruction)     */
		                          /* following NOP)                           */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			emit_nullpointer_check(cd, iptr, s1);

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				unresolved_field *uf = iptr->sx.s23.s3.uf;

				fieldtype = uf->fieldref->parseddesc.fd->type;

				codegen_addpatchref(cd, PATCHER_putfieldconst,
									uf, 0);

				disp = 0;

			}
			else
			{
				fieldinfo *fi = iptr->sx.s23.s3.fmiref->p.field;

				fieldtype = fi->type;
				disp = fi->offset;
			}


			switch (fieldtype) {
			case TYPE_INT:
			case TYPE_ADR:
				M_IST32_IMM(iptr->sx.s23.s2.constval, s1, disp);
				break;
			case TYPE_LNG:
				M_IST32_IMM(iptr->sx.s23.s2.constval & 0xffffffff, s1, disp);
				M_IST32_IMM(((s4)iptr->sx.s23.s2.constval) >> 31, s1, disp + 4);
				break;
			default:
				assert(0);
			}
			break;


		/* branch operations **************************************************/

		case ICMD_ATHROW:       /* ..., objectref ==> ... (, objectref)       */

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			M_INTMOVE(s1, REG_ITMP1_XPTR);

#ifdef ENABLE_VERIFIER
			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				codegen_addpatchref(cd, PATCHER_athrow_areturn,
									iptr->sx.s23.s2.uc, 0);
			}
#endif /* ENABLE_VERIFIER */

			M_CALL_IMM(0);                            /* passing exception pc */
			M_POP(REG_ITMP2_XPC);

			M_MOV_IMM(asm_handle_exception, REG_ITMP3);
			M_JMP(REG_ITMP3);
			break;

		case ICMD_GOTO:         /* ... ==> ...                                */
		case ICMD_RET:          /* ... ==> ...                                */

#if defined(ENABLE_SSA)
			if ( ls != NULL ) {
				last_cmd_was_goto = true;
				/* In case of a Goto phimoves have to be inserted before the */
				/* jump */
				codegen_insert_phi_moves(jd, bptr);
			}
#endif
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
			M_CMP_IMM(iptr->sx.val.i, s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IFEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_LEQ:       /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			if (iptr->sx.val.l == 0) {
				M_INTMOVE(GET_LOW_REG(s1), REG_ITMP1);
				M_OR(GET_HIGH_REG(s1), REG_ITMP1);
			}
			else {
				M_LNGMOVE(s1, REG_ITMP12_PACKED);
				M_XOR_IMM(iptr->sx.val.l, REG_ITMP1);
				M_XOR_IMM(iptr->sx.val.l >> 32, REG_ITMP2);
				M_OR(REG_ITMP2, REG_ITMP1);
			}
			emit_beq(cd, iptr->dst.block);
			break;

		case ICMD_IF_LLT:       /* ..., value ==> ...                         */

			if (iptr->sx.val.l == 0) {
				/* If high 32-bit are less than zero, then the 64-bits
				   are too. */
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
				M_CMP_IMM(0, s1);
				emit_blt(cd, iptr->dst.block);
			}
			else {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_CMP_IMM(iptr->sx.val.l >> 32, GET_HIGH_REG(s1));
				emit_blt(cd, iptr->dst.block);
				M_BGT(6 + 6);
				M_CMP_IMM32(iptr->sx.val.l, GET_LOW_REG(s1));
				emit_bult(cd, iptr->dst.block);
			}			
			break;

		case ICMD_IF_LLE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			M_CMP_IMM(iptr->sx.val.l >> 32, GET_HIGH_REG(s1));
			emit_blt(cd, iptr->dst.block);
			M_BGT(6 + 6);
			M_CMP_IMM32(iptr->sx.val.l, GET_LOW_REG(s1));
			emit_bule(cd, iptr->dst.block);
			break;

		case ICMD_IF_LNE:       /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			if (iptr->sx.val.l == 0) {
				M_INTMOVE(GET_LOW_REG(s1), REG_ITMP1);
				M_OR(GET_HIGH_REG(s1), REG_ITMP1);
			}
			else {
				M_LNGMOVE(s1, REG_ITMP12_PACKED);
				M_XOR_IMM(iptr->sx.val.l, REG_ITMP1);
				M_XOR_IMM(iptr->sx.val.l >> 32, REG_ITMP2);
				M_OR(REG_ITMP2, REG_ITMP1);
			}
			emit_bne(cd, iptr->dst.block);
			break;

		case ICMD_IF_LGT:       /* ..., value ==> ...                         */

			s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
			M_CMP_IMM(iptr->sx.val.l >> 32, GET_HIGH_REG(s1));
			emit_bgt(cd, iptr->dst.block);
			M_BLT(6 + 6);
			M_CMP_IMM32(iptr->sx.val.l, GET_LOW_REG(s1));
			emit_bugt(cd, iptr->dst.block);
			break;

		case ICMD_IF_LGE:       /* ..., value ==> ...                         */

			if (iptr->sx.val.l == 0) {
				/* If high 32-bit are greater equal zero, then the
				   64-bits are too. */
				s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
				M_CMP_IMM(0, s1);
				emit_bge(cd, iptr->dst.block);
			}
			else {
				s1 = emit_load_s1(jd, iptr, REG_ITMP12_PACKED);
				M_CMP_IMM(iptr->sx.val.l >> 32, GET_HIGH_REG(s1));
				emit_bgt(cd, iptr->dst.block);
				M_BLT(6 + 6);
				M_CMP_IMM32(iptr->sx.val.l, GET_LOW_REG(s1));
				emit_buge(cd, iptr->dst.block);
			}
			break;

		case ICMD_IF_ICMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ICMPNE:
		case ICMD_IF_ICMPLT:
		case ICMD_IF_ICMPGT:
		case ICMD_IF_ICMPGE:
		case ICMD_IF_ICMPLE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s2, s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_ICMPEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_ACMPEQ:    /* ..., value, value ==> ...                  */
		case ICMD_IF_ACMPNE:

			s1 = emit_load_s1(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2(jd, iptr, REG_ITMP2);
			M_CMP(s2, s1);
			emit_bcc(cd, iptr->dst.block, iptr->opc - ICMD_IF_ACMPEQ, BRANCH_OPT_NONE);
			break;

		case ICMD_IF_LCMPEQ:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_INTMOVE(s1, REG_ITMP1);
			M_XOR(s2, REG_ITMP1);
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);
			M_INTMOVE(s1, REG_ITMP2);
			M_XOR(s2, REG_ITMP2);
			M_OR(REG_ITMP1, REG_ITMP2);
			emit_beq(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPNE:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_INTMOVE(s1, REG_ITMP1);
			M_XOR(s2, REG_ITMP1);
			s1 = emit_load_s1_high(jd, iptr, REG_ITMP2);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP3);
			M_INTMOVE(s1, REG_ITMP2);
			M_XOR(s2, REG_ITMP2);
			M_OR(REG_ITMP1, REG_ITMP2);
			emit_bne(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPLT:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s2, s1);
			emit_blt(cd, iptr->dst.block);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_BGT(2 + 6);
			M_CMP(s2, s1);
			emit_bult(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPGT:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s2, s1);
			emit_bgt(cd, iptr->dst.block);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_BLT(2 + 6);
			M_CMP(s2, s1);
			emit_bugt(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPLE:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s2, s1);
			emit_blt(cd, iptr->dst.block);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_BGT(2 + 6);
			M_CMP(s2, s1);
			emit_bule(cd, iptr->dst.block);
			break;

		case ICMD_IF_LCMPGE:    /* ..., value, value ==> ...                  */

			s1 = emit_load_s1_high(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_high(jd, iptr, REG_ITMP2);
			M_CMP(s2, s1);
			emit_bgt(cd, iptr->dst.block);
			s1 = emit_load_s1_low(jd, iptr, REG_ITMP1);
			s2 = emit_load_s2_low(jd, iptr, REG_ITMP2);
			M_BLT(2 + 6);
			M_CMP(s2, s1);
			emit_buge(cd, iptr->dst.block);
			break;


		case ICMD_IRETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT);
			M_INTMOVE(s1, REG_RESULT);
			goto nowperformreturn;

		case ICMD_LRETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT_PACKED);
			M_LNGMOVE(s1, REG_RESULT_PACKED);
			goto nowperformreturn;

		case ICMD_ARETURN:      /* ..., retvalue ==> ...                      */

			REPLACEMENT_POINT_RETURN(cd, iptr);
			s1 = emit_load_s1(jd, iptr, REG_RESULT);
			M_INTMOVE(s1, REG_RESULT);

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
			goto nowperformreturn;

		case ICMD_RETURN:      /* ...  ==> ...                                */

			REPLACEMENT_POINT_RETURN(cd, iptr);

nowperformreturn:
			{
			s4 i, p;
			
  			p = cd->stackframesize;
			
#if !defined(NDEBUG)
			emit_verbosecall_exit(jd);
#endif

#if defined(ENABLE_THREADS)
			if (checksync && (m->flags & ACC_SYNCHRONIZED)) {
				M_ALD(REG_ITMP2, REG_SP, rd->memuse * 4);

				/* we need to save the proper return value */
				switch (iptr->opc) {
				case ICMD_IRETURN:
				case ICMD_ARETURN:
					M_IST(REG_RESULT, REG_SP, rd->memuse * 4);
					break;

				case ICMD_LRETURN:
					M_LST(REG_RESULT_PACKED, REG_SP, rd->memuse * 4);
					break;

				case ICMD_FRETURN:
					emit_fstps_membase(cd, REG_SP, rd->memuse * 4);
					break;

				case ICMD_DRETURN:
					emit_fstpl_membase(cd, REG_SP, rd->memuse * 4);
					break;
				}

				M_AST(REG_ITMP2, REG_SP, 0);
				M_MOV_IMM(LOCK_monitor_exit, REG_ITMP3);
				M_CALL(REG_ITMP3);

				/* and now restore the proper return value */
				switch (iptr->opc) {
				case ICMD_IRETURN:
				case ICMD_ARETURN:
					M_ILD(REG_RESULT, REG_SP, rd->memuse * 4);
					break;

				case ICMD_LRETURN:
					M_LLD(REG_RESULT_PACKED, REG_SP, rd->memuse * 4);
					break;

				case ICMD_FRETURN:
					emit_flds_membase(cd, REG_SP, rd->memuse * 4);
					break;

				case ICMD_DRETURN:
					emit_fldl_membase(cd, REG_SP, rd->memuse * 4);
					break;
				}
			}
#endif

			/* restore saved registers */

			for (i = INT_SAV_CNT - 1; i >= rd->savintreguse; i--) {
				p--; M_ALD(rd->savintregs[i], REG_SP, p * 4);
			}

			for (i = FLT_SAV_CNT - 1; i >= rd->savfltreguse; i--) {
  				p--;
				emit_fldl_membase(cd, REG_SP, p * 4);
				if (iptr->opc == ICMD_FRETURN || iptr->opc == ICMD_DRETURN) {
					assert(0);
/* 					emit_fstp_reg(cd, rd->savfltregs[i] + fpu_st_offset + 1); */
				} else {
					assert(0);
/* 					emit_fstp_reg(cd, rd->savfltregs[i] + fpu_st_offset); */
				}
			}

			/* deallocate stack */

			if (cd->stackframesize)
				M_AADD_IMM(cd->stackframesize * 4, REG_SP);

			emit_ret(cd);
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

				i = i - l + 1;

                /* range check */

				M_CMP_IMM(i - 1, REG_ITMP1);
				emit_bugt(cd, table[0].block);

				/* build jump table top down and use address of lowest entry */

				table += i;

				while (--i >= 0) {
					dseg_add_target(cd, table->block); 
					--table;
				}

				/* length of dataseg after last dseg_addtarget is used
				   by load */

				M_MOV_IMM(0, REG_ITMP2);
				dseg_adddata(cd);
				emit_mov_memindex_reg(cd, -(cd->dseglen), REG_ITMP2, REG_ITMP1, 2, REG_ITMP1);
				M_JMP(REG_ITMP1);
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
					M_CMP_IMM(lookup->value, s1);
					emit_beq(cd, lookup->target.block);
					lookup++;
				}

				emit_br(cd, iptr->sx.s23.s3.lookupdefault.block);
				ALIGNCODENOP;
			}
			break;

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
				md = iptr->sx.s23.s3.um->methodref->parseddesc.md;
				lm = NULL;
			}
			else {
				lm = iptr->sx.s23.s3.fmiref->p.method;
				md = lm->parseddesc;
			}

gen_method:
			s3 = md->paramcount;

			MCODECHECK((s3 << 1) + 64);

			/* copy arguments to registers or stack location                  */

			for (s3 = s3 - 1; s3 >= 0; s3--) {
				var = VAR(iptr->sx.s23.s2.args[s3]);
	  
				/* Already Preallocated (ARGVAR) ? */
				if (var->flags & PREALLOC)
					continue;
				if (IS_INT_LNG_TYPE(var->type)) {
					if (!md->params[s3].inmemory) {
						log_text("No integer argument registers available!");
						assert(0);

					} else {
						if (IS_2_WORD_TYPE(var->type)) {
							d = emit_load(jd, iptr, var, REG_ITMP12_PACKED);
							M_LST(d, REG_SP, md->params[s3].regoff * 4);
						} else {
							d = emit_load(jd, iptr, var, REG_ITMP1);
							M_IST(d, REG_SP, md->params[s3].regoff * 4);
						}
					}

				} else {
					if (!md->params[s3].inmemory) {
						s1 = md->params[s3].regoff;
						d = emit_load(jd, iptr, var, s1);
						M_FLTMOVE(d, s1);

					} else {
						d = emit_load(jd, iptr, var, REG_FTMP1);
						if (IS_2_WORD_TYPE(var->type))
							M_DST(d, REG_SP, md->params[s3].regoff * 4);
						else
							M_FST(d, REG_SP, md->params[s3].regoff * 4);
					}
				}
			} /* end of for */

			switch (iptr->opc) {
			case ICMD_BUILTIN:
				disp = (ptrint) bte->fp;
				d = md->returntype.type;

				M_MOV_IMM(disp, REG_ITMP1);
				M_CALL(REG_ITMP1);

				emit_exception_check(cd, iptr);
				break;

			case ICMD_INVOKESPECIAL:
				M_ALD(REG_ITMP1, REG_SP, 0 * 4);
				emit_nullpointer_check(cd, iptr, REG_ITMP1);
				/* fall through */

			case ICMD_INVOKESTATIC:
				if (lm == NULL) {
					unresolved_method *um = iptr->sx.s23.s3.um;

					codegen_addpatchref(cd, PATCHER_invokestatic_special,
										um, 0);

					disp = 0;
					d = md->returntype.type;
				}
				else {
					disp = (ptrint) lm->stubroutine;
					d = lm->parseddesc->returntype.type;
				}

				M_MOV_IMM(disp, REG_ITMP2);
				M_CALL(REG_ITMP2);
				break;

			case ICMD_INVOKEVIRTUAL:
				M_ALD(REG_ITMP1, REG_SP, 0 * 4);
				emit_nullpointer_check(cd, iptr, s1);

				if (lm == NULL) {
					unresolved_method *um = iptr->sx.s23.s3.um;

					codegen_addpatchref(cd, PATCHER_invokevirtual, um, 0);

					s1 = 0;
					d = md->returntype.type;
				}
				else {
					s1 = OFFSET(vftbl_t, table[0]) +
						sizeof(methodptr) * lm->vftblindex;
					d = md->returntype.type;
				}

				M_ALD(REG_METHODPTR, REG_ITMP1,
					  OFFSET(java_objectheader, vftbl));
				M_ALD32(REG_ITMP3, REG_METHODPTR, s1);
				M_CALL(REG_ITMP3);
				break;

			case ICMD_INVOKEINTERFACE:
				M_ALD(REG_ITMP1, REG_SP, 0 * 4);
				emit_nullpointer_check(cd, iptr, s1);

				if (lm == NULL) {
					unresolved_method *um = iptr->sx.s23.s3.um;

					codegen_addpatchref(cd, PATCHER_invokeinterface, um, 0);

					s1 = 0;
					s2 = 0;
					d = md->returntype.type;
				}
				else {
					s1 = OFFSET(vftbl_t, interfacetable[0]) -
						sizeof(methodptr) * lm->class->index;

					s2 = sizeof(methodptr) * (lm - lm->class->methods);

					d = md->returntype.type;
				}

				M_ALD(REG_METHODPTR, REG_ITMP1,
					  OFFSET(java_objectheader, vftbl));
				M_ALD32(REG_METHODPTR, REG_METHODPTR, s1);
				M_ALD32(REG_ITMP3, REG_METHODPTR, s2);
				M_CALL(REG_ITMP3);
				break;
			}

			/* store size of call code in replacement point */

			REPLACEMENT_POINT_INVOKE_RETURN(cd, iptr);

			/* d contains return type */

			if (d != TYPE_VOID) {
#if defined(ENABLE_SSA)
				if ((ls == NULL) || (!IS_TEMPVAR_INDEX(iptr->dst.varindex)) ||
					(ls->lifetime[-iptr->dst.varindex-1].type != -1)) 
					/* a "living" stackslot */
#endif
				{
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
						s1 = codegen_reg_of_dst(jd, iptr, REG_NULL);
					}
					emit_store_dst(jd, iptr, s1);
				}
			}
			break;


		case ICMD_CHECKCAST:  /* ..., objectref ==> ..., objectref            */

			if (!(iptr->flags.bits & INS_FLAG_ARRAY)) {
				/* object type cast-check */

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
			
				if ((super == NULL) || !(super->flags & ACC_INTERFACE))
					CODEGEN_CRITICAL_SECTION_NEW;

				s1 = emit_load_s1(jd, iptr, REG_ITMP1);

				/* if class is not resolved, check which code to call */

				if (super == NULL) {
					M_TEST(s1);
					emit_label_beq(cd, BRANCH_LABEL_1);

					codegen_addpatchref(cd, PATCHER_checkcast_instanceof_flags,
										iptr->sx.s23.s3.c.ref, 0);

					M_MOV_IMM(0, REG_ITMP2);                  /* super->flags */
					M_AND_IMM32(ACC_INTERFACE, REG_ITMP2);
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
						codegen_addpatchref(cd, PATCHER_checkcast_interface,
											iptr->sx.s23.s3.c.ref,
											0);
					}

					M_ILD32(REG_ITMP3,
							REG_ITMP2, OFFSET(vftbl_t, interfacetablelength));
					M_ISUB_IMM32(superindex, REG_ITMP3);
					/* XXX do we need this one? */
					M_TEST(REG_ITMP3);
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
						codegen_addpatchref(cd, PATCHER_checkcast_class,
											iptr->sx.s23.s3.c.ref,
											0);
					}

					M_MOV_IMM(supervftbl, REG_ITMP3);

					CODEGEN_CRITICAL_SECTION_START;

					M_ILD32(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, baseval));

					/* 				if (s1 != REG_ITMP1) { */
					/* 					emit_mov_membase_reg(cd, REG_ITMP3, OFFSET(vftbl_t, baseval), REG_ITMP1); */
					/* 					emit_mov_membase_reg(cd, REG_ITMP3, OFFSET(vftbl_t, diffval), REG_ITMP3); */
					/* #if defined(ENABLE_THREADS) */
					/* 					codegen_threadcritstop(cd, cd->mcodeptr - cd->mcodebase); */
					/* #endif */
					/* 					emit_alu_reg_reg(cd, ALU_SUB, REG_ITMP1, REG_ITMP2); */

					/* 				} else { */
					M_ILD32(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, baseval));
					M_ISUB(REG_ITMP3, REG_ITMP2);
					M_MOV_IMM(supervftbl, REG_ITMP3);
					M_ILD(REG_ITMP3, REG_ITMP3, OFFSET(vftbl_t, diffval));

					CODEGEN_CRITICAL_SECTION_END;

					/* 				} */

					M_CMP(REG_ITMP3, REG_ITMP2);
					emit_classcast_check(cd, iptr, BRANCH_ULE, REG_ITMP3, s1);

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
				M_AST(s1, REG_SP, 0 * 4);

				if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
					codegen_addpatchref(cd, PATCHER_builtin_arraycheckcast,
										iptr->sx.s23.s3.c.ref, 0);
				}

				M_AST_IMM(iptr->sx.s23.s3.c.cls, REG_SP, 1 * 4);
				M_MOV_IMM(BUILTIN_arraycheckcast, REG_ITMP3);
				M_CALL(REG_ITMP3);

				s1 = emit_load_s1(jd, iptr, REG_ITMP2);
				M_TEST(REG_RESULT);
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
				M_INTMOVE(s1, REG_ITMP1);
				s1 = REG_ITMP1;
			}

			M_CLR(d);

			/* if class is not resolved, check which code to call */

			if (super == NULL) {
				M_TEST(s1);
				emit_label_beq(cd, BRANCH_LABEL_1);

				codegen_addpatchref(cd, PATCHER_checkcast_instanceof_flags,
									iptr->sx.s23.s3.c.ref, 0);

				M_MOV_IMM(0, REG_ITMP3);                      /* super->flags */
				M_AND_IMM32(ACC_INTERFACE, REG_ITMP3);
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
					codegen_addpatchref(cd, PATCHER_instanceof_interface,
										iptr->sx.s23.s3.c.ref, 0);
				}

				M_ILD32(REG_ITMP3,
						REG_ITMP1, OFFSET(vftbl_t, interfacetablelength));
				M_ISUB_IMM32(superindex, REG_ITMP3);
				M_TEST(REG_ITMP3);

				disp = (2 + 4 /* mov_membase32_reg */ + 2 /* test */ +
						6 /* jcc */ + 5 /* mov_imm_reg */);

				M_BLE(disp);
				M_ALD32(REG_ITMP1, REG_ITMP1,
						OFFSET(vftbl_t, interfacetable[0]) -
						superindex * sizeof(methodptr*));
				M_TEST(REG_ITMP1);
/*  					emit_setcc_reg(cd, CC_A, d); */
/*  					emit_jcc(cd, CC_BE, 5); */
				M_BEQ(5);
				M_MOV_IMM(1, d);

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
					codegen_addpatchref(cd, PATCHER_instanceof_class,
										iptr->sx.s23.s3.c.ref, 0);
				}

				M_MOV_IMM(supervftbl, REG_ITMP2);

				CODEGEN_CRITICAL_SECTION_START;

				M_ILD(REG_ITMP1, REG_ITMP1, OFFSET(vftbl_t, baseval));
				M_ILD(REG_ITMP3, REG_ITMP2, OFFSET(vftbl_t, diffval));
				M_ILD(REG_ITMP2, REG_ITMP2, OFFSET(vftbl_t, baseval));

				CODEGEN_CRITICAL_SECTION_END;

				M_ISUB(REG_ITMP2, REG_ITMP1);
				M_CLR(d);                                 /* may be REG_ITMP2 */
				M_CMP(REG_ITMP3, REG_ITMP1);
				M_BA(5);
				M_MOV_IMM(1, d);

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
				/* copy SAVEDVAR sizes to stack */
				var = VAR(iptr->sx.s23.s2.args[s1]);

				/* Already Preallocated? */
				if (!(var->flags & PREALLOC)) {
					if (var->flags & INMEMORY) {
						M_ILD(REG_ITMP1, REG_SP, var->vv.regoff * 4);
						M_IST(REG_ITMP1, REG_SP, (s1 + 3) * 4);
					}
					else
						M_IST(var->vv.regoff, REG_SP, (s1 + 3) * 4);
				}
			}

			/* is a patcher function set? */

			if (INSTRUCTION_IS_UNRESOLVED(iptr)) {
				codegen_addpatchref(cd, PATCHER_builtin_multianewarray,
									iptr->sx.s23.s3.c.ref, 0);

				disp = 0;

			}
			else
				disp = (ptrint) iptr->sx.s23.s3.c.cls;

			/* a0 = dimension count */

			M_IST_IMM(iptr->s1.argcount, REG_SP, 0 * 4);

			/* a1 = arraydescriptor */

			M_IST_IMM(disp, REG_SP, 1 * 4);

			/* a2 = pointer to dimensions = stack pointer */

			M_MOV(REG_SP, REG_ITMP1);
			M_AADD_IMM(3 * 4, REG_ITMP1);
			M_AST(REG_ITMP1, REG_SP, 2 * 4);

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
		
	MCODECHECK(64);

#if defined(ENABLE_LSRA) && !defined(ENABLE_SSA)
	if (!opt_lsra)
#endif
#if defined(ENABLE_SSA)
	if ( ls != NULL ) {
		/* by edge splitting, in Blocks with phi moves there can only */
		/* be a goto as last command, no other Jump/Branch Command    */
		if (!last_cmd_was_goto)
			codegen_insert_phi_moves(jd, bptr);
	}

#endif

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

#if defined(ENABLE_SSA)
void codegen_insert_phi_moves(jitdata *jd, basicblock *bptr) {
	/* look for phi moves */
	int t_a,s_a,i, type;
	int t_lt, s_lt; /* lifetime indices of phi_moves */
	s4 t_regoff, s_regoff, s_flags, t_flags;
	codegendata *cd;
	lsradata *ls;

	MCODECHECK(512);

	ls = jd->ls;
	cd = jd->cd;
	
	/* Moves from phi functions with highest indices have to be */
	/* inserted first, since this is the order as is used for   */
	/* conflict resolution */
	for(i = ls->num_phi_moves[bptr->nr] - 1; i >= 0 ; i--) {
		t_a = ls->phi_moves[bptr->nr][i][0];
		s_a = ls->phi_moves[bptr->nr][i][1];
#if defined(SSA_DEBUG_VERBOSE)
		if (compileverbose)
			printf("BB %3i Move %3i <- %3i ", bptr->nr, t_a, s_a);
#endif
		if (t_a >= 0) {
			/* local var lifetimes */
			t_lt = ls->maxlifetimes + t_a;
			type = ls->lifetime[t_lt].type;
		}
		else {
			t_lt = -t_a-1;
			type = ls->lifetime[t_lt].local_ss->s->type;
			/* stackslot lifetime */
		}

		if (type == -1) {
#if defined(SSA_DEBUG_VERBOSE)
			if (compileverbose)
				printf("...returning - phi lifetimes where joined\n");
#endif
			return;
		}

		if (s_a >= 0) {
			/* local var lifetimes */
			s_lt = ls->maxlifetimes + s_a;
			type = ls->lifetime[s_lt].type;
		}
		else {
			s_lt = -s_a-1;
			type = ls->lifetime[s_lt].type;
			/* stackslot lifetime */
		}

		if (type == -1) {
#if defined(SSA_DEBUG_VERBOSE)
			if (compileverbose)
				printf("...returning - phi lifetimes where joined\n");
#endif
			return;
		}

		if (t_a >= 0) {
			t_flags = VAR(t_a)->flags;
			t_regoff = VAR(t_a)->vv.regoff;
			
		}
		else {
			t_flags = ls->lifetime[t_lt].local_ss->s->flags;
			t_regoff = ls->lifetime[t_lt].local_ss->s->regoff;
		}

		if (s_a >= 0) {
			/* local var move */
			s_flags = VAR(s_a)->flags;
			s_regoff = VAR(s_a)->vv.regoff;
		} else {
			/* stackslot lifetime */
			s_flags = ls->lifetime[s_lt].local_ss->s->flags;
			s_regoff = ls->lifetime[s_lt].local_ss->s->regoff;
		}

		if (type == -1) {
#if defined(SSA_DEBUG_VERBOSE)
			if (compileverbose)
				printf("...returning - phi lifetimes where joined\n");
#endif
			return;
		}

		cg_move(cd, type, s_regoff, s_flags, t_regoff, t_flags);

#if defined(SSA_DEBUG_VERBOSE)
		if (compileverbose) {
			if (IS_INMEMORY(t_flags) && IS_INMEMORY(s_flags)) {
				/* mem -> mem */
				printf("M%3i <- M%3i",t_regoff,s_regoff);
			}
			else 	if (IS_INMEMORY(s_flags)) {
				/* mem -> reg */
				printf("R%3i <- M%3i",t_regoff,s_regoff);
			}
			else if (IS_INMEMORY(t_flags)) {
				/* reg -> mem */
				printf("M%3i <- R%3i",t_regoff,s_regoff);
			}
			else {
				/* reg -> reg */
				printf("R%3i <- R%3i",t_regoff,s_regoff);
			}
			printf("\n");
		}
#endif /* defined(SSA_DEBUG_VERBOSE) */
	}
}

void cg_move(codegendata *cd, s4 type, s4 src_regoff, s4 src_flags,
			 s4 dst_regoff, s4 dst_flags) {
	if ((IS_INMEMORY(dst_flags)) && (IS_INMEMORY(src_flags))) {
		/* mem -> mem */
		if (dst_regoff != src_regoff) {
			if (!IS_2_WORD_TYPE(type)) {
				if (IS_FLT_DBL_TYPE(type)) {
					emit_flds_membase(cd, REG_SP, src_regoff * 4);
					emit_fstps_membase(cd, REG_SP, dst_regoff * 4);
				} else{
					emit_mov_membase_reg(cd, REG_SP, src_regoff * 4,
							REG_ITMP1);
					emit_mov_reg_membase(cd, REG_ITMP1, REG_SP, dst_regoff * 4);
				}
			} else { /* LONG OR DOUBLE */
				if (IS_FLT_DBL_TYPE(type)) {
					emit_fldl_membase( cd, REG_SP, src_regoff * 4);
					emit_fstpl_membase(cd, REG_SP, dst_regoff * 4);
				} else {
					emit_mov_membase_reg(cd, REG_SP, src_regoff * 4,
							REG_ITMP1);
					emit_mov_reg_membase(cd, REG_ITMP1, REG_SP, dst_regoff * 4);
					emit_mov_membase_reg(cd, REG_SP, src_regoff * 4 + 4,
                            REG_ITMP1);             
					emit_mov_reg_membase(cd, REG_ITMP1, REG_SP, 
							dst_regoff * 4 + 4);
				}
			}
		}
	} else {
		if (IS_FLT_DBL_TYPE(type)) {
			log_text("cg_move: flt/dbl type have to be in memory\n");
/* 			assert(0); */
		}
		if (IS_2_WORD_TYPE(type)) {
			log_text("cg_move: longs have to be in memory\n");
/* 			assert(0); */
		}
		if (IS_INMEMORY(src_flags)) {
			/* mem -> reg */
			emit_mov_membase_reg(cd, REG_SP, src_regoff * 4, dst_regoff);
		} else if (IS_INMEMORY(dst_flags)) {
			/* reg -> mem */
			emit_mov_reg_membase(cd, src_regoff, REG_SP, dst_regoff * 4);
		} else {
			/* reg -> reg */
			/* only ints can be in regs on i386 */
			M_INTMOVE(src_regoff,dst_regoff);
		}
	}
}
#endif /* defined(ENABLE_SSA) */


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

	M_MOV_IMM(m, REG_ITMP1);
	M_MOV_IMM(asm_call_jit_compiler, REG_ITMP3);
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
	s4           i, j;                 /* count variables                    */
	s4           t;
	s4           s1, s2;

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
		1 +                             /* function pointer                   */
		4 +                             /* 4 arguments (start_native_call)    */
		nmd->memuse;

    /* keep stack 16-byte aligned */

	cd->stackframesize |= 0x3;

	/* create method header */

	(void) dseg_add_unique_address(cd, code);              /* CodeinfoPointer */
	(void) dseg_add_unique_s4(cd, cd->stackframesize * 4); /* FrameSize       */
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

		M_MOV_IMM(code, REG_ITMP1);
		M_IADD_IMM_MEMBASE(1, REG_ITMP1, OFFSET(codeinfo, frequency));
	}
#endif

	/* calculate stackframe size for native function */

	M_ASUB_IMM(cd->stackframesize * 4, REG_SP);

#if !defined(NDEBUG)
	emit_verbosecall_enter(jd);
#endif

	/* get function address (this must happen before the stackframeinfo) */

#if !defined(WITH_STATIC_CLASSPATH)
	if (f == NULL)
		codegen_addpatchref(cd, PATCHER_resolve_native, m, 0);
#endif

	M_AST_IMM((ptrint) f, REG_SP, 4 * 4);

	/* Mark the whole fpu stack as free for native functions (only for saved  */
	/* register count == 0).                                                  */

	emit_ffree_reg(cd, 0);
	emit_ffree_reg(cd, 1);
	emit_ffree_reg(cd, 2);
	emit_ffree_reg(cd, 3);
	emit_ffree_reg(cd, 4);
	emit_ffree_reg(cd, 5);
	emit_ffree_reg(cd, 6);
	emit_ffree_reg(cd, 7);

	/* prepare data structures for native function call */

	M_MOV(REG_SP, REG_ITMP1);
	M_AADD_IMM(cd->stackframesize * 4, REG_ITMP1);

	M_AST(REG_ITMP1, REG_SP, 0 * 4);
	M_IST_IMM(0, REG_SP, 1 * 4);
	dseg_adddata(cd);

	M_MOV(REG_SP, REG_ITMP2);
	M_AADD_IMM(cd->stackframesize * 4 + SIZEOF_VOID_P, REG_ITMP2);

	M_AST(REG_ITMP2, REG_SP, 2 * 4);
	M_ALD(REG_ITMP3, REG_SP, cd->stackframesize * 4);
	M_AST(REG_ITMP3, REG_SP, 3 * 4);
	M_MOV_IMM(codegen_start_native_call, REG_ITMP1);
	M_CALL(REG_ITMP1);

	M_ALD(REG_ITMP3, REG_SP, 4 * 4);

	/* copy arguments into new stackframe */

	for (i = md->paramcount - 1, j = i + nativeparams; i >= 0; i--, j--) {
		t = md->paramtypes[i].type;

		if (!md->params[i].inmemory) {
			/* no integer argument registers */
		} else {       /* float/double in memory can be copied like int/longs */
			s1 = (md->params[i].regoff + cd->stackframesize + 1) * 4;
			s2 = nmd->params[j].regoff * 4;

			M_ILD(REG_ITMP1, REG_SP, s1);
			M_IST(REG_ITMP1, REG_SP, s2);
			if (IS_2_WORD_TYPE(t)) {
				M_ILD(REG_ITMP1, REG_SP, s1 + 4);
				M_IST(REG_ITMP1, REG_SP, s2 + 4);
			}
		}
	}

	/* if function is static, put class into second argument */

	if (m->flags & ACC_STATIC)
		M_AST_IMM(m->class, REG_SP, 1 * 4);

	/* put env into first argument */

	M_AST_IMM(_Jv_env, REG_SP, 0 * 4);

	/* call the native function */

	M_CALL(REG_ITMP3);

	/* save return value */

	switch (md->returntype.type) {
	case TYPE_INT:
	case TYPE_ADR:
		M_IST(REG_RESULT, REG_SP, 1 * 4);
		break;
	case TYPE_LNG:
		M_LST(REG_RESULT_PACKED, REG_SP, 1 * 4);
		break;
	case TYPE_FLT:
		emit_fsts_membase(cd, REG_SP, 1 * 4);
		break;
	case TYPE_DBL:
		emit_fstl_membase(cd, REG_SP, 1 * 4);
		break;
	case TYPE_VOID:
		break;
	}

#if !defined(NDEBUG)
	emit_verbosecall_exit(jd);
#endif

	/* remove native stackframe info */

	M_MOV(REG_SP, REG_ITMP1);
	M_AADD_IMM(cd->stackframesize * 4, REG_ITMP1);

	M_AST(REG_ITMP1, REG_SP, 0 * 4);
	M_MOV_IMM(codegen_finish_native_call, REG_ITMP1);
	M_CALL(REG_ITMP1);
	M_MOV(REG_RESULT, REG_ITMP2);                 /* REG_ITMP3 == REG_RESULT2 */

	/* restore return value */

	switch (md->returntype.type) {
	case TYPE_INT:
	case TYPE_ADR:
		M_ILD(REG_RESULT, REG_SP, 1 * 4);
		break;
	case TYPE_LNG:
		M_LLD(REG_RESULT_PACKED, REG_SP, 1 * 4);
		break;
	case TYPE_FLT:
		emit_flds_membase(cd, REG_SP, 1 * 4);
		break;
	case TYPE_DBL:
		emit_fldl_membase(cd, REG_SP, 1 * 4);
		break;
	case TYPE_VOID:
		break;
	}

	M_AADD_IMM(cd->stackframesize * 4, REG_SP);

	/* check for exception */

	M_TEST(REG_ITMP2);
	M_BNE(1);

	M_RET;

	/* handle exception */

	M_MOV(REG_ITMP2, REG_ITMP1_XPTR);
	M_ALD(REG_ITMP2_XPC, REG_SP, 0);
	M_ASUB_IMM(2, REG_ITMP2_XPC);

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

/* src/vm/jit/mips/asmpart.S - Java-C interface functions for MIPS

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

   $Id: asmpart.S 7678M 2007-10-27 12:14:22Z (local) $

*/


#include "config.h"

#include "vm/jit/mips/md-abi.h"
#include "vm/jit/mips/md-asm.h"
#include "vm/jit/mips/offsets.h"

#include "vm/jit/abi-asm.h"
#include "vm/jit/methodheader.h"


	.text
	.set    noat


/* export functions ***********************************************************/

	.globl asm_vm_call_method
	.globl asm_vm_call_method_int
	.globl asm_vm_call_method_long
	.globl asm_vm_call_method_float
	.globl asm_vm_call_method_double
	.globl asm_vm_call_method_exception_handler
	.globl asm_vm_call_method_end

	.globl asm_call_jit_compiler

	.globl asm_handle_exception
	.globl asm_handle_nat_exception

	.globl asm_abstractmethoderror

	.globl asm_patcher_wrapper

#if defined(ENABLE_REPLACEMENT)
	.globl asm_replacement_out
	.globl asm_replacement_in
#endif

	.globl asm_getclassvalues_atomic
	.globl asm_criticalsections

	.globl compare_and_swap


/* asm_vm_call_method **********************************************************
*                                                                              *
*   This function calls a Java-method (which possibly needs compilation)       *
*   with up to 4 address parameters.                                           *
*                                                                              *
*   This functions calls the JIT-compiler which eventually translates the      *
*   method into machine code.                                                  *
*                                                                              *
*   A possibly throwed exception will be returned to the caller as function    *
*   return value, so the java method cannot return a fucntion value (this      *
*   function usually calls 'main' and '<clinit>' which do not return a         *
*   function value).                                                           *
*                                                                              *
*   C-prototype:                                                               *
*    javaobject_header *asm_calljavafunction (methodinfo *m,                   *
*         void *arg1, void *arg2, void *arg3, void *arg4);                     *
*                                                                              *
*******************************************************************************/

	.ent    asm_vm_call_method

	.align  3

#if SIZEOF_VOID_P == 8

	.dword  0                           /* catch type all                     */
	.dword  0                           /* handler pc                         */
	.dword  0                           /* end pc                             */
	.dword  0                           /* start pc                           */
	.word   1                           /* extable size                       */
	.word   0                           /* 4-byte ALIGNMENT PADDING           */
	.dword  0                           /* line number table start            */
	.dword  0                           /* line number table size             */
	.word   0                           /* 4-byte ALIGNMENT PADDING           */
	.word   0                           /* fltsave                            */
	.word   0                           /* intsave                            */
	.word   0                           /* isleaf                             */
	.word   0                           /* IsSync                             */
	.word   0                           /* frame size                         */
	.dword  0                           /* codeinfo pointer                   */

#else /* SIZEOF_VOID_P == 8 */

	.word   0                           /* catch type all                     */
	.word   0                           /* handler pc                         */
	.word   0                           /* end pc                             */
	.word   0                           /* start pc                           */
	.word   1                           /* extable size                       */
	.word   0                           /* line number table start            */
	.word   0                           /* line number table size             */
	.word   0                           /* fltsave                            */
	.word   0                           /* intsave                            */
	.word   0                           /* isleaf                             */
	.word   0                           /* IsSync                             */
	.word   0                           /* frame size                         */
	.word   0                           /* method pointer (pointer to name)   */

#endif /* SIZEOF_VOID_P == 8 */

asm_vm_call_method:
asm_vm_call_method_int:
asm_vm_call_method_long:
asm_vm_call_method_float:
asm_vm_call_method_double:
	.set    noreorder                 /* XXX we need to recompute pv          */

	aaddiu  sp,sp,-12*8               /* allocate stack space (only 11 needed)*/
	ast     ra,0*8(sp)                /* save return address                  */

	bal     L_asm_vm_call_method_compute_pv
	ast     pv,1*8(sp)                /* procedure vector                     */
L_asm_vm_call_method_compute_pv:
	aaddiu  pv,ra,-4*4

	ast     s7,3*8(sp)

#if SIZEOF_VOID_P == 8
	sdc1    fss0,5*8(sp)              /* save non JavaABI saved flt registers */
	sdc1    fss1,6*8(sp)
	sdc1    fss2,7*8(sp)
	sdc1    fss3,8*8(sp)
	sdc1    fss4,9*8(sp)
	sdc1    fss5,10*8(sp)
#endif

	ast     a0,4*8(sp)                /* save method pointer for compiler     */

	move    t0,a2                     /* address of first block               */
	move    s7,a1                     /* argument count                       */
	blez    s7,calljava_argsloaded
	nop

#if SIZEOF_VOID_P == 8

	ald     a0,offvmargdata(t0)
	ldc1    fa0,offvmargdata(t0)
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded
	nop

	ald     a1,offvmargdata+sizevmarg*1(t0)
	ldc1    fa1,offvmargdata+sizevmarg*1(t0)
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded
	nop

	ald     a2,offvmargdata+sizevmarg*2(t0)
	ldc1    fa2,offvmargdata+sizevmarg*2(t0)
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded
	nop

	ald     a3,offvmargdata+sizevmarg*3(t0)
	ldc1    fa3,offvmargdata+sizevmarg*3(t0)
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded
	nop

	ald     a4,offvmargdata+sizevmarg*4(t0)
	ldc1    fa4,offvmargdata+sizevmarg*4(t0)
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded
	nop

	ald     a5,offvmargdata+sizevmarg*5(t0)
	ldc1    fa5,offvmargdata+sizevmarg*5(t0)
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded
	nop

	ald     a6,offvmargdata+sizevmarg*6(t0)
	ldc1    fa6,offvmargdata+sizevmarg*6(t0)
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded
	nop

	ald     a7,offvmargdata+sizevmarg*7(t0)
	ldc1    fa7,offvmargdata+sizevmarg*7(t0)
	aaddi   s7,s7,-1

#else /* SIZEOF_VOID_P == 8 */

#if WORDS_BIGENDIAN == 1
	ald     a0,offvmargdata+4(t0)
#else
	ald     a0,offvmargdata(t0)
#endif
#if !defined(ENABLE_SOFTFLOAT)
	ldc1    fa0,offvmargdata(t0)
#endif
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded

#if WORDS_BIGENDIAN == 1
	ald     a1,offvmargdata+4+sizevmarg*1(t0)
#else
	ald     a1,offvmargdata+sizevmarg*1(t0)
#endif
#if !defined(ENABLE_SOFTFLOAT)
	ldc1    fa1,offvmargdata+sizevmarg*1(t0)
#endif
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded

#if WORDS_BIGENDIAN == 1
	ald     a2,offvmargdata+4+sizevmarg*2(t0)
#else
	ald     a2,offvmargdata+sizevmarg*2(t0)
#endif
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded

#if WORDS_BIGENDIAN == 1
	ald     a3,offvmargdata+4+sizevmarg*3(t0)
#else
	ald     a3,offvmargdata+sizevmarg*3(t0)
#endif
	aaddi   s7,s7,-1
	blez    s7,calljava_argsloaded

#endif /* SIZEOF_VOID_P == 8 */

calljava_argsloaded:
	move    t4,sp                     /* save stack pointer                   */
	blez    s7,calljava_nocopy
	nop

#if SIZEOF_VOID_P == 4
	aaddiu  s7,s7,4                   /* add stack space for 4 arguments      */
#endif
	subu    t1,zero,s7                /* remaining argument count (negative)  */
	sll     t2,t1,3                   /* calculate stackframe size            */
	aaddu   sp,sp,t2                  /* create stackframe                    */
	aaddu   t2,t2,t4                  /* also set temp sp                     */
#if SIZEOF_VOID_P == 4
	aaddiu  t2,t2,4*8                 /* skip stack space for 4 arguments     */
	addiu   t1,t1,4
#endif

calljava_copyloop:
#if SIZEOF_VOID_P == 8
	ald     t3,offvmargdata+sizevmarg*8(t0)
#else
# if WORDS_BIGENDIAN == 1
	ald     t3,offvmargdata+4+sizevmarg*4(t0)
# else
	ald     t3,offvmargdata+sizevmarg*4(t0)
# endif
#endif
	ast     t3,0(t2)                  /* store argument on stack              */
	addi    t1,t1,1                   /* count 1 argument                     */
	aaddi   t0,t0,sizevmarg           /* load address of next block           */
	aaddi   t2,t2,8                   /* increase stack position              */
	bnez    t1,calljava_copyloop      /* all arguments copied?                */
	nop

calljava_nocopy:
	ald     itmp1,4*8(t4)             /* pass method pointer via itmp1        */

	ala     mptr,asm_call_jit_compiler/* fake virtual function call (2 instr) */
	ast     mptr,2*8(t4)              /* store function address               */
	ala     mptr,1*8(t4)              /* set method pointer                   */

	ald     pv,1*8(mptr)              /* method call as in Java               */
	jalr    pv                        /* call JIT compiler                    */
	nop
L_asm_vm_call_method_recompute_pv:
#if SIZEOF_VOID_P == 8
	aaddiu  pv,ra,-76*4               /* recompute procedure vector           */
#else
	aaddiu  pv,ra,(asm_vm_call_method - L_asm_vm_call_method_recompute_pv)
#endif

	.set    reorder                   /* XXX we need to recompute pv          */

	sll     t1,s7,3                   /* remove argument stackframe           */
	aaddu   sp,sp,t1

calljava_return2:
	ald     ra,0*8(sp)                /* restore return address               */
	ald     pv,1*8(sp)                /* restore procedure vector             */
	ald     s7,3*8(sp)

#if SIZEOF_VOID_P == 8
	ldc1    fss0,5*8(sp)              /* restore non JavaABI saved flt regs   */
	ldc1    fss1,6*8(sp)
	ldc1    fss2,7*8(sp)
	ldc1    fss3,8*8(sp)
	ldc1    fss4,9*8(sp)
	ldc1    fss5,10*8(sp)
#endif

	aaddiu  sp,sp,12*8                /* free stack space                     */
	j       ra                        /* return                               */

asm_vm_call_method_exception_handler:
	sll     t1,s7,3                   /* remove stackframe                    */
	aaddu   sp,sp,t1
#if SIZEOF_VOID_P == 4
	aaddiu  sp,sp,-4*4                /* reserve space for 1 argument         */
#endif

	move    a0,itmp1                  
	jal     builtin_throw_exception
#if SIZEOF_VOID_P == 4
	aaddiu  sp,sp,4*4
#endif
asm_vm_call_method_end:
	b       calljava_return2

	.end    asm_vm_call_method


/****************** function asm_call_jit_compiler *****************************
*                                                                              *
*   invokes the compiler for untranslated JavaVM methods.                      *
*                                                                              *
*   Register REG_ITEMP1 contains a pointer to the method info structure        *
*   (prepared by createcompilerstub). Using the return address in R31 and the  *
*   offset in the LDA instruction or using the value in methodptr R25 the      *
*   patching address for storing the method address can be computed:           *
*                                                                              *
*   method address was either loaded using                                     *
*   M_ALD (REG_PV, REG_PV, a)        ; invokestatic/special    ($28)           *
*   M_JSR (REG_RA, REG_PV);                                                    *
*   M_NOP                                                                      *
*   M_LDA (REG_PV, REG_RA, val)                                                *
*   or                                                                         *
*   M_ALD (REG_PV, REG_METHODPTR, m) ; invokevirtual/interface ($25)           *
*   M_JSR (REG_RA, REG_PV);                                                    *
*   M_NOP                                                                      *
*   in the static case the method pointer can be computed using the            *
*   return address and the lda function following the jmp instruction          *
*                                                                              *
*******************************************************************************/

	.ent    asm_call_jit_compiler

asm_call_jit_compiler:
#if SIZEOF_VOID_P == 8

	aaddiu  sp,sp,-(ARG_CNT+2)*8  /* +2: keep stack 16-bytes aligned          */

	ast     ra,0*8(sp)            /* save return address                      */

	SAVE_ARGUMENT_REGISTERS(1)

	move    a0,itmp1              /* pass methodinfo pointer                  */
	move    a1,mptr               /* pass method pointer                      */
	aaddiu  a2,sp,(ARG_CNT+2)*8   /* pass java sp                             */
	move    a3,ra
	jal     jit_asm_compile       /* call jit compiler                        */
	move    pv,v0

	ald     ra,0*8(sp)            /* restore return address                   */

	RESTORE_ARGUMENT_REGISTERS(1)

	aaddiu  sp,sp,(ARG_CNT+2)*8   /* remove stack frame                       */

#else /* SIZEOF_VOID_P == 8 */

	aaddiu  sp,sp,-(ARG_CNT+2)*8  /* +4: keep stack 16-bytes aligned          */

	ast     ra,4*4+0*4(sp)        /* save return address                      */

	SAVE_ARGUMENT_REGISTERS(6)

	move    a0,itmp1              /* pass methodinfo pointer                  */
	move    a1,mptr               /* pass method pointer                      */
	aaddiu  a2,sp,(ARG_CNT+2)*8   /* pass java sp                             */
	move    a3,ra
	jal     jit_asm_compile       /* call jit compiler                        */
	move    pv,v0

	ald     ra,4*4+0*4(sp)        /* restore return address                   */

	RESTORE_ARGUMENT_REGISTERS(6)

	aaddiu  sp,sp,(ARG_CNT+2)*8   /* remove stack frame                       */

#endif /* SIZEOF_VOID_P == 8 */

	beqz    pv,L_asm_call_jit_compiler_exception

	jr      pv                    /* and call method. The method returns      */
	                              /* directly to the caller (ra).             */

L_asm_call_jit_compiler_exception:
	aaddiu  sp,sp,-2*8
	ast     ra,0*8(sp)
	jal     exceptions_get_and_clear_exception
	ald     ra,0*8(sp)
	aaddiu  sp,sp,2*8

	move    xptr,v0               /* get exception                            */
	aaddiu  xpc,ra,-4             /* exception address is RA - 4              */
	b       asm_handle_nat_exception

	.end    asm_call_jit_compiler


/* asm_handle_exception ********************************************************

   This function handles an exception. It does not use the usual calling
   conventions. The exception pointer is passed in REG_ITMP1 and the
   pc from the exception raising position is passed in REG_ITMP2. It searches
   the local exception table for a handler. If no one is found, it unwinds
   stacks and continues searching the callers.

*******************************************************************************/

	.ent    asm_handle_nat_exception

asm_handle_nat_exception:
L_asm_handle_exception_stack_loop:
#if SIZEOF_VOID_P == 8
	aaddiu  sp,sp,-6*8                  /* keep stack 16-byte aligned         */
	ast     xptr,0*8(sp)                /* save exception pointer             */
	ast     xpc,1*8(sp)                 /* save exception pc                  */
	ast     ra,3*8(sp)                  /* save RA                            */
	ast     zero,4*8(sp)                /* save maybe-leaf flag (cleared)     */
#else
	aaddiu  sp,sp,-(4*4+6*8)            /* allocate stack                     */
	ast     xptr,4*4+0*8(sp)            /* save exception pointer             */
	ast     xpc,4*4+1*8(sp)             /* save exception pc                  */
	ast     ra,4*4+3*8(sp)              /* save return address                */
	ast     zero,4*4+4*8(sp)            /* save maybe-leaf flag (cleared)     */
#endif

	move    a0,ra                       /* pass RA                            */
	jal     md_codegen_get_pv_from_pc   /* get PV from RA                     */

#if SIZEOF_VOID_P == 8
	ast     v0,2*8(sp)                  /* save PV                            */

	ald     a0,0*8(sp)                  /* pass xptr                          */
	ald     a1,1*8(sp)                  /* pass xpc                           */
	move    a2,v0                       /* pass PV                            */
	aaddiu  a3,sp,6*8                   /* pass Java SP                       */
#else
	ast     v0,4*4+2*8(sp)              /* save data segment pointer          */

	ald     a0,4*4+0*8(sp)              /* pass exception pointer             */
	ald     a1,4*4+1*8(sp)              /* pass exception pc                  */
	move    a2,v0                       /* pass data segment pointer          */
	aaddiu  a3,sp,(4*4+6*8)             /* pass Java stack pointer            */
#endif

	b       L_asm_handle_exception_continue

	.aent    asm_handle_exception

asm_handle_exception:
	aaddiu  sp,sp,-(ARG_CNT+TMP_CNT)*8  /* create maybe-leaf stackframe       */

	SAVE_ARGUMENT_REGISTERS(0)          /* we save arg and temp registers in  */
	SAVE_TEMPORARY_REGISTERS(ARG_CNT)   /* case this is a leaf method         */

#if SIZEOF_VOID_P == 8
	aaddiu  sp,sp,-6*8                  /* allocate stack                     */
	ast     xptr,0*8(sp)                /* save exception pointer             */
	ast     pv,2*8(sp)                  /* save PV                            */
	ast     ra,3*8(sp)                  /* save RA                            */
	addu    t0,zero,1                   /* set maybe-leaf flag                */
	ast     t0,4*8(sp)                  /* save maybe-leaf flag               */
#else
	aaddiu  sp,sp,-(4*4+6*8)            /* allocate stack                     */
	ast     xptr,4*4+0*8(sp)            /* save exception pointer             */
	ast     xpc,4*4+1*8(sp)             /* save exception pc                  */
	ast     pv,4*4+2*8(sp)              /* save data segment pointer          */
	ast     ra,4*4+3*8(sp)              /* save return address                */
	addu    t0,zero,1                   /* set maybe-leaf flag                */
	ast     t0,4*4+4*8(sp)              /* save maybe-leaf flag               */
#endif

	move    a0,xptr                     /* pass xptr                          */
	move    a1,xpc                      /* pass xpc                           */
	move    a2,pv                       /* pass PV                            */

#if SIZEOF_VOID_P == 8
	aaddiu  a3,sp,(ARG_CNT+TMP_CNT+6)*8 /* pass Java SP                       */
#else
	aaddiu  a3,sp,4*4+(ARG_CNT+TMP_CNT+6)*8 /* pass Java stack pointer        */
#endif

L_asm_handle_exception_continue:
	jal     exceptions_handle_exception
	
	beqz    v0,L_asm_handle_exception_not_catched

	move    xpc,v0                      /* move handlerpc into xpc            */

#if SIZEOF_VOID_P == 8
	ald     xptr,0*8(sp)                /* restore exception pointer          */
	ald     pv,2*8(sp)                  /* restore PV                         */
	ald     ra,3*8(sp)                  /* restore RA                         */
	ald     t0,4*8(sp)                  /* get maybe-leaf flag                */
	aaddiu  sp,sp,6*8                   /* free stackframe                    */
#else
	ald     xptr,4*4+0*8(sp)            /* restore exception pointer          */
	ald     pv,4*4+2*8(sp)              /* restore data segment pointer       */
	ald     ra,4*4+3*8(sp)              /* restore return address             */
	ald     t0,4*4+4*8(sp)              /* get maybe-leaf flag                */
	aaddiu  sp,sp,4*4+6*8               /* free stackframe                    */
#endif
	
	beqz    t0,L_asm_handle_exception_no_leaf

	RESTORE_ARGUMENT_REGISTERS(0)       /* if this is a leaf method, we have  */
	RESTORE_TEMPORARY_REGISTERS(ARG_CNT)/* to restore arg and temp registers  */
	
	aaddiu  sp,sp,(ARG_CNT+TMP_CNT)*8   /* remove maybe-leaf stackframe       */

L_asm_handle_exception_no_leaf:
	jr      xpc                         /* jump to the handler                */

L_asm_handle_exception_not_catched:
#if SIZEOF_VOID_P == 8
	ald     xptr,0*8(sp)                /* restore xptr                       */
	ald     pv,2*8(sp)                  /* restore PV                         */
	ald     ra,3*8(sp)                  /* restore RA                         */
	ald     t0,4*8(sp)                  /* get maybe-leaf flag                */
	aaddiu  sp,sp,6*8                   /* free stackframe                    */
#else
	ald     xptr,4*4+0*8(sp)            /* restore xptr                       */
	ald     pv,4*4+2*8(sp)              /* restore PV                         */
	ald     ra,4*4+3*8(sp)              /* restore RA                         */
	ald     t0,4*4+4*8(sp)              /* get maybe-leaf flag                */
	aaddiu  sp,sp,4*4+6*8               /* free stackframe                    */
#endif
	
	beqz    t0,L_asm_handle_exception_no_leaf_stack

	aaddiu  sp,sp,(ARG_CNT+TMP_CNT)*8   /* remove maybe-leaf stackframe       */
	move    t0,zero                     /* clear the maybe-leaf flag          */

L_asm_handle_exception_no_leaf_stack:
	lw      t1,FrameSize(pv)            /* get frame size                     */
	aaddu   t1,sp,t1                    /* pointer to save area               */

	lw      t2,IsLeaf(pv)               /* is leaf procedure                  */
	bnez    t2,L_asm_handle_exception_no_ra_restore

	ald     ra,-1*8(t1)                 /* restore ra                         */
	aaddiu  t1,t1,-8                    /* t1--                               */

L_asm_handle_exception_no_ra_restore:
	move    xpc,ra                      /* the new xpc is ra                  */
	lw      t2,IntSave(pv)              /* t1 = saved int register count      */
	ala     t3,ex_int2                  /* t3 = current pc                    */
	sll     t2,t2,2                     /* t2 = register count * 4            */
	asubu   t3,t3,t2                    /* t3 = IntSave - 4 * register count  */
	jr      t3                          /* jump to save position              */

	ald     s0,-8*8(t1)
	ald     s1,-7*8(t1)
	ald     s2,-6*8(t1)
	ald     s3,-5*8(t1)
	ald     s4,-4*8(t1)
	ald     s5,-3*8(t1)
	ald     s6,-2*8(t1)
	ald     s7,-1*8(t1)

ex_int2:
	sll     t2,t2,1               /* t2 = register count * 4 * 2              */
	asubu   t1,t1,t2              /* t1 = t0 - 8 * register count             */

	lw      t2,FltSave(pv)        /* t2 = saved flt register count            */
	ala     t3,ex_flt2            /* t3 = current pc                          */
	sll     t2,t2,2               /* t2 = register count * 4                  */
	asubu   t3,t3,t2              /* t3 = ex_int_sav - 4 * register count     */
	jr      t3                          /* jump to save position              */

#if SIZEOF_VOID_P == 8
	ldc1    fs0,-4*8(t1)
	ldc1    fs1,-3*8(t1)
	ldc1    fs2,-2*8(t1)
	ldc1    fs3,-1*8(t1)
#else /* SIZEOF_VOID_P == 8 */
# if !defined(ENABLE_SOFTFLOAT)
	ldc1    fs0,-4*8(t1)
	ldc1    fs1,-3*8(t1)
	ldc1    fs2,-2*8(t1)
	ldc1    fs3,-1*8(t1)
	ldc1    fs4,-1*8(t1)
	ldc1    fs5,-1*8(t1)
# endif /* !defined(ENABLE_SOFTFLOAT) */
#endif /* SIZEOF_VOID_P == 8 */

ex_flt2:
	lw      t1,FrameSize(pv)            /* get frame size                     */
	aaddu   sp,sp,t1                    /* unwind stack                       */
	b       L_asm_handle_exception_stack_loop

	.end    asm_handle_nat_exception


/* asm_abstractmethoderror *****************************************************

   Creates and throws an AbstractMethodError.

*******************************************************************************/

	.ent    asm_abstractmethoderror

asm_abstractmethoderror:
	aaddiu  sp,sp,-2*8                  /* create stackframe                  */
	ast     ra,0*8(sp)                  /* save return address                */
	aaddiu  a0,sp,2*8                   /* pass java sp                       */
	move    a1,ra                       /* pass exception address             */
	jal     exceptions_asm_new_abstractmethoderror
	ald     ra,0*8(sp)                  /* restore return address             */
	aaddiu  sp,sp,2*8                   /* remove stackframe                  */

	move    xptr,v0                     /* get exception pointer              */
	aaddiu  xpc,ra,-4                   /* exception address is ra - 4        */
	b       asm_handle_nat_exception

	.end    asm_abstractmethoderror


/* asm_patcher_wrapper *********************************************************

   XXX

   Stack layout:
     56   return address into JIT code (patch position)
     48   pointer to virtual java_objectheader
     40   machine code (which is patched back later)
     32   machine code (which is patched back later)
     24   machine code (which is patched back later)
     16   unresolved class/method/field reference
      8   data segment displacement from load instructions
      0   patcher function pointer to call

*******************************************************************************/
		
    .ent    asm_patcher_wrapper

asm_patcher_wrapper:
#if SIZEOF_VOID_P == 8

	aaddiu  sp,sp,-((2+16+22+4)*8)/* create stack frame                       */

	SAVE_RETURN_REGISTERS(0)      /* save 1 int/1 float return registers      */
	SAVE_ARGUMENT_REGISTERS(2)    /* save 8 int/8 float argument registers    */
	SAVE_TEMPORARY_REGISTERS(18)  /* save 5 int/16 float temporary registers  */

	ast     itmp1,(2+16+22+0)*8(sp) /* save itmp1                             */
	ast     itmp2,(2+16+22+1)*8(sp) /* save itmp2                             */
	ast     ra,(2+16+22+2)*8(sp)  /* save method return address (for leafs)   */
	ast     pv,(2+16+22+3)*8(sp)  /* save pv of calling java function         */

	aaddiu  a0,sp,(2+16+22+4)*8   /* pass SP of patcher stub                  */
	move    a1,pv                 /* pass PV                                  */
	move    a2,ra                 /* pass RA (correct for leafs)              */
	jal     patcher_wrapper
	move    itmp3,v0

	RESTORE_RETURN_REGISTERS(0)   /* restore 1 int/1 float return registers   */
	RESTORE_ARGUMENT_REGISTERS(2) /* restore 8 int/8 float argument registers */
	RESTORE_TEMPORARY_REGISTERS(18) /* restore 5 int/16 float temporary reg.  */

	ald     itmp1,(2+16+22+0)*8(sp) /* restore itmp1                          */
	ald     itmp2,(2+16+22+1)*8(sp) /* restore itmp2                          */
	ald     ra,(2+16+22+2)*8(sp)  /* restore method return address (for leafs)*/
	ald     pv,(2+16+22+3)*8(sp)  /* restore pv of calling java function      */

	bnez    itmp3,L_asm_patcher_wrapper_exception

	ald     itmp3,(7+2+16+22+4)*8(sp) /* load RA                              */
	aaddiu  sp,sp,(8+2+16+22+4)*8 /* remove stack frame                       */

	jr      itmp3                 /* jump to new patched code                 */

L_asm_patcher_wrapper_exception:
	move    xptr,itmp3            /* get exception                            */
	ald     xpc,(7+2+16+22+4)*8(sp) /* xpc is RA                              */
	aaddiu  sp,sp,(8+2+16+22+4)*8 /* remove stack frame                       */

#else /* SIZEOF_VOID_P == 8 */

	aaddiu  sp,sp,-((6+4+8+16+7)*4) /* create stack frame                     */
	                              /* +7 keeps the SP 16-bytes aligned         */

	SAVE_RETURN_REGISTERS(6)      /* save 2 int / 1 float return registers    */
	SAVE_ARGUMENT_REGISTERS(10)   /* save 4 int / 2 float argument registers  */
	SAVE_TEMPORARY_REGISTERS(18)  /* save 8 int / 4 float temporary registers */

	ast     itmp1,(6+4+8+16+0)*4(sp) /* save itmp1                            */
	ast     itmp2,(6+4+8+16+1)*4(sp) /* save itmp2                            */
	ast     ra,(6+4+8+16+2)*4(sp) /* save method return address (for leafs)   */
	ast     pv,(6+4+8+16+3)*4(sp) /* save pv of calling java function         */

	aaddiu  a0,sp,(6+4+8+16+7)*4  /* pass SP of patcher stub                  */
	move    a1,pv                 /* pass PV                                  */
	move    a2,ra                 /* pass RA (correct for leafs)              */
	jal     patcher_wrapper
	move    itmp3,v0

	RESTORE_RETURN_REGISTERS(6)   /* restore 2 int / 2 float return registers */
	RESTORE_ARGUMENT_REGISTERS(10) /* restore 4 int / 2 float argument regs   */
	RESTORE_TEMPORARY_REGISTERS(18) /* restore 8 int / 4 float temporary regs */

	ald     itmp1,(6+4+8+16+0)*4(sp) /* restore itmp1                         */
	ald     itmp2,(6+4+8+16+1)*4(sp) /* restore itmp2                         */
	ald     ra,(6+4+8+16+2)*4(sp) /* restore method return address (for leafs)*/
	ald     pv,(6+4+8+16+3)*4(sp) /* restore pv of calling java function      */

	bnez    itmp3,L_asm_wrapper_patcher_exception

	ald     itmp3,7*8+(6+4+8+16+7)*4(sp) /* load RA                           */
	aaddiu  sp,sp,8*8+(6+4+8+16+7)*4 /* remove stack frame                    */

	jr      itmp3                 /* jump to new patched code                 */

L_asm_wrapper_patcher_exception:
	move    xptr,itmp3            /* get exception                            */
	ald     xpc,7*8+(6+4+8+16+7)*4(sp) /* xpc is RA                           */
	aaddiu  sp,sp,8*8+(6+4+8+16+7)*4 /* remove stack frame                    */

#endif /* SIZEOF_VOID_P == 8 */

	b       asm_handle_exception

	.end	asm_patcher_wrapper

#if defined(ENABLE_REPLACEMENT)
		
/* asm_replacement_out *********************************************************

   This code is jumped to from the replacement-out stubs that are executed
   when a thread reaches an activated replacement point.

   The purpose of asm_replacement_out is to read out the parts of the
   execution state that cannot be accessed from C code, store this state,
   and then call the C function replace_me.

   Stack layout:
     16                 start of stack inside method to replace
      0   rplpoint *    info on the replacement point that was reached

   NOTE: itmp3 has been clobbered by the replacement-out stub!

*******************************************************************************/

/* some room to accomodate changes of the stack frame size during replacement */
	/* XXX we should find a cleaner solution here */
#define REPLACEMENT_ROOM  512

#define REPLACEMENT_STACK_OFFSET ((sizeexecutionstate + REPLACEMENT_ROOM + 0xf) & ~0xf)

	.ent asm_replacement_out

asm_replacement_out:
    /* create stack frame */
	aaddiu  sp,sp,-REPLACEMENT_STACK_OFFSET

	/* save registers in execution state */
	ast     $0 ,( 0*8+offes_intregs)(sp)
	ast     $1 ,( 1*8+offes_intregs)(sp)
	ast     $2 ,( 2*8+offes_intregs)(sp)
	ast     $3 ,( 3*8+offes_intregs)(sp)
	ast     $4 ,( 4*8+offes_intregs)(sp)
	ast     $5 ,( 5*8+offes_intregs)(sp)
	ast     $6 ,( 6*8+offes_intregs)(sp)
	ast     $7 ,( 7*8+offes_intregs)(sp)
	ast     $8 ,( 8*8+offes_intregs)(sp)
	ast     $9 ,( 9*8+offes_intregs)(sp)
	ast     $10,(10*8+offes_intregs)(sp)
	ast     $11,(11*8+offes_intregs)(sp)
	ast     $12,(12*8+offes_intregs)(sp)
	ast     $13,(13*8+offes_intregs)(sp)
	ast     $14,(14*8+offes_intregs)(sp)
	ast     $15,(15*8+offes_intregs)(sp)
	ast     $16,(16*8+offes_intregs)(sp)
	ast     $17,(17*8+offes_intregs)(sp)
	ast     $18,(18*8+offes_intregs)(sp)
	ast     $19,(19*8+offes_intregs)(sp)
	ast     $20,(20*8+offes_intregs)(sp)
	ast     $21,(21*8+offes_intregs)(sp)
	ast     $22,(22*8+offes_intregs)(sp)
	ast     $23,(23*8+offes_intregs)(sp)
	ast     $24,(24*8+offes_intregs)(sp)
	ast     $25,(25*8+offes_intregs)(sp)
	ast     $26,(26*8+offes_intregs)(sp)
	ast     $27,(27*8+offes_intregs)(sp)
	ast     $28,(28*8+offes_intregs)(sp)
	ast     $29,(29*8+offes_intregs)(sp)
	ast     $30,(30*8+offes_intregs)(sp)
	ast     $31,(31*8+offes_intregs)(sp)

#if SIZEOF_VOID_P == 8

	sdc1    $f0 ,( 0*8+offes_fltregs)(sp)
	sdc1    $f1 ,( 1*8+offes_fltregs)(sp)
	sdc1    $f2 ,( 2*8+offes_fltregs)(sp)
	sdc1    $f3 ,( 3*8+offes_fltregs)(sp)
	sdc1    $f4 ,( 4*8+offes_fltregs)(sp)
	sdc1    $f5 ,( 5*8+offes_fltregs)(sp)
	sdc1    $f6 ,( 6*8+offes_fltregs)(sp)
	sdc1    $f7 ,( 7*8+offes_fltregs)(sp)
	sdc1    $f8 ,( 8*8+offes_fltregs)(sp)
	sdc1    $f9 ,( 9*8+offes_fltregs)(sp)
	sdc1    $f10,(10*8+offes_fltregs)(sp)
	sdc1    $f11,(11*8+offes_fltregs)(sp)
	sdc1    $f12,(12*8+offes_fltregs)(sp)
	sdc1    $f13,(13*8+offes_fltregs)(sp)
	sdc1    $f14,(14*8+offes_fltregs)(sp)
	sdc1    $f15,(15*8+offes_fltregs)(sp)
	sdc1    $f16,(16*8+offes_fltregs)(sp)
	sdc1    $f17,(17*8+offes_fltregs)(sp)
	sdc1    $f18,(18*8+offes_fltregs)(sp)
	sdc1    $f19,(19*8+offes_fltregs)(sp)
	sdc1    $f20,(20*8+offes_fltregs)(sp)
	sdc1    $f21,(21*8+offes_fltregs)(sp)
	sdc1    $f22,(22*8+offes_fltregs)(sp)
	sdc1    $f23,(23*8+offes_fltregs)(sp)
	sdc1    $f24,(24*8+offes_fltregs)(sp)
	sdc1    $f25,(25*8+offes_fltregs)(sp)
	sdc1    $f26,(26*8+offes_fltregs)(sp)
	sdc1    $f27,(27*8+offes_fltregs)(sp)
	sdc1    $f28,(28*8+offes_fltregs)(sp)
	sdc1    $f29,(29*8+offes_fltregs)(sp)
	sdc1    $f30,(30*8+offes_fltregs)(sp)
	sdc1    $f31,(31*8+offes_fltregs)(sp)

#else /* SIZEOF_VOID_P == 8 */

	sdc1    $f0 ,( 0*8+offes_fltregs)(sp)
	sdc1    $f2 ,( 2*8+offes_fltregs)(sp)
	sdc1    $f4 ,( 4*8+offes_fltregs)(sp)
	sdc1    $f6 ,( 6*8+offes_fltregs)(sp)
	sdc1    $f8 ,( 8*8+offes_fltregs)(sp)
	sdc1    $f10,(10*8+offes_fltregs)(sp)
	sdc1    $f12,(12*8+offes_fltregs)(sp)
	sdc1    $f14,(14*8+offes_fltregs)(sp)
	sdc1    $f16,(16*8+offes_fltregs)(sp)
	sdc1    $f18,(18*8+offes_fltregs)(sp)
	sdc1    $f20,(20*8+offes_fltregs)(sp)
	sdc1    $f22,(22*8+offes_fltregs)(sp)
	sdc1    $f24,(24*8+offes_fltregs)(sp)
	sdc1    $f26,(26*8+offes_fltregs)(sp)
	sdc1    $f28,(28*8+offes_fltregs)(sp)
	sdc1    $f30,(30*8+offes_fltregs)(sp)

#endif /* SIZEOF_VOID_P == 8 */
	
	/* calculate sp of method */
	aaddiu  itmp1,sp,(REPLACEMENT_STACK_OFFSET + 2*8)
	ast     itmp1,(offes_sp)(sp)

	/* store pv */
	ast     pv,(offes_pv)(sp)

	/* call replace_me */
	ald     a0,-(2*8)(itmp1)            /* arg0: rplpoint *                   */
    move    a1,sp                       /* arg1: execution state              */
    jal     replace_me                  /* call C function replace_me         */
	jal     abort                       /* NEVER REACHED                      */

	.end asm_replacement_out

/* asm_replacement_in **********************************************************

   This code writes the given execution state and jumps to the replacement
   code.

   This function never returns!

   NOTE: itmp3 is not restored!

   C prototype:
      void asm_replacement_in(executionstate *es);

*******************************************************************************/

	.ent asm_replacement_in
	
asm_replacement_in:
	/* a0 == executionstate *es */

	/* set new sp and pv */
	ald     sp,(offes_sp)(a0)
	ald     pv,(offes_pv)(a0)
	
	/* copy registers from execution state */
	/* $0 is zero                     */
	ald     $1 ,( 1*8+offes_intregs)(a0)
	ald     $2 ,( 2*8+offes_intregs)(a0)
	ald     $3 ,( 2*8+offes_intregs)(a0)
	/* a0 is loaded below             */
	ald     $5 ,( 5*8+offes_intregs)(a0)
	ald     $6 ,( 6*8+offes_intregs)(a0)
	ald     $7 ,( 7*8+offes_intregs)(a0)
	ald     $8 ,( 8*8+offes_intregs)(a0)
	ald     $9 ,( 9*8+offes_intregs)(a0)
	ald     $10,(10*8+offes_intregs)(a0)
	ald     $11,(11*8+offes_intregs)(a0)
	ald     $12,(12*8+offes_intregs)(a0)
	ald     $13,(13*8+offes_intregs)(a0)
	ald     $14,(14*8+offes_intregs)(a0)
	ald     $15,(15*8+offes_intregs)(a0)
	ald     $16,(16*8+offes_intregs)(a0)
	ald     $17,(17*8+offes_intregs)(a0)
	ald     $18,(18*8+offes_intregs)(a0)
	ald     $19,(19*8+offes_intregs)(a0)
	ald     $20,(20*8+offes_intregs)(a0)
	ald     $21,(21*8+offes_intregs)(a0)
	ald     $22,(22*8+offes_intregs)(a0)
	ald     $23,(23*8+offes_intregs)(a0)
	ald     $24,(24*8+offes_intregs)(a0)
	ald     $25,(25*8+offes_intregs)(a0)
	ald     $26,(26*8+offes_intregs)(a0)
	ald     $27,(27*8+offes_intregs)(a0)
	ald     $28,(28*8+offes_intregs)(a0)
	/* $29 is sp                      */
	/* $30 is pv                      */
	ald     $31,(31*8+offes_intregs)(a0)
	
#if SIZEOF_VOID_P == 8

	ldc1    $f0 ,( 0*8+offes_fltregs)(a0)
	ldc1    $f1 ,( 1*8+offes_fltregs)(a0)
	ldc1    $f2 ,( 2*8+offes_fltregs)(a0)
	ldc1    $f3 ,( 3*8+offes_fltregs)(a0)
	ldc1    $f4 ,( 4*8+offes_fltregs)(a0)
	ldc1    $f5 ,( 5*8+offes_fltregs)(a0)
	ldc1    $f6 ,( 6*8+offes_fltregs)(a0)
	ldc1    $f7 ,( 7*8+offes_fltregs)(a0)
	ldc1    $f8 ,( 8*8+offes_fltregs)(a0)
	ldc1    $f9 ,( 9*8+offes_fltregs)(a0)
	ldc1    $f10,(10*8+offes_fltregs)(a0)
	ldc1    $f11,(11*8+offes_fltregs)(a0)
	ldc1    $f12,(12*8+offes_fltregs)(a0)
	ldc1    $f13,(13*8+offes_fltregs)(a0)
	ldc1    $f14,(14*8+offes_fltregs)(a0)
	ldc1    $f15,(15*8+offes_fltregs)(a0)
	ldc1    $f16,(16*8+offes_fltregs)(a0)
	ldc1    $f17,(17*8+offes_fltregs)(a0)
	ldc1    $f18,(18*8+offes_fltregs)(a0)
	ldc1    $f19,(19*8+offes_fltregs)(a0)
	ldc1    $f20,(20*8+offes_fltregs)(a0)
	ldc1    $f21,(21*8+offes_fltregs)(a0)
	ldc1    $f22,(22*8+offes_fltregs)(a0)
	ldc1    $f23,(23*8+offes_fltregs)(a0)
	ldc1    $f24,(24*8+offes_fltregs)(a0)
	ldc1    $f25,(25*8+offes_fltregs)(a0)
	ldc1    $f26,(26*8+offes_fltregs)(a0)
	ldc1    $f27,(27*8+offes_fltregs)(a0)
	ldc1    $f28,(28*8+offes_fltregs)(a0)
	ldc1    $f29,(29*8+offes_fltregs)(a0)
	ldc1    $f30,(30*8+offes_fltregs)(a0)
	ldc1    $f31,(31*8+offes_fltregs)(a0)

#else /* SIZEOF_VOID_P == 8 */

	ldc1    $f0 ,( 0*8+offes_fltregs)(a0)
	ldc1    $f2 ,( 2*8+offes_fltregs)(a0)
	ldc1    $f4 ,( 4*8+offes_fltregs)(a0)
	ldc1    $f6 ,( 6*8+offes_fltregs)(a0)
	ldc1    $f8 ,( 8*8+offes_fltregs)(a0)
	ldc1    $f10,(10*8+offes_fltregs)(a0)
	ldc1    $f12,(12*8+offes_fltregs)(a0)
	ldc1    $f14,(14*8+offes_fltregs)(a0)
	ldc1    $f16,(16*8+offes_fltregs)(a0)
	ldc1    $f18,(18*8+offes_fltregs)(a0)
	ldc1    $f20,(20*8+offes_fltregs)(a0)
	ldc1    $f22,(22*8+offes_fltregs)(a0)
	ldc1    $f24,(24*8+offes_fltregs)(a0)
	ldc1    $f26,(26*8+offes_fltregs)(a0)
	ldc1    $f28,(28*8+offes_fltregs)(a0)
	ldc1    $f30,(30*8+offes_fltregs)(a0)

#endif /* SIZEOF_VOID_P == 8 */

	/* load new pc */

	ald     itmp3,offes_pc(a0)

	/* load a0 */
	
	ald     a0,(4*8+offes_intregs)(a0)

	/* jump to new code */

	jr      itmp3

	.end asm_replacement_in

#endif /* defined(ENABLE_REPLACEMENT) */


	.ent    asm_getclassvalues_atomic

asm_getclassvalues_atomic:
_crit_restart:
_crit_begin:
	lw      t0,offbaseval(a0)
	lw      t1,offdiffval(a0)
	lw      t2,offbaseval(a1)
_crit_end:
	sw      t0,offcast_super_baseval(a2)
	sw      t1,offcast_super_diffval(a2)
	sw      t2,offcast_sub_baseval(a2)
	j       ra

	.end    asm_getclassvalues_atomic

    .data

asm_criticalsections:
#if defined(ENABLE_THREADS)
    .dword  _crit_begin
    .dword  _crit_end
    .dword  _crit_restart
#endif
    .dword  0


	.text

	.ent    compare_and_swap

compare_and_swap:
1:
	lw      v0,0(a0)
	bne     v0,a1,2f
	move    t0,a2
	sw      t0,0(a0)
2:
	j       ra

	.end    compare_and_swap


/* disable exec-stacks ********************************************************/

#if defined(__linux__) && defined(__ELF__)
	.section .note.GNU-stack,"",%progbits
#endif


/*
 * These are local overrides for various environment variables in Emacs.
 * Please do not remove this and leave it at the end of the file, where
 * Emacs will automagically detect them.
 * ---------------------------------------------------------------------
 * Local variables:
 * mode: asm
 * indent-tabs-mode: t
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 * vim:noexpandtab:sw=4:ts=4:
 */

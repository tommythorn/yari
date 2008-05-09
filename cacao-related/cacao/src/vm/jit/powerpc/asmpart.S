/* src/vm/jit/powerpc/asmpart.S - Java-C interface functions for PowerPC
		
   Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
   C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
   E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
   J. Wenninger, Institut f. Computersprachen - TU Wien

   This file is part of CACAO.

   This program is free software.text;  you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation;  either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY	;  without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program;  if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   $Id: asmpart.S 7678 2007-04-09 17:23:55Z twisti $

*/


#include "config.h"

#include "md-abi.h"
#include "md-asm.h"

#include "vm/jit/abi-asm.h"
#include "vm/jit/methodheader.h"
#include "vm/jit/powerpc/offsets.h"


	.text

	.align 2


/* export functions ***********************************************************/

	.globl asm_vm_call_method
	.globl asm_vm_call_method_int
	.globl asm_vm_call_method_long
	.globl asm_vm_call_method_float
	.globl asm_vm_call_method_double

	.globl asm_vm_call_method_exception_handler
	.globl asm_vm_call_method_end

	.globl asm_call_jit_compiler

	.globl asm_handle_nat_exception
	.globl asm_handle_exception

	.globl asm_abstractmethoderror

	.globl asm_patcher_wrapper

#if defined(ENABLE_REPLACEMENT)
	.globl asm_replacement_out
	.globl asm_replacement_in
#endif

	.globl asm_cacheflush

	.globl asm_compare_and_swap
	.globl asm_memory_barrier

	.globl asm_criticalsections
	.globl asm_getclassvalues_atomic


/* asm_vm_call_method **********************************************************
*                                                                              *
*   This function calls a Java-method (which possibly needs compilation)       *
*   with up to 4 address parameters.                                           *
*                                                                              *
*   This functions calls the JIT-compiler which eventually translates the      *
*   method into machine code.                                                  *
*                                                                              *
*   C-prototype:                                                               *
*    javaobject_header *asm_calljavamethod (methodinfo *m,                     *
*         void *arg1, void *arg2, void *arg3, void *arg4);                     *
*                                                                              *
*******************************************************************************/

	.align 2

	.long   0                         /* catch type all                       */
	.long   0                         /* exception handler pc                 */
	.long   0                         /* end pc                               */
	.long   0                         /* start pc                             */
	.long   1                         /* extable size                         */
	.long   0                         /* line number table start              */
	.long   0                         /* line number table size               */
	.long   0                         /* fltsave                              */
	.long   0                         /* intsave                              */
	.long   0                         /* IsLeaf                               */
	.long   0                         /* IsSync                               */
	.long   0                         /* frame size                           */
	.long   0                         /* codeinfo pointer                     */

asm_vm_call_method:
asm_vm_call_method_int:
asm_vm_call_method_long:
asm_vm_call_method_float:
asm_vm_call_method_double:
	mflr    r0
	stw     r0,LA_LR_OFFSET(sp)
	stwu    sp,-40*4(sp)              /* keep stack 16-byte aligned           */

	stw     s0,8*4(sp)                /* save used callee saved registers     */
	stw     a0,9*4(sp)                /* save method pointer for compiler     */

#if defined(__DARWIN__)
	stw     itmp1,10*4(sp)            /* register r11 is callee saved         */
#endif
	stw     pv,11*4(sp)               /* save PV register                     */

	stw     itmp3,12*4(sp)            /* registers r14-r31 are callee saved   */
	stfd	ftmp1,14*4(sp)            /* registers f14-f31 are callee saved   */
	stfd	ftmp2,16*4(sp)

#if defined(__DARWIN__)
	stw     t1,18*4(sp)
	stw     t2,19*4(sp)
	stw     t3,20*4(sp)
	stw     t4,21*4(sp)
	stw     t5,22*4(sp)
	stw     t6,23*4(sp)
	stw     t7,24*4(sp)

	stfd	ft0,26*4(sp)
	stfd	ft1,28*4(sp)
	stfd	ft2,30*4(sp)
	stfd	ft3,32*4(sp)
	stfd	ft4,34*4(sp)
	stfd	ft5,36*4(sp)
#else
	SAVE_TEMPORARY_REGISTERS(18)      /* the offset has to be even            */
#endif

	mr      itmp2,a1                  /* arg count                            */
	mr      itmp1,a2                  /* pointer to arg block                 */

	mr      t4,itmp2                  /* save argument count                  */
	mr      t5,itmp1                  /* save argument block pointer          */

	mr      s0,sp                     /* save current sp to s0                */

	addi    itmp1,itmp1,-sizevmarg    /* initialize pointer (smaller code)    */
	addi    itmp2,itmp2,1             /* initialize argument count            */
	li      t0,0                      /* initialize integer argument counter  */
	li      t1,0                      /* initialize float argument counter    */
	li      t6,0                      /* initialize integer register counter  */
#if defined(__DARWIN__)
	li      t7,0                      /* initialize stack slot counter        */
#endif

	mflr    r0                        /* save link register (PIC code)        */
	bl      L_asm_vm_call_method_get_pc
L_asm_vm_call_method_get_pc:
	mflr    t3                        /* t3 contains the current pc           */
	mtlr    r0

L_register_copy:
	addi    itmp1,itmp1,sizevmarg     /* goto next argument block             */
	addi    itmp2,itmp2,-1            /* argument count - 1                   */
	mr.     itmp2,itmp2
	beq     L_register_copy_done

	lwz     itmp3,offvmargtype+4(itmp1)
	andi.   r0,itmp3,0x0002           /* is this a float/double type?         */
	bne     L_register_handle_float
	
L_register_handle_int:
	cmpwi   t6,INT_ARG_CNT            /* are we out of integer argument       */
	beq     L_register_copy           /* registers? yes, next loop            */

	andi.   r0,itmp3,0x0001           /* is this a 2-word type?               */
	bne     L_register_handle_long

#if defined(__DARWIN__)
	addis   itmp3,t3,ha16(L_jumptable_int - L_asm_vm_call_method_get_pc)
	la      itmp3,lo16(L_jumptable_int - L_asm_vm_call_method_get_pc)(itmp3)
#else
	lis     itmp3,L_jumptable_int@ha
	addi    itmp3,itmp3,L_jumptable_int@l
#endif

	slwi    t2,t6,2                   /* multiple of 4-bytes                  */
	add     itmp3,itmp3,t2            /* calculate address of jumptable       */
	lwz     itmp3,0(itmp3)            /* load function address                */
	mtctr   itmp3
	addi    t0,t0,1                   /* integer argument counter             */
	addi    t6,t6,1                   /* integer argument register counter    */
#if defined(__DARWIN__)
	addi    t7,t7,1                   /* stack slot counter                   */
#endif
	bctr

L_register_handle_long:
#if defined(__DARWIN__)
	addis   itmp3,t3,ha16(L_jumptable_long - L_asm_vm_call_method_get_pc)
	la      itmp3,lo16(L_jumptable_long - L_asm_vm_call_method_get_pc)(itmp3)
#else
	lis     itmp3,L_jumptable_long@ha
	addi    itmp3,itmp3,L_jumptable_long@l
#endif
#if !defined(__DARWIN__)
	addi    t6,t6,1                   /* align to even numbers                */
	andi.   t6,t6,0xfffe
#endif

	cmpwi   t6,(INT_ARG_CNT - 1)      /* are we out of integer argument       */
	blt     L_register_handle_long_continue /* registers?                     */

	li      t6,INT_ARG_CNT            /* yes, set integer argument register   */
	b       L_register_copy           /* count to max and next loop           */

L_register_handle_long_continue:
	slwi    t2,t6,2                   /* multiple of 4-bytes                  */
	add     itmp3,itmp3,t2            /* calculate address of jumptable       */
	lwz     itmp3,0(itmp3)            /* load function address                */
	mtctr   itmp3
	addi    t0,t0,1                   /* integer argument counter             */
	addi    t6,t6,2                   /* integer argument register counter    */
#if defined(__DARWIN__)
	addi    t7,t7,2                   /* stack slot counter                   */
#endif
	bctr

L_register_handle_float:
	cmpwi   t1,FLT_ARG_CNT            /* are we out of float argument         */
	beq     L_register_copy           /* registers? yes, next loop            */

	andi.   r0,itmp3,0x0001           /* is this a 2-word type?               */
	bne     L_register_handle_double

#if defined(__DARWIN__)
	addis   itmp3,t3,ha16(L_jumptable_float - L_asm_vm_call_method_get_pc)
	la      itmp3,lo16(L_jumptable_float - L_asm_vm_call_method_get_pc)(itmp3)
#else
	lis     itmp3,L_jumptable_float@ha
	addi    itmp3,itmp3,L_jumptable_float@l
#endif

	slwi    t2,t1,2                   /* multiple of 4-bytes                  */
	add     itmp3,itmp3,t2            /* calculate address of jumptable       */
	lwz     itmp3,0(itmp3)            /* load function address                */
	mtctr   itmp3
	addi    t1,t1,1                   /* float argument counter               */
#if defined(__DARWIN__)
	addi    t7,t7,1                   /* stack slot counter                   */
	addi    t6,t6,1                   /* skip 1 integer argument register     */
#endif
	bctr

L_register_handle_double:
#if defined(__DARWIN__)
	addis   itmp3,t3,ha16(L_jumptable_double - L_asm_vm_call_method_get_pc)
	la      itmp3,lo16(L_jumptable_double - L_asm_vm_call_method_get_pc)(itmp3)
#else
	lis     itmp3,L_jumptable_double@ha
	addi    itmp3,itmp3,L_jumptable_double@l
#endif

	slwi    t2,t1,2                   /* multiple of 4-bytes                  */
	add     itmp3,itmp3,t2            /* calculate address of jumptable       */
	lwz     itmp3,0(itmp3)            /* load function address                */
	mtctr   itmp3
	addi    t1,t1,1                   /* float argument counter               */
#if defined(__DARWIN__)
	addi    t7,t7,2                   /* stack slot counter                   */
	addi    t6,t6,2                   /* skip 2 integer argument registers    */
#endif
	bctr

L_register_copy_done:
	                                  /* calculate remaining arguments        */
	sub     itmp3,t4,t0               /* - integer arguments in registers     */
	sub     itmp3,itmp3,t1            /* - float arguments in registers       */
	mr.     itmp3,itmp3
	beq     L_stack_copy_done

	mr      itmp2,t4                  /* restore argument count               */
	mr      itmp1,t5                  /* restore argument block pointer       */

	slwi    t4,itmp3,3                /* XXX use 8-bytes slots for now        */
	addi    t4,t4,LA_SIZE             /* add size of linkage area             */

#if defined(__DARWIN__)
	slwi    t5,t7,2                   /* add stack space for arguments        */
	add     t4,t4,t5
#endif

	sub     sp,sp,t4

	mr      t6,sp                     /* use t6 as temporary sp               */
	addi    t6,t6,LA_SIZE             /* skip linkage area                    */
#if defined(__DARWIN__)
	add     t6,t6,t5                  /* skip stack space for arguments       */
#endif

	addi    itmp1,itmp1,-sizevmarg    /* initialize pointer (smaller code)    */
	addi    itmp2,itmp2,1             /* initialize argument count            */
	
L_stack_copy_loop:
	addi    itmp1,itmp1,sizevmarg     /* goto next argument block             */
	addi    itmp2,itmp2,-1            /* argument count - 1                   */
	mr.     itmp2,itmp2
	beq     L_stack_copy_done
	
	lwz     itmp3,offvmargtype+4(itmp1)
	andi.   r0,itmp3,0x0002           /* is this a float/double type?         */
	bne     L_stack_handle_float

L_stack_handle_int:
	addi    t0,t0,-1                  /* arguments assigned to registers      */
	mr.     t0,t0
	bge     L_stack_copy_loop

	andi.   r0,itmp3,0x0001           /* is this a 2-word type?               */
	bne     L_stack_handle_long

	lwz     itmp3,offvmargdata+4(itmp1) /* get integer argument               */
	stw     itmp3,0(t6)               /* and store it on the stack            */
	addi    t6,t6,4                   /* increase temporary sp by 1 slot      */
	b       L_stack_copy_loop

L_stack_handle_long:
#if !defined(__DARWIN__)
	addi    t6,t6,4                   /* align stack to 8-bytes               */
	rlwinm  t6,t6,0,30,28             /* clear lower 4-bits                   */
#endif

	lwz     itmp3,offvmargdata+0(itmp1) /* get long argument                  */
	stw     itmp3,0(t6)               /* and store it on the stack            */
	lwz     itmp3,offvmargdata+4(itmp1)
	stw     itmp3,4(t6)
	addi    t6,t6,8                   /* increase temporary sp by 2 slots     */
	b       L_stack_copy_loop
		
L_stack_handle_float:
	addi    t1,t1,-1                  /* arguments assigned to registers      */
	mr.     t1,t1
	bge     L_stack_copy_loop

	andi.   r0,itmp3,0x0001           /* is this a 2-word type?               */
	bne     L_stack_handle_double

	lfs     ftmp3,offvmargdata(itmp1) /* get float argument                   */
	stfs    ftmp3,0(t6)               /* and store it on the stack            */
	addi    t6,t6,4                   /* increase temporary sp by 1 slot      */
	b       L_stack_copy_loop

L_stack_handle_double:
#if !defined(__DARWIN__)
	addi    t6,t6,4                   /* align stack to 8-bytes               */
	rlwinm  t6,t6,0,30,28             /* clear lower 4-bits                   */
#endif

	lfd     ftmp3,offvmargdata(itmp1) /* get double argument                  */
	stfd    ftmp3,0(t6)               /* and store it on the stack            */
	addi    t6,t6,8                   /* increase temporary sp by 2 slots     */
	b       L_stack_copy_loop

L_stack_copy_done:
	lwz     itmp1,9*4(s0)             /* pass method pointer via tmp1         */

#if defined(__DARWIN__)
	addis   mptr,t3,ha16(L_asm_call_jit_compiler - L_asm_vm_call_method_get_pc)
	la      mptr,lo16(L_asm_call_jit_compiler - L_asm_vm_call_method_get_pc)(mptr)
#else
	lis     mptr,L_asm_call_jit_compiler@ha
	addi    mptr,mptr,L_asm_call_jit_compiler@l
#endif
	stw     mptr,7*4(s0)
	addi    mptr,s0,7*4

	lwz     pv,0*4(mptr)
	mtctr   pv
	bctrl
1:
	mflr    itmp1
#if defined(__DARWIN__)
	addi    pv,itmp1,lo16(asm_vm_call_method - 1b)
#else
	addi    pv,itmp1,(asm_vm_call_method - 1b)@l
#endif

L_asm_vm_call_method_return:
	mr      sp,s0                     /* restore the function's sp            */

	lwz     s0,8*4(sp)                /* restore used callee saved registers  */

#if defined(__DARWIN__)
	lwz     itmp1,10*4(sp)            /* register r11 is callee saved         */
#endif
	lwz     pv,11*4(sp)               /* save PV register                     */

	lwz     itmp3,12*4(sp)
	lfd     ftmp1,14*4(sp)            /* registers f14-f31 are callee saved   */
	lfd     ftmp2,16*4(sp)

#if defined(__DARWIN__)
	lwz     t1,18*4(sp)
	lwz     t2,19*4(sp)
	lwz     t3,20*4(sp)
	lwz     t4,21*4(sp)
	lwz     t5,22*4(sp)
	lwz     t6,23*4(sp)
	lwz     t7,24*4(sp)

	lfd     ft0,26*4(sp)
	lfd     ft1,28*4(sp)
	lfd     ft2,30*4(sp)
	lfd     ft3,32*4(sp)
	lfd     ft4,34*4(sp)
	lfd     ft5,36*4(sp)
#else
	RESTORE_TEMPORARY_REGISTERS(18)   /* the offset has to be even            */
#endif

	lwz     r0,40*4+LA_LR_OFFSET(sp)
	mtlr    r0
	addi    sp,sp,40*4
	blr

asm_vm_call_method_exception_handler:
	mr      a0,itmp1
	bl      builtin_throw_exception
	b       L_asm_vm_call_method_return


	.data
	.align  2

L_jumptable_int:
	.long   L_handle_a0
	.long   L_handle_a1
	.long   L_handle_a2
	.long   L_handle_a3
	.long   L_handle_a4
	.long   L_handle_a5
	.long   L_handle_a6
	.long   L_handle_a7

	.text
	.align  2

L_handle_a0:
	lwz     a0,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a1:
	lwz     a1,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a2:
	lwz     a2,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a3:
	lwz     a3,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a4:
	lwz     a4,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a5:
	lwz     a5,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a6:
	lwz     a6,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a7:
	lwz     a7,offvmargdata+4(itmp1)
	b       L_register_copy


	.data
	.align  2

L_jumptable_long:
#if defined(__DARWIN__)
	.long   L_handle_a0_a1
	.long   L_handle_a1_a2
	.long   L_handle_a2_a3
	.long   L_handle_a3_a4
	.long   L_handle_a4_a5
	.long   L_handle_a5_a6
	.long   L_handle_a6_a7
#else
	/* we have two entries here, so we get the even argument register
	alignment for linux */

	.long   L_handle_a0_a1
	.long   0
	.long   L_handle_a2_a3
	.long   0
	.long   L_handle_a4_a5
	.long   0
	.long   L_handle_a6_a7
#endif

	.text
	.align  2

L_handle_a0_a1:
	lwz     a0,offvmargdata+0(itmp1)
	lwz     a1,offvmargdata+4(itmp1)
	b       L_register_copy
#if defined(__DARWIN__)
L_handle_a1_a2:
	lwz     a1,offvmargdata+0(itmp1)
	lwz     a2,offvmargdata+4(itmp1)
	b       L_register_copy
#endif
L_handle_a2_a3:
	lwz     a2,offvmargdata+0(itmp1)
	lwz     a3,offvmargdata+4(itmp1)
	b       L_register_copy
#if defined(__DARWIN__)
L_handle_a3_a4:
	lwz     a3,offvmargdata+0(itmp1)
	lwz     a4,offvmargdata+4(itmp1)
	b       L_register_copy
#endif
L_handle_a4_a5:
	lwz     a4,offvmargdata+0(itmp1)
	lwz     a5,offvmargdata+4(itmp1)
	b       L_register_copy
#if defined(__DARWIN__)
L_handle_a5_a6:
	lwz     a5,offvmargdata+0(itmp1)
	lwz     a6,offvmargdata+4(itmp1)
	b       L_register_copy
#endif
L_handle_a6_a7:
	lwz     a6,offvmargdata+0(itmp1)
	lwz     a7,offvmargdata+4(itmp1)
	b       L_register_copy


	.data
	.align  2

L_jumptable_float:
	.long   L_handle_fa0
	.long   L_handle_fa1
	.long   L_handle_fa2
	.long   L_handle_fa3
	.long   L_handle_fa4
	.long   L_handle_fa5
	.long   L_handle_fa6
	.long   L_handle_fa7

#if defined(__DARWIN__)
	.long   L_handle_fa8
	.long   L_handle_fa9
	.long   L_handle_fa10
	.long   L_handle_fa11
	.long   L_handle_fa12
#endif

	.text
	.align  2

L_handle_fa0:
	lfs     fa0,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa1:
	lfs     fa1,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa2:
	lfs     fa2,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa3:
	lfs     fa3,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa4:
	lfs     fa4,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa5:
	lfs     fa5,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa6:
	lfs     fa6,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa7:
	lfs     fa7,offvmargdata(itmp1)
	b       L_register_copy

#if defined(__DARWIN__)
L_handle_fa8:
	lfs     fa8,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa9:
	lfs     fa9,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa10:
	lfs     fa10,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa11:
	lfs     fa11,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fa12:
	lfs     fa12,offvmargdata(itmp1)
	b       L_register_copy
#endif


	.data
	.align  2

L_jumptable_double:
	.long   L_handle_fda0
	.long   L_handle_fda1
	.long   L_handle_fda2
	.long   L_handle_fda3
	.long   L_handle_fda4
	.long   L_handle_fda5
	.long   L_handle_fda6
	.long   L_handle_fda7

#if defined(__DARWIN__)
	.long   L_handle_fda8
	.long   L_handle_fda9
	.long   L_handle_fda10
	.long   L_handle_fda11
	.long   L_handle_fda12
#endif

	.text
	.align  2

L_handle_fda0:
	lfd     fa0,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda1:
	lfd     fa1,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda2:
	lfd     fa2,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda3:
	lfd     fa3,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda4:
	lfd     fa4,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda5:
	lfd     fa5,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda6:
	lfd     fa6,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda7:
	lfd     fa7,offvmargdata(itmp1)
	b       L_register_copy

#if defined(__DARWIN__)
L_handle_fda8:
	lfd     fa8,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda9:
	lfd     fa9,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda10:
	lfd     fa10,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda11:
	lfd     fa11,offvmargdata(itmp1)
	b       L_register_copy
L_handle_fda12:
	lfd     fa12,offvmargdata(itmp1)
	b       L_register_copy
#endif

asm_vm_call_method_end:
	nop

/* asm_call_jit_compiler *******************************************************

   Invokes the compiler for untranslated JavaVM methods.

*******************************************************************************/

asm_call_jit_compiler:
L_asm_call_jit_compiler:                /* required for PIC code              */
	mflr    r0
	stw     r0,LA_LR_OFFSET(sp)         /* save return address                */
	stwu    sp,-(LA_SIZE + 4*4 + INT_ARG_CNT*4 + FLT_ARG_CNT*8)(sp)

#if defined(__DARWIN__)
	stw     a0,LA_SIZE+(4+0)*4(sp)
	stw     a1,LA_SIZE+(4+1)*4(sp)
	stw     a2,LA_SIZE+(4+2)*4(sp)
	stw     a3,LA_SIZE+(4+3)*4(sp)
	stw     a4,LA_SIZE+(4+4)*4(sp)
	stw     a5,LA_SIZE+(4+5)*4(sp)
	stw     a6,LA_SIZE+(4+6)*4(sp)
	stw     a7,LA_SIZE+(4+7)*4(sp)

	stfd    fa0,LA_SIZE+(4+8)*4(sp)
	stfd    fa1,LA_SIZE+(4+10)*4(sp)
	stfd    fa2,LA_SIZE+(4+12)*4(sp)
	stfd    fa3,LA_SIZE+(4+14)*4(sp)
	stfd    fa4,LA_SIZE+(4+16)*4(sp)
	stfd    fa5,LA_SIZE+(4+18)*4(sp)
	stfd    fa6,LA_SIZE+(4+20)*4(sp)
	stfd    fa7,LA_SIZE+(4+22)*4(sp)
	stfd    fa8,LA_SIZE+(4+24)*4(sp)
	stfd    fa9,LA_SIZE+(4+26)*4(sp)
	stfd    fa10,LA_SIZE+(4+28)*4(sp)
	stfd    fa11,LA_SIZE+(4+30)*4(sp)
	stfd    fa12,LA_SIZE+(4+32)*4(sp)
#else
	SAVE_ARGUMENT_REGISTERS(LA_SIZE_IN_POINTERS)
#endif

	mr      a0,itmp1
	mr      a1,mptr
	addi    a2,sp,(LA_SIZE + 4*4 + INT_ARG_CNT*4 + FLT_ARG_CNT*8)
	lwz     a3,(LA_SIZE + 4*4 + INT_ARG_CNT*4 + FLT_ARG_CNT*8)+LA_LR_OFFSET(sp)
	bl      jit_asm_compile
	mr      pv,v0                       /* move address to pv register        */

#if defined(__DARWIN__)
	lwz     a0,LA_SIZE+(4+0)*4(sp)
	lwz     a1,LA_SIZE+(4+1)*4(sp)
	lwz     a2,LA_SIZE+(4+2)*4(sp)
	lwz     a3,LA_SIZE+(4+3)*4(sp)
	lwz     a4,LA_SIZE+(4+4)*4(sp)
	lwz     a5,LA_SIZE+(4+5)*4(sp)
	lwz     a6,LA_SIZE+(4+6)*4(sp)
	lwz     a7,LA_SIZE+(4+7)*4(sp)

	lfd     fa0,LA_SIZE+(4+8)*4(sp)
	lfd     fa1,LA_SIZE+(4+10)*4(sp)
	lfd     fa2,LA_SIZE+(4+12)*4(sp)
	lfd     fa3,LA_SIZE+(4+14)*4(sp)
	lfd     fa4,LA_SIZE+(4+16)*4(sp)
	lfd     fa5,LA_SIZE+(4+18)*4(sp)
	lfd     fa6,LA_SIZE+(4+20)*4(sp)
	lfd     fa7,LA_SIZE+(4+22)*4(sp)
	lfd     fa8,LA_SIZE+(4+24)*4(sp)
	lfd     fa9,LA_SIZE+(4+26)*4(sp)
	lfd     fa10,LA_SIZE+(4+28)*4(sp)
	lfd     fa11,LA_SIZE+(4+30)*4(sp)
	lfd     fa12,LA_SIZE+(4+32)*4(sp)
#else
	RESTORE_ARGUMENT_REGISTERS(LA_SIZE_IN_POINTERS)
#endif

	lwz     itmp1,(LA_SIZE + 4*4 + INT_ARG_CNT*4 + FLT_ARG_CNT*8)+LA_LR_OFFSET(sp)
	mtlr    itmp1

	addi    sp,sp,(LA_SIZE + 4*4 + INT_ARG_CNT*4 + FLT_ARG_CNT*8)

	mr.     pv,pv                       /* test for exception                 */
	beq     L_asm_call_jit_compiler_exception

	mtctr   pv                          /* move method address to control reg */
	bctr                                /* and call the Java method           */

L_asm_call_jit_compiler_exception:
	mflr	r0
	stw     r0,LA_LR_OFFSET(sp)
	stwu    sp,-LA_SIZE_ALIGNED(sp)     /* preserve linkage area              */
	bl      exceptions_get_and_clear_exception
	lwz     xpc,LA_SIZE_ALIGNED+LA_LR_OFFSET(sp)
	mtlr    xpc	
	addi    sp,sp,LA_SIZE_ALIGNED

	mr      xptr,v0                     /* get exception                      */
	addi    xpc,xpc,-4                  /* exception address is ra - 4        */
	b       L_asm_handle_nat_exception


/********************* function asm_handle_exception ***************************
*                                                                              *
*   This function handles an exception. It does not use the usual calling      *
*   conventions. The exception pointer is passed in REG_ITMP1 and the          *
*   pc from the exception raising position is passed in REG_ITMP2. It searches *
*   the local exception table for a handler. If no one is found, it unwinds    *
*   stacks and continues searching the callers.                                *
*                                                                              *
*   void asm_handle_exception (exceptionptr, exceptionpc);                     *
*                                                                              *
*******************************************************************************/
		
asm_handle_nat_exception:
L_asm_handle_nat_exception:             /* required for PIC code              */
L_asm_handle_exception_stack_loop:
	mflr    r0
	addi    sp,sp,-(LA_SIZE+((4+6)*4))  /* allocate stack (+4 for darwin)     */
	stw     xptr,LA_SIZE+(4+0)*4(sp)    /* save exception pointer             */
	stw     xpc,LA_SIZE+(4+1)*4(sp)     /* save exception pc                  */
	stw     r0,LA_SIZE+(4+3)*4(sp)      /* save return address                */
	li      itmp3,0
	stw     itmp3,LA_SIZE+(4+4)*4(sp)   /* save maybe-leaf flag (cleared)     */

	mr      a0,r0                       /* pass return address                */
	bl      md_codegen_get_pv_from_pc   /* get PV from RA                     */
	stw     v0,LA_SIZE+(4+2)*4(sp)      /* save data segment pointer          */

	lwz     a0,LA_SIZE+(4+0)*4(sp)      /* pass xptr                          */
	lwz     a1,LA_SIZE+(4+1)*4(sp)      /* pass xpc                           */
	lwz     a2,LA_SIZE+(4+2)*4(sp)      /* pass PV (v0 == a0)                 */
	addi    a3,sp,LA_SIZE+((4+6)*4)     /* pass Java SP                       */

	b       L_asm_handle_exception_continue

asm_handle_exception:
L_asm_handle_exception:                 /* required for PIC code              */
	addi    sp,sp,-(ARG_CNT+TMP_CNT)*8  /* create maybe-leaf stackframe       */

#if defined(__DARWIN__)
#else
	SAVE_ARGUMENT_REGISTERS(0)          /* we save arg and temp registers in  */
	SAVE_TEMPORARY_REGISTERS(ARG_CNT)   /* case this is a leaf method         */
#endif

	addi    sp,sp,-(LA_SIZE+(4+6)*4)    /* allocate stack                     */
	stw     xptr,LA_SIZE+(4+0)*4(sp)    /* save xptr                          */
	stw     pv,LA_SIZE+(4+2)*4(sp)      /* save PV                            */
	mflr    r0                          /* save RA                            */
	stw     r0,LA_SIZE+(4+3)*4(sp)
	li      t0,1                        /* set maybe-leaf flag                */
	stw     t0,LA_SIZE+(4+4)*4(sp)      /* save maybe-leaf flag               */

	mr      a0,xptr                     /* pass exception pointer             */
	mr      a1,xpc                      /* pass exception pc                  */
	mr      a2,pv                       /* pass data segment pointer          */
	addi    a3,sp,LA_SIZE+(ARG_CNT+TMP_CNT)*8+(4+6)*4

L_asm_handle_exception_continue:
	bl      exceptions_handle_exception

	mr.     v0,v0
	beq     L_asm_handle_exception_not_catched

	mr      xpc,v0                      /* move handlerpc into xpc            */
	lwz     xptr,LA_SIZE+(4+0)*4(sp)    /* restore xptr                       */
	lwz     pv,LA_SIZE+(4+2)*4(sp)      /* restore PV                         */
	lwz     r0,LA_SIZE+(4+3)*4(sp)      /* restore RA                         */
	mtlr    r0
	lwz     t0,LA_SIZE+(4+4)*4(sp)      /* get maybe-leaf flag                */
	addi    sp,sp,LA_SIZE+(4+6)*4       /* free stack frame                   */

	mr.     t0,t0
	beq     L_asm_handle_exception_no_leaf

#if defined(__DARWIN__)
#else
	RESTORE_ARGUMENT_REGISTERS(0)       /* if this is a leaf method, we have  */
	RESTORE_TEMPORARY_REGISTERS(ARG_CNT)/* to restore arg and temp registers  */
#endif

	addi    sp,sp,(ARG_CNT+TMP_CNT)*8   /* remove maybe-leaf stackframe       */

L_asm_handle_exception_no_leaf:
	mtctr   xpc                         /* jump to the handler                */
	bctr

L_asm_handle_exception_not_catched:
	lwz     xptr,LA_SIZE+(4+0)*4(sp)    /* restore xptr                       */
	lwz     pv,LA_SIZE+(4+2)*4(sp)      /* restore PV                         */
	lwz     r0,LA_SIZE+(4+3)*4(sp)      /* restore RA                         */
	mtlr    r0
	lwz     t0,LA_SIZE+(4+4)*4(sp)      /* get maybe-leaf flag                */
	addi    sp,sp,LA_SIZE+(4+6)*4       /* free stack frame                   */

	mr.     t0,t0
	beq     L_asm_handle_exception_no_leaf_stack

	addi    sp,sp,(ARG_CNT+TMP_CNT)*8   /* remove maybe-leaf stackframe       */
	li      t0,0                        /* clear the maybe-leaf flag          */

L_asm_handle_exception_no_leaf_stack:
	lwz     t1,FrameSize(pv)            /* get frame size                     */
	add     t1,sp,t1                    /* pointer to save area               */

	lwz     t2,IsLeaf(pv)               /* is leaf procedure                  */
	mr.     t2,t2
	bne     L_asm_handle_exception_no_ra_restore

	lwz     r0,LA_LR_OFFSET(t1)         /* restore ra                         */
	mtlr    r0

L_asm_handle_exception_no_ra_restore:
	mflr    xpc                         /* the new xpc is ra                  */
	mr      t4,xpc                      /* save RA                            */
	lwz     t2,IntSave(pv)              /* t2 = saved int register count      */
	bl      ex_int1
ex_int1:
	mflr    t3                          /* t3 = current pc                    */
#if defined(__DARWIN__)
	addi    t3,t3,lo16(ex_int2-ex_int1)
#else
	addi    t3,t3,(ex_int2-ex_int1)@l
#endif
	slwi    t2,t2,2                     /* t2 = register count * 4            */
	subf    t3,t2,t3                    /* t3 = IntSave - t2                  */
	mtctr   t3
	bctr

	lwz     s0,-10*4(t1)
	lwz     s1,-9*4(t1)
	lwz     s2,-8*4(t1)
	lwz     s3,-7*4(t1)
	lwz     s4,-6*4(t1)
	lwz     s5,-5*4(t1)
	lwz     s6,-4*4(t1)
	lwz     s7,-3*4(t1)
	lwz     s8,-2*4(t1)
	lwz     s9,-1*4(t1)

ex_int2:
	subf    t1,t2,t1                    /* t1 = t1 - register count * 4       */

	lwz     t2,FltSave(pv)
	bl      ex_flt1
ex_flt1:
	mflr    t3
#if defined(__DARWIN__)
	addi    t3,t3,lo16(ex_flt2-ex_flt1)
#else
	addi    t3,t3,(ex_flt2-ex_flt1)@l
#endif
	slwi    t2,t2,2                     /* t2 = register count * 4            */
	subf    t3,t2,t3                    /* t3 = FltSave - t2                  */
	mtctr   t3
	bctr

	lfd     fs0,-10*8(t1)
	lfd     fs1,-9*8(t1)
	lfd     fs2,-8*8(t1)
	lfd     fs3,-7*8(t1)
	lfd     fs4,-6*8(t1)
	lfd     fs5,-5*8(t1)
	lfd     fs6,-4*8(t1)
	lfd     fs7,-3*8(t1)
	lfd     fs8,-2*8(t1)
	lfd     fs9,-1*8(t1)

ex_flt2:
	mtlr    t4                          /* restore RA                         */
	lwz     t1,FrameSize(pv)            /* get frame size                     */
	add     sp,sp,t1                    /* unwind stack                       */
	b       L_asm_handle_exception_stack_loop


/* asm_abstractmethoderror *****************************************************

   Creates and throws an AbstractMethodError.

*******************************************************************************/

asm_abstractmethoderror:
	mflr	r0
	stw     r0,LA_LR_OFFSET(sp)
	stwu    sp,-LA_SIZE_ALIGNED(sp)     /* preserve linkage area              */
	addi    a0,sp,LA_SIZE_ALIGNED       /* pass java sp                       */
	mr      a1,r0                       /* pass exception address             */
	bl      exceptions_asm_new_abstractmethoderror
	lwz     r0,LA_SIZE_ALIGNED+LA_LR_OFFSET(sp)
	mtlr    r0                          /* restore return address             */
	addi    sp,sp,LA_SIZE_ALIGNED

	mr      xptr,v0                     /* get exception pointer              */
	mr      xpc,r0                      /* we can't use r0 directly in addi   */
	addi    xpc,xpc,-4                  /* exception address is ra - 4        */
	b       L_asm_handle_nat_exception


/* asm_patcher_wrapper *********************************************************

   XXX

   Stack layout:
     20   return address into JIT code (patch position)
     16   pointer to virtual java_objectheader
     12   machine code (which is patched back later)
      8   unresolved class/method/field reference
      4   data segment displacement from load instructions
      0   patcher function pointer to call (pv is saved here afterwards)

*******************************************************************************/

asm_patcher_wrapper:
	mflr	r0                    /* get Java return address (leaf)           */
	stw     r0,6*4(sp)            /* store it in the stub stackframe          */
	                              /* keep stack 16-bytes aligned: 6+1+37 = 44 */
	stwu    sp,-(LA_SIZE+(5+58)*4)(sp)

#if defined(__DARWIN__)
	stw     a0,LA_SIZE+(5+0)*4(sp)      /* save argument registers            */
	stw     a1,LA_SIZE+(5+1)*4(sp)      /* preserve linkage area (24 bytes)   */
	stw     a2,LA_SIZE+(5+2)*4(sp)      /* and 4 bytes for 4 argument         */
	stw     a3,LA_SIZE+(5+3)*4(sp)
	stw     a4,LA_SIZE+(5+4)*4(sp)
	stw     a5,LA_SIZE+(5+5)*4(sp)
	stw     a6,LA_SIZE+(5+6)*4(sp)
	stw     a7,LA_SIZE+(5+7)*4(sp)

	stfd    fa0,LA_SIZE+(5+8)*4(sp)
	stfd    fa1,LA_SIZE+(5+10)*4(sp)
	stfd    fa2,LA_SIZE+(5+12)*4(sp)
	stfd    fa3,LA_SIZE+(5+14)*4(sp)
	stfd    fa4,LA_SIZE+(5+16)*4(sp)
	stfd    fa5,LA_SIZE+(5+18)*4(sp)
	stfd    fa6,LA_SIZE+(5+20)*4(sp)
	stfd    fa7,LA_SIZE+(5+22)*4(sp)
	stfd    fa8,LA_SIZE+(5+24)*4(sp)
	stfd    fa9,LA_SIZE+(5+26)*4(sp)
	stfd    fa10,LA_SIZE+(5+28)*4(sp)
	stfd    fa11,LA_SIZE+(5+30)*4(sp)
	stfd    fa12,LA_SIZE+(5+32)*4(sp)

	stw     t0,LA_SIZE+(5+33)*4(sp)
	stw     t1,LA_SIZE+(5+34)*4(sp)
	stw     t2,LA_SIZE+(5+35)*4(sp)
	stw     t3,LA_SIZE+(5+36)*4(sp)
	stw     t4,LA_SIZE+(5+37)*4(sp)
	stw     t5,LA_SIZE+(5+38)*4(sp)
	stw     t6,LA_SIZE+(5+39)*4(sp)
	stw     t7,LA_SIZE+(5+40)*4(sp)

	stfd    ft0,LA_SIZE+(5+42)*4(sp)
	stfd    ft1,LA_SIZE+(5+44)*4(sp)
	stfd    ft2,LA_SIZE+(5+46)*4(sp)
	stfd    ft3,LA_SIZE+(5+48)*4(sp)
	stfd    ft4,LA_SIZE+(5+50)*4(sp)
	stfd    ft5,LA_SIZE+(5+52)*4(sp)
#else
	/* save 8 int/8 float arguments */
	SAVE_ARGUMENT_REGISTERS(LA_SIZE_IN_POINTERS+1)
	SAVE_TEMPORARY_REGISTERS(LA_SIZE_IN_POINTERS+1+24)
#endif

	stw     itmp1,LA_SIZE+(5+54)*4(sp)
	stw     itmp2,LA_SIZE+(5+55)*4(sp)
	stw     pv,LA_SIZE+(5+56)*4(sp)

	addi    a0,sp,LA_SIZE+(5+58)*4      /* pass SP of patcher stub            */
	mr      a1,pv                       /* pass PV                            */
	mr      a2,r0                       /* pass RA (correct for leafs)        */
	bl      patcher_wrapper
	stw     v0,LA_SIZE+(5+57)*4(sp)     /* save return value                  */

#if defined(__DARWIN__)
	lwz     a0,LA_SIZE+(5+0)*4(sp)
	lwz     a1,LA_SIZE+(5+1)*4(sp)
	lwz     a2,LA_SIZE+(5+2)*4(sp)
	lwz     a3,LA_SIZE+(5+3)*4(sp)
	lwz     a4,LA_SIZE+(5+4)*4(sp)
	lwz     a5,LA_SIZE+(5+5)*4(sp)
	lwz     a6,LA_SIZE+(5+6)*4(sp)
	lwz     a7,LA_SIZE+(5+7)*4(sp)

	lfd     fa0,LA_SIZE+(5+8)*4(sp)
	lfd     fa1,LA_SIZE+(5+10)*4(sp)
	lfd     fa2,LA_SIZE+(5+12)*4(sp)
	lfd     fa3,LA_SIZE+(5+14)*4(sp)
	lfd     fa4,LA_SIZE+(5+16)*4(sp)
	lfd     fa5,LA_SIZE+(5+18)*4(sp)
	lfd     fa6,LA_SIZE+(5+20)*4(sp)
	lfd     fa7,LA_SIZE+(5+22)*4(sp)
	lfd     fa8,LA_SIZE+(5+24)*4(sp)
	lfd     fa9,LA_SIZE+(5+26)*4(sp)
	lfd     fa10,LA_SIZE+(5+28)*4(sp)
	lfd     fa11,LA_SIZE+(5+30)*4(sp)
	lfd     fa12,LA_SIZE+(5+32)*4(sp)

	lwz     t0,LA_SIZE+(5+33)*4(sp)
	lwz     t1,LA_SIZE+(5+34)*4(sp)
	lwz     t2,LA_SIZE+(5+35)*4(sp)
	lwz     t3,LA_SIZE+(5+36)*4(sp)
	lwz     t4,LA_SIZE+(5+37)*4(sp)
	lwz     t5,LA_SIZE+(5+38)*4(sp)
	lwz     t6,LA_SIZE+(5+39)*4(sp)
	lwz     t7,LA_SIZE+(5+40)*4(sp)

	lfd     ft0,LA_SIZE+(5+42)*4(sp)
	lfd     ft1,LA_SIZE+(5+44)*4(sp)
	lfd     ft2,LA_SIZE+(5+46)*4(sp)
	lfd     ft3,LA_SIZE+(5+48)*4(sp)
	lfd     ft4,LA_SIZE+(5+50)*4(sp)
	lfd     ft5,LA_SIZE+(5+52)*4(sp)
#else
	/* restore 8 int/8 float arguments */
	RESTORE_ARGUMENT_REGISTERS(LA_SIZE_IN_POINTERS+1)
	RESTORE_TEMPORARY_REGISTERS(LA_SIZE_IN_POINTERS+1+24)
#endif

	lwz     itmp1,LA_SIZE+(5+54)*4(sp)
	lwz     itmp2,LA_SIZE+(5+55)*4(sp)
	lwz     pv,LA_SIZE+(5+56)*4(sp)
	lwz     itmp3,LA_SIZE+(5+57)*4(sp)  /* restore return value into temp reg.*/

	lwz     r0,6*4+LA_SIZE+(5+58)*4(sp) /* restore RA                         */
	mtlr	r0

	mr.     itmp3,itmp3           /* check for an exception                   */
	bne     L_asm_patcher_wrapper_exception

	                              /* get return address (into JIT code)       */
	lwz     itmp3,5*4+LA_SIZE+(5+58)*4(sp)

	                              /* remove stack frame + patcher stub stack  */
	addi    sp,sp,8*4+LA_SIZE+(5+58)*4

	mtctr   itmp3
	bctr                          /* jump to new patched code                 */

L_asm_patcher_wrapper_exception:
	mr      xptr,itmp3                  /* get exception                      */
	lwz     xpc,5*4+LA_SIZE+(5+58)*4(sp)
	addi    sp,sp,8*4+LA_SIZE+(5+58)*4
	b       L_asm_handle_exception

#if defined(ENABLE_REPLACEMENT)

/* asm_replacement_out *********************************************************

   This code is jumped to from the replacement-out stubs that are executed
   when a thread reaches an activated replacement point.

   The purpose of asm_replacement_out is to read out the parts of the
   execution state that cannot be accessed from C code, store this state,
   and then call the C function replace_me.

   Stack layout:
      16                start of stack inside method to replace
      0   rplpoint *    info on the replacement point that was reached

   NOTE: itmp3 has been clobbered by the replacement-out stub!

*******************************************************************************/

/* some room to accomodate changes of the stack frame size during replacement */
	/* XXX we should find a cleaner solution here */
#define REPLACEMENT_ROOM  512

#define sizeexecutionstate_ALIGNED  ((sizeexecutionstate + 15) & ~15)

asm_replacement_out:
    /* create stack frame */
	addi    sp,sp,-(sizeexecutionstate_ALIGNED + REPLACEMENT_ROOM)

	/* save link register */
	mflr    itmp3

	/* save registers in execution state */
	stw     r0 ,( 0*4+offes_intregs)(sp)
	stw     r1 ,( 1*4+offes_intregs)(sp)
	stw     r2 ,( 2*4+offes_intregs)(sp)
	stw     r3 ,( 3*4+offes_intregs)(sp)
	stw     r4 ,( 4*4+offes_intregs)(sp)
	stw     r5 ,( 5*4+offes_intregs)(sp)
	stw     r6 ,( 6*4+offes_intregs)(sp)
	stw     r7 ,( 7*4+offes_intregs)(sp)
	stw     r8 ,( 8*4+offes_intregs)(sp)
	stw     r9 ,( 9*4+offes_intregs)(sp)
	stw     r10,(10*4+offes_intregs)(sp)
	stw     r11,(11*4+offes_intregs)(sp)
	stw     r12,(12*4+offes_intregs)(sp)
	stw     r13,(13*4+offes_intregs)(sp)
	stw     r14,(14*4+offes_intregs)(sp)
	stw     r15,(15*4+offes_intregs)(sp)
	stw     r16,(16*4+offes_intregs)(sp) /* link register stored as itmp3 */
	stw     r17,(17*4+offes_intregs)(sp)
	stw     r18,(18*4+offes_intregs)(sp)
	stw     r19,(19*4+offes_intregs)(sp)
	stw     r20,(20*4+offes_intregs)(sp)
	stw     r21,(21*4+offes_intregs)(sp)
	stw     r22,(22*4+offes_intregs)(sp)
	stw     r23,(23*4+offes_intregs)(sp)
	stw     r24,(24*4+offes_intregs)(sp)
	stw     r25,(25*4+offes_intregs)(sp)
	stw     r26,(26*4+offes_intregs)(sp)
	stw     r27,(27*4+offes_intregs)(sp)
	stw     r28,(28*4+offes_intregs)(sp)
	stw     r29,(29*4+offes_intregs)(sp)
	stw     r30,(30*4+offes_intregs)(sp)
	stw     r31,(31*4+offes_intregs)(sp)
	
	stfd    fr0 ,( 0*8+offes_fltregs)(sp)
	stfd    fr1 ,( 1*8+offes_fltregs)(sp)
	stfd    fr2 ,( 2*8+offes_fltregs)(sp)
	stfd    fr3 ,( 3*8+offes_fltregs)(sp)
	stfd    fr4 ,( 4*8+offes_fltregs)(sp)
	stfd    fr5 ,( 5*8+offes_fltregs)(sp)
	stfd    fr6 ,( 6*8+offes_fltregs)(sp)
	stfd    fr7 ,( 7*8+offes_fltregs)(sp)
	stfd    fr8 ,( 8*8+offes_fltregs)(sp)
	stfd    fr9 ,( 9*8+offes_fltregs)(sp)
	stfd    fr10,(10*8+offes_fltregs)(sp)
	stfd    fr11,(11*8+offes_fltregs)(sp)
	stfd    fr12,(12*8+offes_fltregs)(sp)
	stfd    fr13,(13*8+offes_fltregs)(sp)
	stfd    fr14,(14*8+offes_fltregs)(sp)
	stfd    fr15,(15*8+offes_fltregs)(sp)
	stfd    fr16,(16*8+offes_fltregs)(sp)
	stfd    fr17,(17*8+offes_fltregs)(sp)
	stfd    fr18,(18*8+offes_fltregs)(sp)
	stfd    fr19,(19*8+offes_fltregs)(sp)
	stfd    fr20,(20*8+offes_fltregs)(sp)
	stfd    fr21,(21*8+offes_fltregs)(sp)
	stfd    fr22,(22*8+offes_fltregs)(sp)
	stfd    fr23,(23*8+offes_fltregs)(sp)
	stfd    fr24,(24*8+offes_fltregs)(sp)
	stfd    fr25,(25*8+offes_fltregs)(sp)
	stfd    fr26,(26*8+offes_fltregs)(sp)
	stfd    fr27,(27*8+offes_fltregs)(sp)
	stfd    fr28,(28*8+offes_fltregs)(sp)
	stfd    fr29,(29*8+offes_fltregs)(sp)
	stfd    fr30,(30*8+offes_fltregs)(sp)
	stfd    fr31,(31*8+offes_fltregs)(sp)
	
	/* calculate sp of method */
	addi    itmp1,sp,(sizeexecutionstate_ALIGNED + REPLACEMENT_ROOM + 4*4)
	stw     itmp1,(offes_sp)(sp)

	/* store pv */
	stw     pv,(offes_pv)(sp)

	/* call replace_me */
	lwz     a0,-(4*4)(itmp1)            /* arg0: rplpoint *                   */
	mr      a1,sp                       /* arg1: execution state              */
	addi    sp,sp,-(LA_SIZE_ALIGNED)
	b       replace_me                  /* call C function replace_me         */

/* asm_replacement_in **********************************************************

   This code writes the given execution state and jumps to the replacement
   code.

   This function never returns!

   NOTE: itmp3 is not restored!

   C prototype:
      void asm_replacement_in(executionstate *es, replace_safestack_t *st);

*******************************************************************************/

asm_replacement_in:
	/* a0 == executionstate *es      */
	/* a1 == replace_safestack_t *st */

	/* get arguments */
	mr		s1,a1                       /* replace_safestack_t *st            */
	mr		s2,a0                       /* executionstate *es == safe stack   */

	/* switch to the safe stack */
	mr		sp,s2

	/* reserve linkage area */
	addi	sp,sp,-(LA_SIZE_ALIGNED)

	/* call replace_build_execution_state(st) */
	mr		a0,s1
	bl		replace_build_execution_state

	/* set new sp */
	lwz		sp,(offes_sp)(s2)

	/* build stack frame */
	addi	sp,sp,-(sizeexecutionstate_ALIGNED)

	/* call replace_free_safestack(st,& of allocated executionstate_t) */
	mr		a1,sp /* tmpes */
	mr		a0,s1 /* st    */
	addi    sp,sp,-(LA_SIZE_ALIGNED)  /* reserve linkage area */
	bl		replace_free_safestack
	addi    sp,sp,+(LA_SIZE_ALIGNED)  /* tear down linkage area */

	/* set new pv */
	lwz     pv,(offes_pv)(sp)
	
	/* copy registers from execution state */
	lwz     r0 ,( 0*4+offes_intregs)(sp)
	/* r1 is sp                       */
	/* r2 is reserved                 */
	lwz     a0 ,( 3*4+offes_intregs)(sp)
	lwz     r4 ,( 4*4+offes_intregs)(sp)
	lwz     r5 ,( 5*4+offes_intregs)(sp)
	lwz     r6 ,( 6*4+offes_intregs)(sp)
	lwz     r7 ,( 7*4+offes_intregs)(sp)
	lwz     r8 ,( 8*4+offes_intregs)(sp)
	lwz     r9 ,( 9*4+offes_intregs)(sp)
	lwz     r10,(10*4+offes_intregs)(sp)
	lwz     r11,(11*4+offes_intregs)(sp)
	lwz     r12,(12*4+offes_intregs)(sp)
	/* r13 is pv                      */
	lwz     r14,(14*4+offes_intregs)(sp)
	lwz     r15,(15*4+offes_intregs)(sp)
	lwz     r16,(16*4+offes_intregs)(sp) /* itmp3, later to link register */
	lwz     r17,(17*4+offes_intregs)(sp)
	lwz     r18,(18*4+offes_intregs)(sp)
	lwz     r19,(19*4+offes_intregs)(sp)
	lwz     r20,(20*4+offes_intregs)(sp)
	lwz     r21,(21*4+offes_intregs)(sp)
	lwz     r22,(22*4+offes_intregs)(sp)
	lwz     r23,(23*4+offes_intregs)(sp)
	lwz     r24,(24*4+offes_intregs)(sp)
	lwz     r25,(25*4+offes_intregs)(sp)
	lwz     r26,(26*4+offes_intregs)(sp)
	lwz     r27,(27*4+offes_intregs)(sp)
	lwz     r28,(28*4+offes_intregs)(sp)
	lwz     r29,(29*4+offes_intregs)(sp)
	lwz     r30,(30*4+offes_intregs)(sp)
	lwz     r31,(31*4+offes_intregs)(sp)
	
	lfd     fr0 ,( 0*8+offes_fltregs)(sp)
	lfd     fr1 ,( 1*8+offes_fltregs)(sp)
	lfd     fr2 ,( 2*8+offes_fltregs)(sp)
	lfd     fr3 ,( 3*8+offes_fltregs)(sp)
	lfd     fr4 ,( 4*8+offes_fltregs)(sp)
	lfd     fr5 ,( 5*8+offes_fltregs)(sp)
	lfd     fr6 ,( 6*8+offes_fltregs)(sp)
	lfd     fr7 ,( 7*8+offes_fltregs)(sp)
	lfd     fr8 ,( 8*8+offes_fltregs)(sp)
	lfd     fr9 ,( 9*8+offes_fltregs)(sp)
	lfd     fr10,(10*8+offes_fltregs)(sp)
	lfd     fr11,(11*8+offes_fltregs)(sp)
	lfd     fr12,(12*8+offes_fltregs)(sp)
	lfd     fr13,(13*8+offes_fltregs)(sp)
	lfd     fr14,(14*8+offes_fltregs)(sp)
	lfd     fr15,(15*8+offes_fltregs)(sp)
	lfd     fr16,(16*8+offes_fltregs)(sp)
	lfd     fr17,(17*8+offes_fltregs)(sp)
	lfd     fr18,(18*8+offes_fltregs)(sp)
	lfd     fr19,(19*8+offes_fltregs)(sp)
	lfd     fr20,(20*8+offes_fltregs)(sp)
	lfd     fr21,(21*8+offes_fltregs)(sp)
	lfd     fr22,(22*8+offes_fltregs)(sp)
	lfd     fr23,(23*8+offes_fltregs)(sp)
	lfd     fr24,(24*8+offes_fltregs)(sp)
	lfd     fr25,(25*8+offes_fltregs)(sp)
	lfd     fr26,(26*8+offes_fltregs)(sp)
	lfd     fr27,(27*8+offes_fltregs)(sp)
	lfd     fr28,(28*8+offes_fltregs)(sp)
	lfd     fr29,(29*8+offes_fltregs)(sp)
	lfd     fr30,(30*8+offes_fltregs)(sp)
	lfd     fr31,(31*8+offes_fltregs)(sp)

	/* restore link register */

	mtlr    itmp3
	
	/* load new pc */

	lwz     itmp3,offes_pc(sp)

	/* remove stack frame */

	addi	sp,sp,+(sizeexecutionstate_ALIGNED)

	/* jump to new code */

	mtctr   itmp3
	bctr

#endif /* defined(ENABLE_REPLACEMENT) */

/*********************************************************************/

asm_cacheflush:
	add     a1,a0,a1
	rlwinm  a0,a0,0,0,26
	addi    a1,a1,31
	rlwinm  a1,a1,0,0,26
	mr      a2,a0
1:
	cmplw   a0,a1
	bge     0f
	dcbst   0,a0
	addi    a0,a0,32
	b       1b
0:
	sync
1:
	cmplw   a2,a1
	bge     0f
	icbi    0,a2
	addi    a2,a2,32
	b       1b
0:
	sync
	isync
	blr


/* asm_compare_and_swap ********************************************************

   XXX

*******************************************************************************/

asm_compare_and_swap:
1:  lwarx   a6,r0,a0 
	subf.   r0,a6,a1 
	bne-    2f 
	or      r0,a2,a2 
	stwcx.  r0,r0,a0 
	bne-    1b 
2: 
	mr      a0,a6
	blr


/* asm_memory_barrier **********************************************************

   XXX

*******************************************************************************/

asm_memory_barrier:
	sync
	blr


asm_getclassvalues_atomic:
_crit_restart:
_crit_begin:
	lwz     a3,offbaseval(a0)
	lwz     a4,offdiffval(a0)
	lwz     a5,offbaseval(a1)
_crit_end:
	stw     a3,offcast_super_baseval(a2)
	stw     a4,offcast_super_diffval(a2)
	stw     a5,offcast_sub_baseval(a2)
	blr

	.data

asm_criticalsections:
#if defined(ENABLE_THREADS)
	.long   _crit_begin
	.long   _crit_end
	.long   _crit_restart
#endif
	.long 0


#if defined(__DARWIN__)

.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_builtin_throw_exception$stub:
	.indirect_symbol _builtin_throw_exception
	mflr r0
	bcl 20,31,L00$_builtin_throw_exception
L00$_builtin_throw_exception:
	mflr r11
	addis r11,r11,ha16(L_builtin_throw_exception$lazy_ptr - L00$_builtin_throw_exception)
	mtlr r0
	lwzu r12,lo16(L_builtin_throw_exception$lazy_ptr - L00$_builtin_throw_exception)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_builtin_throw_exception$lazy_ptr:
	.indirect_symbol _builtin_throw_exception
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_md_codegen_get_pv_from_pc$stub:
	.indirect_symbol _md_codegen_get_pv_from_pc
	mflr r0
	bcl 20,31,L00$_md_codegen_get_pv_from_pc
L00$_md_codegen_get_pv_from_pc:
	mflr r11
	addis r11,r11,ha16(L_md_codegen_get_pv_from_pc$lazy_ptr - L00$_md_codegen_get_pv_from_pc)
	mtlr r0
	lwzu r12,lo16(L_md_codegen_get_pv_from_pc$lazy_ptr - L00$_md_codegen_get_pv_from_pc)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_md_codegen_get_pv_from_pc$lazy_ptr:
	.indirect_symbol _md_codegen_get_pv_from_pc
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_exceptions_handle_exception$stub:
	.indirect_symbol _exceptions_handle_exception
	mflr r0
	bcl 20,31,L00$_exceptions_handle_exception
L00$_exceptions_handle_exception:
	mflr r11
	addis r11,r11,ha16(L_exceptions_handle_exception$lazy_ptr - L00$_exceptions_handle_exception)
	mtlr r0
	lwzu r12,lo16(L_exceptions_handle_exception$lazy_ptr - L00$_exceptions_handle_exception)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_exceptions_handle_exception$lazy_ptr:
	.indirect_symbol _exceptions_handle_exception
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_stacktrace_create_extern_stackframeinfo$stub:
	.indirect_symbol _stacktrace_create_extern_stackframeinfo
	mflr r0
	bcl 20,31,L00$_stacktrace_create_extern_stackframeinfo
L00$_stacktrace_create_extern_stackframeinfo:
	mflr r11
	addis r11,r11,ha16(L_stacktrace_create_extern_stackframeinfo$lazy_ptr - L00$_stacktrace_create_extern_stackframeinfo)
	mtlr r0
	lwzu r12,lo16(L_stacktrace_create_extern_stackframeinfo$lazy_ptr - L00$_stacktrace_create_extern_stackframeinfo)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_stacktrace_create_extern_stackframeinfo$lazy_ptr:
	.indirect_symbol _stacktrace_create_extern_stackframeinfo
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_jit_asm_compile$stub:
	.indirect_symbol _jit_asm_compile
	mflr r0
	bcl 20,31,L00$_jit_asm_compile
L00$_jit_asm_compile:
	mflr r11
	addis r11,r11,ha16(L_jit_asm_compile$lazy_ptr - L00$_jit_asm_compile)
	mtlr r0
	lwzu r12,lo16(L_jit_asm_compile$lazy_ptr - L00$_jit_asm_compile)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_jit_asm_compile$lazy_ptr:
	.indirect_symbol _jit_asm_compile
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_stacktrace_remove_stackframeinfo$stub:
	.indirect_symbol _stacktrace_remove_stackframeinfo
	mflr r0
	bcl 20,31,L00$_stacktrace_remove_stackframeinfo
L00$_stacktrace_remove_stackframeinfo:
	mflr r11
	addis r11,r11,ha16(L_stacktrace_remove_stackframeinfo$lazy_ptr - L00$_stacktrace_remove_stackframeinfo)
	mtlr r0
	lwzu r12,lo16(L_stacktrace_remove_stackframeinfo$lazy_ptr - L00$_stacktrace_remove_stackframeinfo)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_stacktrace_remove_stackframeinfo$lazy_ptr:
	.indirect_symbol _stacktrace_remove_stackframeinfo
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_exceptions_get_and_clear_exception$stub:
	.indirect_symbol _exceptions_get_and_clear_exception
	mflr r0
	bcl 20,31,L00$_exceptions_get_and_clear_exception
L00$_exceptions_get_and_clear_exception:
	mflr r11
	addis r11,r11,ha16(L_exceptions_get_and_clear_exception$lazy_ptr - L00$_exceptions_get_and_clear_exception)
	mtlr r0
	lwzu r12,lo16(L_exceptions_get_and_clear_exception$lazy_ptr - L00$_exceptions_get_and_clear_exception)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_exceptions_get_and_clear_exception$lazy_ptr:
	.indirect_symbol _exceptions_get_and_clear_exception
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_exceptions_asm_new_abstractmethoderror$stub:
	.indirect_symbol _exceptions_asm_new_abstractmethoderror
	mflr r0
	bcl 20,31,L00$_exceptions_asm_new_abstractmethoderror
L00$_exceptions_asm_new_abstractmethoderror:
	mflr r11
	addis r11,r11,ha16(L_exceptions_asm_new_abstractmethoderror$lazy_ptr - L00$_exceptions_asm_new_abstractmethoderror)
	mtlr r0
	lwzu r12,lo16(L_exceptions_asm_new_abstractmethoderror$lazy_ptr - L00$_exceptions_asm_new_abstractmethoderror)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_exceptions_asm_new_abstractmethoderror$lazy_ptr:
	.indirect_symbol _exceptions_asm_new_abstractmethoderror
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_patcher_wrapper$stub:
	.indirect_symbol _patcher_wrapper
	mflr r0
	bcl 20,31,L00$_patcher_wrapper
L00$_patcher_wrapper:
	mflr r11
	addis r11,r11,ha16(L_patcher_wrapper$lazy_ptr - L00$_patcher_wrapper)
	mtlr r0
	lwzu r12,lo16(L_patcher_wrapper$lazy_ptr - L00$_patcher_wrapper)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_patcher_wrapper$lazy_ptr:
	.indirect_symbol _patcher_wrapper
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_replace_me$stub:
	.indirect_symbol _replace_me
	mflr r0
	bcl 20,31,L00$_replace_me
L00$_replace_me:
	mflr r11
	addis r11,r11,ha16(L_replace_me$lazy_ptr - L00$_replace_me)
	mtlr r0
	lwzu r12,lo16(L_replace_me$lazy_ptr - L00$_replace_me)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_replace_me$lazy_ptr:
	.indirect_symbol _replace_me
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_replace_build_execution_state$stub:
	.indirect_symbol _replace_build_execution_state
	mflr r0
	bcl 20,31,L00$_replace_build_execution_state
L00$_replace_build_execution_state:
	mflr r11
	addis r11,r11,ha16(L_replace_build_execution_state$lazy_ptr - L00$_replace_build_execution_state)
	mtlr r0
	lwzu r12,lo16(L_replace_build_execution_state$lazy_ptr - L00$_replace_build_execution_state)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_replace_build_execution_state$lazy_ptr:
	.indirect_symbol _replace_build_execution_state
	.long dyld_stub_binding_helper


.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
	.align 2
L_replace_free_safestack$stub:
	.indirect_symbol _replace_free_safestack
	mflr r0
	bcl 20,31,L00$_replace_free_safestack
L00$_replace_free_safestack:
	mflr r11
	addis r11,r11,ha16(L_replace_free_safestack$lazy_ptr - L00$_replace_free_safestack)
	mtlr r0
	lwzu r12,lo16(L_replace_free_safestack$lazy_ptr - L00$_replace_free_safestack)(r11)
	mtctr r12
	bctr
.data
.lazy_symbol_pointer
L_replace_free_safestack$lazy_ptr:
	.indirect_symbol _replace_free_safestack
	.long dyld_stub_binding_helper

#endif /* defined(__DARWIN__) */


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

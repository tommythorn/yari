/* src/vm/jit/powerpc64/asmpart.S - Java-C interface functions for PowerPC64
		
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

#define __ASSEMBLY__

#include "md-abi.h"
#include "md-asm.h"

#include "vm/jit/abi-asm.h"
#include "vm/jit/methodheader.h"
#include "vm/jit/powerpc64/offsets.h"

.section ".toc","aw"
.section ".text"

	.align 2	


/* export functions ***********************************************************/

	.globl asm_vm_call_method_exception_handler
	.globl asm_vm_call_method_end

	.globl asm_call_jit_compiler

	.globl asm_handle_nat_exception
	.globl asm_handle_exception

	.globl asm_abstractmethoderror

	.globl asm_patcher_wrapper

#if defined(ENABLE_REPLACEMENT)
	.globl asm_replacement_out
	.globl .asm_replacement_in
#endif

	.globl asm_cacheflush
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
	/* this is the method header see src/vm/jit/methodheader.h */

	.align	8

	.quad   0                           /* catch type all                     */
	.quad   0                           /* handler pc                         */
	.quad   0                           /* end pc                             */
	.quad   0                           /* start pc                           */
	.long   1                           /* extable size                       */
	.long   0                           /* ALIGNMENT PADDING                  */
	.quad   0                           /* line number table  start           */
	.quad   0                           /* line number table  size            */
	.long   0                           /* ALIGNMENT PADDING                  */
	.long   0                           /* fltsave                            */
	.long   0                           /* intsave                            */
	.long   0                           /* isleaf                             */
	.long   0                           /* IsSync                             */
	.long   0                           /* frame size                         */
	.quad   0                           /* codeinfo pointer                   */

#ifdef ENABLE_LIBJVM
	
	.globl asm_vm_call_method
	.globl asm_vm_call_method_int
	.globl asm_vm_call_method_long
	.globl asm_vm_call_method_float
	.globl asm_vm_call_method_double
	.section ".opd","aw"
	.align 3

	asm_vm_call_method:
	asm_vm_call_method_int:
	asm_vm_call_method_long:
	asm_vm_call_method_float:
	asm_vm_call_method_double:
		.quad	.asm_vm_call_method,.TOC.@tocbase,0
		.previous
		.size asm_vm_call_method, 24
		.type .asm_vm_call_method,@function
		.globl .asm_vm_call_method
#else
	asm_vm_call_method:
	.globl asm_vm_call_method
	asm_vm_call_method_int:
	.globl asm_vm_call_method_int
	asm_vm_call_method_long:
	.globl asm_vm_call_method_long
	asm_vm_call_method_float:
	.globl asm_vm_call_method_float
	asm_vm_call_method_double:
	.globl asm_vm_call_method_double
#endif

.asm_vm_call_method:
.asm_vm_call_method_int:
.asm_vm_call_method_long:
.asm_vm_call_method_float:
.asm_vm_call_method_double:
	mflr    r0
	std     r0,LA_LR_OFFSET(sp)
	stdu    sp,-40*8(sp)
	
	std     s0,8*8(sp)                /* save used callee saved registers     */
	std     a0,9*8(sp)                /* save method pointer for compiler     */

#if defined(__DARWIN__)
	std     itmp1,10*8(sp)            /* register r11 is callee saved         */
#endif
	std     pv,11*8(sp)               /* save PV register                     */

	std     itmp3,12*8(sp)            /* registers r14-r31 are callee saved   */
	stfd	ftmp1,13*8(sp)            /* registers f14-f31 are callee saved   */
	stfd	ftmp2,14*8(sp)

#if defined(__DARWIN__)
	std     t1,15*8(r1)
	std     t2,16*8(r1)
	std     t3,17*8(r1)
	std     t4,18*8(r1)
	std     t5,19*8(r1)
	std     t6,20*8(r1)
	std     t7,21*8(r1)

	stfd	ft0,22*8(r1)
	stfd	ft1,23*8(r1)
	stfd	ft2,24*8(r1)
	stfd	ft3,25*8(r1)
	stfd	ft4,26*8(r1)
	stfd	ft5,27*8(r1)
#else
	SAVE_TEMPORARY_REGISTERS(15)      /* the offset has to be even            */
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
	li      t3,8                      /* initialize PA counter*/

L_register_copy:
	addi    itmp1,itmp1,sizevmarg     /* goto next argument block             */
	addi    itmp2,itmp2,-1            /* argument count - 1                   */
	mr.     itmp2,itmp2
	beq     L_register_copy_done

	addi	t3,t3,-1                  /* uses a PA slot                       */
	lwz     itmp3,offvmargtype+4(itmp1)
	andi.   r0,itmp3,0x0002           /* is this a float/double type?         */
	bne     L_register_handle_float
	
L_register_handle_int:
	cmpwi   t6,INT_ARG_CNT            /* are we out of integer argument       */
	beq     L_register_copy           /* registers? yes, next loop            */

	andi.   r0,itmp3,0x0001           /* is this a 2-word type?               */
	bne     L_register_handle_long

#if defined(__DARWIN__)
	#error "FIXME for darwin"
#else
	lis     itmp3,L_jumptable_int@highest		/* load 64bit address	*/
	ori	itmp3,itmp3,L_jumptable_int@higher
	rldicr 	itmp3,itmp3,32,31
	oris	itmp3,itmp3,L_jumptable_int@h
	ori	itmp3,itmp3,L_jumptable_int@l
#endif
	b	L_register_do_copy_longint

L_register_handle_long:

#if defined(__DARWIN__)
	#error "FIXME for darwin"
#else
	lis     itmp3,L_jumptable_long@highest		/* load 64bit address	*/
	ori	itmp3,itmp3,L_jumptable_long@higher
	rldicr 	itmp3,itmp3,32,31
	oris	itmp3,itmp3,L_jumptable_long@h
	ori	itmp3,itmp3,L_jumptable_long@l
#endif

L_register_do_copy_longint:
	slwi    t2,t6,3                   /* multiple of 8-bytes                  */
	add     itmp3,itmp3,t2            /* calculate address of jumptable       */
	ld      itmp3,0(itmp3)            /* load function address                */
	mtctr   itmp3
	addi    t0,t0,1                   /* integer argument counter             */
	addi    t6,t6,1                   /* integer argument register counter    */
	bctr

L_register_handle_float:
	cmpwi   t1,FLT_ARG_CNT            /* are we out of float argument         */
	beq     L_register_copy           /* registers? yes, next loop            */

	andi.   r0,itmp3,0x0001           /* is this a 2-word type?               */
	bne     L_register_handle_double

#if defined(__DARWIN__)
	#error "FIXME for darwin"
#else
	lis     itmp3,L_jumptable_float@highest		/* load 64bit address	*/
	ori	itmp3,itmp3,L_jumptable_float@higher
	rldicr 	itmp3,itmp3,32,31
	oris	itmp3,itmp3,L_jumptable_float@h
	ori	itmp3,itmp3,L_jumptable_float@l
#endif
	b L_register_do_copy_floatdouble

L_register_handle_double:

#if defined(__DARWIN__)
	#error "FIXME for darwin"
#else
	lis     itmp3,L_jumptable_double@highest		/* load 64bit address	*/
	ori	itmp3,itmp3,L_jumptable_double@higher
	rldicr 	itmp3,itmp3,32,31
	oris	itmp3,itmp3,L_jumptable_double@h
	ori	itmp3,itmp3,L_jumptable_double@l
#endif


L_register_do_copy_floatdouble:

	slwi    t2,t1,3                   /* multiple of 8-bytes                  */
	add     itmp3,itmp3,t2            /* calculate address of jumptable       */
	ld      itmp3,0(itmp3)            /* load function address                */
	mtctr   itmp3
	addi    t1,t1,1                   /* float argument counter               */
	mr.     t3,t3                     /* are we still in PA ?                 */
	blt     L_float_not_uses_PA 
	addi    t6,t6,1                   /* if so it uses an interger arg reg    */
L_float_not_uses_PA:
	bctr


L_register_copy_done:
	subi    sp,sp,PA_SIZE             /* PA_SIZE are used by definition       */
	                                  /* calculate remaining arguments        */
	sub     itmp3,t4,t0               /* - integer arguments in registers     */
	sub     itmp3,itmp3,t1            /* - float arguments in registers       */
	mr.     itmp3,itmp3
	beq     L_stack_copy_done

	mr      itmp2,t4                  /* restore argument count               */
	mr      itmp1,t5                  /* restore argument block pointer       */

	slwi    t4,itmp3,3                /* XXX use 8-bytes slots for now        */
	addi    t4,t4,LA_SIZE             /* add size of linkage area             */
	sub     sp,sp,t4

	mr      t6,sp                     /* use t6 as temporary sp               */
	addi    t6,t6,LA_SIZE             /* skip linkage area                    */

	addi    itmp1,itmp1,-sizevmarg    /* initialize pointer (smaller code)    */
	addi    itmp2,itmp2,1             /* initialize argument count            */
	li      t3,8                      /* initialize PA counter                */
	addi	t6,t6,-8                  /* make code simpler                    */
	
L_stack_copy_loop:
	addi    itmp1,itmp1,sizevmarg     /* goto next argument block             */
	addi    itmp2,itmp2,-1            /* argument count - 1                   */
	mr.     itmp2,itmp2
	beq     L_stack_copy_done
	addi    t6,t6,8                   /* increase stack */
L_stack_not_uses_PA:

	lwz     itmp3,offvmargtype+4(itmp1)
	andi.   r0,itmp3,0x0002           /* is this a float/double type?         */
	bne     L_stack_handle_float

L_stack_handle_int:
	addi    t0,t0,-1                  /* arguments assigned to registers      */
	mr.     t0,t0
	bge     L_stack_copy_loop

	andi.   r0,itmp3,0x0001           /* is this a 2-word type?               */
	bne     L_stack_handle_long

	lwa     itmp3,offvmargdata+4(itmp1) /* get integer argument               */
	std     itmp3,0(t6)               /* and store it on the stack            */
	b       L_stack_copy_loop

L_stack_handle_long:
	ld      itmp3,offvmargdata+0(itmp1) /* get long argument                  */
	std     itmp3,0(t6)               /* and store it on the stack            */
	b       L_stack_copy_loop
		
L_stack_handle_float:
	addi    t1,t1,-1                  /* arguments assigned to registers      */
	mr.     t1,t1
	bge     L_stack_copy_loop

	andi.   r0,itmp3,0x0001           /* is this a 2-word type?               */
	bne     L_stack_handle_double

	lfs     ftmp3,offvmargdata(itmp1) /* get float argument                   */
	stfd    ftmp3,0(t6)               /* and store it on the stack            */
	b       L_stack_copy_loop

L_stack_handle_double:
	lfd     ftmp3,offvmargdata(itmp1) /* get double argument                  */
	stfd    ftmp3,0(t6)               /* and store it on the stack            */
	b       L_stack_copy_loop

L_stack_copy_done:
	ld      itmp1,9*8(s0)             /* pass method pointer via tmp1         */

#if defined(__DARWIN__)
	addis   mptr,t3,ha16(L_asm_call_jit_compiler - L_asm_vm_call_method_get_pc)
	la      mptr,lo16(L_asm_call_jit_compiler - L_asm_vm_call_method_get_pc)(mptr)
#else
	lis     mptr,L_asm_call_jit_compiler@highest	/* load 64bit address	*/
	ori	mptr,mptr,L_asm_call_jit_compiler@higher
	rldicr	mptr,mptr,32,31
	oris	mptr,mptr,L_asm_call_jit_compiler@h
	ori	mptr,mptr,L_asm_call_jit_compiler@l
#endif
	std     mptr,7*8(s0)
	addi    mptr,s0,7*8

	ld 	pv,0*8(mptr)
	mtctr   pv
	bctrl
1:
	mflr    itmp1
#if defined(__DARWIN__)
	addi    pv,itmp1,lo16(.asm_vm_call_method - 1b)
#else
	addi    pv,itmp1,(.asm_vm_call_method - 1b)@l
#endif

L_asm_vm_call_method_return:
	mr      sp,s0                     /* restore the function's sp            */

	ld      s0,8*8(sp)                /* restore used callee saved registers  */

#if defined(__DARWIN__)
	lwz     itmp1,10*4(sp)            /* register r11 is callee saved         */
#endif
	ld      pv,11*8(sp)               /* save PV register                     */

	ld      itmp3,12*8(sp)
	lfd     ftmp1,13*8(sp)            /* registers f14-f31 are callee saved   */
	lfd     ftmp2,14*8(sp)

#if defined(__DARWIN__)
	lwz     t1,18*4(r1)
	lwz     t2,19*4(r1)
	lwz     t3,20*4(r1)
	lwz     t4,21*4(r1)
	lwz     t5,22*4(r1)
	lwz     t6,23*4(r1)
	lwz     t7,24*4(r1)

	lfd     ft0,26*4(r1)
	lfd     ft1,28*4(r1)
	lfd     ft2,30*4(r1)
	lfd     ft3,32*4(r1)
	lfd     ft4,34*4(r1)
	lfd     ft5,36*4(r1)
#else
	RESTORE_TEMPORARY_REGISTERS(15)   /* the offset has to be even            */
#endif

	ld     r0,40*8+LA_LR_OFFSET(r1)
	mtlr    r0
	addi    r1,r1,40*8
	blr

asm_vm_call_method_exception_handler:
	mr      r3,itmp1
	bl      builtin_throw_exception
	b       L_asm_vm_call_method_return


	.data
	.align  8

L_jumptable_int:
	.quad   L_handle_a0
	.quad   L_handle_a1
	.quad   L_handle_a2
	.quad   L_handle_a3
	.quad   L_handle_a4
	.quad   L_handle_a5
	.quad   L_handle_a6
	.quad   L_handle_a7

	.text
	.align  4

L_handle_a0:
	lwa     a0,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a1:
	lwa     a1,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a2:
	lwa     a2,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a3:
	lwa     a3,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a4:
	lwa     a4,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a5:
	lwa     a5,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a6:
	lwa     a6,offvmargdata+4(itmp1)
	b       L_register_copy
L_handle_a7:
	lwa     a7,offvmargdata+4(itmp1)
	b       L_register_copy


	.data
	.align  8

L_jumptable_long:
	.quad   L_handle_a0l
	.quad   L_handle_a1l
	.quad   L_handle_a2l
	.quad   L_handle_a3l
	.quad   L_handle_a4l
	.quad   L_handle_a5l
	.quad   L_handle_a6l
	.quad   L_handle_a7l

	.text
	.align  4

L_handle_a0l:
	ld     a0,offvmargdata(itmp1)
	b       L_register_copy
L_handle_a1l:
	ld     a1,offvmargdata(itmp1)
	b       L_register_copy
L_handle_a2l:
	ld     a2,offvmargdata(itmp1)
	b       L_register_copy
L_handle_a3l:
	ld     a3,offvmargdata(itmp1)
	b       L_register_copy
L_handle_a4l:
	ld     a4,offvmargdata(itmp1)
	b       L_register_copy
L_handle_a5l:
	ld     a5,offvmargdata(itmp1)
	b       L_register_copy
L_handle_a6l:
	ld     a6,offvmargdata(itmp1)
	b       L_register_copy
L_handle_a7l:
	ld     a7,offvmargdata(itmp1)
	b       L_register_copy

	.data
	.align  8

L_jumptable_float:
	.quad   L_handle_fa0
	.quad   L_handle_fa1
	.quad   L_handle_fa2
	.quad   L_handle_fa3
	.quad   L_handle_fa4
	.quad   L_handle_fa5
	.quad   L_handle_fa6
	.quad   L_handle_fa7
	.quad   L_handle_fa8
	.quad   L_handle_fa9
	.quad   L_handle_fa10
	.quad   L_handle_fa11
	.quad   L_handle_fa12

	.text
	.align  4

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

	.data
	.align  8

L_jumptable_double:
	.quad   L_handle_fda0
	.quad   L_handle_fda1
	.quad   L_handle_fda2
	.quad   L_handle_fda3
	.quad   L_handle_fda4
	.quad   L_handle_fda5
	.quad   L_handle_fda6
	.quad   L_handle_fda7
	.quad   L_handle_fda8
	.quad   L_handle_fda9
	.quad   L_handle_fda10
	.quad   L_handle_fda11
	.quad   L_handle_fda12

	.text
	.align  4

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

asm_vm_call_method_end:
	nop

/* asm_call_jit_compiler *******************************************************

   Invokes the compiler for untranslated JavaVM methods.

*******************************************************************************/

asm_call_jit_compiler:
L_asm_call_jit_compiler:                /* required for PIC code              */
	mflr    r0
	std     r0,LA_LR_OFFSET(sp)         /* save return address                */
	stdu    r1,-(LA_SIZE+PA_SIZE+ARG_CNT*8)(sp)

#if defined(__DARWIN__)
	stw     a0,LA_SIZE+(5+0)*8(r1)
	stw     a1,LA_SIZE+(5+1)*8(r1)
	stw     a2,LA_SIZE+(5+2)*8(r1)
	stw     a3,LA_SIZE+(5+3)*8(r1)
	stw     a4,LA_SIZE+(5+4)*8(r1)
	stw     a5,LA_SIZE+(5+5)*8(r1)
	stw     a6,LA_SIZE+(5+6)*8(r1)
	stw     a7,LA_SIZE+(5+7)*8(r1)

	stfd    fa0,LA_SIZE+(5+8)*8(r1)
	stfd    fa1,LA_SIZE+(5+10)*8(r1)
	stfd    fa2,LA_SIZE+(5+12)*8(r1)
	stfd    fa3,LA_SIZE+(5+14)*8(r1)
	stfd    fa4,LA_SIZE+(5+16)*8(r1)
	stfd    fa5,LA_SIZE+(5+18)*8(r1)
	stfd    fa6,LA_SIZE+(5+20)*8(r1)
	stfd    fa7,LA_SIZE+(5+22)*8(r1)
	stfd    fa8,LA_SIZE+(5+24)*8(r1)
	stfd    fa9,LA_SIZE+(5+26)*8(r1)
	stfd    fa10,LA_SIZE+(5+28)*8(r1)
	stfd    fa11,LA_SIZE+(5+30)*8(r1)
	stfd    fa12,LA_SIZE+(5+32)*8(r1)
#else
	SAVE_ARGUMENT_REGISTERS(LA_SIZE_IN_POINTERS+PA_SIZE_IN_POINTERS)
#endif

	mr      a0,itmp1
	mr      a1,mptr
	addi    a2,sp,(LA_SIZE + PA_SIZE+ ARG_CNT*8)
	ld      a3,(LA_SIZE + PA_SIZE + ARG_CNT*8)+LA_LR_OFFSET(sp)
	bl      jit_asm_compile
	ori	r0,r0,0			    /* nop needed after jump to function desc. */
	mr      pv,v0                       /* move address to pv register        */

#if defined(__DARWIN__)
	lwz     a0,LA_SIZE+(+5+0)*8(r1)
	lwz     a1,LA_SIZE+(+5+1)*8(r1)
	lwz     a2,LA_SIZE+(+5+2)*8(r1)
	lwz     a3,LA_SIZE+(+5+3)*8(r1)
	lwz     a4,LA_SIZE+(+5+4)*8(r1)
	lwz     a5,LA_SIZE+(+5+5)*8(r1)
	lwz     a6,LA_SIZE+(+5+6)*8(r1)
	lwz     a7,LA_SIZE+(+5+7)*8(r1)

	lfd     fa0,LA_SIZE+(+5+8)*8(r1)
	lfd     fa1,LA_SIZE+(+5+10)*8(r1)
	lfd     fa2,LA_SIZE+(+5+12)*8(r1)
	lfd     fa3,LA_SIZE+(+5+14)*8(r1)
	lfd     fa4,LA_SIZE+(+5+16)*8(r1)
	lfd     fa5,LA_SIZE+(+5+18)*8(r1)
	lfd     fa6,LA_SIZE+(+5+20)*8(r1)
	lfd     fa7,LA_SIZE+(+5+22)*8(r1)
	lfd     fa8,LA_SIZE+(+5+24)*8(r1)
	lfd     fa9,LA_SIZE+(+5+26)*8(r1)
	lfd     fa10,LA_SIZE+(+5+28)*8(r1)
	lfd     fa11,LA_SIZE+(+5+30)*8(r1)
	lfd     fa12,LA_SIZE+(+5+32)*8(r1)
#else
	RESTORE_ARGUMENT_REGISTERS(LA_SIZE_IN_POINTERS+PA_SIZE_IN_POINTERS)
#endif

	ld     itmp1,(LA_SIZE + PA_SIZE + ARG_CNT*8)+LA_LR_OFFSET(sp)
	mtlr   itmp1

	addi    sp,sp,(LA_SIZE + PA_SIZE + ARG_CNT*8)

	mr.     pv,pv                       /* test for exception                 */
	beq     L_asm_call_jit_compiler_exception

	mtctr   pv                          /* move method address to control reg */
	bctr                                /* and call the Java method           */

L_asm_call_jit_compiler_exception:
	mflr	r0
	std     r0,LA_LR_OFFSET(sp)
	stdu    sp,-LA_SIZE_ALIGNED(sp)     /* preserve linkage area              */
	bl      exceptions_get_and_clear_exception
	ld      xpc,LA_SIZE_ALIGNED+LA_LR_OFFSET(sp)
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
	addi    sp,sp,-(LA_SIZE+PA_SIZE+((4+6)*8))  /* allocate stack (+4 for darwin)     */
	std     xptr,LA_SIZE+PA_SIZE+(4+0)*8(sp)    /* save exception pointer             */
	std     xpc,LA_SIZE+PA_SIZE+(4+1)*8(sp)     /* save exception pc                  */
	std     r0,LA_SIZE+PA_SIZE+(4+3)*8(sp)      /* save return address                */
	li      itmp3,0
	std     itmp3,LA_SIZE+PA_SIZE+(4+4)*8(sp)   /* save maybe-leaf flag (cleared)     */

	mr      a0,r0                       /* pass return address                */
	bl      md_codegen_get_pv_from_pc   /* get PV from RA                     */
	std     v0,LA_SIZE+PA_SIZE+(4+2)*8(sp)      /* save data segment pointer          */

	ld      a0,LA_SIZE+PA_SIZE+(4+0)*8(sp)      /* pass xptr                          */
	ld      a1,LA_SIZE+PA_SIZE+(4+1)*8(sp)      /* pass xpc                           */
	ld      a2,LA_SIZE+PA_SIZE+(4+2)*8(sp)      /* pass PV (v0 == a0)                 */
	addi    a3,sp,LA_SIZE+PA_SIZE+((4+6)*8)     /* pass Java SP                       */

	b       L_asm_handle_exception_continue


asm_handle_exception:
L_asm_handle_exception:                 /* required for PIC code              */
	addi    sp,sp,-(ARG_CNT+TMP_CNT)*8  /* create maybe-leaf stackframe       */

#if defined(__DARWIN__)
#else
	SAVE_ARGUMENT_REGISTERS(0)          /* we save arg and temp registers in  */
	SAVE_TEMPORARY_REGISTERS(ARG_CNT)   /* case this is a leaf method         */
#endif

	addi    sp,sp,-(LA_SIZE+PA_SIZE+(4+6)*8)        /* allocate stack                     */
	std     xptr,LA_SIZE+PA_SIZE+(4+0)*8(sp)        /* save exception pointer             */
	std     pv,LA_SIZE+PA_SIZE+(4+2)*8(sp)          /* save data segment pointer          */
	mflr    r0           		                /* save return address                */
	std     r0,LA_SIZE+PA_SIZE+(4+3)*8(sp)		
	li      t0, 1
	std     t0, LA_SIZE+PA_SIZE+(4+4)*8(sp)         /* maybe-leaf flag */
	
	mr      a0,xptr                     /* pass exception pointer             */
	mr      a1,xpc                      /* pass exception pc                  */
	mr      a2,pv                       /* pass data segment pointer          */
	addi    a3,sp,LA_SIZE+PA_SIZE+(ARG_CNT+TMP_CNT)*8+(4+6)*8


L_asm_handle_exception_continue:
	bl      exceptions_handle_exception

	mr.     v0,v0
	beq     L_asm_handle_exception_not_catched

	mr      xpc,v0                              /* move handlerpc into xpc            */
	ld      xptr,LA_SIZE+PA_SIZE+(4+0)*8(sp)    /* restore exception pointer          */
	ld      pv,LA_SIZE+PA_SIZE+(4+2)*8(sp)      /* restore data segment pointer       */
	ld      r0,LA_SIZE+PA_SIZE+(4+3)*8(sp)      /* restore return address             */
	mtlr    r0
	ld      t0,LA_SIZE+PA_SIZE+(4+4)*8(sp)      /* get maybe-leaf flag                */
	addi    sp,sp,LA_SIZE+PA_SIZE+(4+6)*8       /* free stack frame                   */

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
	ld      xptr,LA_SIZE+PA_SIZE+(4+0)*8(sp)        /* restore exception pointer          */
	ld      pv,LA_SIZE+PA_SIZE+(4+2)*8(sp)          /* restore data segment pointer       */
	ld      r0,LA_SIZE+PA_SIZE+(4+3)*8(sp)          /* restore return address             */
	mtlr    r0
	ld      t0,LA_SIZE+PA_SIZE+(4+4)*8(sp)          /* get maybe-leaf flag                */
	addi    sp,sp,LA_SIZE+PA_SIZE+(4+6)*8           /* free stack frame                   */

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

	ld      r0,LA_LR_OFFSET(t1)         /* restore ra                         */
	mtlr    r0

L_asm_handle_exception_no_ra_restore:
	mflr    xpc                         /* the new xpc is ra                  */
	mr      t4,xpc                      /* save RA */
	lwz     t2,IntSave(pv)              /* t1 = saved int register count      */
	bl      ex_int1
ex_int1:
	mflr    t3                          /* t3 = current pc                    */
#if defined(__DARWIN__)
	addi    t2,t2,lo16(ex_int2-ex_int1)
#else
	addi    t3,t3,(ex_int2-ex_int1)@l
#endif
	slwi    t2,t2,2                     /* t2 = register count * 4            */
	subf    t3,t2,t3                    /* t3 = IntSave - t2                  */
	mtctr   t3
	bctr

	ld      s0,-9*8(t1)
	ld      s1,-8*8(t1)
	ld      s2,-7*8(t1)
	ld      s3,-6*8(t1)
	ld      s4,-5*8(t1)
	ld      s5,-4*8(t1)
	ld      s6,-3*8(t1)
	ld      s7,-2*8(t1)
	ld      s8,-1*8(t1)

ex_int2:
	subf    t1,t2,t1                    /* t1 = t1 - register count * 4       */
	lwz     t2,FltSave(pv)
	bl      ex_flt1
ex_flt1:
	mflr    t3
#if defined(__DARWIN__)
	addi    t2,t2,lo16(ex_flt2-ex_flt1)
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
	mtlr    t4                                   /* restore RA */
	lwz     t1,FrameSize(pv)                     
	add     sp,sp,t1                             /* unwind stack */
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
     40   return address into JIT code (patch position)
     32   pointer to virtual java_objectheader
     24   machine code (which is patched back later)
     16   unresolved class/method/field reference
      8   data segment displacement from load instructions
      0   patcher function pointer to call (pv is saved here afterwards)

*******************************************************************************/

asm_patcher_wrapper:
	mflr	r0                    /* get Java return address (leaf)           */
	std     r0,6*8(sp)            /* store it in the stub stackframe          */
	                              /* keep stack 16-bytes aligned: 6+1+37 = 44 */
	stdu    sp,-(LA_SIZE+PA_SIZE+ARG_CNT*8+TMP_CNT*8+4*8)(sp)

#if defined(__DARWIN__)
	stw     a0,LA_SIZE+(5+0)*8(r1)      /* save argument registers            */
	stw     a1,LA_SIZE+(5+1)*8(r1)      /* preserve linkage area (24 bytes)   */
	stw     a2,LA_SIZE+(5+2)*8(r1)      /* and 4 bytes for 4 argument         */
	stw     a3,LA_SIZE+(5+3)*8(r1)
	stw     a4,LA_SIZE+(5+4)*8(r1)
	stw     a5,LA_SIZE+(5+5)*8(r1)
	stw     a6,LA_SIZE+(5+6)*8(r1)
	stw     a7,LA_SIZE+(5+7)*8(r1)

	stfd    fa0,LA_SIZE+(5+8)*8(sp)
	stfd    fa1,LA_SIZE+(5+10)*8(sp)
	stfd    fa2,LA_SIZE+(5+12)*8(sp)
	stfd    fa3,LA_SIZE+(5+14)*8(sp)
	stfd    fa4,LA_SIZE+(5+16)*8(sp)
	stfd    fa5,LA_SIZE+(5+18)*8(sp)
	stfd    fa6,LA_SIZE+(5+20)*8(sp)
	stfd    fa7,LA_SIZE+(5+22)*8(sp)
	stfd    fa8,LA_SIZE+(5+24)*8(sp)
	stfd    fa9,LA_SIZE+(5+26)*8(sp)
	stfd    fa10,LA_SIZE+(5+28)*8(sp)
	stfd    fa11,LA_SIZE+(5+30)*8(sp)
	stfd    fa12,LA_SIZE+(5+32)*8(sp)	/* XXX */

	stw     t0,LA_SIZE+(+5+33)*8(r1)
	stw     t1,LA_SIZE+(+5+34)*8(r1)
	stw     t2,LA_SIZE+(+5+35)*8(r1)
	stw     t3,LA_SIZE+(+5+36)*8(r1)
	stw     t4,LA_SIZE+(+5+37)*8(r1)
	stw     t5,LA_SIZE+(+5+38)*8(r1)
	stw     t6,LA_SIZE+(+5+39)*8(r1)
	stw     t7,LA_SIZE+(+5+40)*8(r1)

	stfd    ft0,LA_SIZE+(+5+42)*8(r1)
	stfd    ft1,LA_SIZE+(+5+44)*8(r1)
	stfd    ft2,LA_SIZE+(+5+46)*8(r1)
	stfd    ft3,LA_SIZE+(+5+48)*8(r1)
	stfd    ft4,LA_SIZE+(+5+50)*8(r1)
	stfd    ft5,LA_SIZE+(+5+52)*8(r1)
#else
	SAVE_ARGUMENT_REGISTERS(LA_SIZE_IN_POINTERS+PA_SIZE_IN_POINTERS) /* save 8 int/8 float arguments   */
	SAVE_TEMPORARY_REGISTERS(LA_SIZE_IN_POINTERS+PA_SIZE_IN_POINTERS+ARG_CNT)
#endif

	std     itmp1,LA_SIZE+PA_SIZE+(ARG_CNT+TMP_CNT)*8+1*8(sp)
	std     itmp2,LA_SIZE+PA_SIZE+(ARG_CNT+TMP_CNT)*8+2*8(sp)
	std     pv,LA_SIZE+PA_SIZE+(ARG_CNT+TMP_CNT)*8+3*8(sp)

	addi    a0,sp,LA_SIZE+PA_SIZE+ARG_CNT*8+TMP_CNT*8+4*8      /* pass SP of patcher stub            */
	mr      a1,pv                       /* pass PV                            */
	mr      a2,r0                       /* pass RA (correct for leafs)        */
	bl      patcher_wrapper
	std     v0,LA_SIZE+PA_SIZE+(ARG_CNT+TMP_CNT)*8+4*8(sp)     /* save return value                  */

#if defined(__DARWIN__)
	lwz     a0,LA_SIZE+(5+0)*8(r1)
	lwz     a1,LA_SIZE+(5+1)*8(r1)
	lwz     a2,LA_SIZE+(5+2)*8(r1)
	lwz     a3,LA_SIZE+(5+3)*8(r1)
	lwz     a4,LA_SIZE+(5+4)*8(r1)
	lwz     a5,LA_SIZE+(5+5)*8(r1)
	lwz     a6,LA_SIZE+(5+6)*8(r1)
	lwz     a7,LA_SIZE+(5+7)*8(r1)

	lfd     fa0,LA_SIZE+(5+8)*8(sp)
	lfd     fa1,LA_SIZE+(5+10)*8(sp)
	lfd     fa2,LA_SIZE+(5+12)*8(sp)
	lfd     fa3,LA_SIZE+(5+14)*8(sp)
	lfd     fa4,LA_SIZE+(5+16)*8(sp)
	lfd     fa5,LA_SIZE+(5+18)*8(sp)
	lfd     fa6,LA_SIZE+(5+20)*8(sp)
	lfd     fa7,LA_SIZE+(5+22)*8(sp)
	lfd     fa8,LA_SIZE+(5+24)*8(sp)
	lfd     fa9,LA_SIZE+(5+26)*8(sp)
	lfd     fa10,LA_SIZE+(5+28)*8(sp)
	lfd     fa11,LA_SIZE+(5+30)*8(sp)
	lfd     fa12,LA_SIZE+(5+32)*8(sp)

	lwz     t0,LA_SIZE+(+5+33)*8(r1)
	lwz     t1,LA_SIZE+(+5+34)*8(r1)
	lwz     t2,LA_SIZE+(+5+35)*8(r1)
	lwz     t3,LA_SIZE+(+5+36)*8(r1)
	lwz     t4,LA_SIZE+(+5+37)*8(r1)
	lwz     t5,LA_SIZE+(+5+38)*8(r1)
	lwz     t6,LA_SIZE+(+5+39)*8(r1)
	lwz     t7,LA_SIZE+(+5+40)*8(r1)

	lfd     ft0,LA_SIZE+(+5+42)*8(r1)
	lfd     ft1,LA_SIZE+(+5+44)*8(r1)
	lfd     ft2,LA_SIZE+(+5+46)*8(r1)
	lfd     ft3,LA_SIZE+(+5+48)*8(r1)
	lfd     ft4,LA_SIZE+(+5+50)*8(r1)
	lfd     ft5,LA_SIZE+(+5+52)*8(r1)
#else
	RESTORE_ARGUMENT_REGISTERS(LA_SIZE_IN_POINTERS+PA_SIZE_IN_POINTERS) /* restore 8 int/8 float args  */
	RESTORE_TEMPORARY_REGISTERS(LA_SIZE_IN_POINTERS+PA_SIZE_IN_POINTERS+ARG_CNT)
#endif

	ld     itmp1,LA_SIZE+PA_SIZE+(ARG_CNT+TMP_CNT)*8+1*8(sp)
	ld     itmp2,LA_SIZE+PA_SIZE+(ARG_CNT+TMP_CNT)*8+2*8(sp)
	ld     pv,LA_SIZE+PA_SIZE+(ARG_CNT+TMP_CNT)*8+3*8(sp)
	ld     itmp3,LA_SIZE+PA_SIZE+(ARG_CNT+TMP_CNT)*8+4*8(sp)

	ld    	r0,LA_SIZE+PA_SIZE+ARG_CNT*8+TMP_CNT*8+4*8+6*8(sp) /* restore RA                      */
	mtlr	r0

	mr.     itmp3,itmp3           /* check for an exception                   */
	bne     L_asm_patcher_wrapper_exception

	                              /* get return address (into JIT code)       */
	ld     itmp3,LA_SIZE+PA_SIZE+ARG_CNT*8+TMP_CNT*8+4*8+5*8(sp)

	                              /* remove stack frame + patcher stub stack  */
	addi    sp,sp,LA_SIZE+PA_SIZE+ARG_CNT*8+TMP_CNT*8+4*8+8*8

	mtctr   itmp3
	bctr                          /* jump to new patched code                 */

L_asm_patcher_wrapper_exception:
	mr      xptr,itmp3                  /* get exception                      */
	ld      xpc,LA_SIZE+PA_SIZE+ARG_CNT*8+TMP_CNT*8+4*8+5*8(sp)
	addi    sp,sp,LA_SIZE+PA_SIZE+ARG_CNT*8+TMP_CNT*8+4*8+8*8
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

asm_replacement_out:
    /* create stack frame */
	addi    sp,sp,-(sizeexecutionstate + REPLACEMENT_ROOM) /* XXX align */

	/* save link register */
	mflr    r16

	/* save registers in execution state */
	stw     r0 ,( 0*8+offes_intregs)(sp)
	stw     r1 ,( 1*8+offes_intregs)(sp)
	stw     r2 ,( 2*8+offes_intregs)(sp)
	stw     r3 ,( 3*8+offes_intregs)(sp)
	stw     r4 ,( 4*8+offes_intregs)(sp)
	stw     r5 ,( 5*8+offes_intregs)(sp)
	stw     r6 ,( 6*8+offes_intregs)(sp)
	stw     r7 ,( 7*8+offes_intregs)(sp)
	stw     r8 ,( 8*8+offes_intregs)(sp)
	stw     r9 ,( 9*8+offes_intregs)(sp)
	stw     r10,(10*8+offes_intregs)(sp)
	stw     r11,(11*8+offes_intregs)(sp)
	stw     r12,(12*8+offes_intregs)(sp)
	stw     r13,(13*8+offes_intregs)(sp)
	stw     r14,(14*8+offes_intregs)(sp)
	stw     r15,(15*8+offes_intregs)(sp)
	stw     r16,(16*8+offes_intregs)(sp) /* link register */
	stw     r17,(17*8+offes_intregs)(sp)
	stw     r18,(18*8+offes_intregs)(sp)
	stw     r19,(19*8+offes_intregs)(sp)
	stw     r20,(20*8+offes_intregs)(sp)
	stw     r21,(21*8+offes_intregs)(sp)
	stw     r22,(22*8+offes_intregs)(sp)
	stw     r23,(23*8+offes_intregs)(sp)
	stw     r24,(24*8+offes_intregs)(sp)
	stw     r25,(25*8+offes_intregs)(sp)
	stw     r26,(26*8+offes_intregs)(sp)
	stw     r27,(27*8+offes_intregs)(sp)
	stw     r28,(28*8+offes_intregs)(sp)
	stw     r29,(29*8+offes_intregs)(sp)
	stw     r30,(30*8+offes_intregs)(sp)
	stw     r31,(31*8+offes_intregs)(sp)
	
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
	addi    itmp1,sp,(sizeexecutionstate + REPLACEMENT_ROOM + 4*4)
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
      void asm_replacement_in(executionstate *es);

*******************************************************************************/

.asm_replacement_in:
	/* a0 == executionstate *es */

	/* set new sp and pv */
	lwz     sp,(offes_sp)(a0)
	lwz     pv,(offes_pv)(a0)
	
	/* copy registers from execution state */
	lwz     r0 ,( 0*8+offes_intregs)(a0)
	/* r1 is sp                       */
	/* r2 is reserved                 */
	/* a0 is loaded below             */
	lwz     r4 ,( 4*8+offes_intregs)(a0)
	lwz     r5 ,( 5*8+offes_intregs)(a0)
	lwz     r6 ,( 6*8+offes_intregs)(a0)
	lwz     r7 ,( 7*8+offes_intregs)(a0)
	lwz     r8 ,( 8*8+offes_intregs)(a0)
	lwz     r9 ,( 9*8+offes_intregs)(a0)
	lwz     r10,(10*8+offes_intregs)(a0)
	lwz     r11,(11*8+offes_intregs)(a0)
	lwz     r12,(12*8+offes_intregs)(a0)
	/* r13 is pv                      */
	lwz     r14,(14*8+offes_intregs)(a0)
	lwz     r15,(15*8+offes_intregs)(a0)
	lwz     r16,(16*8+offes_intregs)(a0) /* link register */
	lwz     r17,(17*8+offes_intregs)(a0)
	lwz     r18,(18*8+offes_intregs)(a0)
	lwz     r19,(19*8+offes_intregs)(a0)
	lwz     r20,(20*8+offes_intregs)(a0)
	lwz     r21,(21*8+offes_intregs)(a0)
	lwz     r22,(22*8+offes_intregs)(a0)
	lwz     r23,(23*8+offes_intregs)(a0)
	lwz     r24,(24*8+offes_intregs)(a0)
	lwz     r25,(25*8+offes_intregs)(a0)
	lwz     r26,(26*8+offes_intregs)(a0)
	lwz     r27,(27*8+offes_intregs)(a0)
	lwz     r28,(28*8+offes_intregs)(a0)
	lwz     r29,(29*8+offes_intregs)(a0)
	lwz     r30,(30*8+offes_intregs)(a0)
	lwz     r31,(31*8+offes_intregs)(a0)
	
	lfd     fr0 ,( 0*8+offes_fltregs)(a0)
	lfd     fr1 ,( 1*8+offes_fltregs)(a0)
	lfd     fr2 ,( 2*8+offes_fltregs)(a0)
	lfd     fr3 ,( 3*8+offes_fltregs)(a0)
	lfd     fr4 ,( 4*8+offes_fltregs)(a0)
	lfd     fr5 ,( 5*8+offes_fltregs)(a0)
	lfd     fr6 ,( 6*8+offes_fltregs)(a0)
	lfd     fr7 ,( 7*8+offes_fltregs)(a0)
	lfd     fr8 ,( 8*8+offes_fltregs)(a0)
	lfd     fr9 ,( 9*8+offes_fltregs)(a0)
	lfd     fr10,(10*8+offes_fltregs)(a0)
	lfd     fr11,(11*8+offes_fltregs)(a0)
	lfd     fr12,(12*8+offes_fltregs)(a0)
	lfd     fr13,(13*8+offes_fltregs)(a0)
	lfd     fr14,(14*8+offes_fltregs)(a0)
	lfd     fr15,(15*8+offes_fltregs)(a0)
	lfd     fr16,(16*8+offes_fltregs)(a0)
	lfd     fr17,(17*8+offes_fltregs)(a0)
	lfd     fr18,(18*8+offes_fltregs)(a0)
	lfd     fr19,(19*8+offes_fltregs)(a0)
	lfd     fr20,(20*8+offes_fltregs)(a0)
	lfd     fr21,(21*8+offes_fltregs)(a0)
	lfd     fr22,(22*8+offes_fltregs)(a0)
	lfd     fr23,(23*8+offes_fltregs)(a0)
	lfd     fr24,(24*8+offes_fltregs)(a0)
	lfd     fr25,(25*8+offes_fltregs)(a0)
	lfd     fr26,(26*8+offes_fltregs)(a0)
	lfd     fr27,(27*8+offes_fltregs)(a0)
	lfd     fr28,(28*8+offes_fltregs)(a0)
	lfd     fr29,(29*8+offes_fltregs)(a0)
	lfd     fr30,(30*8+offes_fltregs)(a0)
	lfd     fr31,(31*8+offes_fltregs)(a0)

	/* restore link register */

	mtlr    r16
	
	/* load new pc */

	lwz     itmp3,offes_pc(a0)

	/* load a0 */
	
	lwz     a0,(3*8+offes_intregs)(a0)

	/* jump to new code */

	mtctr   itmp3
	bctr

#endif /* defined(ENABLE_REPLACEMENT) */

/* asm_cacheflush **************************************************************
	copied from linux/arch/ppc64/kernel/vdso64/cacheflush.S
	assumes 128 byte cache line size.
	All registers used may be trashed for fun and profit.
*******************************************************************************/

	.section ".opd","aw"
	.align 3
asm_cacheflush:
		.quad	.asm_cacheflush,.TOC.@tocbase,0
		.previous
		.size asm_cacheflush, 24
		.type .asm_cacheflush,@function
		.globl .asm_cacheflush 
.asm_cacheflush:
	/* construct the AND mask */
	li	r6,   0xffffffffffff8000
	ori	r6,r6,0x000000000000ff80

	add     r4,r3,r4
	and.	r3,r3,r6
	addi    r4,r4,127
	and.	r4,r4,r6
	mr      r5,r3
1:
	cmpld   r3,r4
	bge     0f
	dcbst   0,r3
	addi    r3,r3,128
	b       1b
0:
	sync
1:
	cmpld   r5,r4
	bge     0f
	icbi    0,r5
	addi    r5,r5,128
	b       1b
0:
	sync
	isync
	blr

/*
		asm_getclassvalues_atomic 
*/
	.section ".opd","aw"
	.align 3
asm_getclassvalues_atomic:
		.quad	.asm_getclassvalues_atomic,.TOC.@tocbase,0
		.previous
		.size asm_getclassvalues_atomic, 24
		.type .asm_getclassvalues_atomic,@function
		.globl .asm_getclassvalues_atomic
.asm_getclassvalues_atomic:

_crit_restart:
_crit_begin:
	lwz     r6,offbaseval(r3)
	lwz     r7,offdiffval(r3)
	lwz     r8,offbaseval(r4)
_crit_end:
	stw     r6,offcast_super_baseval(r5)
	stw     r7,offcast_super_diffval(r5)
	stw     r8,offcast_sub_baseval(r5)
	blr

	.data

asm_criticalsections:
#if defined(ENABLE_THREADS)
	.quad   _crit_begin
	.quad   _crit_end
	.quad   _crit_restart
#endif
	.quad 0


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

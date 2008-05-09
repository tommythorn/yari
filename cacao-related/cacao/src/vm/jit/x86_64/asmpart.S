/* src/vm/jit/x86_64/asmpart.S - Java-C interface functions for x86_64

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

   $Id: asmpart.S 7678 2007-04-09 17:23:55Z twisti $

*/


#include "config.h"

#include "vm/jit/x86_64/arch.h"
#include "vm/jit/x86_64/md-abi.h"
#include "vm/jit/x86_64/md-asm.h"
#include "vm/jit/x86_64/offsets.h"

#include "vm/jit/abi-asm.h"
#include "vm/jit/methodheader.h"


	.text


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

	.globl asm_builtin_f2i
	.globl asm_builtin_f2l
	.globl asm_builtin_d2i
	.globl asm_builtin_d2l

	.globl asm_compare_and_swap
	.globl asm_memory_barrier

	.globl asm_criticalsections
	.globl asm_getclassvalues_atomic


/********************* function asm_calljavafunction ***************************
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

asm_vm_call_method:
asm_vm_call_method_int:
asm_vm_call_method_long:
asm_vm_call_method_float:
asm_vm_call_method_double:
	sub     $(7*8),sp                   /* keep stack 16-byte aligned         */
	mov     %rbx,0*8(sp)                /* %rbx is not a callee saved in cacao*/
	mov     s0,1*8(sp)
	mov     s1,2*8(sp)
	mov     s2,3*8(sp)
	mov     s3,4*8(sp)
	mov     s4,5*8(sp)

	mov     a0,itmp1                    /* move method pointer for compiler   */
	xor     %rbp,%rbp                   /* set argument stack frame to zero   */

	test    a1,a1                       /* maybe we have no args...           */
	jle     L_copy_done

	mov     a1,itmp3                    /* arg count                          */
	mov     a2,itmp2                    /* pointer to arg block               */

	mov     itmp2,%r14                  /* save argument block pointer        */
	mov     itmp3,%r15                  /* save argument count                */

	sub     $sizevmarg,itmp2            /* initialize pointer (smaller code)  */
	add     $1,itmp3                    /* initialize argument count          */
	xor     %r12,%r12                   /* initialize integer argument counter*/
	xor     %r13,%r13                   /* initialize float argument counter  */

L_register_copy:
	add     $sizevmarg,itmp2            /* goto next argument block           */
	dec     itmp3                       /* argument count - 1                 */
	jz      L_register_copy_done
	andb    $0x02,offvmargtype(itmp2)   /* is this a float/double type?       */
	jnz     L_register_handle_float     /* yes, handle it                     */

	cmp     $INT_ARG_CNT,%r12           /* are we out of integer argument     */
	je      L_register_copy             /* register? yes, next loop           */

	lea     jumptable_integer(%rip),%rbp
	mov     0(%rbp,%r12,8),%rbx
	inc     %r12                      /* integer argument counter + 1         */
	jmp     *%rbx

L_register_handle_float:
	cmp     $FLT_ARG_CNT,%r13         /* are we out of float argument         */
	je      L_register_copy           /* register? yes, next loop             */

	lea     jumptable_float(%rip),%rbp
	mov     0(%rbp,%r13,8),%rbx
	inc     %r13                      /* float argument counter + 1           */
	jmp     *%rbx
	
L_register_copy_done:
	mov     %r15,%rbp                 /* calculate remaining arguments        */
	sub     %r12,%rbp                 /* - integer arguments in registers     */
	sub     %r13,%rbp                 /* - float arguments in registers       */
	jle     L_copy_done               /* are all assigned to registers?       */

	and     $0xfffffffffffffffe,%rbp  /* keep stack 16-byte aligned           */
	shl     $3,%rbp                   /* calculate stack size                 */
	sub     %rbp,sp                   /* stack frame for arguments            */
	mov     sp,%rbx                   /* use %rbx as temp sp                  */

	sub     $sizevmarg,%r14           /* initialize pointer (smaller code)    */
	add     $1,%r15                   /* initialize argument count            */
		
L_stack_copy_loop:
	add     $sizevmarg,%r14           /* goto next argument block             */
	dec     %r15                      /* are there any arguments left?        */
	jz      L_copy_done               /* no test needed after dec             */

	andb    $0x02,offvmargtype(%r14)    /* is this a float/double type?       */
	jnz     L_stack_handle_float
	dec     %r12                        /* arguments assigned to registers    */
	jge     L_stack_copy_loop
	jmp     L_stack_copy

L_stack_handle_float:
	dec     %r13                        /* arguments assigned to registers    */
	jge     L_stack_copy_loop

L_stack_copy:
	mov     offvmargdata(%r14),itmp3    /* copy s8 argument onto stack        */
	mov     itmp3,0(%rbx)
	add     $8,%rbx                     /* increase sp to next argument       */
	jmp     L_stack_copy_loop

L_copy_done:
	                                    /* itmp1 still contains method pointer*/
	lea     L_asm_call_jit_compiler(%rip),mptr
	mov     sp,itmp3                    /* calculate the old stack pointer    */
	add     bp,itmp3
	mov     mptr,6*8(itmp3)
	lea     (6*8-256)(itmp3),mptr       /* We subtract 256 to force the next  */
	                                    /* move instruction to have a 32-bit  */
	                                    /* offset.                            */

	mov     (0*8+256)(mptr),itmp3       /* method call as in Java             */
	call    *itmp3                      /* call JIT compiler                  */

	add     bp,sp                       /* remove argument stack frame if any */

L_asm_vm_call_method_return:
	mov     0*8(sp),%rbx                /* restore callee saved registers     */
	mov     1*8(sp),s0
	mov     2*8(sp),s1
	mov     3*8(sp),s2
	mov     4*8(sp),s3
	mov     5*8(sp),s4
	add     $(7*8),sp                   /* free stack space                   */
	ret
		
asm_vm_call_method_exception_handler:
	mov     xptr,a0                     /* pass exception pointer             */
	call    builtin_throw_exception@PLT
	jmp     L_asm_vm_call_method_return


jumptable_integer:
	.quad   handle_a0
	.quad   handle_a1
	.quad   handle_a2
	.quad   handle_a3
	.quad   handle_a4
	.quad   handle_a5

handle_a0:
	mov     offvmargdata(itmp2),a0
	jmp     L_register_copy
handle_a1:
	mov     offvmargdata(itmp2),a1
	jmp     L_register_copy
handle_a2:
	mov     offvmargdata(itmp2),a2
	jmp     L_register_copy
handle_a3:
	mov     offvmargdata(itmp2),a3
	jmp     L_register_copy
handle_a4:
	mov     offvmargdata(itmp2),a4
	jmp     L_register_copy
handle_a5:
	mov     offvmargdata(itmp2),a5
	jmp     L_register_copy


jumptable_float:
	.quad   handle_fa0
	.quad   handle_fa1
	.quad   handle_fa2
	.quad   handle_fa3
	.quad   handle_fa4
	.quad   handle_fa5
	.quad   handle_fa6
	.quad   handle_fa7

handle_fa0:
	movq    offvmargdata(itmp2),fa0
	jmp     L_register_copy
handle_fa1:
	movq    offvmargdata(itmp2),fa1
	jmp     L_register_copy
handle_fa2:
	movq    offvmargdata(itmp2),fa2
	jmp     L_register_copy
handle_fa3:
	movq    offvmargdata(itmp2),fa3
	jmp     L_register_copy
handle_fa4:
	movq    offvmargdata(itmp2),fa4
	jmp     L_register_copy
handle_fa5:
	movq    offvmargdata(itmp2),fa5
	jmp     L_register_copy
handle_fa6:
	movq    offvmargdata(itmp2),fa6
	jmp     L_register_copy
handle_fa7:
	movq    offvmargdata(itmp2),fa7
	jmp     L_register_copy

asm_vm_call_method_end:
	nop

/****************** function asm_call_jit_compiler *****************************
*                                                                              *
*   invokes the compiler for untranslated JavaVM methods.                      *
*                                                                              *
*   Register R0 contains a pointer to the method info structure (prepared      *
*   by createcompilerstub). Using the return address in R26 and the            *
*   offset in the LDA instruction or using the value in methodptr R28 the      *
*   patching address for storing the method address can be computed:           *
*                                                                              *
*   method address was either loaded using                                     *
*                                                                              *
*   i386_mov_imm_reg(a, REG_ITMP2)                ; invokestatic/special       *
*   i386_call_reg(REG_ITMP2)                                                   *
*                                                                              *
*   or                                                                         *
*                                                                              *
*   i386_mov_membase_reg(REG_SP, 0, REG_ITMP2)    ; invokevirtual/interface    *
*   i386_mov_membase_reg(REG_ITMP2, OFFSET(, vftbl), REG_ITMP3)                *
*   i386_mov_membase_reg(REG_ITMP3, OFFSET(vftbl, table[0]) + \                *
*       sizeof(methodptr) * m->vftblindex, REG_ITMP1)                          *
*   i386_call_reg(REG_ITMP1)                                                   *
*                                                                              *
*   in the static case the method pointer can be computed using the            *
*   return address and the lda function following the jmp instruction          *
*                                                                              *
*******************************************************************************/

asm_call_jit_compiler:
L_asm_call_jit_compiler:                /* required for PIC code              */
	sub     $(ARG_CNT+1)*8,sp           /* +1: keep stack 16-byte aligned     */

	SAVE_ARGUMENT_REGISTERS(0)

	mov     itmp1,a0                    /* pass methodinfo pointer            */
	mov     mptr,a1                     /* pass method pointer                */
	mov     sp,a2                       /* pass java sp                       */
	add     $(1+ARG_CNT+1)*8,a2
	mov     (ARG_CNT+1)*8(sp),a3	    /* pass ra to java function           */
	call    jit_asm_compile@PLT

	RESTORE_ARGUMENT_REGISTERS(0)

	add     $(ARG_CNT+1)*8,sp           /* remove stack frame                 */

	test    v0,v0                       /* check for exception                */
	je      L_asm_call_jit_compiler_exception

	jmp     *v0                         /* ...and now call the new method     */

L_asm_call_jit_compiler_exception:
	call    exceptions_get_and_clear_exception@PLT
	pop     xpc                         /* delete return address              */
	sub     $3,xpc                      /* faulting address is ra - 3         */
	jmp     L_asm_handle_exception


/* asm_handle_exception ********************************************************
*                                                                              *
*   This function handles an exception. It does not use the usual calling      *
*   conventions. The exception pointer is passed in REG_ITMP1 and the          *
*   pc from the exception raising position is passed in REG_ITMP2. It searches *
*   the local exception table for a handler. If no one is found, it unwinds    *
*   stacks and continues searching the callers.                                *
*                                                                              *
*******************************************************************************/

asm_handle_nat_exception:
	add     $8,sp                       /* clear return address of native stub*/
		
asm_handle_exception:
L_asm_handle_exception:                 /* required for PIC code              */
	sub     $((ARG_CNT+TMP_CNT)*8),sp   /* create maybe-leaf stackframe       */

	SAVE_ARGUMENT_REGISTERS(0)          /* we save arg and temp registers in  */
	SAVE_TEMPORARY_REGISTERS(ARG_CNT)   /* case this is a leaf method         */

	mov     $((ARG_CNT+TMP_CNT)*8),a3   /* prepare a3 for handle_exception    */
	mov     $1,t0                       /* set maybe-leaf flag                */

L_asm_handle_exception_stack_loop:
	sub     $(6*8),sp
	mov     xptr,0*8(sp)                /* save exception pointer             */
	mov     xpc,1*8(sp)                 /* save exception pc                  */
	add     sp,a3                       /* calculate Java sp into a3...       */
	add     $(6*8),a3
	mov     a3,3*8(sp)                  /* ...and save it                     */
	mov     t0,4*8(sp)                  /* save maybe-leaf flag               */

	mov     xpc,a0                      /* exception pc                       */
	call    codegen_get_pv_from_pc@PLT
	mov     v0,2*8(sp)                  /* save data segment pointer          */
        
	mov     0*8(sp),a0                  /* pass exception pointer             */
	mov     1*8(sp),a1                  /* pass exception pc                  */
	mov     v0,a2                       /* pass data segment pointer          */
	mov     3*8(sp),a3                  /* pass Java stack pointer            */
	call    exceptions_handle_exception@PLT

	test    v0,v0
	jz      L_asm_handle_exception_not_catched

	mov     v0,xpc                      /* move handlerpc into xpc            */
	mov     0*8(sp),xptr                /* restore exception pointer          */
	mov     4*8(sp),t0                  /* get maybe-leaf flag                */
	add     $(6*8),sp                   /* free stack frame                   */

	test    t0,t0                       /* test for maybe-leaf flag           */
	jz      L_asm_handle_exception_no_leaf

	RESTORE_ARGUMENT_REGISTERS(0)       /* if this is a leaf method, we have  */
	RESTORE_TEMPORARY_REGISTERS(ARG_CNT)/* to restore arg and temp registers  */

	add     $((ARG_CNT+TMP_CNT)*8),sp   /* remove maybe-leaf stackframe       */

L_asm_handle_exception_no_leaf:
	jmp     *xpc                        /* jump to the handler                */

L_asm_handle_exception_not_catched:
	mov     0*8(sp),xptr                /* restore exception pointer          */
	mov     2*8(sp),itmp3               /* restore data segment pointer       */
	mov     4*8(sp),t0                  /* get maybe-leaf flag                */
	add     $(6*8),sp

	test    t0,t0
	jz      L_asm_handle_exception_no_leaf_stack

	add     $((ARG_CNT+TMP_CNT)*8),sp   /* remove maybe-leaf stackframe       */
	xor     t0,t0                       /* clear the isleaf flags             */

L_asm_handle_exception_no_leaf_stack:
	mov     FrameSize(itmp3),itmp2l     /* get frame size                     */
	add     sp,itmp2                    /* pointer to save area               */
	
	mov     IntSave(itmp3),a0l          /* a0l = saved int register count     */
	test    a0l,a0l
	je      noint
	
	cmp     $1,a0l
	je      int1
	cmp     $2,a0l
	je      int2
	cmp     $3,a0l
	je      int3
	cmp     $4,a0l
	je      int4
	
	mov     -5*8(itmp2),s0
int4:   
	mov     -4*8(itmp2),s1
int3:   
	mov     -3*8(itmp2),s2
int2:   
	mov     -2*8(itmp2),s3
int1:   
	mov     -1*8(itmp2),s4

	shl     $3,a0l                      /* multiply by 8 bytes                */
	sub     a0,itmp2
		
noint:
#if 0
	mov     FltSave(itmp3),a0l          /* a0l = saved flt register count     */
	test	a0l,a0l
	je      noflt
	
	cmpl    $1,a0l
	je      flt1
	cmpl    $2,a0l
	je      flt2
	cmpl    $3,a0l
	je      flt3
	cmpl    $4,a0l
	je      flt4

	movq    -5*8(itmp2),%xmm11
flt4:   
	movq    -4*8(itmp2),%xmm12
flt3:   
	movq    -3*8(itmp2),%xmm13
flt2:   
	movq    -2*8(itmp2),%xmm14
flt1:   
	movq    -1*8(itmp2),%xmm15
                
noflt:
#endif
	mov     FrameSize(itmp3),itmp2l     /* get frame size                     */
	add     itmp2,sp                    /* unwind stack                       */

	                                    /* exception pointer is still set     */
	pop     xpc                         /* the new xpc is return address      */
	sub     $3,xpc                      /* subtract 3 bytes for call          */

	xor     a3,a3                       /* prepare a3 for handle_exception    */
	
	jmp		L_asm_handle_exception_stack_loop


/* asm_abstractmethoderror *****************************************************

   Creates and throws an AbstractMethodError.

*******************************************************************************/

asm_abstractmethoderror:
	mov     sp,a0                       /* pass java sp                       */
	add     $1*8,a0
	mov     0*8(sp),a1                  /* pass exception address             */
	sub     $3,a1
	call	exceptions_asm_new_abstractmethoderror@PLT
	                                    /* exception pointer is return value  */
	pop     xpc                         /* get exception address              */
	sub     $3,xpc                      /* exception address is ra - 3        */
	jmp     L_asm_handle_exception


/* asm_patcher_wrapper *********************************************************

   XXX

   Stack layout:
     40   return address
     32   pointer to virtual java_objectheader
     24   machine code (which is patched back later)
     16   unresolved class/method/field reference
      8   data segment displacement from load instructions
      0   pointer to patcher function
     -8   bp

*******************************************************************************/

asm_patcher_wrapper:
	push    bp                          /* save base pointer                  */
	mov     sp,bp                       /* move actual sp to bp               */
	sub     $(3+ARG_CNT+TMP_CNT)*8,sp
	and     $0xfffffffffffffff0,sp      /* align sp to 16-byte (this is for   */
	                                    /* leaf functions)                    */

	SAVE_ARGUMENT_REGISTERS(3)
	SAVE_TEMPORARY_REGISTERS(3+ARG_CNT)

	mov     itmp1,0*8(sp)               /* save itmp1 and itmp2               */
	mov     itmp2,1*8(sp)               /* can be used by some instructions   */

	mov     bp,a0                       /* pass SP of patcher stub            */
	add     $(1*8),a0
	mov     $0,a1                       /* pass PV (if NULL, use findmethod)  */
	mov     $0,a2                       /* pass RA (it's on the stack)        */
	call    patcher_wrapper@PLT
	mov     v0,2*8(sp)                  /* save return value                  */

	RESTORE_ARGUMENT_REGISTERS(3)
	RESTORE_TEMPORARY_REGISTERS(3+ARG_CNT)

	mov     0*8(sp),itmp1               /* restore itmp1 and itmp2            */
	mov     1*8(sp),itmp2               /* can be used by some instructions   */
	mov     2*8(sp),itmp3               /* restore return value               */

	mov     bp,sp                       /* restore original sp                */
	pop     bp                          /* restore bp                         */
	add     $(5*8),sp                   /* remove patcher stackframe, keep RA */

	test    itmp3,itmp3                 /* exception thrown?                  */
	jne     L_asm_patcher_wrapper_exception
	ret                                 /* call new patched code              */

L_asm_patcher_wrapper_exception:
	mov     itmp3,xptr                  /* get exception                      */
	pop     xpc                         /* get and remove return address      */
	jmp     L_asm_handle_exception

#if defined(ENABLE_REPLACEMENT)

/* asm_replacement_out *********************************************************

   This code is jumped to from the replacement-out stubs that are executed
   when a thread reaches an activated replacement point.

   The purpose of asm_replacement_out is to read out the parts of the
   execution state that cannot be accessed from C code, store this state,
   and then call the C function replace_me.

   Stack layout:
      8                 start of stack inside method to replace
      0   rplpoint *    info on the replacement point that was reached

*******************************************************************************/

/* some room to accomodate changes of the stack frame size during replacement */
	/* XXX we should find a cleaner solution here */
#define REPLACEMENT_ROOM  512

asm_replacement_out:
    /* create stack frame */
	sub     $(sizeexecutionstate + REPLACEMENT_ROOM),sp

	/* save registers in execution state */
	mov     %rax,(RAX*8+offes_intregs)(sp)
	mov     %rbx,(RBX*8+offes_intregs)(sp)
	mov     %rcx,(RCX*8+offes_intregs)(sp)
	mov     %rdx,(RDX*8+offes_intregs)(sp)
	mov     %rsi,(RSI*8+offes_intregs)(sp)
	mov     %rdi,(RDI*8+offes_intregs)(sp)
	mov     %rbp,(RBP*8+offes_intregs)(sp)
	movq    $0  ,(RSP*8+offes_intregs)(sp) /* not used */
	mov     %r8 ,(R8 *8+offes_intregs)(sp)
	mov     %r9 ,(R9 *8+offes_intregs)(sp)
	mov     %r10,(R10*8+offes_intregs)(sp)
	mov     %r11,(R11*8+offes_intregs)(sp)
	mov     %r12,(R12*8+offes_intregs)(sp)
	mov     %r13,(R13*8+offes_intregs)(sp)
	mov     %r14,(R14*8+offes_intregs)(sp)
	mov     %r15,(R15*8+offes_intregs)(sp)

	movq    %xmm0 ,(XMM0 *8+offes_fltregs)(sp)
	movq    %xmm1 ,(XMM1 *8+offes_fltregs)(sp)
	movq    %xmm2 ,(XMM2 *8+offes_fltregs)(sp)
	movq    %xmm3 ,(XMM3 *8+offes_fltregs)(sp)
	movq    %xmm4 ,(XMM4 *8+offes_fltregs)(sp)
	movq    %xmm5 ,(XMM5 *8+offes_fltregs)(sp)
	movq    %xmm6 ,(XMM6 *8+offes_fltregs)(sp)
	movq    %xmm7 ,(XMM7 *8+offes_fltregs)(sp)
	movq    %xmm8 ,(XMM8 *8+offes_fltregs)(sp)
	movq    %xmm9 ,(XMM9 *8+offes_fltregs)(sp)
	movq    %xmm10,(XMM10*8+offes_fltregs)(sp)
	movq    %xmm11,(XMM11*8+offes_fltregs)(sp)
	movq    %xmm12,(XMM12*8+offes_fltregs)(sp)
	movq    %xmm13,(XMM13*8+offes_fltregs)(sp)
	movq    %xmm14,(XMM14*8+offes_fltregs)(sp)
	movq    %xmm15,(XMM15*8+offes_fltregs)(sp)

	/* calculate sp of method */
	mov     sp,itmp1
	add     $(sizeexecutionstate + REPLACEMENT_ROOM + 8),itmp1
	mov     itmp1,(offes_sp)(sp)

	/* pv must be looked up via AVL tree */
	movq    $0,(offes_pv)(sp)

	/* call replace_me */
	mov     -8(itmp1),a0                /* rplpoint *                         */
    mov     sp,a1                       /* arg1: execution state              */
    call    replace_me@PLT              /* call C function replace_me         */
    call    abort@PLT                   /* NEVER REACHED                      */

/* asm_replacement_in **********************************************************

   This code writes the given execution state and jumps to the replacement
   code.

   This function never returns!

   C prototype:
      void asm_replacement_in(executionstate *es, replace_safestack_t *st);

*******************************************************************************/

asm_replacement_in:
	/* get arguments */
	mov     a1,s1                       /* replace_safestack_t *st            */
	mov     a0,%rbp                     /* executionstate *es == safe stack   */

	/* switch to the safe stack */
	mov     %rbp,sp

	/* call replace_build_execution_state(st) */
	mov		s1,a0
	call    replace_build_execution_state@PLT

	/* set new sp */
	mov     (offes_sp)(%rbp),sp

	/* push address of new code */
	pushq   (offes_pc)(%rbp)

	/* allocate an executionstate_t on the stack */
	sub		$(sizeexecutionstate),sp

	/* call replace_free_safestack(st,& of allocated executionstate_t) */
	mov		sp,a1
	mov		s1,a0
	call    replace_free_safestack@PLT

	/* copy registers from execution state */
	movq    (XMM0 *8+offes_fltregs)(sp),%xmm0
	movq    (XMM1 *8+offes_fltregs)(sp),%xmm1
	movq    (XMM2 *8+offes_fltregs)(sp),%xmm2
	movq    (XMM3 *8+offes_fltregs)(sp),%xmm3
	movq    (XMM4 *8+offes_fltregs)(sp),%xmm4
	movq    (XMM5 *8+offes_fltregs)(sp),%xmm5
	movq    (XMM6 *8+offes_fltregs)(sp),%xmm6
	movq    (XMM7 *8+offes_fltregs)(sp),%xmm7
	movq    (XMM8 *8+offes_fltregs)(sp),%xmm8
	movq    (XMM9 *8+offes_fltregs)(sp),%xmm9
	movq    (XMM10*8+offes_fltregs)(sp),%xmm10
	movq    (XMM11*8+offes_fltregs)(sp),%xmm11
	movq    (XMM12*8+offes_fltregs)(sp),%xmm12
	movq    (XMM13*8+offes_fltregs)(sp),%xmm13
	movq    (XMM14*8+offes_fltregs)(sp),%xmm14
	movq    (XMM15*8+offes_fltregs)(sp),%xmm15

	mov     (RAX*8+offes_intregs)(sp),%rax
	mov     (RBX*8+offes_intregs)(sp),%rbx
	mov     (RCX*8+offes_intregs)(sp),%rcx
	mov     (RDX*8+offes_intregs)(sp),%rdx
	mov     (RSI*8+offes_intregs)(sp),%rsi
	mov     (RDI*8+offes_intregs)(sp),%rdi
	mov     (RBP*8+offes_intregs)(sp),%rbp
	mov     (R8 *8+offes_intregs)(sp),%r8
	mov     (R9 *8+offes_intregs)(sp),%r9
	mov     (R10*8+offes_intregs)(sp),%r10
	mov     (R11*8+offes_intregs)(sp),%r11
	mov     (R12*8+offes_intregs)(sp),%r12
	mov     (R13*8+offes_intregs)(sp),%r13
	mov     (R14*8+offes_intregs)(sp),%r14
	mov     (R15*8+offes_intregs)(sp),%r15

	/* pop the execution state off the stack */
	add		$(sizeexecutionstate),sp

	/* jump to new code */
	ret

#endif /* defined(ENABLE_REPLACEMENT) */


/* asm_builtin_x2x *************************************************************
*                                                                              *
*   Wrapper functions for float to int corner cases                            *
*                                                                              *
*******************************************************************************/

asm_builtin_f2i:
	sub     $(ARG_CNT*8),sp
	
	SAVE_ARGUMENT_REGISTERS(0)
	
	movq    ftmp1,fa0
	call    builtin_f2i@PLT
	
	RESTORE_ARGUMENT_REGISTERS(0)
	
	add     $(ARG_CNT*8),sp
	ret


asm_builtin_f2l:
	sub     $(ARG_CNT*8),sp
	
	SAVE_ARGUMENT_REGISTERS(0)
	
	movq    ftmp1,fa0
	call    builtin_f2l@PLT
	
	RESTORE_ARGUMENT_REGISTERS(0)
	
	add     $(ARG_CNT*8),sp
	ret


asm_builtin_d2i:
	sub     $(ARG_CNT*8),sp
	
	SAVE_ARGUMENT_REGISTERS(0)
	
	movq    ftmp1,fa0
	call    builtin_d2i@PLT
	
	RESTORE_ARGUMENT_REGISTERS(0)
	
	add     $(ARG_CNT*8),sp
	ret


asm_builtin_d2l:
	sub     $(ARG_CNT*8),sp
	
	SAVE_ARGUMENT_REGISTERS(0)
	
	movq    ftmp1,fa0
	call    builtin_d2l@PLT
	
	RESTORE_ARGUMENT_REGISTERS(0)
	
	add     $(ARG_CNT*8),sp
	ret


/* asm_compare_and_swap ********************************************************

   Does an atomic compare and swap.  Required for the lock
   implementation.

*******************************************************************************/

asm_compare_and_swap:
	mov     a1,v0                       /* v0 is %rax                         */
	lock cmpxchg a2,(a0)
	ret


/* asm_memory_barrier **********************************************************

   A memory barrier for the Java Memory Model.

*******************************************************************************/

asm_memory_barrier:
	mfence
	ret


asm_getclassvalues_atomic:
_crit_restart:
_crit_begin:
	movl    offbaseval(a0),itmp1l
	movl    offdiffval(a0),itmp2l
	movl    offbaseval(a1),itmp3l
_crit_end:
	movl    itmp1l,offcast_super_baseval(a2)
	movl    itmp2l,offcast_super_diffval(a2)
	movl    itmp3l,offcast_sub_baseval(a2)
	ret

	.data
		
asm_criticalsections:
#if defined(ENABLE_THREADS)
	.quad   _crit_begin
	.quad   _crit_end
	.quad   _crit_restart
#endif
	.quad 0


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

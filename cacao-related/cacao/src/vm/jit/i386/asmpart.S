/* src/vm/jit/i386/asmpart.S - Java-C interface functions for i386

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

#include "md-asm.h"

#include "vm/jit/i386/arch.h"
#include "vm/jit/i386/md-abi.h"
#include "vm/jit/i386/offsets.h"

#include "vm/jit/abi-asm.h"
#include "vm/jit/methodheader.h"


	.text


/* export functions ***********************************************************/

	.globl asm_md_init

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

	.globl asm_builtin_f2i
	.globl asm_builtin_f2l
	.globl asm_builtin_d2i
	.globl asm_builtin_d2l

	.globl asm_compare_and_swap
	.globl asm_memory_barrier

	.globl asm_criticalsections
	.globl asm_getclassvalues_atomic

	.globl asm_get_cycle_count


/* asm_md_init *****************************************************************

   Initialize machine dependent stuff.

   See: http://www.srware.com/linux_numerics.txt

   This puts the X86 FPU in 64-bit precision mode.  The default under
   Linux is to use 80-bit mode, which produces subtle differences from
   FreeBSD and other systems, eg, (int)(1000*atof("0.3")) is 300 in
   64-bit mode, 299 in 80-bit mode.

   Fixes: http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=350729

*******************************************************************************/

asm_md_init:
	sub     $4,sp                       /* allocate space for the FPU state   */
	fnstcw  (sp)                        /* get the FPU state                  */
	mov     (sp),%eax
	and     $0xfcff,%ax                 /* remove the extended mode flag      */
	or      $0x0200,%ax                 /* put the double mode flag           */
	mov     %eax,(sp)                   /* store new FPU state                */
	fldcw   (sp)                        /* setup new FPU state                */
	add     $4,sp
	ret


/********************* function asm_calljavafunction ***************************
*                                                                              *
*   This function calls a Java-method (which possibly needs compilation)       *
*   with up to 4 address parameters.                                           *
*                                                                              *
*   This functions calls the JIT-compiler which eventually translates the      *
*   method into machine code.                                                  *
*                                                                              *
*   C-prototype:                                                               *
*    javaobject_header *asm_vm_call_method(methodinfo *m,                      *
*         u4 count, u4 size, void *callblock);                                 *
*                                                                              *
*******************************************************************************/

	.align	8

	.long   0                           /* catch type all                     */
	.long   0                           /* handler pc                         */
	.long   0                           /* end pc                             */
	.long   0                           /* start pc                           */
	.long   1                           /* extable size                       */
	.long   0                           /* line number table start            */
	.long   0                           /* line number table size             */
	.long   0                           /* fltsave                            */
	.long   0                           /* intsave                            */
	.long   0                           /* isleaf                             */
	.long   0                           /* IsSync                             */
	.long   0                           /* frame size                         */
	.long   0                           /* codeinfo pointer                   */

asm_vm_call_method:
asm_vm_call_method_int:
asm_vm_call_method_long:
asm_vm_call_method_float:
asm_vm_call_method_double:
	push    bp
	mov     sp,bp                       /* save stackptr                      */
	sub     $(4*4),sp                   /* create stackframe                  */
	and     $0xfffffff0,sp              /* align stack to 16-byte             */

	mov     t0,0*4(sp)                  /* save registers                     */
	mov     s1,1*4(sp)
	mov     s2,2*4(sp)

	mov     4*4(bp),itmp1               /* pointer to arg block (4(push)+4(return)+4+4)*/
	mov     3*4(bp),itmp2               /* arg count            (4(push)+4(return)+4 */

	mov     sp,s1                       /* save the stackpointer              */

	test    itmp2,itmp2                 /* maybe we have no args              */
	jle     L_asm_vm_call_method_copy_done

	mov     itmp2,itmp3                 /* calculate stack size               */
	mov     itmp1,%edi                  /* save pointer to arg block          */

calljava_calcstacksize:
	mov     offvmargtype(itmp1),t0
	test    $1,t0                       /* two word type?                     */
	jz      calljava_onewordtype

	sub     $4,sp                       /* add 1 slot to stackframe size      */

calljava_onewordtype:
	sub     $4,sp                       /* add 1 slot to stackframe size      */
	sub     $1,itmp3
	test	itmp3,itmp3                 /* any args left?                     */
	jz      calljava_setstack

	add     $sizevmarg,itmp1            /* goto next argument block           */
	jmp     calljava_calcstacksize
		
calljava_setstack:				
	mov     %edi,itmp1                  /* restore pointer to arg block       */
	and     $0xfffffff0,sp              /* align stack to 16-byte             */
	mov     sp,itmp3                    /* initialize pointer for copying     */

calljava_copyloop:
	mov     offvmargdata(itmp1),t0      /* get 4-bytes of argument            */
	mov     t0,(itmp3)                  /* and store them on the stack        */
	add     $4,itmp3                    /* increase sp to next argument       */

	mov     offvmargtype(itmp1),t0      /* get the argument type              */
	test    $1,t0                       /* two word type?                     */
	jz      calljava_copynext

	mov     offvmargdata+4(itmp1),t0    /* get upper 4-bytes of 2 word type   */
	mov     t0,(itmp3)			
	add     $4,itmp3                    /* increase sp to next argument       */

calljava_copynext:		
	sub     $1,itmp2                    /* are there any args left?           */
	test    itmp2,itmp2
	jle     L_asm_vm_call_method_copy_done

	add     $sizevmarg,itmp1            /* goto next argument block           */
	jmp     calljava_copyloop

L_asm_vm_call_method_copy_done:
	mov     2*4(bp),itmp1               /* move function pointer to itmp1     */

	lea     L_asm_call_jit_compiler,mptr
	mov     mptr,3*4(s1)
	lea     (3*4-256)(s1),mptr          /* We subtract 256 to force the next  */
	                                    /* move instruction to have a 32-bit  */
	                                    /* offset.                            */

	mov     (0*4+256)(mptr),itmp3       /* method call as in Java             */
	call    *itmp3                      /* call JIT compiler                  */

L_asm_vm_call_method_return:
	mov     s1,sp                       /* restore stackpointer               */

	mov     0*4(sp),t0                  /* restore registers                  */
	mov     1*4(sp),s1
	mov     2*4(sp),s2

	leave
	ret

asm_vm_call_method_exception_handler:
	push    xptr                        /* pass exception pointer             */
	call    builtin_throw_exception
	add     $4,sp
asm_vm_call_method_end:
	jmp     L_asm_vm_call_method_return


/* asm_call_jit_compiler *******************************************************

   Invokes the compiler for untranslated JavaVM methods.

   Register R0 contains a pointer to the method info structure (prepared
   by createcompilerstub). Using the return address in R26 and the
   offset in the LDA instruction or using the value in methodptr R28 the
   patching address for storing the method address can be computed:

   Method address was either loaded using

   i386_mov_imm_reg(a, REG_ITMP2)                ; invokestatic/special
   i386_call_reg(REG_ITMP2)

   or

   i386_mov_membase_reg(REG_SP, 0, REG_ITMP1)    ; invokevirtual/interface
   i386_mov_membase_reg(REG_ITMP1, OFFSET(, vftbl), REG_ITMP2)
   i386_mov_membase_reg(REG_ITMP2, OFFSET(vftbl, table[0]) + \
       sizeof(methodptr) * m->vftblindex, REG_ITMP1)
   i386_call_reg(REG_ITMP1)

   In the static case the method pointer can be computed using the
   return address and the lda function following the jmp instruction.

*******************************************************************************/

asm_call_jit_compiler:
L_asm_call_jit_compiler:                /* required for PIC code              */
	sub     $(4*4),sp                   /* keep stack 16-byte aligned         */

	mov     itmp1,0*4(sp)               /* pass methodinfo pointer            */
	mov     mptr,1*4(sp)                /* pass method pointer                */
	mov     sp,itmp2                    /* pass java sp                       */
	add     $((1+4)*4),itmp2
	mov     itmp2,2*4(sp)
	mov     4*4(sp),itmp3               /* pass java ra                       */
	mov     itmp3,3*4(sp)
	call    jit_asm_compile

	add     $(4*4),sp                   /* remove stack frame                 */

	test    v0,v0                       /* check for exception                */
	je      L_asm_call_jit_compiler_exception

	jmp		*v0                         /* ...and now call the new method     */

L_asm_call_jit_compiler_exception:
	call    exceptions_get_and_clear_exception
	                                    /* v0 == xptr                         */
	pop     xpc                         /* get return address                 */
	sub     $2,xpc                      /* faulting address is ra - 2         */
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
	add     $4,sp                       /* clear return address of native stub*/
		
asm_handle_exception:
L_asm_handle_exception:                 /* required for PIC code              */
	sub     $((ARG_CNT+TMP_CNT+3)*4),sp /* keep stack 16-byte aligned         */

	SAVE_ARGUMENT_REGISTERS(0)          /* we save arg and temp registers in  */
	SAVE_TEMPORARY_REGISTERS(ARG_CNT)   /* case this is a leaf method         */

	mov     $((ARG_CNT+TMP_CNT+3)*4),itmp3 /* prepare a3 for handle_exception */
	mov     $1,t0                       /* set maybe-leaf flag                */

L_asm_handle_exception_stack_loop:
	sub     $(12*4),sp                  /* keep stack 16-byte aligned         */
	mov     xptr,4*4(sp)                /* save exception pointer             */
	mov     xpc,5*4(sp)                 /* save exception pc                  */
	add     sp,itmp3                    /* calculate Java sp into a3...       */
	add     $(12*4),itmp3
	mov     itmp3,7*4(sp)               /* ...and save it                     */
	mov     t0,8*4(sp)                  /* save maybe-leaf flag               */

	mov     xpc,0*4(sp)                 /* pass exception pc                  */
	call    codegen_get_pv_from_pc
	mov     v0,6*4(sp)                  /* save data segment pointer          */

	mov     4*4(sp),itmp3               /* pass exception pointer             */
	mov     itmp3,0*4(sp)
	mov     5*4(sp),itmp3               /* pass exception pc                  */
	mov     itmp3,1*4(sp)
	mov     v0,2*4(sp)                  /* pass data segment pointer          */
	mov     7*4(sp),itmp3               /* pass Java stack pointer            */
	mov     itmp3,3*4(sp)
	call    exceptions_handle_exception

	test    v0,v0
	jz      L_asm_handle_exception_not_catched

	mov     v0,xpc                      /* move handlerpc into xpc            */
	mov     4*4(sp),xptr                /* restore exception pointer          */
	mov     8*4(sp),t0                  /* get maybe-leaf flag                */
	add     $(12*4),sp                  /* free stackframe                    */

	test    t0,t0                       /* test for maybe-leaf flag           */
	jz      L_asm_handle_exception_no_leaf

	RESTORE_ARGUMENT_REGISTERS(0)       /* if this is a leaf method, we have  */
	RESTORE_TEMPORARY_REGISTERS(ARG_CNT)/* to restore arg and temp registers  */

	add     $((ARG_CNT+TMP_CNT+3)*4),sp /* remove maybe-leaf stackframe       */

L_asm_handle_exception_no_leaf:
	jmp     *xpc                        /* jump to exception handler          */

L_asm_handle_exception_not_catched:
	mov     4*4(sp),xptr                /* restore exception pointer          */
	mov     6*4(sp),itmp3               /* restore data segment pointer       */
	mov     8*4(sp),t0                  /* get maybe-leaf flag                */
	add     $(12*4),sp                  /* free stackframe                    */

	test    t0,t0
	jz      L_asm_handle_exception_no_leaf_stack

	add     $((ARG_CNT+TMP_CNT+3)*4),sp /* remove maybe-leaf stackframe       */
	xor     t0,t0                       /* clear the maybe-leaf flag          */

L_asm_handle_exception_no_leaf_stack:
	mov     FrameSize(itmp3),itmp2      /* get frame size                     */
	add     sp,itmp2                    /* pointer to save area               */

	push    xptr                        /* we are out of registers            */

	mov     IntSave(itmp3),itmp1        /* itmp1 = saved int register count   */
	test    itmp1,itmp1
	je      noint

	cmp     $1,itmp1
	je      int1
	cmp     $2,itmp1
	je      int2

	mov     -3*4(itmp2),s0
int2:	
	mov     -2*4(itmp2),s1
int1:	
	mov     -1*4(itmp2),s2

	shl     $2,itmp1                    /* multiply by 4 bytes                */
	sub     itmp1,itmp2
		
noint:
#if 0
	mov     FltSave(itmp3),itmp1        /* itmp1 = saved flt register count   */
	test    itmp1,itmp1
	je      noflt

	cmp     $1,itmp1
	je      flt1
	cmp     $2,itmp1
	je      flt2
	cmp     $3,itmp1
	je      flt3
		
	fldl    -4*8(itmp2)
	fstp    %st(1)
flt3:
	fldl    -3*8(itmp2)
	fstp    %st(2)
flt2:
	fldl    -2*8(itmp2)
	fstp    %st(3)
flt1:
	fldl    -1*8(itmp2)
	fstp    %st(4)
		
noflt:
#endif
	pop     xptr                        /* restore exception pointer          */
	mov     FrameSize(itmp3),itmp2      /* get frame size                     */
	add     itmp2,sp                    /* unwind stack                       */

	pop     xpc                         /* the new xpc is return address      */
	sub     $2,xpc                      /* subtract 2-bytes for call          */

	xor     itmp3,itmp3                 /* prepare a3 for handle_exception    */

	jmp     L_asm_handle_exception_stack_loop
		

/* asm_abstractmethoderror *****************************************************

   Creates and throws an AbstractMethodError.

*******************************************************************************/

asm_abstractmethoderror:
	sub     $(3*4),sp                   /* keep stack 16-byte aligned         */
	mov     sp,itmp1                    /* pass java sp                       */
	add     $((1+3)*4),itmp1
	mov     itmp1,0*4(sp)
	mov     3*4(sp),itmp2               /* pass exception address             */
	sub     $2,itmp2
	mov     itmp2,1*4(sp)
	call	exceptions_asm_new_abstractmethoderror
	                                    /* exception pointer is return value  */
	add     $(3*4),sp                   /* remove stack frame                 */

	pop     xpc                         /* get exception address              */
	sub     $2,xpc                      /* exception address is ra - 2        */
	jmp     L_asm_handle_exception


/* asm_patcher_wrapper *********************************************************

   XXX

   Stack layout:
     24   return address
     20   REG_ITMP3
     16   pointer to virtual java_objectheader
     12   last byte of machine code (xmcode)
      8   machine code (which is patched back later)
      4   unresolved field reference
      0   patcher function pointer to call

*******************************************************************************/

asm_patcher_wrapper:
	sub     $((1+4+4)*4),sp             /* keep stack 16-byte aligned         */

	mov     itmp1,(0+4)*4(sp)           /* save itmp1 and itmp2               */
	mov     itmp2,(1+4)*4(sp)

	mov     sp,itmp1                    /* pass SP of patcher stub            */
	add     $((1+4+4)*4),itmp1
	mov     itmp1,0*4(sp)
	movl    $0,1*4(sp)                  /* pass PV (if NULL, use findmethod)  */
	movl    $0,2*4(sp)                  /* pass RA (it's on the stack)        */
	call    patcher_wrapper
	mov     v0,itmp3                    /* save return value                  */

	mov     (0+4)*4(sp),itmp1           /* restore itmp1 and itmp2            */
	mov     (1+4)*4(sp),itmp2

	test    itmp3,itmp3                 /* exception thrown?                  */
	jne     L_asm_patcher_wrapper_exception

	mov     (5+1+4+4)*4(sp),itmp3       /* restore itmp3                      */
	add     $((6+1+4+4)*4),sp           /* remove stack frame, keep RA        */

	ret                                 /* jump to new patched code           */

L_asm_patcher_wrapper_exception:
	add     $((6+1+4+4)*4),sp           /* remove stack frame, keep RA        */
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
      4                 start of stack inside method to replace
      0   rplpoint *    info on the replacement point that was reached

*******************************************************************************/

/* some room to accomodate changes of the stack frame size during replacement */
	/* XXX we should find a cleaner solution here */
#define REPLACEMENT_ROOM  512

asm_replacement_out:
    /* create stack frame */
	sub     $(sizeexecutionstate + REPLACEMENT_ROOM),sp

	/* save registers in execution state */
	mov     %eax,(EAX*4+offes_intregs)(sp)
	mov     %ebx,(EBX*4+offes_intregs)(sp)
	mov     %ecx,(ECX*4+offes_intregs)(sp)
	mov     %edx,(EDX*4+offes_intregs)(sp)
	mov     %esi,(ESI*4+offes_intregs)(sp)
	mov     %edi,(EDI*4+offes_intregs)(sp)
	mov     %ebp,(EBP*4+offes_intregs)(sp)
	movl    $0  ,(ESP*4+offes_intregs)(sp) /* not used */

	/* calculate sp of method */
	mov     sp,itmp1
	add     $(sizeexecutionstate + REPLACEMENT_ROOM + 4),itmp1
	mov     itmp1,(offes_sp)(sp)

	/* pv must be looked up via AVL tree */
	movl    $0,(offes_pv)(sp)

	/* call replace_me */
	mov     -4(itmp1),itmp1             /* rplpoint *                         */
    push    sp                          /* arg1: execution state              */
    push    itmp1                       /* arg0: replacement point            */
    call    replace_me                  /* call C function replace_me         */


/* asm_replacement_in **********************************************************

   This code writes the given execution state and jumps to the replacement
   code.

   This function never returns!

   C prototype:
      void asm_replacement_in(executionstate *es, replace_safestack_t *st);

*******************************************************************************/

asm_replacement_in:
	/* get arguments */
	mov     8(sp),%esi                  /* replace_safestack_t *st            */
	mov     4(sp),%ebp                  /* executionstate *es == safe stack   */

	/* switch to the safe stack and build a stack frame */
	mov     %ebp,sp
	sub		$(1*4),sp

	/* call replace_build_execution_state(st) */
	mov		%esi,(0*4)(sp)
	call    replace_build_execution_state

	/* set new sp */
	mov     (offes_sp)(%ebp),sp

	/* push address of new code */
	push    (offes_pc)(%ebp)

	/* allocate an executionstate_t on the stack */
	sub		$(sizeexecutionstate),sp

	/* call replace_free_safestack(st,& of allocated executionstate_t) */
	push    sp   /* tmpes */
	push    %esi /* st    */
	call    replace_free_safestack
	add     $(2*4),sp

	/* copy registers from execution state */
	mov     (EAX*4+offes_intregs)(sp),%eax
	mov     (EBX*4+offes_intregs)(sp),%ebx
	mov     (ECX*4+offes_intregs)(sp),%ecx
	mov     (EDX*4+offes_intregs)(sp),%edx
	mov     (ESI*4+offes_intregs)(sp),%esi
	mov     (EDI*4+offes_intregs)(sp),%edi
	mov     (EBP*4+offes_intregs)(sp),%ebp

	/* pop the execution state off the stack */
	add		$(sizeexecutionstate),sp

	/* jump to new code, hold your thumbs! ;) */
	ret

#endif /* defined(ENABLE_REPLACEMENT) */


/************************ function asm_builtin_x2x *****************************
*                                                                              *
*   Wrapper functions for corner cases                                         *
*                                                                              *
*******************************************************************************/

asm_builtin_f2i:
	sub     $(3*4),%esp
	fsts    (%esp)
	call    builtin_f2i
	add     $(3*4),%esp
	ret

asm_builtin_d2i:
	sub     $(3*4),%esp
	fstl    (%esp)
	call    builtin_d2i
	add     $(3*4),%esp
	ret

asm_builtin_f2l:
	sub     $(3*4),%esp
	fsts    (%esp)
	call    builtin_f2l
	add     $(3*4),%esp
	ret

asm_builtin_d2l:
	sub     $(3*4),%esp
	fstl    (%esp)
	call    builtin_d2l
	add     $(3*4),%esp
	ret


/* asm_compare_and_swap ********************************************************

   Does an atomic compare and swap.  Required for the lock
   implementation.

   Atomically do the following: Check if the location still contains
   `oldval`. If so, replace it by `newval` and return `oldval`.

   RETURN VALUE:
       the old value at *p

   long compare_and_swap(volatile long *p, long oldval, long newval);

*******************************************************************************/

asm_compare_and_swap:
	mov     1*4(sp),%ecx            /* load p into a register                 */
	mov     2*4(sp),%eax            /* load oldval into return register       */
	mov     3*4(sp),%edx            /* load newval into a register            */
	lock; cmpxchgl %edx,0(%ecx)
	ret


/* asm_memory_barrier **********************************************************

   A memory barrier for the Java Memory Model.

*******************************************************************************/

asm_memory_barrier:
	lock; add $0,0(sp)
	ret

		
asm_getclassvalues_atomic:
_crit_restart2:
	mov     4(%esp),%ecx        /* super */
	mov     8(%esp),%edx        /* sub */
_crit_begin2:
	mov     offbaseval(%ecx),%eax
	mov     offdiffval(%ecx),%ecx
	mov     offbaseval(%edx),%edx
_crit_end2:
	push    %ebx
	mov     16(%esp),%ebx      /* out */
	mov     %eax,offcast_super_baseval(%ebx)
	mov     %ecx,offcast_super_diffval(%ebx)
	mov     %edx,offcast_sub_baseval(%ebx)
	pop     %ebx
	ret

	.data

asm_criticalsections:
#if defined(ENABLE_THREADS)
#if 0
	.long   _crit_begin1
	.long   _crit_end1
	.long   _crit_restart1
#endif
	.long   _crit_begin2
	.long   _crit_end2
	.long   _crit_restart2
#endif
	.long 0


/* asm_get_cycle_count *********************************************************

   Get the current time-stamp counter from the CPU.

*******************************************************************************/

asm_get_cycle_count:
	rdtsc
	ret


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

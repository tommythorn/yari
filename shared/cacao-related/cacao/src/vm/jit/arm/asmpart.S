/* src/vm/jit/arm/asmpart.S - Java-C interface functions for ARM

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

   $Id: asmpart.S 7763 2007-04-19 12:19:19Z michi $

*/


#include "config.h"

#include "vm/jit/arm/offsets.h"
#include "vm/jit/arm/md-asm.h"

#include "vm/jit/methodheader.h"


	.file "asmpart.S"
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

	.globl asm_handle_exception
	.globl asm_handle_nat_exception

	.globl asm_abstractmethoderror

	.globl asm_patcher_wrapper

	.globl asm_cacheflush

	.globl asm_getclassvalues_atomic
	.globl asm_criticalsections


#if !defined(ENABLE_THREADS)
asm_exceptionptr:
	.word _no_threads_exceptionptr
#endif

asm_jitcompilerptr:
	.word asm_call_jit_compiler

asm_criticalsections:
#if defined(ENABLE_THREADS)
	.word _crit_begin
	.word _crit_end
	.word _crit_restart
#endif
	.word 0


/* asm_vm_call_method **********************************************************

   This function calls a Java-method (which possibly needs compilation)
   with up to 4 address parameters.

   This functions calls the JIT-compiler which eventually translates the
   method into machine code.

*******************************************************************************/

	.align  2

	.word   0                           /* catch type all                     */
	.word   0                           /* handler pc                         */
	.word   0                           /* end pc                             */
	.word   0                           /* start pc                           */
	.word   1                           /* extable size                       */
	.word   0                           /* line number table start            */
	.word   0                           /* line number table size             */
	.word   0                           /* FltSave                            */
	.word   0                           /* IntSave                            */
	.word   0                           /* IsLeaf                             */
	.word   0                           /* IsSync                             */
	.word   0                           /* FrameSize                          */
	.word   0                           /* CodeinfoPointer                    */

asm_vm_call_method:
asm_vm_call_method_int:
asm_vm_call_method_long:
/* asm_vm_call_method_float:
asm_vm_call_method_double: */
	SAVE_SCRATCH_REGISTERS            /* save our personal scratch regs   */
	stmfd sp!, {v1}                   /* V1 is used to recompute SP ...   */
	mov   v1, #0                      /* ... when using stack arguments   */
	ldr   ip, asm_jitcompilerptr
	str   ip, [sp, #-4]!              /* store fake address               */
	mov   mptr, sp                    /* set method pointer               */

	mov   itmp1, a0                   /* pass methodinfo* via ITMP1       */

	cmp   a1, #0                      /* do we have arguments?            */
	ble   asm_calljava_copyfinish     /* no -> do not care :-)            */

	/* REMEMBER: stack space for arguments is reserved here! */
	/* TODO: we possibly reserve to much here */
	mov   v1, a1, lsl #3              /* how much stack do we alloc?      */
	sub   sp, sp, v1                  /* allocate stack for arguments!    */

	mov   itmp3, #0                   /* stack position                   */
asm_calljava_copyloop:                /* reorder stack arguments!         */
#if defined(__ARMEL__)
	ldr   ip, [a2,#offvmargdata]      /* get LOW word of argument         */
	str   ip, [sp, itmp3]
	add   itmp3, itmp3, #4
	ldr   ip, [a2,#offvmargtype]      /* is it a 2_WORD_TYPE?             */
	tst   ip, #1
	ldrne ip, [a2,#offvmargdata + 4]  /* yes -> get HIGH word of argument */
	strne ip, [sp, itmp3]
	addne itmp3, itmp3, #4
#else /* defined(__ARMEB__) */
	ldr   ip, [a2,#offvmargtype + 4]  /* get our item type (it is u8)    */
	teq   ip, #2                      /* is it a TYPE_FLOAT?              */
	ldreq ip, [a2,#offvmargdata]      /* yes -> get LOW word of float     */
	streq ip, [sp, itmp3]
	addeq itmp3, itmp3, #4
	beq   asm_calljava_copydone
	tst   ip, #1                      /* is it a 2_WORD_TYPE?             */
	ldrne ip, [a2,#offvmargdata]      /* yes -> get HIGH word of argument */
	strne ip, [sp, itmp3]
	addne itmp3, itmp3, #4
	ldr   ip, [a2,#offvmargdata + 4]  /* get LOW word of argument         */
	str   ip, [sp, itmp3]
	add   itmp3, itmp3, #4
asm_calljava_copydone:
#endif
	add   a2, a2, #sizevmarg          /* next argument block              */
	subs  a1, a1, #1
	bgt   asm_calljava_copyloop

	/* REMEMBER: first four args are passed in regs, take them out again  */
	ldmfd sp, {a0, a1, a2, a3}        /* load first four args to register */
	cmp   v1, #16                     /* do we have four arguments?       */
	addlt sp, sp, v1
	movlt v1, #0
	addge sp, sp, #16
	subge v1, v1, #16

asm_calljava_copyfinish:
	/* REMEMBER: do the method call just like in java! */
	ldr   ip, [mptr]                  /* fake virtual function call       */
	mov   lr, pc 
 	mov   pc, ip 
fake2:
	sub   ip, pc, #(fake2 - asm_vm_call_method)+8

	add   sp, sp, v1                  /* free stack arguments!            */
	add   sp, sp, #4                  /* free fake address                */
	ldmfd sp!, {v1}
	RESTORE_SCRATCH_REGS_AND_RETURN   /* return to caller, restore regs   */

asm_vm_call_method_exception_handler:
	mov   a0, xptr                    /* exception pointer is arg1        */
	bl    builtin_throw_exception     /* throw the exception              */
	mov   res1, #0                    /* return NULL                      */
	mov   res2, #0                    /* return NULL                      */
	add   sp, sp, v1                  /* free stack arguments!            */
	add   sp, sp, #4                  /* free fake address                */
	ldmfd sp!, {v1}
	RESTORE_SCRATCH_REGS_AND_RETURN   /* return to caller, restore regs   */

asm_vm_call_method_float:
	mov a0,#0x51
	b asm_debug
asm_vm_call_method_double:
	mov a0,#0x52
	b asm_debug

asm_vm_call_method_end:


/****************** function asm_call_jit_compiler *****************************
*                                                                              *
*   Invokes the compiler for untranslated JavaVM methods.                      *
*   What this method does:                                                     *
*    - save args and LR                                                        *
*    - fire up jit_compile (pass methodinfo pointer)                           *
*    - try to find out where to write back the new method pointer              *
*    - restore args and LR                                                     *
*    - check for exceptions                                                    *
*    - eventually write back new method pointer                                *
*    - call jit code (wich will then return to caller)                         *
*                                                                              *
*   These methods can call us: codegen_compilerstub & asm_calljavafunction     *
*   ATTENTION: use REG_ITMP1 to pass methodinfo pointer to me!                 *
*                                                                              *
*******************************************************************************/

#define MYSTACKSIZE (6*4)

asm_call_jit_compiler:
	SAVE_ARGUMENT_REGISTERS             /* save our argument registers & LR   */
	sub   sp, sp, #4                    /* keep stack 8-byte aligned          */

	mov   a0, itmp1                     /* pass methodinfo pointer            */
	mov   a1, mptr                      /* pass method pointer                */
	add   a2, sp, #MYSTACKSIZE          /* pass Java sp                       */
	mov   a3, lr                        /* pass Java RA (correct for leafs)   */
	bl    jit_asm_compile
	mov   itmp1, res1                   /* save pointer to new jit-code       */

	tst   itmp1,itmp1                   /* check for exeption                 */
	beq   L_asm_call_jit_compiler_exception

	add   sp, sp, #4                    /* keep stack 8-byte aligned          */
	RESTORE_ARGUMENT_REGISTERS          /* load our argument registers & LR   */

	mov   ip, itmp1
	mov   pc, ip                        /* call jit-code                      */

L_asm_call_jit_compiler_exception:
	bl    exceptions_get_and_clear_exception
	mov   xptr, res1                    /* get exception                      */

	add   sp, sp, #4                    /* keep stack 8-byte aligned          */
	RESTORE_ARGUMENT_REGISTERS          /* load LR                            */

	sub   xpc, lr, #4                   /* xpc = instruction that called us   */
	b     asm_handle_nat_exception


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
	/*TODO:maybe make a macro out of it!!!*/
	SAVE_ARGUMENT_REGISTERS  
	mov   a0, lr
	bl    md_codegen_get_pv_from_pc
	mov   ip, res1
	RESTORE_ARGUMENT_REGISTERS  
	/* fall through */

asm_handle_exception:
	stmfd sp!, {r0 - r3}               /* save possible used registers    */
	mov   itmp3, #1                    /* set maybe-leaf flag             */
	mov   a3, #(4*4)                   /* prepare a3 for handle_exception */

asm_handle_exception_loop:
	stmfd sp!, {ip,lr}                 /* call exception helper here!     */
	mov   a0, xptr                     /* pass exception pointer          */
	mov   a1, xpc                      /* pass exception pointer          */
	mov   a2, ip                       /* pass data segment pointer       */
	add   a3, sp, a3                   /* calculate Java sp into a3...    */
	add   a3, a3, #(2*4)
	bl    exceptions_handle_exception
	ldmfd sp!, {ip,lr}

	tst   a0, a0
	beq   asm_handle_exception_not_catched

	mov   xpc, a0                      /* move handlerpc into xpc         */
	tst   itmp3,itmp3                  /* if this is a lead method ...    */
	ldmnefd sp!, {r0 - r3}             /* restore argument registers      */

	mov   pc, xpc                      /* jump to handler                 */

asm_handle_exception_not_catched:
	tst   itmp3,itmp3                  /* if this is a lead method ...    */
	addne sp, sp, #(4*4)               /* remove maybe-leaf stackframe    */
	movne itmp3, #0                    /* remove maybe-leaf flag          */

	ldr   a2, [ip, #FrameSize]         /* t2 = frame size                 */
	add   a0, sp, a2                   /* t0 = pointer to save area       */
	ldr   a1, [ip, #IsLeaf]            /* t1 = is leaf procedure          */
	tst   a1, a1                       /* if is leaf ...                  */
	ldreq lr, [a0, #-4]!               /* ... restore RA                  */
	mov   xpc, lr                      /* the new xpc is RA               */

	ldr   a1, [ip, #IntSave]           /* t1 = saved int register count   */
	rsb   a1, a1, #5                   /* t1 = count of unsaved registers */
	sub   a1, a1, #1
	add   pc, pc, a1, lsl #2           /* do not load unsaved registers   */
	ldr   v1, [a0, #-20]               /* ... but restore the other ones  */
	ldr   v2, [a0, #-16]
	ldr   v3, [a0, #-12]
	ldr   v4, [a0, #- 8]
	ldr   v5, [a0, #- 4]

	add   sp, sp, a2                   /* unwind stack (using t2)         */
	mov   a3, #0                       /* prepare a3 for handle_exception */

	/*TODO:maybe make a macro out of it!!!*/
	SAVE_ARGUMENT_REGISTERS  
	mov   a0, lr
	bl    md_codegen_get_pv_from_pc
	mov   ip, res1
	RESTORE_ARGUMENT_REGISTERS  

	b     asm_handle_exception_loop


/* asm_patcher_wrapper *********************************************************
*                                                                              *
*   TODO: document me                                                          *
*                                                                              *
*   Stack layout when calling patcher function:                                *
*    28   empty because stack needs to be aligned                              *
*    24   saved REG_ITMP3, should be restored                ( -8)             *
*    20   data segment displacement from load instructions   (-12)             *
*    16   return address into JIT code (patch position)      (-16)             *
*    12   pointer to virtual java_objectheader                                 *
*     8   machine code (which is patched back later)                           *
*   [ 8   result of patcher function (indicates exception)  ]                  *
*     4   unresolved class/method/field reference                              *
*   [ 0   patcher function pointer to call                  ]                  *
*     0   saved IP of caller (caller needs it!)                                *
*                                                                              *
*******************************************************************************/

#define PATCHSTACKSIZE 8*4

asm_patcher_wrapper:
	mov   itmp3, sp                     /* preserve original SP in ITMP3      */

	SAVE_ARGUMENT_REGISTERS_IP          /* save our argument registers & LR   */
	SAVE_FLOAT_REGISTERS                /* save our float registers here      */

	mov   a0, itmp3                     /* pass SP of patcher stub            */
	mov   a1, ip                        /* pass PV                            */
	mov   a2, lr                        /* pass RA (correct for leafs)        */
	bl    patcher_wrapper
	mov   itmp3, res1                   /* save return value                  */

	RESTORE_FLOAT_REGISTERS             /* restore our float registers here   */
	RESTORE_ARGUMENT_REGISTERS_IP       /* load our argument registers & LR   */

	add   sp, sp, #PATCHSTACKSIZE       /* remove patcher stack frame         */

	tst   itmp3, itmp3                  /* check for an exception             */
	bne   L_asm_patcher_wrapper_exception

	ldr   itmp3, [sp, #-8]              /* restore ITMP3 for calling method   */
	ldr   pc, [sp, #-16]                /* jump to new patched code           */

L_asm_patcher_wrapper_exception:
	mov   xptr, itmp3                   /* get exception                      */
	ldr   xpc, [sp, #-16]               /* RA is xpc                          */

	/* Note: A normal branch instruction could modify the PV here,            */
	/*       so we use this construct instead.                                */
	ldr   pc, [pc, #-4]
	.word asm_handle_exception


/* asm_abstractmethoderror *****************************************************

   Creates and throws an AbstractMethodError.

*******************************************************************************/

asm_abstractmethoderror:
	stmfd sp!, {lr}                     /* save return address                */
	add   a0, sp, #(1*4)                /* pass java sp                       */
	mov   a1, lr                        /* pass exception address             */
	bl    exceptions_asm_new_abstractmethoderror
	ldmfd sp!, {lr}                     /* restore return address             */

	mov   xptr, res1                    /* get exception pointer              */
	sub   xpc, lr, #4                   /* exception address is ra - 4        */
	b     asm_handle_nat_exception

		
/********************* function asm_cacheflush *********************************
*                                                                              *
*   TODO: document me                                                          *
*                                                                              *
*   void asm_cacheflush(void *p, s4 size);                                     *
*                                                                              *
*******************************************************************************/

#if 1
.equ sys_cacheflush, 0x9f0002
asm_cacheflush:
	add   a1, a0, a1
	mov   a2, #0

#if 0
	/* TWISTI: required on iyonix, maybe a linux-2.4 bug */
	/* TODO: repeair this! */
	/* cacheflush is messed up beyond all repair! */
	mov a0, #0x0
	mov a1, #0xff000000
#endif

	swi   #sys_cacheflush
	mov   pc, lr
#endif


/********************* function asm_getclassvalues_atomic *********************/

asm_getclassvalues_atomic:
	stmfd sp!, {r4, r5, r6}
_crit_restart:
_crit_begin:
	ldr   r4,[a0,#offbaseval]
	ldr   r5,[a0,#offdiffval]
	ldr   r6,[a1,#offbaseval]
_crit_end:
	str   r4,[a2,#offcast_super_baseval]
	str   r5,[a2,#offcast_super_diffval]
	str   r6,[a2,#offcast_sub_baseval]
	ldmfd sp!, {r4, r5, r6}
	mov   pc, lr


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

/* src/vm/jit/s390/asmpart.S - Java-C interface functions for s390

   Copyright (C) 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
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

   $Id: asmpart.S 7839 2007-04-29 22:46:56Z pm $

*/


#include "config.h"

#include "vm/jit/s390/arch.h"
#include "vm/jit/s390/md-abi.h"
#include "vm/jit/s390/md-asm.h"
#include "vm/jit/s390/offsets.h"

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

	.globl asm_replacement_out
	.globl asm_replacement_in

	.globl asm_builtin_f2i
	.globl asm_builtin_f2l
	.globl asm_builtin_d2i
	.globl asm_builtin_d2l

	.globl asm_criticalsections
	.globl asm_getclassvalues_atomic


asm_abstractmethoderror:
	.long 0
asm_replacement_out:
	.long 0
asm_replacement_in:
	.long 0
asm_builtin_f2i:
	.long 0
asm_builtin_f2l:
	.long 0
asm_builtin_d2i:
	.long 0
asm_builtin_d2l:
	.long 0

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

/*

a0:   methodinfo *m
a1:   s4 vmargscount   ---> v0: java_objectheader *
a2:   vm_arg *vmargs
r14:  return address

96 ...     on stack parameters (none)
0 - 96     register save area
-------------------------------------------------- <- SP on asm_vm_... entry
           local variables
		   saved return address (important to be at 0(sp) because of md_stacktrace_get_returnaddress)
----------------------------------------- <- SP after stack frame allocation
           arguments on stack
---------------------------------------------------- <- SP on JIT code entry
           saved return address (callee saved)

*/


/*
	Regiser usage:
	itmp1: argument block pointer
	itmp2: argument counter
	s0: integer argument counter
	s1: float argument counter
	s2: integer register counter
	s3: backup argument block pointer
	s4: backup argument count
*/

	stm   %r6, %r15, 24(sp)               /* save callers regiters */
	stm   a0, a2, 8(sp)                   /* save arguments */
	ahi   sp, -8                          /* allocate stack space for local variables */
	st    %r14, 0(sp)                     /* store RA once more at bottom of stack frame */

	ltr   a1, a1                          /* maybe we have no args... */
	je    L_no_args

	lr    itmp2, a1                       /* load arg count */
	lr    itmp1, a2                       /* load arg pointer */

	ahi   itmp1, -sizevmarg               /* initialize arg pointer */
	ahi   itmp2, 1                        /* initialize arg count */ 
	lhi   s0, 0                           /* initialize integer arg counter */
	lhi   s2, 0                           /* initialize integer register counter */
	lhi   s1, 0                           /* initialize float arg counter */

	lr    s4, a1                          /* backup arg count */
	lr    s3, a2                          /* backup arg pointer */

L_register_copy:

	ahi   itmp1, sizevmarg                /* forward arg pointer */
	ahi   itmp2, -1                       /* decrement arg count */
	je    L_register_copy_done            /* no arguments left */

	tm    offvmargtype+7(itmp1), 0x02     /* is this a float/double type? */
	jne   L_register_handle_float

L_register_handle_int:

	chi   s2, INT_ARG_CNT	              /* are we out of integer arg registers ? */
	je    L_register_copy                 /* yes, next loop */

	tm    offvmargtype+7(itmp1), 0x01       /* is this a 2 word type ? */
	jne   L_register_handle_long

	ahi   s0, 1                           /* increment integer arg counter */
	ahi   s2, 1                           /* increment integer register counter */

	/* handle argument */

	chi   s2, 1
	je    L_handle_i0
	chi   s2, 2
	je    L_handle_i1
	chi   s2, 3
	je    L_handle_i2
	chi   s2, 4
	je    L_handle_i3
	chi   s2, 5
	je    L_handle_i4

L_register_handle_long:

	chi   s2, (INT_ARG_CNT - 1)           /* are there 2 integer arg registers left ? */
	jl    L_register_handle_long_continue /* yes */
	lhi   s2, INT_ARG_CNT                 /* no, drop last register */
	j     L_register_copy

L_register_handle_long_continue:

	ahi   s0, 1                           /* increment integer arg counter */
	ahi   s2, 2                           /* consume 2 integer arg registers */

	/* handle argument */

	chi   s2, 2
	je    L_handle_l0
	chi   s2, 3
	je    L_handle_l1
	chi   s2, 4
	je    L_handle_l2
	chi   s2, 5
	je    L_handle_l3

L_register_handle_float:

	chi   s1, FLT_ARG_CNT                 /* are we out of float arg registers */
	je    L_register_copy                 /* no arg regisers left */

	ahi   s1, 1                           /* increment float argument counter */

	tm    offvmargtype+7(itmp1), 0x01     /* is this a 2 word type ? */
	jne   L_register_handle_double

	/* handle argument */

	chi   s1, 1
	je    L_handle_f0
	chi   s1, 2
	je    L_handle_f1

L_register_handle_double:

	/* handle argument */

	chi   s1, 1
	je    L_handle_d0
	chi   s1, 2
	je    L_handle_d1

L_register_copy_done:

/*
	Regiser usage:
	itmp1: argument block pointer
	itmp2: argument counter
	s0: integer argument counter (initialized by previous code) 
	s1: float argument counter (initialized by previous code)
	s2: pointer to current argument on stack
	s3: backup argument block pointer (used to initialize itmp1)
	    after used as backup of original stack pointer
	s4: backup argument count (used to initialize itmp2)
	    after used as size of parameters on stack
*/

	lr    itmp2, s4                     /* restore argument counter */
	lr    itmp1, s3                     /* restore argument block pointer */

	                                    /* calculate remaining arguments */
	sr    s4, s0                        /* - integer arguments in registers */
	sr    s4, s1                        /* - float arguments in registers */

	lr    s3, sp                        /* backup stack pointer (does not alter CC) */

	je    L_copy_done                   /* no arguments left for stack */

	sll   s4, 3                         /* allocate 8 bytes per parameter on stack */	
	sr    sp, s4                        /* allocate stack space for arguments */ 

	lr    s2, sp                        /* points now to current argument on stack */

	ahi   itmp1, -sizevmarg             /* initialize argument block pointer */
	ahi   itmp2, 1                      /* initialize argument counter */

L_stack_copy_loop:

	ahi   itmp1, sizevmarg              /* forward argument block pointer */
	ahi   itmp2, -1                     /* decrement argument counter */
	je    L_copy_done                   /* all arguments done */

	tm    offvmargtype+7(itmp1), 0x02   /* is this a float/double type? */
	jne   L_stack_handle_float

L_stack_handle_int:

	ahi   s0, -1                         /* decrement number of integer arguments in regs */
	jhe   L_stack_copy_loop              /* argument is in register */

	tm    offvmargtype+7(itmp1), 0x01    /* is this a 2 word type ? */
	jne   L_stack_handle_long

	mvc   0(4, s2), offvmargdata+4(itmp1)/* copy integer value */
	ahi   s2, 4
	j     L_stack_copy_loop

L_stack_handle_long:

	mvc   0(8, s2), offvmargdata(itmp1)  /* copy long value */
	ahi   s2, 8
	j     L_stack_copy_loop

L_stack_handle_float:

	ahi   s1, -1                         /* decrement number of float arguments in regs */
	jhe   L_stack_copy_loop              /* argument is in register */

	tm    offvmargtype+7(itmp1), 0x01    /* is this a 2 word type ? */
	jne   L_stack_handle_double

	mvc   0(4, s2), offvmargdata(itmp1)  /* copy float value */
	ahi   s2, 4
	j     L_stack_copy_loop

L_stack_handle_double:

	mvc   0(8, s2), offvmargdata(itmp1)  /* copy double value */
	ahi   s2, 8
	j     L_stack_copy_loop

L_copy_done:

	/* Now we call the compiler in a rather questionable way i needed
	 * some days to understand:
	 *
	 * We can't simply call asm_call_jit_compiler, but we have to call an 
	 * address loaded from memory like it is done in JIT code.
	 *
	 * This is because the compiler will intercept the instruction before 
	 * the call instruction, extract the address where the function pointer
	 * has been loaded from and overwrite it with the code entry.
	 *
	 * Arguments are passed in temporary registers.
	 */

	/* load address of L_asm_call_jit_compiler into memory */

	basr  mptr, 0                         /* store PC */
L_basr:
	la    mptr, L_asm_call_jit_compiler-L_basr(mptr) /* add offset to PC */
	st    mptr, 4(s3)                     /* store on stack */

	l     itmp1, 8+8(s3)                  /* load methodinfo for compiler */
	la    mptr, 4(s3)                     /* store **function in mptr for compiler */

	/* call L_asm_call_jit_compiler like JIT code would do */

	l     itmp3, 0(mptr)                  /* load address of target from memory */
	basr  %r14, itmp3                     /* jump to target */

	/* todo will s4 survive the call? */
	ar    sp, s4                          /* remove stack space for arguments */

L_asm_vm_call_method_return:

	ahi   sp, 8                           /* remove stack space for local variables */
	lm    %r6, %r15, 24(sp)               /* restore callers registers */
	br    %r14                            /* return */

asm_vm_call_method_exception_handler:
	lr    a0, xptr

	bras  %r14, L_avcmeh_bras
	.long builtin_throw_exception
L_avcmeh_bras:
	l     %r14, 0(%r14)
	ahi   sp, -96
	basr  %r14, %r14
	ahi   sp, 96

	j     L_asm_vm_call_method_return

/* .... */

L_no_args:
	lr    s3, sp
	lhi   s4, 0
	j     L_copy_done

L_handle_i0:
	l     a0, offvmargdata+4(itmp1)
	j     L_register_copy
L_handle_i1:
	l     a1, offvmargdata+4(itmp1)
	j     L_register_copy
L_handle_i2:
	l     a2, offvmargdata+4(itmp1)
	j     L_register_copy
L_handle_i3:
	l     a3, offvmargdata+4(itmp1)
	j     L_register_copy
L_handle_i4:
	l     a4, offvmargdata+4(itmp1)
	j     L_register_copy

L_handle_l0:
	lm    a0, a1, offvmargdata(itmp1)
	j     L_register_copy
L_handle_l1:
	lm    a1, a2, offvmargdata(itmp1)
	j     L_register_copy
L_handle_l2:
	lm    a2, a3, offvmargdata(itmp1)
	j     L_register_copy
L_handle_l3:
	lm    a3, a4, offvmargdata(itmp1)
	j     L_register_copy

L_handle_f0:
	le    fa0, offvmargdata(itmp1)
	j     L_register_copy
L_handle_f1:
	le    fa1, offvmargdata(itmp1)
	j     L_register_copy

L_handle_d0:
	ld    fa0, offvmargdata(itmp1)
	j     L_register_copy
L_handle_d1:
	ld    fa1, offvmargdata(itmp1)
	j     L_register_copy

asm_vm_call_method_end:
	brc   0,0	

/****************** function asm_call_jit_compiler *****************************
*                                                                              *
*   invokes the compiler for untranslated JavaVM methods.                      *
*                                                                              *
*   itmp1: methodinfo pointer                                                  *
*   itmp2: method pointer                                                      *
*                                                                              *
*******************************************************************************/

/*

argument registers: arguments (like in JIT)

        arguments on stack (like in JIT)
------------------------------------------------------------- <- SP on entry

        saved return address                                           \
        stored volatile (in terms of C ABI) floag argument registers   | 
96      stored volatile (in terms of C ABI) integer argument registers | ACJC_STACKFRAME
0 - 96  register save area (C ABI)                                     /
-------------------------------------------------- <- SP for jit_asm_compile
*/

/* This is called from a compiler stub.
 * Arguments are already in registers and the stack is setup like in CACAO.
 */

asm_call_jit_compiler:
L_asm_call_jit_compiler:

#	define ACJC_STACKFRAME (4 + (4 * 4) + (2 * 8) + 96)

	ahi     sp,-ACJC_STACKFRAME        /* allocate stack space */

	stm	    %r2,%r5,96(sp)             /* store volatile int arg regs */
	std     %f0,96+16(sp)              /* store volatile float arg regs */
	std     %f2,96+24(sp)              
	st      %r14,96+32(sp)             /* store return address */

	/* load arguments */

	lr      a0,itmp1                   /* pass methodinfo pointer            */
	lr      a1,itmp2                   /* pass method pointer                */
	la      a2,ACJC_STACKFRAME(sp)     /* pass java sp                       */
	la      a3,0(%r14)                 /* pass return address, make sure bit 32 is 0 */

	/* call jit_asm_compile in a PIC way */

	bras    itmp2, L_bras_jac
	.long   jit_asm_compile
L_bras_jac:
	l       itmp2, 0(itmp2)
	basr    %r14, itmp2

	lr      pv, v0                     /* save return value */

	lm      %r2,%r5,96(sp)             /* restore volatile int arg regs */
	ld      %f0,96+16(sp)              /* restore volatile float arg regs */
	ld      %f2,96+24(sp)              /* restore volatile float arg regs */

	ltr     pv,pv
	je      L_asm_call_jit_compiler_exception

	l       %r14,96+32(sp)             /* restore return address */
	ahi     sp, ACJC_STACKFRAME        /* remove stack frame */

jit_code_entry:                        /* label to set breakpoint on */
	br      pv                         /* call the method, it will return to the caller */


L_asm_call_jit_compiler_exception:
	bras    itmp2, L_bras_acjce
	.long exceptions_get_and_clear_exception
L_bras_acjce:
	l       itmp2, 0(itmp2)
	basr    %r14, itmp2
	lr      xptr, %r2
	l       xpc,96+32(sp)              /* restore return address */
	ahi     sp, ACJC_STACKFRAME        /* remove stack frame */
	j       L_asm_handle_nat_exception


#if 0
/* asm_handle_exception ********************************************************
*                                                                              *
*   This function handles an exception. It does not use the usual calling      *
*   conventions. The exception pointer is passed in REG_ITMP1 and the          *
*   pc from the exception raising position is passed in REG_ITMP2. It searches *
*   the local exception table for a handler. If no one is found, it unwinds    *
*   stacks and continues searching the callers.                                *
*                                                                              *
*******************************************************************************/

#endif

asm_handle_nat_exception:
L_asm_handle_nat_exception:
	/* TODO really nothing here ? */
asm_handle_exception:
L_asm_handle_exception:                 /* required for PIC code              */

	ahi     sp, -(ARGUMENT_REGISTERS_SIZE + TEMPORARY_REGISTERS_SIZE) /* create maybe-leaf stackframe */
	STORE_ARGUMENT_REGISTERS(0)
	STORE_TEMPORARY_REGISTERS(ARGUMENT_REGISTERS_SIZE)	
	lhi     a3, (ARGUMENT_REGISTERS_SIZE + TEMPORARY_REGISTERS_SIZE) /* prepare a3 for handle_exception */

	lhi     %r0, 1                      /* set maybe-leaf flag */

L_asm_handle_exception_stack_loop:
	ahi     sp,-(6*4)
	st      xptr,0*4(sp)                /* save exception pointer             */
	st      xpc,1*4(sp)                 /* save exception pc                  */
	la      a3,(6*4)(a3,sp)             /* calculate Java sp into a3...       */
	st      a3,3*4(sp)                  /* ...and save it                     */
	st      %r0,4*4(sp)                 /* save maybe-leaf flag               */

	lr      a0,xpc                      /* exception pc                       */

	ahi     sp,-96                      /* add register save area for C code */

	bras    %r14,L_ahe_bras             /* call codegen_get_pv_from_pc */
	.long   codegen_get_pv_from_pc
L_ahe_bras:
	l       %r14,0(%r14)
	basr    %r14,%r14
	st      v0,2*4+96(sp)               /* save data segment pointer          */
    
	lr      a2,v0                       /* pass data segment pointer          */
	l       a0,0*4+96(sp)               /* pass exception pointer             */
	l       a1,1*4+96(sp)               /* pass exception pc                  */
	l       a3,3*4+96(sp)               /* pass Java stack pointer            */

	bras    %r14,L_ahe_bras2            /* call exceptions_handle_exception */
	.long   exceptions_handle_exception
L_ahe_bras2:
	l       %r14,0(%r14)
	basr    %r14,%r14

	ahi     sp,96                       /* remove regiser save area for C code */

	ltr     v0,v0
	jz      L_asm_handle_exception_not_catched

	lr      xpc,v0                      /* move handlerpc into xpc            */
	l       xptr,0*4(sp)                /* restore exception pointer          */
	l       pv,2*4(sp)                  /* restore PV                         */
	l       %r0,4*4(sp)                 /* get maybe-leaf flag                */
	ahi     sp,(6*4)                    /* free stack frame                   */

	ltr     %r0, %r0
	jz      L_asm_handle_exception_no_leaf

	LOAD_ARGUMENT_REGISTERS(0)
	LOAD_TEMPORARY_REGISTERS(ARGUMENT_REGISTERS_SIZE)	

	ahi     sp, (ARGUMENT_REGISTERS_SIZE + TEMPORARY_REGISTERS_SIZE) /* Remove maybe-leaf stackframe */

L_asm_handle_exception_no_leaf:
	br      xpc                         /* jump to the handler */

L_asm_handle_exception_not_catched:
	l       xptr,0*4(sp)                /* restore exception pointer          */
	l       itmp3,2*4(sp)               /* restore data segment pointer       */
	ahi     itmp3,-0xfff                /* for negative displacements */
	l       %r0,4*4(sp)                 /* get maybe-leaf flag                */
	ahi     sp,(6*4)

	ltr     %r0,%r0
	jz      L_asm_handle_exception_no_leaf_stack

	ahi     sp, (ARGUMENT_REGISTERS_SIZE + TEMPORARY_REGISTERS_SIZE) /* Remove maybe-leaf stackframe */
	lhi     %r0,0                       /* clear the isleaf flag              */

	/*
	+-----------------+-----------+---------+----+
	| Memuse          | Float Sav | Int Sav | RA |
	|                 | 0 ... n   | 0 ... n |    |
	+-----------------+-----------+---------+----+
	^                 ^           ^
	SP                F           I

	                              ^         ^    ^
                                  p3        p2   p1	
	*/

L_asm_handle_exception_no_leaf_stack:

	l       itmp2,0xfff+FrameSize(itmp3)/* get frame size                     */
	la      itmp2,0(itmp2,sp)           /* pointer to save area (p1) */
	ahi     itmp2,-4                    /* skip RA (p2) */
	ahi     itmp2,-0xfff                /* for negative displacements */

	l       a0,0xfff+IntSave(itmp3)     /* a0 = saved int register count  */

	ltr     a0,a0
	je      noint

	chi     a0,1
	je      int1
	chi     a0,2
	je      int2
	chi     a0,3
	je      int3
	chi     a0,4
	je      int4
	
int5:
	l       s0,0xfff-5*4(itmp2)
int4:   
	l       s1,0xfff-4*4(itmp2)
int3:   
	l       s2,0xfff-3*4(itmp2)
int2:   
	l       s3,0xfff-2*4(itmp2)
int1:   
	l       s4,0xfff-1*4(itmp2)

noint:

	sll     a0,2                        /* a0 = saved int register count * 4 */
	sr      itmp2, a0                   /* skip Int Sav (p3) */

	l       a0,0xfff+FltSave(itmp3)
	ltr	    a0,a0                       /* Number of saved floating point registers */
	je      noflt

	chi     a0,1
	je      flt1

flt2:   
	ld    %f4,0xfff-2*8(itmp2)
flt1:   
	ld    %f6,0xfff-1*8(itmp2)
                
noflt:

	l       itmp3,0xfff+FrameSize(itmp3)/* get frame size (at least 4 - RA)   */
	ahi     itmp3,-4                    /* substract 4 */
	l       xpc,0(itmp3,sp)             /* load the new xpc -  return address */
	la      sp, 4(itmp3,sp)             /* unwind stack                       */

	                                    /* exception pointer is still set     */
#if 0
	sub     $3,xpc                      /* subtract 3 bytes for call          */
#endif

	lhi     a3,0                        /* prepare a3 for handle_exception    */
	
	j		L_asm_handle_exception_stack_loop


#if 0

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

#endif

/* asm_patcher_wrapper *********************************************************

   XXX

   Stack layout:
     20   return address into JIT code (patch position)
     16   pointer to virtual java_objectheader
     12   machine code (which is patched back later)
      8   unresolved class/method/field reference
      4   data segment displacement from load instructions
      0   patcher function pointer to call (pv afterwards)

*******************************************************************************/

asm_patcher_wrapper:
#	define  apw_sfs (96 + 4 + VOLATILE_INTEGER_REGISTERS_SIZE + VOLATILE_FLOAT_REGISTERS_SIZE)

	ahi     sp, -apw_sfs                /* create stack frame */

	/* store all volatile registers and a2, because we will touch it */

	st      a2, 96(sp)
	STORE_VOLATILE_INTEGER_REGISTERS(96 + 4)
	STORE_VOLATILE_FLOAT_REGISTERS(96 + 4 + VOLATILE_INTEGER_REGISTERS_SIZE)

	/* pass arguments */

	la      a0, apw_sfs(sp)             /* pass SP of patcher stub */
	lr      a1, pv                      /* pass PV (if NULL, use findmethod)  */
	lhi     a2, 0                       /* pass RA                            */

	/* call patcher_wrapper */

	bras    itmp1, L_apw_bras           /* call patcher_wrapper */
	.long   patcher_wrapper
L_apw_bras:
	l       itmp1, 0(itmp1)
	basr    %r14, itmp1

	/* store return value */

	st      v0,0(sp)                    /* save return value */

	/* restore volatile registers */

	l       a2, 96(sp)
	LOAD_VOLATILE_INTEGER_REGISTERS(96 + 4)
	LOAD_VOLATILE_FLOAT_REGISTERS(96 + 4 + VOLATILE_INTEGER_REGISTERS_SIZE)

	l       itmp3, 0(sp)                /* restore return value */
	ltr     itmp3, itmp3                /* exception thrown ? */
	jne     L_asm_patcher_wrapper_exception /* handle exception */
	l       itmp3, apw_sfs + (5 * 4)(sp) /* load return address to JIT from stack */
	ahi     sp, apw_sfs + (6 * 4)       /* remove stack frame, and stack frame by patcher stub */
	br      itmp3                       /* return */

L_asm_patcher_wrapper_exception:
	lr      xptr,itmp3                  /* get exception                      */
	l       xpc, apw_sfs + (5 * 4)(sp)  /* load return address to JIT from stack */
	ahi     sp, apw_sfs + (6 * 4)       /* remove stack frame, and stack frame by patcher stub */
	j       L_asm_handle_exception

#if 0

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
      void asm_replacement_in(executionstate *es);

*******************************************************************************/

asm_replacement_in:
	mov     a0,%rbp                     /* executionstate *es                 */

	/* set new sp */
	mov     (offes_sp)(%rbp),%rsp
	
	/* store address of new code */
	push    (offes_pc)(%rbp)
	
	/* copy registers from execution state */
	movq    (XMM0 *8+offes_fltregs)(%rbp),%xmm0
	movq    (XMM1 *8+offes_fltregs)(%rbp),%xmm1
	movq    (XMM2 *8+offes_fltregs)(%rbp),%xmm2
	movq    (XMM3 *8+offes_fltregs)(%rbp),%xmm3
	movq    (XMM4 *8+offes_fltregs)(%rbp),%xmm4
	movq    (XMM5 *8+offes_fltregs)(%rbp),%xmm5
	movq    (XMM6 *8+offes_fltregs)(%rbp),%xmm6
	movq    (XMM7 *8+offes_fltregs)(%rbp),%xmm7
	movq    (XMM8 *8+offes_fltregs)(%rbp),%xmm8
	movq    (XMM9 *8+offes_fltregs)(%rbp),%xmm9
	movq    (XMM10*8+offes_fltregs)(%rbp),%xmm10
	movq    (XMM11*8+offes_fltregs)(%rbp),%xmm11
	movq    (XMM12*8+offes_fltregs)(%rbp),%xmm12
	movq    (XMM13*8+offes_fltregs)(%rbp),%xmm13
	movq    (XMM14*8+offes_fltregs)(%rbp),%xmm14
	movq    (XMM15*8+offes_fltregs)(%rbp),%xmm15

	mov     (RAX*8+offes_intregs)(%rbp),%rax
	mov     (RBX*8+offes_intregs)(%rbp),%rbx
	mov     (RCX*8+offes_intregs)(%rbp),%rcx
	mov     (RDX*8+offes_intregs)(%rbp),%rdx
	mov     (RSI*8+offes_intregs)(%rbp),%rsi
	mov     (RDI*8+offes_intregs)(%rbp),%rdi
	mov     (R8 *8+offes_intregs)(%rbp),%r8
	mov     (R9 *8+offes_intregs)(%rbp),%r9
	mov     (R10*8+offes_intregs)(%rbp),%r10
	mov     (R11*8+offes_intregs)(%rbp),%r11
	mov     (R12*8+offes_intregs)(%rbp),%r12
	mov     (R13*8+offes_intregs)(%rbp),%r13
	mov     (R14*8+offes_intregs)(%rbp),%r14
	mov     (R15*8+offes_intregs)(%rbp),%r15

	mov     (RBP*8+offes_intregs)(%rbp),%rbp

	/* jump to new code */
	ret


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

#endif /* if 0 */

/* TODO use move here ? */

asm_getclassvalues_atomic:
_crit_restart:
_crit_begin:
	l       %r0,offbaseval(a0)
	l       %r1,offdiffval(a0)
	l       a3,offbaseval(a1)
_crit_end:
	st      %r0,offcast_super_baseval(a2)
	st      %r1,offcast_super_diffval(a2)
	st      a3,offcast_sub_baseval(a2)
	br      %r14

	.data
		
asm_criticalsections:
#if defined(ENABLE_THREADS)
	.long   _crit_begin
	.long   _crit_end
	.long   _crit_restart
#endif
	.long 0


/* disable exec-stacks ********************************************************/

#if 0

#if defined(__linux__) && defined(__ELF__)
	.section .note.GNU-stack,"",%progbits
#endif


#endif /* if 0 */

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

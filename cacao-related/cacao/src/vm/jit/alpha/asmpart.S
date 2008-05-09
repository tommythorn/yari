/* src/vm/jit/alpha/asmpart.S - Java-C interface functions for alpha

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

#include "vm/jit/alpha/md-abi.h"
#include "vm/jit/alpha/md-asm.h"
#include "vm/jit/alpha/offsets.h"

#include "vm/jit/abi-asm.h"
#include "vm/jit/methodheader.h"


	.text
	.set    noat
	.set    noreorder


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

	.globl asm_compare_and_swap
	.globl asm_memory_barrier

	.globl asm_criticalsections
	.globl asm_getclassvalues_atomic

	.globl asm_md_init
	.globl asm_cacheflush


/* asm_vm_call_method **********************************************************
*                                                                              *
*   This function calls a Java-method (which possibly needs compilation)       *
*   with up to 4 address parameters.                                           *
*                                                                              *
*   This functions calls the JIT-compiler which eventually translates the      *
*   method into machine code.                                                  *
*                                                                              *
*   C-prototype:                                                               *
*    javaobject_header *asm_calljavafunction (methodinfo *m,                   *
*         void *arg1, void *arg2, void *arg3, void *arg4);                     *
*                                                                              *
*******************************************************************************/

	.ent    asm_vm_call_method

	.align  3

	.quad   0                           /* catch type all                     */
	.quad   0                           /* handler pc                         */
	.quad   0                           /* end pc                             */
	.quad   0                           /* start pc                           */
	.long   1                           /* extable size                       */
	.long   0                           /* ALIGNMENT PADDING                  */
	.quad   0                           /* line number table start            */
	.quad   0                           /* line number table size             */
	.long   0                           /* ALIGNMENT PADDING                  */
	.long   0                           /* fltsave                            */
	.long   1                           /* intsave                            */
	.long   0                           /* isleaf                             */
	.long   0                           /* IsSync                             */
	.long   0                           /* frame size                         */
	.quad   0                           /* codeinfo pointer                   */

asm_vm_call_method:
asm_vm_call_method_int:
asm_vm_call_method_long:
asm_vm_call_method_float:
asm_vm_call_method_double:
	ldgp    gp,0(pv)
	lda     sp,-5*8(sp)               /* allocate stack space                 */
	stq     ra,0*8(sp)                /* save return address                  */
	stq     gp,1*8(sp)                /* save global pointer                  */
	stq     s6,3*8(sp)

	stq     a0,4*8(sp)                /* save method pointer for compiler     */

	mov     a2,t0                     /* pointer to arg block                 */
	mov     a1,s6                     /* arg count                            */

	ble     s6,calljava_argsloaded
	lda     s6,-1(s6)
	ldq     a0,offvmargdata(t0)
	ldt     $f16,offvmargdata(t0)
	ble     s6,calljava_argsloaded

	lda     s6,-1(s6)
	ldq     a1,offvmargdata+sizevmarg*1(t0)
	ldt     $f17,offvmargdata+sizevmarg*1(t0)
	ble     s6,calljava_argsloaded

	lda     s6,-1(s6)
	ldq     a2,offvmargdata+sizevmarg*2(t0)
	ldt     $f18,offvmargdata+sizevmarg*2(t0)
	ble     s6,calljava_argsloaded

	lda     s6,-1(s6)
	ldq     a3,offvmargdata+sizevmarg*3(t0)
	ldt     $f19,offvmargdata+sizevmarg*3(t0)
	ble     s6,calljava_argsloaded

	lda     s6,-1(s6)
	ldq     a4,offvmargdata+sizevmarg*4(t0)
	ldt     $f20,offvmargdata+sizevmarg*4(t0)
	ble     s6,calljava_argsloaded

	lda     s6,-1(s6)
	ldq     a5,offvmargdata+sizevmarg*5(t0)
	ldt     $f21,offvmargdata+sizevmarg*5(t0)
calljava_argsloaded:
	mov     sp,t4
	ble     s6,calljava_nocopy
	negq    s6,t1
	s8addq  t1,sp,sp
	s8addq  t1,t4,t2

calljava_copyloop:
	ldq     t3,offvmargdata+sizevmarg*6(t0)
	stq     t3,0(t2)
	lda     t1,1(t1)
	lda     t0,sizevmarg(t0)
	lda     t2,8(t2)
	bne     t1,calljava_copyloop

calljava_nocopy:
	ldq     itmp1,4*8(t4)             /* pass method pointer via itmp1        */

	lda     mptr,asm_call_jit_compiler/* fake virtual function call (2 instr) */
	stq     mptr,2*8(t4)              /* store function address               */
	lda     mptr,1*8(t4)              /* set method pointer                   */

	ldq     pv,1*8(mptr)              /* method call as in Java               */
	jmp     ra,(pv)                   /* call JIT compiler                    */
calljava_jit2:
	lda     pv,(asm_vm_call_method - calljava_jit2)(ra)

	s8addq  s6,sp,sp
calljava_return2:
	ldq     ra,0*8(sp)                /* restore return address               */
	ldq     gp,1*8(sp)                /* restore global pointer               */
	ldq     s6,3*8(sp)
	lda     sp,5*8(sp)                /* free stack space                     */

calljava_ret2:
	jmp     zero,(ra)

asm_vm_call_method_exception_handler:
	s8addq  s6,sp,sp
	ldq     gp,1*8(sp)                /* restore global pointer               */
	mov     itmp1,a0
	jsr     ra,builtin_throw_exception
	ldq     ra,0*8(sp)                /* restore return address               */
	ldq     s6,3*8(sp)
	lda     sp,5*8(sp)                /* free stack space                     */
asm_vm_call_method_end:					
	jmp     zero,(ra)

	.end    asm_vm_call_method


/* asm_call_jit_compiler *******************************************************

   Invokes the compiler for untranslated Java methods.

*******************************************************************************/

	.ent    asm_call_jit_compiler

asm_call_jit_compiler:
	ldgp    gp,0(pv)
	lda     sp,-(ARG_CNT+2)*8(sp) /* +2: keep stack 16-byte aligned           */

	stq     ra,0*8(sp)            /* save return address                      */

	SAVE_ARGUMENT_REGISTERS(1)    /* save 6 int/6 float argument registers    */

	mov     itmp1,a0              /* pass methodinfo pointer                  */
	mov     mptr,a1               /* pass method pointer                      */
	lda     a2,(ARG_CNT+2)*8(sp)  /* pass java sp                             */
	mov     ra,a3
	jsr     ra,jit_asm_compile    /* call jit compiler                        */
	mov     v0,pv

	ldq     ra,0*8(sp)            /* load return address                      */

	RESTORE_ARGUMENT_REGISTERS(1) /* restore 6 int/6 float argument registers */

	lda     sp,(ARG_CNT+2)*8(sp)  /* remove stack frame                       */

	beq     pv,L_asm_call_jit_compiler_exception

	jmp     zero,(pv)             /* and call method, the method returns      */
	                              /* directly to the caller (ra).             */

L_asm_call_jit_compiler_exception:
	subq    sp,2*8,sp
	stq     ra,0*8(sp)            /* save return address (xpc)                */
	jsr     ra,exceptions_get_and_clear_exception
	ldq     ra,0*8(sp)            /* restore return address (xpc)             */
	addq    sp,2*8,sp

	mov     v0,xptr               /* get exception                            */
	subq    ra,4,xpc              /* exception address is ra - 4              */
	br      L_asm_handle_nat_exception

	.end    asm_call_jit_compiler


/* asm_handle_exception ********************************************************

   This function handles an exception. It does not use the usual calling
   conventions. The exception pointer is passed in REG_ITMP1 and the
   pc from the exception raising position is passed in REG_ITMP2. It searches
   the local exception table for a handler. If no one is found, it unwinds
   stacks and continues searching the callers.

   ATTENTION: itmp3 == gp!

*******************************************************************************/

	.ent    asm_handle_nat_exception

asm_handle_nat_exception:
L_asm_handle_nat_exception:       /* required for PIC code                    */
L_asm_handle_exception_stack_loop:
	lda     sp,-6*8(sp)                 /* keep stack 16-byte aligned         */
	stq     xptr,0*8(sp)                /* save xptr                          */
	stq     xpc,1*8(sp)                 /* save xpc                           */
	stq     ra,3*8(sp)                  /* save RA                            */
	stq     zero,4*8(sp)                /* save maybe-leaf flag (cleared)     */

	mov     ra,a0                       /* pass RA                            */

	br      ra,L_asm_handle_exception_load_gp
L_asm_handle_exception_load_gp:
	ldgp    gp,0(ra)                    /* load gp                            */

	jsr     ra,md_codegen_get_pv_from_pc/* get PV from RA                     */
	stq     v0,2*8(sp)                  /* save PV                            */

	ldq     a0,0*8(sp)                  /* pass xptr                          */
	ldq     a1,1*8(sp)                  /* pass xpc                           */
	mov     v0,a2                       /* pass PV                            */
	addq    sp,6*8,a3                   /* pass Java SP                       */

	br      L_asm_handle_exception_continue

	.aent    asm_handle_exception

asm_handle_exception:
L_asm_handle_exception:                 /* required for PIC code              */
	lda     sp,-(ARG_CNT+TMP_CNT)*8(sp) /* create maybe-leaf stackframe       */

	SAVE_ARGUMENT_REGISTERS(0)          /* we save arg and temp registers in  */
	SAVE_TEMPORARY_REGISTERS(ARG_CNT)   /* case this is a leaf method         */

	lda     sp,-6*8(sp)                 /* keep stack 16-byte aligned         */
	stq     xptr,0*8(sp)                /* save xptr                          */
	stq     pv,2*8(sp)                  /* save PV                            */
	stq     ra,3*8(sp)                  /* save RA                            */
	lda     t0,1(zero)                  /* set maybe-leaf flag                */
	stq     t0,4*8(sp)                  /* save maybe-leaf flag               */

	br      ra,L_asm_handle_exception_load_gp_2
L_asm_handle_exception_load_gp_2:
	ldgp    gp,0(ra)                    /* load gp                            */

	mov     xptr,a0                     /* pass xptr                          */
	mov     xpc,a1                      /* pass xpc                           */
	mov     pv,a2                       /* pass PV                            */
	lda     a3,(ARG_CNT+TMP_CNT+6)*8(sp)/* pass Java SP                       */

L_asm_handle_exception_continue:
	jsr     ra,exceptions_handle_exception

	beq     v0,L_asm_handle_exception_not_catched

	mov     v0,xpc                      /* move handlerpc into xpc            */
	ldq     xptr,0*8(sp)                /* restore xptr                       */
	ldq     pv,2*8(sp)                  /* restore PV                         */
	ldq     ra,3*8(sp)                  /* restore RA                         */
	ldq     t0,4*8(sp)                  /* get maybe-leaf flag                */
	lda     sp,6*8(sp)                  /* free stack frame                   */

	beq     t0,L_asm_handle_exception_no_leaf

	RESTORE_ARGUMENT_REGISTERS(0)       /* if this is a leaf method, we have  */
	RESTORE_TEMPORARY_REGISTERS(ARG_CNT)/* to restore arg and temp registers  */
	
	lda     sp,(ARG_CNT+TMP_CNT)*8(sp)  /* remove maybe-leaf stackframe       */

L_asm_handle_exception_no_leaf:
	jmp     zero,(xpc)                  /* jump to the handler                */

L_asm_handle_exception_not_catched:
	ldq     xptr,0*8(sp)                /* restore xptr                       */
	ldq     pv,2*8(sp)                  /* restore PV                         */
	ldq     ra,3*8(sp)                  /* restore RA                         */
	ldq     t0,4*8(sp)                  /* get maybe-leaf flag                */
	lda     sp,6*8(sp)

	beq     t0,L_asm_handle_exception_no_leaf_stack

	lda     sp,(ARG_CNT+TMP_CNT)*8(sp)  /* remove maybe-leaf stackframe       */
	mov     zero,t0                     /* clear the maybe-leaf flag          */

L_asm_handle_exception_no_leaf_stack:
	ldl     t1,FrameSize(pv)            /* get frame size                     */
	addq    t1,sp,t1                    /* pointer to save area               */

	ldl     t2,IsLeaf(pv)               /* is leaf procedure                  */
	bne     t2,L_asm_handle_exception_no_ra_restore

	ldq     ra,-1*8(t1)                 /* restore ra                         */
	subq    t1,8,t1                     /* t1--                               */

L_asm_handle_exception_no_ra_restore:
	mov     ra,xpc                      /* the new xpc is ra                  */
	ldl     t2,IntSave(pv)              /* t2 = saved int register count      */
	br      t3,ex_int1                  /* t3 = current pc                    */
ex_int1:
	lda     t3,(ex_int2 - ex_int1)(t3)
	negl    t2,t2                       /* negate register count              */
	s4addq  t2,t3,t3                    /* t2 = IntSave - register count * 4  */
	jmp     zero,(t3)                   /* jump to save position              */

	ldq     s0,-7*8(t1)
	ldq     s1,-6*8(t1)
	ldq     s2,-5*8(t1)
	ldq     s3,-4*8(t1)
	ldq     s4,-3*8(t1)
	ldq     s5,-2*8(t1)
	ldq     s6,-1*8(t1)

ex_int2:
	s8addq  t2,t1,t1                    /* t1 = t1 - 8 * register count       */

	ldl     t2,FltSave(pv)              /* t2 = saved flt register count      */
	br      t3,ex_flt1                  /* t3 = current pc                    */
ex_flt1:
	lda     t3,(ex_flt2 - ex_flt1)(t3)
	negl    t2,t2                       /* negate register count              */
	s4addq  t2,t3,t3                    /* t2 = FltSave - 4 * register count  */
	jmp     zero,(t3)                   /* jump to save position              */

	ldt     fs0,-8*8(t1)
	ldt     fs1,-7*8(t1)
	ldt     fs2,-6*8(t1)
	ldt     fs3,-5*8(t1)
	ldt     fs4,-4*8(t1)
	ldt     fs5,-3*8(t1)
	ldt     fs6,-2*8(t1)
	ldt     fs7,-1*8(t1)

ex_flt2:
	ldl     t1,FrameSize(pv)            /* get frame size                     */
	addq    sp,t1,sp                    /* unwind stack                       */
	br      L_asm_handle_exception_stack_loop

	.end    asm_handle_nat_exception


/* asm_abstractmethoderror *****************************************************

   Creates and throws an AbstractMethodError.

*******************************************************************************/

	.ent    asm_abstractmethoderror

asm_abstractmethoderror:
	subq    sp,2*8,sp                   /* create stackframe                  */
	stq     ra,0*8(sp)                  /* save return address                */
	addq    sp,2*8,a0                   /* pass java sp                       */
	mov     ra,a1                       /* pass exception address             */
	jsr     ra,exceptions_asm_new_abstractmethoderror
	ldq     ra,0*8(sp)                  /* restore return address             */
	addq    sp,2*8,sp                   /* remove stackframe                  */

	mov     v0,xptr                     /* get exception pointer              */
	subq    ra,4,xpc                    /* exception address is ra - 4        */
	br      L_asm_handle_nat_exception

	.end    asm_abstractmethoderror


/* asm_patcher_wrapper *********************************************************

   XXX

   Stack layout:
     40   return address into JIT code (patch position)
     32   pointer to virtual java_objectheader
     24   machine code (which is patched back later)
     16   unresolved class/method/field reference
      8   data segment displacement from load instructions
      0   patcher function pointer to call (pv afterwards)

   ATTENTION: itmp3 == gp! But we don't need gp do call the patcher function.

*******************************************************************************/
		
	.ent    asm_patcher_wrapper

asm_patcher_wrapper:
	lda     sp,-((2+12+27+4)*8)(sp) /* create stack frame                     */

	SAVE_RETURN_REGISTERS(0)      /* save 1 int/1 float return registers      */
	SAVE_ARGUMENT_REGISTERS(2)    /* save 6 int/6 float argument registers    */
	SAVE_TEMPORARY_REGISTERS(14)  /* save 11 int/16 float temporary registers */

	stq     itmp1,(2+12+27+0)*8(sp) /* save itmp1                             */
	stq     itmp2,(2+12+27+1)*8(sp) /* save itmp2                             */
	stq     ra,(2+12+27+2)*8(sp)  /* save method return address (for leafs)   */
	stq     pv,(2+12+27+3)*8(sp)  /* save pv of calling java function         */

	br      ra,L_asm_patcher_wrapper_load_gp
L_asm_patcher_wrapper_load_gp:
	ldgp    gp,0(ra)              /* load gp (it's not set correctly in jit)  */

	lda     a0,(2+12+27+4)*8(sp)  /* pass SP of patcher stub                  */
	mov     pv,a1                 /* pass PV                                  */
	ldq     a2,(2+12+27+2)*8(sp)  /* pass RA (correct for leafs)              */
	jsr     ra,patcher_wrapper
	ldgp    gp,0(ra)
	stq     v0,(0+2+12+27+4)*8(sp) /* save return value                       */

	RESTORE_RETURN_REGISTERS(0)   /* restore 1 int/1 float return registers   */
	RESTORE_ARGUMENT_REGISTERS(2) /* restore 6 int/6 float argument registers */
	RESTORE_TEMPORARY_REGISTERS(14) /* restore 11 integer temporary registers */

	ldq     itmp1,(2+12+27+0)*8(sp) /* restore itmp1                          */
	ldq     itmp2,(2+12+27+1)*8(sp) /* restore itmp2                          */
	ldq     ra,(2+12+27+2)*8(sp)  /* restore method return address (for leafs)*/
	ldq     pv,(2+12+27+3)*8(sp)  /* restore pv of calling java function      */

	ldq     itmp3,(0+2+12+27+4)*8(sp) /* get return value                     */
	bne     itmp3,L_asm_patcher_wrapper_exception

	ldq     itmp3,(5+2+12+27+4)*8(sp) /* get RA to JIT                        */
	lda     sp,(6+2+12+27+4)*8(sp) /* remove stack frame                      */

	jmp     zero,(itmp3)          /* jump to new patched code                 */

L_asm_patcher_wrapper_exception:
	mov     itmp3,xptr            /* get exception                            */
	ldq     xpc,(5+2+12+27+4)*8(sp) /* RA is xpc                              */
	lda     sp,(6+2+12+27+4)*8(sp) /* remove stack frame                      */
	br      L_asm_handle_exception

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
	lda     sp,-(REPLACEMENT_STACK_OFFSET)(sp)

	/* save registers in execution state */
	stq     $0 ,( 0*8+offes_intregs)(sp)
	stq     $1 ,( 1*8+offes_intregs)(sp)
	stq     $2 ,( 2*8+offes_intregs)(sp)
	stq     $3 ,( 3*8+offes_intregs)(sp)
	stq     $4 ,( 4*8+offes_intregs)(sp)
	stq     $5 ,( 5*8+offes_intregs)(sp)
	stq     $6 ,( 6*8+offes_intregs)(sp)
	stq     $7 ,( 7*8+offes_intregs)(sp)
	stq     $8 ,( 8*8+offes_intregs)(sp)
	stq     $9 ,( 9*8+offes_intregs)(sp)
	stq     $10,(10*8+offes_intregs)(sp)
	stq     $11,(11*8+offes_intregs)(sp)
	stq     $12,(12*8+offes_intregs)(sp)
	stq     $13,(13*8+offes_intregs)(sp)
	stq     $14,(14*8+offes_intregs)(sp)
	stq     $15,(15*8+offes_intregs)(sp)
	stq     $16,(16*8+offes_intregs)(sp)
	stq     $17,(17*8+offes_intregs)(sp)
	stq     $18,(18*8+offes_intregs)(sp)
	stq     $19,(19*8+offes_intregs)(sp)
	stq     $20,(20*8+offes_intregs)(sp)
	stq     $21,(21*8+offes_intregs)(sp)
	stq     $22,(22*8+offes_intregs)(sp)
	stq     $23,(23*8+offes_intregs)(sp)
	stq     $24,(24*8+offes_intregs)(sp)
	stq     $25,(25*8+offes_intregs)(sp)
	stq     $26,(26*8+offes_intregs)(sp)
	stq     $27,(27*8+offes_intregs)(sp)
	stq     $28,(28*8+offes_intregs)(sp)
	stq     $29,(29*8+offes_intregs)(sp)
	stq     $30,(30*8+offes_intregs)(sp)
	stq     $31,(31*8+offes_intregs)(sp)
	
	stt     $f0 ,( 0*8+offes_fltregs)(sp)
	stt     $f1 ,( 1*8+offes_fltregs)(sp)
	stt     $f2 ,( 2*8+offes_fltregs)(sp)
	stt     $f3 ,( 3*8+offes_fltregs)(sp)
	stt     $f4 ,( 4*8+offes_fltregs)(sp)
	stt     $f5 ,( 5*8+offes_fltregs)(sp)
	stt     $f6 ,( 6*8+offes_fltregs)(sp)
	stt     $f7 ,( 7*8+offes_fltregs)(sp)
	stt     $f8 ,( 8*8+offes_fltregs)(sp)
	stt     $f9 ,( 9*8+offes_fltregs)(sp)
	stt     $f10,(10*8+offes_fltregs)(sp)
	stt     $f11,(11*8+offes_fltregs)(sp)
	stt     $f12,(12*8+offes_fltregs)(sp)
	stt     $f13,(13*8+offes_fltregs)(sp)
	stt     $f14,(14*8+offes_fltregs)(sp)
	stt     $f15,(15*8+offes_fltregs)(sp)
	stt     $f16,(16*8+offes_fltregs)(sp)
	stt     $f17,(17*8+offes_fltregs)(sp)
	stt     $f18,(18*8+offes_fltregs)(sp)
	stt     $f19,(19*8+offes_fltregs)(sp)
	stt     $f20,(20*8+offes_fltregs)(sp)
	stt     $f21,(21*8+offes_fltregs)(sp)
	stt     $f22,(22*8+offes_fltregs)(sp)
	stt     $f23,(23*8+offes_fltregs)(sp)
	stt     $f24,(24*8+offes_fltregs)(sp)
	stt     $f25,(25*8+offes_fltregs)(sp)
	stt     $f26,(26*8+offes_fltregs)(sp)
	stt     $f27,(27*8+offes_fltregs)(sp)
	stt     $f28,(28*8+offes_fltregs)(sp)
	stt     $f29,(29*8+offes_fltregs)(sp)
	stt     $f30,(30*8+offes_fltregs)(sp)
	stt     $f31,(31*8+offes_fltregs)(sp)
	
	/* calculate sp of method */
	lda     itmp1,(REPLACEMENT_STACK_OFFSET + 2*8)(sp)
	stq     itmp1,(offes_sp)(sp)

	br      ra,L_asm_replacement_out_load_gp
L_asm_replacement_out_load_gp:
	ldgp    gp,0(ra)                    /* load gp                            */

	/* store pv */
	stq     pv,(offes_pv)(sp)

	/* call replace_me */
	ldq     a0,-(2*8)(itmp1)            /* arg0: rplpoint *                   */
    mov     sp,a1                       /* arg1: execution state              */
    jmp     zero,replace_me             /* call C function replace_me         */
    jmp     zero,abort                  /* NEVER REACHED                      */

	.end asm_replacement_out

/* asm_replacement_in **********************************************************

   This code writes the given execution state and jumps to the replacement
   code.

   This function never returns!

   NOTE: itmp3 is not restored!

   C prototype:
      void asm_replacement_in(executionstate *es, replace_safestack_t *st);

*******************************************************************************/

	.ent asm_replacement_in
	
asm_replacement_in:
	/* a0 == executionstate *es */

	/* get arguments */
	mov     a1,s1                       /* replace_safestack_t *st            */
	mov     a0,s2                       /* executionstate *es == safe stack   */

	/* switch to the safe stack */
	mov     s2,sp

	/* call replace_build_execution_state(st) */
	mov		s1,a0
	jsr		ra,replace_build_execution_state

	/* set new sp */
	ldq		sp,(offes_sp)(s2)

	/* build stack frame */
	lda     sp,(-sizeexecutionstate)(sp)

	/* call replace_free_safestack(st,& of allocated executionstate_t) */
	mov		sp,a1 /* tmpes */
	mov		s1,a0 /* st    */
	jsr		ra,replace_free_safestack

	/* set new pv */
	ldq     pv,(offes_pv)(sp)
	
	/* copy registers from execution state */
	ldq     $0 ,( 0*8+offes_intregs)(sp)
	ldq     $1 ,( 1*8+offes_intregs)(sp)
	ldq     $2 ,( 2*8+offes_intregs)(sp)
	ldq     $3 ,( 3*8+offes_intregs)(sp)
	ldq     $4 ,( 4*8+offes_intregs)(sp)
	ldq     $5 ,( 5*8+offes_intregs)(sp)
	ldq     $6 ,( 6*8+offes_intregs)(sp)
	ldq     $7 ,( 7*8+offes_intregs)(sp)
	ldq     $8 ,( 8*8+offes_intregs)(sp)
	ldq     $9 ,( 9*8+offes_intregs)(sp)
	ldq     $10,(10*8+offes_intregs)(sp)
	ldq     $11,(11*8+offes_intregs)(sp)
	ldq     $12,(12*8+offes_intregs)(sp)
	ldq     $13,(13*8+offes_intregs)(sp)
	ldq     $14,(14*8+offes_intregs)(sp)
	ldq     $15,(15*8+offes_intregs)(sp)
	ldq     a0, (16*8+offes_intregs)(sp)
	ldq     $17,(17*8+offes_intregs)(sp)
	ldq     $18,(18*8+offes_intregs)(sp)
	ldq     $19,(19*8+offes_intregs)(sp)
	ldq     $20,(20*8+offes_intregs)(sp)
	ldq     $21,(21*8+offes_intregs)(sp)
	ldq     $22,(22*8+offes_intregs)(sp)
	ldq     $23,(23*8+offes_intregs)(sp)
	ldq     $24,(24*8+offes_intregs)(sp)
	ldq     $25,(25*8+offes_intregs)(sp)
	ldq     $26,(26*8+offes_intregs)(sp)
	/* $27 is pv                    */
	ldq     $28,(28*8+offes_intregs)(sp)
	ldq     $29,(29*8+offes_intregs)(sp)
	/* $30 is sp                      */
	/* $31 is zero                    */
	
	ldt     $f0 ,( 0*8+offes_fltregs)(sp)
	ldt     $f1 ,( 1*8+offes_fltregs)(sp)
	ldt     $f2 ,( 2*8+offes_fltregs)(sp)
	ldt     $f3 ,( 3*8+offes_fltregs)(sp)
	ldt     $f4 ,( 4*8+offes_fltregs)(sp)
	ldt     $f5 ,( 5*8+offes_fltregs)(sp)
	ldt     $f6 ,( 6*8+offes_fltregs)(sp)
	ldt     $f7 ,( 7*8+offes_fltregs)(sp)
	ldt     $f8 ,( 8*8+offes_fltregs)(sp)
	ldt     $f9 ,( 9*8+offes_fltregs)(sp)
	ldt     $f10,(10*8+offes_fltregs)(sp)
	ldt     $f11,(11*8+offes_fltregs)(sp)
	ldt     $f12,(12*8+offes_fltregs)(sp)
	ldt     $f13,(13*8+offes_fltregs)(sp)
	ldt     $f14,(14*8+offes_fltregs)(sp)
	ldt     $f15,(15*8+offes_fltregs)(sp)
	ldt     $f16,(16*8+offes_fltregs)(sp)
	ldt     $f17,(17*8+offes_fltregs)(sp)
	ldt     $f18,(18*8+offes_fltregs)(sp)
	ldt     $f19,(19*8+offes_fltregs)(sp)
	ldt     $f20,(20*8+offes_fltregs)(sp)
	ldt     $f21,(21*8+offes_fltregs)(sp)
	ldt     $f22,(22*8+offes_fltregs)(sp)
	ldt     $f23,(23*8+offes_fltregs)(sp)
	ldt     $f24,(24*8+offes_fltregs)(sp)
	ldt     $f25,(25*8+offes_fltregs)(sp)
	ldt     $f26,(26*8+offes_fltregs)(sp)
	ldt     $f27,(27*8+offes_fltregs)(sp)
	ldt     $f28,(28*8+offes_fltregs)(sp)
	ldt     $f29,(29*8+offes_fltregs)(sp)
	ldt     $f30,(30*8+offes_fltregs)(sp)
	ldt     $f31,(31*8+offes_fltregs)(sp)

	/* load new pc */

	ldq     itmp3,offes_pc(sp)

	/* remove stack frame */

	lda		sp,(sizeexecutionstate)(sp)

	/* jump to new code */

	jmp     zero,(itmp3)

	.end asm_replacement_in

#endif /* defined(ENABLE_REPLACEMENT) */


/* asm_compare_and_swap ********************************************************

   Does an atomic compare and swap.  Required for the lock
   implementation.

   Atomically do the following: Check if the location still contains
   `oldval`. If so, replace it by `newval` and return `oldval`.

   RETURN VALUE:
       the old value at *p

   long compare_and_swap(volatile long *p, long oldval, long newval);

*******************************************************************************/

	.ent    asm_compare_and_swap

asm_compare_and_swap:
1:
	ldq_l   v0,0(a0)
	cmpeq   v0,a1,t0
	beq     t0,2f
	mov     a2,t0
	stq_c   t0,0(a0)
	beq     t0,1b
2:
	jmp     zero,(ra)

	.end    asm_compare_and_swap


/* asm_memory_barrier **********************************************************

   A memory barrier for the Java Memory Model.

*******************************************************************************/

	.ent    asm_memory_barrier

asm_memory_barrier:
	mb
	jmp     zero,(ra)

	.end    asm_memory_barrier


	.ent    asm_getclassvalues_atomic

asm_getclassvalues_atomic:
_crit_restart:
_crit_begin:
	ldl     t0,offbaseval(a0)
	ldl     t1,offdiffval(a0)
	ldl     t2,offbaseval(a1)
_crit_end:
	stl     t0,offcast_super_baseval(a2)
	stl     t1,offcast_super_diffval(a2)
	stl     t2,offcast_sub_baseval(a2)
	jmp     zero,(ra)

	.end    asm_getclassvalues_atomic


    .data

asm_criticalsections:
#if defined(ENABLE_THREADS)
    .quad   _crit_begin
    .quad   _crit_end
    .quad   _crit_restart
#endif
    .quad   0


/* asm_md_init *****************************************************************

   Initialize machine dependent stuff.

   Determines if the byte support instruction set (21164a and higher)
   is available.

*******************************************************************************/

	.ent    asm_md_init

asm_md_init:
	.long   0x47e03c20                  /* amask   1,v0                       */
	jmp     zero,(ra)                   /* return                             */

	.end    asm_md_init


/* asm_cacheflush **************************************************************

   XXX

*******************************************************************************/

	.ent    asm_cacheflush

asm_cacheflush:
	call_pal PAL_imb              /* synchronize instruction cache            */
	jmp     zero,(ra)

	.end    asm_cacheflush


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

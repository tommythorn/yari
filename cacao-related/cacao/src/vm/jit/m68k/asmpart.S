/*	src/vm/jit/m68k/asmpart.S
 *	for cacao 
 *	Roland Lezuo
 */

#include "config.h"

#include "vm/jit/m68k/offsets.h"
#include "md-abi.h"
#include "vm/jit/methodheader.h"

.data
call_jit_dummy:
	.long 0xdeadbeef

.text

.globl asm_vm_call_method
.globl asm_vm_call_method_int
.globl asm_vm_call_method_long
.globl asm_vm_call_method_float
.globl asm_vm_call_method_double
.globl asm_vm_call_method_end
.globl asm_vm_call_method_exception_handler

.globl asm_call_jit_compiler

.globl asm_patcher_wrapper

.globl asm_getclassvalues_atomic
.globl asm_abstractmethoderror
.globl asm_criticalsections

.globl asm_handle_exception
.globl asm_handle_nat_exception

/*
 *	This functions implement the C prototyped funtion
 *	java_objectheader *asm_vm_call_method(methodinfo *m, s4 vmargscount,vm_arg *vmargs);
 *	the _int, _long, _float, _double are used for different return types
 *
 *	The function may still be uncompiled, so the jit compiler gets invoked.
 *
 *
 */

	/* this is the method header see src/vm/jit/methodheader.h */
	.align	4
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
	addal	#(-11*4),%sp						/* create stackframe to save registers */
	moveml	%d2/%d3/%d4/%d5/%d6/%d7/%a2/%a3/%a4/%a5/%fp,%sp@	/* save registers */

	/* fetch arguments from vmargs data structure */
	movel	%sp@(11*4+1*4),%a2				/* methodinfo argument in atmp1 */
	movel	%sp@(11*4+3*4),%a3				/* args block */
	movel	%sp@(11*4+2*4),%d2				/* arg count */

	moveal	%sp, %a5					/* memorize stack */
	tstl	%d2							/* do we have arguments ? */
	beq	L_asm_vm_call_method_copy_done
	subql	#1,%d2						/* simplifies offset calulation */

	movel	#(sizevmarg), %d0
	mulsl	%d2, %d0					
	moveal	%a3, %a0
	addal	%d0, %a0					/* %a0 points to last vmarg block */

	addql	#1, %d2
L_asm_vm_call_copy_arg:
	subql	#1, %d2						/* decrement argument counter */
	movel	%a0@(offvmargtype+4), %d1	/* %d1 contains type of arg, vmargtype is u8 */	

	cmpil	#2, %d1						/* float type ? */
	bne		L_asm_vm_call_copy_int

	movel	%a0@(offvmargdata), %d0		/* float has different endianess as int */
	movel	%d0, %sp@-
	bra		L_asm_vm_call_copy_1_word_type

L_asm_vm_call_copy_int:
	movel	%a0@(offvmargdata+4), %d0
	movel	%d0,%sp@-					/* push argument onto stack */

	btstl	#0, %d1						/* test if 2 word type */
	beq	L_asm_vm_call_copy_1_word_type
	movel	%a0@(offvmargdata), %d0		/* push second word onto stack */
	movel	%d0,%sp@-
L_asm_vm_call_copy_1_word_type:

	subl	#(sizevmarg),%a0
	tstl 	%d2							/* arguments left ? */
	bne	L_asm_vm_call_copy_arg

L_asm_vm_call_method_copy_done:

	leal 	asm_call_jit_compiler,%a4			/* we need to fake a invocation as it would happen from jit code */
	movel	%a4, call_jit_dummy					/* we need a writeable memory location */
	moveal	call_jit_dummy, %a4					/* XXX do we have a race condition here ? */
	jsr	%a4@

L_asm_vm_call_method_return:
	movel	%a5, %sp						/* pop arguments off stack */
	moveml  %sp@, %d2/%d3/%d4/%d5/%d6/%d7/%a2/%a3/%a4/%a5/%fp	/* restore registers */
	addal 	#(11*4),%sp						/* restore stack */
	moveal	%d0, %a0						/* return value in %a0, too */
asm_vm_call_method_end:						/* symbol needed to insert method into avl tree */
	rts										/* return to c code */

/* asm_vm_call_method_exception_handler	********************************************************************
 *
 * calls void *builtin_throw_exception(java_objectheader *xptr) when no other handler is appropiate
 * this functions gets called indirectly from asm_handle_exception, which back then moved xptr to %a2
 * clear software design is in the eye of the beholder.
 ************************************************************************************************************/
asm_vm_call_method_exception_handler:
	movel	%a2, %sp@-			/* push xptr argument */
	jsr 	builtin_throw_exception
	lea	%sp@(4), %sp			/* pop arg off stack */
	jmp 	L_asm_vm_call_method_return


/* asm_call_jit_compiler ************************************************************************************
 *	Invokes compiler for untranslated java methods.
 *	C prototype: void asm_call_jit_compiler(void);
 *	BUT: this does not match reality, arguments _ARE_ passed via createcompilerstub and asm_vm_call_method...
 *	arguments passed via %a2(methodinfo) == REG_ATMP1
 *                       %a3(mptr)       == REG_ATMP2
 ************************************************************************************************************/
asm_call_jit_compiler:
	addal	#(-4*4),%sp						/* create stackframe to save registers */
	moveml	%a0/%a1/%d0/%d1,%sp@			/* save volatile registers */
#if !defined(ENABLE_SOFTFLOAT)
	fmoved	%fp0, %sp@-
	fmoved	%fp1, %sp@-
	movel	%sp@(8*4), %sp@-
	pea		%sp@(8*4+8)
#else
	movel 	%sp@(4*4), %sp@-				/* push arguments onto stack (ra)*/
	pea 	%sp@(4*4+8)						/* the old stack pointer*/
#endif
	movel 	%a3,%sp@-						/* mptr */
	movel 	%a2,%sp@-						/* methodinfo */

	/* C prototype: u1 *jit_asm_compile(methodinfo *m, u1 *mptr, u1 *sp, u1 *ra); */
	jsr	jit_asm_compile						/* invoke compiler */
	addal	#(4*4),%sp						/* pop arguments off stack */
	moveal	%d0, %a2						/* to tmp register */

#if !defined(ENABLE_SOFTFLOAT)
	fdmoved	%sp@+, %fp1
	fdmoved	%sp@+, %fp0
#endif

	moveml %sp@,%a0/%a1/%d0/%d1				/* restore volatile registers */
	addal	#(4*4),%sp						/* remove stackframe */

	tstl	%a2								/* check for exception */
	beq	L_asm_call_jit_compiler_exception

	jmp	%a2@								/* invoke java method */
	jsr 	0								/* we should not ever return here */

L_asm_call_jit_compiler_exception:
	jsr		exceptions_get_and_clear_exception		/* exception object in %d0 now */
	moveal	%d0, %a2								/* move exception object into sptr register */
	moveal	%sp@+, %a3								/* pop return address into exception address reg */
	jmp asm_handle_exception						/* handle exception */


/* asm_patcher_wrapper ********************************************************
	
  prepares arguments on stack
  calls patcher_wrapper signature: java_objectheader *patcher_wrapper(u1 *sp, u1 *pv, u1 *ra);

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

  /* save scratch registers */
  movel %a0, %sp@-
  movel %a1, %sp@-
  movel %d0, %sp@-
  movel %d1, %sp@-

#if defined(ENABLE_SOFTFLOAT)
  /* calculate original value of sp */
  movel %sp, %d0
  addil #4*4, %d0
#else
  fmoved %fp0, %sp@-
  fmoved %fp1, %sp@-

  movel %sp, %d0
  addil #8*4, %d0
#endif

  clrl	%sp@-				/* pass ra */
  clrl	%sp@-				/* pass pv, if null use findmethod */
  movel %d0, %sp@-			/* pass sp of patcher stub */
  jsr	patcher_wrapper		/* return value in %d0 */

  lea	%sp@(3*4), %sp		/* pop arguments off stack */
  tst	%d0					/* test if exception occured */
  bne	L_asm_patcher_wrapper_exception

#if !defined(ENABLE_SOFTFLOAT)
  fdmoved %sp@+, %fp1
  fdmoved %sp@+, %fp0
#endif
  movel %sp@+, %d1
  movel %sp@+, %d0
  movel %sp@+, %a1
  movel %sp@+, %a0

  lea   %sp@(6*4), %sp			/* restore stack and remove patcher stub*/
  rts							/* back to jit code */

L_asm_patcher_wrapper_exception:
  /* WARNING: the stack is still disturbed, look at asm_patcher_wrapper for details */
  /* we do not need to restore the content of the registers, I hope */
#if !defined(ENABLE_SOFTFLOAT)
	lea		%sp@(6*4), %sp
#else
	lea		%sp@(4*4), %sp
#endif

  lea   	%sp@(5*4), %sp				/* restore stack and remove patcher stub*/
  movel		%sp@+, %d4					/* restore REG_ITMP3, stored in emit_patcher_stubs */
  moveal	%d0, %a2					/* xptr, pointer to exception object */
  moveal	%sp@+, %a3					/* pop return address into exception address register */
  jmp		asm_handle_exception		/* handle exception */
  illegal								/* XXX: we never come back */

/********************************************************************************
	Reads a few values atomically. 
	C signature:
	void asm_getclassvalues_atomic(vftbl_t *super, vftbl_t *sub, castinfo *out);

	super ... sp@(4)
	sub   ... sp@(8)
	out   ... sp@(12)
********************************************************************************/
asm_getclassvalues_atomic:
_crit_restart:
_crit_begin:
	moveal 	%sp@(4), %a0
	movel	%a0@(offbaseval), %d0
	movel	%a0@(offdiffval), %d1
	
	moveal	%sp@(8), %a0
	moveal	%a0@(offbaseval), %a0
_crit_end:
	moveal	%sp@(12), %a1
	movel	%d0, %a1@(offcast_super_baseval)
	movel	%d1, %a1@(offcast_super_diffval)
	movel	%a0, %a1@(offcast_sub_baseval)

	rts
.data
asm_criticalsections:
#if defined(ENABLE_THREADS)
	.long _crit_begin
	.long _crit_end
	.long _crit_restart
#endif
	.long 0
.text

asm_abstractmethoderror:

/* asm_handle_exception ********************************************************
*                                                                              *
*   This function handles an exception. It does not use the usual calling      *
*   conventions. The exception pointer is passed in REG_ATMP1 and the          *
*   pc from the exception raising position is passed in REG_ATMP2. It searches *
*   the local exception table for a handler. If no one is found, it unwinds    *
*   stacks and continues searching the callers.                                *
*                                                                              *
*   void asm_handle_exception (void);
*   exception object pointer...%a2   exception raising address...%a3           *
*                                                                              *
*******************************************************************************/
asm_handle_nat_exception:
	lea	%sp@(4), %sp
asm_handle_exception:
L_asm_handle_exception_stack_loop:
	
	/* we need the dseg, figure it out */
	movel	%a3, %sp@-				/* push ra argument */
	jsr	md_codegen_get_pv_from_pc	/* pv in %d0 now */
	movel	%d0, %d2				/* move to safe register */
	lea	%sp@(4), %sp				/* pop args off stack */

	/* now call the following c function */
	/* u1 *exceptions_handle_exception(java_objectheader *xptr, u1 *xpc, u1 *pv, u1 *sp) */
	movel	%sp,%sp@-
	movel	%d2,%sp@-
	movel	%a3,%sp@-
	movel	%a2,%sp@-
	jsr	exceptions_handle_exception		/* %d0 is address of handler or 0 when not catched */
	lea	%sp@(4*4), %sp				/* pop args off stack */
	tstl	%d0
	beq	L_asm_handle_exception_not_catched

	/* %d0 contains address of exception handler */
	moveal	%d0, %a0
	moveal	%a2, %a1					/* XXX FIXME, xptr is expected in %a1 by java code, whyever */
	jmp	%a0@

L_asm_handle_exception_not_catched:
	/* we did not find an exception handler in this stackframe */
	/* remove this frame and search in the one above */
	/* %a2 containts exception object ptr, %d2 the actual pv */
	moveal	%d2, %a3
	movel	%a3@(FrameSize), %d2

	moveal	%sp, %a0
	addal	%d2, %a0	/* %a0 now points to top of stackframe, where saved regs are */

	/* the saved registers have to be restored */
	/* XXX ugly hack: intsave and adrsave share one field */
	movel	%a3@(IntSave), %d0
	andil	#0x0000ffff, %d0	/* this is IntSave */
	cmpb	#0,%d0
	beq	L_asm_handle_ex_int_done
	movel	-(%a0), %d7

	cmpb	#1,%d0
	beq	L_asm_handle_ex_int_done
	movel	-(%a0), %d6

	cmpb	#2,%d0 
	beq	L_asm_handle_ex_int_done
	movel	-(%a0), %d5

L_asm_handle_ex_int_done:

	movel	%a3@(IntSave), %d0
	andil	#0xffff0000, %d0	/* this is AdrSave */
	tstl	%d0
	beq	L_handle_exception_nat_catched_no_adr
	moveal	-(%a0), %a5
L_handle_exception_nat_catched_no_adr:

#if !defined(ENABLE_SOFTFLOAT)
	movel	%a3@(FltSave), %d0
	cmpb	#0,%d0
	beq	L_asm_handle_ex_flt_done
	fdmoved	-(%a0), %fp7

	cmpb	#1,%d0
	beq	L_asm_handle_ex_flt_done
	fdmoved	-(%a0), %fp6
	
	cmpb	#2,%d0
	beq	L_asm_handle_ex_flt_done
	fdmoved	-(%a0), %fp5

L_asm_handle_ex_flt_done:
#else
	/* nothing to do */
#endif
	addal	%d2, %sp		/* remove old stackframe */
	moveal	%sp@+, %a3		/* make return address, new exception rasing address */
	subal	#2, %a3			/* it was a jsr %aX, which is 4 bytes long */
	jmp	L_asm_handle_exception_stack_loop


illegal
.long 0x23234242


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

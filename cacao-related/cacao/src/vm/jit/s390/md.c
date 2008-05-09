/* src/vm/jit/x86_64/md.c - machine dependent x86_64 Linux functions

   Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
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

   Contact: cacao@cacaojvm.org

   Authors: Christian Thalinger

   Changes: Edwin Steiner

   $Id: md.c 7848 2007-05-01 21:40:26Z pm $

*/

#define _GNU_SOURCE

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>

#include "vm/jit/s390/md-abi.h"

#if defined(ENABLE_THREADS)
# include "threads/threads-common.h"
# include "threads/native/threads.h"
#endif

#include "vm/exceptions.h"
#include "vm/signallocal.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/stacktrace.h"

#if !defined(NDEBUG) && defined(ENABLE_DISASSEMBLER)
#include "vmcore/options.h" /* XXX debug */
#include "vm/jit/disass.h" /* XXX debug */
#endif

#include "vm/jit/codegen-common.h"
#include "vm/jit/s390/codegen.h"

#include <assert.h>
#define OOPS() assert(0);

/* prototypes *****************************************************************/

void md_signal_handler_sigill(int sig, siginfo_t *siginfo, void *_p);

void md_dump_context(u1 *pc, mcontext_t *mc);

/* md_init *********************************************************************

   Do some machine dependent initialization.

*******************************************************************************/

void md_init(void)
{
	struct sigaction act;
	
	act.sa_sigaction = md_signal_handler_sigill;
	act.sa_flags     = SA_NODEFER | SA_SIGINFO;

	if (sigaction(SIGILL, &act, NULL) == -1)	{
		vm_abort("%s: error registering SIGILL signal handler.", __FUNCTION__);
	}
}

/* md_dump_context ************************************************************
 
   Logs the machine context
  
*******************************************************************************/

void md_dump_context(u1 *pc, mcontext_t *mc) {
	int i;
	
	union {
		u8 l;
		fpreg_t fr;
	} freg;

	log_println("Dumping context.");

	log_println("Program counter: 0x%08X", pc);

#if defined(ENABLE_DISASSEMBLER)
	log_println("Printing instruction at program counter:");
	disassinstr(pc);
#endif

	log_println("General purpose registers:");

	for (i = 0; i < 16; i++) {
		log_println("\tr%d:\t0x%08X\t%d", i, mc->gregs[i], mc->gregs[i]);
	}

	log_println("Floating point registers:");

	for (i = 0; i < 16; i++) {
		freg.fr.d = mc->fpregs.fprs[i].d;
		log_println("\tf%d\t0x%016llX\t(double)%e\t(float)%f", i, freg.l, freg.fr.d, freg.fr.f);
	}

#if defined(ENABLE_THREADS)
	log_println("Dumping the current stacktrace:");
	threads_print_stacktrace();
#endif

}

/* md_signal_handler_sigsegv ***************************************************

   NullPointerException signal handler for hardware null pointer
   check.

*******************************************************************************/

void md_signal_handler_sigsegv(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t        *_uc;
	mcontext_t        *_mc;
	u1                *pv;
	u1                *sp;
	u1                *ra;
	u1                *xpc;
	s4                 type;
	ptrint             val;
	java_objectheader *e;
	s4                 base;
	s4                 is_null;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	xpc = (u1 *)_mc->psw.addr;

	/* Check opcodes and verify it is a null pointer exception */

	switch (xpc[0]) {
		case 0x58: /* L */
		case 0x50: /* ST */
			base = (xpc[2] >> 4) & 0xF;
			if (base == 0) {
				is_null = 1;
			} else if (_mc->gregs[base] == 0) {
				is_null = 1;
			} else {
				is_null = 0;
			}
			break;
	}

	if (! is_null) {
#if !defined(NDEBUG)
		md_dump_context(xpc, _mc);
#endif
		vm_abort("%s: segmentation fault at %p, aborting.", __FUNCTION__, xpc);
	}

	pv = (u1 *)_mc->gregs[REG_PV];
	sp = (u1 *)_mc->gregs[REG_SP];
	ra = xpc;
	type = EXCEPTION_HARDWARE_NULLPOINTER;
	val = 0;

	e = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

	_mc->gregs[REG_ITMP2_XPC] = (ptrint) xpc;
	_mc->gregs[REG_ITMP1_XPTR] = (ptrint) e;
	_mc->psw.addr = (ptrint) asm_handle_exception;
}

void md_signal_handler_sigill(int sig, siginfo_t *siginfo, void *_p) {
	ucontext_t        *_uc;
	mcontext_t        *_mc;
	u1                *xpc;
	u1                *ra;
	u1                *pv;
	u1                *sp;
	s4                 type;
	ptrint             val;
	java_objectheader *e;
	s4                 reg;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;
	xpc = ra = siginfo->si_addr;

	/* Our trap instruction has the format: { 0x02, one_byte_of_data }. */

	if ((siginfo->si_code == ILL_ILLOPC) && (xpc[0] == 0x02)) {

		/* bits 7-4 contain a register holding a value */
		reg = (xpc[1] >> 4) & 0xF;

		/* bits 3-0 designate the exception type */
		type = xpc[1] & 0xF;  

		pv = (u1 *)_mc->gregs[REG_PV];
		sp = (u1 *)_mc->gregs[REG_SP];
		val = (ptrint)_mc->gregs[reg];

		e = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

		_mc->gregs[REG_ITMP1_XPTR] = (ptrint)e;
		_mc->gregs[REG_ITMP2_XPC] = (ptrint)xpc;
		_mc->psw.addr = (ptrint) asm_handle_exception;

	} else {
#if !defined(NDEBUG)
		md_dump_context(xpc, _mc);
#endif
		vm_abort("%s: illegal instruction at %p, aborting.", __FUNCTION__, xpc);
	}
}

/* md_signal_handler_sigfpe ****************************************************

   ArithmeticException signal handler for hardware divide by zero
   check.

*******************************************************************************/

void md_signal_handler_sigfpe(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t         *_uc;
	mcontext_t         *_mc;
	u1                 *pv;
	u1                 *sp;
	u1                 *ra;
	u1                 *xpc;
	u1                 *pc;
	s4                  r1, r2;
	s4                  type;
	ptrint              val;
	java_objectheader  *e;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	/* Instruction that raised signal */
	xpc = siginfo->si_addr;

	/* Check opcodes */

	if (xpc[0] == 0x1D) { /* DR */

		r1 = (xpc[1] >> 4) & 0xF;
		r2 = xpc[1] & 0xF;

		if (
			(_mc->gregs[r1] == 0xFFFFFFFF) &&
			(_mc->gregs[r1 + 1] == 0x80000000) && 
			(_mc->gregs[r2] == 0xFFFFFFFF)
		) {
			/* handle special case */
			/* next instruction */
			pc = (u1 *)_mc->psw.addr;
			/* reminder */
			_mc->gregs[r1] = 0;
			/* quotient */
			_mc->gregs[r1 + 1] = 0x80000000;
			/* continue at next instruction */
			_mc->psw.addr = (ptrint) pc;

			return;
		} else if (_mc->gregs[r2] == 0) {
			/* division by 0 */

			pv = (u1 *)_mc->gregs[REG_PV];
			sp = (u1 *)_mc->gregs[REG_SP];
			ra = xpc;

			type = EXCEPTION_HARDWARE_ARITHMETIC;
			val = 0;

			e = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

			_mc->gregs[REG_ITMP1_XPTR] = (ptrint)e;
			_mc->gregs[REG_ITMP2_XPC] = (ptrint)xpc;
			_mc->psw.addr = (ptrint) asm_handle_exception;

			return;
		}
	}

	/* Could not handle signal */

#if !defined(NDEBUG)
	md_dump_context(xpc, _mc);
#endif
	vm_abort("%s: floating point exception at %p, aborting.", __FUNCTION__, xpc);
}


/* md_signal_handler_sigusr2 ***************************************************

   Signal handler for profiling sampling.

*******************************************************************************/

#if defined(ENABLE_THREADS)
void md_signal_handler_sigusr2(int sig, siginfo_t *siginfo, void *_p)
{
	threadobject *t;
	ucontext_t   *_uc;
	mcontext_t   *_mc;
	u1           *pc;

	t = THREADOBJECT;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	/* ATTENTION: Don't use CACAO's internal REG_* defines as they are
	   different to the ones in <ucontext.h>. */

	pc = (u1 *) _mc->psw.addr;

	t->pc = pc;
}
#endif


#if defined(ENABLE_THREADS)
void thread_restartcriticalsection(ucontext_t *_uc)
{
	mcontext_t *_mc;
	void       *pc;

	_mc = &_uc->uc_mcontext;

	pc = critical_find_restart_point((void *) _mc->psw.addr);

	if (pc != NULL)
		_mc->psw.addr = (ptrint) pc;
}
#endif


/* md_codegen_patch_branch *****************************************************

   Back-patches a branch instruction.

*******************************************************************************/

void md_codegen_patch_branch(codegendata *cd, s4 branchmpc, s4 targetmpc)
{

	s4 *mcodeptr;
	s4  disp;                           /* branch displacement                */

	/* calculate the patch position */

	mcodeptr = (s4 *) (cd->mcodebase + branchmpc);

	/* Calculate the branch displacement. */

	disp = targetmpc - branchmpc;
	disp += 4; /* size of branch */
	disp /= 2; /* specified in halfwords */

	ASSERT_VALID_BRANCH(disp);	

	/* patch the branch instruction before the mcodeptr */

	mcodeptr[-1] |= (disp & 0xFFFF);
}


/* md_stacktrace_get_returnaddress *********************************************

   Returns the return address of the current stackframe, specified by
   the passed stack pointer and the stack frame size.

*******************************************************************************/

u1 *md_stacktrace_get_returnaddress(u1 *sp, u4 framesize)
{
	u1 *ra;

	/* on S390 the return address is located on the top of the stackframe */

	ra = *((u1 **) (sp + framesize - SIZEOF_VOID_P));

	return ra;
}


/* md_get_method_patch_address *************************************************

   Gets the patch address of the currently compiled method. The offset
   is extracted from the load instruction(s) before the jump and added
   to the right base address (PV or REG_METHODPTR).

   INVOKESTATIC/SPECIAL:

0x7748d7b2:   a7 18 ff d4                      lhi      %r1,-44  
(load dseg offset)
0x7748d7b6:   58 d1 d0 00                      l        %r13,0(%r1,%r13)
(load pv)
0x7748d7ba:   0d ed                            basr     %r14,%r13
(jump to pv)

   INVOKEVIRTUAL:

0x7748d82a:   58 c0 20 00                      l        %r12,0(%r2)
(load mptr)
0x7748d82e:   58 d0 c0 00                      l        %r13,0(%r12)
(load pv from mptr)
0x7748d832:   0d ed                            basr     %r14,%r13
(jump to pv)


   INVOKEINTERFACE:

last 2 instructions the same as in invokevirtual

*******************************************************************************/

u1 *md_get_method_patch_address(u1 *ra, stackframeinfo *sfi, u1 *mptr)
{
	u1  base;
	s4  offset;
	u1 *pa;                             /* patch address                      */

	/* go back to the load before the call instruction */

	ra = ra - 2 /* sizeof bcr */ - 4 /* sizeof l */;

	/* get the base register of the load */

	base = ra[2] >> 4;

	/* check for the different calls */

	if (base == 0xd) { /* pv relative */
		/* INVOKESTATIC/SPECIAL */

		/* the offset is in the load before the load */

		offset = *((s2 *) (ra - 2));

		/* add the offset to the procedure vector */

		pa = sfi->pv + offset;
	}
	else if (base == 0xc) { /* mptr relative */
		/* INVOKEVIRTUAL/INTERFACE */

		offset = *((u2 *)(ra + 2)) & 0xFFF;

		/* add offset to method pointer */
		
		pa = mptr + offset;
	}
	else {
		/* catch any problems */
		assert(0); 
	}

	return pa;
}


/* md_codegen_get_pv_from_pc ***************************************************

   On this architecture just a wrapper function to
   codegen_get_pv_from_pc.

*******************************************************************************/

u1 *md_codegen_get_pv_from_pc(u1 *ra)
{
	u1 *pv;

	/* Get the start address of the function which contains this
       address from the method table. */

	pv = codegen_get_pv_from_pc(ra);

	return pv;
}


/* md_cacheflush ***************************************************************

   Calls the system's function to flush the instruction and data
   cache.

*******************************************************************************/

void md_cacheflush(u1 *addr, s4 nbytes)
{
	/* do nothing */
}


/* md_icacheflush **************************************************************

   Calls the system's function to flush the instruction cache.

*******************************************************************************/

void md_icacheflush(u1 *addr, s4 nbytes)
{
	/* do nothing */
}


/* md_dcacheflush **************************************************************

   Calls the system's function to flush the data cache.

*******************************************************************************/

void md_dcacheflush(u1 *addr, s4 nbytes)
{
	/* do nothing */
}


/* md_patch_replacement_point **************************************************

   Patch the given replacement point.

*******************************************************************************/
#if 0
void md_patch_replacement_point(rplpoint *rp)
{
    u8 mcode;

	/* XXX this is probably unsafe! */

	/* save the current machine code */
	mcode = *(u8*)rp->pc;

	/* write spinning instruction */
	*(u2*)(rp->pc) = 0xebfe;

	/* write 5th byte */
	rp->pc[4] = (rp->mcode >> 32);

	/* write first word */
    *(u4*)(rp->pc) = (u4) rp->mcode;

	/* store saved mcode */
	rp->mcode = mcode;
	
#if !defined(NDEBUG) && defined(ENABLE_DISASSEMBLER)
	{
		u1* u1ptr = rp->pc;
		DISASSINSTR(u1ptr);
		fflush(stdout);
	}
#endif
			
    /* XXX if required asm_cacheflush(rp->pc,8); */
}
#endif
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

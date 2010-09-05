/* src/vm/jit/x86_64/linux/md-os.c - machine dependent x86_64 Linux functions

   Copyright (C) 2007 R. Grafl, A. Krall, C. Kruegel,
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

   $Id: md.c 7249 2007-01-29 19:32:52Z twisti $

*/


#define _GNU_SOURCE

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>

#include "vm/types.h"

#include "vm/jit/x86_64/codegen.h"

#if defined(ENABLE_THREADS)
# include "threads/native/threads.h"
#endif

#include "vm/exceptions.h"
#include "vm/signallocal.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/stacktrace.h"


/* md_signal_handler_sigsegv ***************************************************

   Signal handler for hardware exception.

*******************************************************************************/

void md_signal_handler_sigsegv(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t        *_uc;
	mcontext_t        *_mc;
	u1                *sp;
	u1                *ra;
	u1                *xpc;
	u1                 opc;
	u1                 mod;
	u1                 rm;
	s4                 d;
	s4                 disp;
	s4                 type;
	ptrint             val;
	java_objectheader *o;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	/* ATTENTION: Don't use CACAO's internal REG_* defines as they are
	   different to the ones in <ucontext.h>. */

	sp  = (u1 *) _mc->gregs[REG_RSP];
	xpc = (u1 *) _mc->gregs[REG_RIP];
	ra  = xpc;                          /* return address is equal to xpc     */

#if 0
	/* check for StackOverflowException */

	threads_check_stackoverflow(sp);
#endif

	/* get exception-throwing instruction */

	opc = M_ALD_MEM_GET_OPC(xpc);
	mod = M_ALD_MEM_GET_MOD(xpc);
	rm  = M_ALD_MEM_GET_RM(xpc);

	/* for values see emit_mov_mem_reg and emit_mem */

	if ((opc == 0x8b) && (mod == 0) && (rm == 4)) {
		/* this was a hardware-exception */

		d    = M_ALD_MEM_GET_REG(xpc);
		disp = M_ALD_MEM_GET_DISP(xpc);

		/* we use the exception type as load displacement */

		type = disp;

		/* XXX FIX ME! */

		/* ATTENTION: The _mc->gregs layout is even worse than on
		   i386! See /usr/include/sys/ucontext.h.  We need a
		   switch-case here... */

		switch (d) {
		case 0:  /* REG_RAX == 13 */
			d = REG_RAX;
			break;
		case 1:  /* REG_RCX == 14 */
			d = REG_RCX;
			break;
		case 2:  /* REG_RDX == 12 */
			d = REG_RDX;
			break;
		case 3:  /* REG_RBX == 11 */
			d = REG_RBX;
			break;
		case 4:  /* REG_RSP == 15 */
			d = REG_RSP;
			break;
		case 5:  /* REG_RBP == 10 */
			d = REG_RBP;
			break;
		case 6:  /* REG_RSI == 9  */
			d = REG_RSI;
			break;
		case 7:  /* REG_RDI == 8  */
			d = REG_RDI;
			break;
		case 8:  /* REG_R8  == 0  */
		case 9:  /* REG_R9  == 1  */
		case 10: /* REG_R10 == 2  */
		case 11: /* REG_R11 == 3  */
		case 12: /* REG_R12 == 4  */
		case 13: /* REG_R13 == 5  */
		case 14: /* REG_R14 == 6  */
		case 15: /* REG_R15 == 7  */
			d = d - 8;
			break;
		}

		val = _mc->gregs[d];
	}
	else {
		/* this was a normal NPE */

		type = EXCEPTION_HARDWARE_NULLPOINTER;
	}

	/* generate appropriate exception */

	o = exceptions_new_hardware_exception(NULL, sp, ra, xpc, type, val);

	/* set registers */

	_mc->gregs[REG_RAX] = (ptrint) o;
	_mc->gregs[REG_R10] = (ptrint) xpc;                      /* REG_ITMP2_XPC */
	_mc->gregs[REG_RIP] = (ptrint) asm_handle_exception;
}


/* md_signal_handler_sigfpe ****************************************************

   ArithmeticException signal handler for hardware divide by zero
   check.

*******************************************************************************/

void md_signal_handler_sigfpe(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t        *_uc;
	mcontext_t        *_mc;
	u1                *pv;
	u1                *sp;
	u1                *ra;
	u1                *xpc;
	s4                 type;
	ptrint             val;
	java_objectheader *o;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	/* ATTENTION: Don't use CACAO's internal REG_* defines as they are
	   different to the ones in <ucontext.h>. */

	pv  = NULL;
	sp  = (u1 *) _mc->gregs[REG_RSP];
	xpc = (u1 *) _mc->gregs[REG_RIP];
	ra  = xpc;                          /* return address is equal to xpc     */

	/* this is an ArithmeticException */

	type = EXCEPTION_HARDWARE_ARITHMETIC;
	val  = 0;

	/* generate appropriate exception */

	o = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

	/* set registers */

	_mc->gregs[REG_RAX] = (ptrint) o;
	_mc->gregs[REG_R10] = (ptrint) xpc;                      /* REG_ITMP2_XPC */
	_mc->gregs[REG_RIP] = (ptrint) asm_handle_exception;
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

	pc = (u1 *) _mc->gregs[REG_RIP];

	t->pc = pc;
}
#endif


/* md_critical_section_restart *************************************************

   Search the critical sections tree for a matching section and set
   the PC to the restart point, if necessary.

*******************************************************************************/

#if defined(ENABLE_THREADS)
void md_critical_section_restart(ucontext_t *_uc)
{
	mcontext_t *_mc;
	u1         *pc;
	void       *npc;

	_mc = &_uc->uc_mcontext;

	/* ATTENTION: Don't use CACAO's internal REG_* defines as they are
	   different to the ones in <ucontext.h>. */

	pc = (u1 *) _mc->gregs[REG_RIP];

	npc = critical_find_restart_point(pc);

	if (npc != NULL) {
		log_println("md_critical_section_restart: pc=%p, npc=%p", pc, npc);
		_mc->gregs[REG_RIP] = (ptrint) npc;
	}
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

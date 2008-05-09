/* src/vm/jit/x86_64/freebsd/md-os.c - machine dependent x86_64 FreeBSD functions

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


#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>

#if defined(ENABLE_THREADS)
# include "threads/native/threads.h"
#endif

#include "vm/signallocal.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/stacktrace.h"


/* md_signal_handler_sigsegv ***************************************************

   NullPointerException signal handler for hardware null pointer
   check.

*******************************************************************************/

void md_signal_handler_sigsegv(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t *_uc;
	mcontext_t *_mc;
	u1         *sp;
	u1         *ra;
	u1         *xpc;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	sp  = (u1 *) _mc->mc_rsp;
	xpc = (u1 *) _mc->mc_rip;
	ra  = xpc;                          /* return address is equal to xpc     */

#if 0
	/* check for StackOverflowException */

	threads_check_stackoverflow(sp);
#endif

	_mc->mc_rax =
		(ptrint) stacktrace_hardware_nullpointerexception(NULL, sp, ra, xpc);

	_mc->mc_r10 = (ptrint) xpc;                              /* REG_ITMP2_XPC */
	_mc->mc_rip = (ptrint) asm_handle_exception;
}


/* md_signal_handler_sigfpe ****************************************************

   ArithmeticException signal handler for hardware divide by zero
   check.

*******************************************************************************/

void md_signal_handler_sigfpe(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t  *_uc;
	mcontext_t  *_mc;
	u1          *sp;
	u1          *ra;
	u1          *xpc;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	sp  = (u1 *) _mc->mc_rsp;
	xpc = (u1 *) _mc->mc_rip;
	ra  = xpc;                          /* return address is equal to xpc     */

	_mc->mc_rax =
		(ptrint) stacktrace_hardware_arithmeticexception(NULL, sp, ra, xpc);

	_mc->mc_r10 = (ptrint) xpc;                              /* REG_ITMP2_XPC */
	_mc->mc_rip = (ptrint) asm_handle_exception;
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

	pc = (u1 *) _mc->mc_rip;

	t->pc = pc;
}
#endif


#if defined(ENABLE_THREADS)
void thread_restartcriticalsection(ucontext_t *_uc)
{
	mcontext_t *_mc;
	u1         *pc;
	void       *critical;

	_mc = &_uc->uc_mcontext;

	pc = (u1 *) _mc->mc_rip;

	critical = critical_find_restart_point(pc);

	if (critical != NULL)
		_mc->mc_rip = (ptrint) critical;
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

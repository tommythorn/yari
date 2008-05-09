/* src/vm/jit/i386/freebsd/md-os.c - machine dependent i386 FreeBSD functions

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

   Changes:

   $Id: md-os.c 4921 2006-05-15 14:24:36Z twisti $

*/


#include "config.h"

#include <ucontext.h>

#include "vm/jit/i386/md-abi.h"

#include "vm/exceptions.h"
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
	
	sp  = (u1 *) _mc->mc_esp;
	xpc = (u1 *) _mc->mc_eip;
	ra  = xpc;                          /* return address is equal to xpc     */

	_mc->mc_eax =
		(ptrint) stacktrace_hardware_nullpointerexception(NULL, sp, ra, xpc);
	
	_mc->mc_ecx = (ptrint) xpc;                              /* REG_ITMP2_XPC */
	_mc->mc_eip = (ptrint) asm_handle_exception;
}


/* md_signal_handler_sigfpe ****************************************************

   ArithmeticException signal handler for hardware divide by zero
   check.

*******************************************************************************/

void md_signal_handler_sigfpe(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t *_uc;
	mcontext_t *_mc;
	u1         *sp;
	u1         *ra;
	u1         *xpc;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	sp  = (u1 *) _mc->mc_esp;
	xpc = (u1 *) _mc->mc_eip;
	ra  = xpc;                          /* return address is equal to xpc     */

	_mc->mc_eax =
		(ptrint) stacktrace_hardware_arithmeticexception(NULL, sp, ra, xpc);
	
	_mc->mc_ecx = (ptrint) xpc;                              /* REG_ITMP2_XPC */
	_mc->mc_eip = (ptrint) asm_handle_exception;
}


#if defined(ENABLE_THREADS)
void thread_restartcriticalsection(ucontext_t *uc)
{
	void *critical;

	critical = critical_find_restart_point((void *) uc->uc_mcontext.mc_eip);

	if (critical)
		uc->uc_mcontext.mc_eip = (ptrint) critical;
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
 */

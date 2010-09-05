/* src/vm/jit/i386/linux/md-os.c - machine dependent i386 Linux functions

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

   $Id: md-os.c 7865 2007-05-03 21:29:40Z twisti $

*/


#define _GNU_SOURCE                   /* include REG_ defines from ucontext.h */

#include "config.h"

#include <ucontext.h>

#include "vm/types.h"

#include "vm/jit/i386/codegen.h"

#include "threads/threads-common.h"

#include "vm/exceptions.h"
#include "vm/signallocal.h"
#include "vm/stringlocal.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/stacktrace.h"


/* md_signal_handler_sigsegv ***************************************************

   Signal handler for hardware exceptions.

*******************************************************************************/

void md_signal_handler_sigsegv(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t        *_uc;
	mcontext_t        *_mc;
	u1                *pv;
	u1                *sp;
	u1                *ra;
	u1                *xpc;
	u1                 opc;
	u1                 mod;
	u1                 rm;
	s4                 d;
	s4                 disp;
	ptrint             val;
	s4                 type;
	java_objectheader *o;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	pv  = NULL;                 /* is resolved during stackframeinfo creation */
	sp  = (u1 *) _mc->gregs[REG_ESP];
	xpc = (u1 *) _mc->gregs[REG_EIP];
	ra  = xpc;                              /* return address is equal to XPC */

	/* get exception-throwing instruction */

	opc = M_ALD_MEM_GET_OPC(xpc);
	mod = M_ALD_MEM_GET_MOD(xpc);
	rm  = M_ALD_MEM_GET_RM(xpc);

	/* for values see emit_mov_mem_reg and emit_mem */

	if ((opc == 0x8b) && (mod == 0) && (rm == 5)) {
		/* this was a hardware-exception */

		d    = M_ALD_MEM_GET_REG(xpc);
		disp = M_ALD_MEM_GET_DISP(xpc);

		/* we use the exception type as load displacement */

		type = disp;

		/* ATTENTION: The _mc->gregs layout is completely crazy!  The
		   registers are reversed starting with number 4 for REG_EDI
		   (see /usr/include/sys/ucontext.h).  We have to convert that
		   here. */

		val = _mc->gregs[REG_EAX - d];
	}
	else {
		/* this was a normal NPE */

		type = EXCEPTION_HARDWARE_NULLPOINTER;
	}

	/* generate appropriate exception */

	o = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

	/* set registers */

	_mc->gregs[REG_EAX] = (ptrint) o;
	_mc->gregs[REG_ECX] = (ptrint) xpc;                      /* REG_ITMP2_XPC */
	_mc->gregs[REG_EIP] = (ptrint) asm_handle_exception;
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

	pv  = NULL;                 /* is resolved during stackframeinfo creation */
	sp  = (u1 *) _mc->gregs[REG_ESP];
	xpc = (u1 *) _mc->gregs[REG_EIP];
	ra  = xpc;                          /* return address is equal to xpc     */

	/* this is an ArithmeticException */

	type = EXCEPTION_HARDWARE_ARITHMETIC;
	val  = 0;

	/* generate appropriate exception */

	o = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

	_mc->gregs[REG_EAX] = (ptrint) o;
	_mc->gregs[REG_ECX] = (ptrint) xpc;                      /* REG_ITMP2_XPC */
	_mc->gregs[REG_EIP] = (ptrint) asm_handle_exception;
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

	pc = (u1 *) _mc->gregs[REG_EIP];

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
	mcontext_t    *_mc;
	u1            *pc;
	void          *npc;

	_mc = &_uc->uc_mcontext;

	pc = (u1 *) _mc->gregs[REG_EIP];

	npc = critical_find_restart_point(pc);

	if (npc != NULL) {
		log_println("md_critical_section_restart: pc=%p, npc=%p", pc, npc);
		_mc->gregs[REG_EIP] = (ptrint) npc;
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
 */

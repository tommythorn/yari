/* src/vm/jit/powerpc/linux/md-os.c - machine dependent PowerPC Linux functions

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

   $Id: md-os.c 7864 2007-05-03 21:17:26Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <ucontext.h>

#include "vm/types.h"

#include "vm/jit/powerpc/codegen.h"
#include "vm/jit/powerpc/linux/md-abi.h"

#if defined(ENABLE_THREADS)
# include "threads/native/threads.h"
#endif

#include "vm/exceptions.h"
#include "vm/signallocal.h"
#include "vm/stringlocal.h"
#include "vm/jit/asmpart.h"

#if defined(ENABLE_PROFILING)
# include "vm/jit/optimizing/profile.h"
#endif

#include "vm/jit/stacktrace.h"


/* md_signal_handler_sigsegv ***************************************************

   Signal handler for hardware-exceptions.

*******************************************************************************/

void md_signal_handler_sigsegv(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t        *_uc;
	mcontext_t        *_mc;
	unsigned long     *_gregs;
	u1                *pv;
	u1                *sp;
	u1                *ra;
	u1                *xpc;
	u4                 mcode;
	s4                 s1;
	s4                 disp;
	s4                 d;
	ptrint             addr;
	ptrint             val;
	s4                 type;
	java_objectheader *o;

 	_uc = (ucontext_t *) _p;

#if defined(__UCLIBC__)
	_mc    = &(_uc->uc_mcontext);
	_gregs = _mc->regs->gpr;
#else
	_mc    = _uc->uc_mcontext.uc_regs;
	_gregs = _mc->gregs;
#endif

	pv  = (u1 *) _gregs[REG_PV];
	sp  = (u1 *) _gregs[REG_SP];
	ra  = (u1 *) _gregs[PT_LNK];                 /* this is correct for leafs */
	xpc = (u1 *) _gregs[PT_NIP];

	/* get exception-throwing instruction */

	mcode = *((u4 *) xpc);

	s1   = M_INSTR_OP2_IMM_A(mcode);
	disp = M_INSTR_OP2_IMM_I(mcode);
	d    = M_INSTR_OP2_IMM_D(mcode);

	val  = _gregs[d];

	/* check for special-load */

	if (s1 == REG_ZERO) {
		/* we use the exception type as load displacement */

		type = disp;
	}
	else {
		/* This is a normal NPE: addr must be NULL and the NPE-type
		   define is 0. */

		addr = _gregs[s1];
		type = EXCEPTION_HARDWARE_NULLPOINTER;

		if (addr != 0)
			vm_abort("md_signal_handler_sigsegv: faulting address is not NULL: addr=%p", addr);
	}

	/* generate appropriate exception */

	o = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

	/* set registers */

	_gregs[REG_ITMP1_XPTR] = (ptrint) o;
	_gregs[REG_ITMP2_XPC]  = (ptrint) xpc;
	_gregs[PT_NIP]         = (ptrint) asm_handle_exception;
}


/* md_signal_handler_sigtrap ***************************************************

   Signal handler for hardware-traps.

*******************************************************************************/

void md_signal_handler_sigtrap(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t        *_uc;
	mcontext_t        *_mc;
	unsigned long     *_gregs;
	u1                *pv;
	u1                *sp;
	u1                *ra;
	u1                *xpc;
	u4                 mcode;
	s4                 s1;
	ptrint             val;
	s4                 type;
	java_objectheader *o;

 	_uc = (ucontext_t *) _p;

#if defined(__UCLIBC__)
	_mc    = &(_uc->uc_mcontext);
	_gregs = _mc->regs->gpr;
#else
	_mc    = _uc->uc_mcontext.uc_regs;
	_gregs = _mc->gregs;
#endif

	pv  = (u1 *) _gregs[REG_PV];
	sp  = (u1 *) _gregs[REG_SP];
	ra  = (u1 *) _gregs[PT_LNK];                 /* this is correct for leafs */
	xpc = (u1 *) _gregs[PT_NIP];

	/* get exception-throwing instruction */

	mcode = *((u4 *) xpc);

	s1 = M_OP3_GET_A(mcode);

	/* for now we only handle ArrayIndexOutOfBoundsException */

	type = EXCEPTION_HARDWARE_ARRAYINDEXOUTOFBOUNDS;
	val  = _gregs[s1];

	/* generate appropriate exception */

	o = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

	/* set registers */

	_gregs[REG_ITMP1_XPTR] = (ptrint) o;
	_gregs[REG_ITMP2_XPC]  = (ptrint) xpc;
	_gregs[PT_NIP]         = (ptrint) asm_handle_exception;
}


/* md_signal_handler_sigusr2 ***************************************************

   Signal handler for profiling sampling.

*******************************************************************************/

#if defined(ENABLE_THREADS)
void md_signal_handler_sigusr2(int sig, siginfo_t *siginfo, void *_p)
{
	threadobject  *tobj;
	ucontext_t    *_uc;
	mcontext_t    *_mc;
	unsigned long *_gregs;
	u1            *pc;

	tobj = THREADOBJECT;

 	_uc = (ucontext_t *) _p;

#if defined(__UCLIBC__)
	_mc    = &(_uc->uc_mcontext);
	_gregs = _mc->regs->gpr;
#else
	_mc    = _uc->uc_mcontext.uc_regs;
	_gregs = _mc->gregs;
#endif

	pc = (u1 *) _gregs[PT_NIP];

	tobj->pc = pc;
}


/* md_critical_section_restart *************************************************

   Search the critical sections tree for a matching section and set
   the PC to the restart point, if necessary.

*******************************************************************************/

void md_critical_section_restart(ucontext_t *_uc)
{
	mcontext_t    *_mc;
	unsigned long *_gregs;
	u1            *pc;
	u1            *npc;

#if defined(__UCLIBC__)
	_mc    = &(_uc->uc_mcontext);
	_gregs = _mc->regs->gpr;
#else
	_mc    = _uc->uc_mcontext.uc_regs;
	_gregs = _mc->gregs;
#endif

	pc = (u1 *) _gregs[PT_NIP];

	npc = critical_find_restart_point(pc);

	if (npc != NULL) {
		log_println("md_critical_section_restart: pc=%p, npc=%p", pc, npc);

		_gregs[PT_NIP] = (ptrint) npc;
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

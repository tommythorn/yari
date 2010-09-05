/* src/vm/jit/alpha/linux/md-os.c - machine dependent Alpha Linux functions

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

   $Id: md-os.c 7886 2007-05-07 21:34:01Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <ucontext.h>

#include "vm/types.h"

#include "vm/jit/alpha/codegen.h"
#include "vm/jit/alpha/md-abi.h"

#if defined(ENABLE_THREADS)
# include "threads/native/threads.h"
#endif

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
	ucontext_t        *_uc;
	mcontext_t        *_mc;
	u1                *pv;
	u1                *sp;
	u1                *ra;
	u1                *xpc;
	u4                 mcode;
	s4                 d;
	s4                 s1;
	s4                 disp;
	ptrint             val;
	ptrint             addr;
	s4                 type;
	java_objectheader *e;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	pv  = (u1 *) _mc->sc_regs[REG_PV];
	sp  = (u1 *) _mc->sc_regs[REG_SP];
	ra  = (u1 *) _mc->sc_regs[REG_RA];           /* this is correct for leafs */
	xpc = (u1 *) _mc->sc_pc;

	/* get exception-throwing instruction */

	mcode = *((u4 *) xpc);

	d    = M_MEM_GET_A(mcode);
	s1   = M_MEM_GET_B(mcode);
	disp = M_MEM_GET_DISP(mcode);

	val   = _mc->sc_regs[d];

	/* check for special-load */

	if (s1 == REG_ZERO) {
		/* we use the exception type as load displacement */

		type = disp;
	}
	else {
		/* This is a normal NPE: addr must be NULL and the NPE-type
		   define is 0. */

		addr = _mc->sc_regs[s1];
		type = (s4) addr;
	}

	/* generate appropriate exception */

	e = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

	/* set registers */

	_mc->sc_regs[REG_ITMP1_XPTR] = (ptrint) e;
	_mc->sc_regs[REG_ITMP2_XPC]  = (ptrint) xpc;
	_mc->sc_pc                   = (ptrint) asm_handle_exception;
}


/* md_signal_handler_sigusr2 ***************************************************

   Signal handler for profiling sampling.

*******************************************************************************/

#if defined(ENABLE_THREADS)
void md_signal_handler_sigusr2(int sig, siginfo_t *siginfo, void *_p)
{
	threadobject *tobj;
	ucontext_t   *_uc;
	mcontext_t   *_mc;
	u1           *pc;

	tobj = THREADOBJECT;

	_uc = (ucontext_t *) _p;
	_mc = &_uc->uc_mcontext;

	pc = (u1 *) _mc->sc_pc;

	tobj->pc = pc;
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
	u1         *npc;

	_mc = &_uc->uc_mcontext;

	pc = (u1 *) _mc->sc_pc;

	npc = critical_find_restart_point(pc);

	if (npc != NULL) {
		log_println("md_critical_section_restart: pc=%p, npc=%p", pc, npc);
		_mc->sc_pc = (ptrint) npc;
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

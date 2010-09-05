/* src/vm/jit/i386/darwin/md-os.c - machine dependent i386 Darwin functions

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

   $Id: md-os.c 5074 2006-07-04 16:05:35Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <signal.h>
#include <ucontext.h>

#include "vm/types.h"

#include "vm/jit/i386/codegen.h"

#include "threads/threads-common.h"

#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/signallocal.h"
#include "vm/stringlocal.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/stacktrace.h"

#include "vm/jit/i386/codegen.h"

#if MAC_OS_X_VERSION < MAC_OS_X_VERSION_10_5
/* Trying to support compatibility between versions of Mac OS X isn't
   worth the pain */
#error Sorry, this has been hacked to only run on Leopard
#endif

/* md_signal_handler_sigsegv ***************************************************

   Signal handler for hardware exceptions.

*******************************************************************************/

void md_signal_handler_sigsegv(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t          *_uc;
	mcontext_t           _mc;
	u1                  *pv;
	_STRUCT_X86_THREAD_STATE32 *_ss;
	u1                  *sp;
	u1                  *ra;
	u1                  *xpc;
    u1                   opc;
    u1                   mod;
    u1                   rm;
    s4                   d;
    s4                   disp;
    ptrint               val;
    s4                   type;
    java_objectheader   *o;

	_uc = (ucontext_t *) _p;
	_mc = _uc->uc_mcontext;
	_ss = &_mc->__ss;

    pv  = NULL;                 /* is resolved during stackframeinfo creation */
	sp  = (u1 *) _ss->__esp;
	xpc = (u1 *) _ss->__eip;
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

        val = (d == 0) ? _ss->__eax :
            ((d == 1) ? _ss->__ecx :
            ((d == 2) ? _ss->__edx :
            ((d == 3) ? _ss->__ebx :
            ((d == 4) ? _ss->__esp :
            ((d == 5) ? _ss->__ebp :
            ((d == 6) ? _ss->__esi : _ss->__edi))))));
    }
    else {
        /* this was a normal NPE */

        type = EXCEPTION_HARDWARE_NULLPOINTER;
    }

    /* generate appropriate exception */

    o = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

    /* set registers */

    _ss->__eax = (ptrint) o;
	_ss->__ecx = (ptrint) xpc;
	_ss->__eip = (ptrint) asm_handle_exception;
}


/* md_signal_handler_sigfpe ****************************************************

   ArithmeticException signal handler for hardware divide by zero
   check.

*******************************************************************************/

void md_signal_handler_sigfpe(int sig, siginfo_t *siginfo, void *_p)
{
	ucontext_t          *_uc;
	mcontext_t           _mc;
    u1                  *pv;
	_STRUCT_X86_THREAD_STATE32 *_ss;
	u1                  *sp;
	u1                  *ra;
	u1                  *xpc;
    s4                   type;
    ptrint               val;
    java_objectheader   *o;


	_uc = (ucontext_t *) _p;
	_mc = _uc->uc_mcontext;
	_ss = &_mc->__ss;

    pv  = NULL;                 /* is resolved during stackframeinfo creation */
	sp  = (u1 *) _ss->__esp;
	xpc = (u1 *) _ss->__eip;
	ra  = xpc;                          /* return address is equal to xpc     */

    /* this is an ArithmeticException */

    type = EXCEPTION_HARDWARE_ARITHMETIC;
    val  = 0;

    /* generate appropriate exception */

    o = exceptions_new_hardware_exception(pv, sp, ra, xpc, type, val);

    _ss->__eax = (ptrint) o;
	_ss->__ecx = (ptrint) xpc;
	_ss->__eip = (ptrint) asm_handle_exception;
}


/* md_signal_handler_sigusr2 ***************************************************

   Signal handler for profiling sampling.

*******************************************************************************/

void md_signal_handler_sigusr2(int sig, siginfo_t *siginfo, void *_p)
{
	threadobject        *t;
	ucontext_t          *_uc;
	mcontext_t           _mc;
	_STRUCT_X86_THREAD_STATE32 *_ss;
	u1                  *pc;

	t = THREADOBJECT;

	_uc = (ucontext_t *) _p;
	_mc = _uc->uc_mcontext;
	_ss = &_mc->__ss;

	pc = (u1 *) _ss->__eip;

	/* t->__pc = pc;

	   I don't know why I get THREADOBJECT of NULL, but apparently
	   it's only used for profiling */
}


#if defined(ENABLE_THREADS)
void thread_restartcriticalsection(ucontext_t *_uc)
{
	mcontext_t           _mc;
	_STRUCT_X86_THREAD_STATE32 *_ss;
	u1                  *pc;
	void                *rpc;

	_mc = _uc->uc_mcontext;
	_ss = &_mc->ss;

	pc = (u1 *) _ss->__eip;

	rpc = critical_find_restart_point(pc);

	if (rpc != NULL)
		_ss->__eip = (ptrint) rpc;
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

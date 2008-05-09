/* src/vm/signal.c - machine independent signal functions

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

   $Id: signal.c 7831M 2007-05-07 19:33:16Z (local) $

*/


#include "config.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(HAVE_SYS_MMAN_H)
#include <sys/mman.h>
#endif

#if defined(__DARWIN__)
/* If we compile with -ansi on darwin, <sys/types.h> is not
 included. So let's do it here. */
# include <sys/types.h>
#endif

#include "vm/types.h"

#include "arch.h"

#include "mm/memory.h"

#if defined(ENABLE_THREADS)
# include "threads/threads-common.h"
#endif

#include "vm/exceptions.h"
#include "vm/signallocal.h"
#include "vm/vm.h"

#include "vmcore/options.h"

#if defined(ENABLE_STATISTICS)
# include "vmcore/statistics.h"
#endif

/* function prototypes ********************************************************/

void signal_handler_sighup(int sig, siginfo_t *siginfo, void *_p);


/* signal_init *****************************************************************

   Initializes the signal subsystem and installs the signal handler.

*******************************************************************************/

void signal_init(void)
{
#if !defined(__CYGWIN__)  && 0
	int              pagesize;
	sigset_t         mask;
	struct sigaction act;

	/* mmap a memory page at address 0x0, so our hardware-exceptions
	   work. */

	pagesize = getpagesize();

	(void) memory_mmap_anon(NULL, pagesize, PROT_NONE, MAP_PRIVATE | MAP_FIXED);

	/* check if we get into trouble with our hardware-exceptions */

	assert(OFFSET(java_bytearray, data) > EXCEPTION_HARDWARE_PATCHER);

	/* Block the following signals (SIGINT for <ctrl>-c, SIGQUIT for
	   <ctrl>-\).  We enable them later in signal_thread, but only for
	   this thread. */

	if (sigemptyset(&mask) != 0)
		vm_abort("signal_init: sigemptyset failed: %s", strerror(errno));

	if (sigaddset(&mask, SIGINT) != 0)
		vm_abort("signal_init: sigaddset failed: %s", strerror(errno));

#if !defined(__FREEBSD__)
	if (sigaddset(&mask, SIGQUIT) != 0)
		vm_abort("signal_init: sigaddset failed: %s", strerror(errno));
#endif

	if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0)
		vm_abort("signal_init: sigprocmask failed: %s", strerror(errno));

#if defined(ENABLE_GC_BOEHM)
	/* Allocate something so the garbage collector's signal handlers
	   are installed. */

	(void) GCNEW(u1);
#endif

	/* Install signal handlers for signals we want to catch in all
	   threads. */

	sigemptyset(&act.sa_mask);

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
	if (!opt_intrp) {
# endif
		/* SIGSEGV handler */

		act.sa_sigaction = md_signal_handler_sigsegv;
		act.sa_flags     = SA_NODEFER | SA_SIGINFO;

#  if defined(SIGSEGV)
		sigaction(SIGSEGV, &act, NULL);
#  endif

#  if defined(SIGBUS)
		sigaction(SIGBUS, &act, NULL);
#  endif

#  if SUPPORT_HARDWARE_DIVIDE_BY_ZERO
		/* SIGFPE handler */

		act.sa_sigaction = md_signal_handler_sigfpe;
		act.sa_flags     = SA_NODEFER | SA_SIGINFO;
		sigaction(SIGFPE, &act, NULL);
#  endif

#  if defined(__ARM__)
		/* XXX use better defines for that (in arch.h) */
		/* SIGILL handler */

		act.sa_sigaction = md_signal_handler_sigill;
		act.sa_flags     = SA_NODEFER | SA_SIGINFO;
		sigaction(SIGILL, &act, NULL);
#  endif

#  if defined(__POWERPC__)
		/* XXX use better defines for that (in arch.h) */
		/* SIGTRAP handler */

		act.sa_sigaction = md_signal_handler_sigtrap;
		act.sa_flags     = SA_NODEFER | SA_SIGINFO;
		sigaction(SIGTRAP, &act, NULL);
#  endif
# if defined(ENABLE_INTRP)
	}
# endif
#endif /* !defined(ENABLE_INTRP) */

#if defined(ENABLE_THREADS)
	/* SIGHUP handler for threads_thread_interrupt */

	act.sa_sigaction = signal_handler_sighup;
	act.sa_flags     = 0;
	sigaction(SIGHUP, &act, NULL);
#endif

#if defined(ENABLE_THREADS) && defined(ENABLE_PROFILING)
	/* SIGUSR2 handler for profiling sampling */

	act.sa_sigaction = md_signal_handler_sigusr2;
	act.sa_flags     = SA_SIGINFO;
	sigaction(SIGUSR2, &act, NULL);
#endif

#endif /* !defined(__CYGWIN__) */
}


/* signal_thread ************************************************************

   This thread sets the signal mask to catch the user input signals
   (SIGINT, SIGQUIT).  We use such a thread, so we don't get the
   signals on every single thread running.  Especially, this makes
   problems on slow machines.

*******************************************************************************/


#if defined(ENABLE_THREADS)
static void signal_thread(void)
{
	sigset_t mask;
	int      sig;

	if (sigemptyset(&mask) != 0)
		vm_abort("signal_thread: sigemptyset failed: %s", strerror(errno));

	if (sigaddset(&mask, SIGINT) != 0)
		vm_abort("signal_thread: sigaddset failed: %s", strerror(errno));

#if !defined(__FREEBSD__)
	if (sigaddset(&mask, SIGQUIT) != 0)
		vm_abort("signal_thread: sigaddset failed: %s", strerror(errno));
#endif

	while (true) {
		/* just wait for a signal */

		/* XXX We don't check for an error here, although the man-page
		   states sigwait does not return an error (which is wrong!),
		   but it seems to make problems with Boehm-GC.  We should
		   revisit this code with our new exact-GC. */

/* 		if (sigwait(&mask, &sig) != 0) */
/* 			vm_abort("signal_thread: sigwait failed: %s", strerror(errno)); */
		(void) sigwait(&mask, &sig);

		switch (sig) {
		case SIGINT:
			/* exit the vm properly */

			vm_exit(0);
			break;

		case SIGQUIT:
			/* print a thread dump */
#if defined(ENABLE_THREADS)
			threads_dump();
#endif

#if defined(ENABLE_STATISTICS)
			if (opt_stat)
				statistics_print_memory_usage();
#endif
			break;
		}
	}

	/* this should not happen */

	vm_abort("signal_thread: this thread should not exit!");
}


/* signal_start_thread *********************************************************

   Starts the signal handler thread.

*******************************************************************************/

bool signal_start_thread(void)
{
#if defined(ENABLE_THREADS)
	utf *name;

	name = utf_new_char("Signal Handler");

	if (!threads_thread_start_internal(name, signal_thread))
		return false;

	/* everything's ok */

	return true;
#else
#warning FIX ME!
#endif
}
#endif


/* signal_handler_sighup *******************************************************

   This handler is required by threads_thread_interrupt and does
   nothing.

*******************************************************************************/

#if defined(ENABLE_THREADS)
void signal_handler_sighup(int sig, siginfo_t *siginfo, void *_p)
{
	/* do nothing */
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

/* src/vm/signallocal.h - machine independent signal functions

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

   $Id: signallocal.h 7596M 2007-05-07 20:21:25Z (local) $

*/


#ifndef _CACAO_SIGNAL_H
#define _CACAO_SIGNAL_H

#include "config.h"

#include <signal.h>

#include "vm/types.h"

#include "vm/global.h"

#if defined(__MIPS__)
typedef void siginfo_t;
#endif

/* function prototypes ********************************************************/

void signal_init(void);
bool signal_start_thread(void);

/* machine dependent signal handler */

void md_signal_handler_sigsegv(int sig, siginfo_t *siginfo, void *_p);

#if SUPPORT_HARDWARE_DIVIDE_BY_ZERO
void md_signal_handler_sigfpe(int sig, siginfo_t *siginfo, void *_p);
#endif

#if defined(__ARM__)
/* XXX use better defines for that (in arch.h) */
void md_signal_handler_sigill(int sig, siginfo_t *siginfo, void *_p);
#endif

#if defined(__POWERPC__)
/* XXX use better defines for that (in arch.h) */
void md_signal_handler_sigtrap(int sig, siginfo_t *siginfo, void *_p);
#endif

void md_signal_handler_sigusr2(int sig, siginfo_t *siginfo, void *_p);

#endif /* _CACAO_SIGNAL_H */


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

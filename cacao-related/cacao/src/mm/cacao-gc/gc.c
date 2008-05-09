/* src/mm/cacao-gc/gc.c - main garbage collector methods

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

   Authors: Michael Starzinger

   $Id$

*/


#include "config.h"
#include <signal.h>
#include <stdlib.h>
#include "vm/types.h"

#if defined(ENABLE_THREADS)
# include "threads/native/threads.h"
#else
# include "threads/none/threads.h"
#endif

#include "gc.h"
#include "heap.h"
#include "mark.h"
#include "mm/memory.h"
#include "toolbox/logging.h"
#include "vm/exceptions.h"
#include "vm/options.h"


/* Development Break **********************************************************/

#if defined(ENABLE_THREADS)
# error "GC does not work with threads enabled!"
#endif

#if defined(ENABLE_INTRP)
# error "GC does not work with interpreter enabled!"
#endif

#if defined(ENABLE_JVMTI)
# error "GC does not work with JVMTI enabled!"
#endif


/* gc_init *********************************************************************

   Initializes the garbage collector.

*******************************************************************************/

void gc_init(u4 heapmaxsize, u4 heapstartsize)
{
	if (opt_verbosegc)
		dolog("GC: Initialising with heap-size %d (max. %d)",
			heapstartsize, heapmaxsize);

	heap_base = malloc(heapstartsize);

	if (heap_base == NULL)
		exceptions_throw_outofmemory_exit();

	/* this is needed for linear allocation */
	heap_ptr = heap_base;

	heap_current_size = heapstartsize;
	heap_maximal_size = heapmaxsize;
	heap_free_size = heap_current_size;
	heap_used_size = 0;
}


/* gc_call *********************************************************************

   Forces a full collection of the whole Java Heap.
   This is the function which is called by System.VMRuntime.gc()

*******************************************************************************/

void gc_call(void)
{
	rootset_t *rs;
	s4         dumpsize;

	if (opt_verbosegc)
		dolog("GC: Forced Collection ...");

	/* TODO: move the following to gc_collect() */

	/* remember start of dump memory area */
	dumpsize = dump_size();

	GC_LOG( heap_println_usage(); );
	/*GC_LOG( heap_dump_region(heap_base, heap_ptr, false); );*/

	/* find the rootset for the current thread */
	rs = DNEW(rootset_t);
	mark_rootset_from_thread(THREADOBJECT, rs);

	/* mark the objects considering the given rootset */
	mark_me(rs);
	GC_LOG( heap_dump_region(heap_base, heap_ptr, true); );

	/* compact the heap */
	/*compact_me(rs, heap_base, heap_ptr);*/

	/* TODO: check my return value! */
	/*heap_increase_size();*/

    /* free dump memory area */
    dump_release(dumpsize);

	if (opt_verbosegc)
		dolog("GC: Forced Collection finished.");
}


/* Informational getter functions *********************************************/

s8 gc_get_heap_size(void)     { return heap_current_size; }
s8 gc_get_free_bytes(void)    { return heap_free_size; }
s8 gc_get_total_bytes(void)   { return heap_used_size; }
s8 gc_get_max_heap_size(void) { return heap_maximal_size; }


/* Thread specific stuff ******************************************************/

#if defined(ENABLE_THREADS)
int GC_signum1()
{
	return SIGUSR1;
}

int GC_signum2()
{
	return SIGUSR2;
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

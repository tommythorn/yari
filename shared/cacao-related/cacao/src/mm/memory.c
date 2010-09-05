/* src/mm/memory.c - memory management

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

   $Id: memory.c 7831M 2007-05-08 12:29:29Z (local) $

*/


#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#include "native/native.h"

#include "threads/lock-common.h"
#include "threads/threads-common.h"

#include "toolbox/logging.h"

#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vmcore/options.h"

#if defined(ENABLE_STATISTICS)
# include "vmcore/statistics.h"
#endif


/* constants for ENABLE_MEMCHECK **********************************************/

#if defined(ENABLE_MEMCHECK)
#define MEMORY_CANARY_SIZE          16
#define MEMORY_CANARY_FIRST_BYTE    0xca
#define MEMORY_CLEAR_BYTE           0xa5
#endif /* defined(ENABLE_MEMCHECK) */


/*******************************************************************************

  This structure is used for dump memory allocation if cacao
  runs without threads.

*******************************************************************************/

#if !defined(ENABLE_THREADS)
static dumpinfo_t _no_threads_dumpinfo;
#endif

#if defined(ENABLE_THREADS)
#define DUMPINFO    &((threadobject *) THREADOBJECT)->dumpinfo
#else
#define DUMPINFO    &_no_threads_dumpinfo
#endif


/* global code memory variables ***********************************************/

#define DEFAULT_CODE_MEMORY_SIZE    128 * 1024 /* defaulting to 128kB         */

#if defined(ENABLE_THREADS)
static java_objectheader *lock_code_memory = NULL;
#endif
static void              *code_memory      = NULL;
static int                code_memory_size = 0;
static int                pagesize         = 4096;


/* memory_init *****************************************************************

   Initialize the memory subsystem.

*******************************************************************************/

bool memory_init(void)
{
#if defined(ENABLE_THREADS)
	/* create lock for code memory */

	lock_code_memory = NEW(java_objectheader);

	lock_init_object_lock(lock_code_memory);
#endif

	/* get the pagesize of this architecture */

#if defined(HAVE_GETPAGESIZE)
	pagesize = getpagesize();
#endif

	/* everything's ok */

	return true;
}


/* memory_mmap_anon ************************************************************

   Maps anonymous memory, even on systems not defining
   MAP_ANON(YMOUS).

*******************************************************************************/

void *memory_mmap_anon(void *addr, size_t len, int prot, int flags)
{
/*	void *p;

#if defined(MAP_ANON) || defined(MAP_ANONYMOUS)
	p = mmap(addr, len, prot,
# if defined(MAP_ANON)
			 MAP_ANON | flags,
# else
			 MAP_ANONYMOUS | flags,
# endif
			 -1, 0);
#else
	int fd;

	fd = open("/dev/zero", O_RDONLY, 0);

	if (fd == -1)
		vm_abort("memory_mmap_anon: open failed: %s", strerror(errno));

	p = mmap(addr, len, prot, flags, fd, 0);
#endif

#if defined(MAP_FAILED)
	if (p == MAP_FAILED)
#else
	if (p == (void *) -1)
#endif
		vm_abort("memory_mmap_anon: mmap failed: %s", strerror(errno));
*/
	return addr;
}


/* memory_checked_alloc ********************************************************

   Allocated zeroed-out memory and does an OOM check.

   ERROR HANDLING:
      XXX If no memory could be allocated, this function justs *exists*.

*******************************************************************************/

static void *memory_checked_alloc(s4 size)
{
	/* always allocate memory zeroed out */
	void *p = calloc(size, 1);
	if (p == NULL)
		vm_abort("memory_checked_alloc: calloc failed: out of memory");

	return p;
}


/* memory_cnew *****************************************************************

   Allocates memory from the heap via mmap and make the memory read-,
   write-, and executeable.

*******************************************************************************/

void *memory_cnew(s4 size)
{
	void *p;

	LOCK_MONITOR_ENTER(lock_code_memory);

	size = MEMORY_ALIGN(size, ALIGNSIZE);

	/* check if enough memory is available */

	if (size > code_memory_size) {
		/* set default code size */

		code_memory_size = DEFAULT_CODE_MEMORY_SIZE;

		/* do we need more? */

		if (size > code_memory_size)
			code_memory_size = size;

		/* align the size of the memory to be allocated */

		code_memory_size = MEMORY_ALIGN(code_memory_size, pagesize);

#if defined(ENABLE_STATISTICS)
		if (opt_stat) {
			codememusage += code_memory_size;

			if (codememusage > maxcodememusage)
				maxcodememusage = codememusage;
		}
#endif

		/* allocate the memory */
#undef HAVE_MMAP

#if defined(HAVE_MMAP)
		p = memory_mmap_anon(NULL, code_memory_size,
							 PROT_READ | PROT_WRITE | PROT_EXEC,
							 MAP_PRIVATE);
#else
		p = malloc(code_memory_size);

# if defined(__HAVE_MPROTECT)
		if (mprotect(p, code_memory_size,
					 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
			vm_abort("memory_cnew: mprotect failed: %s", strerror(errno));
		}
# endif
#endif
		/* set global code memory pointer */

		code_memory = p;
	}

	/* get a memory chunk of the allocated memory */

	p = code_memory;

	code_memory       = (void *) ((ptrint) code_memory + size);
	code_memory_size -= size;

	LOCK_MONITOR_EXIT(lock_code_memory);

	return p;
}


/* memory_cfree ****************************************************************

   Frees the code memory pointed to.

   ATTENTION: This function currently does NOTHING!  Because we don't
   have a memory management for code memory.

*******************************************************************************/

void memory_cfree(void *p, s4 size)
{
	/* do nothing */
}


void *mem_alloc(s4 size)
{
	void *m;

	if (size == 0)
		return NULL;

#if defined(ENABLE_STATISTICS)
	if (opt_stat) {
		memoryusage += size;

		if (memoryusage > maxmemusage)
			maxmemusage = memoryusage;
	}
#endif

	m = memory_checked_alloc(size);

#if defined(ENABLE_MEMCHECK)
	/* XXX we would like to poison the memory, but callers rely on */
	/* the zeroing. This should change sooner or later.            */
	/* memset(m, MEMORY_CLEAR_BYTE, size); */
#endif

	return m;
}


void *mem_realloc(void *src, s4 len1, s4 len2)
{
	void *dst;

	/* prevent compiler warnings */

 	dst = NULL;

	if (src == NULL)
		if (len1 != 0)
			vm_abort("mem_realloc: reallocating memoryblock with address NULL, length != 0");

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		memoryusage = (memoryusage - len1) + len2;
#endif

#if defined(ENABLE_MEMCHECK)
	if (len2 < len1)
		memset((u1*)dst + len2, MEMORY_CLEAR_BYTE, len1 - len2);
#endif

	dst = realloc(src, len2);

	if (dst == NULL)
		vm_abort("mem_realloc: realloc failed: out of memory");

#if defined(ENABLE_MEMCHECK)
	if (len2 > len1)
		memset((u1*)dst + len1, MEMORY_CLEAR_BYTE, len2 - len1);
#endif

	return dst;
}


void mem_free(void *m, s4 size)
{
	if (!m) {
		if (size == 0)
			return;

		log_text("returned memoryblock with address NULL, length != 0");
		assert(0);
	}

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		memoryusage -= size;
#endif

#if defined(ENABLE_MEMCHECK)
	/* destroy the contents */
	memset(m, MEMORY_CLEAR_BYTE, size);
#endif

	free(m);
}


/* memory_thread ***************************************************************

   Prints regularly memory statistics.

*******************************************************************************/

#if defined(ENABLE_THREADS) && !defined(NDEBUG)
static void memory_thread(void)
{
	while (true) {
		/* sleep thread for 2 seconds */

		threads_sleep(2 * 1000, 0);

# if defined(ENABLE_STATISTICS)
		/* print memory usage */

		statistics_print_memory_usage();

		/* print GC memory usage */

		statistics_print_gc_memory_usage();
# endif
	}
}
#endif


/* memory_start_thread *********************************************************

   Starts the memory profiling thread.

*******************************************************************************/

#if defined(ENABLE_THREADS) && !defined(NDEBUG)
bool memory_start_thread(void)
{
	utf *name;

	name = utf_new_char("Memory Profiler");

	/* start the memory profiling thread */

	if (!threads_thread_start_internal(name, memory_thread))
		return false;

	/* everything's ok */

	return true;
}
#endif


/* dump_check_canaries *********************************************************

   Check canaries in dump memory.

   IN:
      di...........dumpinfo_t * of the dump area to check
	  bottomsize...dump size down to which the dump area should be checked
	               (specify 0 to check the whole dump area)

   ERROR HANDLING:
      If any canary has been changed, this function aborts the VM with
	  an error message.

*******************************************************************************/

#if defined(ENABLE_MEMCHECK)
void dump_check_canaries(dumpinfo_t *di, s4 bottomsize)
{
	dump_allocation_t *da;
	u1 *pm;
	s4 i, j;

	/* iterate over all dump memory allocations above bottomsize */

	da = di->allocations;
	while (da && da->useddumpsize >= bottomsize) {
		/* check canaries */

		pm = da->mem - MEMORY_CANARY_SIZE;
		for (i=0; i<MEMORY_CANARY_SIZE; ++i)
			if (pm[i] != i + MEMORY_CANARY_FIRST_BYTE) {
				fprintf(stderr, "canary bytes:");
				for (j=0; j<MEMORY_CANARY_SIZE; ++j)
					fprintf(stderr, " %02x", pm[j]);
				fprintf(stderr,"\n");
				vm_abort("error: dump memory bottom canary killed: "
						 "%p (%d bytes allocated at %p)\n",
						pm + i, da->size, da->mem);
			}

		pm = da->mem + da->size;
		for (i=0; i<MEMORY_CANARY_SIZE; ++i)
			if (pm[i] != i + MEMORY_CANARY_FIRST_BYTE) {
				fprintf(stderr, "canary bytes:");
				for (j=0; j<MEMORY_CANARY_SIZE; ++j)
					fprintf(stderr, " %02x", pm[j]);
				fprintf(stderr,"\n");
				vm_abort("error: dump memory top canary killed: "
						 "%p (%d bytes allocated at %p)\n",
						pm + i, da->size, da->mem);
			}

		da = da->next;
	}
}
#endif /* defined(ENABLE_MEMCHECK) */


/* dump_alloc ******************************************************************

   Allocate memory in the dump area.

   IN:
      size.........size of block to allocate, in bytes
	  			   may be zero, in which case NULL is returned

   RETURN VALUE:
      pointer to allocated memory, or
	  NULL iff `size` was zero

   ERROR HANDLING:
      XXX This function uses `memory_checked_alloc`, which *exits* if no 
	  memory could be allocated.

   THREADS:
      dump_alloc is thread safe. Each thread has its own dump memory area.

   dump_alloc is a fast allocator suitable for scratch memory that can be
   collectively freed when the current activity (eg. compiling) is done.

   You cannot selectively free dump memory. Before you start allocating it, 
   you remember the current size returned by `dump_size`. Later, when you no 
   longer need the memory, call `dump_release` with the remembered size and
   all dump memory allocated since the call to `dump_size` will be freed.

*******************************************************************************/

void *dump_alloc(s4 size)
{
#if defined(DISABLE_DUMP)

	/* use malloc memory for dump memory (for debugging only!) */

	return mem_alloc(size);

#else /* !defined(DISABLE_DUMP) */

	void       *m;
	dumpinfo_t *di;
#if defined(ENABLE_MEMCHECK)
	s4          origsize = size; /* needed for the canary system */
#endif

	/* If no threads are used, the dumpinfo structure is a static structure   */
	/* defined at the top of this file.                                       */

	di = DUMPINFO;

	if (size == 0)
		return NULL;

#if defined(ENABLE_MEMCHECK)
	size += 2*MEMORY_CANARY_SIZE;
#endif

	size = MEMORY_ALIGN(size, ALIGNSIZE);


	if (di->useddumpsize + size > di->allocateddumpsize) {
		dumpblock_t *newdumpblock;
		s4         newdumpblocksize;

		/* allocate a new dumplist structure */
		newdumpblock = memory_checked_alloc(sizeof(dumpblock_t));

		/* If requested size is greater than the default, make the new dump   */
		/* block as big as the size requested. Else use the default size.     */

		if (size > DUMPBLOCKSIZE) {
			newdumpblocksize = size;

		} else {
			newdumpblocksize = DUMPBLOCKSIZE;
		}

		/* allocate dumpblock memory */

		newdumpblock->dumpmem = memory_checked_alloc(newdumpblocksize);

		newdumpblock->prev = di->currentdumpblock;
		newdumpblock->size = newdumpblocksize;
		di->currentdumpblock = newdumpblock;

		/* Used dump size is previously allocated dump size, because the      */
		/* remaining free memory of the previous dump block cannot be used.   */

		di->useddumpsize = di->allocateddumpsize;

		/* increase the allocated dump size by the size of the new dump block */

		di->allocateddumpsize += newdumpblocksize;

#if defined(ENABLE_STATISTICS)
		/* the amount of globally allocated dump memory (thread save) */

		if (opt_stat)
			globalallocateddumpsize += newdumpblocksize;
#endif
	}

	/* current dump block base address + the size of the current dump block - */
	/* the size of the unused memory = new start address                      */

	m = di->currentdumpblock->dumpmem + di->currentdumpblock->size -
		(di->allocateddumpsize - di->useddumpsize);

#if defined(ENABLE_MEMCHECK)
	{
		dump_allocation_t *da = NEW(dump_allocation_t);
		s4 i;
		u1 *pm;

		/* add the allocation to our linked list of allocations */

		da->next = di->allocations;
		da->mem = (u1*) m + MEMORY_CANARY_SIZE;
		da->size = origsize;
		da->useddumpsize = di->useddumpsize;

		di->allocations = da;

		/* write the canaries */

		pm = (u1*)m;
		for (i=0; i<MEMORY_CANARY_SIZE; ++i)
			pm[i] = i + MEMORY_CANARY_FIRST_BYTE;
		pm = da->mem + da->size;
		for (i=0; i<MEMORY_CANARY_SIZE; ++i)
			pm[i] = i + MEMORY_CANARY_FIRST_BYTE;

		/* make m point after the bottom canary */

		m = (u1*)m + MEMORY_CANARY_SIZE;

		/* clear the memory */

		memset(m, MEMORY_CLEAR_BYTE, da->size);
	}
#endif /* defined(ENABLE_MEMCHECK) */

	/* increase used dump size by the allocated memory size */

	di->useddumpsize += size;

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		if (di->useddumpsize > maxdumpsize)
			maxdumpsize = di->useddumpsize;
#endif

	return m;

#endif /* defined(DISABLE_DUMP) */
}


/* dump_realloc ****************************************************************

   Stupid realloc implementation for dump memory. Avoid, if possible.

*******************************************************************************/

void *dump_realloc(void *src, s4 len1, s4 len2)
{
#if defined(DISABLE_DUMP)
	/* use malloc memory for dump memory (for debugging only!) */

	return mem_realloc(src, len1, len2);
#else
	void *dst = dump_alloc(len2);

	memcpy(dst, src, len1);

#if defined(ENABLE_MEMCHECK)
	/* destroy the source */
	memset(src, MEMORY_CLEAR_BYTE, len1);
#endif

	return dst;
#endif
}


/* dump_release ****************************************************************

   Release dump memory above the given size.

   IN:
       size........All dump memory above this mark will be freed. Usually
	               `size` will be the return value of a `dump_size` call
				   made earlier.

	ERROR HANDLING:
	   XXX If the given size is invalid, this function *exits* with an
	       error message.
				   
	See `dump_alloc`.

*******************************************************************************/

void dump_release(s4 size)
{
#if defined(DISABLE_DUMP)

	/* use malloc memory for dump memory (for debugging only!) */

	/* do nothing */

#else /* !defined(DISABLE_DUMP) */

	dumpinfo_t *di;

	/* If no threads are used, the dumpinfo structure is a static structure   */
	/* defined at the top of this file.                                       */

	di = DUMPINFO;

	if ((size < 0) || (size > di->useddumpsize))
		vm_abort("Illegal dump release size: %d", size);

#if defined(ENABLE_MEMCHECK)
	{
		dump_allocation_t *da, *next;

		/* check canaries */

		dump_check_canaries(di, size);

		/* iterate over all dump memory allocations about to be released */

		da = di->allocations;
		while (da && da->useddumpsize >= size) {
			next = da->next;

			/* invalidate the freed memory */

			memset(da->mem, MEMORY_CLEAR_BYTE, da->size);

			FREE(da, dump_allocation_t);

			da = next;
		}
		di->allocations = da;
	}
#endif /* defined(ENABLE_MEMCHECK) */

	/* reset the used dump size to the size specified */

	di->useddumpsize = size;

	while (di->currentdumpblock && di->allocateddumpsize - di->currentdumpblock->size >= di->useddumpsize) {
		dumpblock_t *tmp = di->currentdumpblock;

		di->allocateddumpsize -= tmp->size;
		di->currentdumpblock = tmp->prev;

#if defined(ENABLE_STATISTICS)
		/* the amount of globally allocated dump memory (thread save) */

		if (opt_stat)
			globalallocateddumpsize -= tmp->size;
#endif

		/* release the dump memory and the dumpinfo structure */

		free(tmp->dumpmem);
		free(tmp);
	}

#endif /* defined(DISABLE_DUMP) */
}


/* dump_size *******************************************************************

   Return the current size of the dump memory area. See `dump_alloc`.

*******************************************************************************/

s4 dump_size(void)
{
#if defined(DISABLE_DUMP)
	/* use malloc memory for dump memory (for debugging only!) */

	return 0;

#else /* !defined(DISABLE_DUMP) */

	dumpinfo_t *di;

	/* If no threads are used, the dumpinfo structure is a static structure   */
	/* defined at the top of this file.                                       */

	di = DUMPINFO;

	if (di == NULL)
		return 0;

	return di->useddumpsize;

#endif /* defined(DISABLE_DUMP) */
}


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

/* src/mm/memory.h - macros for memory management

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

   $Id: memory.h 7812 2007-04-25 18:51:03Z twisti $

*/


#ifndef _MEMORY_H
#define _MEMORY_H

/* forward typedefs ***********************************************************/

typedef struct dumpblock_t dumpblock_t;
typedef struct dumpinfo_t  dumpinfo_t;

#include "config.h"

#include <string.h>

#include "vm/types.h"


/* ATTENTION: We need to define dumpblock_t and dumpinfo_t before
   internal includes, as we need dumpinfo_t as nested structure in
   threadobject. */

/* dumpblock ******************************************************************/

#define DUMPBLOCKSIZE    2 << 13    /* 2 * 8192 bytes */
#define ALIGNSIZE        8

struct dumpblock_t {
	dumpblock_t *prev;
	u1          *dumpmem;
	s4           size;
};


/* dump_allocation *************************************************************

   This struct is used to record dump memory allocations for ENABLE_MEMCHECK.

*******************************************************************************/

#if defined(ENABLE_MEMCHECK)
typedef struct dump_allocation_t dump_allocation_t;

struct dump_allocation_t {
	dump_allocation_t *next;
	u1                *mem;
	s4                 useddumpsize;
	s4                 size;
};
#endif


/* dumpinfo *******************************************************************/

struct dumpinfo_t {
	dumpblock_t       *currentdumpblock;        /* the current top-most block */
	s4                 allocateddumpsize;     /* allocated bytes in this area */
	s4                 useddumpsize;          /* used bytes in this dump area */
#if defined(ENABLE_MEMCHECK)
	dump_allocation_t *allocations;       /* list of allocations in this area */
#endif
};


/* internal includes **********************************************************/

#include "mm/gc-common.h"


/* 
---------------------------- Interface description -----------------------

There are two possible choices for allocating memory:

	1.   explicit allocating / deallocating

			mem_alloc ..... allocate a memory block 
			mem_free ...... free a memory block
			mem_realloc ... change size of a memory block (position may change)
			mem_usage ..... amount of allocated memory


	2.   explicit allocating, automatic deallocating
	
			dump_alloc .... allocate a memory block in the dump area
			dump_realloc .. change size of a memory block (position may change)
			dump_size ..... marks the current top of dump
			dump_release .. free all memory requested after the mark
			                
	
There are some useful macros:

	NEW (type) ....... allocate memory for an element of type `type`
	FREE (ptr,type) .. free memory
	
	MNEW (type,num) .. allocate memory for an array
	MFREE (ptr,type,num) .. free memory
	
	MREALLOC (ptr,type,num1,num2) .. enlarge the array to size num2
	                                 
These macros do the same except they operate on the dump area:
	
	DNEW,  DMNEW, DMREALLOC   (there is no DFREE)


-------------------------------------------------------------------------------

Some more macros:

	MEMORY_ALIGN (pos, size) ... make pos divisible by size. always returns an
                                 address >= pos.
	                      
	
	OFFSET (s,el) ....... returns the offset of 'el' in structure 's' in bytes.
	                      
	MCOPY (dest,src,type,num) ... copy 'num' elements of type 'type'.
	

*/

#define MEMORY_ALIGN(pos,size) ((((pos) + (size) - 1) / (size)) * (size))
#define PADDING(pos,size)     (MEMORY_ALIGN((pos),(size)) - (pos))
#define OFFSET(s,el)          ((s4) ((ptrint) &(((s*) 0)->el)))


#define NEW(type)             ((type *) mem_alloc(sizeof(type)))
#define FREE(ptr,type)        mem_free((ptr), sizeof(type))

#define MNEW(type,num)        ((type *) mem_alloc(sizeof(type) * (num)))
#define MFREE(ptr,type,num)   mem_free((ptr), sizeof(type) * (num))

#define MREALLOC(ptr,type,num1,num2) mem_realloc((ptr), sizeof(type) * (num1), \
                                                        sizeof(type) * (num2))


#define DNEW(type)            ((type *) dump_alloc(sizeof(type)))
#define DMNEW(type,num)       ((type *) dump_alloc(sizeof(type) * (num)))
#define DMREALLOC(ptr,type,num1,num2) dump_realloc((ptr), sizeof(type) * (num1), \
                                                          sizeof(type) * (num2))

#define MCOPY(dest,src,type,num) memcpy((dest), (src), sizeof(type) * (num))
#define MSET(ptr,byte,type,num) memset((ptr), (byte), sizeof(type) * (num))
#define MZERO(ptr,type,num)     MSET(ptr,0,type,num)
#define MMOVE(dest,src,type,num) memmove((dest), (src), sizeof(type) * (num))

#define CNEW(type,num)        ((type *) memory_cnew(sizeof(type) * (num)))
#define CFREE(ptr,num)        memory_cfree((ptr),(num))


/* GC macros ******************************************************************/

/* Uncollectable memory which can contain references */

#define GCNEW_UNCOLLECTABLE(type,num) ((type *) heap_alloc_uncollectable(sizeof(type) * (num)))

#define GCNEW(type)           heap_allocate(sizeof(type), true, NULL)
#define GCMNEW(type,num)      heap_allocate(sizeof(type) * (num), true, NULL)

#define GCFREE(ptr)           heap_free((ptr))


/* function prototypes ********************************************************/

/* initializes the memory subsystem */
bool memory_init(void);

void *memory_mmap_anon(void *addr, size_t len, int prot, int flags);

void *memory_cnew(s4 size);
void  memory_cfree(void *p, s4 size);

void *mem_alloc(s4 size);
void  mem_free(void *m, s4 size);
void *mem_realloc(void *src, s4 len1, s4 len2);

#if defined(ENABLE_THREADS)
bool  memory_start_thread(void);
#endif

void *dump_alloc(s4 size);
void *dump_realloc(void *src, s4 len1, s4 len2);
s4    dump_size(void);
void  dump_release(s4 size);

#endif /* _MEMORY_H */


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

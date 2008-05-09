/* src/toolbox/hashtable.c - functions for internal hashtables

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

   $Id: hashtable.c 7813 2007-04-25 19:20:13Z twisti $

*/


#include "config.h"
#include "vm/types.h"

#include "mm/memory.h"

#include "threads/lock-common.h"

#include "toolbox/hashtable.h"

#include "vm/global.h"


/* hashtable_create ************************************************************

   Initializes a hashtable structure and allocates memory. The
   parameter size specifies the initial size of the hashtable.
	
*******************************************************************************/

void hashtable_create(hashtable *hash, u4 size)
{
	/* initialize locking pointer */

#if defined(ENABLE_THREADS)
	/* We need to seperately allocate a java_objectheader here, as we
	   need to store the lock object in the new hashtable if it's
	   resized.  Otherwise we get an IllegalMonitorStateException. */

	hash->header = NEW(java_objectheader);

	LOCK_INIT_OBJECT_LOCK(hash->header);
#endif

	/* set initial hash values */

	hash->size    = size;
	hash->entries = 0;
	hash->ptr     = MNEW(void*, size);

	/* MNEW always allocates memory zeroed out, no need to clear the table */
}


/* hashtable_resize ************************************************************

   Creates a new hashtable with specified size and moves the important
   stuff from the old hashtable.

*******************************************************************************/

hashtable *hashtable_resize(hashtable *hash, u4 size)
{
	hashtable *newhash;

	/* create new hashtable with specified size */

	newhash = NEW(hashtable);

	hashtable_create(newhash, size);

#if defined(ENABLE_THREADS)
	/* We need to store the old lock object in the new hashtable.
	   Otherwise we get an IllegalMonitorStateException. */

	FREE(newhash->header, java_objectheader);

	newhash->header  = hash->header;
#endif

	/* store the number of entries in the new hashtable */

	newhash->entries = hash->entries;

	return newhash;
}


/* hashtable_free **************************************************************

   Simply frees the hashtable.

   ATTENTION: It does NOT free the lock object!

*******************************************************************************/

void hashtable_free(hashtable *hash)
{
	MFREE(hash->ptr, void*, hash->size);
	FREE(hash, hashtable);
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
 */

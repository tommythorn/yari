/* src/vmcore/zip.c - ZIP file handling for bootstrap classloader

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

   $Id: zip.h 7246 2007-01-29 18:49:05Z twisti $

*/


#ifndef _ZIP_H
#define _ZIP_H

#include "config.h"
#include "vm/types.h"

#include "toolbox/hashtable.h"

#include "vm/global.h"

#include "vmcore/class.h"
#include "vmcore/loader.h"
#include "vmcore/suck.h"
#include "vmcore/utf8.h"


/* hashtable_zipfile_entry ****************************************************/

typedef struct hashtable_zipfile_entry hashtable_zipfile_entry;

struct hashtable_zipfile_entry {
	utf                     *filename;
	u2                       compressionmethod;
	u4                       compressedsize;
	u4                       uncompressedsize;
	u1                      *data;
	hashtable_zipfile_entry *hashlink;
};


/* function prototypes ********************************************************/

hashtable *zip_open(char *path);
hashtable_zipfile_entry *zip_find(list_classpath_entry *lce, utf *u);
classbuffer *zip_get(list_classpath_entry *lce, classinfo *c);

#endif /* _ZIP_H */

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

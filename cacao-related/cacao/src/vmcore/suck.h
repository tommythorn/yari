/* src/vmcore/suck.h - functions to read LE ordered types from a buffer

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

   $Id: suck.h 7783 2007-04-20 13:28:27Z twisti $

*/


#ifndef _SUCK_H
#define _SUCK_H

#include "config.h"
#include "vm/types.h"

#include "toolbox/hashtable.h"
#include "toolbox/list.h"

#include "vm/global.h"

#include "vmcore/class.h"
#include "vmcore/loader.h"


/* list_classpath_entry *******************************************************/

enum {
	CLASSPATH_PATH,
	CLASSPATH_ARCHIVE
};

typedef struct list_classpath_entry list_classpath_entry;

struct list_classpath_entry {
#if defined(ENABLE_THREADS)
	java_objectheader  header;	        /* monitor locking on zip/jar files   */
#endif
	s4                 type;
	char              *path;
	s4                 pathlen;
#if defined(ENABLE_ZLIB)
	hashtable         *htclasses;
#endif
	listnode_t         linkage;
};


/* macros to read LE and BE types from a buffer ********************************

   BE macros are for Java class file loading.
   LE macros are for ZIP file loading.

*******************************************************************************/

/* LE macros (for ZIP files ) *************************************************/

#if defined(__I386__) || defined(__X86_64__)

/* we can optimize the LE access on little endian machines without alignment */

#define SUCK_LE_U1(p)    *((u1 *) (p))
#define SUCK_LE_U2(p)    *((u2 *) (p))
#define SUCK_LE_U4(p)    *((u4 *) (p))

#if U8_AVAILABLE == 1
#define SUCK_LE_U8(p)    *((u8 *) (p))
#endif

#else /* defined(__I386__) || defined(__X86_64__) */

#define SUCK_LE_U1(p) \
      ((u1) (p)[0])

#define SUCK_LE_U2(p) \
    ((((u2) (p)[1]) << 8) + \
      ((u2) (p)[0]))

#define SUCK_LE_U4(p) \
    ((((u4) (p)[3]) << 24) + \
     (((u4) (p)[2]) << 16) + \
     (((u4) (p)[1]) << 8) + \
      ((u4) (p)[0]))

#if U8_AVAILABLE == 1
#define SUCK_LE_U8(p) \
    ((((u8) (p)[7]) << 56) + \
     (((u8) (p)[6]) << 48) + \
     (((u8) (p)[5]) << 40) + \
     (((u8) (p)[4]) << 32) + \
     (((u8) (p)[3]) << 24) + \
     (((u8) (p)[2]) << 16) + \
     (((u8) (p)[1]) << 8) + \
      ((u8) (p)[0]))
#endif

#endif /* defined(__I386__) || defined(__X86_64__) */


/* BE macros (for Java class files ) ******************************************/

#define SUCK_BE_U1(p) \
      ((u1) (p)[0])

#define SUCK_BE_U2(p) \
    ((((u2) (p)[0]) << 8) + \
      ((u2) (p)[1]))

#define SUCK_BE_U4(p) \
    ((((u4) (p)[0]) << 24) + \
     (((u4) (p)[1]) << 16) + \
     (((u4) (p)[2]) << 8) + \
      ((u4) (p)[3]))

#if U8_AVAILABLE == 1
#define SUCK_BE_U8(p) \
    ((((u8) (p)[0]) << 56) + \
     (((u8) (p)[1]) << 48) + \
     (((u8) (p)[2]) << 40) + \
     (((u8) (p)[3]) << 32) + \
     (((u8) (p)[4]) << 24) + \
     (((u8) (p)[5]) << 16) + \
     (((u8) (p)[6]) << 8) + \
      ((u8) (p)[7]))
#endif


#define SUCK_BE_S1(p)    (s1) SUCK_BE_U1(p)
#define SUCK_BE_S2(p)    (s2) SUCK_BE_U2(p)
#define SUCK_BE_S4(p)    (s4) SUCK_BE_U4(p)
#define SUCK_BE_S8(p)    (s8) SUCK_BE_U8(p)


/* signed suck defines ********************************************************/

#define suck_s1(a)    (s1) suck_u1((a))
#define suck_s2(a)    (s2) suck_u2((a))
#define suck_s4(a)    (s4) suck_u4((a))
#define suck_s8(a)    (s8) suck_u8((a))


/* export variables ***********************************************************/

extern list_t *list_classpath_entries;


/* function prototypes ********************************************************/

bool suck_init(void);

void suck_add(char *classpath);
void suck_add_from_property(char *key);

bool suck_check_classbuffer_size(classbuffer *cb, s4 len);

u1 suck_u1(classbuffer *cb);
u2 suck_u2(classbuffer *cb);
u4 suck_u4(classbuffer *cb);
u8 suck_u8(classbuffer *cb);

float suck_float(classbuffer *cb);
double suck_double(classbuffer *cb);

void suck_nbytes(u1 *buffer, classbuffer *cb, s4 len);
void suck_skip_nbytes(classbuffer *cb, s4 len);

classbuffer *suck_start(classinfo *c);

void suck_stop(classbuffer *cb);

#endif /* _SUCK_H */

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

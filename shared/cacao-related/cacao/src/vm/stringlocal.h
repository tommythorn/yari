/* src/vm/stringlocal.h - string header

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

   $Id: stringlocal.h 7246 2007-01-29 18:49:05Z twisti $

*/


#ifndef _STRINGLOCAL_H
#define _STRINGLOCAL_H

typedef struct literalstring literalstring;


#include "config.h"
#include "vm/types.h"

#include "toolbox/hashtable.h"

#include "vm/global.h"

#include "vmcore/utf8.h"


/* data structure of internal javastrings stored in global hashtable **********/

struct literalstring {
	literalstring     *hashlink;        /* link for external hash chain       */
	java_objectheader *string;  
};


/* javastring-hashtable */
extern hashtable hashtable_string;


/* function prototypes ********************************************************/

/* initialize string subsystem */
bool string_init(void);

void stringtable_update(void);

/* creates a new object of type java/lang/String from a utf-text */
java_objectheader *javastring_new(utf *text);

/* creates a new object of type java/lang/String from a utf-text, changes slashes to dots */
java_objectheader *javastring_new_slash_to_dot(utf *text);

/* creates a new object of type java/lang/String from an ASCII c-string */
java_objectheader *javastring_new_from_ascii(const char *text);

/* creates a new object of type java/lang/String from UTF-8 */
java_objectheader *javastring_new_from_utf_buffer(const char *buffer, u4 blength);
java_objectheader *javastring_new_from_utf_string(const char *utfstr);

/* creates a new object of type java/lang/String from (possibly invalid) UTF-8 */
java_objectheader *javastring_safe_new_from_utf8(const char *text);

/* make c-string from a javastring (debugging) */
char *javastring_tochar(java_objectheader *string);

/* make utf symbol from javastring */
utf *javastring_toutf(java_objectheader *string, bool isclassname);

/* creates a new javastring with the text of the u2-array */
java_objectheader *literalstring_u2(java_chararray *a, u4 length, u4 offset,
									bool copymode);

/* creates a new javastring with the text of the utf-symbol */
java_objectheader *literalstring_new(utf *u);

/* dispose a javastring */
void literalstring_free(java_objectheader*);

#endif /* _STRINGLOCAL_H */


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

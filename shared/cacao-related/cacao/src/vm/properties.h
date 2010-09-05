/* src/vm/properties.h - handling commandline properties

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

   $Id: properties.h 7246 2007-01-29 18:49:05Z twisti $

*/


#ifndef _PROPERTIES_H
#define _PROPERTIES_H

#include "config.h"
#include "vm/types.h"

#include "vm/global.h"


/* function prototypes ********************************************************/

bool  properties_init(void);
bool  properties_postinit(void);

void  properties_add(char *key, char *value);
char *properties_get(char *key);

void  properties_system_add(java_objectheader *p, char *key, char *value);

#if defined(ENABLE_JAVASE)
void  properties_system_add_all(java_objectheader *p);
#endif

#endif /* _PROPERTIES_H */


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

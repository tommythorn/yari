/* src/toolbox/bitvector.h - bitvector header

   Copyright (C) 2005, 2006 R. Grafl, A. Krall, C. Kruegel, C. Oates,
   R. Obermaisser, M. Platter, M. Probst, S. Ring, E. Steiner,
   C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich, J. Wenninger,
   Institut f. Computersprachen - TU Wien

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.

   Contact: cacao@complang.tuwien.ac.at

   Authors: Christian Ullrich

   $Id: bitvector.h$

*/


#ifndef _BITVECTOR_H
#define _BITVECTOR_H

#if !defined(NDEBUG)
#include <assert.h>

/* define BV_DEBUG_CHECK to activate the bound checks */

/* #define BV_DEBUG_CHECK */

/* no debug messages implemented till now */

/* #define BV_DEBUG_VERBOSE */
#endif

#if defined(BV_DEBUG_CHECK)
#define _BV_CHECK_BOUNDS(i,l,h) assert( ((i) >= (l)) && ((i) < (h)));
#define _BV_ASSERT(a) assert((a));
#else
#define _BV_CHECK_BOUNDS(i,l,h);
#define _BV_ASSERT(a);
#endif
typedef int *bitvector;

/* function prototypes */
char *bv_to_string(bitvector bv, char *string, int size);
bitvector bv_new(int size);           /* Create a new Bitvector for size Bits */
                                      /* All bits are reset                   */
void bv_set_bit(bitvector bv, int bit);    /* set Bit bit of bitvector       */
void bv_reset_bit(bitvector bv, int bit);  /* reset Bit bit of bitvector     */
void bv_reset(bitvector bv, int size);     /* reset the whole bitvector      */
bool bv_is_empty(bitvector bv, int size);  /* Returns if no Bit is set       */
bool bv_get_bit(bitvector bv, int bit);    /* Returns if Bit bit is set      */
bool bv_equal(bitvector s1, bitvector s2, int size);

/* copy the whole bitvector    */

void bv_copy(bitvector dst, bitvector src, int size); 

/* d = s1 \ s2     */

void bv_minus(bitvector d, bitvector s1, bitvector s2, int size);

/* d = s1 union s2 */

void bv_union(bitvector d, bitvector s1, bitvector s2, int size);

#endif /* _BITVECTOR_H */


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

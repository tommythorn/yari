/* src/threads/native/generic-primitives.h - machine independent atomic
                                             operations

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

   Authors: Christian Thalinger
            Anton Ertl

   Changes: 

   $Id: generic-primitives.h 4357 2006-01-22 23:33:38Z twisti $

*/


#ifndef _MACHINE_INSTR_H
#define _MACHINE_INSTR_H

#include <pthread.h>

extern pthread_mutex_t _atomic_add_lock;
extern pthread_mutex_t _cas_lock;
extern pthread_mutex_t _mb_lock;


static inline void atomic_add(volatile int *mem, int val)
{
  pthread_mutex_lock(&_atomic_add_lock);

  /* do the atomic add */
  *mem += val;

  pthread_mutex_unlock(&_atomic_add_lock);
}


static inline long compare_and_swap(volatile long *p, long oldval, long newval)
{
  long ret;

  pthread_mutex_lock(&_cas_lock);

  /* do the compare-and-swap */

  ret = *p;

  if (oldval == ret)
    *p = newval;

  pthread_mutex_unlock(&_cas_lock);

  return ret;
}


#define MEMORY_BARRIER()                  (pthread_mutex_lock(&_mb_lock), \
                                           pthread_mutex_unlock(&_mb_lock))
#define STORE_ORDER_BARRIER()             MEMORY_BARRIER()
#define MEMORY_BARRIER_BEFORE_ATOMIC()    /* nothing */
#define MEMORY_BARRIER_AFTER_ATOMIC()     /* nothing */

#endif /* _MACHINE_INSTR_H */


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

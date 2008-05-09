/* src/vm/jit/m68k/machine-instr.h  

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

   $Id: arch.h 5330 2006-09-05 18:43:12Z edwin $

*/


#ifndef _MACHINE_INSTR_H
#define _MACHINE_INSTR_H

static inline long compare_and_swap(long *p, long oldval, long newval)
{
	/* XXX, coldifre has no atomic compare and swap instrcution */
	#warning "compare_and_swap is not atmically"
	if (*p == oldval)	{
		*p = newval;
		return oldval;
	} 
	return *p;
}


#define STORE_ORDER_BARRIER() __asm__ __volatile__ ("" : : : "memory");
#define MEMORY_BARRIER_BEFORE_ATOMIC() __asm__ __volatile__ ("" : : : "memory");
#define MEMORY_BARRIER_AFTER_ATOMIC() __asm__ __volatile__ ("" : : : "memory");
#define MEMORY_BARRIER() __asm__ __volatile__ ( "" : : : "memory" );

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

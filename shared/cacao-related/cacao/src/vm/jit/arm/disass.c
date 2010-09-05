/* src/vm/jit/arm/disass.c - wrapper functions for GNU binutils disassembler

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

   $Id: disass.c 7333 2007-02-11 22:17:27Z twisti $

*/


#include "config.h"

#include <dis-asm.h>
#include <stdio.h>

#include "vm/types.h"

#include "vm/global.h"

#include "vm/jit/disass.h"


/* disassinstr *****************************************************************

   Outputs a disassembler listing of one machine code instruction on
   'stdout'.

   code: pointer to instructions machine code

*******************************************************************************/

u1 *disassinstr(u1 *code)
{
	if (!disass_initialized) {
		INIT_DISASSEMBLE_INFO(info, stdout, disass_printf);
		info.read_memory_func = &disass_buffer_read_memory;
		disass_initialized = true;
	}

	printf("0x%08x:   %08x    ", (u4) code, *((s4 *) code));

#if defined(__ARMEL__)
	print_insn_little_arm((bfd_vma) code, &info);
#else
	print_insn_big_arm((bfd_vma) code, &info);
#endif

	printf("\n");

	/* 1 instruction is 4-bytes long */
	return code + 4;
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

/* src/vm/jit/disass.h - disassembler header

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

   $Id: disass.h 7596 2007-03-28 21:05:53Z twisti $

*/


#ifndef _DISASS_H
#define _DISASS_H

#include "config.h"

#if defined(WITH_BINUTILS_DISASSEMBLER)
# include <dis-asm.h>
#endif

#include "vm/types.h"

#if defined(ENABLE_DISASSEMBLER)

/* some macros ****************************************************************/

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)

#define DISASSINSTR(code) \
    do { \
        if (opt_intrp) \
            (code) = intrp_disassinstr((code)); \
        else \
            (code) = disassinstr((code)); \
    } while (0)

#define DISASSEMBLE(start,end) \
    do { \
        if (opt_intrp) \
            intrp_disassemble((start), (end)); \
        else \
            disassemble((start), (end)); \
    } while (0)

# else /* defined(ENABLE_INTRP) */

#define DISASSINSTR(code) \
    (code) = disassinstr((code))

#define DISASSEMBLE(start,end) \
    disassemble((start), (end))

# endif /* defined(ENABLE_INTRP) */
#else /* defined(ENABLE_JIT) */

#define DISASSINSTR(code) \
    (code) = intrp_disassinstr((code))

#define DISASSEMBLE(start,end) \
    intrp_disassemble((start), (end))

#endif /* defined(ENABLE_JIT) */


/* export global variables ****************************************************/

#if defined(WITH_BINUTILS_DISASSEMBLER)
extern disassemble_info info;
extern bool disass_initialized;
#endif

#if defined(__I386__) || defined(__X86_64__) || defined(__S390__) || defined(__M68K__)
extern char disass_buf[512];
extern s4   disass_len;
#endif


/* function prototypes *******************************************************/

#if defined(ENABLE_JIT)
void disassemble(u1 *start, u1 *end);
#endif

#if defined(WITH_BINUTILS_DISASSEMBLER)
void disass_printf(PTR p, const char *fmt, ...);

int disass_buffer_read_memory(bfd_vma memaddr, bfd_byte *myaddr, unsigned int length, struct disassemble_info *info);
#endif

/* machine dependent functions */

#if defined(ENABLE_JIT)
u1 *disassinstr(u1 *code);
#endif

#if defined(ENABLE_INTRP)
u1 *intrp_disassinstr(u1 *code);
void intrp_disassemble(u1 *start, u1 *end);
#endif

#endif /* defined(ENABLE_DISASSEMBLER) */

#endif /* _DISASS_H */


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

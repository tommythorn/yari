/* src/vm/jit/i386/arch.h - architecture defines for i386

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

   $Id: arch.h 7695 2007-04-12 19:49:34Z twisti $

*/


#ifndef _ARCH_H
#define _ARCH_H

#include "config.h"


/* define x86 register numbers ************************************************/

#define EAX    0
#define ECX    1
#define EDX    2
#define EBX    3
#define ESP    4
#define EBP    5
#define ESI    6
#define EDI    7

/* define architecture features ***********************************************/

#define U8_AVAILABLE                     1

#define SUPPORT_DIVISION                 1
#define SUPPORT_LONG                     1

#define SUPPORT_I2F                      1
#define SUPPORT_I2D                      1
#define SUPPORT_L2F                      1
#define SUPPORT_L2D                      1

/* ATTENTION: i386 architectures support these conversions, but we
   need the builtin functions in corner cases */
#define SUPPORT_F2I                      0
#define SUPPORT_F2L                      0
#define SUPPORT_D2I                      0
#define SUPPORT_D2L                      0

#define SUPPORT_LONG_ADD                 1
#define SUPPORT_LONG_CMP                 1
#define SUPPORT_LONG_CMP_CONST           1
#define SUPPORT_LONG_LOGICAL             1
#define SUPPORT_LONG_SHIFT               1
#define SUPPORT_LONG_MUL                 1
#define SUPPORT_LONG_DIV                 0

#define SUPPORT_LONG_DIV_POW2            1
#define SUPPORT_LONG_REM_POW2            0

#define SUPPORT_CONST_LOGICAL            1  /* AND, OR, XOR with immediates   */
#define SUPPORT_CONST_MUL                1  /* mutiply with immediate         */

#define SUPPORT_CONST_STORE              1  /* do we support const stores     */
#define SUPPORT_CONST_STORE_ZERO_ONLY    0  /* on some risc machines we can   */
                                            /* only store REG_ZERO            */


/* float **********************************************************************/

#define SUPPORT_FLOAT                    1

#if defined(ENABLE_SOFT_FLOAT_CMP)
# define SUPPORT_FLOAT_CMP               0
#else
# define SUPPORT_FLOAT_CMP               1
#endif


/* double *********************************************************************/

#define SUPPORT_DOUBLE                   1

#if defined(ENABLE_SOFT_FLOAT_CMP)
# define SUPPORT_DOUBLE_CMP              0
#else
# define SUPPORT_DOUBLE_CMP              1
#endif


#define HAS_4BYTE_STACKSLOT
#define ALIGN_LONGS_IN_MEMORY       /* Align Longs and/or Doubles at         */
#define ALIGN_DOUBLES_IN_MEMORY     /* 2*Stackslotsize relativ to stackframe */
/* Memory Positions for not Interface Stackslots (allocate_scratch_registers)*/
/* are not properly aligned in case HAS_4_BYTE_STACKSLOT is not defined!     */
/* For HAS_4_BYTE_STACKSLOT archs no distinction is made between long and dbl*/
/* define SUPPORT_COMBINE_INTEGER_REGISTERS */


/* branches *******************************************************************/

#define SUPPORT_BRANCH_CONDITIONAL_CONDITION_REGISTER       1
#define SUPPORT_BRANCH_CONDITIONAL_ONE_INTEGER_REGISTER     0
#define SUPPORT_BRANCH_CONDITIONAL_TWO_INTEGER_REGISTERS    0
#define SUPPORT_BRANCH_CONDITIONAL_UNSIGNED_CONDITIONS      1


/* exceptions *****************************************************************/

#define SUPPORT_HARDWARE_DIVIDE_BY_ZERO  1


/* replacement ****************************************************************/

#define REPLACEMENT_PATCH_SIZE           5       /* bytes */
#define REPLACEMENT_STUB_SIZE            (5+5+1) /* bytes */

#endif /* _ARCH_H */


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

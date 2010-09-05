/* src/vm/jit/i386/md-abi.c - functions for i386 Linux ABI

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

   $Id: md-abi.c 7713 2007-04-15 21:49:48Z twisti $

*/


#include "config.h"
#include "vm/types.h"

#include "vm/jit/i386/md-abi.h"

#include "vm/global.h"

#include "vm/jit/abi.h"

#include "vmcore/descriptor.h"


/* register descripton - array ************************************************/

s4 nregdescint[] = {
    REG_RET, REG_RES, REG_RES, REG_TMP, REG_RES, REG_SAV, REG_SAV, REG_SAV,
    REG_END
};

const char *abi_registers_integer_name[] = {
	"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
};

const s4 abi_registers_integer_argument[] = {
	-1,
};

const s4 abi_registers_integer_saved[] = {
	5, /* s0 */
	6, /* s1 */
	7, /* s2 */
};

const s4 abi_registers_integer_temporary[] = {
	3, /* t0 */
};


s4 nregdescfloat[] = {
 /* rounding problems with callee saved registers */
 /* REG_SAV, REG_SAV, REG_SAV, REG_SAV, REG_TMP, REG_TMP, REG_RES, REG_RES, */
 /* REG_TMP, REG_TMP, REG_TMP, REG_TMP, REG_TMP, REG_TMP, REG_RES, REG_RES, */
    REG_RES, REG_RES, REG_RES, REG_RES, REG_RES, REG_RES, REG_RES, REG_RES,
    REG_END
};

const s4 abi_registers_float_argument[] = {
	-1,
};

const s4 abi_registers_float_saved[] = {
	-1,
};

const s4 abi_registers_float_temporary[] = {
	-1,
};


/* md_param_alloc **************************************************************

   Allocate Arguments to Stackslots according the Calling Conventions

   --- in
   md->paramcount:           Number of arguments for this method
   md->paramtypes[].type:    Argument types
   
   --- out
   md->params[].inmemory:    Argument spilled on stack
   md->params[].regoff:      Stack offset or rd->arg[int|flt]regs index
   md->memuse:               Stackslots needed for argument spilling
   md->argintreguse:         max number of integer arguments used
   md->argfltreguse:         max number of float arguments used

*******************************************************************************/

void md_param_alloc(methoddesc *md)
{
	paramdesc *pd;
	s4        stacksize;
	s4        i;

	pd = md->params;
	stacksize = 0;

	for (i = 0; i < md->paramcount; i++, pd++) {
		pd->inmemory = true;
		pd->regoff = stacksize;
		stacksize += IS_2_WORD_TYPE(md->paramtypes[i].type) ? 2 : 1;
	}

	md->memuse = stacksize;
	md->argintreguse = 0;
	md->argfltreguse = 0;
}


/* md_param_alloc_native *******************************************************

   Pre-allocate arguments according the native ABI.

*******************************************************************************/

void md_param_alloc_native(methoddesc *md)
{
	/* On i386 we use the same ABI for JIT method calls as for native
	   method calls. */

	md_param_alloc(md);
}


/* md_return_alloc *************************************************************

   No straight forward precoloring of the Java Stackelement containing
   the return value possible for i386, since it uses "reserved"
   registers for return values

*******************************************************************************/

void md_return_alloc(jitdata *jd, stackptr stackslot)
{
	/* nothing */
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

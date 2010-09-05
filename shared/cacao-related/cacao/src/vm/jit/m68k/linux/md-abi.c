/* src/vm/jit/m68k/linux/md-abi.c - linux specific abi functions

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



#include "config.h"

#include <assert.h>

#include "md-abi.h"

#include "vm/types.h"

#include "vm/jit/abi.h"

/* Integer register description */
/*                        D0,      D1,   ITMP1,   ITMP2,   ITMP3,      D5,      D6,      D7 */
s4 nregdescint[] = { REG_TMP, REG_TMP, REG_RES, REG_RES, REG_RES, REG_SAV, REG_SAV, REG_SAV, REG_END };

const char *abi_registers_integer_name [] = { "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7" };

const s4 abi_registers_integer_argument[] 	;
const s4 abi_registers_integer_saved[] 		= {5,6,7};
const s4 abi_registers_integer_temporary[] 	= {0,1};

/* Address register description */
/* A6 is used as framepointer by native code, so be sure it is REG_RES */
/*                        A0,      A1,   ATMP1,   ATMP2,   ATMP3,      A5,  REG_FP, REG_SP */
s4 nregdescadr[] = { REG_TMP, REG_TMP, REG_RES, REG_RES, REG_RES, REG_SAV, REG_RES, REG_RES, REG_END };


/* Floatingpoint register description */
/*                        F0,      F1,   FTMP1,   FTMP2,   FTMP3,      F5,      F6,      F7 */
#if !defined(ENABLE_SOFTFLOAT)
s4 nregdescfloat[] = { REG_TMP, REG_TMP, REG_RES, REG_RES, REG_RES, REG_SAV, REG_SAV, REG_SAV, REG_END };
#else
s4 nregdescfloat[] = { REG_RES, REG_RES, REG_RES, REG_RES, REG_RES, REG_RES, REG_RES, REG_RES, REG_END };
#endif

const char *abi_registers_float_name [] = { "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7" };

const s4 abi_registers_float_argument[] 	;
const s4 abi_registers_float_saved[] 		= {5,6,7};
const s4 abi_registers_float_temporary[] 	= {0,1};


/* md_param_alloc_native *******************************************************
 *
 *    Pre-allocate arguments according the native ABI.
 *
 *    *******************************************************************************/

void md_param_alloc_native(methoddesc *md)
{
		/* For now use system ABI */
	        md_param_alloc(md);
}


/* md_param_alloc **************************************************************
 *
 *    Allocate Arguments to Stackslots according the Calling Conventions
 *
 *       --- in
 *       md->paramcount:           Number of arguments for this method
 *       md->paramtypes[].type:    Argument types
 *
 *       --- out
 *       md->params[].inmemory:    Argument spilled on stack
 *       md->params[].regoff:      Stack offset or rd->arg[int|flt]regs index
 *       md->memuse:               Stackslots needed for argument spilling
 *       md->argintreguse:         max number of integer arguments used
 *       md->argfltreguse:         max number of float arguments used
 *
 ********************************************************************************/
void md_param_alloc(methoddesc *md)
{
	paramdesc 	*pd;
	s4	stacksize;
	s4	i;

	pd = md->params;
	stacksize = 0;

	for (i=0; i<md->paramcount; i++, pd++)	{
		pd->inmemory = true;
		pd->regoff = stacksize;
		stacksize += IS_2_WORD_TYPE(md->paramtypes[i].type) ? 2:1;
	}

	md->memuse = stacksize;
	md->argintreguse = 0;
	md->argfltreguse = 0;
	md->argadrreguse = 0;
}

/* md_return_alloc *************************************************************

   Precolor the Java Stackelement containing the Return Value, if
   possible. We stick to the abi as closed as possible and therefore use
   %d0 for all word types %d0-%d1 for 2 word types. %f0-%f1 for floats/doubles
*******************************************************************************/

void md_return_alloc(jitdata *jd, stackptr stackslot)
{ 

/* The Problem: %a0, %a1, %d0 and %d1 are scratch registers by platform abi
 * so they are defined REG_TMP, which i think is correct. But a preallocated
 * register has to be REG_SAV it seems. One could make theese REG_SAV but 
 * then there would be a abi mismatch with native functions...for now no
 * return_alloc.*/

#if 0
	methodinfo   *m;
	registerdata *rd;
	methoddesc   *md;

	/* get required compiler data */

	m  = jd->m;
	rd = jd->rd;

	md = m->parseddesc;

	/* XXX wondering if this is correct */
	assert(rd->argintreguse == 0);
	assert(rd->argadrreguse == 0);
	assert(rd->argfltreguse == 0);

	VAR(stackslot->varnum)->flags = PREALLOC;
	switch (md->returntype.type)	{
		case TYPE_INT:
		case TYPE_ADR:
			VAR(stackslot->varnum)->vv.regoff = REG_RESULT;
			break;
		case TYPE_LNG:
			VAR(stackslot->varnum)->vv.regoff = REG_RESULT_PACKED;
			break;

		case TYPE_FLT:
		case TYPE_DBL:
			VAR(stackslot->varnum)->vv.regoff = REG_FRESULT;
			break;

		default: assert(0);
	}
#endif

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
 * vim:noexpandtab:sw=4:ts=4:
 */

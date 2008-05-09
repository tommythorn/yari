/* src/vm/jit/arm/md-abi.c - functions for arm ABI

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

#include "vm/jit/arm/md-abi.h"

#include "vm/global.h"

#include "vm/jit/abi.h"

#include "vmcore/descriptor.h"


/* register descripton array **************************************************/

s4 nregdescint[] = {
	REG_ARG, REG_ARG, REG_ARG, REG_ARG, REG_SAV, REG_SAV, REG_SAV, REG_SAV,
	REG_SAV, REG_RES, REG_RES, REG_RES, REG_RES, REG_RES, REG_RES, REG_RES,
	REG_END
};

const char *abi_registers_integer_name[] = {
	"a1", "a2", "a3", "a4", "v1", "v2", "v3", "v4",
	"v5", "t3", "t1", "t2", "ip", "sp", "lr", "pc",
};

const s4 abi_registers_integer_argument[] = {
	0,  /* a0 */
	1,  /* a1 */
	2,  /* a2 */
	3,  /* a3 */
	REG_SPLIT,
};

const s4 abi_registers_integer_saved[] = {
	4,  /* s0 */
	5,  /* s1 */
	6,  /* s2 */
	7,  /* s3 */
	8,  /* s4 */
};

const s4 abi_registers_integer_temporary[] = {
	-1,
};


#if defined(ENABLE_SOFTFLOAT)
s4 nregdescfloat[] = {
	REG_RES, REG_RES, REG_RES, REG_RES,
	REG_RES, REG_RES, REG_RES, REG_RES,
	REG_END
};
#else
/* TODO: FPA register usage conventions */
s4 nregdescfloat[] = {
	REG_TMP, REG_TMP, REG_TMP, REG_TMP,
	REG_TMP, REG_TMP, REG_RES, REG_RES,
	REG_END
};
#endif /* defined(ENABLE_SOFTFLOAT) */

const s4 abi_registers_float_argument[] = {
	-1,
};

const s4 abi_registers_float_saved[] = {
	-1,
};

const s4 abi_registers_float_temporary[] = {
#if defined(ENABLE_SOFTFLOAT)
	-1,
#else
	0,  /* ft0 */
	1,  /* ft1 */
	2,  /* ft2 */
	3,  /* ft3 */
	4,  /* ft4 */
	5,  /* ft5 */
#endif
};


/* md_param_alloc **************************************************************

   Allocate Arguments to Stackslots according the Calling Conventions

   --- in:
   md->paramcount:           Number of arguments for this method
   md->paramtypes[].type:    Argument types

   --- out:
   md->params[].inmemory:    Argument spilled on stack
   md->params[].regoff:      Stack offset or rd->arg[int|flt]regs index
   md->memuse:               Stackslots needed for argument spilling
   md->argintreguse:         max number of integer arguments used
   md->argfltreguse:         max number of float arguments used

*******************************************************************************/

void md_param_alloc(methoddesc *md)
{
	paramdesc *pd;
	s4         i;
	s4         reguse;
	s4         stacksize;

	/* set default values */
	reguse = 0;
	stacksize = 0;

	/* get params field of methoddesc */
	pd = md->params;

	for (i = 0; i < md->paramcount; i++, pd++) {
		switch (md->paramtypes[i].type) {
		case TYPE_INT:
		case TYPE_ADR:
		case TYPE_FLT:
			if (reguse < INT_ARG_CNT) {
				pd->inmemory = false;
				pd->regoff   = abi_registers_integer_argument[reguse];
				reguse++;
			}
			else {
				pd->inmemory = true;
				pd->regoff   = stacksize;
				stacksize++;
			}
			break;

		case TYPE_LNG:
		case TYPE_DBL:
			if (reguse+1 < INT_ARG_CNT) {
				pd->inmemory = false;
#if defined(__ARMEL__)
				pd->regoff   =
					PACK_REGS(abi_registers_integer_argument[reguse],
							  abi_registers_integer_argument[reguse + 1]);
#else
				pd->regoff   =
					PACK_REGS(abi_registers_integer_argument[reguse + 1],
							  abi_registers_integer_argument[reguse]);
#endif
				reguse += 2;
			}
			else if (reguse < INT_ARG_CNT) {
				pd->inmemory = false;
#if defined(__ARMEL__)
				pd->regoff   =
					PACK_REGS(abi_registers_integer_argument[reguse],
							  abi_registers_integer_argument[INT_ARG_CNT]);
#else
				pd->regoff   =
					PACK_REGS(abi_registers_integer_argument[INT_ARG_CNT],
							  abi_registers_integer_argument[reguse]);
#endif
				reguse++;
				stacksize++;
			}
			else {
				pd->inmemory = true;
				pd->regoff   = stacksize;
				stacksize += 2;
			}
			break;
		}
	}

	/* Since R0/R1 (==A0/A1) are used for passing return values, this
	   argument register usage has to be regarded, too. */

	if (md->returntype.type != TYPE_VOID) {
		if (!IS_2_WORD_TYPE(md->returntype.type)) {
			if (reguse < 1)
				reguse = 1;
		}
		else {
			if (reguse < 2)
				reguse = 2;
		}
	}

	/* fill register and stack usage */

	md->argintreguse = reguse;
	md->argfltreguse = 0;
	md->memuse       = stacksize;
}


/* md_param_alloc_native *******************************************************

   Pre-allocate arguments according the native ABI.

*******************************************************************************/

#define ALIGN_2(a)    do { if ((a) & 0x1) (a)++; } while (0)

void md_param_alloc_native(methoddesc *md)
{
	paramdesc *pd;
	s4         i;
	s4         reguse;
	s4         stacksize;

	/* set default values */

	reguse    = 0;
	stacksize = 0;

	/* get params field of methoddesc */

	pd = md->params;

	for (i = 0; i < md->paramcount; i++, pd++) {
		switch (md->paramtypes[i].type) {
		case TYPE_INT:
		case TYPE_ADR:
		case TYPE_FLT:
			if (reguse < INT_ARG_CNT) {
				pd->inmemory = false;
				pd->regoff   = abi_registers_integer_argument[reguse];
				reguse++;
			}
			else {
				pd->inmemory = true;
				pd->regoff   = stacksize;
				stacksize++;
			}
			break;

		case TYPE_LNG:
		case TYPE_DBL:
			if (reguse < (INT_ARG_CNT - 1)) {
#if defined(__ARM_EABI__)
				ALIGN_2(reguse);
#endif
				pd->inmemory = false;
#if defined(__ARMEL__)
				pd->regoff   =
					PACK_REGS(abi_registers_integer_argument[reguse],
							  abi_registers_integer_argument[reguse + 1]);
#else
				pd->regoff   =
					PACK_REGS(abi_registers_integer_argument[reguse + 1],
							  abi_registers_integer_argument[reguse]);
#endif
				reguse += 2;
			}
#if !defined(__ARM_EABI__)
			else if (reguse < INT_ARG_CNT) {
				pd->inmemory = false;
# if defined(__ARMEL__)
				pd->regoff   =
					PACK_REGS(abi_registers_integer_argument[reguse],
							  abi_registers_integer_argument[INT_ARG_CNT]);
# else
				pd->regoff   =
					PACK_REGS(abi_registers_integer_argument[INT_ARG_CNT],
							  abi_registers_integer_argument[reguse]);
# endif
				reguse++;
				stacksize++;
			}
#endif
			else {
#if defined(__ARM_EABI__)
				ALIGN_2(stacksize);
#endif
				pd->inmemory  = true;
				pd->regoff    = stacksize;
				reguse        = INT_ARG_CNT;
				stacksize    += 2;
			}
			break;
		}
	}

	/* Since R0/R1 (==A0/A1) are used for passing return values, this
	   argument register usage has to be regarded, too. */

	if (md->returntype.type != TYPE_VOID) {
		if (!IS_2_WORD_TYPE(md->returntype.type)) {
			if (reguse < 1)
				reguse = 1;
		}
		else {
			if (reguse < 2)
				reguse = 2;
		}
	}

	/* fill register and stack usage */

	md->argintreguse = reguse;
	md->argfltreguse = 0;
	md->memuse       = stacksize;
}


/* md_return_alloc *************************************************************

   Precolor the Java Stackelement containing the Return Value, if possible.

   --- in
   m:                       Methodinfo of current method
   return_type:             Return Type of the Method (TYPE_INT.. TYPE_ADR)
                            TYPE_VOID is not allowed!
   stackslot:               Java Stackslot to contain the Return Value

   --- out
   if precoloring was possible:
   VAR(stackslot->varnum)->flags = PREALLOC
   VAR(stackslot->varnum)->vv.regoff = [REG_RESULT, (REG_RESULT2/REG_RESULT), REG_FRESULT]
   rd->arg[flt|int]reguse   set to a value according the register usage

*******************************************************************************/

void md_return_alloc(jitdata *jd, stackptr stackslot)
{
	methodinfo   *m;
	registerdata *rd;
	methoddesc   *md;

	/* get required compiler data */

	m  = jd->m;
	rd = jd->rd;

	md = m->parseddesc;

	/* In Leafmethods Local Vars holding parameters are precolored to
	   their argument register -> so leafmethods with paramcount > 0
	   could already use R0 == a00! */

	if (!jd->isleafmethod || (md->paramcount == 0)) {
		/* Only precolor the stackslot, if it is not a SAVEDVAR <->
		   has not to survive method invokations. */

		if (!(stackslot->flags & SAVEDVAR)) {
#if !defined(ENABLE_SOFTFLOAT)
			/* Stackelements containing float or double values
			   (TYPE_FLT | TYPE_DBL) cannot be precolored, because we
			   use integer register to pass return values. (floats:
			   R0, doubles: R0/R1) */

			if (!IS_FLT_DBL_TYPE(md->returntype.type)) {
#endif

				VAR(stackslot->varnum)->flags = PREALLOC;

				if (!IS_2_WORD_TYPE(md->returntype.type)) {
					if (rd->argintreguse < 1)
						rd->argintreguse = 1;

					VAR(stackslot->varnum)->vv.regoff = REG_RESULT;
				}
				else {
					if (rd->argintreguse < 2)
						rd->argintreguse = 2;

					VAR(stackslot->varnum)->vv.regoff = REG_RESULT_PACKED;
				}

#if !defined(ENABLE_SOFTFLOAT)
			}
#endif
		}
	}
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

/* src/vm/jit/reg.c - register allocator setup

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

   $Id: reg.c 7713 2007-04-15 21:49:48Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "arch.h"
#include "md-abi.h"

#include "mm/memory.h"
#include "vm/jit/abi.h"
#include "vm/jit/reg.h"


/* reg_setup *******************************************************************

   TODO

*******************************************************************************/

void reg_setup(jitdata *jd)
{
	methodinfo   *m;
	registerdata *rd;
	s4            i;

	/* get required compiler data */

	m  = jd->m;
	rd = jd->rd;

	/* setup the integer register table */

	rd->tmpintregs = DMNEW(s4, INT_TMP_CNT);
	rd->savintregs = DMNEW(s4, INT_SAV_CNT);
	rd->freeargintregs = DMNEW(s4, INT_ARG_CNT);
	rd->freetmpintregs = DMNEW(s4, INT_TMP_CNT);
	rd->freesavintregs = DMNEW(s4, INT_SAV_CNT);

	rd->argintreguse = 0;
	rd->tmpintreguse = 0;
	rd->savintreguse = 0;

	for (i = 0; i < INT_REG_CNT; i++) {
		switch (nregdescint[i]) {
		case REG_RET:
			rd->intreg_ret = i; 
			break;
		case REG_SAV:
			rd->savintregs[rd->savintreguse++] = i;
			break;
		case REG_TMP:
  			rd->tmpintregs[rd->tmpintreguse++] = i; 
			break;
		}
	}
	assert(rd->savintreguse == INT_SAV_CNT);
	assert(rd->tmpintreguse == INT_TMP_CNT);

#ifdef HAS_ADDRESS_REGISTER_FILE
	/* setup the address register table */

	rd->argadrregs = DMNEW(s4, ADR_ARG_CNT);
	rd->tmpadrregs = DMNEW(s4, ADR_TMP_CNT);
	rd->savadrregs = DMNEW(s4, ADR_SAV_CNT);
	rd->freeargadrregs = DMNEW(s4, ADR_ARG_CNT);
	rd->freetmpadrregs = DMNEW(s4, ADR_TMP_CNT);
	rd->freesavadrregs = DMNEW(s4, ADR_SAV_CNT);

	/*rd->adrreg_argnum = 0; XXX ask twisti */
	rd->argadrreguse = 0;
	rd->tmpadrreguse = 0;
	rd->savadrreguse = 0;

	for (i = 0; i < ADR_REG_CNT; i++) {
		switch (nregdescadr[i]) {
		case REG_RET:
			rd->adrreg_ret = i; 
			break;
		case REG_SAV:
			rd->savadrregs[rd->savadrreguse++] = i;
			break;
		case REG_TMP:
  			rd->tmpadrregs[rd->tmpadrreguse++] = i; 
			break;
		case REG_ARG:
			rd->argadrregs[rd->argadrreguse++] = i;
			break;
		}
	}
	assert(rd->savadrreguse == ADR_SAV_CNT);
	assert(rd->tmpadrreguse == ADR_TMP_CNT);
	assert(rd->argadrreguse == ADR_ARG_CNT);
#endif
		
	/* setup the float register table */

	rd->tmpfltregs = DMNEW(s4, FLT_TMP_CNT);
	rd->savfltregs = DMNEW(s4, FLT_SAV_CNT);
	rd->freeargfltregs = DMNEW(s4, FLT_ARG_CNT);
	rd->freetmpfltregs = DMNEW(s4, FLT_TMP_CNT);
	rd->freesavfltregs = DMNEW(s4, FLT_SAV_CNT);

	rd->argfltreguse = 0;
	rd->tmpfltreguse = 0;
	rd->savfltreguse = 0;

	for (i = 0; i < FLT_REG_CNT; i++) {
		switch (nregdescfloat[i]) {
		case REG_RET:
			rd->fltreg_ret = i;
			break;
		case REG_SAV:
			rd->savfltregs[rd->savfltreguse++] = i;
			break;
		case REG_TMP:
			rd->tmpfltregs[rd->tmpfltreguse++] = i;
			break;
		}
	}
	assert(rd->savfltreguse == FLT_SAV_CNT);
	assert(rd->tmpfltreguse == FLT_TMP_CNT);


	rd->freemem    = DMNEW(s4, m->maxstack);
#if defined(HAS_4BYTE_STACKSLOT)
	rd->freemem_2  = DMNEW(s4, m->maxstack);
#endif

#if defined(SPECIALMEMUSE)
# if defined(__DARWIN__)
	/* 6*4=24 byte linkage area + 8*4=32 byte minimum parameter Area */
	rd->memuse = LA_SIZE_IN_POINTERS + INT_ARG_CNT; 
# else
	rd->memuse = LA_SIZE_IN_POINTERS;
# endif
#else
	rd->memuse = 0; /* init to zero -> analyse_stack will set it to a higher  */
	                /* value, if appropriate */
#endif

	/* Set rd->arg*reguse to *_ARG_CNBT to not use unused argument            */
	/* registers as temp registers  */
#if defined(HAS_ADDRESS_REGISTER_FILE)
	rd->argadrreguse = 0;
#endif /* defined(HAS_ADDRESS_REGISTER_FILE) */
	rd->argintreguse = 0;
	rd->argfltreguse = 0;
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

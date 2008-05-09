/* src/vm/cfg.c - build a control-flow graph

   Copyright (C) 2006 R. Grafl, A. Krall, C. Kruegel, C. Oates,
   R. Obermaisser, M. Platter, M. Probst, S. Ring, E. Steiner,
   C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich, J. Wenninger,
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

   Changes: Edwin Steiner

   $Id: cacao.c 4357 2006-01-22 23:33:38Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "mm/memory.h"
#include "vm/jit/jit.h"
#include "vm/jit/stack.h"


/* cfg_allocate_predecessors ***************************************************

   Allocates the predecessor array, if there is none, and resets the
   predecessor count.

*******************************************************************************/

static void cfg_allocate_predecessors(basicblock *bptr)
{
	if (bptr->predecessors == NULL) {
		bptr->predecessors = DMNEW(basicblock*, bptr->predecessorcount);

		bptr->predecessorcount = 0;
	}
}


/* cfg_allocate_successors *****************************************************

   Allocates the succecessor array, if there is none, and resets the
   predecessor count.

*******************************************************************************/

static void cfg_allocate_successors(basicblock *bptr)
{
	if (bptr->successors == NULL) {
		bptr->successors = DMNEW(basicblock*, bptr->successorcount);

		bptr->successorcount = 0;
	}
}


/* cfg_insert_predecessor ******************************************************

   Inserts a predecessor into the array, but checks for duplicate
   entries.  This is used for TABLESWITCH and LOOKUPSWITCH.

*******************************************************************************/

static void cfg_insert_predecessors(basicblock *bptr, basicblock *pbptr)
{
	basicblock **tbptr;
	s4           i;

	tbptr = bptr->predecessors;

	/* check if the predecessors is already stored in the array */

	for (i = 0; i < bptr->predecessorcount; i++, tbptr++)
		if (*tbptr == pbptr)
			return;

	/* not found, insert it */

	bptr->predecessors[bptr->predecessorcount] = pbptr;
	bptr->predecessorcount++;
}


/* cfg_build *******************************************************************

   Build a control-flow graph in finding all predecessors and
   successors for the basic blocks.

*******************************************************************************/

bool cfg_build(jitdata *jd)
{
	basicblock      *bptr;
	basicblock      *tbptr;
	basicblock      *ntbptr;
	instruction     *iptr;
	branch_target_t *table;
	lookup_target_t *lookup;
	s4               i;

	/* process all basic blocks to find the predecessor/successor counts */

	bptr = jd->basicblocks;

	for (bptr = jd->basicblocks; bptr != NULL; bptr = bptr->next) {
		if ((bptr->icount == 0) || (bptr->flags == BBUNDEF))
			continue;

		iptr = bptr->iinstr + bptr->icount - 1;

		/* skip NOPs at the end of the block */

		while (iptr->opc == ICMD_NOP) {
			if (iptr == bptr->iinstr)
				break;
			iptr--;
		}

		switch (iptr->opc) {
		case ICMD_RETURN:
		case ICMD_IRETURN:
		case ICMD_LRETURN:
		case ICMD_FRETURN:
		case ICMD_DRETURN:
		case ICMD_ARETURN:
		case ICMD_ATHROW:
			break;

		case ICMD_IFEQ:
		case ICMD_IFNE:
		case ICMD_IFLT:
		case ICMD_IFGE:
		case ICMD_IFGT:
		case ICMD_IFLE:

		case ICMD_IFNULL:
		case ICMD_IFNONNULL:

		case ICMD_IF_ICMPEQ:
		case ICMD_IF_ICMPNE:
		case ICMD_IF_ICMPLT:
		case ICMD_IF_ICMPGE:
		case ICMD_IF_ICMPGT:
		case ICMD_IF_ICMPLE:

		case ICMD_IF_ACMPEQ:
		case ICMD_IF_ACMPNE:
			bptr->successorcount += 2;

			tbptr  = iptr->dst.block;
			ntbptr = bptr->next;

			tbptr->predecessorcount++;
			ntbptr->predecessorcount++;
			break;

		case ICMD_JSR:
			bptr->successorcount++;

			tbptr = iptr->sx.s23.s3.jsrtarget.block;
			tbptr->predecessorcount++;
			break;

		case ICMD_GOTO:
		case ICMD_RET:
			bptr->successorcount++;

			tbptr = iptr->dst.block;
			tbptr->predecessorcount++;
			break;

		case ICMD_TABLESWITCH:
			table = iptr->dst.table;

			bptr->successorcount++;

			tbptr = table->block;
			tbptr->predecessorcount++;
			table++;

			i = iptr->sx.s23.s3.tablehigh - iptr->sx.s23.s2.tablelow + 1;

			while (--i >= 0) {
				bptr->successorcount++;

				tbptr = table->block;
				tbptr->predecessorcount++;
				table++;
			}
			break;
					
		case ICMD_LOOKUPSWITCH:
			lookup = iptr->dst.lookup;

			bptr->successorcount++;

			tbptr = iptr->sx.s23.s3.lookupdefault.block;
			tbptr->predecessorcount++;

			i = iptr->sx.s23.s2.lookupcount;

			while (--i >= 0) {
				bptr->successorcount++;

				tbptr = lookup->target.block;
				tbptr->predecessorcount++;
				lookup++;
			}
			break;

		default:
			bptr->successorcount++;

			tbptr = bptr->next;

			/* An exception handler has no predecessors. */

			if (tbptr->type != BBTYPE_EXH)
				tbptr->predecessorcount++;
			break;
		}
	}

	/* Second iteration to allocate the arrays and insert the basic
	   block pointers. */

	bptr = jd->basicblocks;

	for (bptr = jd->basicblocks; bptr != NULL; bptr = bptr->next) {
		if ((bptr->icount == 0) || (bptr->flags == BBUNDEF))
			continue;

		iptr = bptr->iinstr + bptr->icount - 1;

		/* skip NOPs at the end of the block */

		while (iptr->opc == ICMD_NOP) {
			if (iptr == bptr->iinstr)
				break;
			iptr--;
		}

		switch (iptr->opc) {
		case ICMD_RETURN:
		case ICMD_IRETURN:
		case ICMD_LRETURN:
		case ICMD_FRETURN:
		case ICMD_DRETURN:
		case ICMD_ARETURN:
		case ICMD_ATHROW:
			break;

		case ICMD_IFEQ:
		case ICMD_IFNE:
		case ICMD_IFLT:
		case ICMD_IFGE:
		case ICMD_IFGT:
		case ICMD_IFLE:

		case ICMD_IFNULL:
		case ICMD_IFNONNULL:

		case ICMD_IF_ICMPEQ:
		case ICMD_IF_ICMPNE:
		case ICMD_IF_ICMPLT:
		case ICMD_IF_ICMPGE:
		case ICMD_IF_ICMPGT:
		case ICMD_IF_ICMPLE:

		case ICMD_IF_ACMPEQ:
		case ICMD_IF_ACMPNE:
			tbptr  = iptr->dst.block;
			ntbptr = bptr->next;

			cfg_allocate_successors(bptr);

			bptr->successors[0] = tbptr;
			bptr->successors[1] = ntbptr;
			bptr->successorcount += 2;

			cfg_allocate_predecessors(tbptr);
			cfg_allocate_predecessors(ntbptr);

			tbptr->predecessors[tbptr->predecessorcount] = bptr;
			tbptr->predecessorcount++;

			ntbptr->predecessors[ntbptr->predecessorcount] = bptr;
			ntbptr->predecessorcount++;
			break;

		case ICMD_JSR:
			tbptr = iptr->sx.s23.s3.jsrtarget.block;
			goto goto_tail;

		case ICMD_GOTO:
		case ICMD_RET:
			tbptr = iptr->dst.block;
goto_tail:
			cfg_allocate_successors(bptr);

			bptr->successors[0] = tbptr;
			bptr->successorcount++;

			cfg_allocate_predecessors(tbptr);

			tbptr->predecessors[tbptr->predecessorcount] = bptr;
			tbptr->predecessorcount++;
			break;

		case ICMD_TABLESWITCH:
			table = iptr->dst.table;

			tbptr = table->block;
			table++;

			cfg_allocate_successors(bptr);

			bptr->successors[0] = tbptr;
			bptr->successorcount++;

			cfg_allocate_predecessors(tbptr);

			tbptr->predecessors[tbptr->predecessorcount] = bptr;
			tbptr->predecessorcount++;

			i = iptr->sx.s23.s3.tablehigh - iptr->sx.s23.s2.tablelow + 1;

			while (--i >= 0) {
				tbptr = table->block;
				table++;

				bptr->successors[bptr->successorcount] = tbptr;
				bptr->successorcount++;

				cfg_allocate_predecessors(tbptr);
				cfg_insert_predecessors(tbptr, bptr);
			}
			break;
					
		case ICMD_LOOKUPSWITCH:
			lookup = iptr->dst.lookup;

			tbptr = iptr->sx.s23.s3.lookupdefault.block;

			cfg_allocate_successors(bptr);

			bptr->successors[0] = tbptr;
			bptr->successorcount++;

			cfg_allocate_predecessors(tbptr);

			tbptr->predecessors[tbptr->predecessorcount] = bptr;
			tbptr->predecessorcount++;

			i = iptr->sx.s23.s2.lookupcount;

			while (--i >= 0) {
				tbptr = lookup->target.block;
				lookup++;

				bptr->successors[bptr->successorcount] = tbptr;
				bptr->successorcount++;

				cfg_allocate_predecessors(tbptr);
				cfg_insert_predecessors(tbptr, bptr);
			}
			break;

		default:
			tbptr = bptr->next;

			cfg_allocate_successors(bptr);

			bptr->successors[0] = tbptr;
			bptr->successorcount++;

			/* An exception handler has no predecessors. */

			if (tbptr->type != BBTYPE_EXH) {
				cfg_allocate_predecessors(tbptr);

				tbptr->predecessors[tbptr->predecessorcount] = bptr;
				tbptr->predecessorcount++;
			}
			break;
		}
	}

	/* everything's ok */

	return true;
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

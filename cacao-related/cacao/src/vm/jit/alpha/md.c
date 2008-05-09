/* src/vm/jit/alpha/md.c - machine dependent Alpha functions

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

   $Id: md.c 7596 2007-03-28 21:05:53Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <ucontext.h>

#if defined(__LINUX__)
# include <asm/fpu.h>

extern unsigned long ieee_get_fp_control();
extern void ieee_set_fp_control(unsigned long fp_control);
#endif

#include "vm/types.h"

#include "vm/jit/alpha/md-abi.h"

#include "vm/exceptions.h"
#include "vm/stringlocal.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/stacktrace.h"

#if !defined(NDEBUG) && defined(ENABLE_DISASSEMBLER)
#include "vmcore/options.h" /* XXX debug */
#include "vm/jit/disass.h" /* XXX debug */
#endif


/* global variables ***********************************************************/

bool has_ext_instr_set = false;             /* has instruction set extensions */


/* md_init *********************************************************************

   Do some machine dependent initialization.

*******************************************************************************/

void md_init(void)
{
	/* check for extended instruction set */

	has_ext_instr_set = !asm_md_init();

#if defined(__LINUX__)
	/* Linux on Digital Alpha needs an initialisation of the ieee
	   floating point control for IEEE compliant arithmetic (option
	   -mieee of GCC). Under Digital Unix this is done
	   automatically. */

	/* initialize floating point control */

	ieee_set_fp_control(ieee_get_fp_control()
						& ~IEEE_TRAP_ENABLE_INV
						& ~IEEE_TRAP_ENABLE_DZE
/*  						& ~IEEE_TRAP_ENABLE_UNF   we dont want underflow */
						& ~IEEE_TRAP_ENABLE_OVF);
#endif
}


/* md_stacktrace_get_returnaddress *********************************************

   Returns the return address of the current stackframe, specified by
   the passed stack pointer and the stack frame size.

*******************************************************************************/

u1 *md_stacktrace_get_returnaddress(u1 *sp, u4 framesize)
{
	u1 *ra;

	/* on Alpha the return address is located on the top of the stackframe */

	ra = *((u1 **) (sp + framesize - SIZEOF_VOID_P));

	return ra;
}


/* md_get_method_patch_address *************************************************

   Gets the patch address of the currently compiled method. The offset
   is extracted from the load instruction(s) before the jump and added
   to the right base address (PV or REG_METHODPTR).

   INVOKESTATIC/SPECIAL:

   a77bffb8    ldq     pv,-72(pv)
   6b5b4000    jsr     (pv)

   INVOKEVIRTUAL:

   a7900000    ldq     at,0(a0)
   a77c0000    ldq     pv,0(at)
   6b5b4000    jsr     (pv)

   INVOKEINTERFACE:

   a7900000    ldq     at,0(a0)
   a79cff98    ldq     at,-104(at)
   a77c0018    ldq     pv,24(at)
   6b5b4000    jsr     (pv)

*******************************************************************************/

u1 *md_get_method_patch_address(u1 *ra, stackframeinfo *sfi, u1 *mptr)
{
	u4  mcode;
	s4  offset;
	u1 *pa;                             /* patch address                      */

	/* go back to the actual load instruction (2 instructions on Alpha) */

	ra = ra - 2 * 4;

	/* get first instruction word on current PC */

	mcode = *((u4 *) ra);

	/* check if we have 2 instructions (lui) */

	if ((mcode >> 16) == 0x3c19) {
		/* XXX write a regression for this */
		assert(0);

		/* get displacement of first instruction (lui) */

		offset = (s4) (mcode << 16);

		/* get displacement of second instruction (daddiu) */

		mcode = *((u4 *) (ra + 1 * 4));

		assert((mcode >> 16) == 0x6739);

		offset += (s2) (mcode & 0x0000ffff);

	} else {
		/* get first instruction (ldq) */

		mcode = *((u4 *) ra);

		/* get the offset from the instruction */

		offset = (s2) (mcode & 0x0000ffff);

		/* check for call with REG_METHODPTR: ldq pv,0(at) */

		if ((mcode >> 16) == 0xa77c) {
			/* in this case we use the passed method pointer */

			/* return NULL if no mptr was specified (used for replacement) */

			if (mptr == NULL)
				return NULL;

			pa = mptr + offset;

		} else {
			/* in the normal case we check for a `ldq pv,-72(pv)' instruction */

			assert((mcode >> 16) == 0xa77b);

			/* and get the final data segment address */

			pa = sfi->pv + offset;
		}
	}

	return pa;
}


/* md_codegen_get_pv_from_pc ***************************************************

   Machine code:

   6b5b4000    jsr     (pv)
   277afffe    ldah    pv,-2(ra)
   237ba61c    lda     pv,-23012(pv)

*******************************************************************************/

u1 *md_codegen_get_pv_from_pc(u1 *ra)
{
	u1 *pv;
	u4  mcode;
	s4  offset;

	pv = ra;

	/* get first instruction word after jump */

	mcode = *((u4 *) ra);

	/* check if we have 2 instructions (ldah, lda) */

	if ((mcode >> 16) == 0x277a) {
		/* get displacement of first instruction (ldah) */

		offset = (s4) (mcode << 16);
		pv += offset;

		/* get displacement of second instruction (lda) */

		mcode = *((u4 *) (ra + 1 * 4));

		assert((mcode >> 16) == 0x237b);

		offset = (s2) (mcode & 0x0000ffff);
		pv += offset;
	}
	else {
		/* get displacement of first instruction (lda) */

		assert((mcode >> 16) == 0x237a);

		offset = (s2) (mcode & 0x0000ffff);
		pv += offset;
	}

	return pv;
}


/* md_cacheflush ***************************************************************

   Calls the system's function to flush the instruction and data
   cache.

*******************************************************************************/

void md_cacheflush(u1 *addr, s4 nbytes)
{
	asm_cacheflush(addr, nbytes);
}


/* md_icacheflush **************************************************************

   Calls the system's function to flush the instruction cache.

*******************************************************************************/

void md_icacheflush(u1 *addr, s4 nbytes)
{
	asm_cacheflush(addr, nbytes);
}


/* md_patch_replacement_point **************************************************

   Patch the given replacement point.

*******************************************************************************/

#if defined(ENABLE_REPLACEMENT)
void md_patch_replacement_point(codeinfo *code, s4 index, rplpoint *rp, u1 *savedmcode)
{
	s4 disp;
	u4 mcode;

	if (index < 0) {
		/* restore the patched-over instruction */
		*(u4*)(rp->pc) = *(u4*)(savedmcode);
	}
	else {
		/* save the current machine code */
		*(u4*)(savedmcode) = *(u4*)(rp->pc);

		/* build the machine code for the patch */
		disp = ((u4*)code->replacementstubs - (u4*)rp->pc)
			   + index * REPLACEMENT_STUB_SIZE
			   - 1;

		/* BR */
        mcode = (((s4) (0x30)) << 26) | ((REG_ZERO) << 21) | ((disp) & 0x1fffff);

		/* write the new machine code */
		*(u4*)(rp->pc) = mcode;
	}
	
#if !defined(NDEBUG) && defined(ENABLE_DISASSEMBLER) && 0
	{
		u1* u1ptr = rp->pc;
		DISASSINSTR(u1ptr);
		fflush(stdout);
	}
#endif
			
	/* flush instruction cache */
    md_icacheflush(rp->pc,4);
}
#endif /* defined(ENABLE_REPLACEMENT) */


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

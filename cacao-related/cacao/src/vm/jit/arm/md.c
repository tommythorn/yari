/* src/vm/jit/arm/md.c - machine dependent Arm functions

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

   $Id: md.c 7653 2007-04-03 14:34:23Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "vm/jit/arm/md-abi.h"

#include "vm/exceptions.h"
#include "vm/global.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/md.h"

#include "vm/jit/codegen-common.h" /* REMOVE ME: for codegendata */


/* md_init *********************************************************************

   Do some machine dependent initialization.

*******************************************************************************/

void md_init(void)
{
	/* do nothing here */
}


/* md_stacktrace_get_returnaddress *********************************************

   Returns the return address of the current stackframe, specified by
   the passed stack pointer and the stack frame size.

*******************************************************************************/

u1 *md_stacktrace_get_returnaddress(u1 *sp, u4 framesize)
{
	u1 *ra;

	/* On ARM the return address is located on the top of the
	   stackframe. */
	/* ATTENTION: This is only true for non-leaf methods!!! */

	ra = *((u1 **) (sp + framesize - SIZEOF_VOID_P));

	return ra;
}


/* md_assembler_get_patch_address **********************************************

   Gets the patch address of the currently compiled method. The offset
   is extracted from the load instruction(s) before the jump and added
   to the right base address (PV or REG_METHODPTR).

   Machine code:

   e51cc040    ldr   ip, [ip, #-64]
   e1a0e00f    mov   lr, pc
   e1a0f00c    mov   pc, ip

   or

   e590b000    ldr   fp, [r0]
   e59bc000    ldr   ip, [fp]
   e1a0e00f    mov   lr, pc
   e1a0f00c    mov   pc, ip

   How we find out the patching address to store new method pointer:
    - loaded IP with LDR IP,[METHODPTR]?
        yes=INVOKEVIRTUAL or INVOKEINTERFACE (things are easy!)
    - loaded IP from data segment
        yes=INVOKESTATIC or INVOKESPECIAL (things are complicated)
        recompute pointer to data segment, maybe larger offset 

*******************************************************************************/

u1 *md_get_method_patch_address(u1 *ra, stackframeinfo *sfi, u1 *mptr)
{
	u4  mcode;
	s4  offset;
	u1 *pa;                             /* patch address                      */

	/* sanity check: are we inside jit code? */

	assert(*((u4 *) (ra - 2*4)) == 0xe1a0e00f /*MOV LR,PC*/);
	assert(*((u4 *) (ra - 1*4)) == 0xe1a0f00c /*MOV PC,IP*/);

	/* get the load instruction and offset */

	mcode  = *((u4 *) (ra - 12));
	offset = (s4) (mcode & 0x0fff);

	assert ((mcode & 0xff70f000) == 0xe510c000);

	if ((mcode & 0x000f0000) == 0x000b0000) {
		/* sanity check: offset was positive */

		assert((mcode & 0x00800000) == 0x00800000);

		/* we loaded from REG_METHODPTR */

		pa = mptr + offset;
	}
	else {
		/* sanity check: we loaded from REG_IP; offset was negative or zero */

		assert((mcode & 0x008f0000) == 0x000c0000 ||
		       (mcode & 0x008f0fff) == 0x008c0000);

		/* we loaded from data segment; offset can be larger */

		mcode = *((u4 *) (ra - 4*4));

		/* check for "SUB IP, IP, #??, ROTL 12" */

		if ((mcode & 0xffffff00) == 0xe24cca00)
			offset += (s4) ((mcode & 0x00ff) << 12);

		/* and get the final data segment address */

		pa = sfi->pv - offset;        
	}

	return pa;
}


/* md_codegen_get_pv_from_pc ***************************************************

   TODO: document me

*******************************************************************************/

u1 *md_codegen_get_pv_from_pc(u1 *ra)
{
	u1 *pv;
	u4  mcode1, mcode2, mcode3;

	pv = ra;

	/* this can either be a RECOMPUTE_IP in JIT code or a fake in asm_calljavafunction */
	mcode1 = *((u4*) ra);
	if ((mcode1 & 0xffffff00) == 0xe24fcf00 /*sub ip,pc,#__*/)
		pv -= (s4) ((mcode1 & 0x000000ff) <<  2);
	else if ((mcode1 & 0xffffff00) == 0xe24fc000 /*sub ip,pc,#__*/)
		pv -= (s4) (mcode1 & 0x000000ff);
	else {
		/* if this happens, we got an unexpected instruction at (*ra) */
		vm_abort("Unable to find method: %p (instr=%x)", ra, mcode1);
	}

	/* if we have a RECOMPUTE_IP there can be more than one instruction */
	mcode2 = *((u4*) (ra + 4));
	mcode3 = *((u4*) (ra + 8));
	if ((mcode2 & 0xffffff00) == 0xe24ccb00 /*sub ip,ip,#__*/)
		pv -= (s4) ((mcode2 & 0x000000ff) << 10);
	if ((mcode3 & 0xffffff00) == 0xe24cc700 /*sub ip,ip,#__*/)
		pv -= (s4) ((mcode3 & 0x000000ff) << 18);

	/* we used PC-relative adressing; but now it is LR-relative */
	pv += 8;

	/* if we found our method the data segment has to be valid */
	/* we check this by looking up the IsLeaf field, which has to be boolean */
	assert( *((s4*)pv-4) == (s4)true || *((s4*)pv-4) == (s4)false ); 

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


/* md_dcacheflush **************************************************************

   Calls the system's function to flush the data cache.

*******************************************************************************/

void md_dcacheflush(u1 *addr, s4 nbytes)
{
	asm_cacheflush(addr, nbytes);
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

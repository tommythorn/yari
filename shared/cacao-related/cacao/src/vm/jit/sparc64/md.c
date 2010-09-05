/* src/vm/jit/alpha/md.c - machine dependent SPARC functions

   Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
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

   $Id: md.c 6265 2007-01-02 20:40:57Z edwin $

*/


#include "config.h"

#include <assert.h>


#include "vm/types.h"

#include "vm/jit/sparc64/md-abi.h"

#include "vm/exceptions.h"
#include "vm/stringlocal.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/stacktrace.h"

/* assembler function prototypes **********************************************/
void asm_store_fp_state_reg(u8 *mem);
void asm_load_fp_state_reg(u8 *mem);



/* shift away 13-bit immediate,  mask rd and rs1    */
#define SHIFT_AND_MASK(instr) \
	((instr >> 13) & 0x60fc1)

/* NOP is defined as a SETHI instruction with rd and imm. set to zero */
/* therefore we check if the 22-bit immediate is zero */
#define IS_SETHI(instr) \
	(((instr & 0xc1c00000)  == 0x01000000) \
	&& ((instr & 0x3fffff) != 0x0))
	
#define IS_LDX_IMM(instr) \
	(((instr >> 13) & 0x60fc1) == 0x602c1)
	
#define IS_SUB(instr) \
	(((instr >> 13) & 0x60fc0) == 0x40100)

inline s2 decode_13bit_imm(u4 instr) {
	s2 imm;

	/* mask everything else in the instruction */
	imm = instr & 0x00001fff;

	/* sign extend 13-bit to 16-bit */
	imm <<= 3;
	imm >>= 3;

	return imm;
}

/* md_init *********************************************************************

   Do some machine dependent initialization.

*******************************************************************************/

void md_init(void)
{
	/* do nothing */
}


/* md_stacktrace_get_returnaddress *********************************************

   Returns the return address of the current stackframe, specified by
   the passed stack pointer and the stack frame size.

*******************************************************************************/

u1 *md_stacktrace_get_returnaddress(u1 *sp, u4 framesize)
{
	u1 *ra;
	/* flush register windows to the stack */
	__asm__ ("flushw");

	/* the return address resides in register i7, the last register in the
	 * 16-extended-word save area
	 */
	ra = *((u1 **) (sp + 120 + BIAS));
	
	/* NOTE: on SPARC ra is the address of the call instruction */

	return ra;
}

u1 *md_get_framepointer(u1 *sp)
{
	u1 *fp;
	/* flush register windows to the stack */
	__asm__ ("flushw");

	fp = *((u1 **) (sp + 112 + BIAS));

	return fp;
}

u1 *md_get_pv_from_stackframe(u1 *sp)
{
	u1 *pv;
	/* flush register windows to the stack */
	__asm__ ("flushw");

	pv = *((u1 **) (sp + 104 + BIAS));

	return pv;
}

/* md_codegen_get_pv_from_pc ***************************************************

   This reconstructs and returns the PV of a method given a return address
   pointer. (basically, same was as the generated code following the jump does)
   
   Machine code:

   6b5b4000    jmpl    (pv)
   10000000    nop
   277afffe    ldah    pv,-2(ra)
   237ba61c    lda     pv,-23012(pv)

*******************************************************************************/

u1 *md_codegen_get_pv_from_pc(u1 *ra)
{
	u1 *pv;
	u8  mcode;
	u4  mcode_masked;
	s4  offset;

	pv = ra;

	/* get the instruction word after jump and nop */
	mcode = *((u4 *) (ra+8) );

	/* check if we have a sethi insruction */
	if (IS_SETHI(mcode)) {
		s4 xor_imm;
				
		/* get 22-bit immediate of sethi instruction */
		offset = (s4) (mcode & 0x3fffff);
		offset = offset << 10;
		
		/* now the xor */
		mcode = *((u4 *) (ra+12) );
		xor_imm = decode_13bit_imm(mcode);
		
		offset ^= xor_imm;	 
	}
	else {
		u4 mcode_masked;
		
		mcode_masked = SHIFT_AND_MASK(mcode);

		assert(mcode_masked == 0x40001);

		/* mask and extend the negative sign for the 13 bit immediate */
		offset = decode_13bit_imm(mcode);
	}
	
	pv += offset;

	return pv;
}

/* md_get_method_patch_address *************************************************

   Gets the patch address of the currently compiled method. The offset
   is extracted from the load instruction(s) before the jump and added
   to the right base address (PV or REG_METHODPTR).

   INVOKESTATIC/SPECIAL:

   dfdeffb8    ldx      [i5 - 72],o5
   03c0f809    jmp      o5
   00000000    nop

   INVOKEVIRTUAL:

   dc990000    ld       t9,0(a0)
   df3e0000    ld       [g2 + 0],o5
   03c0f809    jmp      o5
   00000000    nop

   INVOKEINTERFACE:

   dc990000    ld       t9,0(a0)
   df39ff90    ld       [g2 - 112],g2
   df3e0018    ld       [g2 + 24],o5
   03c0f809    jmp      o5
   00000000    nop

*******************************************************************************/

u1 *md_get_method_patch_address(u1 *ra, stackframeinfo *sfi, u1 *mptr)
{
	u4  mcode, mcode_masked;
	s4  offset;
	u1 *pa, *iptr;

	/* go back to the location of a possible sethi (3 instruction before jump) */
	/* note: ra is the address of the jump instruction on SPARC                */
	iptr = ra - 3 * 4;

	/* get first instruction word on current PC */

	mcode = *((u4 *) iptr);

	/* check for sethi instruction */

	if (IS_SETHI(mcode)) {
		/* XXX write a regression for this */

		/* get 22-bit immediate of sethi instruction */

		offset = (s4) (mcode & 0x3fffff);
		offset = offset << 10;
		
		/* goto next instruction */
		iptr += 4;
		mcode = *((u4 *) iptr);
		
		/* make sure it's a sub instruction (pv - big_disp) */
		assert(IS_SUB(mcode));
		offset = -offset;

		/* get displacement of load instruction */

		mcode = *((u4 *) (ra - 1 * 4));
		assert(IS_LDX_IMM(mcode));

		offset += decode_13bit_imm(mcode);
		
		pa = sfi->pv + offset;

		return pa;
	}


	/* simple (one-instruction) load */
	iptr = ra - 1 * 4;
	mcode = *((u4 *) iptr);

	/* shift and mask rd */

	mcode_masked = (mcode >> 13) & 0x060fff;
	
	/* get the offset from the instruction */

	offset = decode_13bit_imm(mcode);

	/* check for call with rs1 == REG_METHODPTR: ldx [g2+x],pv_caller */

	if (mcode_masked == 0x0602c5) {
		/* in this case we use the passed method pointer */

		/* return NULL if no mptr was specified (used for replacement) */

		if (mptr == NULL)
			return NULL;

		pa = mptr + offset;

	} else {
		/* in the normal case we check for a `ldx [i5+x],pv_caller' instruction */

		assert(mcode_masked  == 0x0602fb);

		/* and get the final data segment address */

		pa = sfi->pv + offset;
	}
	

	return pa;
}


/* md_cacheflush ***************************************************************

   Calls the system's function to flush the instruction and data
   cache.

*******************************************************************************/

void md_cacheflush(u1 *addr, s4 nbytes)
{
	/* don't know yet */	
}


/* md_dcacheflush **************************************************************

   Calls the system's function to flush the data cache.

*******************************************************************************/

void md_dcacheflush(u1 *addr, s4 nbytes)
{
	/* XXX don't know yet */	
}


/* md_patch_replacement_point **************************************************

   Patch the given replacement point.

*******************************************************************************/

#if defined(ENABLE_REPLACEMENT)
void md_patch_replacement_point(codeinfo *code, s4 index, rplpoint *rp, u1 *savedmcode)
{
	s4 disp;
	u4 mcode;
	
	assert(0);

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

		mcode = (((s4)(0x00))<<30) | ((0)<<29) | ((0x8)<<25) | (0x1<<22) | (0<<20)
			  | (1 << 19 ) | ((disp) & 0x007ffff);

		/* write the new machine code */
		*(u4*)(rp->pc) = (u4) mcode;
	}
	
#if !defined(NDEBUG) && defined(ENABLE_DISASSEMBLER)
	{
		u1* u1ptr = rp->pc;
		DISASSINSTR(u1ptr);
		fflush(stdout);
	}
#endif
			
	/* flush instruction cache */
    /* md_icacheflush(rp->pc,4); */
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

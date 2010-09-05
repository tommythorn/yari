/* src/vm/jit/m68k/codegen.h

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


#ifndef _CODEGEN_H
#define _CODEGEN_H

#include "config.h"

#include <stdint.h>

#include "md-abi.h"
#include "vm/jit/m68k/emit.h"


#define MCODECHECK(icnt) \
    do { \
        if ((cd->mcodeptr + (icnt) * 4) > cd->mcodeend) \
            codegen_increase(cd); \
    } while (0)


/* branch defines *************************************************************/

#define BRANCH_NOPS \
    do { \
        if (CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd)) { \
	    M_NOP; \
            M_NOP; \
            M_NOP; \
        } else { \
	    M_NOP; \
            M_NOP; \
        } \
    } while (0)


/* patcher defines ************************************************************/

#define PATCHER_NOPS \
	do { M_TPFL; M_TPF; M_TPF } while (0);


/* stub defines ***************************************************************/

#define COMPILERSTUB_CODESIZE (6+6+6+2)


/* coldfire instruction format:
 * -----------------------------
 * | Op Word                    |
 * |____________________________|
 * | extension word 1(optional) |
 * |____________________________|
 * | extension word 2(optional) |
 * |____________________________|
 *
 *
 * Opword:
 *
 *                     | effective addr
 *                     |  mode |  reg.
 * x x x x x x x x x x | M M M | R R R
 *
 * MMM = 000 ---> data register   direct: RRR = register
 * MMM = 001 ---> addr register   direct: RRR = register
 * MMM = 010 ---> addr register indirect: RRR = register
 * MMM = 011 ---> addr reg ind postincr : RRR = register
 * MMM = 100 ---> addr reg ind predecr  : RRR = register
 * MMM = 101 ---> addr reg ind + displac: RRR = register + ext.wrd.1
 * MMM = 110 ---> addr reg ind + scaled index and 8 bit displacement
 * MMM = 111, RRR = 010 ---> PC ind. with displacement + ext.wrd.1
 * MMM = 111, RRR = 011 ---> PC ind. with scaled inex + 8but displacement
 * MMM = 111, RRR = 000 + ext.wrd.1 ---> absolute short
 * MMM = 111, RRR = 001 + ext.wrd.1+2 -> absolute long
 * MMM = 111, RRR = 100 + ext.wrd1/2 --> immediate data
 */

/* one word opcodes */
#define OPWORD(op, mode, reg) \
	do { \
		*((u2*)cd->mcodeptr) = (u2) (((op) << 6) | ((mode)<<3) | (reg)); \
		cd->mcodeptr += 2; \
	} while(0);
/* opword + one extension word */
/* usage of int16_t instead of s2 as is clashes with variable name */
#define OPWORD_IMM16(op, mode, reg, imm) \
	do { \
		*((u2*)cd->mcodeptr) = (u2) (((op) << 6) | ((mode)<<3) | (reg)); \
		cd->mcodeptr += 2; \
		*((int16_t*)(cd->mcodeptr)) = imm; \
		cd->mcodeptr += 2; \
	} while(0);

/* opword + two extension words */
/* usage of int32_t instead of s4 as is clashes with variable name */
#define OPWORD_IMM32(op, mode, reg, imm) \
	do { \
		*((u2*)cd->mcodeptr) = (u2) (((op) << 6) | ((mode)<<3) | (reg)); \
		cd->mcodeptr += 2; \
		*((int32_t*)(cd->mcodeptr)) = imm; \
		cd->mcodeptr += 4; \
	} while(0);

/* create NOPS to align basicblock boundaries 
 * using real nops here as they are not executed, so no performance penalty
 **/
#define ALIGNCODENOP \
    do { \
        for (s1 = 0; s1 < (s4) (((ptrint) cd->mcodeptr) & 7); s1++) \
            M_NOP; \
    } while (0)



#define PATCHER_CALL_SIZE 	6

#define M_NOP		OPWORD(0x139,6,1)				/* 0x4371 do not use as it syncs pipeline */
#define M_ILLEGAL	OPWORD(0x12b,7,4)				/* 0x4afc */
#define M_TPF		OPWORD(0x147,7,4)				/* tfp with no  ext. word use instead of NOP*/
#define M_TPFW		OPWORD(0x147,7,2)				/* tfp with one ext. word */
#define M_TPFL		OPWORD(0x147,7,3)				/* tfp with two ext. word */

/*M_XMOVX....M_XMOVX(source, destination) */
#define M_IMOV(a,b)	OPWORD( ( (2<<6) | ((b) << 3) | 0), 0, (a))	/* move.l */
#define M_AMOV(a,b)	OPWORD( ( (2<<6) | ((b) << 3) | 1), 1, (a))	/* movea.l */
#define M_IMOV_IMM(a,b)	emit_mov_imm_reg(cd, (a), (b))
#define M_AMOV_IMM(a,b)	OPWORD_IMM32( ( (2<<6) | ((b) << 3) | 1), 7, 4, (a))

/* for sure generates a 32 bit immedeate form, needed when there are patchers involved */
#define M_IMOV_IMM32(a,b)	OPWORD_IMM32(((2<<6) | ((b) << 3) | 0), 7, 4, (a))

#define	M_ICLR(a)	OPWORD(0x10a, 0, (a))				/* clr.l */
#define M_ISET(a)	OPWORD( ( (0xa <<6) | (0 << 3) | 5), 0, (a))	/* mov3q #-1 */

#define M_JMP(a)	OPWORD(0x13b,2,(a))				/* jmp %aX@ */
#define M_JMP_IMM(a)	OPWORD_IMM32(0x13b,7,1,(a))			/* jmp.l */
#define M_JSR(a)	OPWORD(0x13a,2,(a))				/* jsr %aX@ */
#define M_JSR_IMM(a)	OPWORD_IMM32(0x13a,7,1,(a))			/* jsr.l */
#define M_BSR_IMM(a)	OPWORD_IMM32(0x187,7,7,(a))			/* bsr.l */
#define M_JSR_PCREL(a)	OPWORD_IMM16(0x13a,7,2,(a))			/* jsr.l (d16,PC) */

#define M_RET		OPWORD(0x139,6,5)				/* 0x4375 */
#define M_LINK(a,b)	OPWORD_IMM16(0x139,2,(a), (b))			/* link */
#define M_UNLK(a)	OPWORD(0x139, 3, (a))				/* unlk */

/* push and pop are implemented using move.l */
/* we need 3 variants, data, address and float registers */
/* also a POPALL and PUSHALL for verbose:call code, use them only there! */
#define M_IPUSH(a)		OPWORD(0xbc,0,(a))
#define M_APUSH(a)		OPWORD(0xbc,1,(a))

#define M_IPOP(a)		OPWORD( ( (2<<6) | ((a) << 3) | 0 ), 3, REG_SP)
#define M_APOP(a)		OPWORD( ( (2<<6) | ((a) << 3) | 1 ), 3, REG_SP)		/* movea.l acutally */


#define M_IPUSH_IMM(a)		OPWORD_IMM32(0x121,7,1, (a))				/* pea.l */

#define	M_PUSHALL		OPWORD_IMM16(0x123,2,REG_SP,0xFFFF)			/* A0-A7, D0-D7 pushed onto stack */
#define M_POPALL		OPWORD_IMM16(0x133,2,REG_SP,0xFFFF)			/* A0-A7, D0-D7 poped off stack */

/* M_XLD(a,b,c)....M_XLD(destinationreg, addressbase, offset)	*/
#define M_ILD(a,b,c)		OPWORD_IMM16( ( (2<<6) | ((a) << 3) | 0), 5, (b), (c))
#define M_ALD(a,b,c)		OPWORD_IMM16( ( (2<<6) | ((a) << 3) | 1), 5, (b), (c))
#define M_LLD(a,b,c)		do {\
					M_ILD(GET_HIGH_REG(a), (b), (c));\
					M_ILD(GET_LOW_REG (a), (b), (c)+4);\
				} while(0);

#if !defined(ENABLE_SOFTFLOAT)
	#define M_FLD(a,b,c)		OPWORD_IMM32( 0x3c8, 5, (b), ( (( (0x11 << 10) | ((a)<<7) | 0x40 )<<16) | (((int16_t)(c)) & 0x0000ffff)) )
	#define M_DLD(a,b,c)		OPWORD_IMM32( 0x3c8, 5, (b), ( (( (0x15 << 10) | ((a)<<7) | 0x44 )<<16) | (((int16_t)(c)) & 0x0000ffff)) )
#endif

/* M_XST(a,b,c)....M_XST(sourceregister, addressbase, offset)   */
#define M_IST(a,b,c)		OPWORD_IMM16( ( (2<<6) | ((b) << 3) | 5), 0, (a), (c))
#define M_AST(a,b,c)		OPWORD_IMM16( ( (2<<6) | ((b) << 3) | 5), 1, (a), (c))
#define M_LST(a,b,c)		do	{\
					M_IST(GET_HIGH_REG(a), (b), (c));\
					M_IST(GET_LOW_REG (a), (b), (c)+4);\
				} while(0);

#if !defined(ENABLE_SOFTFLOAT)
	#define M_FST(a,b,c)		OPWORD_IMM32( 0x3c8, 5, (b), ( ((  (0x19 <<10) | ((a)<<7) | 0  )<<16) | (((int16_t)(c)) & 0x0000ffff)) )
	#define M_DST(a,b,c)		OPWORD_IMM32( 0x3c8, 5, (b), ( ((  (0x1d <<10) | ((a)<<7) | 0  )<<16) | (((int16_t)(c)) & 0x0000ffff)) )
#endif

/*M_XADD_IMM(a,b)...M_XADD_IMM(offset, reg) */
#define M_AADD_IMM(a,b)		OPWORD_IMM32( ( (0xd<<6) | ((b)<<3) | 7), 7, 4, (a))
#define M_IADD_IMM(a,b)		OPWORD_IMM32( ( (0xd<<6) | ((b)<<3) | 2), 7, 4, (a))
#define M_ISUB_IMM(a,b)		M_IADD_IMM(-(a), (b))

/* M_OP(source, dest) ...  dest (OP) source -> dest*/
#define M_ISUB(a,b)		OPWORD ( ( (9<<6)   | ((b)<<3) | 2), 0, (a))			/* sub.l */
#define M_IADD(a,b)		OPWORD ( ( (0xd<<6) | ((b)<<3) | 2), 0, (a))			/* add.l */
#define M_IADDX(a,b)	OPWORD ( ( (0xd<<6) | ((b)<<3) | 6), 0, (a))			/* addx.l */

#define M_IMUL(a,b)		OPWORD_IMM16 ( 0x130, 0, (a), ( ((b) << 12) | (1 << 11))) 	/* muls.l */
#define M_IDIV(a,b)		OPWORD_IMM16 ( 0x131, 0, (a), ( ((b) << 12) | (1 << 11) | (b)))	/* divs.l */

#define M_ISSL(a,b)		OPWORD ( ( (0xe<<6) | ((a) << 3) | 6), 4, (b))			/* asl.l */
#define M_ISSR(a,b)		OPWORD ( ( (0xe<<6) | ((a) << 3) | 2), 4, (b))			/* asr.l */
#define M_IUSR(a,b)		OPWORD ( ( (0xe<<6) | ((a) << 3) | 2), 5, (b))			/* lsr.l */

#define M_IAND(a,b)		OPWORD ( ( (0xc<<6) | ((b) << 3) | 2), 0, (a))			/* and.l */
#define M_IOR(a,b)		OPWORD ( ( (0x8<<6) | ((b) << 3) | 2), 0, (a))			/* or.l */
#define M_IXOR(a,b)		OPWORD ( ( (0xb<<6) | ((a) << 3) | 6), 0, (b))			/* eor.l */


/* M_IX_IMM(imm, register)	*/
#define M_IAND_IMM(a,b)		OPWORD_IMM32( 0xa, 0, (b), (a))					/* andi.l # */
#define M_IOR_IMM(a,b)		OPWORD_IMM32( 0x2, 0, (b), (a))					/* ori.l # */
#define M_IXOR_IMM(a,b)		OPWORD_IMM32( 0x2a,0, (b), (a))					/* eori.l # */


/* ultra sepcial 3 register form, b%a = c, (a!=c) */
#define M_IREM(a,b,c)		OPWORD_IMM16 ( 0x131, 0, (a), ( ((b) << 12) | (1 << 11) | (c)))	/* rems.l */

/* M_OP(dest) */
#define M_INEG(a)		OPWORD(0x112, 0, (a))						/* neg.l */

/* only generate opcode when condition true */
#define OPWORD_COND(c, u,v,w)	\
	do { \
		if ( (c) ) { OPWORD( (u),(v),(w) ) }  \
	} while(0);
#define OPWORD_IMM16_COND(c, u,v,w,x)	\
	do { \
		if ( (c) ) { OPWORD_IMM16( (u),(v),(w),(x) ) }  \
	} while(0);
/* assert on the opcode */
#define OPWORD_ASSERT(a, u,v,w)	\
	do { \
		assert((a)); \
		OPWORD( (u),(v),(w) )  \
	} while(0);

/* M_XMOVE....M_XMOVE(sourcereg, destreg) */
#define M_INTMOVE(a,b)		OPWORD_COND(((a) != (b)), ( ( 2<<6) | ((b) << 3) | 0), 0, (a));
#define M_ADRMOVE(a,b)		OPWORD_COND(((a) != (b)), ( ( 2<<6) | ((b) << 3) | 1), 1, (a));
#define M_INT2ADRMOVE(a,b)	OPWORD( ( (2<<6) | ((b) << 3) | 1), 0, (a));
#define M_ADR2INTMOVE(a,b)	OPWORD( ( (2<<6) | ((b) << 3) | 0), 1, (a));
#define M_LNGMOVE(a,b)		do 	{\
					M_INTMOVE(GET_LOW_REG (a), GET_LOW_REG (b));\
					M_INTMOVE(GET_HIGH_REG(a), GET_HIGH_REG(b));\
				} while(0);

#if !defined(ENABLE_SOFTLFOAT)
	#define M_FLTMOVE(a,b)		OPWORD_IMM16_COND( ((a)!=(b)), 0x3c8, 0, 0, ((a)<<10) | ((b)<<7) | 0x40)
	#define M_INT2FLTMOVE(a,b)	OPWORD_IMM16( 0x3c8, 0, (a), ((0x11 << 10) | ((b) << 7) | 0x40 )) 
	#define M_DBLMOVE(a,b)		OPWORD_IMM16_COND( ((a)!=(b)), 0x3c8, 0, 0, ((a)<<10) | ((b)<<7) | 0x44)
#endif
/* M_XTST....M_XTST(register) */
#define M_ITST(a)		OPWORD(0x12a, 0, (a))			/* tst.l */
#define M_ATST(a)		OPWORD(0x12a, 1, (a))			/* tst.l */

/* M_XCMPI....M_XMCPI(immideate, register) */
#define M_ICMP_IMM(a,b)		OPWORD_IMM32( 0x32, 0, (b), (a))
#if 0
#define M_ACMPI(a,b)		OPWORD_IMM32( ( (0xb << 6) | ((b) << 3) | 7), 7, 4, (a))	/* cmpa.l # */
#endif
/* M_XCMP....M_XCMP(reg1, reg2) */
#define M_ICMP(b,a)		OPWORD( ( (0xb << 6) | ((a) << 3) | 2), 0, (b))			/* cmp.l */
#define M_ACMP(b,a)		OPWORD( ( (0xb << 6) | ((a) << 3) | 7), 1, (b))			/* cmpa.l */


/* All kind of branches one could ever possibly need, each with 16 and 32 bit displacement */
/* BRANCH16 and BRANCH32 are helpers */
#define BRANCH8(cond,imm) \
	do { \
		*((u2*)cd->mcodeptr) = (u2) (((0x6) << 12) | ((cond)<<8) | (int8_t)imm); \
		cd->mcodeptr += 2; \
	} while(0);

#define BRANCH16(cond,imm) \
	do { \
		*((u2*)cd->mcodeptr) = (u2) (((0x6) << 12) | ((cond)<<8) | 0x00); \
		cd->mcodeptr += 2; \
		*((int16_t*)(cd->mcodeptr)) = imm; \
		cd->mcodeptr += 2; \
	} while(0);
#define BRANCH32(cond,imm) \
	do { \
		*((u2*)cd->mcodeptr) = (u2) (((0x6) << 12) | ((cond)<<8) | 0xff); \
		cd->mcodeptr += 2; \
		*((int32_t*)(cd->mcodeptr)) = imm; \
		cd->mcodeptr += 4; \
	} while(0);

#define M_BR_16(a)			BRANCH16(0x0, (a))	/* branch always */
#define M_BR_32(a)			BRANCH32(0x0, (a))

#define M_BCS(a)			BRANCH8 (0x5, (a))	/* carry set */

#define M_BEQ(a)			BRANCH8 (0x7, (a))
#define M_BEQ_16(a)			BRANCH16(0x7, (a))
#define M_BEQ_32(a)			BRANCH32(0x7, (a))

#define M_BNE(a)			BRANCH8 (0x6, (a))
#define M_BNE_16(a)			BRANCH16(0x6, (a))
#define M_BNE_32(a)			BRANCH32(0x6, (a))

#define M_BLT(a)			BRANCH8 (0xd, (a))
#define M_BLT_16(a)			BRANCH16(0xd, (a))
#define M_BLT_32(a)			BRANCH32(0xd, (a))

#define M_BGE(a)			BRANCH8 (0xc, (a))
#define M_BGE_16(a)			BRANCH16(0xc, (a))
#define M_BGE_32(a)			BRANCH32(0xc, (a))

#define M_BGT(a)			BRANCH8 (0xe, (a))
#define M_BGT_16(a)			BRANCH16(0xe, (a))
#define M_BGT_32(a)			BRANCH32(0xe, (a))

#define M_BLE(a)			BRANCH8 (0xf, (a))
#define M_BLE_16(a)			BRANCH16(0xf, (a))
#define M_BLE_32(a)			BRANCH32(0xf, (a))

#define M_BHI(a)			BRANCH8 (0x2, (a))
#define M_BHI_16(a)			BRANCH16(0x2, (a))
#define M_BHI_32(a)			BRANCH32(0x2, (a))

#define M_BLS(a)			BRANCH8 (0x3, (a))

#define	M_BMI(a)			BRANCH8(0xb, (a))
#define M_BPL(a)			BRANCH8(0xa, (a))

#define M_BNAN_16(a)			M_ILLEGAL		/* TODO */
#define M_BNAN_32(a)			M_ILLEGAL



/* array store/load stuff */
/* M_LXXX(baseaddressregister, targetregister)	*/
/* M_SXXX(baseaddressregsiter, sourceregister)  */
#define M_LBZX(a,c)			OPWORD( ( (1<<6) | ((c) << 3) | 0), 2, (a))	/* move.l */
#define M_LHZX(a,c)			OPWORD( ( (3<<6) | ((c) << 3) | 0), 2, (a))
#define	M_LWZX(a,c)			OPWORD( ( (2<<6) | ((c) << 3) | 0), 2, (a))
#define M_LAX(a,c)			OPWORD( ( (2<<6) | ((c) << 3) | 1), 2, (a))	/* movea.l */

#define M_STBX(a,c)			OPWORD( ( (1<<6) | ((a) << 3) | 2), 0, (c))	/* move.l */
#define M_STHX(a,c)			OPWORD( ( (3<<6) | ((a) << 3) | 2), 0, (c))
#define M_STWX(a,c)			OPWORD( ( (2<<6) | ((a) << 3) | 2), 0, (c))
#define M_STAX(a,c)			OPWORD( ( (2<<6) | ((a) << 3) | 2), 1, (c))	/* movea.l */


#define M_BSEXT(a,b)			OPWORD( ( (7<<6) | ((b) << 3) | 4), 0, (a))	/* mvs.b */
#define M_CZEXT(a,b)			OPWORD( ( (7<<6) | ((b) << 3) | 7), 0, (a))	/* mvz.w */
#define M_SSEXT(a,b)			OPWORD( ( (7<<6) | ((b) << 3) | 5), 0, (a))	/* mvs.w */
#define M_HSEXT(a,b)			M_ILLEGAL

/* adds content of integer reg a to address register b, result is b */
#define M_AADDINT(a,b)			OPWORD( ( (0xd<<6) | ((b) << 3) | 7), 0, (a))	/* adda.l */
#define M_ASUBINT(a,b)			OPWORD( ( (0x9<<6) | ((b) << 3) | 7), 0, (a))   /* suba.l */

/* immideate a shift left int register b, immideate has 3 bits */
#define M_ISSL_IMM(a,b)			OPWORD_ASSERT( ((a)<=7), ( (0xe<<6) | ((a) << 3) | 6), 1, (b))	/* lsl */
#define M_ISSR_IMM(a,b)			OPWORD_ASSERT( ((a)<=7), ( (0xe<<6) | ((a) << 3) | 2), 0, (b))	/* lsl */
#define M_IUSR_IMM(a,b)			OPWORD_ASSERT( ((a)<=7), ( (0xe<<6) | ((a) << 3) | 2), 1, (b))	/* lsl */


/* constant handling */
/* XCONST(constant value, register) */
#define LCONST(a,b)			do {\
						M_IMOV_IMM(((uint32_t)(a)), GET_LOW_REG((b)));\
						M_IMOV_IMM(((uint32_t)(a>>32)), GET_HIGH_REG((b)));\
					} while(0);

#if !defined(ENABLE_SOFTFLOAT)
	#define FCONST(a,b)		do	{\
							M_IMOV_IMM((a), REG_ITMP1);\
							OPWORD_IMM16( 0x3c8, 0, REG_ITMP1, ( (0x11 << 10) | ((b)<<7) | 0x40) );\
						} while(0);
#endif

#define M_TRAP_SETREGISTER(a)		OPWORD( 0x128, 0, (a))		/* tst.b */
#define M_TRAP(a) \
	do { \
		*((u2*)cd->mcodeptr) = (u2) ( 0x4e40 | (a) ); \
		cd->mcodeptr += 2; \
	} while(0);


#if !defined(ENABLE_SOFTFLOAT)
	
	#define M_FCMP(b,a)		OPWORD_IMM16(0x3c8, 0, 0, ((a)<<10) | ((b)<<7) | 0x38 )			/* fcmp.d */

	#define	M_BFEQ(a)		OPWORD_IMM16(0x3ca, 0, 0x01, (a))
	#define	M_BFLT(a)		OPWORD_IMM16(0x3ca, 0, 0x14, (a))
	#define	M_BFGT(a)		OPWORD_IMM16(0x3ca, 0, 0x12, (a))
	#define M_BFUN(a)		OPWORD_IMM16(0x3ca, 0, 0x10, (a))

	#define	M_FADD(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x62 )		/* fsadd */
	#define	M_DADD(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x66 )		/* fdadd */

	#define	M_FSUB(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x68 )		/* fssub */
	#define	M_DSUB(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x6c )		/* fdsub */

	#define M_FMUL(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x63 )		/* fsmul */
	#define M_DMUL(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x67 )		/* fdmul */

	#define	M_FDIV(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x60 )		/* fsdiv */
	#define	M_DDIV(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x64 )		/* fddiv */

	#define M_D2F(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x40 )		/* fmove.s */
	#define M_F2D(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x44 )		/* fmove.d */

	#define	M_FNEG(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x5a )		/* fneg.s */
	#define	M_DNEG(a,b)		OPWORD_IMM16(0x3c8, 0, 0, ((a) << 10) | ((b) << 7) | 0x5e )		/* fneg.d */
#endif

#endif /* _CODEGEN_H */

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

/* src/vm/jit/arm/codegen.h - code generation macros and definitions for ARM

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

   $Id: codegen.h 7691 2007-04-12 12:45:10Z twisti $

*/


#ifndef _CODEGEN_H
#define _CODEGEN_H

#include "config.h"


/* helper macros for generating code ******************************************/

#if defined(__ARMEL__)
#define SPLIT_OPEN(type, reg, tmpreg) \
	if (IS_2_WORD_TYPE(type) && GET_HIGH_REG(reg)==REG_SPLIT) { \
		/*dolog("SPLIT_OPEN({R%d;SPL} > {R%d;R%d})", GET_LOW_REG(reg), GET_LOW_REG(reg), tmpreg);*/ \
		/*assert(GET_LOW_REG(reg) == 3);*/ \
		(reg) = PACK_REGS(GET_LOW_REG(reg), tmpreg); \
	}
#define SPLIT_LOAD(type, reg, offset) \
	if (IS_2_WORD_TYPE(type) && GET_LOW_REG(reg)==3) { \
		/*dolog("SPLIT_LOAD({R%d;R%d} from [%x])", GET_LOW_REG(reg), GET_HIGH_REG(reg), offset);*/ \
		M_LDR(GET_HIGH_REG(reg), REG_SP, 4 * (offset)); \
	}
#define SPLIT_STORE_AND_CLOSE(type, reg, offset) \
	if (IS_2_WORD_TYPE(type) && GET_LOW_REG(reg)==3) { \
		/*dolog("SPLIT_STORE({R%d;R%d} to [%x])", GET_LOW_REG(reg), GET_HIGH_REG(reg), offset);*/ \
		M_STR(GET_HIGH_REG(reg), REG_SP, 4 * (offset)); \
		(reg) = PACK_REGS(GET_LOW_REG(reg), REG_SPLIT); \
	}
#else /* defined(__ARMEB__) */
#define SPLIT_OPEN(type, reg, tmpreg) \
	if (IS_2_WORD_TYPE(type) && GET_LOW_REG(reg)==REG_SPLIT) { \
		/*dolog("SPLIT_OPEN({SPL;R%d} > {R%d;R%d})", GET_HIGH_REG(reg), tmpreg, GET_HIGH_REG(reg));*/ \
		/*assert(GET_HIGH_REG(reg) == 3);*/ \
		(reg) = PACK_REGS(tmpreg, GET_HIGH_REG(reg)); \
	}
#define SPLIT_LOAD(type, reg, offset) \
	if (IS_2_WORD_TYPE(type) && GET_HIGH_REG(reg)==3) { \
		/*dolog("SPLIT_LOAD({R%d;R%d} from [%x])", GET_LOW_REG(reg), GET_HIGH_REG(reg), offset);*/ \
		M_LDR(GET_LOW_REG(reg), REG_SP, 4 * (offset)); \
	}
#define SPLIT_STORE_AND_CLOSE(type, reg, offset) \
	if (IS_2_WORD_TYPE(type) && GET_HIGH_REG(reg)==3) { \
		/*dolog("SPLIT_STORE({R%d;R%d} to [%x])", GET_LOW_REG(reg), GET_HIGH_REG(reg), offset);*/ \
		M_STR(GET_LOW_REG(reg), REG_SP, 4 * (offset)); \
		(reg) = PACK_REGS(REG_SPLIT, GET_HIGH_REG(reg)); \
	}
#endif


#define MCODECHECK(icnt) \
    do { \
        if ((cd->mcodeptr + (icnt) * 4) > cd->mcodeend) \
            codegen_increase(cd); \
    } while (0)


/* TODO: correct this! */
#define IS_IMM(val) ( ((val) >= 0) && ((val) <= 255) )
#define IS_OFFSET(off,max) ((s4)(off) <= (max) && (s4)(off) >= -(max))

#if !defined(NDEBUG)
# define CHECK_INT_REG(r) if ((r)<0 || (r)>15) printf("CHECK_INT_REG: this is not an integer register: %d\n", r); assert((r)>=0 && (r)<=15)
# define CHECK_FLT_REG(r) if ((r)<0 || (r)>7) printf("CHECK_FLT_REG: this is not an float register: %d\n", r); assert((r)>=0 && (r)<=7)
# define CHECK_OFFSET(off,max) \
	if (!IS_OFFSET(off,max)) printf("CHECK_OFFSET: offset out of range: %x (>%x) SEVERE ERROR!!!\n", ((off)<0)?-(off):off, max); \
	assert(IS_OFFSET(off,max))
#else
# define CHECK_INT_REG(r)
# define CHECK_FLT_REG(r)
# define CHECK_OFFSET(off,max)
#endif


/* branch defines *************************************************************/

#define BRANCH_NOPS \
    do { \
        M_NOP; \
    } while (0)


/* patcher defines ************************************************************/

#define PATCHER_CALL_SIZE    1 * 4      /* an instruction is 4-bytes long     */

#define PATCHER_NOPS \
    do { \
        M_NOP; \
    } while (0)


/* stub defines ***************************************************************/

#define COMPILERSTUB_CODESIZE    2 * 4


/* lazy debugger **************************************************************/

#if !defined(NDEBUG)
void asm_debug(int a1, int a2, int a3, int a4);
void asm_debug_intern(int a1, int a2, int a3, int a4);

/* if called with this macros, it can be placed nearly anywhere */
/* almost all registers are saved and restored afterwards       */
/* it uses a long branch to call the asm_debug_intern (no exit) */
#define ASM_DEBUG_PREPARE \
	M_STMFD(0x7fff, REG_SP)
#define ASM_DEBUG_EXECUTE \
	M_LONGBRANCH(asm_debug_intern); \
	M_LDMFD(0x7fff, REG_SP)
#endif


/* macros to create code ******************************************************/

/* the condition field */
#define COND_EQ 0x0  /* Equal        Z set   */
#define COND_NE 0x1  /* Not equal    Z clear */
#define COND_CS 0x2  /* Carry set    C set   */
#define COND_CC 0x3  /* Carry clear  C clear */
#define COND_MI 0x4  /* Negative     N set   */
#define COND_PL 0x5  /* Positive     N clear */
#define COND_VS 0x6  /* Overflow     V set   */
#define COND_VC 0x7  /* No overflow  V clear */
#define COND_HI 0x8  /* Unsigned higher      */
#define COND_LS 0x9  /* Unsigned lower, same */
#define COND_GE 0xA  /* Sig. greater, equal  */
#define COND_LT 0xB  /* Sig. less than       */
#define COND_GT 0xC  /* Sig. greater than    */
#define COND_LE 0xD  /* Sig. less, equal     */
#define COND_AL 0xE  /* Always               */
#define CONDNV  0xF  /* Special (see A3-5)   */
#define UNCOND COND_AL

/* data processing operation: M_DAT
   cond ... conditional execution
   op ..... opcode
   d ...... destination reg
   n ...... source reg
   S ...... update condition codes
   I ...... switch to immediate mode
   shift .. shifter operand
*/

#define M_DAT(cond,op,d,n,S,I,shift) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | ((op) << 21) | ((d) << 12) | ((n) << 16) | ((I) << 25) | ((S) << 20) | ((shift) & 0x00000fff)); \
        cd->mcodeptr += 4; \
    } while (0)


/* load and store instruction: M_MEM
   cond ... conditional execution
   L ...... load (L=1) or store (L=0)
   B ...... unsigned byte (B=1) or word (B=0)
   d ...... destination reg
   n ...... base reg for addressing
   adr .... addressing mode specific
*/

#define M_MEM(cond,L,B,d,n,adr,I,P,U,W) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (1 << 26) | ((L) << 20) | ((B) << 22) | ((d) << 12) | ((n) << 16) | ((adr) & 0x0fff) | ((I) << 25) | ((P) << 24) | ((U) << 23) | ((W) << 21)); \
        cd->mcodeptr += 4; \
    } while (0)


/* load and store instruction: M_MEM2
   cond ... conditional execution
   L ...... load (L=1) or store (L=0)
   H ...... halfword (H=1) or signed byte (H=0)
   S ...... signed (S=1) or unsigned (S=0) halfword
   d ...... destination reg
   n ...... base reg for addressing
   adr .... addressing mode specific
*/

#define M_MEM2(cond,L,H,S,d,n,adr,I,P,U,W) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (1 << 22) | (0x9 << 4) | ((L) << 20) | ((H) << 5) | ((S) << 6) | ((d) << 12) | ((n) << 16) | ((adr) & 0x0f) | (((adr) & 0xf0) << (8-4)) | ((I) << 22) | ((P) << 24) | ((U) << 23) | ((W) << 21)); \
        cd->mcodeptr += 4; \
    } while (0)


/* load and store multiple instruction: M_MEM_MULTI
   cond ... conditional execution
   L ...... load (L=1) or store (L=0)
   S ...... special (see "The ARM ARM A3-21")
   regs ... register list
   n ...... base reg for addressing
*/

#define M_MEM_MULTI(cond,L,S,regs,n,P,U,W) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (1 << 27) | ((L) << 20) | ((S) << 22) | ((n) << 16) | ((regs) & 0xffff) | ((P) << 24) | ((U) << 23) | ((W) << 21)); \
        cd->mcodeptr += 4; \
    } while (0)


/* branch and branch with link: M_BRA
   cond ... conditional execution
   L ...... branch with link (L=1)
   offset . 24bit offset
*/

#define M_BRA(cond,L,offset) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (0x5 << 25) | ((L) << 24) | ((offset) & 0x00ffffff)); \
        cd->mcodeptr += 4; \
    } while (0)


/* multiplies: M_MULT
   cond ... conditional execution
   d ...... destination register
   n, m ... source registers
   S ...... update conditional codes
   A ...... accumulate flag (enables third source)
   s ...... third source register
*/

#define M_MULT(cond,d,n,m,S,A,s) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | ((d) << 16) | ((n) << 8) | (m) | (0x09 << 4) | ((S) << 20) | ((A) << 21) | ((s) << 12)); \
        cd->mcodeptr += 4; \
    } while (0)


/* no operation (mov r0,r0): M_NOP */

#define M_NOP \
    do { \
        *((u4 *) cd->mcodeptr) = (0xe1a00000); \
        cd->mcodeptr += 4; \
    } while (0)


/* software breakpoint (only v5 and above): M_BREAKPOINT */

#define M_BREAKPOINT(imm) \
    do { \
        *((u4 *) cd->mcodeptr) = (0x0e12 << 20) | (0x07 << 4) | (((imm) & 0xfff0) << (8-4)) | ((imm) & 0x0f); \
        cd->mcodeptr += 4; \
    } while (0)


/* undefined instruction used for hardware exceptions */

#define M_UNDEFINED(cond,imm,n) \
	do { \
		*((u4 *) cd->mcodeptr) = ((cond) << 28) | (0x7f << 20) | (((imm) & 0x0fff) << 8) | (0x0f << 4) | (n); \
		cd->mcodeptr += 4; \
	} while (0)


#if !defined(ENABLE_SOFTFLOAT)

/* M_CPDO **********************************************************************

   Floating-Point Coprocessor Data Operations

   cond ... conditional execution
   op ..... opcode
   D ...... dyadic (D=0) or monadic (D=1) instruction
   Fd ..... destination float-register
   Fn ..... source float-register
   Fm ..... source float-register or immediate

*******************************************************************************/

#define M_CPDOS(cond,op,D,Fd,Fn,Fm) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (0x0e << 24) | (1 << 8) | ((op) << 20) | ((D) << 15) | ((Fd) << 12) | ((Fn) << 16) | ((Fm) & 0x0f)); \
        cd->mcodeptr += 4; \
    } while (0)


#define M_CPDOD(cond,op,D,Fd,Fn,Fm) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (0x0e << 24) | (1 << 8) | ((op) << 20) | ((D) << 15) | ((Fd) << 12) | ((Fn) << 16) | ((Fm) & 0x0f) | (1 << 7)); \
        cd->mcodeptr += 4; \
    } while (0)


/* M_CPDT **********************************************************************

   Floating-Point Coprocessor Data Transfer

   cond ... conditional execution
   L ...... load (L=1) or store (L=0)
   Fd ..... destination float-register
   n ...... base reg for addressing

*******************************************************************************/

#define M_CPDT(cond,L,T1,T0,Fd,n,off,P,U,W) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (0x0c << 24) | (1 << 8) | ((L) << 20) | ((T1) << 22) | ((T0) << 15) | ((Fd) << 12) | ((n) << 16) | ((off) & 0xff) | ((P) << 24) | ((U) << 23) | ((W) << 21)); \
        cd->mcodeptr += 4; \
    } while (0)


/* M_CPRT **********************************************************************

   Floating-Point Coprocessor Register Transfer

   XXX

*******************************************************************************/

#define M_CPRTS(cond,L,d,Fn,Fm) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (0x0e << 24) | (1 << 8) | (1 << 4) | ((L) << 20) | ((d) << 12) | ((Fn) << 16) | (Fm)); \
        cd->mcodeptr += 4; \
    } while (0)


#define M_CPRTD(cond,L,d,Fn,Fm) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (0x0e << 24) | (1 << 8) | (1 << 4) | ((L) << 20) | ((d) << 12) | ((Fn) << 16) | (Fm) | (1 << 7)); \
        cd->mcodeptr += 4; \
    } while (0)


#define M_CPRTI(cond,L,d,Fn,Fm) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (0x0e << 24) | (1 << 8) | (1 << 4) | ((L) << 20) | ((d) << 12) | ((Fn) << 16) | (Fm) | (3 << 5)); \
        cd->mcodeptr += 4; \
    } while (0)


/* XXX TWISTI: replace X by something useful */

#define M_CPRTX(cond,L,d,Fn,Fm) \
    do { \
        *((u4 *) cd->mcodeptr) = (((cond) << 28) | (0x0e << 24) | (1 << 8) | (1 << 4) | ((L) << 20) | ((d) << 12) | ((Fn) << 16) | (Fm) | (1 << 23)); \
        cd->mcodeptr += 4; \
    } while (0)

#endif /* !defined(ENABLE_SOFTFLOAT) */


/* used to store values! */
#define DCD(val) \
    do { \
        *((u4 *) cd->mcodeptr) = val; \
        cd->mcodeptr += 4; \
    } while (0)


/* used to directly access shifter; insert this as shifter operand! */
#define REG_LSL(reg, shift) ( (((shift) & 0x1f) << 7) | ((reg) & 0x0f) )
#define REG_LSR(reg, shift) ( (((shift) & 0x1f) << 7) | ((reg) & 0x0f) | (1 << 5) )
#define REG_ASR(reg, shift) ( (((shift) & 0x1f) << 7) | ((reg) & 0x0f) | (1 << 6) )
#define REG_LSL_REG(reg, s) ( (((s) & 0x0f) << 8) | ((reg) & 0x0f) | (1 << 4) )
#define REG_LSR_REG(reg, s) ( (((s) & 0x0f) << 8) | ((reg) & 0x0f) | (1 << 4) | (1 << 5) )
#define REG_ASR_REG(reg, s) ( (((s) & 0x0f) << 8) | ((reg) & 0x0f) | (1 << 4) | (1 << 6) )

/* used to directly rotate immediate values; insert this as immediate! */
/* ATTENTION: this rotates the immediate right by (2 * rot) bits */
#define IMM_ROTR(imm, rot) ( ((imm) & 0xff) | (((rot) & 0x0f) << 8) )
#define IMM_ROTL(imm, rot) IMM_ROTR(imm, 16-(rot))

/* macros for all arm instructions ********************************************/

#define M_ADD(d,a,b)       M_DAT(UNCOND,0x04,d,a,0,0,b)         /* d = a +  b */
#define M_ADC(d,a,b)       M_DAT(UNCOND,0x05,d,a,0,0,b)         /* d = a +  b (with Carry) */
#define M_SUB(d,a,b)       M_DAT(UNCOND,0x02,d,a,0,0,b)         /* d = a -  b */
#define M_SBC(d,a,b)       M_DAT(UNCOND,0x06,d,a,0,0,b)         /* d = a -  b (with Carry) */
#define M_AND(a,b,d)       M_DAT(UNCOND,0x00,d,a,0,0,b)         /* d = a &  b */
#define M_ORR(a,b,d)       M_DAT(UNCOND,0x0c,d,a,0,0,b)         /* d = a |  b */
#define M_EOR(a,b,d)       M_DAT(UNCOND,0x01,d,a,0,0,b)         /* d = a ^  b */
#define M_TST(a,b)         M_DAT(UNCOND,0x08,0,a,1,0,b)         /* TST a &  b */
#define M_TEQ(a,b)         M_DAT(UNCOND,0x09,0,a,1,0,b)         /* TST a ^  b */
#define M_CMP(a,b)         M_DAT(UNCOND,0x0a,0,a,1,0,b)         /* TST a -  b */
#define M_MOV(d,b)         M_DAT(UNCOND,0x0d,d,0,0,0,b)         /* d =      b */
#define M_ADD_S(d,a,b)     M_DAT(UNCOND,0x04,d,a,1,0,b)         /* d = a +  b (update Flags) */
#define M_SUB_S(d,a,b)     M_DAT(UNCOND,0x02,d,a,1,0,b)         /* d = a -  b (update Flags) */
#define M_ORR_S(a,b,d)     M_DAT(UNCOND,0x0c,d,a,1,0,b)         /* d = a |  b (update flags) */
#define M_MOV_S(d,b)       M_DAT(UNCOND,0x0d,d,0,1,0,b)         /* d =      b (update Flags) */

#define M_ADD_IMM(d,a,i)   M_DAT(UNCOND,0x04,d,a,0,1,i)         /* d = a +  i */
#define M_ADC_IMM(d,a,i)   M_DAT(UNCOND,0x05,d,a,0,1,i)         /* d = a +  i (with Carry) */
#define M_SUB_IMM(d,a,i)   M_DAT(UNCOND,0x02,d,a,0,1,i)         /* d = a -  i */
#define M_SBC_IMM(d,a,i)   M_DAT(UNCOND,0x06,d,a,0,1,i)         /* d = a -  i (with Carry) */
#define M_RSB_IMM(d,a,i)   M_DAT(UNCOND,0x03,d,a,0,1,i)         /* d = -a + i */
#define M_RSC_IMM(d,a,i)   M_DAT(UNCOND,0x07,d,a,0,1,i)         /* d = -a + i (with Carry) */
#define M_AND_IMM(a,i,d)   M_DAT(UNCOND,0x00,d,a,0,1,i)         /* d = a &  i */
#define M_TST_IMM(a,i)     M_DAT(UNCOND,0x08,0,a,1,1,i)         /* TST a &  i */
#define M_TEQ_IMM(a,i)     M_DAT(UNCOND,0x09,0,a,1,1,i)         /* TST a ^  i */
#define M_CMP_IMM(a,i)     M_DAT(UNCOND,0x0a,0,a,1,1,i)         /* TST a -  i */
#define M_CMN_IMM(a,i)     M_DAT(UNCOND,0x0b,0,a,1,1,i)         /* TST a +  i */
#define M_MOV_IMM(d,i)     M_DAT(UNCOND,0x0d,d,0,0,1,i)         /* d =      i */
#define M_ADD_IMMS(d,a,i)  M_DAT(UNCOND,0x04,d,a,1,1,i)         /* d = a +  i (update Flags) */
#define M_SUB_IMMS(d,a,i)  M_DAT(UNCOND,0x02,d,a,1,1,i)         /* d = a -  i (update Flags) */
#define M_RSB_IMMS(d,a,i)  M_DAT(UNCOND,0x03,d,a,1,1,i)         /* d = -a + i (update Flags) */

#define M_ADDSUB_IMM(d,a,i) if((i)>=0) M_ADD_IMM(d,a,i); else M_SUB_IMM(d,a,-(i))
#define M_MOVEQ(a,d)       M_DAT(COND_EQ,0x0d,d,0,0,0,a)

#define M_MOVVS_IMM(i,d)   M_DAT(COND_VS,0x0d,d,0,0,1,i)
#define M_MOVEQ_IMM(i,d)   M_DAT(COND_EQ,0x0d,d,0,0,1,i)
#define M_MOVNE_IMM(i,d)   M_DAT(COND_NE,0x0d,d,0,0,1,i)
#define M_MOVLT_IMM(i,d)   M_DAT(COND_LT,0x0d,d,0,0,1,i)
#define M_MOVGT_IMM(i,d)   M_DAT(COND_GT,0x0d,d,0,0,1,i)
#define M_MOVLS_IMM(i,d)   M_DAT(COND_LS,0x0d,d,0,0,1,i)

#define M_ADDHI_IMM(d,a,i) M_DAT(COND_HI,0x04,d,a,0,1,i)
#define M_ADDLT_IMM(d,a,i) M_DAT(COND_LT,0x04,d,a,0,1,i)
#define M_ADDGT_IMM(d,a,i) M_DAT(COND_GT,0x04,d,a,0,1,i)
#define M_SUBLO_IMM(d,a,i) M_DAT(COND_CC,0x02,d,a,0,1,i)
#define M_SUBLT_IMM(d,a,i) M_DAT(COND_LT,0x02,d,a,0,1,i)
#define M_SUBGT_IMM(d,a,i) M_DAT(COND_GT,0x02,d,a,0,1,i)
#define M_RSBMI_IMM(d,a,i) M_DAT(COND_MI,0x03,d,a,0,1,i)
#define M_ADCMI_IMM(d,a,i) M_DAT(COND_MI,0x05,d,a,0,1,i)

#define M_CMPEQ(a,b)       M_DAT(COND_EQ,0x0a,0,a,1,0,b)        /* TST a -  b */
#define M_CMPLE(a,b)       M_DAT(COND_LE,0x0a,0,a,1,0,b)        /* TST a -  b */

#define M_CMPEQ_IMM(a,i)   M_DAT(COND_EQ,0x0a,0,a,1,1,i)

#define M_MUL(d,a,b)       M_MULT(UNCOND,d,a,b,0,0,0x0)         /* d = a *  b */


#define M_LDMFD(regs,base) M_MEM_MULTI(UNCOND,1,0,regs,base,0,1,1)
#define M_STMFD(regs,base) M_MEM_MULTI(UNCOND,0,0,regs,base,1,0,1)

#define M_LDR_INTERN(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x0fff); \
        M_MEM(UNCOND,1,0,d,base,(((off) < 0) ? -(off) : off),0,1,(((off) < 0) ? 0 : 1),0); \
    } while (0)

#define M_STR_INTERN(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x0fff); \
        M_MEM(UNCOND,0,0,d,base,(((off) < 0) ? -(off) : off),0,1,(((off) < 0) ? 0 : 1),0); \
    } while (0)

#define M_LDR_UPDATE(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x0fff); \
        M_MEM(UNCOND,1,0,d,base,(((off) < 0) ? -(off) : off),0,0,(((off) < 0) ? 0 : 1),0); \
    } while (0)

#define M_STR_UPDATE(d,base,off) \
    do { \
        CHECK_OFFSET(off,0x0fff); \
        M_MEM(UNCOND,0,0,d,base,(((off) < 0) ? -(off) : off),0,1,(((off) < 0) ? 0 : 1),1); \
    } while (0)


#define M_LDRH(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x00ff); \
        assert(off >= 0); \
        M_MEM2(UNCOND,1,1,0,d,base,off,1,1,1,0); \
    } while (0)

#define M_LDRSH(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x00ff); \
        assert(off >= 0); \
        M_MEM2(UNCOND,1,1,1,d,base,off,1,1,1,0); \
    } while (0)

#define M_LDRSB(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x00ff); \
        assert(off >= 0); \
        M_MEM2(UNCOND,1,0,1,d,base,off,1,1,1,0); \
    } while (0)

#define M_STRH(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x00ff); \
        assert(off >= 0); \
        M_MEM2(UNCOND,0,1,0,d,base,off,1,1,1,0); \
    } while (0)

#define M_STRB(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x0fff); \
        assert(off >= 0); \
        M_MEM(UNCOND,0,1,d,base,off,0,1,1,0); \
    } while (0)

												      
#if !defined(ENABLE_SOFTFLOAT)

#define M_LDFS_INTERN(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x03ff); \
        M_CPDT(UNCOND,1,0,0,d,base,(((off) < 0) ? -(off) >> 2 : (off) >> 2),1,(((off) < 0) ? 0 : 1),0); \
    } while (0)

#define M_LDFD_INTERN(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x03ff); \
        M_CPDT(UNCOND,1,0,1,d,base,(((off) < 0) ? -(off) >> 2 : (off) >> 2),1,(((off) < 0) ? 0 : 1),0); \
    } while (0)

#define M_STFS_INTERN(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x03ff); \
        M_CPDT(UNCOND,0,0,0,d,base,(((off) < 0) ? -(off) >> 2 : (off) >> 2),1,(((off) < 0) ? 0 : 1),0); \
    } while (0)

#define M_STFD_INTERN(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x03ff); \
        M_CPDT(UNCOND,0,0,1,d,base,(((off) < 0) ? -(off) >> 2 : (off) >> 2),1,(((off) < 0) ? 0 : 1),0); \
    } while (0)

#define M_LDFS_UPDATE(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x03ff); \
        M_CPDT(UNCOND,1,0,0,d,base,(((off) < 0) ? -(off) >> 2 : (off) >> 2),0,(((off) < 0) ? 0 : 1),1); \
    } while (0)

#define M_LDFD_UPDATE(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x03ff); \
        M_CPDT(UNCOND,1,0,1,d,base,(((off) < 0) ? -(off) >> 2 : (off) >> 2),0,(((off) < 0) ? 0 : 1),1); \
    } while (0)

#define M_STFS_UPDATE(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x03ff); \
        M_CPDT(UNCOND,0,0,0,d,base,(((off) < 0) ? -(off) >> 2 : (off) >> 2),1,(((off) < 0) ? 0 : 1),1); \
    } while (0)

#define M_STFD_UPDATE(d,base,off) \
    do { \
        CHECK_OFFSET(off, 0x03ff); \
        M_CPDT(UNCOND,0,0,1,d,base,(((off) < 0) ? -(off) >> 2 : (off) >> 2),1,(((off) < 0) ? 0 : 1),1); \
    } while (0)

#define M_ADFS(d,a,b)      M_CPDOS(UNCOND,0x00,0,d,a,b)         /* d = a +  b */
#define M_SUFS(d,a,b)      M_CPDOS(UNCOND,0x02,0,d,a,b)         /* d = a -  b */
#define M_RSFS(d,a,b)      M_CPDOS(UNCOND,0x03,0,d,a,b)         /* d = b -  a */
#define M_MUFS(d,a,b)      M_CPDOS(UNCOND,0x01,0,d,a,b)         /* d = a *  b */
#define M_DVFS(d,a,b)      M_CPDOS(UNCOND,0x04,0,d,a,b)         /* d = a /  b */
#define M_RMFS(d,a,b)      M_CPDOS(UNCOND,0x08,0,d,a,b)         /* d = a %  b */
#define M_ADFD(d,a,b)      M_CPDOD(UNCOND,0x00,0,d,a,b)         /* d = a +  b */
#define M_SUFD(d,a,b)      M_CPDOD(UNCOND,0x02,0,d,a,b)         /* d = a -  b */
#define M_RSFD(d,a,b)      M_CPDOD(UNCOND,0x03,0,d,a,b)         /* d = b -  a */
#define M_MUFD(d,a,b)      M_CPDOD(UNCOND,0x01,0,d,a,b)         /* d = a *  b */
#define M_DVFD(d,a,b)      M_CPDOD(UNCOND,0x04,0,d,a,b)         /* d = a /  b */
#define M_RMFD(d,a,b)      M_CPDOD(UNCOND,0x08,0,d,a,b)         /* d = a %  b */
#define M_MVFS(d,a)        M_CPDOS(UNCOND,0x00,1,d,0,a)         /* d =      a */
#define M_MVFD(d,a)        M_CPDOD(UNCOND,0x00,1,d,0,a)         /* d =      a */
#define M_MNFS(d,a)        M_CPDOS(UNCOND,0x01,1,d,0,a)         /* d =    - a */
#define M_MNFD(d,a)        M_CPDOD(UNCOND,0x01,1,d,0,a)         /* d =    - a */
#define M_CMF(a,b)         M_CPRTX(UNCOND,1,0x0f,a,b)           /* COMPARE a;  b */
#define M_FLTS(d,a)        M_CPRTS(UNCOND,0,a,d,0)              /* d = (float) a */
#define M_FLTD(d,a)        M_CPRTD(UNCOND,0,a,d,0)              /* d = (float) a */
#define M_FIX(d,a)         M_CPRTI(UNCOND,1,d,0,a)              /* d = (int)   a */

#endif /* !defined(ENABLE_SOFTFLOAT) */


#define M_B(off)           M_BRA(UNCOND,0,off)    /* unconditional branch */
#define M_BL(off)          M_BRA(UNCOND,1,off)    /* branch and link      */
#define M_BEQ(off)         M_BRA(COND_EQ,0,off)   /* conditional branches */
#define M_BNE(off)         M_BRA(COND_NE,0,off)
#define M_BGE(off)         M_BRA(COND_GE,0,off)
#define M_BGT(off)         M_BRA(COND_GT,0,off)
#define M_BLT(off)         M_BRA(COND_LT,0,off)
#define M_BLE(off)         M_BRA(COND_LE,0,off)
#define M_BHI(off)         M_BRA(COND_HI,0,off)   /* unsigned conditional */
#define M_BHS(off)         M_BRA(COND_CS,0,off)
#define M_BLO(off)         M_BRA(COND_CC,0,off)
#define M_BLS(off)         M_BRA(COND_LS,0,off)


#define M_FMOV(a,b)        M_MVFS(b,a)
#define M_DMOV(a,b)        M_MVFD(b,a)


#define M_TRAPEQ(a,i)      M_UNDEFINED(COND_EQ,i,a);
#define M_TRAPLE(a,i)      M_UNDEFINED(COND_LE,i,a);
#define M_TRAPHI(a,i)      M_UNDEFINED(COND_HI,i,a);
#define M_TRAPHS(a,i)      M_UNDEFINED(COND_CS,i,a);


/* if we do not have double-word load/store command, we can fake them */
/* ATTENTION: the original LDRD/STRD of ARMv5e would always use (Rd/Rd+1),
   so these faked versions are more "powerful" */

#if defined(__ARMEL__)

#define M_LDRD_INTERN(d,base,off) \
    do { \
        M_LDR_INTERN(GET_LOW_REG(d), base, off); \
        M_LDR_INTERN(GET_HIGH_REG(d), base, (off) + 4); \
    } while (0)

#define M_STRD_INTERN(d,base,off) \
    do { \
        M_STR_INTERN(GET_LOW_REG(d), base, off); \
        M_STR_INTERN(GET_HIGH_REG(d), base, (off) + 4); \
    } while (0)

#define M_LDRD_ALTERN(d,base,off) \
    do { \
        M_LDR_INTERN(GET_HIGH_REG(d), base, (off) + 4); \
        M_LDR_INTERN(GET_LOW_REG(d), base, off); \
    } while (0)

#define M_LDRD_UPDATE(d,base,off) \
    do { \
        assert((off) == +8); \
        M_LDR_UPDATE(GET_LOW_REG(d), base, 4); \
        M_LDR_UPDATE(GET_HIGH_REG(d), base, 4); \
    } while (0)

#define M_STRD_UPDATE(d,base,off) \
    do { \
        assert((off) == -8); \
        M_STR_UPDATE(GET_HIGH_REG(d), base, -4); \
        M_STR_UPDATE(GET_LOW_REG(d), base, -4); \
    } while (0)

#define GET_FIRST_REG(d)  GET_LOW_REG(d)
#define GET_SECOND_REG(d) GET_HIGH_REG(d)

#else /* defined(__ARMEB__) */

#define M_LDRD_INTERN(d,base,off) \
    do { \
        M_LDR_INTERN(GET_HIGH_REG(d), base, off); \
        M_LDR_INTERN(GET_LOW_REG(d), base, (off) + 4); \
    } while (0)

#define M_STRD_INTERN(d,base,off) \
    do { \
        M_STR_INTERN(GET_HIGH_REG(d), base, off); \
        M_STR_INTERN(GET_LOW_REG(d), base, (off) + 4); \
    } while (0)

#define M_LDRD_ALTERN(d,base,off) \
    do { \
        M_LDR_INTERN(GET_LOW_REG(d), base, (off) + 4); \
        M_LDR_INTERN(GET_HIGH_REG(d), base, off); \
    } while (0)

#define M_LDRD_UPDATE(d,base,off) \
    do { \
        assert((off) == +8); \
        M_LDR_UPDATE(GET_HIGH_REG(d), base, 4); \
        M_LDR_UPDATE(GET_LOW_REG(d), base, 4); \
    } while (0)

#define M_STRD_UPDATE(d,base,off) \
    do { \
        assert((off) == -8); \
        M_STR_UPDATE(GET_LOW_REG(d), base, -4); \
        M_STR_UPDATE(GET_HIGH_REG(d) ,base, -4); \
    } while (0)

#define GET_FIRST_REG(d)  GET_HIGH_REG(d)
#define GET_SECOND_REG(d) GET_LOW_REG(d)

#endif /* defined(__ARMEB__) */


/* M_LDR/M_STR:
   these are replacements for the original LDR/STR instructions, which can
   handle longer offsets (up to 20bits). the original functions are now
   called M_xxx_INTERN.
*/
/* ATTENTION: We use ITMP3 here, take into account that it gets destroyed.
   This means that only ITMP1 and ITMP2 can be used in reg_of_var()!!!
*/
/* ATTENTION2: It is possible to use ITMP3 as base reg. Remember that when
   changing these macros!!!
*/

#define M_LDR(d, base, offset) \
do { \
	CHECK_OFFSET(offset, 0x0fffff); \
	if (IS_OFFSET(offset, 0x000fff)) { \
		M_LDR_INTERN(d, base, offset); \
	} else { \
		/* we cannot handle REG_PC here */ \
		assert((d) != REG_PC); \
		if ((offset) > 0) { \
			M_ADD_IMM(d, base, IMM_ROTL((offset) >> 12, 6)); \
			M_LDR_INTERN(d, d, (offset) & 0x000fff); \
		} else { \
			M_SUB_IMM(d, base, IMM_ROTL((-(offset)) >> 12, 6)); \
			M_LDR_INTERN(d, d, -(-(offset) & 0x000fff)); \
		} \
	} \
} while (0)

#define M_LDR_NEGATIVE(d, base, offset) { \
	/*assert((offset) <= 0);*/ \
	if (IS_OFFSET(offset, 0x000fff)) { \
		M_LDR_INTERN(d, base, offset); \
	} else { \
		/* we cannot handle REG_PC here */ \
		assert((d) != REG_PC); \
		M_SUB_IMM(d, base, IMM_ROTL((-(offset)) >> 12, 6)); \
		M_LDR_INTERN(d, d, -(-(offset) & 0x000fff)); \
	} \
}

#define M_LDRD(d, base, offset) \
do { \
	CHECK_OFFSET(offset, 0x0fffff - 4); \
	if (IS_OFFSET(offset, 0x000fff - 4)) { \
		if (GET_FIRST_REG(d) != (base)) { \
			M_LDRD_INTERN(d, base, offset); \
		} else { \
			M_LDRD_ALTERN(d, base, offset); \
		} \
	} else if (IS_OFFSET(offset, 0x000fff)) { \
		dolog("M_LDRD: this offset seems to be complicated (%d)", offset); \
		assert(0); \
	} else { \
		if ((offset) > 0) { \
			M_ADD_IMM(GET_SECOND_REG(d), base, IMM_ROTL((offset) >> 12, 6)); \
			M_LDRD_INTERN(d, GET_SECOND_REG(d), (offset) & 0x000fff); \
		} else { \
			M_SUB_IMM(GET_SECOND_REG(d), base, IMM_ROTL((-(offset)) >> 12, 6)); \
			M_LDRD_INTERN(d, GET_SECOND_REG(d), -(-(offset) & 0x000fff)); \
		} \
	} \
} while (0)

#if !defined(ENABLE_SOFTFLOAT)
#define M_LDFS(d, base, offset) \
do { \
	CHECK_OFFSET(offset, 0x03ffff); \
	if (IS_OFFSET(offset, 0x03ff)) { \
		M_LDFS_INTERN(d, base, offset); \
	} else { \
		if ((offset) > 0) { \
			M_ADD_IMM(REG_ITMP3, base, IMM_ROTL((offset) >> 10, 5)); \
			M_LDFS_INTERN(d, REG_ITMP3, (offset) & 0x03ff); \
		} else { \
			M_SUB_IMM(REG_ITMP3, base, IMM_ROTL((-(offset)) >> 10, 5)); \
			M_LDFS_INTERN(d, REG_ITMP3, -(-(offset) & 0x03ff)); \
		} \
	} \
} while (0)

#define M_LDFD(d, base, offset) \
do { \
	CHECK_OFFSET(offset, 0x03ffff); \
	if (IS_OFFSET(offset, 0x03ff)) { \
		M_LDFD_INTERN(d, base, offset); \
	} else { \
		if ((offset) > 0) { \
			M_ADD_IMM(REG_ITMP3, base, IMM_ROTL((offset) >> 10, 5)); \
			M_LDFD_INTERN(d, REG_ITMP3, (offset) & 0x03ff); \
		} else { \
			M_SUB_IMM(REG_ITMP3, base, IMM_ROTL((-(offset)) >> 10, 5)); \
			M_LDFD_INTERN(d, REG_ITMP3, -(-(offset) & 0x03ff)); \
		} \
	} \
} while (0)

#endif /* !defined(ENABLE_SOFTFLOAT) */

#define M_STR(d, base, offset) \
do { \
	assert((d) != REG_ITMP3); \
	CHECK_OFFSET(offset, 0x0fffff); \
	if (IS_OFFSET(offset, 0x000fff)) { \
		M_STR_INTERN(d, base, offset); \
	} else { \
		if ((offset) > 0) { \
			M_ADD_IMM(REG_ITMP3, base, IMM_ROTL((offset) >> 12, 6)); \
			M_STR_INTERN(d, REG_ITMP3, (offset) & 0x000fff); \
		} else { \
			M_SUB_IMM(REG_ITMP3, base, IMM_ROTL((-(offset)) >> 12, 6)); \
			M_STR_INTERN(d, REG_ITMP3, -(-(offset) & 0x000fff)); \
		} \
	} \
} while (0)

#define M_STRD(d, base, offset) \
do { \
	assert(GET_LOW_REG(d) != REG_ITMP3); \
	assert(GET_HIGH_REG(d) != REG_ITMP3); \
	CHECK_OFFSET(offset, 0x0fffff - 4); \
	if (IS_OFFSET(offset, 0x000fff - 4)) { \
		M_STRD_INTERN(d,base,offset); \
	} else if (IS_OFFSET(offset, 0x000fff)) { \
		dolog("M_STRD: this offset seems to be complicated (%d)", offset); \
		assert(0); \
	} else { \
		if ((offset) > 0) { \
			M_ADD_IMM(REG_ITMP3, base, IMM_ROTL((offset) >> 12, 6)); \
			M_STRD_INTERN(d, REG_ITMP3, (offset) & 0x000fff); \
		} else { \
			M_SUB_IMM(REG_ITMP3, base, IMM_ROTL((-(offset)) >> 12, 6)); \
			M_STRD_INTERN(d, REG_ITMP3, -(-(offset) & 0x000fff)); \
		} \
	} \
} while (0)

#if !defined(ENABLE_SOFTFLOAT)

#define M_STFS(d, base, offset) \
do { \
	CHECK_OFFSET(offset, 0x03ffff); \
	if (IS_OFFSET(offset, 0x03ff)) { \
		M_STFS_INTERN(d, base, offset); \
	} else { \
		if ((offset) > 0) { \
			M_ADD_IMM(REG_ITMP3, base, IMM_ROTL((offset) >> 10, 5)); \
			M_STFS_INTERN(d, REG_ITMP3, (offset) & 0x03ff); \
		} else { \
			M_SUB_IMM(REG_ITMP3, base, IMM_ROTL((-(offset)) >> 10, 5)); \
			M_STFS_INTERN(d, REG_ITMP3, -(-(offset) & 0x03ff)); \
		} \
	} \
} while (0)

#define M_STFD(d, base, offset) \
do { \
	CHECK_OFFSET(offset, 0x03ffff); \
	if (IS_OFFSET(offset, 0x03ff)) { \
		M_STFD_INTERN(d, base, offset); \
	} else { \
		if ((offset) > 0) { \
			M_ADD_IMM(REG_ITMP3, base, IMM_ROTL((offset) >> 10, 5)); \
			M_STFD_INTERN(d, REG_ITMP3, (offset) & 0x03ff); \
		} else { \
			M_SUB_IMM(REG_ITMP3, base, IMM_ROTL((-(offset)) >> 10, 5)); \
			M_STFD_INTERN(d, REG_ITMP3, -(-(offset) & 0x03ff)); \
		} \
	} \
} while (0)

#endif /* !defined(ENABLE_SOFTFLOAT) */

/* M_???_IMM_EXT_MUL4:
   extended immediate operations, to handle immediates lager than 8bit.
   ATTENTION: the immediate is rotatet left by 2 (multiplied by 4)!!!
*/

#define M_ADD_IMM_EXT_MUL4(d,n,imm) \
    do { \
        assert(d != REG_PC); \
        assert((imm) >= 0 && (imm) <= 0x00ffffff); \
        M_ADD_IMM(d, n, IMM_ROTL(imm, 1)); \
        if ((imm) > 0x000000ff) M_ADD_IMM(d, d, IMM_ROTL((imm) >>  8, 5)); \
        if ((imm) > 0x0000ffff) M_ADD_IMM(d, d, IMM_ROTL((imm) >> 16, 9)); \
    } while (0)

#define M_SUB_IMM_EXT_MUL4(d,n,imm) \
    do { \
        assert(d != REG_PC); \
        assert((imm) >= 0 && (imm) <= 0x00ffffff); \
        M_SUB_IMM(d, n, IMM_ROTL(imm, 1)); \
        if ((imm) > 0x000000ff) M_SUB_IMM(d, d, IMM_ROTL((imm) >>  8, 5)); \
        if ((imm) > 0x0000ffff) M_SUB_IMM(d, d, IMM_ROTL((imm) >> 16, 9)); \
    } while (0)


/* ICONST/LCONST:
   loads the integer/long value const into register d.
*/

#define ICONST(d,c)                     emit_iconst(cd, (d), (c))

#define ICONST_CONDITIONAL(cond,d,const) \
	if (IS_IMM(const)) { \
		/* M_MOV_IMM */ M_DAT(cond,0x0d,d,0,0,1,const); \
	} else { \
		disp = dseg_adds4(cd, const); \
		/* TODO: implement this using M_DSEG_LOAD!!! */ \
		/* M_LDR_INTERN */ CHECK_OFFSET(disp,0x0fff); M_MEM(cond,1,0,d,REG_PV,(disp<0)?-disp:disp,0,1,(disp<0)?0:1,0); \
	}

#define LCONST(d,c) \
	if (IS_IMM((c) >> 32)) { \
		M_MOV_IMM(GET_HIGH_REG(d), (s4) ((s8) (c) >> 32)); \
		ICONST(GET_LOW_REG(d), (s4) ((s8) (c) & 0xffffffff)); \
	} else if (IS_IMM((c) & 0xffffffff)) { \
		M_MOV_IMM(GET_LOW_REG(d), (s4) ((s8) (c) & 0xffffffff)); \
		ICONST(GET_HIGH_REG(d), (s4) ((s8) (c) >> 32)); \
	} else { \
		disp = dseg_add_s8(cd, (c)); \
		M_LDRD(d, REG_PV, disp); \
	}


#if !defined(ENABLE_SOFTFLOAT)

#define FCONST(d,c) \
    do { \
        disp = dseg_add_float(cd, (c)); \
        M_LDFS(d, REG_PV, disp); \
    } while (0)

#define DCONST(d,c) \
    do { \
        disp = dseg_add_double(cd, (c)); \
        M_LDFD(d, REG_PV, disp); \
    } while (0)

#endif /* !defined(ENABLE_SOFTFLOAT) */


/* M_RECOMPUTE_PV:
   used to recompute our PV (we use the IP for this) out of the current PC
   ATTENTION: if you change this, you have to look at other functions as well!
   Following things depend on it: asm_call_jit_compiler(); codegen_findmethod();
*/
#define M_RECOMPUTE_PV(disp) \
	disp += 8; /* we use PC relative addr.  */ \
	assert((disp & 0x03) == 0); \
	assert(disp >= 0 && disp <= 0x03ffffff); \
	M_SUB_IMM(REG_PV, REG_PC, IMM_ROTL(disp >> 2, 1)); \
	if (disp > 0x000003ff) M_SUB_IMM(REG_PV, REG_PV, IMM_ROTL(disp >> 10, 5)); \
	if (disp > 0x0003ffff) M_SUB_IMM(REG_PV, REG_PV, IMM_ROTL(disp >> 18, 9)); \

/* M_INTMOVE:
   generates an integer-move from register a to b.
   if a and b are the same int-register, no code will be generated.
*/

#define M_INTMOVE(a,b) \
    do { \
        if ((a) != (b)) \
            M_MOV(b, a); \
    } while (0)

#define M_LNGMOVE(a,b) \
    do { \
        if (GET_HIGH_REG(a) == GET_LOW_REG(b)) { \
            assert((GET_LOW_REG(a) != GET_HIGH_REG(b))); \
            M_INTMOVE(GET_HIGH_REG(a), GET_HIGH_REG(b)); \
            M_INTMOVE(GET_LOW_REG(a), GET_LOW_REG(b)); \
        } else { \
            M_INTMOVE(GET_LOW_REG(a), GET_LOW_REG(b)); \
            M_INTMOVE(GET_HIGH_REG(a), GET_HIGH_REG(b)); \
        } \
    } while (0)


#if !defined(ENABLE_SOFTFLOAT)

/* M_FLTMOVE:
   generates a floating-point-move from register a to b.
   if a and b are the same float-register, no code will be generated.
*/

#define M_FLTMOVE(a,b) \
    do { \
        if ((a) != (b)) \
            M_FMOV(a, b); \
    } while (0)

#define M_DBLMOVE(a,b) \
    do { \
        if ((a) != (b)) \
            M_DMOV(a, b); \
    } while (0)

#endif

#if !defined(ENABLE_SOFTFLOAT)
/* M_CAST_INT_TO_FLT_TYPED:
   loads the value of the integer-register a (argument or result) into
   float-register Fb. (and vice versa)
*/
#define M_CAST_INT_TO_FLT_TYPED(t,a,Fb) \
	CHECK_FLT_REG(Fb); \
	if ((t) == TYPE_FLT) { \
		CHECK_INT_REG(a); \
		M_STR_UPDATE(a, REG_SP, -4); \
		M_LDFS_UPDATE(Fb, REG_SP, 4); \
	} else { \
		CHECK_INT_REG(GET_LOW_REG(a)); \
		CHECK_INT_REG(GET_HIGH_REG(a)); \
		M_STRD_UPDATE(a, REG_SP, -8); \
		M_LDFD_UPDATE(Fb, REG_SP, 8); \
	}
#define M_CAST_FLT_TO_INT_TYPED(t,Fa,b) \
	CHECK_FLT_REG(Fa); \
	if ((t) == TYPE_FLT) { \
		CHECK_INT_REG(b); \
		M_STFS_UPDATE(Fa, REG_SP, -4); \
		M_LDR_UPDATE(b, REG_SP, 4); \
	} else { \
		CHECK_INT_REG(GET_LOW_REG(b)); \
		CHECK_INT_REG(GET_HIGH_REG(b)); \
		M_STFD_UPDATE(Fa, REG_SP, -8); \
		M_LDRD_UPDATE(b, REG_SP, 8); \
	}
#endif /* !defined(ENABLE_SOFTFLOAT) */


/* M_COMPARE:
   generates the compare part of an if-sequece
   uses M_CMP or M_CMP_IMM to do the compare
   ATTENTION: uses REG_ITMP3 as intermediate register
*/
#define M_COMPARE(reg, val) \
	if (IS_IMM(val)) { \
		M_CMP_IMM(reg, (val)); \
	} else if(IS_IMM(-(val))) { \
		M_CMN_IMM(reg, -(val)); \
	} else { \
		ICONST(REG_ITMP3, (val)); \
		M_CMP(reg, REG_ITMP3); \
	}

/* M_LONGBRANCH:
   performs a long branch to an absolute address with return address in LR
   takes up 3 bytes of code space; address is hard-coded into code
*/
#define M_LONGBRANCH(adr) \
	M_ADD_IMM(REG_LR, REG_PC, 4); \
	M_LDR_INTERN(REG_PC, REG_PC, -4); \
	DCD((s4) adr);

/* M_DSEG_LOAD/BRANCH:
   TODO: document me
   ATTENTION: if you change this, you have to look at the asm_call_jit_compiler!
   ATTENTION: we use M_LDR, so the same restrictions apply to us!
*/
#define M_DSEG_LOAD(reg, offset) \
	M_LDR_NEGATIVE(reg, REG_PV, offset)

#define M_DSEG_BRANCH(offset) \
	if (IS_OFFSET(offset, 0x0fff)) { \
		M_MOV(REG_LR, REG_PC); \
		M_LDR_INTERN(REG_PC, REG_PV, offset); \
	} else { \
		/*assert((offset) <= 0);*/ \
		CHECK_OFFSET(offset,0x0fffff); \
		M_SUB_IMM(REG_ITMP3, REG_PV, ((-(offset) >>  12) & 0xff) | (((10) & 0x0f) << 8)); /*TODO: more to go*/ \
		M_MOV(REG_LR, REG_PC); \
		M_LDR_INTERN(REG_PC, REG_ITMP3, -(-(offset) & 0x0fff)); /*TODO: this looks ugly*/ \
	}


#define M_ILD(a,b,c)                    M_LDR(a,b,c)
#define M_LLD(a,b,c)                    M_LDRD(a,b,c)

#define M_ILD_INTERN(a,b,c)             M_LDR_INTERN(a,b,c)
#define M_LLD_INTERN(a,b,c)             M_LDRD_INTERN(a,b,c)

#define M_ALD(a,b,c)                    M_ILD(a,b,c)
#define M_ALD_INTERN(a,b,c)             M_ILD_INTERN(a,b,c)


#define M_IST(a,b,c)                    M_STR(a,b,c)
#define M_LST(a,b,c)                    M_STRD(a,b,c)

#define M_IST_INTERN(a,b,c)             M_STR_INTERN(a,b,c)
#define M_LST_INTERN(a,b,c)             M_STRD_INTERN(a,b,c)

#define M_AST(a,b,c)                    M_IST(a,b,c)
#define M_AST_INTERN(a,b,c)             M_IST_INTERN(a,b,c)


#if !defined(ENABLE_SOFTFLOAT)

#define M_FLD(a,b,c)                    M_LDFS(a,b,c)
#define M_DLD(a,b,c)                    M_LDFD(a,b,c)

#define M_FLD_INTERN(a,b,c)             M_LDFS_INTERN(a,b,c)
#define M_DLD_INTERN(a,b,c)             M_LDFD_INTERN(a,b,c)


#define M_FST(a,b,c)                    M_STFS(a,b,c)
#define M_DST(a,b,c)                    M_STFD(a,b,c)

#define M_FST_INTERN(a,b,c)             M_STFS_INTERN(a,b,c)
#define M_DST_INTERN(a,b,c)             M_STFD_INTERN(a,b,c)

#endif /* !defined(ENABLE_SOFTFLOAT) */


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

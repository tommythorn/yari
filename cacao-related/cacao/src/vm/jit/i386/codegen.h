/* src/vm/jit/i386/codegen.h - code generation macros and definitions for i386

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
#include "vm/types.h"

#include "vm/jit/i386/emit.h"

#include "vm/jit/jit.h"


#if defined(ENABLE_LSRA)
/* let LSRA allocate reserved registers (REG_ITMP[1|2|3]) */
# define LSRA_USES_REG_RES
#endif


/* additional functions and macros to generate code ***************************/

#define CALCOFFSETBYTES(var, reg, val) \
    if ((s4) (val) < -128 || (s4) (val) > 127) (var) += 4; \
    else if ((s4) (val) != 0) (var) += 1; \
    else if ((reg) == EBP) (var) += 1;


#define CALCIMMEDIATEBYTES(var, val) \
    if ((s4) (val) < -128 || (s4) (val) > 127) (var) += 4; \
    else (var) += 1;


#define ALIGNCODENOP \
    do { \
        for (s1 = 0; s1 < (s4) (((ptrint) cd->mcodeptr) & 7); s1++) \
            M_NOP; \
    } while (0)


/* MCODECHECK(icnt) */

#define MCODECHECK(icnt) \
    do { \
        if ((cd->mcodeptr + (icnt)) > (u1 *) cd->mcodeend) \
            codegen_increase(cd); \
    } while (0)


/* M_INTMOVE:
     generates an integer-move from register a to b.
     if a and b are the same int-register, no code will be generated.
*/ 

#define M_INTMOVE(a,b) \
    do { \
        if ((a) != (b)) \
            M_MOV(a, b); \
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


/* M_FLTMOVE:
    generates a floating-point-move from register a to b.
    if a and b are the same float-register, no code will be generated
*/

#define M_FLTMOVE(reg,dreg)                                          \
    do {                                                             \
        if ((reg) != (dreg)) {                                       \
            log_text("M_FLTMOVE");                                   \
            assert(0);                                               \
        }                                                            \
    } while (0)


#define ICONST(d,c) \
    do { \
        if ((c) == 0) \
            M_CLR(d); \
        else \
            M_MOV_IMM((c), d); \
    } while (0)


#define LCONST(d,c) \
    do { \
        if ((c) == 0) { \
            M_CLR(GET_LOW_REG(d)); \
            M_CLR(GET_HIGH_REG(d)); \
        } else { \
            M_MOV_IMM((c), GET_LOW_REG(d)); \
            M_MOV_IMM((c) >> 32, GET_HIGH_REG(d)); \
        } \
    } while (0)


/* branch defines *************************************************************/

#define BRANCH_UNCONDITIONAL_SIZE    5  /* size in bytes of a branch          */
#define BRANCH_CONDITIONAL_SIZE      6  /* size in bytes of a branch          */

#define BRANCH_NOPS \
    do { \
        M_NOP; \
        M_NOP; \
        M_NOP; \
        M_NOP; \
        M_NOP; \
        M_NOP; \
    } while (0)


/* patcher defines ************************************************************/

#define PATCHER_CALL_SIZE    5          /* size in bytes of a patcher call    */

#define PATCHER_NOPS \
    do { \
        M_NOP; \
        M_NOP; \
        M_NOP; \
        M_NOP; \
        M_NOP; \
    } while (0)


/* stub defines ***************************************************************/

#define COMPILERSTUB_CODESIZE    12


/* macros to create code ******************************************************/

#define M_ILD(a,b,disp)         emit_mov_membase_reg(cd, (b), (disp), (a))
#define M_ILD32(a,b,disp)       emit_mov_membase32_reg(cd, (b), (disp), (a))

#define M_ALD(a,b,disp)         M_ILD(a,b,disp)
#define M_ALD32(a,b,disp)       M_ILD32(a,b,disp)

#define M_ALD_MEM(a,disp)       emit_mov_mem_reg(cd, (disp), (a))

#define M_ALD_MEM_GET_OPC(p)    (*(p))
#define M_ALD_MEM_GET_MOD(p)    (((*(p + 1)) >> 6) & 0x03)
#define M_ALD_MEM_GET_REG(p)    (((*(p + 1)) >> 3) & 0x07)
#define M_ALD_MEM_GET_RM(p)     (((*(p + 1))     ) & 0x07)
#define M_ALD_MEM_GET_DISP(p)   (*((u4 *) (p + 2)))

#define M_LLD(a,b,disp) \
    do { \
        M_ILD(GET_LOW_REG(a),b,disp); \
        M_ILD(GET_HIGH_REG(a),b,disp + 4); \
    } while (0)

#define M_LLD32(a,b,disp) \
    do { \
        M_ILD32(GET_LOW_REG(a),b,disp); \
        M_ILD32(GET_HIGH_REG(a),b,disp + 4); \
    } while (0)

#define M_IST(a,b,disp)         emit_mov_reg_membase(cd, (a), (b), (disp))
#define M_IST_IMM(a,b,disp)     emit_mov_imm_membase(cd, (u4) (a), (b), (disp))
#define M_AST(a,b,disp)         M_IST(a,b,disp)
#define M_AST_IMM(a,b,disp)     M_IST_IMM(a,b,disp)

#define M_IST32(a,b,disp)       emit_mov_reg_membase32(cd, (a), (b), (disp))
#define M_IST32_IMM(a,b,disp)   emit_mov_imm_membase32(cd, (u4) (a), (b), (disp))

#define M_LST(a,b,disp) \
    do { \
        M_IST(GET_LOW_REG(a),b,disp); \
        M_IST(GET_HIGH_REG(a),b,disp + 4); \
    } while (0)

#define M_LST32(a,b,disp) \
    do { \
        M_IST32(GET_LOW_REG(a),b,disp); \
        M_IST32(GET_HIGH_REG(a),b,disp + 4); \
    } while (0)

#define M_LST_IMM(a,b,disp) \
    do { \
        M_IST_IMM(a,b,disp); \
        M_IST_IMM(a >> 32,b,disp + 4); \
    } while (0)

#define M_LST32_IMM(a,b,disp) \
    do { \
        M_IST32_IMM(a,b,disp); \
        M_IST32_IMM(a >> 32,b,disp + 4); \
    } while (0)

#define M_IADD(a,b)             emit_alu_reg_reg(cd, ALU_ADD, (a), (b))
#define M_ISUB(a,b)             emit_alu_reg_reg(cd, ALU_SUB, (a), (b))
#define M_IMUL(a,b)             emit_imul_reg_reg(cd, (a), (b))
#define M_IDIV(a)               emit_idiv_reg(cd, (a))

#define M_MUL(a)                emit_mul_reg(cd, (a))

#define M_IADD_IMM(a,b)         emit_alu_imm_reg(cd, ALU_ADD, (a), (b))
#define M_ISUB_IMM(a,b)         emit_alu_imm_reg(cd, ALU_SUB, (a), (b))
#define M_IMUL_IMM(a,b,c)       emit_imul_imm_reg_reg(cd, (b), (a), (c))

#define M_IADD_IMM32(a,b)       emit_alu_imm32_reg(cd, ALU_ADD, (a), (b))
#define M_ISUB_IMM32(a,b)       emit_alu_imm32_reg(cd, ALU_SUB, (a), (b))

#define M_IADD_IMM_MEMBASE(a,b,c) emit_alu_imm_membase(cd, ALU_ADD, (a), (b), (c))

#define M_ISUB_IMM_MEMABS(a,b)  emit_alu_imm_memabs(cd, ALU_SUB, (a), (b))

#define M_IADDC(a,b)            emit_alu_reg_reg(cd, ALU_ADC, (a), (b))
#define M_ISUBB(a,b)            emit_alu_reg_reg(cd, ALU_SBB, (a), (b))

#define M_IADDC_IMM(a,b)        emit_alu_imm_reg(cd, ALU_ADC, (a), (b))
#define M_ISUBB_IMM(a,b)        emit_alu_imm_reg(cd, ALU_SBB, (a), (b))

#define M_AADD_IMM(a,b)         M_IADD_IMM(a,b)
#define M_AADD_IMM32(a,b)       M_IADD_IMM32(a,b)
#define M_ASUB_IMM(a,b)         M_ISUB_IMM(a,b)

#define M_NEG(a)                emit_neg_reg(cd, (a))

#define M_AND(a,b)              emit_alu_reg_reg(cd, ALU_AND, (a), (b))
#define M_OR(a,b)               emit_alu_reg_reg(cd, ALU_OR, (a), (b))
#define M_XOR(a,b)              emit_alu_reg_reg(cd, ALU_XOR, (a), (b))

#define M_AND_IMM(a,b)          emit_alu_imm_reg(cd, ALU_AND, (a), (b))
#define M_OR_IMM(a,b)           emit_alu_imm_reg(cd, ALU_OR, (a), (b))
#define M_XOR_IMM(a,b)          emit_alu_imm_reg(cd, ALU_XOR, (a), (b))

#define M_AND_IMM32(a,b)        emit_alu_imm32_reg(cd, ALU_AND, (a), (b))

#define M_CLR(a)                M_XOR(a,a)

#define M_PUSH(a)               emit_push_reg(cd, (a))
#define M_PUSH_IMM(a)           emit_push_imm(cd, (s4) (a))
#define M_POP(a)                emit_pop_reg(cd, (a))

#define M_MOV(a,b)              emit_mov_reg_reg(cd, (a), (b))
#define M_MOV_IMM(a,b)          emit_mov_imm_reg(cd, (u4) (a), (b))

#define M_TEST(a)               emit_test_reg_reg(cd, (a), (a))
#define M_TEST_IMM(a,b)         emit_test_imm_reg(cd, (a), (b))

#define M_CMP(a,b)              emit_alu_reg_reg(cd, ALU_CMP, (a), (b))
#define M_CMP_MEMBASE(a,b,c)    emit_alu_membase_reg(cd, ALU_CMP, (a), (b), (c))

#define M_CMP_IMM(a,b)          emit_alu_imm_reg(cd, ALU_CMP, (a), (b))
#define M_CMP_IMM_MEMBASE(a,b,c) emit_alu_imm_membase(cd, ALU_CMP, (a), (b), (c))

#define M_CMP_IMM32(a,b)        emit_alu_imm32_reg(cd, ALU_CMP, (a), (b))

#define M_BSEXT(a,b)            /* XXX does not work, because of nibbles */
#define M_SSEXT(a,b)            emit_movswl_reg_reg(cd, (a), (b))

#define M_CZEXT(a,b)            emit_movzwl_reg_reg(cd, (a), (b))

#define M_CLTD                  emit_cltd(cd)

#define M_SLL(a)                emit_shift_reg(cd, SHIFT_SHL, (a))
#define M_SRA(a)                emit_shift_reg(cd, SHIFT_SAR, (a))
#define M_SRL(a)                emit_shift_reg(cd, SHIFT_SHR, (a))

#define M_SLL_IMM(a,b)          emit_shift_imm_reg(cd, SHIFT_SHL, (a), (b))
#define M_SRA_IMM(a,b)          emit_shift_imm_reg(cd, SHIFT_SAR, (a), (b))
#define M_SRL_IMM(a,b)          emit_shift_imm_reg(cd, SHIFT_SHR, (a), (b))

#define M_SLLD(a,b)             emit_shld_reg_reg(cd, (a), (b))
#define M_SRLD(a,b)             emit_shrd_reg_reg(cd, (a), (b))

#define M_SLLD_IMM(a,b,c)       emit_shld_imm_reg_reg(cd, (a), (b), (c))
#define M_SRLD_IMM(a,b,c)       emit_shrd_imm_reg_reg(cd, (a), (b), (c))

#define M_CALL(a)               emit_call_reg(cd, (a))
#define M_CALL_IMM(a)           emit_call_imm(cd, (a))
#define M_RET                   emit_ret(cd)

#define M_BEQ(a)                emit_jcc(cd, CC_E, (a))
#define M_BNE(a)                emit_jcc(cd, CC_NE, (a))
#define M_BLT(a)                emit_jcc(cd, CC_L, (a))
#define M_BLE(a)                emit_jcc(cd, CC_LE, (a))
#define M_BGE(a)                emit_jcc(cd, CC_GE, (a))
#define M_BGT(a)                emit_jcc(cd, CC_G, (a))

#define M_BB(a)                 emit_jcc(cd, CC_B, (a))
#define M_BBE(a)                emit_jcc(cd, CC_BE, (a))
#define M_BAE(a)                emit_jcc(cd, CC_AE, (a))
#define M_BA(a)                 emit_jcc(cd, CC_A, (a))
#define M_BNS(a)                emit_jcc(cd, CC_NS, (a))
#define M_BS(a)                 emit_jcc(cd, CC_S, (a))

#define M_JMP(a)                emit_jmp_reg(cd, (a))
#define M_JMP_IMM(a)            emit_jmp_imm(cd, (a))

#define M_NOP                   emit_nop(cd)


#define M_FLD(a,b,disp)         emit_flds_membase(cd, (b), (disp))
#define M_DLD(a,b,disp)         emit_fldl_membase(cd, (b), (disp))

#define M_FLD32(a,b,disp)       emit_flds_membase32(cd, (b), (disp))
#define M_DLD32(a,b,disp)       emit_fldl_membase32(cd, (b), (disp))

#define M_FST(a,b,disp)         emit_fstps_membase(cd, (b), (disp))
#define M_DST(a,b,disp)         emit_fstpl_membase(cd, (b), (disp))

#define M_FSTNP(a,b,disp)       emit_fsts_membase(cd, (b), (disp))
#define M_DSTNP(a,b,disp)       emit_fstl_membase(cd, (b), (disp))

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
 */

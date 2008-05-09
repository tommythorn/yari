/* src/vm/jit/mips/disass.c - primitive disassembler for MIPS machine code

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

   $Id: disass.c 7351 2007-02-13 21:32:36Z twisti $

*/


#include "config.h"

#include <stdio.h>

#include "vm/types.h"

#include "vm/jit/abi.h"


/*  The disassembler uses four tables for decoding the instructions. The first
	table (ops) is used to classify the instructions based on the op code and
	contains the instruction names for instructions which don't used the
	function codes. This table is indexed by the op code (6 bit, 64 entries).
	The other tables are either indexed by the function code or some special
	format and branch codes.
*/

#define ITYPE_UNDEF  0          /* undefined instructions (illegal opcode)    */
#define ITYPE_JMP    1          /* jump instructions                          */
#define ITYPE_IMM    2          /* immediate instructions                     */
#define ITYPE_MEM    3          /* memory instructions                        */
#define ITYPE_FMEM   4          /* floating point memory instructions         */
#define ITYPE_BRA    5          /* branch instructions                        */
#define ITYPE_RIMM   6          /* special/branch instructions                */
#define ITYPE_OP     7          /* integer instructions                       */
#define ITYPE_TRAP   8          /* trap instructions                          */
#define ITYPE_DIVMUL 9          /* integer divide/multiply instructions       */
#define ITYPE_MTOJR 10          /* move to and jump register instructions     */
#define ITYPE_MFROM 11          /* move from instructions                     */
#define ITYPE_SYS   12          /* operating system instructions              */
#define ITYPE_FOP   13          /* floating point instructions                */
#define ITYPE_FOP2  14          /* 2 operand floating point instructions      */
#define ITYPE_FCMP  15          /* floating point compare instructions        */


/* instruction decode table for 6 bit op codes                                */

static struct {char *name; int itype;} ops[] = {

	/* 0x00 */  {"special ",    ITYPE_OP},
	/* 0x01 */  {"regimm  ",  ITYPE_RIMM},
	/* 0x02 */  {"j       ",   ITYPE_JMP},
	/* 0x03 */  {"jal     ",   ITYPE_JMP},
	/* 0x04 */  {"beq     ",   ITYPE_BRA},
	/* 0x05 */  {"bne     ",   ITYPE_BRA},
	/* 0x06 */  {"blez    ",   ITYPE_BRA},
	/* 0x07 */  {"bgtz    ",   ITYPE_BRA},

	/* 0x08 */  {"addi    ",   ITYPE_IMM},
	/* 0x09 */  {"addiu   ",   ITYPE_IMM},
	/* 0x0a */  {"slti    ",   ITYPE_IMM},
	/* 0x0b */  {"sltiu   ",   ITYPE_IMM},
	/* 0x0c */  {"andi    ",   ITYPE_IMM},
	/* 0x0d */  {"ori     ",   ITYPE_IMM},
	/* 0x0e */  {"xori    ",   ITYPE_IMM},
	/* 0x0f */  {"lui     ",   ITYPE_IMM},

	/* 0x10 */  {"cop0    ",    ITYPE_OP},
	/* 0x11 */  {"cop1    ",   ITYPE_FOP},
	/* 0x12 */  {"cop2    ",    ITYPE_OP},
	/* 0x13 */  {"",         ITYPE_UNDEF},
	/* 0x14 */  {"beql    ",   ITYPE_BRA},
	/* 0x15 */  {"bnel    ",   ITYPE_BRA},
	/* 0x16 */  {"blezl   ",   ITYPE_BRA},
	/* 0x17 */  {"bgtzl   ",   ITYPE_BRA},

	/* 0x18 */  {"daddi   ",   ITYPE_IMM},
	/* 0x19 */  {"daddiu  ",   ITYPE_IMM},
	/* 0x1a */  {"ldl     ",   ITYPE_MEM},
	/* 0x1b */  {"ldr     ",   ITYPE_MEM},
	/* 0x1c */  {"",         ITYPE_UNDEF},
	/* 0x1d */  {"",         ITYPE_UNDEF},
	/* 0x1e */  {"",         ITYPE_UNDEF},
	/* 0x1f */  {"",         ITYPE_UNDEF},

	/* 0x20 */  {"lb      ",   ITYPE_MEM},
	/* 0x21 */  {"lh      ",   ITYPE_MEM},
	/* 0x22 */  {"lwl     ",   ITYPE_MEM},
	/* 0x23 */  {"lw      ",   ITYPE_MEM},
	/* 0x24 */  {"lbu     ",   ITYPE_MEM},
	/* 0x25 */  {"lhu     ",   ITYPE_MEM},
	/* 0x26 */  {"lwr     ",   ITYPE_MEM},
	/* 0x27 */  {"lwu     ",   ITYPE_MEM},

	/* 0x28 */  {"sb      ",   ITYPE_MEM},
	/* 0x29 */  {"sh      ",   ITYPE_MEM},
	/* 0x2a */  {"swl     ",   ITYPE_MEM},
	/* 0x2b */  {"sw      ",   ITYPE_MEM},
	/* 0x2c */  {"sdl     ",   ITYPE_MEM},
	/* 0x2d */  {"sdr     ",   ITYPE_MEM},
	/* 0x2e */  {"swr     ",   ITYPE_MEM},
	/* 0x2f */  {"cache   ",   ITYPE_MEM},

	/* 0x30 */  {"ll      ",   ITYPE_MEM},
	/* 0x31 */  {"lwc1    ",  ITYPE_FMEM},
	/* 0x32 */  {"lwc2    ",   ITYPE_MEM},
	/* 0x33 */  {"",         ITYPE_UNDEF},
	/* 0x34 */  {"lld     ",   ITYPE_MEM},
	/* 0x35 */  {"ldc1    ",  ITYPE_FMEM},
	/* 0x36 */  {"ldc2    ",   ITYPE_MEM},
	/* 0x37 */  {"ld      ",   ITYPE_MEM},

	/* 0x38 */  {"sc      ",   ITYPE_MEM},
	/* 0x39 */  {"swc1    ",  ITYPE_FMEM},
	/* 0x3a */  {"swc2    ",   ITYPE_MEM},
	/* 0x3b */  {"",         ITYPE_UNDEF},
	/* 0x3c */  {"sld     ",   ITYPE_MEM},
	/* 0x3d */  {"sdc1    ",  ITYPE_FMEM},
	/* 0x3e */  {"sdc2    ",   ITYPE_MEM},
	/* 0x3f */  {"sd      ",   ITYPE_MEM}
};


/* instruction decode table for 6 bit special function codes                  */

static struct {char *name; int ftype;} regops[] = {

	/* 0x00 */  {"sll     ",   ITYPE_IMM},
	/* 0x01 */  {""        , ITYPE_UNDEF},
	/* 0x02 */  {"srl     ",   ITYPE_IMM},
	/* 0x03 */  {"sra     ",   ITYPE_IMM},
	/* 0x04 */  {"sllv    ",    ITYPE_OP},
	/* 0x05 */  {""        , ITYPE_UNDEF},
	/* 0x06 */  {"srlv    ",    ITYPE_OP},
	/* 0x07 */  {"srav    ",    ITYPE_OP},

	/* 0x08 */  {"jr      ", ITYPE_MTOJR},
	/* 0x09 */  {"jalr    ",   ITYPE_JMP},
	/* 0x0a */  {""        , ITYPE_UNDEF},
	/* 0x0b */  {""        , ITYPE_UNDEF},
	/* 0x0c */  {"syscall ",   ITYPE_SYS},
	/* 0x0d */  {"break   ",   ITYPE_SYS},
	/* 0x0e */  {""        , ITYPE_UNDEF},
	/* 0x0f */  {"sync    ",   ITYPE_SYS},

	/* 0x10 */  {"mfhi    ", ITYPE_MFROM},
	/* 0x11 */  {"mthi    ", ITYPE_MTOJR},
	/* 0x12 */  {"mflo    ", ITYPE_MFROM},
	/* 0x13 */  {"mtlo    ", ITYPE_MTOJR},
	/* 0x14 */  {"dsllv   ",    ITYPE_OP},
	/* 0x15 */  {""        , ITYPE_UNDEF},
	/* 0x16 */  {"dslrv   ",    ITYPE_OP},
	/* 0x17 */  {"dsrav   ",    ITYPE_OP},

	/* 0x18 */  {"mult    ",ITYPE_DIVMUL},
	/* 0x19 */  {"multu   ",ITYPE_DIVMUL},
	/* 0x1a */  {"div     ",ITYPE_DIVMUL},
	/* 0x1b */  {"divu    ",ITYPE_DIVMUL},
	/* 0x1c */  {"dmult   ",ITYPE_DIVMUL},
	/* 0x1d */  {"dmultu  ",ITYPE_DIVMUL},
	/* 0x1e */  {"ddiv    ",ITYPE_DIVMUL},
	/* 0x1f */  {"ddivu   ",ITYPE_DIVMUL},

	/* 0x20 */  {"add     ",    ITYPE_OP},
	/* 0x21 */  {"addu    ",    ITYPE_OP},
	/* 0x22 */  {"sub     ",    ITYPE_OP},
	/* 0x23 */  {"subu    ",    ITYPE_OP},
	/* 0x24 */  {"and     ",    ITYPE_OP},
	/* 0x25 */  {"or      ",    ITYPE_OP},
	/* 0x26 */  {"xor     ",    ITYPE_OP},
	/* 0x27 */  {"nor     ",    ITYPE_OP},

	/* 0x28 */  {""        , ITYPE_UNDEF},
	/* 0x29 */  {""        , ITYPE_UNDEF},
	/* 0x2a */  {"slt     ",    ITYPE_OP},
	/* 0x2b */  {"sltu    ",    ITYPE_OP},
	/* 0x2c */  {"dadd    ",    ITYPE_OP},
	/* 0x2d */  {"daddu   ",    ITYPE_OP},
	/* 0x2e */  {"dsub    ",    ITYPE_OP},
	/* 0x2f */  {"dsubu   ",    ITYPE_OP},

	/* 0x30 */  {"tge     ",  ITYPE_TRAP},
	/* 0x31 */  {"tgeu    ",  ITYPE_TRAP},
	/* 0x32 */  {"tlt     ",  ITYPE_TRAP},
	/* 0x33 */  {"tltu    ",  ITYPE_TRAP},
	/* 0x34 */  {"teq     ",  ITYPE_TRAP},
	/* 0x35 */  {""        , ITYPE_UNDEF},
	/* 0x36 */  {"tne     ",  ITYPE_TRAP},
	/* 0x37 */  {""        , ITYPE_UNDEF},

	/* 0x38 */  {"dsll    ",   ITYPE_IMM},
	/* 0x39 */  {""        , ITYPE_UNDEF},
	/* 0x3a */  {"dsrl    ",   ITYPE_IMM},
	/* 0x3b */  {"dsra    ",   ITYPE_IMM},
	/* 0x3c */  {"dsll32  ",   ITYPE_IMM},
	/* 0x3d */  {""        , ITYPE_UNDEF},
	/* 0x3e */  {"dsrl32  ",   ITYPE_IMM},
	/* 0x3f */  {"dsra32  ",   ITYPE_IMM}
};


/* instruction decode table for 5 bit reg immediate function codes            */

static struct {char *name; int ftype;} regimms[] = {

	/* 0x00 */  {"bltz    ",   ITYPE_BRA},
	/* 0x01 */  {"bgez    ",   ITYPE_BRA},
	/* 0x02 */  {"bltzl   ",   ITYPE_BRA},
	/* 0x03 */  {"bgezl   ",   ITYPE_BRA},
	/* 0x04 */  {"",         ITYPE_UNDEF},
	/* 0x05 */  {"",         ITYPE_UNDEF},
	/* 0x06 */  {"",         ITYPE_UNDEF},
	/* 0x07 */  {"",         ITYPE_UNDEF},

	/* 0x08 */  {"tgei    ",   ITYPE_IMM},
	/* 0x09 */  {"dgeiu   ",   ITYPE_IMM},
	/* 0x0a */  {"tlti    ",   ITYPE_IMM},
	/* 0x0b */  {"tltiu   ",   ITYPE_IMM},
	/* 0x0c */  {"teqi    ",   ITYPE_IMM},
	/* 0x0d */  {"",         ITYPE_UNDEF},
	/* 0x0e */  {"tnei    ",   ITYPE_IMM},
	/* 0x0f */  {"",         ITYPE_UNDEF},

	/* 0x10 */  {"bltzal  ",   ITYPE_BRA},
	/* 0x11 */  {"bgezal  ",   ITYPE_BRA},
	/* 0x12 */  {"bltzall ",   ITYPE_BRA},
	/* 0x13 */  {"bgezall ",   ITYPE_BRA},
	/* 0x14 */  {"",         ITYPE_UNDEF},
	/* 0x15 */  {"",         ITYPE_UNDEF},
	/* 0x16 */  {"",         ITYPE_UNDEF},
	/* 0x17 */  {"",         ITYPE_UNDEF},

	/* 0x18 */  {"",         ITYPE_UNDEF},
	/* 0x19 */  {"",         ITYPE_UNDEF},
	/* 0x1a */  {"",         ITYPE_UNDEF},
	/* 0x1b */  {"",         ITYPE_UNDEF},
	/* 0x1c */  {"",         ITYPE_UNDEF},
	/* 0x1d */  {"",         ITYPE_UNDEF},
	/* 0x1e */  {"",         ITYPE_UNDEF},
	/* 0x1f */  {"",         ITYPE_UNDEF}
};


/* instruction decode table for 6 bit floating point op codes                 */

static struct {char *name; char *fill; int ftype;} fops[] = {

	/* 0x00 */  {"add", "   ",   ITYPE_FOP},
	/* 0x01 */  {"sub", "   ",   ITYPE_FOP},
	/* 0x02 */  {"mul", "   ",   ITYPE_FOP},
	/* 0x03 */  {"div", "   ",   ITYPE_FOP},
	/* 0x04 */  {"sqrt", "  ",   ITYPE_FOP},
	/* 0x05 */  {"abs", "   ",  ITYPE_FOP2},
	/* 0x06 */  {"mov", "   ",  ITYPE_FOP2},
	/* 0x07 */  {"neg", "   ",  ITYPE_FOP2},

	/* 0x08 */  {"roundl", "",  ITYPE_FOP2},
	/* 0x09 */  {"truncl", "",  ITYPE_FOP2},
	/* 0x0a */  {"ceill", " ",  ITYPE_FOP2},
	/* 0x0b */  {"floorl", "",  ITYPE_FOP2},
	/* 0x0c */  {"round", " ",  ITYPE_FOP2},
	/* 0x0d */  {"trunc", " ",  ITYPE_FOP2},
	/* 0x0e */  {"ceil", "  ",  ITYPE_FOP2},
	/* 0x0f */  {"floor", " ",  ITYPE_FOP2},

	/* 0x10 */  {"",       "", ITYPE_UNDEF},
	/* 0x11 */  {"",       "", ITYPE_UNDEF},
	/* 0x12 */  {"",       "", ITYPE_UNDEF},
	/* 0x13 */  {"",       "", ITYPE_UNDEF},
	/* 0x14 */  {"",       "", ITYPE_UNDEF},
	/* 0x15 */  {"",       "", ITYPE_UNDEF},
	/* 0x16 */  {"",       "", ITYPE_UNDEF},
	/* 0x17 */  {"",       "", ITYPE_UNDEF},

	/* 0x18 */  {"",       "", ITYPE_UNDEF},
	/* 0x19 */  {"recip", " ",  ITYPE_FOP2},
	/* 0x1a */  {"rsqrt", " ",  ITYPE_FOP2},
	/* 0x1b */  {"",       "", ITYPE_UNDEF},
	/* 0x1c */  {"",       "", ITYPE_UNDEF},
	/* 0x1d */  {"",       "", ITYPE_UNDEF},
	/* 0x1e */  {"",       "", ITYPE_UNDEF},
	/* 0x1f */  {"",       "", ITYPE_UNDEF},

	/* 0x20 */  {"cvts", "  ",  ITYPE_FOP2},
	/* 0x21 */  {"cvtd", "  ",  ITYPE_FOP2},
	/* 0x22 */  {"cvtx", "  ",  ITYPE_FOP2},
	/* 0x23 */  {"cvtq", "  ",  ITYPE_FOP2},
	/* 0x24 */  {"cvtw", "  ",  ITYPE_FOP2},
	/* 0x25 */  {"cvtl", "  ",  ITYPE_FOP2},
	/* 0x26 */  {"",       "", ITYPE_UNDEF},
	/* 0x27 */  {"",       "", ITYPE_UNDEF},

	/* 0x28 */  {"",       "", ITYPE_UNDEF},
	/* 0x29 */  {"",       "", ITYPE_UNDEF},
	/* 0x2a */  {"",       "", ITYPE_UNDEF},
	/* 0x2b */  {"",       "", ITYPE_UNDEF},
	/* 0x2c */  {"",       "", ITYPE_UNDEF},
	/* 0x2d */  {"",       "", ITYPE_UNDEF},
	/* 0x2e */  {"",       "", ITYPE_UNDEF},
	/* 0x2f */  {"",       "", ITYPE_UNDEF},

	/* 0x30 */  {"c.f", "   ",  ITYPE_FCMP},
	/* 0x31 */  {"c.un", "  ",  ITYPE_FCMP},
	/* 0x32 */  {"c.eq", "  ",  ITYPE_FCMP},
	/* 0x33 */  {"c.ueq", " ",  ITYPE_FCMP},
	/* 0x34 */  {"c.olt", " ",  ITYPE_FCMP},
	/* 0x35 */  {"c.ult", " ",  ITYPE_FCMP},
	/* 0x36 */  {"c.ole", " ",  ITYPE_FCMP},
	/* 0x37 */  {"c.ule", " ",  ITYPE_FCMP},

	/* 0x38 */  {"c.sf", "  ",  ITYPE_FCMP},
	/* 0x39 */  {"c.ngle", "",  ITYPE_FCMP},
	/* 0x3a */  {"c.seq", " ",  ITYPE_FCMP},
	/* 0x3b */  {"c.ngl", " ",  ITYPE_FCMP},
	/* 0x3c */  {"c.lt", "  ",  ITYPE_FCMP},
	/* 0x3d */  {"c.nge", " ",  ITYPE_FCMP},
	/* 0x3e */  {"c.le", "  ",  ITYPE_FCMP},
	/* 0x3f */  {"c.ngt", " ",  ITYPE_FCMP}
};


/* format decode table for 3 bit floating point format codes                  */

static char *fmt[] = {
	/* 0x00 */  ".s",
	/* 0x01 */  ".d",
	/* 0x02 */  ".x",
	/* 0x03 */  ".q",
	/* 0x04 */  ".w",
	/* 0x05 */  ".l",
	/* 0x06 */  ".?",
	/* 0x07 */  ".?"
};


/* format decode table for 2 bit floating point branch codes                  */

static char *fbra[] = {
	/* 0x00 */  "bc1f    ",
	/* 0x01 */  "bc1t    ",
	/* 0x02 */  "bc1fl   ",
	/* 0x03 */  "bc1tl   "
};


/* instruction decode table for 32 floating point registers                   */

#if 0
static char *fregs[] = {

	/* 0x00 */  "fv0",   /*  "$f0", */
	/* 0x01 */  "ft16",  /*  "$f1", */
	/* 0x02 */  "fv1",   /*  "$f2", */
	/* 0x03 */  "ft17",  /*  "$f3", */
	/* 0x04 */  "ft0",   /*  "$f4", */
	/* 0x05 */  "ft1",   /*  "$f5", */
	/* 0x06 */  "ft2",   /*  "$f6", */
	/* 0x07 */  "ft3",   /*  "$f7", */

	/* 0x08 */  "ft4",   /*  "$f8", */
	/* 0x09 */  "ft5",   /*  "$f9", */
	/* 0x0a */  "ft6",   /* "$f10", */
	/* 0x0b */  "ft7",   /* "$f11", */
	/* 0x0c */  "fa0",   /* "$f12", */
	/* 0x0d */  "fa1",   /* "$f13", */
	/* 0x0e */  "fa2",   /* "$f14", */
	/* 0x0f */  "fa3",   /* "$f15", */

	/* 0x10 */  "fa4",   /* "$f16", */
	/* 0x11 */  "fa5",   /* "$f17", */
	/* 0x12 */  "fa6",   /* "$f18", */
	/* 0x13 */  "fa7",   /* "$f19", */
	/* 0x14 */  "ft8",   /* "$f20", */
	/* 0x15 */  "ft9",   /* "$f21", */
	/* 0x16 */  "ft10",  /* "$f22", */
	/* 0x17 */  "ft11",  /* "$f23", */

	/* 0x18 */  "fs0",   /* "$f24", */
	/* 0x19 */  "ft12",  /* "$f25", */
	/* 0x1a */  "fs1",   /* "$f26", */
	/* 0x1b */  "ft13",  /* "$f27", */
	/* 0x1c */  "fs2",   /* "$f28", */
	/* 0x1d */  "ft14",  /* "$f29", */
	/* 0x1e */  "fs3",   /* "$f30", */
	/* 0x1f */  "ft15"   /* "$f31"  */
};
#endif


/* disassinstr *****************************************************************

   Outputs a disassembler listing of one machine code instruction on
   'stdout'.

   code: pointer to instructions machine code

*******************************************************************************/

u1 *disassinstr(u1 *code)
{
	s4 op;                      /* 6 bit op code                              */
	s4 opfun;                   /* 6 bit function code                        */
	s4 rs, rt, rd;              /* 5 bit integer register specifiers          */
	s4 fs, ft, fd;              /* 5 bit floating point register specifiers   */
	s4 shift;                   /* 5 bit unsigned shift amount                */
	s4 c;

	c = *((s4 *) code);

	op    = (c >> 26) & 0x3f;   /* 6 bit op code                              */
	opfun = (c >>  0) & 0x3f;   /* 6 bit function code                        */
	rs    = (c >> 21) & 0x1f;   /* 5 bit source register specifier            */
	rt    = (c >> 16) & 0x1f;   /* 5 bit source/destination register specifier*/
	rd    = (c >> 11) & 0x1f;   /* 5 bit destination register specifier       */
	shift = (c >>  6) & 0x1f;   /* 5 bit unsigned shift amount                */

#if SIZEOF_VOID_P == 8
	printf("0x%016lx:   %08x    ", (u8) code, c);
#else
	printf("0x%08x:   %08x    ", (u4) code, c);
#endif

	switch (ops[op].itype) {
	case ITYPE_JMP:                      /* 26 bit unsigned jump offset   */
		printf("%s %#09x\n", ops[op].name, (c & 0x3ffffff) << 2); 
		break;

	case ITYPE_IMM:                      /* 16 bit signed immediate value */
		printf("%s %s,%s,%d\n", ops[op].name, abi_registers_integer_name[rt],
			   abi_registers_integer_name[rs], (c << 16) >> 16); 
		break;

	case ITYPE_MEM:                      /* 16 bit signed memory offset   */
		printf("%s %s,%d(%s)\n", ops[op].name, abi_registers_integer_name[rt],
			   (c << 16) >> 16, abi_registers_integer_name[rs]); 
		break;

	case ITYPE_FMEM:                     /* 16 bit signed memory offset   */
		printf("%s $f%d,%d(%s)\n", ops[op].name, rt, (c << 16) >> 16,
			   abi_registers_integer_name[rs]);
		break;

	case ITYPE_BRA:                      /* 16 bit signed branch offset   */
		if (op == 0x04 && rs == 0 && rt == 0) {
#if SIZEOF_VOID_P == 8
			printf("b        0x%016lx\n", (u8) code + 4 + ((c << 16) >> 14));
#else
			printf("b        0x%08x\n", (u4) code + 4 + ((c << 16) >> 14));
#endif
			break;
		}	
#if SIZEOF_VOID_P == 8
		printf("%s %s,%s,0x%016lx\n", ops[op].name,
			   abi_registers_integer_name[rs],
			   abi_registers_integer_name[rt], 
			   (u8) code + 4 + ((c << 16) >> 14));
#else
		printf("%s %s,%s,0x%08x\n", ops[op].name,
			   abi_registers_integer_name[rs],
			   abi_registers_integer_name[rt], 
			   (u4) code + 4 + ((c << 16) >> 14));
#endif
		break;
			
	case ITYPE_RIMM:
		if (regimms[rt].ftype == ITYPE_IMM)
			printf("%s %s,%d\n", regimms[rt].name,
				   abi_registers_integer_name[rs], (c << 16) >> 16);
		else if (regimms[rt].ftype == ITYPE_BRA)
#if SIZEOF_VOID_P == 8
			printf("%s %s,0x%016lx\n", regimms[rt].name,
				   abi_registers_integer_name[rs],
				   (u8) code + 4 + ((c << 16) >> 14));
#else
			printf("%s %s,0x%08x\n", regimms[rt].name,
				   abi_registers_integer_name[rs],
				   (u4) code + 4 + ((c << 16) >> 14));
#endif
		else
			printf("regimm   %#04x,$%d,%d\n", rt, rs, (c << 16) >> 16);		
		break;

	case ITYPE_OP:
		if (c == 0) {
			printf("nop\n");
			break;
		}
		if (opfun == 0x25 && rt == 0) {
			if (rs == 0)
				printf("clr      %s\n", abi_registers_integer_name[rd]);
			else
				printf("move     %s,%s\n", abi_registers_integer_name[rd],
					   abi_registers_integer_name[rs]);
			break;
		}
		switch (regops[opfun].ftype) {
		case ITYPE_OP:
			printf("%s %s,%s,%s\n", regops[opfun].name,
				   abi_registers_integer_name[rd],
				   abi_registers_integer_name[rs],
				   abi_registers_integer_name[rt]);
			break;
		case ITYPE_IMM:  /* immediate instruction */
			printf("%s %s,%s,%d\n",
				   regops[opfun].name, abi_registers_integer_name[rd],
				   abi_registers_integer_name[rt], shift);
			break;
		case ITYPE_TRAP:
			printf("%s %s,%s,%d\n", regops[opfun].name,
				   abi_registers_integer_name[rs],
				   abi_registers_integer_name[rt], (c << 16) >> 22);
			break;
		case ITYPE_DIVMUL: /* div/mul instruction */
			printf("%s %s,%s\n", regops[opfun].name,
				   abi_registers_integer_name[rs],
				   abi_registers_integer_name[rt]);
			break;
		case ITYPE_JMP:
			if (rd == 31) {
				printf("%s %s\n", regops[opfun].name,
					   abi_registers_integer_name[rs]);
				break;
			}
			printf("%s %s,%s\n", regops[opfun].name,
				   abi_registers_integer_name[rd],
				   abi_registers_integer_name[rs]);
			break;
		case ITYPE_MTOJR:
			if (opfun == 8 && rs == 31) {
				printf("ret\n");
				break;
			}
			printf("%s %s\n", regops[opfun].name,
				   abi_registers_integer_name[rs]);
			break;
		case ITYPE_MFROM:
			printf("%s %s\n", regops[opfun].name,
				   abi_registers_integer_name[rd]);
			break;
		case ITYPE_SYS:
			printf("%s\n", regops[opfun].name);
		default:
			printf("special  (%#04x) $%d,$%d,$%d\n", opfun, rd, rs, rt);
		}		
		break;

	case ITYPE_FOP:
		fs    = (c >> 11) & 0x1f;   /* 5 bit source register              */
		ft    = (c >> 16) & 0x1f;   /* 5 bit source/destination register  */
		fd    = (c >>  6) & 0x1f;   /* 5 bit destination register         */

		if (rs == 8) {              /* floating point branch              */
#if SIZEOF_VOID_P == 8
			printf("%s 0x%016lx\n", fbra[ft & 3],
				   (u8) code + 4 + ((c << 16) >> 14));
#else
			printf("%s 0x%08x\n", fbra[ft & 3],
				   (u4) code + 4 + ((c << 16) >> 14));
#endif
			break;
		}

		if (rs == 0) {              /* move from                          */
			printf("mfc1     %s,$f%d\n", abi_registers_integer_name[rt], fs);
			break;
		}

		if (rs == 1) {              /* double move from                   */
			printf("dmfc1    %s,$f%d\n", abi_registers_integer_name[rt], fs);
			break;
		}

		if (rs == 4) {              /* move to                            */
			printf("mtc1     %s,$f%d\n", abi_registers_integer_name[rt], fs);
			break;
		}

		if (rs == 5) {              /* double move to                     */
			printf("dmtc1    %s,$f%d\n", abi_registers_integer_name[rt], fs);
			break;
		}

		rs    = rs & 7;             /* truncate to 3 bit format specifier */

		if (fops[opfun].ftype == ITYPE_FOP)
			printf("%s%s%s $f%d,$f%d,$f%d\n", fops[opfun].name, fmt[rs],
				   fops[opfun].fill, fd, fs, ft);
		else if (fops[opfun].ftype == ITYPE_FOP2)
			printf("%s%s%s $f%d,$f%d\n", fops[opfun].name, fmt[rs],
				   fops[opfun].fill, fd, fs);
		else if (fops[opfun].ftype == ITYPE_FCMP)
			printf("%s%s%s $f%d,$f%d\n", fops[opfun].name, fmt[rs],
				   fops[opfun].fill, fs, ft);
		else
			printf("cop1     (%#04x) $f%d,$f%d,$f%d\n", opfun, fd, fs, ft);
		break;

	default:
		printf("undef    %#04x(%#04x) $%d,$%d,$%d\n", op, opfun, rd, rs, rt);
	}

	return code + 4;
}


/* disassemble *****************************************************************

   Outputs a disassembler listing of some machine code on 'stdout'.

   start: pointer to first instruction
   end:   pointer to last instruction

*******************************************************************************/

void disassemble(u1 *start, u1 *end)
{
	printf("  --- disassembler listing ---\n");	
	for (; start < end; )
		start = disassinstr(start);
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

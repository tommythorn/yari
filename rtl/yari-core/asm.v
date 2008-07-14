// -----------------------------------------------------------------------
//
//   Copyright 2004 Tommy Thorn - All Rights Reserved
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
//   Bostom MA 02111-1307, USA; either version 2 of the License, or
//   (at your option) any later version; incorporated herein by reference.
//
// -----------------------------------------------------------------------


// XXX Change to parameter REG = 0;

/*
 * Opcodes.
 * XXX Maybe prefix with OP_
 */
`define REG      0  // XXX Rename this SPECIAL
`define REGIMM   1
`define J        2
`define JAL      3
`define BEQ      4
`define BNE      5
`define BLEZ     6
`define BGTZ     7
`define ADDI     8
`define ADDIU    9
`define SLTI    10
`define SLTIU   11
`define ANDI    12
`define ORI     13
`define XORI    14
`define LUI     15
`define CP0     16
`define CP1     17
`define CP2     18
`define CP1X    19
`define BBQL    20

`define BGTZL   23
`define RDHWR   31

`define LB      32
`define LH      33
`define LWL     34
`define LW      35
`define LBU     36
`define LHU     37
`define LWR     38

`define SB      40
`define SH      41
`define SWL     42
`define SW      43

`define SWR     46
`define CACHE   47

`define LL      48
`define LWC1    49
`define LWC2    50
`define PREF    51

`define LDC1    53
`define LDC2    54

`define SC      56
`define SWC1    57
`define SWC2    58

`define SDC1    61
`define SDC2    62

/*
 * Funct - subcodes of REG
 *
 * XXX maybe prefx with FN_ or REG_
 */
`define SLL      0

`define SRL      2
`define SRA      3
`define SLLV     4

`define SRLV     6
`define SRAV     7
`define JR       8
`define JALR     9

`define SYSCALL 12
`define BREAK   13

`define MFHI    16
`define MTHI    17
`define MFLO    18
`define MTLO    19

`define MULT    24
`define MULTU   25
`define DIV     26
`define DIVU    27

`define ADD     32
`define ADDU    33
`define SUB     34
`define SUBU    35
`define AND     36
`define OR      37
`define XOR     38
`define NOR     39

`define SLT     42
`define SLTU    43

/* REGIMM subcodes. */
`define BLTZ     0
`define BGEZ     1
`define BLTZAL  16
`define BGEZAL  17
`define SYNCI   31

/*
 * Coprocessor 0 register names
 */
`define CP0_INDEX 0
`define CP0_RANDOM 1
`define CP0_ENTRYLO0 2
`define CP0_ENTRYLO1 3
`define CP0_CONF 3
`define CP0_CONTEXT 4
`define CP0_PAGEMASK 5
`define CP0_WIRED 6
`define CP0_INFO 7
`define CP0_BADVADDR 8
`define CP0_COUNT 9
`define CP0_ENTRYHI 10
`define CP0_COMPARE 11
`define CP0_STATUS 12
`define CP0_CAUSE 13
`define CP0_EPC 14
`define CP0_PRID 15
`define CP0_CONFIG 16
`define CP0_LLADDR 17
`define CP0_WATCHLO 18
`define CP0_WATCHHI 19
`define CP0_XCONTEXT 20
`define CP0_FRAMEMASK 21
`define CP0_DIAGNOSTIC 22
`define CP0_DEBUG 23
`define CP0_DEPC 24
`define CP0_PERFORMANCE 25
`define CP0_ECC 26
`define CP0_CACHEERR 27
`define CP0_TAGLO 28
`define CP0_TAGHI 29
`define CP0_ERROREPC 30
`define CP0_DESAVE 31

/* CP0 instructions. */

`define C0_TLBR   1
`define C0_TLBWI  2
`define C0_TLBWR  6
`define C0_TLBP   8
`define C0_ERET  24
`define C0_DERET 31
`define C0_WAIT  32

/* CP0 Status reg. fields. */
`define CP0_STATUS_IE  0     // Interrupt enable
`define CP0_STATUS_EXL 1     // Exception level
`define CP0_STATUS_ERL 2     // Error level

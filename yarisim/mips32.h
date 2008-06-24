/*

MIPS32R2 opcode map (incomplete)

Root map: instr [31:26] (6 bit)

         0      1      2      3      4      5      6      7
00 SPECIAL REGIMM      j    jal    beq    bne   blez   bgtz
08    addi  addiu   slti  sltiu   andi    ori   xori    lui
10     cp0    cp1    cp2   cp1x   beql   bnel  blezl  bgtzl
18       -      -      -      -      -      -      -  rdhwr
20      lb     lh    lwl     lw    lbu    lhu    lwr      -
28      sb     sh    swl     sw      -      -    swr  cache
30      ll   lwc1   lwc2   pref      -   ldc1   ldc2      -
38      sc   swc1   lwc2      -      -   sdc1   sdc2      -

Reg map: instr[5:0] (6 bit)

         0      1      2      3      4      5      6      7
00     sll  movci    srl    sra   sllv      -   srlv   srav
08      jr   jalr   movz   movn syscall break      -   sync
10    mfhi   mthi   mflo   mtlo      -      -      -      -
18    mult  multu    div   divu      -      -      -      -
20     add   addu    sub   subu    and     or    xor    nor
28       -      -    slt   sltu      -      -      -      -
30     tge   tgeu    tlt   tltu    teq      -    tne      -
38       -      -      -      -      -      -      -      -

Regimm map: instr[20:16] (5 bit)

         0      1      2      3      4      5      6      7
00    bltz   bgez      -      -      -      -      -      -
08    tgei  tgeiu   tlti  tltiu   teqi      -   tnei      -
10  bltzal bgezal bltzall bgezall    -      -      -      -
18       -      -      -      -      -      -      -  synci

 */




typedef enum root_map {
        SPECIAL =0x00, REGIMM, J, JAL, BEQ, BNE, BLEZ, BGTZ,
        ADDI=0x08, ADDIU, SLTI, SLTIU, ANDI, ORI, XORI, LUI,
        CP0 =0x10, CP1, CP2, CP1X, BEQL, BNEL, BLEZL, BGTZL,
                                                 RDHWR = 0x1f,
        LB  =0x20, LH, LWL, LW, LBU, LHU, LWR,
        SB  =0x28, SH, SWL, SW, SWR =0x2e, CACHE,
        LL  =0x30, LWC1, LWC2, PREF, LDC1=0x35, LDC2,
        SC  =0x38, SWC1, SWC2,       SDC1=0x3d, SDC2,
} root_map_t;

typedef enum reg_map {
        SLL =0x00, SRL = 2, SRA, SLLV, SRLV = 6, SRAV,
        JR  =0x08, JALR, SYSCALL = 0x0c, BREAK = 0x0d,
        MFHI=0x10, MTHI, MFLO, MTLO,
        MULT=0x18, MULTU, DIV, DIVU,
        ADD =0x20, ADDU, SUB, SUBU, AND, OR, XOR, NOR,
        SLT =0x2a, SLTU,
        TEQ =0x34
} reg_map_t;

typedef enum regimm_map {
        BLTZ   = 0x00,
        BGEZ   = 0x01,
        BLTZAL = 0x10,
        BGEZAL = 0x11,
        SYNCI  = 0x1f
} regimm_map_t;


enum cp1_sdw_map {
        CP1_ADD, CP1_SUB, CP1_MUL, CP1_DIV, CP1_SQRT, CP1_ABS, CP1_MOV, CP1_NEG,
        CP1_ROUND_W = 12, CP1_TRUNC_W, CP1_CEIL_W, CP1_FLOOR_W,
        CP1_MOVZ, CP1_MOVN,
        // etc ...
};

/*
  move from coprocessor 0
  cop0=#10:6  MF=#0:5  rt:5 rd:5  0:8  sel:2

  move to coprocessor 0
  cop0=#10:6  MT=#4:5  rt:5 rd:5  0:8  sel:2

  exception return
  cop0=#10:6  co:1  0:19  ERET=#18:6

  debug exception return
  cop0=#10:6  co:1  0:19  DERET=#1F:6

  probe tlb for matching entry
  cop0=#10:6  co:1  0:19  TLBP=#08:6

  read indexed tlb entry
  cop0=#10:6  co:1  0:19  TLBR=#01:6

  write indexed tlb entry
  cop0=#10:6  co:1  0:19  TLBWI=#02:6

  write random tlb entry
  cop0=#10:6  co:1  0:19  TLBWR=#06:6

  enter standby mode
  cop0=#10:6  co:1  impl.dep.:19  WAIT=#20:6

 */
typedef enum c0_map {
        C0_TLBR  = 0x01,
        C0_TLBWI = 0x02,
        C0_TLBWR = 0x06,
        C0_TLBP  = 0x08,
        C0_ERET  = 0x18,
        C0_DERET = 0x1F,
        C0_WAIT  = 0x20,
} c0_map_t;

typedef union mips_instruction {
        struct {
                reg_map_t  funct  :  6;
                unsigned   sa     :  5;
                unsigned   rd     :  5;
                unsigned   rt     :  5;
                unsigned   rs     :  5;
                root_map_t opcode :  6;
        } r;
        struct {
                int        imm    : 16;
                unsigned   rt     :  5;
                unsigned   rs     :  5;
                root_map_t opcode :  6;
        } i;
        struct {
                unsigned   imm    : 16;
                unsigned   rt     :  5;
                unsigned   rs     :  5;
                root_map_t opcode :  6;
        } u;
        struct {
                unsigned   offset : 26;
                root_map_t opcode :  6;
        } j;
        u_int32_t raw;
} inst_t;


/* Coprocessor 0 */

union cp0_context_reg {
        struct {
                unsigned pad:      4; // LSB 0
                unsigned badVPN2: 21;
                unsigned PTEbase:  7; // MSB 31
        };
        unsigned raw;
};

union cp0_status_reg {
        struct {
                // LSB 0
                unsigned ie:  1;     // Interrupt enable
                unsigned exl: 1;     // Exception level
                unsigned erl: 1;     // Error level
                enum { KSU_KERNEL,
                       KSU_SUPERVISOR,
                       KSU_USER } ksu: 2; // Operating mode
                unsigned ux:  1;     // Not used
                unsigned sx:  1;     // Not used
                unsigned kx:  1;     // Not used
                unsigned im:  8;     // Interrupt Mask
                unsigned ds_de:  1;  // Not used
                unsigned ds_ce:  1;  // Not used
                unsigned ds_ch:  1;  // CP0 condition bit
                unsigned ds_pad1:1;
                unsigned ds_sr:  1;  // Soft Reset or NMI occured
                unsigned ds_ts:  1;  // TLB shutdown
                unsigned ds_bev: 1;  // Bootstrap TLB Refill Vector
                unsigned ds_pad2:2;
                unsigned re:  1;
                unsigned res1:2;
                unsigned cu0: 1;
                unsigned res2:3;
                // MSB 31
        };
        unsigned raw;
};

union cp0_cause_reg {
        struct {
                // LSB 0
                unsigned pad1:     2;
                enum { EXC_INT,        // Interrupt
                       EXC_MOD,        // TBL Modified
                       EXC_TLBL,       // TBL Refill (load & fetch)
                       EXC_TLBS,       // TBL Modified (store)
                       EXC_ADEL,       // Address Error (load & fetch)
                       EXC_ADES,       // Address Error (store)
                       EXC_IBE,        // Bus Error (fetch)
                       EXC_DBE,        // Bus Error (load & store)
                       EXC_SYS,        // System Call
                       EXC_BP,         // Breakpoint
                       EXC_RI,         // Reserved Instruction
                       EXC_CPU,        // Coprocessor Unusable
                       EXC_OV,         // Integer Overflow
                       EXC_TR,         // Trap
                       EXC_WATCH = 23  // Watch
                } exc_code: 5;  // Exception code
                unsigned pad2:     1;
                unsigned ip:       8;  // Interrupt pending
                unsigned pad3:    12;
                unsigned ce:       2;  // Coprocessor # of unusable exception
                unsigned pad4:     1;
                unsigned bd:       1;  // Exception in a branch delay slot
                // MSB 31
        };
        unsigned raw;
};





/* Based on mipsregs.h in the linux distributions */

#define _ULCAST_ (unsigned long)

/*
 * Coprocessor 0 register names
 */
#define CP0_INDEX 0
#define CP0_RANDOM 1
#define CP0_ENTRYLO0 2
#define CP0_ENTRYLO1 3
#define CP0_CONF 3
#define CP0_CONTEXT 4
#define CP0_PAGEMASK 5
#define CP0_WIRED 6
#define CP0_INFO 7
#define CP0_BADVADDR 8
#define CP0_COUNT 9
#define CP0_ENTRYHI 10
#define CP0_COMPARE 11
#define CP0_STATUS 12
#define CP0_CAUSE 13
#define CP0_EPC 14
#define CP0_PRID 15
#define CP0_CONFIG 16
#define CP0_LLADDR 17
#define CP0_WATCHLO 18
#define CP0_WATCHHI 19
#define CP0_XCONTEXT 20
#define CP0_FRAMEMASK 21
#define CP0_DIAGNOSTIC 22
#define CP0_DEBUG 23
#define CP0_DEPC 24
#define CP0_PERFORMANCE 25
#define CP0_ECC 26
#define CP0_CACHEERR 27
#define CP0_TAGLO 28
#define CP0_TAGHI 29
#define CP0_ERROREPC 30
#define CP0_DESAVE 31

/*
 * R4x00 interrupt enable / cause bits
 */
#define IE_SW0          (_ULCAST_(1) <<  8)
#define IE_SW1          (_ULCAST_(1) <<  9)
#define IE_IRQ0         (_ULCAST_(1) << 10)
#define IE_IRQ1         (_ULCAST_(1) << 11)
#define IE_IRQ2         (_ULCAST_(1) << 12)
#define IE_IRQ3         (_ULCAST_(1) << 13)
#define IE_IRQ4         (_ULCAST_(1) << 14)
#define IE_IRQ5         (_ULCAST_(1) << 15)

/*
 * R4x00 interrupt cause bits
 */
#define C_SW0           (_ULCAST_(1) <<  8)
#define C_SW1           (_ULCAST_(1) <<  9)
#define C_IRQ0          (_ULCAST_(1) << 10)
#define C_IRQ1          (_ULCAST_(1) << 11)
#define C_IRQ2          (_ULCAST_(1) << 12)
#define C_IRQ3          (_ULCAST_(1) << 13)
#define C_IRQ4          (_ULCAST_(1) << 14)
#define C_IRQ5          (_ULCAST_(1) << 15)

/*
 * Bitfields in the R4xx0 cp0 status register
 */
#define ST0_IE                  0x00000001
#define ST0_EXL                 0x00000002
#define ST0_ERL                 0x00000004
#define ST0_KSU                 0x00000018
#  define KSU_USER              0x00000010
#  define KSU_SUPERVISOR        0x00000008
#  define KSU_KERNEL            0x00000000
#define ST0_UX                  0x00000020
#define ST0_SX                  0x00000040
#define ST0_KX                  0x00000080
#define ST0_DE                  0x00010000
#define ST0_CE                  0x00020000

/*
 * Status register bits available in all MIPS CPUs.
 */
#define ST0_IM                  0x0000ff00
#define  STATUSB_IP0            8
#define  STATUSF_IP0            (_ULCAST_(1) <<  8)
#define  STATUSB_IP1            9
#define  STATUSF_IP1            (_ULCAST_(1) <<  9)
#define  STATUSB_IP2            10
#define  STATUSF_IP2            (_ULCAST_(1) << 10)
#define  STATUSB_IP3            11
#define  STATUSF_IP3            (_ULCAST_(1) << 11)
#define  STATUSB_IP4            12
#define  STATUSF_IP4            (_ULCAST_(1) << 12)
#define  STATUSB_IP5            13
#define  STATUSF_IP5            (_ULCAST_(1) << 13)
#define  STATUSB_IP6            14
#define  STATUSF_IP6            (_ULCAST_(1) << 14)
#define  STATUSB_IP7            15
#define  STATUSF_IP7            (_ULCAST_(1) << 15)
#define  STATUSB_IP8            0
#define  STATUSF_IP8            (_ULCAST_(1) <<  0)
#define  STATUSB_IP9            1
#define  STATUSF_IP9            (_ULCAST_(1) <<  1)
#define  STATUSB_IP10           2
#define  STATUSF_IP10           (_ULCAST_(1) <<  2)
#define  STATUSB_IP11           3
#define  STATUSF_IP11           (_ULCAST_(1) <<  3)
#define  STATUSB_IP12           4
#define  STATUSF_IP12           (_ULCAST_(1) <<  4)
#define  STATUSB_IP13           5
#define  STATUSF_IP13           (_ULCAST_(1) <<  5)
#define  STATUSB_IP14           6
#define  STATUSF_IP14           (_ULCAST_(1) <<  6)
#define  STATUSB_IP15           7
#define  STATUSF_IP15           (_ULCAST_(1) <<  7)
#define ST0_CH                  0x00040000
#define ST0_SR                  0x00100000
#define ST0_TS                  0x00200000
#define ST0_BEV                 0x00400000
#define ST0_RE                  0x02000000
#define ST0_FR                  0x04000000
#define ST0_CU                  0xf0000000
#define ST0_CU0                 0x10000000
#define ST0_CU1                 0x20000000
#define ST0_CU2                 0x40000000
#define ST0_CU3                 0x80000000
#define ST0_XX                  0x80000000      /* MIPS IV naming */

/*
 * Bitfields and bit numbers in the coprocessor 0 cause register.
 *
 * Refer to your MIPS R4xx0 manual, chapter 5 for explanation.
 */
#define  CAUSEB_EXCCODE         2
#define  CAUSEF_EXCCODE         (_ULCAST_(31)  <<  2)
#define  CAUSEB_IP              8
#define  CAUSEF_IP              (_ULCAST_(255) <<  8)
#define  CAUSEB_IP0             8
#define  CAUSEF_IP0             (_ULCAST_(1)   <<  8)
#define  CAUSEB_IP1             9
#define  CAUSEF_IP1             (_ULCAST_(1)   <<  9)
#define  CAUSEB_IP2             10
#define  CAUSEF_IP2             (_ULCAST_(1)   << 10)
#define  CAUSEB_IP3             11
#define  CAUSEF_IP3             (_ULCAST_(1)   << 11)
#define  CAUSEB_IP4             12
#define  CAUSEF_IP4             (_ULCAST_(1)   << 12)
#define  CAUSEB_IP5             13
#define  CAUSEF_IP5             (_ULCAST_(1)   << 13)
#define  CAUSEB_IP6             14
#define  CAUSEF_IP6             (_ULCAST_(1)   << 14)
#define  CAUSEB_IP7             15
#define  CAUSEF_IP7             (_ULCAST_(1)   << 15)
#define  CAUSEB_IV              23
#define  CAUSEF_IV              (_ULCAST_(1)   << 23)
#define  CAUSEB_CE              28
#define  CAUSEF_CE              (_ULCAST_(3)   << 28)
#define  CAUSEB_BD              31
#define  CAUSEF_BD              (_ULCAST_(1)   << 31)

/*
 * Bits in the coprocessor 0 config register.
 */
/* Generic bits.  */
#define CONF_CM_CACHABLE_NO_WA          0
#define CONF_CM_CACHABLE_WA             1
#define CONF_CM_UNCACHED                2
#define CONF_CM_CACHABLE_NONCOHERENT    3
#define CONF_CM_CACHABLE_CE             4
#define CONF_CM_CACHABLE_COW            5
#define CONF_CM_CACHABLE_CUW            6
#define CONF_CM_CACHABLE_ACCELERATED    7
#define CONF_CM_CMASK                   7
#define CONF_BE                 (_ULCAST_(1) << 15)

/* Bits common to various processors.  */
#define CONF_CU                 (_ULCAST_(1) <<  3)
#define CONF_DB                 (_ULCAST_(1) <<  4)
#define CONF_IB                 (_ULCAST_(1) <<  5)
#define CONF_DC                 (_ULCAST_(7) <<  6)
#define CONF_IC                 (_ULCAST_(7) <<  9)
#define CONF_EB                 (_ULCAST_(1) << 13)
#define CONF_EM                 (_ULCAST_(1) << 14)
#define CONF_SM                 (_ULCAST_(1) << 16)
#define CONF_SC                 (_ULCAST_(1) << 17)
#define CONF_EW                 (_ULCAST_(3) << 18)
#define CONF_EP                 (_ULCAST_(15)<< 24)
#define CONF_EC                 (_ULCAST_(7) << 28)
#define CONF_CM                 (_ULCAST_(1) << 31)

/* Bits specific to the VR41xx.  */
#define VR41_CONF_CS            (_ULCAST_(1) << 12)
#define VR41_CONF_M16           (_ULCAST_(1) << 20)
#define VR41_CONF_AD            (_ULCAST_(1) << 23)



/* All the architectual state */
typedef struct MIPS_state {
        /* CPU Registers */
        u_int32_t r[32];
        u_int32_t hi, lo;
        u_int32_t pc;

        /* FPU Registers */
        u_int32_t f[32];
        u_int32_t fcr0, fcr25, fcr26, fcr28, fcsr;

        /* CP0 State */
        u_int32_t cp0r[32];
//      union cp0_context_reg cp0_context = { .raw = 0 };
//      unsigned cp0_badvaddr = 0;
//      unsigned cp0_count    = 0;
//      unsigned cp0_compare  = 0;
        union cp0_status_reg cp0_status;
        union cp0_cause_reg  cp0_cause;
        u_int32_t epc;

} MIPS_state_t;


/* Decode map */

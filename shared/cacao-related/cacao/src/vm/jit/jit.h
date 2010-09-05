/* src/vm/jit/jit.h - code generation header

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

   $Id: jit.h 7742 2007-04-17 20:37:36Z edwin $

*/


#ifndef _JIT_H
#define _JIT_H

/* forward typedefs ***********************************************************/

typedef struct jitdata jitdata;
typedef struct stackelement stackelement;
typedef stackelement *stackptr;
typedef struct basicblock basicblock;
typedef struct instruction instruction;
typedef struct insinfo_inline insinfo_inline;
typedef struct exception_entry exception_entry;


#include "config.h"
#include "vm/types.h"

#include "toolbox/chain.h"

#include "vm/global.h"
#include "vm/resolve.h"

#include "vm/jit/codegen-common.h"
#include "vm/jit/reg.h"
#include "vm/jit/replace.h"

#if defined(ENABLE_INLINING)
# include "vm/jit/inline/inline.h"
#endif

#if defined(ENABLE_LOOP)
# include "vm/jit/loop/loop.h"
#endif
#if defined(ENABLE_SSA) 
# include "vm/jit/optimizing/lsra.h"
#endif
#if defined(ENABLE_LSRA)
# include "vm/jit/allocator/lsra.h"
#endif

#include "vm/jit/verify/typeinfo.h"

#include "vmcore/method.h"
#include "vmcore/references.h"

#if defined(ENABLE_STATISTICS)
# include "vmcore/statistics.h"
#endif


/* common jit/codegen macros **************************************************/

#if defined(ENABLE_STATISTICS)
# define COUNT(x)        (x)++
# define COUNT_SPILLS    count_spills++
#else
# define COUNT(x)        /* nothing */
# define COUNT_SPILLS    /* nothing */
#endif

typedef struct interface_info interface_info;

struct interface_info {
	s4 flags;
	s4 regoff;
};


/* jitdata ********************************************************************/

struct jitdata {
	methodinfo      *m;               /* methodinfo of the method compiled    */
	codeinfo        *code;
	codegendata     *cd;
	registerdata    *rd;
#if defined(ENABLE_LOOP)
	loopdata        *ld;
#endif
#if defined(ENABLE_SSA) || defined(ENABLE_LSRA)
	lsradata        *ls;
#endif

	u4               flags;           /* contains JIT compiler flags          */
	bool             isleafmethod;    /* true, if no subroutines are called   */

	instruction     *instructions;    /* ICMDs, valid between parse and stack */
	basicblock      *basicblocks;     /* start of basic block list            */
	stackelement    *stack;           /* XXX should become stack.c internal   */
	s4               instructioncount;/* XXX remove this?                     */
	s4               basicblockcount; /* number of basic blocks               */
	s4               stackcount;      /* number of stackelements to allocate  */
                                      /* (passed from parse to stack)         */

	varinfo         *var;             /* array of variables                   */
	s4               vartop;          /* next free index in var array         */

	s4               varcount;        /* number of variables in var array     */
	s4               localcount;      /* number of locals at start of var ar. */
    s4              *local_map;       /* map for renaming (de-coallescing)    */
					 /* locals and keeping the coalescing info for simplereg. */
	                 /* local_map[javaindex * 5 + type] =                     */
	                 /*     >= 0......index into jd->var, or                  */
					 /*     UNUSED....this (javaindex,type) pair is not used  */

	s4               maxlocals;       /* max. number of javalocals            */

	interface_info  *interface_map;   /* interface variables (for simplereg)  */
	s4               maxinterfaces;   /* max. number of interface variables   */

	s4               exceptiontablelength; /* exceptiontable length           */
	exception_entry *exceptiontable;       /* the exceptiontable              */

	basicblock      *returnblock;          /* block containing the *RETURN    */
	                                       /* (only use if returncount==1)    */
	s4               returncount;          /* number of return instructions   */
	bool             branchtoentry;        /* true if first block is a target */
	bool             branchtoend;          /* true if end dummy is a target   */
};


#define UNUSED                     -1

#define JITDATA_FLAG_PARSE               0x00000001
#define JITDATA_FLAG_VERIFY              0x00000002

#define JITDATA_FLAG_INSTRUMENT          0x00000004

#define JITDATA_FLAG_IFCONV              0x00000008
#define JITDATA_FLAG_REORDER             0x00000010
#define JITDATA_FLAG_INLINE              0x00000020

#define JITDATA_FLAG_COUNTDOWN           0x00000100

#define JITDATA_FLAG_SHOWINTERMEDIATE    0x20000000
#define JITDATA_FLAG_SHOWDISASSEMBLE     0x40000000
#define JITDATA_FLAG_VERBOSECALL         0x80000000


#define JITDATA_HAS_FLAG_PARSE(jd) \
    ((jd)->flags & JITDATA_FLAG_PARSE)

#define JITDATA_HAS_FLAG_VERIFY(jd) \
    ((jd)->flags & JITDATA_FLAG_VERIFY)

#define JITDATA_HAS_FLAG_INSTRUMENT(jd) \
    ((jd)->flags & JITDATA_FLAG_INSTRUMENT)

#define JITDATA_HAS_FLAG_IFCONV(jd) \
    ((jd)->flags & JITDATA_FLAG_IFCONV)

#define JITDATA_HAS_FLAG_REORDER(jd) \
    ((jd)->flags & JITDATA_FLAG_REORDER)

#define JITDATA_HAS_FLAG_INLINE(jd) \
    ((jd)->flags & JITDATA_FLAG_INLINE)

#define JITDATA_HAS_FLAG_COUNTDOWN(jd) \
    ((jd)->flags & JITDATA_FLAG_COUNTDOWN)

#define JITDATA_HAS_FLAG_SHOWINTERMEDIATE(jd) \
    ((jd)->flags & JITDATA_FLAG_SHOWINTERMEDIATE)

#define JITDATA_HAS_FLAG_SHOWDISASSEMBLE(jd) \
    ((jd)->flags & JITDATA_FLAG_SHOWDISASSEMBLE)

#define JITDATA_HAS_FLAG_VERBOSECALL(jd) \
    ((jd)->flags & JITDATA_FLAG_VERBOSECALL)


/* macros for accessing variables *********************************************
 
   Use VAROP for s1, s2, s3 and dst operands (eg. VAROP(iptr->s1)),
   use VAR if you have the variable index (eg. VAR(iptr->sx.s23.s2.args[0])).

******************************************************************************/

#define VAROP(v) (jd->var + (v).varindex)
#define VAR(i)   (jd->var + (i))


/* exception_entry ************************************************************/

struct exception_entry {
	basicblock           *start;
	basicblock           *end;
	basicblock           *handler;
	classref_or_classinfo catchtype; /* catchtype of exc. (NULL == catchall)  */
	exception_entry      *next;      /* next in list of exceptions when       */
									 /* loops are copied                      */
	exception_entry      *down;      /* next exception_entry                  */
};


/* stack element structure ****************************************************/

/* flags */

#define SAVEDVAR      1         /* variable has to survive method invocations */
#define INMEMORY      2         /* variable stored in memory                  */
#define SAVREG        4         /* allocated to a saved register              */
#define ARGREG        8         /* allocated to an arg register               */
#define PASSTHROUGH  32         /* stackslot was passed-through by an ICMD    */
#define PREALLOC     64         /* preallocated var like for ARGVARS. Used    */
                                /* with the new var system */
#define INOUT    128            /* variable is an invar or/and an outvar      */

#define IS_SAVEDVAR(x)    ((x) & SAVEDVAR)
#define IS_INMEMORY(x)    ((x) & INMEMORY)


/* variable kinds */

#define UNDEFVAR   0            /* stack slot will become temp during regalloc*/
#define TEMPVAR    1            /* stack slot is temp register                */
#define STACKVAR   2            /* stack slot is numbered stack slot          */
#define LOCALVAR   3            /* stack slot is local variable               */
#define ARGVAR     4            /* stack slot is argument variable            */


struct stackelement {
	stackptr prev;              /* pointer to next element towards bottom     */
	instruction *creator;       /* instruction that created this element      */
	s4       type;              /* slot type of stack element                 */
	s4       flags;             /* flags (SAVED, INMEMORY)                    */
	s4       varkind;           /* kind of variable or register               */
	s4       varnum;            /* number of variable                         */
};


/**************************** instruction structure ***************************/

/* branch_target_t: used in TABLESWITCH tables */

typedef union {
    s4                         insindex; /* used in parse                     */
    basicblock                *block;    /* valid after parse                 */
} branch_target_t;

/* lookup_target_t: used in LOOKUPSWITCH tables */

typedef struct {
    s4                         value;    /* case value                        */
    branch_target_t            target;   /* branch target, see above          */
} lookup_target_t;

/*** s1 operand ***/

typedef union {
	s4                         varindex;
    s4                         argcount;
} s1_operand_t;

/*** s2 operand ***/

typedef union {
	s4                         varindex;
	s4                        *args;
    classref_or_classinfo      c;
    unresolved_class          *uc;
    ptrint                     constval;         /* for PUT*CONST             */
    s4                         tablelow;         /* for TABLESWITCH           */
    u4                         lookupcount;      /* for LOOKUPSWITCH          */
	s4                         retaddrnr;        /* for ASTORE                */
} s2_operand_t;

/*** s3 operand ***/

typedef union {
	s4                         varindex;
    ptrint                     constval;
    classref_or_classinfo      c;
    constant_FMIref           *fmiref;
    unresolved_method         *um;
    unresolved_field          *uf;
    insinfo_inline            *inlineinfo;       /* for INLINE_START/END      */
    s4                         tablehigh;        /* for TABLESWITCH           */
    branch_target_t            lookupdefault;    /* for LOOKUPSWITCH          */
    branch_target_t            jsrtarget;        /* for JSR                   */
	s4                         javaindex;        /* for *STORE                */
    struct builtintable_entry *bte;
} s3_operand_t;

/*** val operand ***/

typedef union {
    s4                        i;
    s8                        l;
    float                     f;
    double                    d;
    void                     *anyptr;
    java_objectheader        *stringconst;       /* for ACONST with string    */
    classref_or_classinfo     c;                 /* for ACONST with class     */
} val_operand_t;

/*** dst operand ***/

typedef union {
	s4                         varindex;
    basicblock                *block;       /* valid after parse              */
    branch_target_t           *table;       /* for TABLESWITCH                */
    lookup_target_t           *lookup;      /* for LOOKUPSWITCH               */
    s4                         insindex;    /* used in parse                  */
} dst_operand_t;

/*** flags (32 bits) ***/

#define INS_FLAG_BASICBLOCK    0x01    /* marks a basic block start           */
#define INS_FLAG_UNRESOLVED    0x02    /* contains unresolved field/meth/class*/
#define INS_FLAG_CLASS         0x04    /* for ACONST, PUT*CONST with class    */
#define INS_FLAG_ARRAY         0x08    /* for CHECKCAST/INSTANCEOF with array */
#define INS_FLAG_CHECK         0x10    /* for *ALOAD|*ASTORE: check index     */
                                       /* for BUILTIN: check exception        */
#define INS_FLAG_KILL_PREV     0x04    /* for *STORE, invalidate prev local   */
#define INS_FLAG_KILL_NEXT     0x08    /* for *STORE, invalidate next local   */
#define INS_FLAG_RETADDR       0x10    /* for ASTORE: op is a returnAddress   */

#define INS_FLAG_ID_SHIFT      5
#define INS_FLAG_ID_MASK       (~0 << INS_FLAG_ID_SHIFT)

typedef union {
    u4                  bits;
} flags_operand_t;

/*** instruction ***/

/* The instruction format for the intermediate representation: */

struct instruction {
    u2                      opc;    /* opcode       */
    u2                      line;   /* line number  */
#if SIZEOF_VOID_P == 8
    flags_operand_t         flags;  /* 4 bytes      */
#endif
    s1_operand_t            s1;     /* pointer-size */
    union {
        struct {
            s2_operand_t    s2;     /* pointer-size */
            s3_operand_t    s3;     /* pointer-size */
        } s23;                      /*     XOR      */
        val_operand_t       val;    /*  long-size   */
    } sx;
    dst_operand_t           dst;    /* pointer-size */
#if SIZEOF_VOID_P == 4
    flags_operand_t         flags;  /* 4 bytes      */
#endif
};


#define INSTRUCTION_STARTS_BASICBLOCK(iptr) \
	((iptr)->flags.bits & INS_FLAG_BASICBLOCK)

#define INSTRUCTION_IS_RESOLVED(iptr) \
	(!((iptr)->flags.bits & INS_FLAG_UNRESOLVED))

#define INSTRUCTION_IS_UNRESOLVED(iptr) \
	((iptr)->flags.bits & INS_FLAG_UNRESOLVED)

#define INSTRUCTION_MUST_CHECK(iptr) \
	((iptr)->flags.bits & INS_FLAG_CHECK)

#define INSTRUCTION_GET_FIELDREF(iptr,fref) \
	do { \
		if (iptr->flags.bits & INS_FLAG_UNRESOLVED) \
			fref = iptr->sx.s23.s3.uf->fieldref; \
		else \
			fref = iptr->sx.s23.s3.fmiref; \
	} while (0)

#define INSTRUCTION_GET_METHODREF(iptr,mref) \
	do { \
		if (iptr->flags.bits & INS_FLAG_UNRESOLVED) \
			mref = iptr->sx.s23.s3.um->methodref; \
		else \
			mref = iptr->sx.s23.s3.fmiref; \
	} while (0)

#define INSTRUCTION_GET_METHODDESC(iptr, md) \
	do { \
		if (iptr->flags.bits & INS_FLAG_UNRESOLVED) \
			md = iptr->sx.s23.s3.um->methodref->parseddesc.md; \
		else \
			md = iptr->sx.s23.s3.fmiref->parseddesc.md; \
	} while (0)


/* additional info structs for special instructions ***************************/

/* for ICMD_INLINE_START and ICMD_INLINE_END */

struct insinfo_inline {
	/* fields copied from the inlining tree ----------------------------------*/
	insinfo_inline *parent;     /* insinfo of the surrounding inlining, if any*/
	methodinfo     *method;     /* the inlined method starting/ending here    */
	methodinfo     *outer;      /* the outer method suspended/resumed here    */
	s4              synclocal;      /* local index used for synchronization   */
	bool            synchronize;    /* true if synchronization is needed      */
	s4              throughcount;   /* total # of pass-through variables      */
	s4              paramcount;     /* number of parameters of original call  */
	s4              stackvarscount; /* source stackdepth at INLINE_START      */
	s4             *stackvars;      /* stack vars at INLINE_START             */

	/* fields set by inlining ------------------------------------------------*/
	s4         *javalocals_start; /* javalocals at start of inlined body      */
	s4         *javalocals_end;   /* javalocals after inlined body            */

	/* fields set by replacement point creation ------------------------------*/
#if defined(ENABLE_REPLACEMENT)
	rplpoint   *rp;             /* replacement point at INLINE_START          */
#endif

	/* fields set by the codegen ---------------------------------------------*/
	s4          startmpc;       /* machine code offset of start of inlining   */
};


/* basicblock *****************************************************************/

/* flags */

#define BBDELETED            -2
#define BBUNDEF              -1
#define BBREACHED            0
#define BBFINISHED           1

#define BBTYPECHECK_UNDEF    2
#define BBTYPECHECK_REACHED  3

#define BBTYPE_STD           0  /* standard basic block type                  */
#define BBTYPE_EXH           1  /* exception handler basic block type         */
#define BBTYPE_SBR           2  /* subroutine basic block type                */

#define BBFLAG_REPLACEMENT   0x01  /* put a replacement point at the start    */

/* XXX basicblock wastes quite a lot of memory by having four flag fields     */
/* (flags, bitflags, type and lflags). Probably the last three could be       */
/* combined without loss of efficiency. The first one could be combined with  */
/* the others by using bitfields.                                             */

/* XXX "flags" should probably be called "state", as it is an integer state   */

struct basicblock {
	s4            nr;           /* basic block number                         */
	s4            flags;        /* used during stack analysis, init with -1   */
	s4            bitflags;     /* OR of BBFLAG_... constants, init with 0    */
	s4            type;         /* basic block type (std, xhandler, subroutine*/
	s4            lflags;       /* used during loop copying, init with 0	  */

	s4            icount;       /* number of intermediate code instructions   */
	instruction  *iinstr;       /* pointer to intermediate code instructions  */

	varinfo      *inlocals;     /* copy of locals on block entry              */
	s4           *javalocals;   /* map from java locals to cacao variables[+] */
	s4           *invars;       /* array of in-variables at begin of block    */
	s4           *outvars;      /* array of out-variables at end of block     */
	s4            indepth;      /* stack depth at begin of basic block        */
	s4            outdepth;     /* stack depth end of basic block             */
	s4            varstart;     /* index of first non-invar block variable    */
	s4            varcount;     /* number of non-invar block variables        */

	s4            predecessorcount;
	s4            successorcount;
	basicblock  **predecessors; /* array of predecessor basic blocks          */
	basicblock  **successors;   /* array of successor basic blocks            */

	branchref    *branchrefs;   /* list of branches to be patched             */

	basicblock   *next;         /* used to build a BB list (instead of array) */
	basicblock   *copied_to;    /* points to the copy of this basic block	  */
                                /* when loop nodes are copied                 */
	basicblock   *original;     /* block of which this block is a clone       */
	                            /* NULL for the original block itself         */
	methodinfo   *method;       /* method this block belongs to               */
	insinfo_inline *inlineinfo; /* inlineinfo for the start of this block     */

	s4            mpc;          /* machine code pc at start of block          */
};

/* [+]...the javalocals array: This array is indexed by the javaindex (the    */
/*       local variable index ocurring in the original bytecode). An element  */
/*       javalocals[javaindex] encodes where to find the contents of the      */
/*       original variable at this point in the program.                      */
/*       There are three cases for javalocals[javaindex]:                     */
/*           >= 0.......it's an index into the jd->var array, where the       */
/*                      CACAO variable corresponding to the original local    */
/*                      can be found.                                         */
/*           UNUSED.....the original variable is not live at this point       */
/*           < UNUSED...the original variable contains a returnAddress at     */
/*                      this point. The number of the block to return to can  */
/*                      be calculated using RETADDR_FROM_JAVALOCAL:           */
/*                                                                            */
/*                      javalocals[javaindex] == JAVALOCAL_FROM_RETADDR(nr)   */
/*                      RETADDR_FROM_JAVALOCAL(javalocals[javaindex]) == nr   */

#define JAVALOCAL_FROM_RETADDR(nr)  (UNUSED - (1 + (nr)))
#define RETADDR_FROM_JAVALOCAL(jl)  (UNUSED - (1 + (jl)))


/* Macro for initializing newly allocated basic block's. It does not
   need to zero fields, as we zero out the whole basic block array. */

#define BASICBLOCK_INIT(bptr,m)                        \
	do {                                               \
		bptr->mpc    = -1;                             \
		bptr->flags  = -1;                             \
		bptr->type   = BBTYPE_STD;                     \
		bptr->method = (m);                            \
	} while (0)
			

/* data-flow constants for the ICMD table ************************************/

#define DF_0_TO_0      0
#define DF_1_TO_0      1
#define DF_2_TO_0      2
#define DF_3_TO_0      3

#define DF_DST_BASE    4      /* from this value on, iptr->dst is a variable */

#define DF_0_TO_1      (DF_DST_BASE + 0)
#define DF_1_TO_1      (DF_DST_BASE + 1)
#define DF_2_TO_1      (DF_DST_BASE + 2)
#define DF_3_TO_1      (DF_DST_BASE + 3)
#define DF_N_TO_1      (DF_DST_BASE + 4)

#define DF_INVOKE      (DF_DST_BASE + 5)
#define DF_BUILTIN     (DF_DST_BASE + 6)

#define DF_COPY        (DF_DST_BASE + 7)
#define DF_MOVE        (DF_DST_BASE + 8)

#define DF_DUP         -1
#define DF_DUP_X1      -1
#define DF_DUP_X2      -1
#define DF_DUP2        -1
#define DF_DUP2_X1     -1
#define DF_DUP2_X2     -1
#define DF_SWAP        -1

/* special data-flow recognized by verify/generate.pl: */
#define DF_LOAD        DF_COPY
#define DF_STORE       DF_MOVE
#define DF_IINC        DF_1_TO_1
#define DF_POP         DF_1_TO_0
#define DF_POP2        DF_2_TO_0


/* control-flow constants for the ICMD table *********************************/

#define CF_NORMAL      0
#define CF_IF          1

#define CF_END_BASE    2  /* from here on, they mark the end of a superblock */

#define CF_END         (CF_END_BASE + 0)
#define CF_GOTO        (CF_END_BASE + 1)
#define CF_TABLE       (CF_END_BASE + 2)
#define CF_LOOKUP      (CF_END_BASE + 3)
#define CF_JSR         (CF_END_BASE + 4)
#define CF_RET         (CF_END_BASE + 5)


/* flag constants for the ICMD table *****************************************/

#define ICMDTABLE_PEI    0x0001               /* ICMD may throw an exception */
#define ICMDTABLE_CALLS  0x0002     /* needs registers to be saved, may call */


/* ICMD table entry **********************************************************/

typedef struct icmdtable_entry_t icmdtable_entry_t;

struct icmdtable_entry_t {
#if !defined(NDEBUG)
	char *name;                                /* name, without ICMD_ prefix */
#endif
	s4    dataflow;                             /* a DF_ constant, see above */
	s4    controlflow;                          /* a CF_ constant, see above */
	s4    flags;                        /* a combination of ICMDTABLE_ flags */
};


/* the ICMD table ************************************************************/

extern icmdtable_entry_t icmd_table[256];


/********** JavaVM operation codes (sorted) and instruction lengths ***********/

extern char *opcode_names[256];
extern int jcommandsize[256];
extern int stackreq[256];

#define JAVA_NOP               0
#define ICMD_NOP               0

#define JAVA_ACONST_NULL       1
#define ICMD_ACONST            1        /* val.a = constant                   */

#define JAVA_ICONST_M1         2
#define ICMD_CHECKNULL         2

#define JAVA_ICONST_0          3
#define ICMD_ICONST            3        /* val.i = constant                   */

#define JAVA_ICONST_1          4

#define JAVA_ICONST_2          5
#define ICMD_IDIVPOW2          5        /* val.i = constant                   */

#define JAVA_ICONST_3          6
#define ICMD_LDIVPOW2          6        /* val.l = constant                   */

#define JAVA_ICONST_4          7

#define JAVA_ICONST_5          8

#define JAVA_LCONST_0          9
#define ICMD_LCONST            9        /* val.l = constant                   */

#define JAVA_LCONST_1         10
#define ICMD_LCMPCONST        10        /* val.l = constant                   */

#define JAVA_FCONST_0         11
#define ICMD_FCONST           11        /* val.f = constant                   */

#define JAVA_FCONST_1         12

#define JAVA_FCONST_2         13

#define JAVA_DCONST_0         14
#define ICMD_DCONST           14        /* val.d = constant                   */

#define JAVA_DCONST_1         15
#define ICMD_COPY             15

#define JAVA_BIPUSH           16
#define ICMD_MOVE             16

#define JAVA_SIPUSH           17

#define JAVA_LDC1             18

#define JAVA_LDC2             19

#define JAVA_LDC2W            20

	                                    /* order of LOAD instructions must be */
	                                    /* equal to order of TYPE_* defines   */
#define JAVA_ILOAD            21
#define ICMD_ILOAD            21        /* op1 = local variable               */

#define JAVA_LLOAD            22
#define ICMD_LLOAD            22        /* op1 = local variable               */

#define JAVA_FLOAD            23
#define ICMD_FLOAD            23        /* op1 = local variable               */

#define JAVA_DLOAD            24
#define ICMD_DLOAD            24        /* op1 = local variable               */

#define JAVA_ALOAD            25
#define ICMD_ALOAD            25        /* op1 = local variable               */

#define JAVA_ILOAD_0          26
#define ICMD_IADDCONST        26        /* val.i = constant                   */

#define JAVA_ILOAD_1          27
#define ICMD_ISUBCONST        27        /* val.i = constant                   */

#define JAVA_ILOAD_2          28
#define ICMD_IMULCONST        28        /* val.i = constant                   */

#define JAVA_ILOAD_3          29
#define ICMD_IANDCONST        29        /* val.i = constant                   */

#define JAVA_LLOAD_0          30
#define ICMD_IORCONST         30        /* val.i = constant                   */

#define JAVA_LLOAD_1          31
#define ICMD_IXORCONST        31        /* val.i = constant                   */

#define JAVA_LLOAD_2          32
#define ICMD_ISHLCONST        32        /* val.i = constant                   */

#define JAVA_LLOAD_3          33
#define ICMD_ISHRCONST        33        /* val.i = constant                   */

#define JAVA_FLOAD_0          34
#define ICMD_IUSHRCONST       34        /* val.i = constant                   */

#define JAVA_FLOAD_1          35
#define ICMD_IREMPOW2         35        /* val.i = constant                   */

#define JAVA_FLOAD_2          36
#define ICMD_LADDCONST        36        /* val.l = constant                   */

#define JAVA_FLOAD_3          37
#define ICMD_LSUBCONST        37        /* val.l = constant                   */

#define JAVA_DLOAD_0          38
#define ICMD_LMULCONST        38        /* val.l = constant                   */

#define JAVA_DLOAD_1          39
#define ICMD_LANDCONST        39        /* val.l = constant                   */

#define JAVA_DLOAD_2          40
#define ICMD_LORCONST         40        /* val.l = constant                   */

#define JAVA_DLOAD_3          41
#define ICMD_LXORCONST        41        /* val.l = constant                   */

#define JAVA_ALOAD_0          42
#define ICMD_LSHLCONST        42        /* val.l = constant                   */

#define JAVA_ALOAD_1          43
#define ICMD_LSHRCONST        43        /* val.l = constant                   */

#define JAVA_ALOAD_2          44
#define ICMD_LUSHRCONST       44        /* val.l = constant                   */

#define JAVA_ALOAD_3          45
#define ICMD_LREMPOW2         45        /* val.l = constant                   */

#define JAVA_IALOAD           46
#define ICMD_IALOAD           46

#define JAVA_LALOAD           47
#define ICMD_LALOAD           47

#define JAVA_FALOAD           48
#define ICMD_FALOAD           48

#define JAVA_DALOAD           49
#define ICMD_DALOAD           49

#define JAVA_AALOAD           50
#define ICMD_AALOAD           50

#define JAVA_BALOAD           51
#define ICMD_BALOAD           51

#define JAVA_CALOAD           52
#define ICMD_CALOAD           52

#define JAVA_SALOAD           53
#define ICMD_SALOAD           53

	                                    /* order of STORE instructions must be*/
	                                    /* equal to order of TYPE_* defines   */
#define JAVA_ISTORE           54
#define ICMD_ISTORE           54        /* op1 = local variable               */

#define JAVA_LSTORE           55
#define ICMD_LSTORE           55        /* op1 = local variable               */

#define JAVA_FSTORE           56
#define ICMD_FSTORE           56        /* op1 = local variable               */

#define JAVA_DSTORE           57
#define ICMD_DSTORE           57        /* op1 = local variable               */

#define JAVA_ASTORE           58
#define ICMD_ASTORE           58        /* op1 = local variable               */

#define JAVA_ISTORE_0         59
#define ICMD_IF_LEQ           59        /* op1 = target JavaVM pc, val.l      */

#define JAVA_ISTORE_1         60
#define ICMD_IF_LNE           60        /* op1 = target JavaVM pc, val.l      */

#define JAVA_ISTORE_2         61
#define ICMD_IF_LLT           61        /* op1 = target JavaVM pc, val.l      */

#define JAVA_ISTORE_3         62
#define ICMD_IF_LGE           62        /* op1 = target JavaVM pc, val.l      */

#define JAVA_LSTORE_0         63
#define ICMD_IF_LGT           63        /* op1 = target JavaVM pc, val.l      */

#define JAVA_LSTORE_1         64
#define ICMD_IF_LLE           64        /* op1 = target JavaVM pc, val.l      */

#define JAVA_LSTORE_2         65
#define ICMD_IF_LCMPEQ        65        /* op1 = target JavaVM pc             */

#define JAVA_LSTORE_3         66
#define ICMD_IF_LCMPNE        66        /* op1 = target JavaVM pc             */

#define JAVA_FSTORE_0         67
#define ICMD_IF_LCMPLT        67        /* op1 = target JavaVM pc             */

#define JAVA_FSTORE_1         68
#define ICMD_IF_LCMPGE        68        /* op1 = target JavaVM pc             */

#define JAVA_FSTORE_2         69
#define ICMD_IF_LCMPGT        69        /* op1 = target JavaVM pc             */

#define JAVA_FSTORE_3         70
#define ICMD_IF_LCMPLE        70        /* op1 = target JavaVM pc             */

#define JAVA_DSTORE_0         71

#define JAVA_DSTORE_1         72

#define JAVA_DSTORE_2         73

#define JAVA_DSTORE_3         74

#define JAVA_ASTORE_0         75

#define JAVA_ASTORE_1         76

#define JAVA_ASTORE_2         77

#define JAVA_ASTORE_3         78

#define JAVA_IASTORE          79
#define ICMD_IASTORE          79

#define JAVA_LASTORE          80
#define ICMD_LASTORE          80

#define JAVA_FASTORE          81
#define ICMD_FASTORE          81

#define JAVA_DASTORE          82
#define ICMD_DASTORE          82

#define JAVA_AASTORE          83
#define ICMD_AASTORE          83

#define JAVA_BASTORE          84
#define ICMD_BASTORE          84

#define JAVA_CASTORE          85
#define ICMD_CASTORE          85

#define JAVA_SASTORE          86
#define ICMD_SASTORE          86

#define JAVA_POP              87
#define ICMD_POP              87

#define JAVA_POP2             88
#define ICMD_POP2             88

#define JAVA_DUP              89
#define ICMD_DUP              89

#define JAVA_DUP_X1           90
#define ICMD_DUP_X1           90

#define JAVA_DUP_X2           91
#define ICMD_DUP_X2           91

#define JAVA_DUP2             92
#define ICMD_DUP2             92

#define JAVA_DUP2_X1          93
#define ICMD_DUP2_X1          93

#define JAVA_DUP2_X2          94
#define ICMD_DUP2_X2          94

#define JAVA_SWAP             95
#define ICMD_SWAP             95

#define JAVA_IADD             96
#define ICMD_IADD             96

#define JAVA_LADD             97
#define ICMD_LADD             97

#define JAVA_FADD             98
#define ICMD_FADD             98

#define JAVA_DADD             99
#define ICMD_DADD             99

#define JAVA_ISUB             100
#define ICMD_ISUB             100

#define JAVA_LSUB             101
#define ICMD_LSUB             101

#define JAVA_FSUB             102
#define ICMD_FSUB             102

#define JAVA_DSUB             103
#define ICMD_DSUB             103

#define JAVA_IMUL             104
#define ICMD_IMUL             104

#define JAVA_LMUL             105
#define ICMD_LMUL             105

#define JAVA_FMUL             106
#define ICMD_FMUL             106

#define JAVA_DMUL             107
#define ICMD_DMUL             107

#define JAVA_IDIV             108
#define ICMD_IDIV             108

#define JAVA_LDIV             109
#define ICMD_LDIV             109

#define JAVA_FDIV             110
#define ICMD_FDIV             110

#define JAVA_DDIV             111
#define ICMD_DDIV             111

#define JAVA_IREM             112
#define ICMD_IREM             112

#define JAVA_LREM             113
#define ICMD_LREM             113

#define JAVA_FREM             114
#define ICMD_FREM             114

#define JAVA_DREM             115
#define ICMD_DREM             115

#define JAVA_INEG             116
#define ICMD_INEG             116

#define JAVA_LNEG             117
#define ICMD_LNEG             117

#define JAVA_FNEG             118
#define ICMD_FNEG             118

#define JAVA_DNEG             119
#define ICMD_DNEG             119

#define JAVA_ISHL             120
#define ICMD_ISHL             120

#define JAVA_LSHL             121
#define ICMD_LSHL             121

#define JAVA_ISHR             122
#define ICMD_ISHR             122

#define JAVA_LSHR             123
#define ICMD_LSHR             123

#define JAVA_IUSHR            124
#define ICMD_IUSHR            124

#define JAVA_LUSHR            125
#define ICMD_LUSHR            125

#define JAVA_IAND             126
#define ICMD_IAND             126

#define JAVA_LAND             127
#define ICMD_LAND             127

#define JAVA_IOR              128
#define ICMD_IOR              128

#define JAVA_LOR              129
#define ICMD_LOR              129

#define JAVA_IXOR             130
#define ICMD_IXOR             130

#define JAVA_LXOR             131
#define ICMD_LXOR             131

#define JAVA_IINC             132
#define ICMD_IINC             132   /* op1 = local variable, val.i = constant */

#define JAVA_I2L              133
#define ICMD_I2L              133

#define JAVA_I2F              134
#define ICMD_I2F              134

#define JAVA_I2D              135
#define ICMD_I2D              135

#define JAVA_L2I              136
#define ICMD_L2I              136

#define JAVA_L2F              137
#define ICMD_L2F              137

#define JAVA_L2D              138
#define ICMD_L2D              138

#define JAVA_F2I              139
#define ICMD_F2I              139

#define JAVA_F2L              140
#define ICMD_F2L              140

#define JAVA_F2D              141
#define ICMD_F2D              141

#define JAVA_D2I              142
#define ICMD_D2I              142

#define JAVA_D2L              143
#define ICMD_D2L              143

#define JAVA_D2F              144
#define ICMD_D2F              144

#define JAVA_INT2BYTE         145
#define ICMD_INT2BYTE         145

#define JAVA_INT2CHAR         146
#define ICMD_INT2CHAR         146

#define JAVA_INT2SHORT        147
#define ICMD_INT2SHORT        147

#define JAVA_LCMP             148
#define ICMD_LCMP             148

#define JAVA_FCMPL            149
#define ICMD_FCMPL            149

#define JAVA_FCMPG            150
#define ICMD_FCMPG            150

#define JAVA_DCMPL            151
#define ICMD_DCMPL            151

#define JAVA_DCMPG            152
#define ICMD_DCMPG            152

#define JAVA_IFEQ             153
#define ICMD_IFEQ             153       /* op1 = target JavaVM pc, val.i      */

#define JAVA_IFNE             154
#define ICMD_IFNE             154       /* op1 = target JavaVM pc, val.i      */

#define JAVA_IFLT             155
#define ICMD_IFLT             155       /* op1 = target JavaVM pc, val.i      */

#define JAVA_IFGE             156
#define ICMD_IFGE             156       /* op1 = target JavaVM pc, val.i      */

#define JAVA_IFGT             157
#define ICMD_IFGT             157       /* op1 = target JavaVM pc, val.i      */

#define JAVA_IFLE             158
#define ICMD_IFLE             158       /* op1 = target JavaVM pc, val.i      */

#define JAVA_IF_ICMPEQ        159
#define ICMD_IF_ICMPEQ        159       /* op1 = target JavaVM pc             */

#define JAVA_IF_ICMPNE        160
#define ICMD_IF_ICMPNE        160       /* op1 = target JavaVM pc             */

#define JAVA_IF_ICMPLT        161
#define ICMD_IF_ICMPLT        161       /* op1 = target JavaVM pc             */

#define JAVA_IF_ICMPGE        162
#define ICMD_IF_ICMPGE        162       /* op1 = target JavaVM pc             */

#define JAVA_IF_ICMPGT        163
#define ICMD_IF_ICMPGT        163       /* op1 = target JavaVM pc             */

#define JAVA_IF_ICMPLE        164
#define ICMD_IF_ICMPLE        164       /* op1 = target JavaVM pc             */

#define JAVA_IF_ACMPEQ        165
#define ICMD_IF_ACMPEQ        165       /* op1 = target JavaVM pc             */

#define JAVA_IF_ACMPNE        166
#define ICMD_IF_ACMPNE        166       /* op1 = target JavaVM pc             */

#define JAVA_GOTO             167
#define ICMD_GOTO             167       /* op1 = target JavaVM pc             */

#define JAVA_JSR              168
#define ICMD_JSR              168       /* op1 = target JavaVM pc             */

#define JAVA_RET              169
#define ICMD_RET              169       /* op1 = local variable               */

#define JAVA_TABLESWITCH      170
#define ICMD_TABLESWITCH      170       /* val.a = pointer to s4 table        */
                                        /* length must be computed            */
#define JAVA_LOOKUPSWITCH     171
#define ICMD_LOOKUPSWITCH     171       /* val.a = pointer to s4 table        */
                                        /* length must be computed            */
#define JAVA_IRETURN          172
#define ICMD_IRETURN          172

#define JAVA_LRETURN          173
#define ICMD_LRETURN          173

#define JAVA_FRETURN          174
#define ICMD_FRETURN          174

#define JAVA_DRETURN          175
#define ICMD_DRETURN          175

#define JAVA_ARETURN          176
#define ICMD_ARETURN          176

#define JAVA_RETURN           177
#define ICMD_RETURN           177

#define JAVA_GETSTATIC        178
#define ICMD_GETSTATIC        178       /* op1 = type, val.a = field address  */

#define JAVA_PUTSTATIC        179
#define ICMD_PUTSTATIC        179       /* op1 = type, val.a = field address  */

#define JAVA_GETFIELD         180
#define ICMD_GETFIELD         180       /* op1 = type, val.i = field offset   */

#define JAVA_PUTFIELD         181
#define ICMD_PUTFIELD         181       /* op1 = type, val.i = field offset   */

#define JAVA_INVOKEVIRTUAL    182
#define ICMD_INVOKEVIRTUAL    182       /* val.a = method info pointer        */

#define JAVA_INVOKESPECIAL    183
#define ICMD_INVOKESPECIAL    183       /* val.a = method info pointer        */

#define JAVA_INVOKESTATIC     184
#define ICMD_INVOKESTATIC     184       /* val.a = method info pointer        */

#define JAVA_INVOKEINTERFACE  185
#define ICMD_INVOKEINTERFACE  185       /* val.a = method info pointer        */

/* UNDEF186 */

#define JAVA_NEW              187
#define ICMD_NEW              187       /* op1 = 1, val.a = class pointer     */

#define JAVA_NEWARRAY         188
#define ICMD_NEWARRAY         188       /* op1 = basic type                   */

#define JAVA_ANEWARRAY        189
#define ICMD_ANEWARRAY        189       /* op1 = 0, val.a = array pointer     */
                                        /* op1 = 1, val.a = class pointer     */
#define JAVA_ARRAYLENGTH      190
#define ICMD_ARRAYLENGTH      190

#define JAVA_ATHROW           191
#define ICMD_ATHROW           191

#define JAVA_CHECKCAST        192
#define ICMD_CHECKCAST        192       /* op1 = 0, val.a = array pointer     */
                                        /* op1 = 1, val.a = class pointer     */
#define JAVA_INSTANCEOF       193
#define ICMD_INSTANCEOF       193       /* op1 = 0, val.a = array pointer     */
                                        /* op1 = 1, val.a = class pointer     */
#define JAVA_MONITORENTER     194
#define ICMD_MONITORENTER     194

#define JAVA_MONITOREXIT      195
#define ICMD_MONITOREXIT      195

#define JAVA_WIDE             196

#define JAVA_MULTIANEWARRAY   197
#define ICMD_MULTIANEWARRAY   197       /* op1 = dimension, val.a = array     */
                                        /* pointer                            */
#define JAVA_IFNULL           198
#define ICMD_IFNULL           198       /* op1 = target JavaVM pc             */

#define JAVA_IFNONNULL        199
#define ICMD_IFNONNULL        199       /* op1 = target JavaVM pc             */

#define JAVA_GOTO_W           200

#define JAVA_JSR_W            201

#define JAVA_BREAKPOINT       202

/* UNDEF203 */

#define ICMD_IASTORECONST     204
#define ICMD_LASTORECONST     205
#define ICMD_FASTORECONST     206
#define ICMD_DASTORECONST     207
#define ICMD_AASTORECONST     208
#define ICMD_BASTORECONST     209
#define ICMD_CASTORECONST     210
#define ICMD_SASTORECONST     211

#define ICMD_PUTSTATICCONST   212
#define ICMD_PUTFIELDCONST    213

#define ICMD_IMULPOW2         214
#define ICMD_LMULPOW2         215

#define ICMD_IF_FCMPEQ        216
#define ICMD_IF_FCMPNE        217

#define ICMD_IF_FCMPL_LT      218
#define ICMD_IF_FCMPL_GE      219
#define ICMD_IF_FCMPL_GT      220
#define ICMD_IF_FCMPL_LE      221

#define ICMD_IF_FCMPG_LT      222
#define ICMD_IF_FCMPG_GE      223
#define ICMD_IF_FCMPG_GT      224
#define ICMD_IF_FCMPG_LE      225

#define ICMD_IF_DCMPEQ        226
#define ICMD_IF_DCMPNE        227

#define ICMD_IF_DCMPL_LT      228
#define ICMD_IF_DCMPL_GE      229
#define ICMD_IF_DCMPL_GT      230
#define ICMD_IF_DCMPL_LE      231

#define ICMD_IF_DCMPG_LT      232
#define ICMD_IF_DCMPG_GE      233
#define ICMD_IF_DCMPG_GT      234
#define ICMD_IF_DCMPG_LE      235

#define ICMD_INLINE_START     251       /* instruction before inlined method  */
#define ICMD_INLINE_END       252       /* instruction after inlined method   */
#define ICMD_INLINE_BODY      253       /* start of inlined body              */

#define ICMD_BUILTIN          255       /* internal opcode                    */


/***************************** register types *********************************/

#define REG_RES   0         /* reserved register for OS or code generator     */
#define REG_RET   1         /* return value register                          */
#define REG_EXC   2         /* exception value register                       */
#define REG_SAV   3         /* (callee) saved register                        */
#define REG_TMP   4         /* scratch temporary register (caller saved)      */
#define REG_ARG   5         /* argument register (caller saved)               */

#define REG_END   -1        /* last entry in tables                           */
 
#define PARAMMODE_NUMBERED  0 
#define PARAMMODE_STUFFED   1


/* function prototypes ********************************************************/

/* compiler initialisation */
void jit_init(void);

/* compiler finalisation */
void jit_close(void);

/* create a new jitdata */
jitdata *jit_jitdata_new(methodinfo *m);

/* compile a method with jit compiler */
u1 *jit_compile(methodinfo *m);
u1 *jit_recompile(methodinfo *m);

void jit_invalidate_code(methodinfo *m);
codeinfo *jit_get_current_code(methodinfo *m);
void jit_request_optimization(methodinfo *m);

/* patch the method entrypoint */
u1 *jit_asm_compile(methodinfo *m, u1 *mptr, u1 *sp, u1 *ra);

s4 jit_complement_condition(s4 opcode);

void jit_renumber_basicblocks(jitdata *jd);
#if !defined(NDEBUG)
void jit_check_basicblock_numbers(jitdata *jd);
#endif

#endif /* _JIT_H */


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

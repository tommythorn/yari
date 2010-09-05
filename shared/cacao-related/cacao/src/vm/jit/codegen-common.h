/* src/vm/jit/codegen-common.h - architecture independent code generator stuff

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

   $Id: codegen-common.h 7864 2007-05-03 21:17:26Z twisti $

*/


#ifndef _CODEGEN_COMMON_H
#define _CODEGEN_COMMON_H

/* forward typedefs ***********************************************************/

typedef struct codegendata            codegendata;
typedef struct branchref              branchref;
typedef struct branch_label_ref_t     branch_label_ref_t;
typedef struct critical_section_ref_t critical_section_ref_t;
typedef struct jumpref                jumpref;
typedef struct dataref                dataref;
typedef struct exceptionref           exceptionref;
typedef struct patchref               patchref;
typedef struct linenumberref          linenumberref;


#include "config.h"
#include "vm/types.h"

#include "vm/global.h"

#include "vm/jit/dseg.h"
#include "vm/jit/jit.h"
#include "vm/jit/reg.h"
#include "vm/jit/code.h"
#include "vm/jit/replace.h"

#include "vmcore/descriptor.h"
#include "vmcore/method.h"
#include "vmcore/references.h"


#define MCODEINITSIZE (1<<11)       /* 4/32 Kbyte code area initialization size */
#define DSEGINITSIZE  (1<<11)       /*  4 Kbyte data area initialization size */

#define NCODEINITSIZE (1<<11)       /* 4/32 Kbyte code area initialization size */


/* Register Pack/Unpack Macros ************************************************/

/* ATTENTION: Don't change the order where low and high bits are
   stored! At least mips32 relies in one case on that order. */

#define PACK_REGS(low,high) \
    ( (((high) & 0x0000ffff) << 16) | ((low) & 0x0000ffff) )

#define GET_LOW_REG(a)      ((a) & 0x0000ffff)
#define GET_HIGH_REG(a)    (((a) & 0xffff0000) >> 16)


/* branch conditions **********************************************************/

#define BRANCH_UNCONDITIONAL    -1

#define BRANCH_EQ               (ICMD_IFEQ - ICMD_IFEQ)
#define BRANCH_NE               (ICMD_IFNE - ICMD_IFEQ)
#define BRANCH_LT               (ICMD_IFLT - ICMD_IFEQ)
#define BRANCH_GE               (ICMD_IFGE - ICMD_IFEQ)
#define BRANCH_GT               (ICMD_IFGT - ICMD_IFEQ)
#define BRANCH_LE               (ICMD_IFLE - ICMD_IFEQ)

#define BRANCH_ULT              256
#define BRANCH_ULE              257
#define BRANCH_UGE              258
#define BRANCH_UGT              259

#define BRANCH_NAN              260


/* common branch options ******************************************************/

#define BRANCH_OPT_NONE         0


/* codegendata ****************************************************************/

struct codegendata {
	u4              flags;          /* code generator flags                   */
	u1             *mcodebase;      /* base pointer of code area              */
	u1             *mcodeend;       /* pointer to end of code area            */
	s4              mcodesize;      /* complete size of code area (bytes)     */
	u1             *mcodeptr;       /* code generation pointer                */
	u1             *lastmcodeptr;   /* last patcher position of basic block   */

#if defined(ENABLE_INTRP)
	u1             *ncodebase;      /* base pointer of native code area       */
	s4              ncodesize;      /* complete size of native code area      */
	u1             *ncodeptr;       /* native code generation pointer         */

	u4              lastinstwithoutdispatch; /* ~0 if there was a dispatch    */

	s4              lastpatcheroffset; /* -1 if current super has no patcher  */
	s4              dynsuperm;      /* offsets of start of current dynamic ...*/
	s4              dynsupern;      /* ... superinstruction starts            */
	struct superstart *superstarts; /* list of supers without patchers        */
#endif

	dsegentry      *dseg;           /* chain of data segment entries          */
	s4              dseglen;        /* used size of data area (bytes)         */
                                    /* data area grows from top to bottom     */

	jumpref        *jumpreferences; /* list of jumptable target addresses     */

#if defined(__I386__) || defined(__X86_64__) || defined(__XDSPCORE__) || defined(__M68K__) || defined(ENABLE_INTRP) || defined(__S390__)
	dataref        *datareferences; /* list of data segment references        */
#endif

/* 	list_t         *patchrefs; */
	patchref       *patchrefs;
	list_t         *brancheslabel;
	list_t         *listcritical;   /* list of critical sections              */

	linenumberref  *linenumberreferences; /* list of line numbers and the     */
	                                /* program counters of their first        */
	                                /* instruction                            */
	s4              linenumbertablesizepos;
	s4              linenumbertablestartpos;
	s4              linenumbertab;

	methodinfo     *method;

	s4              stackframesize;    /* stackframe size of this method      */

#if defined(ENABLE_REPLACEMENT)
	rplpoint       *replacementpoint;  /* current replacement point           */
#endif
};


#define CODEGENDATA_FLAG_ERROR           0x00000001
#define CODEGENDATA_FLAG_LONGBRANCHES    0x00000002


#define CODEGENDATA_HAS_FLAG_ERROR(cd) \
    ((cd)->flags & CODEGENDATA_FLAG_ERROR)

#define CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd) \
    ((cd)->flags & CODEGENDATA_FLAG_LONGBRANCHES)


/* branchref *****************************************************************/

struct branchref {
	s4         branchmpc;       /* patching position in code segment          */
	s4         condition;       /* conditional branch condition               */
	s4         reg;             /* register number to check                   */
	u4         options;         /* branch options                             */
	branchref *next;            /* next element in branchref list             */
};


/* branch_label_ref_t *********************************************************/

struct branch_label_ref_t {
	s4         mpc;             /* position in code segment                   */
	s4         label;           /* label number                               */
	s4         condition;       /* conditional branch condition               */
	s4         reg;             /* register number to check                   */
	u4         options;         /* branch options                             */
	listnode_t linkage;
};


/* critical_section_ref_t *****************************************************/

struct critical_section_ref_t {
	s4         start;           /* relative offset to method entry-point      */
	s4         end;
	s4         restart;
	listnode_t linkage;
};


/* jumpref ********************************************************************/

struct jumpref {
	s4          tablepos;       /* patching position in data segment          */
	basicblock *target;         /* target basic block                         */
	jumpref    *next;           /* next element in jumpref list               */
};


/* dataref ********************************************************************/

struct dataref {
	s4       datapos;           /* patching position in generated code        */
	dataref *next;              /* next element in dataref list               */
};


/* patchref *******************************************************************/

struct patchref {
	s4           branchpos;     /* relative offset to method entrypoint       */
	s4           disp;          /* displacement of ref in the data segment    */
	functionptr  patcher;       /* patcher function to call                   */
	voidptr      ref;           /* reference passed                           */
/* 	listnode     linkage; */
	patchref    *next;
};


/* linenumberref **************************************************************/

struct linenumberref {
	s4             tablepos;    /* patching position in data segment          */
	s4             linenumber;  /* line number, used for inserting into the   */
	                            /* table and for validity checking            */
	                            /* -1......start of inlined body              */
	                            /* -2......end of inlined body                */
	                            /* <= -3...special entry with methodinfo *    */
								/* (see doc/inlining_stacktrace.txt)          */
	ptrint         targetmpc;   /* machine code program counter of first      */
	                            /* instruction for given line                 */
								/* NOTE: for linenumber <= -3 this is a the   */
	                            /* (methodinfo *) of the inlined method       */
	linenumberref *next;        /* next element in linenumberref list         */
};


/* methodtree_element *********************************************************/

typedef struct methodtree_element methodtree_element;

struct methodtree_element {
	u1 *startpc;
	u1 *endpc;
};


/* function prototypes ********************************************************/

void codegen_init(void);
void codegen_setup(jitdata *jd);

bool codegen_generate(jitdata *jd);
bool codegen_emit(jitdata *jd);

#if defined(ENABLE_INTRP)
bool intrp_codegen(jitdata *jd);
#endif

void codegen_close(void);

void codegen_increase(codegendata *cd);

#if defined(ENABLE_INTRP)
u1 *codegen_ncode_increase(codegendata *cd, u1 *ncodeptr);
#endif

void codegen_add_branch_ref(codegendata *cd, basicblock *target, s4 condition, s4 reg, u4 options);
void codegen_resolve_branchrefs(codegendata *cd, basicblock *bptr);

void codegen_branch_label_add(codegendata *cd, s4 label, s4 condition, s4 reg, u4 options);


void codegen_add_patch_ref(codegendata *cd, functionptr patcher, voidptr ref,
						   s4 disp);
/* XXX REMOVE ME: don't-break-trunk macro */
#define codegen_addpatchref codegen_add_patch_ref

void codegen_insertmethod(u1 *startpc, u1 *endpc);
u1 *codegen_get_pv_from_pc(u1 *pc);
u1 *codegen_get_pv_from_pc_nocheck(u1 *pc);

#if defined(ENABLE_REPLACEMENT)
#if !defined(NDEBUG)
void codegen_set_replacement_point_notrap(codegendata *cd, s4 type);
void codegen_set_replacement_point(codegendata *cd, s4 type);
#else
void codegen_set_replacement_point_notrap(codegendata *cd);
void codegen_set_replacement_point(codegendata *cd);
#endif
#endif /* defined(ENABLE_REPLACEMENT) */

void codegen_finish(jitdata *jd);

#if defined(ENABLE_DISASSEMBLER)
void codegen_disassemble_nativestub(methodinfo *m, u1 *start, u1 *end);
#endif

/* stub functions */

u1       *codegen_generate_stub_compiler(methodinfo *m);
codeinfo *codegen_generate_stub_native(methodinfo *m, functionptr f);

void      codegen_emit_stub_compiler(jitdata *jd);
void      codegen_emit_stub_native(jitdata *jd, methoddesc *nmd, functionptr f);

#if defined(ENABLE_INTRP)
u1 *intrp_createcompilerstub(methodinfo *m);
u1 *intrp_createnativestub(functionptr f, jitdata *jd, methoddesc *md);
#endif

void removecompilerstub(u1 *stub);
void removenativestub(u1 *stub);

void codegen_start_native_call(u1 *datasp, u1 *pv, u1 *sp, u1 *ra);
java_objectheader *codegen_finish_native_call(u1 *datasp);

s4 codegen_reg_of_var(u2 opcode, varinfo *v, s4 tempregnum);
s4 codegen_reg_of_dst(jitdata *jd, instruction *iptr, s4 tempregnum);

#if defined(ENABLE_THREADS)
void codegen_critical_section_new(codegendata *cd);
void codegen_critical_section_start(codegendata *cd);
void codegen_critical_section_end(codegendata *cd);

# define CODEGEN_CRITICAL_SECTION_NEW      codegen_critical_section_new(cd)
# define CODEGEN_CRITICAL_SECTION_START    codegen_critical_section_start(cd)
# define CODEGEN_CRITICAL_SECTION_END      codegen_critical_section_end(cd)
#else
# define CODEGEN_CRITICAL_SECTION_NEW      /* no-op */
# define CODEGEN_CRITICAL_SECTION_START    /* no-op */
# define CODEGEN_CRITICAL_SECTION_END      /* no-op */
#endif

/* machine dependent functions */
u1 *md_codegen_get_pv_from_pc(u1 *ra);

#endif /* _CODEGEN_COMMON_H */


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

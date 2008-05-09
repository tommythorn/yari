/* src/vm/jit/codegen-common.c - architecture independent code generator stuff

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

   All functions assume the following code area / data area layout:

   +-----------+
   |           |
   | code area | code area grows to higher addresses
   |           |
   +-----------+ <-- start of procedure
   |           |
   | data area | data area grows to lower addresses
   |           |
   +-----------+

   The functions first write into a temporary code/data area allocated by
   "codegen_init". "codegen_finish" copies the code and data area into permanent
   memory. All functions writing values into the data area return the offset
   relative the begin of the code area (start of procedure).	

   $Id: codegen-common.c 7864 2007-05-03 21:17:26Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <string.h>

#include "vm/types.h"

#if defined(ENABLE_JIT)
/* this is required PATCHER_CALL_SIZE */
# include "codegen.h"
#endif

#if defined(__ARM__)
/* this is required for REG_SPLIT */
# include "md-abi.h"
#endif

#include "mm/memory.h"

#include "toolbox/avl.h"
#include "toolbox/list.h"
#include "toolbox/logging.h"

#include "native/jni.h"
#include "native/native.h"

#include "threads/threads-common.h"

#include "vm/exceptions.h"
#include "vm/stringlocal.h"

#include "vm/jit/abi.h"
#include "vm/jit/asmpart.h"
#include "vm/jit/codegen-common.h"

#if defined(ENABLE_DISASSEMBLER)
# include "vm/jit/disass.h"
#endif

#include "vm/jit/dseg.h"
#include "vm/jit/emit-common.h"
#include "vm/jit/jit.h"
#include "vm/jit/md.h"
#include "vm/jit/replace.h"
#include "vm/jit/stacktrace.h"

#if defined(ENABLE_INTRP)
#include "vm/jit/intrp/intrp.h"
#endif

#include "vmcore/method.h"
#include "vmcore/options.h"

# include "vmcore/statistics.h"

#if defined(ENABLE_VMLOG)
#include <vmlog_cacao.h>
#endif


/* in this tree we store all method addresses *********************************/

static avl_tree_t *methodtree = NULL;
static s4 methodtree_comparator(const void *treenode, const void *node);


/* codegen_init ****************************************************************

   TODO

*******************************************************************************/

void codegen_init(void)
{
	/* this tree is global, not method specific */

	if (!methodtree) {
#if defined(ENABLE_JIT)
		methodtree_element *mte;
#endif

		methodtree = avl_create(&methodtree_comparator);

#if defined(ENABLE_JIT)
		/* insert asm_vm_call_method */

		mte = NEW(methodtree_element);

		mte->startpc = (u1 *) (ptrint) asm_vm_call_method;
		mte->endpc   = (u1 *) (ptrint) asm_vm_call_method_end;

		avl_insert(methodtree, mte);
#endif /* defined(ENABLE_JIT) */
	}
}


/* codegen_setup ***************************************************************

   Allocates and initialises code area, data area and references.

*******************************************************************************/

void codegen_setup(jitdata *jd)
{
	methodinfo  *m;
	codegendata *cd;

	/* get required compiler data */
	m  = jd->m;
	cd = jd->cd;

	/* initialize members */

	cd->flags        = 0;

	cd->mcodebase    = DMNEW(u1, MCODEINITSIZE);
	cd->mcodeend     = cd->mcodebase + MCODEINITSIZE;
	cd->mcodesize    = MCODEINITSIZE;

	/* initialize mcode variables */

	cd->mcodeptr     = cd->mcodebase;
	cd->lastmcodeptr = cd->mcodebase;

#if defined(ENABLE_INTRP)
	/* native dynamic superinstructions variables */

	if (opt_intrp) {
		cd->ncodebase = DMNEW(u1, NCODEINITSIZE);
		cd->ncodesize = NCODEINITSIZE;

		/* initialize ncode variables */
	
		cd->ncodeptr = cd->ncodebase;

		cd->lastinstwithoutdispatch = ~0; /* no inst without dispatch */
		cd->superstarts = NULL;
	}
#endif
	cd->dseg           = NULL;
	cd->dseglen        = 0;

	cd->jumpreferences = NULL;

#if defined(__I386__) || defined(__X86_64__) || defined(__XDSPCORE__) || defined(__M68K__) || defined(ENABLE_INTRP)
	cd->datareferences = NULL;
#endif

/* 	cd->patchrefs      = list_create_dump(OFFSET(patchref, linkage)); */
	cd->patchrefs      = NULL;
	cd->brancheslabel  = list_create_dump(OFFSET(branch_label_ref_t, linkage));
	cd->listcritical   = list_create_dump(OFFSET(critical_section_ref_t, linkage));

	cd->linenumberreferences = NULL;
	cd->linenumbertablesizepos = 0;
	cd->linenumbertablestartpos = 0;
	cd->linenumbertab = 0;
}


/* codegen_reset ***************************************************************

   Resets the codegen data structure so we can recompile the method.

*******************************************************************************/

static void codegen_reset(jitdata *jd)
{
	codeinfo    *code;
	codegendata *cd;
	basicblock  *bptr;

	/* get required compiler data */

	code = jd->code;
	cd   = jd->cd;

	/* reset error flag */

	cd->flags          &= ~CODEGENDATA_FLAG_ERROR;

	/* reset some members, we reuse the code memory already allocated
	   as this should have almost the correct size */

	cd->mcodeptr        = cd->mcodebase;
	cd->lastmcodeptr    = cd->mcodebase;

	cd->dseg            = NULL;
	cd->dseglen         = 0;

	cd->jumpreferences  = NULL;

#if defined(__I386__) || defined(__X86_64__) || defined(__XDSPCORE__) || defined(__M68K__) || defined(ENABLE_INTRP)
	cd->datareferences  = NULL;
#endif

/* 	cd->patchrefs       = list_create_dump(OFFSET(patchref, linkage)); */
	cd->patchrefs       = NULL;
	cd->brancheslabel   = list_create_dump(OFFSET(branch_label_ref_t, linkage));
	cd->listcritical    = list_create_dump(OFFSET(critical_section_ref_t, linkage));

	cd->linenumberreferences    = NULL;
	cd->linenumbertablesizepos  = 0;
	cd->linenumbertablestartpos = 0;
	cd->linenumbertab           = 0;
	
	/* We need to clear the mpc and the branch references from all
	   basic blocks as they will definitely change. */

	for (bptr = jd->basicblocks; bptr != NULL; bptr = bptr->next) {
		bptr->mpc        = -1;
		bptr->branchrefs = NULL;
	}

#if defined(ENABLE_REPLACEMENT)
	code->rplpoints     = NULL;
	code->rplpointcount = 0;
	code->regalloc      = NULL;
	code->regalloccount = 0;
	code->globalcount   = 0;
#endif
}


/* codegen_generate ************************************************************

   Generates the code for the currently compiled method.

*******************************************************************************/

bool codegen_generate(jitdata *jd)
{
	codegendata *cd;

	/* get required compiler data */

	cd = jd->cd;

	/* call the machine-dependent code generation function */

	if (!codegen_emit(jd))
		return false;

	/* check for an error */

	if (CODEGENDATA_HAS_FLAG_ERROR(cd)) {
		/* check for long-branches flag, if it is set we recompile the
		   method */

#if !defined(NDEBUG)
        if (compileverbose)
            log_message_method("Re-generating code: ", jd->m);
#endif

		/* XXX maybe we should tag long-branches-methods for recompilation */

		if (CODEGENDATA_HAS_FLAG_LONGBRANCHES(cd)) {
			/* we have to reset the codegendata structure first */

			codegen_reset(jd);

			/* and restart the compiler run */

			if (!codegen_emit(jd))
				return false;
		}
		else {
			vm_abort("codegen_generate: unknown error occurred during codegen_emit: flags=%x\n", cd->flags);
		}

#if !defined(NDEBUG)
        if (compileverbose)
            log_message_method("Re-generating code done: ", jd->m);
#endif
	}

	/* reallocate the memory and finish the code generation */

	codegen_finish(jd);

	/* everything's ok */

	return true;
}


/* codegen_close ***************************************************************

   TODO

*******************************************************************************/

void codegen_close(void)
{
	/* TODO: release avl tree on i386 and x86_64 */
}


/* codegen_increase ************************************************************

   Doubles code area.

*******************************************************************************/

void codegen_increase(codegendata *cd)
{
	u1 *oldmcodebase;

	/* save old mcodebase pointer */

	oldmcodebase = cd->mcodebase;

	/* reallocate to new, doubled memory */

	cd->mcodebase = DMREALLOC(cd->mcodebase,
							  u1,
							  cd->mcodesize,
							  cd->mcodesize * 2);
	cd->mcodesize *= 2;
	cd->mcodeend   = cd->mcodebase + cd->mcodesize;

	/* set new mcodeptr */

	cd->mcodeptr = cd->mcodebase + (cd->mcodeptr - oldmcodebase);

#if defined(__I386__) || defined(__MIPS__) || defined(__X86_64__) || defined(ENABLE_INTRP)
	/* adjust the pointer to the last patcher position */

	if (cd->lastmcodeptr != NULL)
		cd->lastmcodeptr = cd->mcodebase + (cd->lastmcodeptr - oldmcodebase);
#endif
}


/* codegen_ncode_increase ******************************************************

   Doubles code area.

*******************************************************************************/

#if defined(ENABLE_INTRP)
u1 *codegen_ncode_increase(codegendata *cd, u1 *ncodeptr)
{
	u1 *oldncodebase;

	/* save old ncodebase pointer */

	oldncodebase = cd->ncodebase;

	/* reallocate to new, doubled memory */

	cd->ncodebase = DMREALLOC(cd->ncodebase,
							  u1,
							  cd->ncodesize,
							  cd->ncodesize * 2);
	cd->ncodesize *= 2;

	/* return the new ncodeptr */

	return (cd->ncodebase + (ncodeptr - oldncodebase));
}
#endif


/* codegen_add_branch_ref ******************************************************

   Prepends an branch to the list.

*******************************************************************************/

void codegen_add_branch_ref(codegendata *cd, basicblock *target, s4 condition, s4 reg, u4 options)
{
	branchref *br;
	s4         branchmpc;

	STATISTICS(count_branches_unresolved++);

	/* calculate the mpc of the branch instruction */

	branchmpc = cd->mcodeptr - cd->mcodebase;

	br = DNEW(branchref);

	br->branchmpc = branchmpc;
	br->condition = condition;
	br->reg       = reg;
	br->options   = options;
	br->next      = target->branchrefs;

	target->branchrefs = br;
}


/* codegen_resolve_branchrefs **************************************************

   Resolves and patches the branch references of a given basic block.

*******************************************************************************/

void codegen_resolve_branchrefs(codegendata *cd, basicblock *bptr)
{
	branchref *br;
	u1        *mcodeptr;

	/* Save the mcodeptr because in the branch emitting functions
	   we generate code somewhere inside already generated code,
	   but we're still in the actual code generation phase. */

	mcodeptr = cd->mcodeptr;

	/* just to make sure */

	assert(bptr->mpc >= 0);

	for (br = bptr->branchrefs; br != NULL; br = br->next) {
		/* temporary set the mcodeptr */

		cd->mcodeptr = cd->mcodebase + br->branchmpc;

		/* emit_bccz and emit_branch emit the correct code, even if we
		   pass condition == BRANCH_UNCONDITIONAL or reg == -1. */

		emit_bccz(cd, bptr, br->condition, br->reg, br->options);
	}

	/* restore mcodeptr */

	cd->mcodeptr = mcodeptr;
}


/* codegen_branch_label_add ****************************************************

   Append an branch to the label-branch list.

*******************************************************************************/

void codegen_branch_label_add(codegendata *cd, s4 label, s4 condition, s4 reg, u4 options)
{
	list_t             *list;
	branch_label_ref_t *br;
	s4                  mpc;

	/* get the label list */

	list = cd->brancheslabel;
	
	/* calculate the current mpc */

	mpc = cd->mcodeptr - cd->mcodebase;

	br = DNEW(branch_label_ref_t);

	br->mpc       = mpc;
	br->label     = label;
	br->condition = condition;
	br->reg       = reg;
	br->options   = options;

	/* add the branch to the list */

	list_add_last_unsynced(list, br);
}


/* codegen_add_patch_ref *******************************************************

   Appends a new patcher reference to the list of patching positions.

*******************************************************************************/

void codegen_add_patch_ref(codegendata *cd, functionptr patcher, voidptr ref,
						   s4 disp)
{
	patchref *pr;
	s4        branchmpc;

	branchmpc = cd->mcodeptr - cd->mcodebase;

	pr = DNEW(patchref);

	pr->branchpos = branchmpc;
	pr->disp      = disp;
	pr->patcher   = patcher;
	pr->ref       = ref;

/* 	list_add_first(cd->patchrefs, pr); */
	pr->next      = cd->patchrefs;
	cd->patchrefs = pr;

	/* Generate NOPs for opt_shownops. */

	if (opt_shownops)
		PATCHER_NOPS;

#if defined(ENABLE_JIT) && (defined(__I386__) || defined(__MIPS__) || defined(__X86_64__))
	/* On some architectures the patcher stub call instruction might
	   be longer than the actual instruction generated.  On this
	   architectures we store the last patcher call position and after
	   the basic block code generation is completed, we check the
	   range and maybe generate some nop's. */

	cd->lastmcodeptr = cd->mcodeptr + PATCHER_CALL_SIZE;
#endif
}


/* codegen_critical_section_new ************************************************

   Allocates a new critical-section reference and adds it to the
   critical-section list.

*******************************************************************************/

#if defined(ENABLE_THREADS)
void codegen_critical_section_new(codegendata *cd)
{
	list_t                 *list;
	critical_section_ref_t *csr;
	s4                      mpc;

	/* get the critical section list */

	list = cd->listcritical;
	
	/* calculate the current mpc */

	mpc = cd->mcodeptr - cd->mcodebase;

	csr = DNEW(critical_section_ref_t);

	/* We only can set restart right now, as start and end are set by
	   the following, corresponding functions. */

	csr->start   = -1;
	csr->end     = -1;
	csr->restart = mpc;

	/* add the branch to the list */

	list_add_last_unsynced(list, csr);
}
#endif


/* codegen_critical_section_start **********************************************

   Set the start-point of the current critical section (which is the
   last element of the list).

*******************************************************************************/

#if defined(ENABLE_THREADS)
void codegen_critical_section_start(codegendata *cd)
{
	list_t                 *list;
	critical_section_ref_t *csr;
	s4                      mpc;

	/* get the critical section list */

	list = cd->listcritical;
	
	/* calculate the current mpc */

	mpc = cd->mcodeptr - cd->mcodebase;

	/* get the current critical section */

	csr = list_last_unsynced(list);

	/* set the start point */

	assert(csr->start == -1);

	csr->start = mpc;
}
#endif


/* codegen_critical_section_end ************************************************

   Set the end-point of the current critical section (which is the
   last element of the list).

*******************************************************************************/

#if defined(ENABLE_THREADS)
void codegen_critical_section_end(codegendata *cd)
{
	list_t                 *list;
	critical_section_ref_t *csr;
	s4                      mpc;

	/* get the critical section list */

	list = cd->listcritical;
	
	/* calculate the current mpc */

	mpc = cd->mcodeptr - cd->mcodebase;

	/* get the current critical section */

	csr = list_last_unsynced(list);

	/* set the end point */

	assert(csr->end == -1);

	csr->end = mpc;
}
#endif


/* codegen_critical_section_finish *********************************************

   Finish the critical sections, create the critical section nodes for
   the AVL tree and insert them into the tree.

*******************************************************************************/

#if defined(ENABLE_THREADS)
static void codegen_critical_section_finish(jitdata *jd)
{
	codeinfo    *code;
	codegendata *cd;
	list_t                  *list;
	critical_section_ref_t  *csr;
	critical_section_node_t *csn;

	/* get required compiler data */

	code = jd->code;
	cd   = jd->cd;

	/* get the critical section list */

	list = cd->listcritical;

	/* iterate over all critical sections */

	for (csr = list_first_unsynced(list); csr != NULL;
		 csr = list_next_unsynced(list, csr)) {
		/* check if all points are set */

		assert(csr->start   != -1);
		assert(csr->end     != -1);
		assert(csr->restart != -1);

		/* allocate tree node */

		csn = NEW(critical_section_node_t);

		csn->start   = code->entrypoint + csr->start;
		csn->end     = code->entrypoint + csr->end;
		csn->restart = code->entrypoint + csr->restart;

		/* insert into the tree */

		critical_section_register(csn);
	}
}
#endif


/* methodtree_comparator *******************************************************

   Comparator function used for the AVL tree of methods.

   ARGUMENTS:
      treenode....the node from the tree
      node........the node to compare to the tree-node

*******************************************************************************/

static s4 methodtree_comparator(const void *treenode, const void *node)
{
	methodtree_element *mte;
	methodtree_element *mtepc;

	mte   = (methodtree_element *) treenode;
	mtepc = (methodtree_element *) node;

	/* compare both startpc and endpc of pc, even if they have the same value,
	   otherwise the avl_probe sometimes thinks the element is already in the
	   tree */

#ifdef __S390__
	/* On S390 addresses are 31 bit. Compare only 31 bits of value.
	 */
#	define ADDR_MASK(a) ((a) & 0x7FFFFFFF)
#else
#	define ADDR_MASK(a) (a)
#endif

	if (ADDR_MASK((long) mte->startpc) <= ADDR_MASK((long) mtepc->startpc) &&
		ADDR_MASK((long) mtepc->startpc) <= ADDR_MASK((long) mte->endpc) &&
		ADDR_MASK((long) mte->startpc) <= ADDR_MASK((long) mtepc->endpc) &&
		ADDR_MASK((long) mtepc->endpc) <= ADDR_MASK((long) mte->endpc)) {
		return 0;

	} else if (ADDR_MASK((long) mtepc->startpc) < ADDR_MASK((long) mte->startpc)) {
		return -1;

	} else {
		return 1;
	}

#	undef ADDR_MASK
}


/* codegen_insertmethod ********************************************************

   Insert the machine code range of a method into the AVL tree of methods.

*******************************************************************************/

void codegen_insertmethod(u1 *startpc, u1 *endpc)
{
	methodtree_element *mte;

	/* allocate new method entry */

	mte = NEW(methodtree_element);

	mte->startpc = startpc;
	mte->endpc   = endpc;

	/* this function does not return an error, but asserts for
	   duplicate entries */

	avl_insert(methodtree, mte);
}


/* codegen_get_pv_from_pc ******************************************************

   Find the PV for the given PC by searching in the AVL tree of
   methods.

*******************************************************************************/

u1 *codegen_get_pv_from_pc(u1 *pc)
{
	methodtree_element  mtepc;
	methodtree_element *mte;

	/* allocation of the search structure on the stack is much faster */

	mtepc.startpc = pc;
	mtepc.endpc   = pc;

	mte = avl_find(methodtree, &mtepc);

	if (mte == NULL) {
		/* No method was found.  Let's dump a stacktrace. */

#if defined(ENABLE_VMLOG)
		vmlog_cacao_signl("SIGSEGV");
#endif

		log_println("We received a SIGSEGV and tried to handle it, but we were");
		log_println("unable to find a Java method at:");
		log_println("");
#if SIZEOF_VOID_P == 8
		log_println("PC=0x%016lx", pc);
#else
		log_println("PC=0x%08x", pc);
#endif
		log_println("");
		log_println("Dumping the current stacktrace:");

#if defined(ENABLE_THREADS)
		/* XXX michi: This should be available even without threads! */
		threads_print_stacktrace();
#endif

		vm_abort("Exiting...");
	}

	return mte->startpc;
}


/* codegen_get_pv_from_pc_nocheck **********************************************

   Find the PV for the given PC by searching in the AVL tree of
   methods.  This method does not check the return value and is used
   by the profiler.

*******************************************************************************/

u1 *codegen_get_pv_from_pc_nocheck(u1 *pc)
{
	methodtree_element  mtepc;
	methodtree_element *mte;

	/* allocation of the search structure on the stack is much faster */

	mtepc.startpc = pc;
	mtepc.endpc   = pc;

	mte = avl_find(methodtree, &mtepc);

	if (mte == NULL)
		return NULL;
	else
		return mte->startpc;
}


/* codegen_set_replacement_point_notrap ****************************************

   Record the position of a non-trappable replacement point.

*******************************************************************************/

#if defined(ENABLE_REPLACEMENT)
#if !defined(NDEBUG)
void codegen_set_replacement_point_notrap(codegendata *cd, s4 type)
#else
void codegen_set_replacement_point_notrap(codegendata *cd)
#endif
{
	assert(cd->replacementpoint);
	assert(cd->replacementpoint->type == type);
	assert(cd->replacementpoint->flags & RPLPOINT_FLAG_NOTRAP);

	cd->replacementpoint->pc = (u1*) (ptrint) (cd->mcodeptr - cd->mcodebase);

	cd->replacementpoint++;
}
#endif /* defined(ENABLE_REPLACEMENT) */


/* codegen_set_replacement_point ***********************************************

   Record the position of a trappable replacement point.

*******************************************************************************/

#if defined(ENABLE_REPLACEMENT)
#if !defined(NDEBUG)
void codegen_set_replacement_point(codegendata *cd, s4 type)
#else
void codegen_set_replacement_point(codegendata *cd)
#endif
{
	assert(cd->replacementpoint);
	assert(cd->replacementpoint->type == type);
	assert(!(cd->replacementpoint->flags & RPLPOINT_FLAG_NOTRAP));

	cd->replacementpoint->pc = (u1*) (ptrint) (cd->mcodeptr - cd->mcodebase);

	cd->replacementpoint++;

	/* XXX assert(cd->lastmcodeptr <= cd->mcodeptr); */

	cd->lastmcodeptr = cd->mcodeptr + PATCHER_CALL_SIZE;
}
#endif /* defined(ENABLE_REPLACEMENT) */


/* codegen_finish **************************************************************

   Finishes the code generation. A new memory, large enough for both
   data and code, is allocated and data and code are copied together
   to their final layout, unresolved jumps are resolved, ...

*******************************************************************************/

void codegen_finish(jitdata *jd)
{
	codeinfo    *code;
	codegendata *cd;
	s4           mcodelen;
#if defined(ENABLE_INTRP)
	s4           ncodelen;
#endif
	s4           alignedmcodelen;
	jumpref     *jr;
	u1          *epoint;
	s4           alignedlen;

	/* get required compiler data */

	code = jd->code;
	cd   = jd->cd;

	/* prevent compiler warning */

#if defined(ENABLE_INTRP)
	ncodelen = 0;
#endif

	/* calculate the code length */

	mcodelen = (s4) (cd->mcodeptr - cd->mcodebase);

#if defined(ENABLE_STATISTICS)
	if (opt_stat) {
		count_code_len += mcodelen;
		count_data_len += cd->dseglen;
	}
#endif

	alignedmcodelen = MEMORY_ALIGN(mcodelen, MAX_ALIGN);

#if defined(ENABLE_INTRP)
	if (opt_intrp)
		ncodelen = cd->ncodeptr - cd->ncodebase;
	else {
		ncodelen = 0; /* avoid compiler warning */
	}
#endif

	cd->dseglen = MEMORY_ALIGN(cd->dseglen, MAX_ALIGN);
	alignedlen = alignedmcodelen + cd->dseglen;

#if defined(ENABLE_INTRP)
	if (opt_intrp) {
		alignedlen += ncodelen;
	}
#endif

	/* allocate new memory */

	code->mcodelength = mcodelen + cd->dseglen;
	code->mcode       = CNEW(u1, alignedlen);

	/* set the entrypoint of the method */
	
	assert(code->entrypoint == NULL);
	code->entrypoint = epoint = (code->mcode + cd->dseglen);

	/* fill the data segment (code->entrypoint must already be set!) */

	dseg_finish(jd);

	/* copy code to the new location */

	MCOPY((void *) code->entrypoint, cd->mcodebase, u1, mcodelen);

#if defined(ENABLE_INTRP)
	/* relocate native dynamic superinstruction code (if any) */

	if (opt_intrp) {
		cd->mcodebase = code->entrypoint;

		if (ncodelen > 0) {
			u1 *ncodebase = code->mcode + cd->dseglen + alignedmcodelen;

			MCOPY((void *) ncodebase, cd->ncodebase, u1, ncodelen);

			/* flush the instruction and data caches */

			md_cacheflush(ncodebase, ncodelen);

			/* set some cd variables for dynamic_super_rerwite */

			cd->ncodebase = ncodebase;

		} else {
			cd->ncodebase = NULL;
		}

		dynamic_super_rewrite(cd);
	}
#endif

	/* jump table resolving */

	for (jr = cd->jumpreferences; jr != NULL; jr = jr->next)
		*((functionptr *) ((ptrint) epoint + jr->tablepos)) =
			(functionptr) ((ptrint) epoint + (ptrint) jr->target->mpc);

	/* line number table resolving */
	{
		linenumberref *lr;
		ptrint lrtlen = 0;
		ptrint target;

		for (lr = cd->linenumberreferences; lr != NULL; lr = lr->next) {
			lrtlen++;
			target = lr->targetmpc;
			/* if the entry contains an mcode pointer (normal case), resolve it */
			/* (see doc/inlining_stacktrace.txt for details)                    */
			if (lr->linenumber >= -2) {
			    target += (ptrint) epoint;
			}
			*((functionptr *) ((ptrint) epoint + (ptrint) lr->tablepos)) = 
				(functionptr) target;
		}
		
		*((functionptr *) ((ptrint) epoint + cd->linenumbertablestartpos)) =
			(functionptr) ((ptrint) epoint + cd->linenumbertab);

		*((ptrint *) ((ptrint) epoint + cd->linenumbertablesizepos)) = lrtlen;
	}

#if defined(ENABLE_REPLACEMENT)
	/* replacement point resolving */
	{
		int i;
		rplpoint *rp;

		code->replacementstubs += (ptrint) epoint;

		rp = code->rplpoints;
		for (i=0; i<code->rplpointcount; ++i, ++rp) {
			rp->pc = (u1*) ((ptrint) epoint + (ptrint) rp->pc);
		}
	}
#endif /* defined(ENABLE_REPLACEMENT) */

	/* add method into methodtree to find the entrypoint */

	codegen_insertmethod(code->entrypoint, code->entrypoint + mcodelen);

#if defined(__I386__) || defined(__X86_64__) || defined(__XDSPCORE__) || defined(__M68K__) || defined(ENABLE_INTRP)
	/* resolve data segment references */

	dseg_resolve_datareferences(jd);
#endif

#if defined(ENABLE_THREADS)
	/* create cirtical sections */

	codegen_critical_section_finish(jd);
#endif

	/* flush the instruction and data caches */

	md_cacheflush(code->mcode, code->mcodelength);
}


/* codegen_generate_stub_compiler **********************************************

   Wrapper for codegen_emit_stub_compiler.

   Returns:
       pointer to the compiler stub code.

*******************************************************************************/

u1 *codegen_generate_stub_compiler(methodinfo *m)
{
	jitdata     *jd;
	codegendata *cd;
	ptrint      *d;                     /* pointer to data memory             */
	u1          *c;                     /* pointer to code memory             */
	s4           dumpsize;

	/* mark dump memory */

	dumpsize = dump_size();

	/* allocate required data structures */

	jd = DNEW(jitdata);

	jd->m     = m;
	jd->cd    = DNEW(codegendata);
	jd->flags = 0;

	/* get required compiler data */

	cd = jd->cd;

	/* allocate code memory */

	c = CNEW(u1, 3 * SIZEOF_VOID_P + COMPILERSTUB_CODESIZE);

	/* set pointers correctly */

	d = (ptrint *) c;

	cd->mcodebase = c;

	c = c + 3 * SIZEOF_VOID_P;
	cd->mcodeptr = c;

	/* NOTE: The codeinfo pointer is actually a pointer to the
	   methodinfo (this fakes a codeinfo structure). */

	d[0] = (ptrint) asm_call_jit_compiler;
	d[1] = (ptrint) m;
	d[2] = (ptrint) &d[1];                                    /* fake code->m */

	/* call the emit function */

	codegen_emit_stub_compiler(jd);

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		count_cstub_len += 3 * SIZEOF_VOID_P + COMPILERSTUB_CODESIZE;
#endif

	/* flush caches */

	md_cacheflush(cd->mcodebase, 3 * SIZEOF_VOID_P + COMPILERSTUB_CODESIZE);

	/* release dump memory */

	dump_release(dumpsize);

	/* return native stub code */

	return c;
}


/* codegen_generate_stub_native ************************************************

   Wrapper for codegen_emit_stub_native.

   Returns:
       the codeinfo representing the stub code.

*******************************************************************************/

codeinfo *codegen_generate_stub_native(methodinfo *m, functionptr f)
{
	jitdata     *jd;
	codeinfo    *code;
	s4           dumpsize;
	methoddesc  *md;
	methoddesc  *nmd;	
	s4           nativeparams;

	/* mark dump memory */
	dumpsize = dump_size();

	jd = DNEW(jitdata);

	jd->m     = m;
	jd->cd    = DNEW(codegendata);
	jd->rd    = DNEW(registerdata);
	jd->flags = 0;

	/* Allocate codeinfo memory from the heap as we need to keep them. */
	jd->code  = code_codeinfo_new(m); /* XXX check allocation */

	/* get required compiler data */

	code = jd->code;

	/* set the flags for the current JIT run */

#if defined(ENABLE_PROFILING)
	if (opt_prof)
		jd->flags |= JITDATA_FLAG_INSTRUMENT;
#endif

	if (opt_verbosecall)
		jd->flags |= JITDATA_FLAG_VERBOSECALL;

	/* setup code generation stuff */

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
	if (!opt_intrp)
# endif
		reg_setup(jd);
#endif

	codegen_setup(jd);

	/* create new method descriptor with additional native parameters */

	md = m->parseddesc;
	nativeparams = (m->flags & ACC_STATIC) ? 2 : 1;
	
	nmd = (methoddesc *) DMNEW(u1, sizeof(methoddesc) - sizeof(typedesc) +
							   md->paramcount * sizeof(typedesc) +
							   nativeparams * sizeof(typedesc));

	nmd->paramcount = md->paramcount + nativeparams;

	nmd->params = DMNEW(paramdesc, nmd->paramcount);

	nmd->paramtypes[0].type = TYPE_ADR; /* add environment pointer            */

	if (m->flags & ACC_STATIC)
		nmd->paramtypes[1].type = TYPE_ADR; /* add class pointer              */

	MCOPY(nmd->paramtypes + nativeparams, md->paramtypes, typedesc,
		  md->paramcount);

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
	if (!opt_intrp)
# endif
		/* pre-allocate the arguments for the native ABI */

		md_param_alloc_native(nmd);
#endif

	/* generate the code */

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
	if (opt_intrp)
		intrp_createnativestub(f, jd, nmd);
	else
# endif
		codegen_emit_stub_native(jd, nmd, f);
#else
	intrp_createnativestub(f, jd, nmd);
#endif

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		count_nstub_len += code->mcodelength;
#endif

	/* reallocate the memory and finish the code generation */
	codegen_finish(jd);

#if !defined(NDEBUG)
	/* disassemble native stub */

	if (opt_shownativestub) {
#if defined(ENABLE_DISASSEMBLER)
		codegen_disassemble_nativestub(m,
									   (u1 *) (ptrint) code->entrypoint,
									   (u1 *) (ptrint) code->entrypoint + (code->mcodelength - jd->cd->dseglen));
#endif

		/* show data segment */

		if (opt_showddatasegment)
			dseg_display(jd);
	}
#endif /* !defined(NDEBUG) */

	/* release memory */

	dump_release(dumpsize);

	/* return native stub code */

	return code;
}


/* codegen_disassemble_nativestub **********************************************

   Disassembles the generated native stub.

*******************************************************************************/

#if defined(ENABLE_DISASSEMBLER)
void codegen_disassemble_nativestub(methodinfo *m, u1 *start, u1 *end)
{
	printf("Native stub: ");
	utf_fprint_printable_ascii_classname(stdout, m->class->name);
	printf(".");
	utf_fprint_printable_ascii(stdout, m->name);
	utf_fprint_printable_ascii(stdout, m->descriptor);
	printf("\n\nLength: %d\n\n", (s4) (end - start));

	DISASSEMBLE(start, end);
}
#endif


/* codegen_start_native_call ***************************************************

   Prepares the stuff required for a native (JNI) function call:

   - adds a stackframe info structure to the chain, for stacktraces
   - prepares the local references table on the stack

   The layout of the native stub stackframe should look like this:

   +---------------------------+ <- SP (of parent Java function)
   | return address            |
   +---------------------------+
   |                           |
   | stackframe info structure |
   |                           |
   +---------------------------+
   |                           |
   | local references table    |
   |                           |
   +---------------------------+
   |                           |
   | arguments (if any)        |
   |                           |
   +---------------------------+ <- SP (native stub)

*******************************************************************************/

void codegen_start_native_call(u1 *datasp, u1 *pv, u1 *sp, u1 *ra)
{
	stackframeinfo *sfi;
	localref_table *lrt;

	/* get data structures from stack */

	sfi = (stackframeinfo *) (datasp - sizeof(stackframeinfo));
	lrt = (localref_table *) (datasp - sizeof(stackframeinfo) - 
							  sizeof(localref_table));

	/* add a stackframeinfo to the chain */

	stacktrace_create_native_stackframeinfo(sfi, pv, sp, ra);

#if defined(ENABLE_JNI)
	/* add current JNI local references table to this thread */

	lrt->capacity    = LOCALREFTABLE_CAPACITY;
	lrt->used        = 0;
	lrt->localframes = 1;
	lrt->prev        = LOCALREFTABLE;

	/* clear the references array (memset is faster the a for-loop) */

	MSET(lrt->refs, 0, java_objectheader*, LOCALREFTABLE_CAPACITY);

	LOCALREFTABLE = lrt;
#endif
}


/* codegen_finish_native_call **************************************************

   Removes the stuff required for a native (JNI) function call.
   Additionally it checks for an exceptions and in case, get the
   exception object and clear the pointer.

*******************************************************************************/

java_objectheader *codegen_finish_native_call(u1 *datasp)
{
	stackframeinfo     *sfi;
	stackframeinfo    **psfi;
#if defined(ENABLE_JNI)
	localref_table     *lrt;
	localref_table     *plrt;
	s4                  localframes;
#endif
	java_objectheader  *e;

	/* get data structures from stack */

	sfi = (stackframeinfo *) (datasp - sizeof(stackframeinfo));

	/* remove current stackframeinfo from chain */

	psfi = &STACKFRAMEINFO;

	*psfi = sfi->prev;

#if defined(ENABLE_JNI)
	/* release JNI local references tables for this thread */

	lrt = LOCALREFTABLE;

	/* release all current local frames */

	for (localframes = lrt->localframes; localframes >= 1; localframes--) {
		/* get previous frame */

		plrt = lrt->prev;

		/* Clear all reference entries (only for tables allocated on
		   the Java heap). */

		if (localframes > 1)
			MSET(&lrt->refs[0], 0, java_objectheader*, lrt->capacity);

		lrt->prev = NULL;

		/* set new local references table */

		lrt = plrt;
	}

	/* now store the previous local frames in the thread structure */

	LOCALREFTABLE = lrt;
#endif

	/* get the exception and return it */

	e = exceptions_get_and_clear_exception();

	return e;
}


/* removecompilerstub **********************************************************

   Deletes a compilerstub from memory (simply by freeing it).

*******************************************************************************/

void removecompilerstub(u1 *stub)
{
	/* pass size 1 to keep the intern function happy */

	CFREE((void *) stub, 1);
}


/* removenativestub ************************************************************

   Removes a previously created native-stub from memory.
    
*******************************************************************************/

void removenativestub(u1 *stub)
{
	/* pass size 1 to keep the intern function happy */

	CFREE((void *) stub, 1);
}


/* codegen_reg_of_var **********************************************************

   This function determines a register, to which the result of an
   operation should go, when it is ultimatively intended to store the
   result in pseudoregister v.  If v is assigned to an actual
   register, this register will be returned.  Otherwise (when v is
   spilled) this function returns tempregnum.  If not already done,
   regoff and flags are set in the stack location.
       
   On ARM we have to check if a long/double variable is splitted
   across reg/stack (HIGH_REG == REG_SPLIT). We return the actual
   register of v for LOW_REG and the tempregnum for HIGH_REG in such
   cases.  (michi 2005/07/24)

*******************************************************************************/

s4 codegen_reg_of_var(u2 opcode, varinfo *v, s4 tempregnum)
{

#if 0
	/* Do we have to generate a conditional move?  Yes, then always
	   return the temporary register.  The real register is identified
	   during the store. */

	if (opcode & ICMD_CONDITION_MASK)
		return tempregnum;
#endif

	if (!(v->flags & INMEMORY)) {
#if defined(__ARM__) && defined(__ARMEL__)
		if (IS_2_WORD_TYPE(v->type) && (GET_HIGH_REG(v->vv.regoff) == REG_SPLIT))
			return PACK_REGS(GET_LOW_REG(v->vv.regoff),
							 GET_HIGH_REG(tempregnum));
#endif
#if defined(__ARM__) && defined(__ARMEB__)
		if (IS_2_WORD_TYPE(v->type) && (GET_LOW_REG(v->vv.regoff) == REG_SPLIT))
			return PACK_REGS(GET_LOW_REG(tempregnum),
							 GET_HIGH_REG(v->vv.regoff));
#endif
		return v->vv.regoff;
	}

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		count_spills_read++;
#endif

	return tempregnum;
}

/* codegen_reg_of_dst **********************************************************

   This function determines a register, to which the result of an
   operation should go, when it is ultimatively intended to store the
   result in iptr->dst.var.  If dst.var is assigned to an actual
   register, this register will be returned.  Otherwise (when it is
   spilled) this function returns tempregnum.  If not already done,
   regoff and flags are set in the stack location.
       
   On ARM we have to check if a long/double variable is splitted
   across reg/stack (HIGH_REG == REG_SPLIT). We return the actual
   register of dst.var for LOW_REG and the tempregnum for HIGH_REG in such
   cases.  (michi 2005/07/24)

*******************************************************************************/

s4 codegen_reg_of_dst(jitdata *jd, instruction *iptr, s4 tempregnum)
{
	return codegen_reg_of_var(iptr->opc, VAROP(iptr->dst), tempregnum);
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

/* src/vm/jit/optimizing/ssa.c - static single-assignment form

   Copyright (C) 2005, 2006 R. Grafl, A. Krall, C. Kruegel, C. Oates,
   R. Obermaisser, M. Platter, M. Probst, S. Ring, E. Steiner,
   C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich, J. Wenninger,
   Institut f. Computersprachen - TU Wien

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.

   Contact: cacao@complang.tuwien.ac.at

   Authors: Christian Ullrich

   $Id: $

*/
#include <stdio.h>
#include <stdlib.h>

#include "mm/memory.h"

#include "toolbox/bitvector.h"
#include "toolbox/worklist.h"

#include "vm/jit/optimizing/dominators.h"
#include "vm/jit/optimizing/graph.h"
#include "vm/jit/optimizing/lifetimes.h"
#include "vm/jit/optimizing/lsra.h"

#include "vm/jit/optimizing/ssa.h"

#if defined(SSA_DEBUG_VERBOSE)
#include "vm/options.h"   /* compileverbose */
#include "vm/jit/jit.h"          /* icmd_table */
#endif

/* function prototypes */
void dead_code_elimination(methodinfo *m, registerdata *rd, lsradata *ls,
						   graphdata *gd);
void copy_propagation(methodinfo *m,registerdata *rd, lsradata *ls,
					  graphdata *gd);
void replace_use_sites( lsradata *ls, graphdata *gd, struct lifetime *lt,
						int new_v_index, worklist *W);
void ssa_place_phi_functions(codegendata *cd, lsradata *ls, graphdata *gd,
							 dominatordata *dd);
void ssa_Rename_init(methodinfo *m, codegendata *cd, lsradata *ls,
					 graphdata *gd);
void ssa_Rename(methodinfo *m, codegendata *cd, registerdata *rd, lsradata *ls, 
				graphdata *gd, dominatordata *dd);
void ssa_Rename_(codegendata *cd, lsradata *ls, graphdata *gd, 
				 dominatordata *dd, int n);

#ifdef SSA_DEBUG_VERBOSE
void ssa_print_trees(methodinfo *m, codegendata *cd, lsradata *ls,
					 graphdata *gd, dominatordata *dd);
void ssa_print_lt(lsradata *ls);
#endif
/*********************************************************
Data Collection (Use/Definition) while analyze_stack:
Basic Block indices of definition are needed for generation
of the phi Functions.

ssa_set_local_def
ssa_set_interface
ssa_set_use

These two functions use the internal Helpers:
ssa_set_def
ssa_set_interface_
*********************************************************/
void ssa_set_use(lsradata *ls, int b_index, int local_index, int type) {
	/* just count uses to determine the needed max size for use def */
	/* data structures */
	ls->uses++;
}

void ssa_set_def(lsradata *ls, int b_index, int var_index) {

	/* b_index + 1 to leave space for the param init block 0 */
	bv_set_bit(ls->var_def[b_index + 1], var_index);
	/* count number of defs for every var since SSA */
	/* will create a new var for every definition */
	ls->num_defs[var_index]++;
}

void ssa_set_local_def(lsradata *ls, int b_index, int local_index, int type) {

	if (ls->var[local_index][type] == -1) {
		/* New Local Variable encountered -> create a new unique index */
		ls->var[local_index][type] = ls->max_vars;
		ls->var_to_index[ls->max_vars] = ls->max_locals++;
		ls->max_vars++;
#ifdef SSA_DEBUG_VERBOSE
		if (compileverbose)
			printf("Local %3i,%3i -> Var %3i\n",local_index,type,ls->max_vars-1);
#endif
	}

	ssa_set_def(ls, b_index, ls->var[local_index][type]);
}

void ssa_set_interface_(codegendata *cd, lsradata *ls, basicblock *bptr,
				   stackptr s, int depth) {
	int var_index;

	var_index = depth + jd->maxlocals;
	if (ls->var[var_index][s->type] == -1) {
		/* New Interface Stackslot encountered -> create a new unique index */
		ls->var[var_index][s->type] = ls->max_vars;
		ls->var_to_index[ls->max_vars] = ls->max_interfaces--;
		ls->max_vars++;
#ifdef SSA_DEBUG_VERBOSE
		if (compileverbose)
			printf("Interface SS %3i,%3i -> Var %3i\n",bptr->nr+1,depth,ls->max_vars-1);
#endif
	}
	ssa_set_def(ls, bptr->nr, ls->var[var_index][s->type]);
}

void ssa_set_interface(codegendata *cd, lsradata *ls, basicblock *bptr) {
	stackptr out, in;
	int in_d, out_d;

	out = bptr->outstack;
	in = bptr->instack;
	in_d = bptr->indepth;
	out_d = bptr->outdepth;

	/* ignore top Stackelement of instack in case of EXH or SBR blocks */
	/* These are no Interface stackslots! */
	if ((bptr->type == BBTYPE_EXH) ||
		(bptr->type == BBTYPE_SBR)) {
		in_d--;
		in = in->prev;
	}

	for(;(in_d > out_d); in_d--, in = in->prev);

	while((out != NULL)) {
		if (in_d == out_d) {
			if (in != out) {
				/* out interface stackslot is defined in this basic block */
				ssa_set_interface_(cd, ls, bptr, out, out_d - 1);
			}
			out = out->prev;
			out_d--;
			in = in->prev;
			in_d--;
		} else if (in_d < out_d ) {
			/* out interface stackslot is defined in this basic block */
			ssa_set_interface_(cd, ls, bptr, out, out_d - 1);
			out = out->prev;
			out_d--;
		}
	}
}

/*********************************************************************
Initialise needed Data structures
*********************************************************************/

void ssa_init(jitdata *jd) {
	int  p, t;
	methoddesc *md;
	int i, b_index, len;
	int lifetimes;
	stackptr src, dst, tmp;
	instruction *iptr;
	basicblock  *bptr;

	methodinfo *m;
	codegendata *cd;
	registerdata *rd;
	lsradata *ls;

	m = jd->m;
	cd = jd->cd;
	rd = jd->rd;
	ls = jd->ls;
	md = m->parseddesc;

#if defined(SSA_DEBUG_CHECK) || defined(SSA_DEBUG_VERBOSE)
#if defined(SSA_DEBUG_VERBOSE)
	if (compileverbose) {
		printf("%s %s ",m->class->name->text, m->name->text);
		if (jd->isleafmethod)
			printf("**Leafmethod**");
		printf("\n");
	}
#endif
	if (strcmp(m->class->name->text,"java/util/Properties")==0)
		if (strcmp(m->name->text,"load")==0)
#if defined(SSA_DEBUG_VERBOSE)
			if (compileverbose) 
				printf("12-------------------12\n");
#else
	        { int dummy=1; dummy++; }
#endif
#endif

#ifdef SSA_DEBUG_VERBOSE
    if (compileverbose) 
		printf("ssa_init: basicblockcount %3i maxlocals %3i\n",
			   m->basicblockcount, jd->maxlocals);
#endif
	ls->num_defs = DMNEW(int, jd->maxlocals * 5 + cd->maxstack * 5);
	ls->var_to_index = DMNEW(int, jd->maxlocals * 5 + cd->maxstack * 5);
	ls->var = DMNEW(int *, jd->maxlocals + cd->maxstack);
	t = 0;
	for(p = 0; p < jd->maxlocals + cd->maxstack; p++) {
		ls->var[p] = DMNEW(int, 5);
		for(i = 0; i < 5; i++) {
			ls->var[p][i] = -1;
			ls->num_defs[t++] = 0;
		}
	}
	/* init Var Definition bitvectors */
	ls->var_def = DMNEW(int *, m->basicblockcount + 1);
	for(i = 0; i <= m->basicblockcount; i++) {
		ls->var_def[i] =  bv_new(jd->maxlocals * 5 + cd->maxstack * 5);
	}
	ls->uses = 0;
	ls->max_vars = 0;    /* no Vars seen till now */
	                     /* A new Var Index will be created by SSA      */
	                     /* since locals[Index][Type] is a quite sparse array */
	                     /* and locals will be renamed anyway by SSA          */
	ls->max_locals = 0;  /* unique index for every local_var/type pair*/
	ls->max_interfaces = -1; /* unique index for every interface/type pair*/
	                         /* interfaces are < 0, locals > 0 */
	/* Add parameters first in right order, so the new local indices */
	/* 0..p will correspond to "their" parameters */
	/* They get defined at the artificial Block 0, the real method bbs will be*/
	/* moved to start at block 1 */
 	for (p = 0, i = 0; p < md->paramcount; p++) {
 		t = md->paramtypes[p].type;

		ssa_set_local_def(ls, -1, i, t);
 		i++;
 		if (IS_2_WORD_TYPE(t))    /* increment local counter a second time  */
 			i++;                  /* for 2 word types */
	}

	lifetimes = 0;
	bptr = m->basicblocks;

	for(; bptr != NULL; bptr = bptr->next) {
		if (bptr->flags >= BBREACHED) {
			/* 'valid' Basic Block */
			b_index = bptr->nr;

			/* Scan Number of Stack Lifetimes */
			lifetimes += m->basicblocks[b_index].indepth;

			dst = m->basicblocks[b_index].instack;
			len = m->basicblocks[b_index].icount;
			iptr = m->basicblocks[b_index].iinstr;
			for (;len>0; len--, iptr++) {
				src = dst;
				dst = iptr->dst;
				
				/* Reset "leftover" LOCALVAR stackslots */
				switch(iptr->opc) {
				case ICMD_DUP:
					lifetimes +=1;
					if (dst->varkind != ARGVAR)
						dst->varkind = TEMPVAR;
				case ICMD_SWAP:
				case ICMD_DUP2:
					lifetimes += 2;
					for(i=0, tmp = dst; i < 2; i++, tmp = tmp->prev)
						if (tmp->varkind != ARGVAR)
							tmp->varkind = TEMPVAR;
					break;
				case ICMD_DUP_X1:
					lifetimes += 3;
					for(i=0, tmp = dst; i < 3; i++, tmp = tmp->prev)
						if (tmp->varkind != ARGVAR)
							tmp->varkind = TEMPVAR;
					break;
				case ICMD_DUP2_X1:
					lifetimes += 5;
					for(i=0, tmp = dst; i < 5; i++, tmp = tmp->prev)
						if (tmp->varkind != ARGVAR)
							tmp->varkind = TEMPVAR;
					break;
				case ICMD_DUP_X2:
					lifetimes += 4;
					for(i=0, tmp = dst; i < 4; i++, tmp = tmp->prev)
						if (tmp->varkind != ARGVAR)
							tmp->varkind = TEMPVAR;
					break;
				case ICMD_DUP2_X2:
					lifetimes += 6;
					for(i=0, tmp = dst; i < 6; i++, tmp = tmp->prev)
						if (tmp->varkind != ARGVAR)
							tmp->varkind = TEMPVAR;
					break;

				case ICMD_ILOAD:
				case ICMD_LLOAD:
				case ICMD_FLOAD:
				case ICMD_DLOAD:
				case ICMD_ALOAD:
					if (( dst != NULL) && (src != dst))
						lifetimes++;
					dst->varkind = TEMPVAR;
					if (dst->varnum < 0)
						dst->varnum = 0;
					ssa_set_use(ls, b_index, iptr->op1,
								iptr->opc - ICMD_ILOAD);
					break;
					
				case ICMD_IINC:
					if (( dst != NULL) && (src != dst))
						lifetimes++;
					/* For SSA IINC has to be handled as a seperate LOAD */
					/* and STORE. The target local index is held in      */
					/* val._i.op1_t, The immediate val.i is held in     */
					/* val._i.i */
					{
						int v;
						v = iptr->val.i;
						iptr->val._i.op1_t = iptr->op1;
						iptr->val._i.i     = v;
					}

					ssa_set_use(ls, b_index, iptr->op1,TYPE_INT);
					ssa_set_local_def(ls, b_index,
									  iptr->val._i.op1_t, TYPE_INT);
					break;

				case ICMD_ISTORE:
				case ICMD_LSTORE:
				case ICMD_FSTORE:
				case ICMD_DSTORE:
				case ICMD_ASTORE:
					src->varkind = TEMPVAR;
					if (src->varnum < 0)
						src->varnum =0;
					ssa_set_local_def(ls, b_index, iptr->op1, 
									  iptr->opc - ICMD_ISTORE);
					break;


				default:
					if (( dst != NULL) && (src != dst))
						lifetimes++;
				}
			}
			ssa_set_interface(cd, ls, &(m->basicblocks[b_index]));
		}
	}
	ls->maxlifetimes = lifetimes;
	ls->lifetimecount = lifetimes + jd->maxlocals * (TYPE_ADR+1);

}


void ssa_place_phi_functions(codegendata *cd, lsradata *ls, graphdata *gd,
							 dominatordata *dd)
{
	int a,i,j,n,Y;
	bitvector *def_sites;
	bitvector *A_phi;    /* [0..ls->basicblockcount[ of ls->max_vars Bit */
	worklist *W;
	int num_pred;
	bool add_phi;
	stackptr s;

	W = wl_new(ls->basicblockcount);

	def_sites = DMNEW(bitvector, ls->max_vars);
	for(a = 0; a < ls->max_vars; a++)
		def_sites[a] = bv_new(ls->basicblockcount);

	ls->phi = DMNEW(int **, ls->basicblockcount);
	A_phi = DMNEW(bitvector, ls->basicblockcount);
	for(i = 0; i < ls->basicblockcount; i++) {
		ls->phi[i] = DMNEW(int *, ls->max_vars);
		for(j = 0; j < ls->max_vars; j++)
			ls->phi[i][j] = NULL;
		A_phi[i] = bv_new(ls->max_vars);
	}

	for(n = 0; n < ls->basicblockcount; n++)
		for(a = 0; a < ls->max_vars; a++)
			if (bv_get_bit(ls->var_def[n], a))
				bv_set_bit(def_sites[a], n);
#ifdef SSA_DEBUG_VERBOSE
	if (compileverbose) {
		printf("var Definitions:\n");
		for(i = 0; i < ls->max_vars; i++) {
			printf("def_sites[%3i]=%p:",i,(void *)def_sites[i]);
			for(j = 0; j < ls->basicblockcount; j++) {
				if ((j % 5) == 0) printf(" ");
				if (bv_get_bit(def_sites[i], j))
					printf("1");
				else
					printf("0");
			}
			printf(" (");

			printf("\n");
		}
	}
#endif

	for(a = 0; a < ls->max_vars; a++) {
		/* W<-def_sites(a) */
		for(n = 0; n < ls->basicblockcount; n++)
			if (bv_get_bit(def_sites[a],n)) {
				wl_add(W, n);
			}
				
		while (!wl_is_empty(W)) { /* W not empty */
			n = wl_get(W);

			for(i = 0; i < dd->num_DF[n]; i++) {
				Y = dd->DF[n][i];
				/* Node Y is in Dominance Frontier of n -> */
				/* insert phi function for a at top of Y*/
				_SSA_CHECK_BOUNDS(Y, 0, ls->basicblockcount);
				if (bv_get_bit( A_phi[Y], a) == 0) { 
					/* a is not a Element of A_phi[Y] */
					/* a <- phi(a,a...,a) to be inserted at top of Block Y */
					/* phi has as many arguments, as Y has predecessors    */

#if 0
					/* do not add a phi function for interface stackslots */
					/* if a predecessor is not a def site of a <==>       */
					/* the block does not have the corresponding inslot*/
					if ((ls->var_to_index[a] >= 0) ||
						(bv_get_bit(def_sites[a], 
									graph_get_first_predecessor(gd, Y, &iter))))
#endif
					/* for interface stackslots add a phi function only */
					/* if the basicblock has the corresponding incoming */
					/* stackslot -> it could be, that the stackslot is */
					/* not live anymore at Y */

#ifdef SSA_DEBUG_VERBOSE
					if (compileverbose)
						if (ls->var_to_index[a] < 0)
							printf("SS CHeck BB %3i ID %3i V2I %3i A %3i ML %3i\n",Y,
								   ls->basicblocks[Y]->indepth, 
								   ls->var_to_index[a], a, jd->maxlocals);
#endif
					/* Add Locals in any case */
					add_phi = (ls->var_to_index[a] >= 0);
					if (!add_phi) {
						/* Stackslot */
						s = ls->basicblocks[Y]->instack;
						for(i = ls->basicblocks[Y]->indepth-1; i>=0; i--, s = s->prev) {
							_SSA_ASSERT(s != 0);
#ifdef SSA_DEBUG_VERBOSE
					if (compileverbose)
						if (ls->var_to_index[a] < 0)
							printf(" Depth %3i Var %3i\n",i,
								   ls->var[i + jd->maxlocals][s->type]);
#endif
							if (ls->var[i + jd->maxlocals][s->type] == a) {
								add_phi=true;
								break;
							}
						}
					}
					if (add_phi)
					{
						num_pred =  graph_get_num_predecessor(gd, Y);
						ls->phi[Y][a] = DMNEW(int, num_pred + 1);
						for (j = 0; j < num_pred + 1; j++)
							ls->phi[Y][a][j] = a;
						/* increment the number of definitions of a by one */
						/* for this phi function */
						ls->num_defs[a]++;
					}

					bv_set_bit(A_phi[Y], a);
					if (bv_get_bit(ls->var_def[Y],a)==0) {
						/* Iterated Dominance Frontier Criterion:*/
						/* if Y had no definition for a insert it*/
						/* into the Worklist, since now it       */
						/* defines a through the phi function    */
						wl_add(W, Y);
					}
				}
			}
		}
	}
}

void ssa_Rename_init(methodinfo *m, codegendata *cd, lsradata *ls, graphdata *gd) 
{
		
	int a, i, t, p;
	int i_l, i_i;
	
	/* set up new locals */
	/* ls->var[index][type] holds the new unique index  */
	/* in the range of [0..ls-max_vars[                 */
	/* ls->num_defs[index] gives the number of indizes which will be created  */
	/* from SSA */
	/* -> vars will be numbered in this sequence: L0(0)..L0(i) L1(0)..L1(j).*/
	/* ls->var[index][type] will point to each LX(0)  */

	/* as first step cummulate the num_defs array */
	/* for locals */
	/* last element is the maximum local count */
	ls->local_0 = DMNEW(int, ls->max_locals + 1);
	ls->interface_0 = DMNEW(int, -ls->max_interfaces);
	ls->local_0[0] = 0;
	ls->interface_0[0] = 0;
	ls->max_vars_with_indices = 0;
	for(i = 0, i_i = 1, i_l = 1; i < ls->max_vars; i++) {
		ls->max_vars_with_indices += ls->num_defs[i];
		if (ls->var_to_index[i] >= 0) {
			/* local var */
			ls->local_0[i_l] = ls->local_0[i_l-1] + ls->num_defs[i];
			ls->var_to_index[i] = ls->local_0[i_l-1];
			i_l++;
		} else {
			/* interface stackslot */
			ls->interface_0[i_i] = ls->interface_0[i_i-1] + ls->num_defs[i];
			ls->var_to_index[i] = -ls->interface_0[i_i-1] - 1;
			i_i++;
		}
	}

	/* Change the var indices in phi from La to La(0) */
	for(i = 0; i < ls->basicblockcount; i++)
		for (t = 0; t < ls->max_vars; t++)
			if (ls->phi[i][t] != NULL)
				for(p = 0; p < graph_get_num_predecessor(gd, i) + 1; p++)
					ls->phi[i][t][p] = ls->var_to_index[t];
	
	/* Initialization */
	ls->count     = DMNEW(int, ls->max_vars);
	ls->stack     = DMNEW(int *, ls->max_vars);
	ls->stack_top = DMNEW(int, ls->max_vars);
	for(a = 0; a < ls->max_vars; a++) {
		ls->count[a] = 0;
		ls->stack_top[a] = 0;
		/* stack a has to hold number of defs of a Elements + 1 */
		ls->stack[a] = DMNEW(int, ls->num_defs[a] + 1);
		ls->stack[a][ls->stack_top[a]++] = 0;
	}
	if (ls->max_locals > 0) {
		/* Create the num_var_use Array */
		ls->num_var_use = DMNEW(int *, ls->basicblockcount);
		for(i = 0; i < ls->basicblockcount; i++) {
			ls->num_var_use[i] =DMNEW(int, max(1, ls->local_0[ls->max_locals]));
			for(a = 0; a < ls->local_0[ls->max_locals]; a++)
				ls->num_var_use[i][a] = 0;
		}
		/* Create the use_sites Array of Bitvectors*/
		/* use max(1,..), to ensure that the array is created! */
		ls->use_sites =  DMNEW(bitvector, max(1, ls->local_0[ls->max_locals]));
		for(a = 0; a < ls->local_0[ls->max_locals]; a++)
			ls->use_sites[a] = bv_new(ls->basicblockcount);
	}
	/* init lifetimes */

	ls->maxlifetimes = /*m*/ ls->maxlifetimes + ls->basicblockcount * m->maxstack;
	ls->lifetimecount = ls->maxlifetimes + ls->local_0[ls->max_locals] 
		+ cd->maxstack * 5;
	ls->lifetime = DMNEW(struct lifetime, ls->lifetimecount);
	ls->lt_used = DMNEW(int, ls->lifetimecount);
	ls->lt_int = DMNEW(int, ls->lifetimecount);
	ls->lt_int_count = 0;
	ls->lt_flt = DMNEW(int, ls->lifetimecount);
	ls->lt_flt_count = 0;
	ls->lt_mem = DMNEW(int, ls->lifetimecount);
	ls->lt_mem_count = 0;
	for (i=0; i < ls->lifetimecount; i++) ls->lifetime[i].type = -1;

}

int ssa_Rename_def(lsradata *ls, int n, int a) {
	int i;
	
	_SSA_CHECK_BOUNDS(a,0,ls->max_vars);
	ls->count[a]++;
	i = ls->count[a] - 1;
	/* push i on stack[a] */
	_SSA_CHECK_BOUNDS(ls->stack_top[a], 0, ls->num_defs[a] + 1);
	ls->stack[a][ls->stack_top[a]++] = i;
	return i;
}

void ssa_Rename_use(lsradata *ls, int n, int a) {
	if (ls->max_locals > 0) {
		bv_set_bit(ls->use_sites[a], n);
		ls->num_var_use[n][a]++;
	}
}

void ssa_Rename_(codegendata *cd, lsradata *ls, graphdata *gd, 
				 dominatordata *dd, int n) {
	int a, i, j, k, iindex, Y;
	int in_d, out_d;
	instruction *iptr;
	int *def_count;    
	/* [0..ls->max_vars[ Number of Definitions of this var in this  */
	/* Basic Block. Used to remove the entries off the stack at the */
	/* end of the function */

	stackptr in, out;
	graphiterator iter_succ, iter_pred;
	struct lifetime *lt;

	_SSA_CHECK_BOUNDS(n, 0, ls->basicblockcount);

	def_count = DMNEW(int, ls->max_vars);
	for(i = 0; i < ls->max_vars; i++)
		def_count[i] = 0;

	/* change Store of possible phi functions from a0 to ai*/
	for(a = 0; a < ls->max_vars; a++)
		if (ls->phi[n][a] != NULL) {
			def_count[a]++;
				/* do not mark this store as use - maybee this phi function */
				/* can be removed for unused Vars*/
			if (ls->var_to_index[a] >= 0)
				/* local var */
				ls->phi[n][a][0] += ssa_Rename_def(ls, n, a);
			else
				/* interface */
				ls->phi[n][a][0] -= ssa_Rename_def(ls, n, a);
		}

	in = ls->basicblocks[n]->instack;
	in_d = ls->basicblocks[n]->indepth;
	/* change use of instack Interface stackslots except top SBR and EXH */
	/* stackslots */
	if ((ls->basicblocks[n]->type == BBTYPE_EXH) ||
		(ls->basicblocks[n]->type == BBTYPE_SBR)) {
		in_d--;
		in = in->prev;
	}
	out = ls->basicblocks[n]->outstack;
	out_d = ls->basicblocks[n]->outdepth;

	for(;out_d > in_d; out = out->prev, out_d--);

	for (;in != NULL; in = in->prev, in_d--) {
		/* Possible Use of                             */
		/* ls->var[in_d - 1 + jd->maxlocals][in->type] */
		_SSA_CHECK_BOUNDS(in_d - 1 + jd->maxlocals, 0, jd->maxlocals + cd->maxstack);
		a = ls->var[in_d - 1 + jd->maxlocals][in->type];
		_SSA_CHECK_BOUNDS(a, 0, ls->max_vars);
		/* i <- top(stack[a]) */
		_SSA_CHECK_BOUNDS(ls->stack_top[a]-1, 0, ls->num_defs[a]+1);
		i = ls->stack[a][ls->stack_top[a]-1]; 
		_SSA_CHECK_BOUNDS(i, 0, ls->num_defs[a]);
		/* Replace use of x with xi */
#ifdef SSA_DEBUG_VERBOSE
		if (compileverbose)
			printf("Ren Use:BB %3i: stackslot: depth %3i old val: %3i Var,0: %3i varind: %3i\n", n, in_d, in->varnum, ls->var_to_index[a], ls->var_to_index[a]-i);
#endif
		in->varnum = ls->var_to_index[a] - i;
		lt = &(ls->lifetime[-in->varnum-1]);
		lt->v_index = in->varnum;
		lt->bb_last_use = -1;
		lsra_add_ss(lt, in);
		in->varkind = TEMPVAR;
	}
	in = ls->basicblocks[n]->instack;
		
	iptr = ls->basicblocks[n]->iinstr;
	for(iindex = 0; iindex < ls->basicblocks[n]->icount; iindex++, iptr++) {
		switch(iptr->opc) {
		case ICMD_ILOAD:
		case ICMD_LLOAD:
		case ICMD_FLOAD:
		case ICMD_DLOAD:
		case ICMD_ALOAD:
			_SSA_CHECK_BOUNDS(iptr->op1, 0, jd->maxlocals);
			a = ls->var[iptr->op1][iptr->opc - ICMD_ILOAD];
			_SSA_CHECK_BOUNDS(a, 0, ls->max_vars);
			/* i <- top(stack[a]) */
			_SSA_CHECK_BOUNDS(ls->stack_top[a]-1, 0, ls->num_defs[a]+1);
			i = ls->stack[a][ls->stack_top[a]-1]; 
			_SSA_CHECK_BOUNDS(i, 0, ls->num_defs[a]);
			/* Replace use of x with xi */
#if 0
			/* there are no LOCALVAR Stackslots with SSA */
			if ((iptr->dst->varkind == LOCALVAR) &&
				(iptr->dst->varnum == iptr->op1))
				iptr->dst->varnum = ls->var_to_index[a] + i;
#endif
			iptr->op1 = ls->var_to_index[a] + i;

			ssa_Rename_use(ls, n, ls->var_to_index[a] + i);

			break;
		case ICMD_ISTORE:
		case ICMD_LSTORE:
		case ICMD_FSTORE:
		case ICMD_DSTORE:
		case ICMD_ASTORE:
			/* replace definition of a with def of ai */
			_SSA_CHECK_BOUNDS(iptr->op1, 0, jd->maxlocals);
			a = ls->var[iptr->op1][iptr->opc - ICMD_ISTORE];
			_SSA_CHECK_BOUNDS(a, 0, ls->max_vars);
			def_count[a]++;

			i = ssa_Rename_def(ls, n, a);
			_SSA_CHECK_BOUNDS(i, 0, ls->num_defs[a]);

			iptr->op1 = ls->var_to_index[a] + i;
			/* Mark Def as use, too. Since param initialisation is in var_def */
			/* and this would not remove this locals, if not used elsewhere   */
			ssa_Rename_use(ls, n, ls->var_to_index[a] + i);
			break;
		case ICMD_IINC:

			/* Load from iptr->op1 */
			_SSA_CHECK_BOUNDS(iptr->op1, 0, jd->maxlocals);
			a = ls->var[iptr->op1][TYPE_INT];
			_SSA_CHECK_BOUNDS(a, 0, ls->max_vars);
			/* i <- top(stack[a]) */
			_SSA_CHECK_BOUNDS(ls->stack_top[a]-1, 0, ls->num_defs[a]+1);
			i = ls->stack[a][ls->stack_top[a]-1]; 
			_SSA_CHECK_BOUNDS(i, 0, ls->num_defs[a]);
			/* Replace use of x with xi */
			iptr->op1 = ls->var_to_index[a] + i;

			ssa_Rename_use(ls, n, ls->var_to_index[a] + i);			
			/*  Store new(iinced) value in iptr->val._i.opq_t */

			/* replace definition of a with def of ai */
			_SSA_CHECK_BOUNDS(iptr->val._i.op1_t, 0, jd->maxlocals);
			a = ls->var[iptr->val._i.op1_t][TYPE_INT];
			_SSA_CHECK_BOUNDS(a, 0, ls->max_vars);
			def_count[a]++;

			i = ssa_Rename_def(ls, n, a);
			_SSA_CHECK_BOUNDS(i, 0, ls->num_defs[a]);

			iptr->val._i.op1_t = ls->var_to_index[a] + i;
			/* Mark Def as use, too. Since param initialisation is in var_def */
			/* and this would not remove this locals, if not used elsewhere   */
			ssa_Rename_use(ls, n, ls->var_to_index[a] + i);

			break;
		}
		in = iptr->dst;
	}
	/* change def of outstack Interface stackslots */
	in = ls->basicblocks[n]->instack;
	in_d = ls->basicblocks[n]->indepth;
	out = ls->basicblocks[n]->outstack;
	out_d = ls->basicblocks[n]->outdepth;

	for(;in_d > out_d; in = in->prev, in_d--);

	for (;out != NULL; out = out->prev, out_d--) {
		if ((in_d < out_d) || (out != in)) {
			/* Def of ls->var[out_d - 1 + jd->maxlocals][out->type] */
			_SSA_CHECK_BOUNDS(out_d - 1 + jd->maxlocals, 0, jd->maxlocals + cd->maxstack);
			a = ls->var[out_d - 1 + jd->maxlocals][out->type];
			_SSA_CHECK_BOUNDS(a, 0, ls->max_vars);
			def_count[a]++;
			i = ssa_Rename_def(ls, n, a); 
			_SSA_CHECK_BOUNDS(i, 0, ls->num_defs[a]);
			/* Replace use of x with xi */
 #ifdef SSA_DEBUG_VERBOSE
			if (compileverbose)
				printf("Ren def:BB %3i: stackslot: depth %3i old val: %3i Var,0: %3i varind: %3i\n", n, in_d, out->varnum, ls->var_to_index[a], ls->var_to_index[a]-i);
#endif
			out->varnum = ls->var_to_index[a] - i;
			lt = &(ls->lifetime[-out->varnum-1]);
			out->varkind = TEMPVAR;
			lt->v_index = out->varnum;
			lsra_add_ss(lt, out);
			
			ls->lifetime[-out->varnum-1].bb_last_use = -1;
		}
		if (out_d == in_d) {
			in_d--;
			in = in->prev;
		}
	}

	/* change phi Functions of Successors */
	Y = graph_get_first_successor(gd, n, &iter_succ);
	for(; Y != -1; Y = graph_get_next(&iter_succ)) {
		_SSA_CHECK_BOUNDS(Y, 0, ls->basicblockcount);
		k = graph_get_first_predecessor(gd, Y, &iter_pred);
		for (j = 0; (k != -1) && (k != n); j++, k = graph_get_next(&iter_pred));
		_SSA_ASSERT(k == n);
		/* n is jth Predecessor of Y */
		for(a = 0; a < ls->max_vars; a++)
			if (ls->phi[Y][a] != NULL) {
				/* i <- top(stack[a]) */
				_SSA_CHECK_BOUNDS(ls->stack_top[a]-1, 0, ls->num_defs[a]+1);
				i = ls->stack[a][ls->stack_top[a]-1]; 
				_SSA_CHECK_BOUNDS(i, 0, ls->num_defs[a]);
				/* change jth operand from a0 to ai */
				if (ls->var_to_index[a] >= 0) {
					/* local var */
					ls->phi[Y][a][j+1] += i;
					_SSA_CHECK_BOUNDS(ls->phi[Y][a][j+1], 0,
								 ls->local_0[ls->max_locals]);
					/* use by phi function has to be remembered, too */
					ssa_Rename_use(ls, n, ls->phi[Y][a][j+1]);
				} else {
					/* interface */
					ls->phi[Y][a][j+1] -= i;
/* 					_SSA_CHECK_BOUNDS(ls->phi[Y][a][j+1], */
/* 								 ls->interface_0[-ls->max_interfaces-1], 0); */
				}
			}
	}
	
	/* Call ssa_Rename_ for all Childs of n of the Dominator Tree */
	for(i = 0; i < ls->basicblockcount; i++)
		if (dd->idom[i] == n)
			ssa_Rename_(cd, ls, gd, dd, i);

	/* pop Stack[a] for each definition of a var a in the original S */
	for(a = 0; a < ls->max_vars; a++) {
		ls->stack_top[a] -= def_count[a];
		_SSA_ASSERT(ls->stack_top[a] >= 0);
	}
}

void ssa_Rename(methodinfo *m, codegendata *cd, registerdata *rd, lsradata *ls, 
				graphdata *gd, dominatordata *dd)
{
	int i, p, t, type, flags;
	methoddesc *md = m->parseddesc;

	varinfo5 *locals;
#ifdef SSA_DEBUG_VERBOSE
	int j;
#endif

	if (ls->max_vars == 0) {
		/* no locals or interfaces to rename */
	/* init lifetimes */

		ls->maxlifetimes = /*m*/ ls->maxlifetimes + ls->basicblockcount * m->maxstack;
		ls->lifetimecount = ls->maxlifetimes + ls->max_locals + cd->maxstack *5;
		ls->lifetime = DMNEW(struct lifetime, ls->lifetimecount);
		ls->lt_used = DMNEW(int, ls->lifetimecount);
		ls->lt_int = DMNEW(int, ls->lifetimecount);
		ls->lt_int_count = 0;
		ls->lt_flt = DMNEW(int, ls->lifetimecount);
		ls->lt_flt_count = 0;
		ls->lt_mem = DMNEW(int, ls->lifetimecount);
		ls->lt_mem_count = 0;
		for (i=0; i < ls->lifetimecount; i++) ls->lifetime[i].type = -1;
		return;
	}
	
	ssa_Rename_init(m, cd, ls, gd);

	/* Consider definition of Local Vars initialized with Arguments */
	/* in Block 0 */
	/* init is regarded as use too-> ssa_Rename_use ->bullshit!!*/
 	for (p = 0, i= 0; p < md->paramcount; p++) {
 		t = md->paramtypes[p].type;

		/* !!!!! locals are now numbered as the parameters !!!! */
		/* !!!!! no additional increment for 2 word types !!!!! */
		/* this happens later on! here we still need the increment */
	    /* index of var can be in the range from 0 up to not including */
	    /* jd->maxlocals */
		_SSA_CHECK_BOUNDS(i,0,jd->maxlocals);
		_SSA_CHECK_BOUNDS(ls->var[i][t], 0, ls->local_0[ls->max_locals]);
		ssa_Rename_def(ls, 0, ls->var[i][t]);
		i++;
		if (IS_2_WORD_TYPE(t))
			i++;
	}
	ssa_Rename_(cd, ls, gd, dd, 0);

#if 0
	/* DO _NOT_ DO THIS! Look at java.util.stringtokenizer.counttokens! */
	/* if there is no use of the defined Var itself by the phi function */
	/* for an loop path, in which this var is not used, it will not be life */
	/* in this path and overwritten! */

	/* Invalidate all xij from phi(xi0)=xi1,xi2,xi3,..,xin with xij == xi0 */
	/* this happens if the phi function is the first definition of x or in a */
	/* path with a backedge xi has no definition */ 
	/* a phi(xij) = ...,xij,... with the only use and definition of xij by */
	/* this phi function would otherwise "deadlock" the dead code elemination */
	/* invalidate means set it to ls->max_vars_with_indices */
	/* a phi function phi(xi0)=xi1,xi2,...xin wiht xij == xi0 for all j in */
	/* [1,n] can be removed */

	for(i = 0; i < ls->max_vars; i++) {
		for(t = 0; t < ls->basicblockcount; t++) {
			if (ls->phi[t][i] != 0) {
				remove_phi = true;
				for(p = 1; p <= graph_get_num_predecessor(gd, t); p++) {
					if (ls->phi[t][i][0] == ls->phi[t][i][p])
						ls->phi[t][i][p] = ls->max_vars_with_indices;
					else 
						remove_phi = false;
				}
			}
			if (remove_phi)
				ls->phi[t][i] = NULL;
		}
	}
#endif

	/* recreate rd->locals[][] */
	/* now only one (local_index/type) pair exists anymore     */
	/* all var[t][i] with var_to_index[var[t][i]] >= 0 are locals */
	/* max local index after SSA indexing is in ls->local_0[ls->max_locals] */
	
	locals = DMNEW(varinfo5, ls->local_0[ls->max_locals]);
	for(i = 0; i < ls->local_0[ls->max_locals] ; i++)
		for(t = 0; t < 5; t++)
			locals[i][t].type = -1;
	for(t = 0; t < 5; t++) {
		for(i = 0; i < jd->maxlocals; i++) {
			p = ls->var[i][t];
			if (p != -1) {
				_SSA_ASSERT(ls->var_to_index[p] >= 0);
				/* It's a local variable */
				p = ls->var_to_index[p];
				locals[p][t].type = t;
				locals[p][t].flags = rd->locals[i][t].flags;
#ifdef SSA_DEBUG_VERBOSE
				if (compileverbose)
					printf("locals %3i %3i .type = %3i\n",p,t,t);
#endif
			}
		}
	}
	
	type = -1;
	flags = -1;
#ifdef SSA_DEBUG_VERBOSE
	p = -1;
	j = 0;
#endif
	for(i = 0; i < ls->local_0[ls->max_locals]; i++) {
		for(t = 0; (t < 5) && (locals[i][t].type == -1); t++);
		if (t == 5) {
			_SSA_ASSERT(type != -1);
#ifdef SSA_DEBUG_VERBOSE
			if (compileverbose) {
				printf("L%3i=L%3i(%3i) type %3i\n",i,p,j,type);
				j++;
			}
#endif
			locals[i][type].type = type;
			locals[i][type].flags = flags;
		} else {
			type = locals[i][t].type;
			flags = locals[i][t].flags;
#ifdef SSA_DEBUG_VERBOSE
			if (compileverbose) {
				j=0;
				p++;
				printf("L%3i=L%3i(%3i) type %3i\n",i,p,j,type);
				j++;
			}
#endif
		}
	}
 	rd->locals = locals;
 	jd->maxlocals = ls->local_0[ls->max_locals];
}

#ifdef SSA_DEBUG_VERBOSE
void ssa_print_trees(methodinfo *m, codegendata *cd, lsradata *ls,
					 graphdata *gd, dominatordata *dd) {
	int i,j;
	printf("ssa_printtrees: maxlocals %3i", jd->maxlocals);
		
	printf("Dominator Tree: \n");
	for(i = 0; i < ls->basicblockcount; i++) {
		printf("%3i:",i);
		for(j = 0; j < ls->basicblockcount; j++) {
			if (dd->idom[j] == i) {
				printf(" %3i", j);
			}
		}
		printf("\n");
	}

	printf("Dominator Forest:\n");
	for(i = 0; i < ls->basicblockcount; i++) {
		printf("%3i:",i);
		for(j = 0; j < dd->num_DF[i]; j++) {
				printf(" %3i", dd->DF[i][j]);
		}
		printf("\n");
	}

	if (ls->max_locals > 0) {
	printf("Use Sites\n");
   	for(i = 0; i < ls->max_locals; i++) {
		printf("use_sites[%3i]=%p:",i,(void *)ls->use_sites[i]);
		for(j = 0; j < ls->basicblockcount; j++) {
			if ((j % 5) == 0) printf(" ");
			if (bv_get_bit(ls->use_sites[i], j))
				printf("1");
			else
				printf("0");
		}
		printf("\n");
	}
	}

	printf("var Definitions:\n");
   	for(i = 0; i < ls->basicblockcount; i++) {
		printf("var_def[%3i]=%p:",i,(void *)ls->var_def[i]);
		for(j = 0; j < ls->max_vars; j++) {
			if ((j % 5) == 0) printf(" ");
			if (bv_get_bit(ls->var_def[i], j))
				printf("1");
			else
				printf("0");
		}
		printf(" (");
		for(j=0; j < ((((ls->max_vars * 5+7)/8) + sizeof(int) - 1)/sizeof(int));
			j++)
			printf("%8x",ls->var_def[i][j]);
		printf("\n");
	}
}

void ssa_print_phi(lsradata *ls, graphdata *gd) {
	int i,j,k;

	printf("phi Functions (max_vars_with_indices: %3i):\n", 
		   ls->max_vars_with_indices);
	for(i = 0; i < ls->basicblockcount; i++) {
		for(j = 0; j < ls->max_vars; j++) {
			if (ls->phi[i][j] != NULL) {
				printf("BB %3i %3i = phi(", i, ls->phi[i][j][0]);
				for(k = 1; k <= graph_get_num_predecessor(gd, i); k++)
					printf("%3i ",ls->phi[i][j][k]);
				printf(")\n");
			}
		}
	}

}

#endif

void ssa_generate_phi_moves(lsradata *ls, graphdata *gd) {
	int a, i, j, pred;
	graphiterator iter;

	/* count moves to be inserted at the end of each block in moves[] */
	ls->num_phi_moves = DMNEW(int, ls->basicblockcount);
	for(i = 0; i < ls->basicblockcount; i++)
		ls->num_phi_moves[i] = 0;
	for(i = 0; i < ls->basicblockcount; i++)
		for(a = 0; a < ls->max_vars; a++)
			if (ls->phi[i][a] != NULL) {
				pred = graph_get_first_predecessor(gd, i, &iter);
				for(; pred != -1; pred = graph_get_next(&iter)) {
					ls->num_phi_moves[pred]++;
				}
			}

	/* allocate ls->phi_moves */
	ls->phi_moves = DMNEW( int **, ls->basicblockcount);
	for(i = 0; i < ls->basicblockcount; i++) {
		ls->phi_moves[i] = DMNEW( int *, ls->num_phi_moves[i]);
		for(j = 0; j <ls->num_phi_moves[i]; j++)
			ls->phi_moves[i][j] = DMNEW(int, 2);
	}

	/* populate ls->phi_moves */
	for(i = 0; i < ls->basicblockcount; i++)
		ls->num_phi_moves[i] = 0;
	for(i = 0; i < ls->basicblockcount; i++)
		for(a = 0; a < ls->max_vars; a++)
			if (ls->phi[i][a] != NULL) {
				pred = graph_get_first_predecessor(gd, i, &iter);
				for(j = 0; pred != -1; j++, pred = graph_get_next(&iter)) {
					/* target is phi[i][a][0] */
					/* source is phi[i][a][j+1] */
					if (ls->phi[i][a][j+1] != ls->max_vars_with_indices) {
						/* valid move */
						if (ls->phi[i][a][0] != ls->phi[i][a][j+1]) {
							ls->phi_moves[pred][ls->num_phi_moves[pred]][0] =
								ls->phi[i][a][0];
							ls->phi_moves[pred][(ls->num_phi_moves[pred])++][1] = 
								ls->phi[i][a][j+1];
						}
					}
				}
			}
}


void ssa(jitdata *jd, graphdata *gd) {
	struct dominatordata *dd;
	methodinfo *m;
	codegendata *cd;
	registerdata *rd;
	lsradata *ls;

	m = jd->m;
	cd = jd->cd;
	rd = jd->rd;
	ls = jd->ls;

	dd = compute_Dominators(gd, ls->basicblockcount);
	computeDF(gd, dd, ls->basicblockcount, 0);

	ssa_place_phi_functions(cd, ls, gd, dd);
	ssa_Rename(m, cd, rd, ls, gd, dd);
#ifdef SSA_DEBUG_VERBOSE
	if (compileverbose) {
		printf("Phi before Cleanup\n");
		ssa_print_phi(ls, gd);
		ssa_print_lt(ls);
	}
#endif
	scan_lifetimes(m, cd, rd, ls, gd, dd);
#ifdef SSA_DEBUG_VERBOSE
	if (compileverbose) {
		ssa_print_lt(ls);
	}
#endif
	dead_code_elimination(m, rd, ls, gd);
#ifdef SSA_DEBUG_VERBOSE
	if (compileverbose) {
		printf("Phi after dead code elemination\n");
		ssa_print_phi(ls, gd);
		ssa_print_lt(ls);
	}
#endif
	copy_propagation(m, rd, ls, gd);
#ifdef SSA_DEBUG_VERBOSE
	if (compileverbose) {
		printf("Phi after copy propagation\n");
		ssa_print_phi(ls, gd);
		ssa_print_lt(ls);
	}
#endif

	ssa_generate_phi_moves(ls, gd);
	transform_BB(jd, gd);


#ifdef SSA_DEBUG_CHECK
	{
		int i, j, pred, in_d, out_d;
		graphiterator iter_pred;
		stackptr in, out;
		bool phi_define;

		for(i = 0; i < ls->basicblockcount; i++) {
			if (ls->basicblocks[i]->indepth != 0) {
				pred = graph_get_first_predecessor(gd, i, &iter_pred);
				for (; (pred != -1); pred = graph_get_next(&iter_pred)) {
					in_d = ls->basicblocks[i]->indepth;
					in = ls->basicblocks[i]->instack;
					for (;in_d > 0; in_d--, in = in->prev) {
						phi_define = false;
						for (j = 0; (!phi_define) && (j < ls->max_vars); j++) {
							if (ls->phi[i][j] != NULL)
								if (ls->phi[i][j][0] == in->varnum)
									phi_define = true;
						}
						if (!phi_define) {
							/* in not defined in phi function -> check with outstack(s) */
							/* of predecessor(s) */
							out_d = ls->basicblocks[pred]->outdepth;
							out = ls->basicblocks[pred]->outstack;
							_SSA_ASSERT(out_d >= in_d);
							for(; out_d > in_d; out_d--, out = out->prev);
							if ((in->varnum != out->varnum) || 
								(in->varkind != out->varkind)) {
								printf("Method: %s %s\n", m->class->name->text, m->name->text);
									printf("Error: Stack Varnum Mismatch BBin %3i BBout %3i Stackdepth %3i\n", i, pred, in_d);
								if (compileverbose)
									printf("Error: Stack Varnum Mismatch BBin %3i BBout %3i Stackdepth %3i\n", i, pred, in_d);
/* 								else */
/* 									_SSA_ASSERT(0); */
							}
						}
					}
				}
			}
		}
	}

#endif


#ifdef SSA_DEBUG_VERBOSE
	if (compileverbose)
		ssa_print_trees(m,cd,ls, gd, dd);
#endif
}

/****************************************************************************
Optimization
****************************************************************************/


/****************************************************************************
Dead Code Elimination
****************************************************************************/
void dead_code_elimination(methodinfo *m,registerdata *rd, lsradata *ls, graphdata *gd) {
	int a, i, source;
	worklist *W;

	instruction *iptr;
	stackptr src;
	struct lifetime *lt, *s_lt;

	bool remove_statement;
	struct site *use;

	W = wl_new(ls->lifetimecount);
	if (ls->lifetimecount > 0) {
		/* put all lifetimes on Worklist */
		for(a = 0; a < ls->lifetimecount; a++) {
			if (ls->lifetime[a].type != -1) {
				wl_add(W, a);
			}
		}
	}

	/* Remove unused lifetimes */
	while(!wl_is_empty(W)) {
		/* take a var out of Worklist */
		a = wl_get(W);

		lt = &(ls->lifetime[a]);
		if ((lt->def == NULL) || (lt->type == -1))
			/* lifetime was already removed -> no defs anymore */
			continue;

		/* Remove lifetimes, which are only used in a phi function, which 
		   defines them! */
		remove_statement = (lt->use != NULL) && (lt->use->iindex < 0);
		for(use = lt->use; (remove_statement && (use != NULL)); 
			use = use->next)
		{
			remove_statement = remove_statement && 
				(use->b_index == lt->def->b_index) &&
				(use->iindex == lt->def->iindex);
		}
		if (remove_statement) {
#ifdef SSA_DEBUG_CHECK
			/* def == use can only happen in phi functions */
			if (remove_statement)
				_SSA_ASSERT(lt->use->iindex < 0);
#endif
			/* give it free for removal */
			lt->use = NULL;
		}

		if (lt->use == NULL) {
			/* Look at statement of definition of a and remove it,           */
			/* if the Statement has no sideeffects other than the assignemnt */
			/* of a */
			if (lt->def->iindex < 0 ) {
				/* phi function */
				/* delete use of sources , delete phi functions  */

				_SSA_ASSERT(ls->phi[lt->def->b_index][-lt->def->iindex-1] !=
							NULL);

				for (i = 1;i <= graph_get_num_predecessor(gd, lt->def->b_index);
					 i++) 
					{
						source = ls->phi[lt->def->b_index][-lt->def->iindex-1][i];
						if ((source != ls->max_vars_with_indices) && 
							(source != lt->v_index)) {
							/* phi Argument was not already removed (already in 
							   because of selfdefinition) */
							if (source >= 0) {
								/* Local Var */
								s_lt = &(ls->lifetime[ls->maxlifetimes + source]);
							} else {
								/* Interface Stackslot */
								s_lt = &(ls->lifetime[-source-1]);
							}
							/* remove it */
							remove_use_site(s_lt,lt->def->b_index, lt->def->iindex);
							/*  put it on the Worklist */
							if (source >= 0) {
								/* Local Var */
								wl_add(W, ls->maxlifetimes + source);
							} else {
								/* Interface Stackslot */
								wl_add(W, -source - 1);
							}
						}
					}
				/* now delete phi function itself */
				ls->phi[lt->def->b_index][-lt->def->iindex-1] = NULL;
			} else {
				/* "normal" Use by ICMD */
				remove_statement = false;
				if (lt->def->b_index != 0) {
					/* do not look at artificial block 0 (parameter init) */
					iptr = ls->basicblocks[lt->def->b_index]->iinstr + 
						lt->def->iindex;

					if (icmd_table[iptr->opc].flags & ICMDTABLE_PEI)
						remove_statement = false;
					/* if ICMD could throw an exception do not remove it! */
					else {
						/* get src stack */
						if (lt->def->iindex == 0)
							src = ls->basicblocks[lt->def->b_index]->instack;
						else
							src = (iptr-1)->dst;
	 
						remove_statement = true;
						/* Statement has side effects? (== ICMD_INVOKE*) */
						switch (iptr->opc) {
						case ICMD_INVOKEVIRTUAL:
						case ICMD_INVOKESPECIAL:
						case ICMD_INVOKESTATIC:
						case ICMD_INVOKEINTERFACE:
						case ICMD_BUILTIN:   
							/* check if really a side effect is possible */
						case ICMD_MULTIANEWARRAY: /* here, too */
							/* side effects by Statement possible -> 
							   do not remove it */
							remove_statement = false;
							break;

							/* delete use of vars by these statments: */

							/* use of iptr->op1 */
						case ICMD_IINC:
						case ICMD_ILOAD:
						case ICMD_LLOAD:
						case ICMD_FLOAD:
						case ICMD_DLOAD:
						case ICMD_ALOAD:
							s_lt = &(ls->lifetime[ls->maxlifetimes + iptr->op1]);
							remove_use_site(s_lt,lt->def->b_index, lt->def->iindex);
							/* put it on the Worklist */
							wl_add(W, ls->maxlifetimes + iptr->op1);
							break;

							/* remove src->prev and src */
						case ICMD_IALOAD:
						case ICMD_LALOAD:
						case ICMD_FALOAD:
						case ICMD_DALOAD:
						case ICMD_AALOAD:

						case ICMD_BALOAD:
						case ICMD_CALOAD:
						case ICMD_SALOAD:

						case ICMD_LADD:
						case ICMD_LSUB:
						case ICMD_LMUL:

						case ICMD_LOR:
						case ICMD_LAND:
						case ICMD_LXOR:

						case ICMD_LSHL:
						case ICMD_LSHR:
						case ICMD_LUSHR:

						case ICMD_IADD:
						case ICMD_IMUL:

						case ICMD_ISHL:
						case ICMD_ISHR:
						case ICMD_IUSHR:
						case ICMD_IAND:
						case ICMD_IOR:
						case ICMD_IXOR:


						case ICMD_FADD:
						case ICMD_FSUB:
						case ICMD_FMUL:

						case ICMD_DADD:
						case ICMD_DSUB:
						case ICMD_DMUL:
						case ICMD_DDIV:
						case ICMD_DREM:
						case ICMD_ISUB:
						case ICMD_LDIV:
						case ICMD_LREM:

						case ICMD_IDIV:
						case ICMD_IREM:

						case ICMD_FDIV:
						case ICMD_FREM:

						case ICMD_LCMP:
						case ICMD_FCMPL:
						case ICMD_FCMPG:
						case ICMD_DCMPL:
						case ICMD_DCMPG:
							/* Remove src->prev and then "fall through" for removal
							   of src */
							s_lt = &(ls->lifetime[-src->prev->varnum-1]);
							remove_use_site(s_lt,lt->def->b_index, lt->def->iindex);
							/* put it on the Worklist */
							wl_add(W, -src->prev->varnum - 1);
							/* remove src */
						case ICMD_ISTORE:
						case ICMD_LSTORE:
						case ICMD_FSTORE:
						case ICMD_DSTORE:
						case ICMD_ASTORE:
						case ICMD_LADDCONST:
						case ICMD_LSUBCONST:
						case ICMD_LMULCONST:
						case ICMD_LMULPOW2:
						case ICMD_LDIVPOW2:
						case ICMD_LREMPOW2:
						case ICMD_LANDCONST:
						case ICMD_LORCONST:
						case ICMD_LXORCONST:
						case ICMD_LSHLCONST:
						case ICMD_LSHRCONST:
						case ICMD_LUSHRCONST:

						case ICMD_IADDCONST:
						case ICMD_ISUBCONST:
						case ICMD_IMULCONST:
						case ICMD_IMULPOW2:
						case ICMD_IDIVPOW2:
						case ICMD_IREMPOW2:
						case ICMD_IANDCONST:
						case ICMD_IORCONST:
						case ICMD_IXORCONST:
						case ICMD_ISHLCONST:
						case ICMD_ISHRCONST:
						case ICMD_IUSHRCONST:

							/* 					case ICMD_IFEQ_ICONST: */
							/* 					case ICMD_IFNE_ICONST: */
							/* 					case ICMD_IFLT_ICONST: */
							/* 					case ICMD_IFGE_ICONST: */
							/* 					case ICMD_IFGT_ICONST: */
							/* 					case ICMD_IFLE_ICONST: */

						case ICMD_INEG:
						case ICMD_INT2BYTE:
						case ICMD_INT2CHAR:
						case ICMD_INT2SHORT:
						case ICMD_LNEG:
						case ICMD_FNEG:
						case ICMD_DNEG:

						case ICMD_I2L:
						case ICMD_I2F:
						case ICMD_I2D:
						case ICMD_L2I:
						case ICMD_L2F:
						case ICMD_L2D:
						case ICMD_F2I:
						case ICMD_F2L:
						case ICMD_F2D:
						case ICMD_D2I:
						case ICMD_D2L:
						case ICMD_D2F:
						case ICMD_CHECKCAST:
						case ICMD_ARRAYLENGTH:
						case ICMD_INSTANCEOF:

						case ICMD_NEWARRAY:
						case ICMD_ANEWARRAY:

						case ICMD_GETFIELD:
							/* Remove src->prev and then "fall through" for removal
							   of src */
							s_lt = &(ls->lifetime[-src->varnum-1]);
							remove_use_site(s_lt,lt->def->b_index, lt->def->iindex);
							/* put it on the Worklist */
							wl_add(W, -src->varnum - 1);
							break;
							/* ignore these for now */
						case ICMD_DUP:
						case ICMD_DUP2:
						case ICMD_DUP_X1:
						case ICMD_DUP_X2:
						case ICMD_DUP2_X1:
						case ICMD_DUP2_X2:
						case ICMD_SWAP:
#ifdef SSA_DEBUG_VERBOSE
							if (compileverbose)
								printf("INFO2: ICMD_DUPX to be removed\n");
#endif
							/* DUPX has sideefects - cannot be removed, only because
							   one of the output vars is unused
							   TODO: extend the dead code elimination, so that all 
							   output ss are checked*/
							remove_statement = false;
							break;
						} /* switch (iptr->opc) */
					}
					if (remove_statement) {
						/* remove statement */
#ifdef SSA_DEBUG_VERBOSE
						if (compileverbose)
							printf("INFO: %s %s:at BB %3i II %3i NOP-<%s\n",
								   m->class->name->text, m->name->text, 
								   lt->def->b_index, lt->def->iindex, 
								   icmd_table[iptr->opc].name);
#endif
						iptr->opc = ICMD_NOP;
  					}
				} /* (lt->def->b_index != 0) */
			} /* if (lt->def->iindex < 0 ) else */
			/* remove definition of a */
			if (lt->v_index >= 0) {
				/* local var */
				rd->locals[lt->v_index][lt->type].type = -1;
			}
			lt->type = -1;
			lt->def = NULL;
		} /* if (lt->use == NULL) */
		
	} /* while(!wl_is_empty(W)) */
} /* dead_code_elimination */



#if 0
void dead_code_elimination(registerdata *rd, lsradata *ls, graphdata *gd) {
	int A,a,i,n,t;
	bool is_empty;

	int *W_stack;
	int W_top;

	W_top = 0;
	if (ls->max_locals > 0) {
		W_stack = DMNEW(int, ls->max_vars);
		/* put unused local vars on Worklist */
		for(a = 0; a < ls->local_0[ls->max_locals]; a++) {
			if (bv_is_empty(ls->use_sites[a], ls->basicblockcount)) {
				/* a is not used */
				W_stack[W_top++] = a;
			}
		}
	}
	/* Remove unused local vars */
	while(W_top > 0) {
		/* take a var out of Worklist */
		a = W_stack[--W_top];

		if (bv_is_empty(ls->use_sites[a], ls->basicblockcount)) {
			for(t = 0; t < 5; t++)
				rd->locals[a][t].type = -1;
		}
		/* Change and if necessary delete phi functions */
		for(n = 0; n < ls->basicblockcount; n++) {
			for(A = 0; A < ls->max_vars; A++) {
				if (ls->var_to_index[A] >= 0) {
				if (ls->phi[n][A] != NULL) {
					is_empty = true;
					/* look through the arguments of the phi function */
					for(i = 1; i < (graph_get_num_predecessor(gd, n)+1); i++) {
						/* check if entry was not already removed */
						if (ls->phi[n][A][i] != -1) {
							if (bv_is_empty(ls->use_sites[ls->phi[n][A][i]],
												ls->basicblockcount))
								{
									ls->num_var_use[n][ls->phi[n][A][i]]--;
									if (ls->num_var_use[n][ls->phi[n][A][i]] ==
										0) {
								 /* this was the only use in this Basic Block */
										bv_reset_bit(
										     ls->use_sites[ls->phi[n][A][i]],n);
										if (bv_is_empty(
											    ls->use_sites[ls->phi[n][A][i]],
												ls->basicblockcount))
											{
								 /* this was the only use in the whole Method */
								 /* -> put this local var on the Worklist     */
												W_stack[W_top++] =
													ls->phi[n][A][i];
											}
									}
									ls->phi[n][A][i] = -1;
								} else
								is_empty = false;
						}
					}
					/* look at the target of the phi function */
					/* Remove it,if target is not used or all arguments where */
					/* removed */
					if (bv_is_empty(ls->use_sites[ls->phi[n][A][0]],
										ls->basicblockcount) || is_empty) {
						/* phi function can be removed */
						ls->phi[n][A] = NULL;
					}
				} /* if (ls->phi[n][A] != NULL) */
				} /* if (ls->var_to_index[A] >= 0 */
			} /* for(li = 0; li < ls->max_locals; li++) */
		} /* for(n = 0; n < ls->basicblockcount; n++) */
	} /* while(W_top>0) */
	/* Remove unused Local Vars from rd->local */
	if (ls->max_locals > 0)
		for(a = 0; a < ls->local_0[ls->max_locals]; a++) {
			if (bv_is_empty(ls->use_sites[a], ls->basicblockcount)) {
				/* a is not used */
				for(t = 0; t < 5; t++)
					rd->locals[a][t].type = -1;
			}
		}	
}
#endif
/****************************************************************************
Simple Constant Propagation
****************************************************************************/

void simple_constant_propagation() {
}
/****************************************************************************
Optimization
*******************************************************************************/
void copy_propagation(methodinfo *m,registerdata *rd, lsradata *ls,
					  graphdata *gd) {
	int a, i, source;
	int only_source;

	worklist *W;

	instruction *iptr;
	struct lifetime *lt, *s_lt;
	struct stackslot *ss;
	stackptr in;

	W = wl_new(ls->lifetimecount);
	if (ls->lifetimecount > 0) {
		/* put all lifetimes on Worklist */
		for(a = 0; a < ls->lifetimecount; a++) {
			if (ls->lifetime[a].type != -1) {
				wl_add(W, a);
			}
		}
	}

	while(!wl_is_empty(W)) {
		/* take a var out of Worklist */
		a = wl_get(W);

		lt = &(ls->lifetime[a]);
		if (lt->type == -1)
			continue;
		_SSA_ASSERT(lt->def != NULL);
		_SSA_ASSERT(lt->use != NULL);
		if (lt->def->iindex < 0 ) {
			/* phi function */
			/* look, if phi function degenerated to a x = phi(y) */
			/* and if so, substitute y for every use of x */

			_SSA_ASSERT(ls->phi[lt->def->b_index][-lt->def->iindex-1] != NULL);

			only_source = ls->max_vars_with_indices;
			for (i = 1; i <= graph_get_num_predecessor(gd, lt->def->b_index);
				 i++) {
					source = ls->phi[lt->def->b_index][-lt->def->iindex-1][i];
					if (source != ls->max_vars_with_indices) {	
						if (only_source == ls->max_vars_with_indices) {
							/* first valid source argument of phi function */
							only_source = source;
						} else {
							/* second valid source argument of phi function */
							/* exit for loop */
							only_source = ls->max_vars_with_indices;
							break;
						}
					}
			}
			if (only_source != ls->max_vars_with_indices) {
				/* replace all use sites of lt with the var_index only_source */
				replace_use_sites( ls, gd, lt, only_source, W);

				/* delete def of lt and replace uses of lt with "only_source" */
				ls->phi[lt->def->b_index][-lt->def->iindex-1] = NULL;

				if (only_source >= 0) {
					/* Local Var */
					s_lt = &(ls->lifetime[ls->maxlifetimes + only_source]);
					rd->locals[lt->v_index][lt->type].type = -1;
				} else {
					/* Interface Stackslot */
					s_lt = &(ls->lifetime[-only_source-1]);
				}
				remove_use_site(s_lt, lt->def->b_index, lt->def->iindex);
				lt->def = NULL;
				/* move use sites from lt to s_lt */
				move_use_sites(lt, s_lt);
				move_stackslots(lt, s_lt);
				lt->type = -1;
			} /* if (only_source != ls->max_vars_with_indices) */
		} else { /* if (lt->def->iindex < 0 )*/	
			/* def in "normal" ICMD */
			iptr = ls->basicblocks[lt->def->b_index]->iinstr + 
				lt->def->iindex;
			if (lt->v_index >= 0) {
				if (lt->def->b_index == 0)
					continue;
				switch(iptr->opc) {
				case ICMD_ISTORE:
				case ICMD_LSTORE:
				case ICMD_FSTORE:
				case ICMD_DSTORE:
				case ICMD_ASTORE:
					if (lt->def->iindex == 0) {
						/* first instruction in bb -> instack==bb->instack */
						in = ls->basicblocks[lt->def->b_index]->instack;
					} else {
						/* instack is (iptr-1)->dst */
						in = (iptr-1)->dst;
					}
					
					if (in->varkind != LOCALVAR) {
#ifdef SSA_DEBUG_VERBOSE
						if (compileverbose)
							printf("copy propagation xstore: BB %3i I %3i: %3i -> %3i\n", lt->def->b_index, lt->def->iindex, iptr->op1, in->varnum);
#endif
						s_lt = &(ls->lifetime[-in->varnum-1]);

						for (ss = s_lt->local_ss; ss != NULL; ss = ss->next) {
							ss->s->varkind = LOCALVAR;
							ss->s->varnum = iptr->op1;
						}

						/* replace all use sites of s_lt with the var_index */
						/* iptr->op1 */

						replace_use_sites(ls, gd, s_lt, iptr->op1, W);
				
						/* s_lt->def is the new def site of lt */
						/* the old ->def site will get a use site of def */
						/* only one def site */
						_SSA_ASSERT(lt->def->next == NULL);
						_SSA_ASSERT(s_lt->def != NULL);
						_SSA_ASSERT(s_lt->def->next == NULL);

						/* replace def of s_lt with iptr->op1 */
						if (s_lt->def->iindex < 0) {
							/* phi function */
							_SSA_ASSERT(ls->phi[s_lt->def->b_index]
									       [-s_lt->def->iindex-1]
									!= NULL);
							ls->phi[s_lt->def->b_index][-s_lt->def->iindex-1][0]
								= iptr->op1;
						} else
							if (in->varnum != iptr->op1)
								printf("copy propagation: LOCALVAR ss->ISTORE BB %i II %i\n",
									   lt->def->b_index, lt->def->iindex);
							

						/* move def to use sites of lt */
						lt->def->next = lt->use;
						lt->use = lt->def;
						
						lt->def = s_lt->def;

						s_lt->def = NULL;


						/* move use sites from s_lt to lt */
						move_use_sites(s_lt, lt);
						move_stackslots(s_lt, lt);
						s_lt->type = -1;
					}
					break;
				}
			} else {
				/* Def Interface Stackslot */

				switch(iptr->opc) {
				case ICMD_ILOAD:
				case ICMD_LLOAD:
				case ICMD_FLOAD:
				case ICMD_DLOAD:
				case ICMD_ALOAD:
					only_source = lt->local_ss->s->varnum;
					if (lt->local_ss->s->varkind != LOCALVAR) {
#ifdef SSA_DEBUG_VERBOSE
						if (compileverbose)
							printf("copy propagation xload: BB %3i I %3i: %3i -> %3i\n", lt->def->b_index, lt->def->iindex, iptr->op1, lt->local_ss->s->varnum);
#endif
						_SSA_ASSERT(iptr->dst->varnum == lt->local_ss->s->varnum);
						for (ss = lt->local_ss; ss != NULL; ss = ss->next) {
							ss->s->varkind = LOCALVAR;
							ss->s->varnum = iptr->op1;
						}

						/* replace all use sites of lt with the var_index iptr->op1*/

						replace_use_sites( ls, gd, lt, iptr->op1, W);
				
						lt->def = NULL;

						s_lt = &(ls->lifetime[ls->maxlifetimes + iptr->op1]);

						/* move use sites from lt to s_lt */
						move_use_sites(lt, s_lt);
						move_stackslots(lt, s_lt);
						lt->type = -1;
					} else
						if (lt->local_ss->s->varnum != iptr->op1)
							printf("copy propagation: ILOAD -> LOCALVAR ss BB %i II %i\n",
								   lt->def->b_index, lt->def->iindex);

					break;
				}
			}	
		}
	}
}

void replace_use_sites( lsradata *ls, graphdata *gd, struct lifetime *lt,
						int new_v_index, worklist *W) {
	struct site *s;
	int i, source;
	instruction *iptr;
	struct stackslot *ss;

	for(s = lt->use; s != NULL; s = s->next) {
		if (s->iindex < 0) {
			/* Use in phi function */
			for (i = 1;i <= graph_get_num_predecessor(gd,s->b_index);
				 i++) {
				source = ls->phi[s->b_index][-s->iindex-1][i];
				if (source == lt->v_index) {	
#ifdef SSA_DEBUG_VERBOSE
					if (W != NULL) {
					if (compileverbose)
						printf("copy propagation phi: BB %3i I %3i: %3i -> \
                                     %3i\n", s->b_index, s->iindex,
							   new_v_index, source);
					}
#endif
					ls->phi[s->b_index][-s->iindex-1][i]
						= new_v_index;
				}
			}
			if (W != NULL) {
				/* Add var, which is defined by this phi function to */
				/* the worklist */
				source = ls->phi[s->b_index][-s->iindex-1][0];
				if (source >= 0) {
					/* Local Var */
					wl_add(W, ls->maxlifetimes + source);
				} else {
					/* Interface Stackslot */
					wl_add(W, -source - 1);
				}
			}
		} else {
			/* use in ICMD */
	
			iptr = ls->basicblocks[s->b_index]->iinstr + 
				s->iindex;
			if (lt->v_index >= 0) {
				/* Local Var use */
				switch(iptr->opc) {
				case ICMD_ILOAD:
				case ICMD_LLOAD:
				case ICMD_FLOAD:
				case ICMD_DLOAD:
				case ICMD_ALOAD:
				case ICMD_ISTORE:
				case ICMD_LSTORE:
				case ICMD_FSTORE:
				case ICMD_DSTORE:
				case ICMD_ASTORE:
				case ICMD_IINC: /* iptr->op1 == USE */
					/* TODO: check if for XSTORE not src->varnum has */
					/* to be changed instead of iptr->op1 */
					_SSA_ASSERT(iptr->op1 == lt->v_index);
#ifdef SSA_DEBUG_VERBOSE
					if (W != NULL) {
					if (compileverbose)
						printf("copy propagation loc: BB %3i I %3i: %3i -> \
                                    %3i\n", s->b_index, s->iindex,
							   new_v_index, iptr->op1);
					}
#endif
					iptr->op1 = new_v_index;
					break;
				default:
					exit(1);
				}
			} else {
				/* Interface Stackslot Use */
#ifdef SSA_DEBUG_VERBOSE
				if (compileverbose)
					printf("copy propagation int: BB %3i I %3i: %3i -> \
                                 %3i\n", s->b_index, s->iindex, new_v_index,
						   lt->local_ss->s->varnum);
#endif
				for (ss = lt->local_ss; ss != NULL; ss = ss->next) {
					ss->s->varkind = LOCALVAR;
					ss->s->varnum = new_v_index;
				}
							
			}	
		}
	} /* for(s = lt->use; s != NULL; s = s->next) */
}

#ifdef SSA_DEBUG_VERBOSE
void ssa_print_lt(lsradata *ls) {
	int i;
	struct lifetime *lt;
	struct site *use;

	printf("SSA LT Def/Use\n");
	for(i = 0; i < ls->lifetimecount; i++) {
		lt = &(ls->lifetime[i]);
		if (lt->type != -1) {
			printf("VI %3i Type %3i Def: ",lt->v_index, lt->type);
			if (lt->def != NULL)
				printf("%3i,%3i Use: ",lt->def->b_index, lt->def->iindex);
			else
				printf("%3i,%3i Use: ",0,0);
			for(use = lt->use; use != NULL; use = use->next)
				printf("%3i,%3i ",use->b_index, use->iindex);
			printf("\n");
		}
	}
}
#endif
/****************************************************************************
Optimization
****************************************************************************/

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

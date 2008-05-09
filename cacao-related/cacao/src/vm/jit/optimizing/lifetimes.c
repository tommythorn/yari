/* src/vm/jit/lsra.inc - lifetime anaylsis

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

   $Id: lifetimes.c $

*/

#include <stdio.h>
#include <stdlib.h>

#include "mm/memory.h"

#include "toolbox/bitvector.h"
#include "toolbox/worklist.h"

#include "vm/builtin.h"
#include "vm/resolve.h"
#include "vm/exceptions.h"
#include "vm/stringlocal.h"

#include "vm/jit/jit.h"

#include "vm/jit/optimizing/graph.h"
#include "vm/jit/optimizing/lsra.h"
#include "vm/jit/optimizing/ssa.h"
#include "vm/jit/optimizing/lifetimes.h"

#ifdef LT_DEBUG_VERBOSE
#include "vm/options.h"
#endif

#include <time.h>
#include <errno.h>

/* function prototypes */
void _scan_lifetimes(registerdata *rd, lsradata *ls, graphdata *gd,
					basicblock *bptr);
void _lsra_new_stack( lsradata *, stackptr , int , int, int);
void _lsra_from_stack(lsradata *, stackptr , int , int, int);
void lsra_usage_local(lsradata *, s4 , int , int , int , int );


void LifeOutAtBlock(lsradata *ls, graphdata *gd, int *M, int n,
					struct lifetime *lt);
void LifeInAtStatement(lsradata *ls, graphdata *gd, int *M, int b_index, 
					   int iindex, struct lifetime *lt);
void LifeOutAtStatement(lsradata *ls, graphdata *gd, int *M, int b_index, 
						int iindex, struct lifetime *lt);
#ifdef USAGE_COUNT
void lt_get_nesting(lsradata *ls, graphdata *gd, dominatordata *dd);
#endif

void set_use_site(struct lifetime *lt, struct site *use_site) {
}

struct site *get_first_use_site(struct lifetime *lt, lt_iterator *iter) {
	return ((*iter) = lt->use);
}

struct site *get_next_site(lt_iterator *iter) {
	if ((*iter) == NULL)
		return NULL;
	else
		return ((*iter) = (*iter)->next);
}

struct site *get_first_def_site(struct lifetime *lt, lt_iterator *iter) {
	return ((*iter) = lt->def);
}

bool v_is_defined_at_s(lsradata *ls, int b_index, int iindex, 
					   struct lifetime * lt) {
	struct site *def_site;
	bool is_defined_at_s;

	def_site = lt->def;
	is_defined_at_s = ((def_site->b_index == b_index) 
					   && (def_site->iindex == iindex));
	return is_defined_at_s;
}

/****************************************************************************
Get Def & Use Sites
****************************************************************************/
void scan_lifetimes(methodinfo *m, codegendata *cd, registerdata *rd,
					lsradata *ls, graphdata *gd, dominatordata *dd) {
	int i, p;
	s4 t;
	methoddesc *md = m->parseddesc;

	graph_DFS(ls, gd);

#ifdef USAGE_COUNT
	lt_get_nesting(ls, gd, dd);
#endif

#if defined(LT_DEBUG_VERBOSE)
	if (compileverbose) {
		printf("Sorted: ");
		for (i=0; i < ls->basicblockcount; i++) printf("%3i ", ls->sorted[i]);
		printf("\n");
		printf("Sorted_rev: ");
		for (i=0; i < ls->basicblockcount; i++) 
			printf("%3i ", ls->sorted_rev[i]);
		printf("\n");
	}
#endif
	if (ls->max_interfaces != -1) {
		/* init Interface Stackslot lifetimes -1.. */
		for (i = -1; i > -ls->interface_0[-ls->max_interfaces-1]-1; i--) {
			ls->lifetime[-i-1].v_index = i;
			ls->lifetime[-i-1].usagecount = 0;
			ls->lifetime[-i-1].bb_last_use = -1;
			ls->lifetime[-i-1].bb_first_def = -1;
			ls->lifetime[-i-1].local_ss = NULL;
			ls->lifetime[-i-1].savedvar = 0;
			ls->lifetime[-i-1].flags = 0;
			/* .type already set while ssa_Rename_, so we could save a */
			/* lookup table for the type information */
		}
		ls->v_index = -ls->interface_0[-ls->max_interfaces-1]-1;
	}

	for(i = ls->basicblockcount - 1; i>= 0; i--)
		if (ls->sorted[i] != -1)
			_scan_lifetimes(rd, ls, gd, ls->basicblocks[ls->sorted[i]]);

	/* Parameter initialisiation for locals [0 .. paramcount[            */
	/* -> add local var write access at (bb=0,iindex=0)                 */

 	for (p = 0; p < md->paramcount; p++) {
 		t = md->paramtypes[p].type;
		i = ls->local_0[p];
		_LT_ASSERT( i < jd->maxlocals);
#ifdef LT_DEBUG_VERBOSE
		if (compileverbose)
			printf("param %3i -> L %3i/%3i",p,i,t);
#endif
		if (rd->locals[i][t].type >= 0) {
			/* Param to Local init happens before normal Code */
#ifdef LT_DEBUG_VERBOSE
			if (compileverbose)
				printf(" ok\n");
#endif
			lsra_usage_local(ls, i, t, 0, 0, LSRA_STORE); 
		}
#ifdef LT_DEBUG_VERBOSE
		else
			if (compileverbose)
				printf(" .....\n");
#endif
	}  /* end for */
}


bool is_simple_lt(struct lifetime *lt) {
	lt_iterator i_def, i_use;
	struct site *def, *use;
	bool all_in_same_block;

	
	def = get_first_def_site(lt, &i_def);
	use = get_first_use_site(lt, &i_use);
	all_in_same_block = true;
	for (; (all_in_same_block && (use != NULL)); use = get_next_site(&i_use)) {
		all_in_same_block = 
			(use->iindex >= 0) && (use->b_index == def->b_index);
	}
	return all_in_same_block;
}

void lt_is_live(lsradata *ls, struct lifetime *lt, int b_index, int iindex) {
	int bb_sorted;

	bb_sorted = ls->sorted_rev[b_index];

	if ((lt->bb_last_use < bb_sorted) || 
		((lt->bb_last_use == bb_sorted) && (lt->i_last_use < iindex))) {
		lt->bb_last_use = bb_sorted;
		lt->i_last_use  = iindex;
	}
	if ((lt->bb_first_def > bb_sorted) || 
		((lt->bb_first_def == bb_sorted) && (lt->i_first_def > iindex))) {
		lt->bb_first_def = bb_sorted;
		lt->i_first_def  = iindex;
	}
}

void lt_set_simple_use(lsradata *ls, struct lifetime *lt) {
	lt_iterator i_use;
	struct site *use;

	/* SAVEDVAR is nowhere set!!!! */

	/* Def is first use */
/* 	lt->bb_first_def = ls->sorted_rev[lt->def->b_index]; */
/* 	lt->i_first_def = lt->def->iindex; */

	lt_is_live(ls, lt, lt->def->b_index, lt->def->iindex);

	/* get last use */
	use = get_first_use_site(lt, &i_use);
/* 	lt->bb_last_use = ls->sorted_rev[use->b_index]; */
/* 	lt->i_last_use = use->iindex; */
	for (;  (use != NULL); use = get_next_site(&i_use))
		lt_is_live(ls, lt, use->b_index, use->iindex);
/* 		if (use->iindex > lt->i_last_use) */
/* 			lt->i_last_use = use->iindex; */
}

#if defined(JOIN_PHI_LT)
/******************************************************************************
Set up data structures for a interference graphs of variables used in each phi
function
******************************************************************************/
void lt_setup_phi_interference(lsradata *ls, graphdata *gd) {
	int a, b, i, j, t;
	int *stack, stack_top;
	struct igraph_lookup **lookup;
	struct igraph_lookup *tmp;
	int lookup_top, igraph_top;
	struct igraph_vars *new_var;

	lookup_top = igraph_top = 0;
	lookup = DMNEW(struct igraph_lookup *, ls->max_vars_with_indices*2);
	stack  = DMNEW(int, ls->max_vars_with_indices);
	for(b = 0; b < ls->basicblockcount; b++) {
		for(a = 0; a < ls->max_vars; a++) {
			if (ls->phi[b][a] != NULL) {
				
#if defined(LT_DEBUG_VERBOSE)
				if (compileverbose) {
					printf("Phi(%3i, %3i): ", a, gd->num_pred[b]);
				}
#endif
				stack_top = 0;
				/* loop for all vars in this phi function -> setup a interf graph */
				/* structure for it */
				for(j = 0; j < gd->num_pred[b] + 1; j++) {
					if (ls->phi[b][a][j] != ls->max_vars_with_indices) {
						/* used entry */
						stack[stack_top++] = ls->phi[b][a][j];
#if defined(LT_DEBUG_VERBOSE)
						if (compileverbose) {
							printf("%3i ",ls->phi[b][a][j]);
						}
#endif
					}
				}
				_LT_ASSERT(stack_top <= ls->max_vars_with_indices);
#if defined(LT_DEBUG_VERBOSE)
				if (compileverbose) {
					printf("\n ");
				}
#endif
				/* sort (insertion)*/
				/* TODO: make unique sort proc (see lsra_insertion...) */
				for (i = 1; i <= stack_top - 1; i++) {
					j = i;
					t = stack[j];
					while ((j > 0) && (stack[j-1] > t)) {
						stack[j] = stack[j-1];
						j--;
					}
					stack[j] = t;
				}
#if defined(LT_DEBUG_VERBOSE)
				if (compileverbose) {
					printf("Sorted: ");
					for(i=0; i < stack_top; i++)
						printf("%3i ",stack[i]);
					printf("\n");
				}
#endif
				/* now remove duplicates */
				/* t ... new stack_top */
				/* i ... first of duplicate sequence */
				/* j ... next duplicate sequence */
				i = t = 0;
				while (i < stack_top) {
					stack[t] = stack[i];
					t++;
					for(j = i + 1; (j < stack_top)&&(stack[i]==stack[j]); j++);
					if (j == stack_top) {
						/* last duplicate entries */
						stack_top = t;
						break;
					}
					i = j;
				}
				_LT_ASSERT(stack_top <= ls->max_vars_with_indices);
#if defined(LSRA_DEBUG_VERBOSE)
				if (compileverbose) {
					printf("wo duplicates: ");
					for(i=0; i < stack_top; i++)
						printf("%3i ",stack[i]);
					printf("\n");
				}
#endif
				/* setup lookuptable for vars stack[0..stack_top[ to        */
				/* interference graph number & interference graph structure */
				for(i = 0; i < stack_top; i++) {
					_LT_ASSERT(lookup_top < ls->max_vars_with_indices*2);
					lookup[lookup_top] = DNEW(struct igraph_lookup);
					lookup[lookup_top]->var = stack[i]; /* var index */
					lookup[lookup_top]->igraph = igraph_top; /* igraph index */
					lookup_top++;
				}
				igraph_top++;
			}
		}
	}
	ls->igraph = DMNEW(struct igraph , igraph_top);
	ls->igraph_top = igraph_top;
	for(i = 0; i < igraph_top; i++) {
		ls->igraph[i].inter = NULL;
		ls->igraph[i].vars = NULL;
	}
	
	/* sort lookup */

	for (i = 1; i < lookup_top; i++) {
		j = i;
		t = lookup[j]->var;
		tmp = lookup[j];
		while ((j > 0) && (lookup[j-1]->var > t)) {
			lookup[j]=lookup[j-1];
			j--;
		}
		lookup[j] = tmp;
	}

	/* join igraphs for multiple vars  */
	/* TODO: make this more efficient! */
	for (i = 1; i < lookup_top; i++) {
		if (lookup[i-1]->var == lookup[i]->var) {
			for(j = 0; j < lookup_top; j++)
				if (j != i)
					if (lookup[j]->igraph == lookup[i]->igraph)
						lookup[j]->igraph = lookup[i-1]->igraph;
			lookup[i]->igraph = lookup[i-1]->igraph;
		}
	}

	ls->igraph_lookup_top = lookup_top;
	ls->igraph_lookup = DMNEW(struct igraph_lookup *, lookup_top);
	for(i = 0; i < lookup_top; i++) {
		ls->igraph_lookup[i] = lookup[i];
		new_var = DNEW(struct igraph_vars);
		new_var->v = lookup[i]->var;
		new_var->next = ls->igraph[lookup[i]->igraph].vars;
		ls->igraph[lookup[i]->igraph].vars = new_var;
	}
#if defined(LT_DEBUG_VERBOSE)
				if (compileverbose) {
					printf("IGraph(%3i): ",igraph_top);
					for(i = 0; i < igraph_top; i++) {
						printf("%3i(",i);
						for(new_var = ls->igraph[i].vars; new_var != NULL; new_var = new_var->next)
							printf("%3i,",new_var->v);
						printf(") ");
					}
					printf("\n");
					for(i=0; i < lookup_top; i++)
						printf("(%3i->%3i) ",ls->igraph_lookup[i]->var, ls->igraph_lookup[i]->igraph);
					printf("\n");
				}
#endif
}

int get_igraph_index(lsradata *ls, int var) {
	int i, i_max, i_min;

	if (ls->igraph_lookup == NULL)
		return -1;

	i_min = 0;
	i_max = ls->igraph_lookup_top;

	while (true) {
		i = (i_min + i_max)/2;
		if (ls->igraph_lookup[i]->var == var)
			return ls->igraph_lookup[i]->igraph;
		if ((i_max - i_min <= 1))
			return -1;
		if (var < ls->igraph_lookup[i]->var) {
			i_max = i;
		} else {
			i_min = i;
		}
	}
	/* prevent compiler warning */
	return -1;
}

void build_interference(lsradata *ls, int b_index, int iindex,
					 struct lifetime *lt) {
	int igraph_index;
	struct igraph_vars *v;
	struct igraph_interference *i;
	struct lifetime *lt_i;
	
	if ((igraph_index = get_igraph_index(ls, lt->v_index)) == -1)
		return;

	_LT_ASSERT(ls->igraph[igraph_index].vars != NULL);

	for(v = ls->igraph[igraph_index].vars; v != NULL; v = v->next) {
		/* ignore interference with var itself */
		if (v->v != lt->v_index) {
			/* get lifetime of v->v */
			if (v->v >= 0) {
				lt_i = &(ls->lifetime[ls->maxlifetimes + v->v]);
			} else {
				lt_i = &(ls->lifetime[-v->v - 1]);
			}
			_LT_ASSERT(lt_i->v_index == v->v);

			if (v_is_defined_at_s(ls, b_index, iindex, lt_i)) {
				/* search if entry already exists */
				for(i = ls->igraph[igraph_index].inter; i != NULL; i = i->next)
				{
					if ((i->v1 == min(v->v, lt->v_index)) &&
						(i->v2 == max(v->v, lt->v_index)))
						break;
				}
				if (i == NULL) {
					i = DNEW(struct igraph_interference);
					i->v1 = min(v->v, lt->v_index);
					i->v2 = max(v->v, lt->v_index);
					i->next = ls->igraph[igraph_index].inter;
					ls->igraph[igraph_index].inter = i;
				}
			}
		}
	}
	
}
#endif

void LifenessAnalysis(methodinfo *m, lsradata *ls, graphdata *gd) {
	int *M; /* bit_vecor of visited blocks */
	int *use; /* bit_vecor of blocks with use sites visited */
	
	struct site *use_site, *u_site;
	lt_iterator iter, iter1;
	graphiterator pred_iter;

	int lt_index, i, pred, iindex, iindex1;
	struct lifetime *lt;
	int *phi;
	struct timespec time_start,time_end;
#if 0
	bool measure = false;
#endif

/* #define MEASURE_RT */

#ifdef MEASURE_RT
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID,&(time_start)) != 0) {
		fprintf(stderr,"could not get time: %s\n",strerror(errno));
		abort();
	}
#endif
#if 0
	if ((strcmp(m->class->name->text, "java/lang/Object")==0) &&
		(strcmp(m->name->text,"equals")==0)) {
		printf("----------------\n");
/* 		measure = false; */
	}
#endif
#if defined(JOIN_PHI_LT)
	lt_setup_phi_interference(ls, gd);
#endif

	M = bv_new(ls->basicblockcount);
	use = bv_new(ls->basicblockcount);

#ifdef LT_DEBUG_VERBOSE
	if (compileverbose)
	printf("LT_ANALYSE: \n");
#endif
	for(lt_index = 0; lt_index < ls->lifetimecount; lt_index++) {
		lt = &(ls->lifetime[lt_index]);
		if (lt->type == -1)
			continue;
#ifdef LT_DEBUG_VERBOSE
	if (compileverbose)
		printf("LT: %3i:", lt_index);
#endif

#if 0
		if (measure)
			if (clock_gettime(CLOCK_THREAD_CPUTIME_ID,&(time_start)) != 0) {
				fprintf(stderr,"could not get time: %s\n",strerror(errno));
				abort();
			}
#endif
			
		lt->savedvar = 0;

		_LT_ASSERT(lt->def != NULL);
		_LT_ASSERT(lt->def->next == NULL); /* SSA! */
		_LT_ASSERT(lt->use != NULL);

		lt->bb_last_use = -1;
		lt->bb_first_def = ls->basicblockcount;
		
		bv_reset(M, ls->basicblockcount);
		bv_reset(use, ls->basicblockcount);

		use_site = get_first_use_site(lt, &iter);
		for (;use_site != NULL; use_site = get_next_site(&iter)) {
			iindex  = use_site->iindex;
			if ((lt->def->b_index == use_site->b_index) &&
				(iindex < 0) &&
				(iindex <= lt->def->iindex)) {
				if (iindex == lt->def->iindex) /* check this */
					continue;
				/* do normal analysis */
				/* there is a use in a phi function before def site */
			} else if (bv_get_bit(use, use_site->b_index)) {
				continue;
			} else {
				bv_set_bit(use, use_site->b_index);
				/* use sites of this basic block not visited till now */
				/* get use site of this bb with highest iindex (lower than def site)*/
				iindex1 = -1;
				u_site = use_site;
				for(iter1= iter; u_site != NULL; u_site = get_next_site(&iter1)) {
					if ((u_site->b_index == use_site->b_index) &&
						(lt->def->b_index == use_site->b_index) &&
						(u_site->iindex >= 0) &&
						(u_site->iindex < lt->def->iindex) &&
						(u_site->iindex > iindex1)) {
						iindex1 = u_site->iindex;
					} else {
						if ((u_site->b_index == use_site->b_index) &&
							(u_site->iindex > iindex))
							iindex = u_site->iindex;
					}
				}
				if (iindex1 != -1)
					iindex = iindex1;
			}
#ifdef LT_DEBUG_VERBOSE
	if (compileverbose)
			printf("(%3i,%3i)", use_site->b_index, iindex);
#endif

			if (iindex < 0) {
				/* use in phi function */
				/* ls->phi[use_site->b_index][-use_site->iindex-1]*/

				lt_is_live(ls, lt, use_site->b_index, iindex);

				phi = ls->phi[use_site->b_index][-iindex-1];
				_LT_ASSERT(phi != NULL);

				if (lt->v_index != phi[0]) { /* check this */
					/* ignore "Self use/def" in phi function */
					
				
					pred = graph_get_first_predecessor(gd, use_site->b_index,
													   &pred_iter);
					for(i = 1; (pred != -1); i++,pred = 
							graph_get_next(&pred_iter))
						if (lt->v_index == phi[i]) 
							LifeOutAtBlock(ls, gd, M, pred, lt);
				}
			} else
				LifeInAtStatement(ls, gd, M, use_site->b_index, 
								  iindex, lt);
		}
#ifdef LT_DEBUG_VERBOSE
	if (compileverbose)
			printf("\n");
#endif

#if 0
		if (measure) {
			if (clock_gettime(CLOCK_THREAD_CPUTIME_ID,&(time_end)) != 0) {
				fprintf(stderr,"could not get time: %s\n",strerror(errno));
				abort();
			}

			{
				long diff;
				time_t atime;

				diff = (time_end.tv_nsec - time_start.tv_nsec) / 1000;
				atime = time_start.tv_sec;
				while (atime < time_end.tv_sec) {
					atime++;
					diff += 1000000;
				}
				printf("%8li %3i %s.%s.%s\n",diff, lt_index, m->class->name->text, m->name->text,
					   m->descriptor->text);
			}
		}
#endif
	}
#ifdef MEASURE_RT
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID,&(time_end)) != 0) {
		fprintf(stderr,"could not get time: %s\n",strerror(errno));
		abort();
	}

	{
		long diff;
		time_t atime;

		diff = (time_end.tv_nsec - time_start.tv_nsec) / 1000;
		atime = time_start.tv_sec;
		while (atime < time_end.tv_sec) {
			atime++;
			diff += 1000000;
		}
		printf("%8li %s.%s.%s\n",diff, m->class->name->text, m->name->text,
			   m->descriptor->text);
	}
#endif

#if defined(JOIN_PHI_LT)
#if defined(LT_DEBUG_VERBOSE)
	if (compileverbose) {
		struct igraph_interference *inter;
		printf("Interferences: ");
		for(i = 0; i < ls->igraph_top; i++) {
			if (ls->igraph[i].inter != NULL) {
				for(inter = ls->igraph[i].inter; inter != NULL; inter = inter->next)
					printf("%3i(%3i,%3i) ",i,inter->v1,inter->v2);
			}
		}
		printf("\n");
	}
#endif

	{
	struct igraph_vars *var, *var1;
	struct igraph_interference *inter;
	struct lifetime *lt1, *lt2;
	struct stackslot *ss;
	struct site *s;
	int j, k, l;
	instruction *iptr;
	int pred;
	graphiterator iter;

	/* join phi arguments that do not interfere */
	printf("this should not be seen \n");
	for(i = 0; i < ls->igraph_top; i++) {
		if (ls->igraph[i].inter == NULL) {
			lt1 = lt2 = NULL;
			for(var = ls->igraph[i].vars; var != NULL; var = var->next) {
				if (lt1 == NULL) {
					if (var->v >= 0) {
						lt1 = &(ls->lifetime[ls->maxlifetimes + var->v]);
					} else {
						lt1 = &(ls->lifetime[-var->v - 1]);
					}
					_LT_ASSERT(lt1->v_index == var->v);
					continue;
				} else {
					if (var->v >= 0) {
						lt2 = &(ls->lifetime[ls->maxlifetimes + var->v]);
					} else {
						lt2 = &(ls->lifetime[-var->v - 1]);
					}
					_LT_ASSERT(lt2->v_index == var->v);
				}

				if ((lt1->v_index < 0) && (lt2->v_index >=0)) {
					/* swap lt1 and lt2 - cannot join Stackslotlifetime */
					/* into Localvar lifetimes */

					lt = lt1;
					lt1 = lt2;
					lt2 = lt;
				}

				if ((lt1->v_index >=0) && (lt2->v_index >=0)) {
					for(s = lt2->def; s != NULL; s = s->next) {
						if ((s->b_index == 0) && (s->iindex ==0)) {
							/* swap lt1 and lt2 - lt2 is initialized by a param*/

							_LT_ASSERT((lt1->def->b_index != 0)
									   || (lt1->def->iindex !=0));
							lt = lt1;
							lt1 = lt2;
							lt2 = lt;
							break;
						}
					}
				}

				/* already joined */
				if ((lt2->type == -1) || (lt1 == lt2))
					continue;
#if defined(LT_DEBUG_VERBOSE)
				if (compileverbose) {
					printf("Joining %3i into %3i\n",lt2->v_index, lt1->v_index);
				}
#endif

				/* copy local_ss from lt2 to lt1 & rename local_ss->s->varnum */
				while (lt2->local_ss != NULL) {
					if (lt1->v_index >= 0) {
						lt2->local_ss->s->varkind = LOCALVAR;
					}
					/* other direction not possible! (LOCALVAR->TEMPVAR) */
					/* see 'if' above */
					lt2->local_ss->s->varnum  = lt1->v_index;

					ss = lt1->local_ss;
					lt1->local_ss = lt2->local_ss;
					lt2->local_ss = lt2->local_ss->next;
					lt1->local_ss->next = ss;
				}

				/* look at the use sites */
				for(s = lt2->use; s != NULL; s = s->next) {
					if (s->iindex < 0) {
						/* use in phi function -> change */
						pred=graph_get_first_predecessor(gd, s->b_index, &iter);
						for (j = 1; pred != -1; j++,
								 pred = graph_get_next(&iter)) {
							if (ls->phi[s->b_index][-s->iindex-1][j] ==
								lt2->v_index) {	
								ls->phi[s->b_index][-s->iindex-1][j]
									= lt1->v_index;
								/* change in appropriate phi_move, too */
								for(k=0; k<ls->num_phi_moves[pred]; k++) {
									if (ls->phi_moves[pred][k][1] ==
										lt2->v_index) {
										ls->phi_moves[pred][k][1]=lt1->v_index;
									}
/* 									if (ls->phi_moves[pred][k][0] == */
/* 										lt2->v_index) { */
/* 										ls->phi_moves[pred][k][0]=lt1->v_index; */
/* 									} */
								}
							}
						}
						/* change in appropriate phi_move, too */
					} else {
						if (lt2->v_index >= 0) {
							/* lt1&&lt2 are LOCALVAR, XSTORE,IINC and XLOAD */
							/* have to be changed */
							iptr = ls->basicblocks[s->b_index]->iinstr +
								s->iindex;
							if (iptr != NULL) {
								/* no edge splitting block from SSA */
								switch(iptr->opc) {
								case ICMD_ILOAD:
								case ICMD_LLOAD:
								case ICMD_FLOAD:
								case ICMD_DLOAD:
								case ICMD_ALOAD:/* iptr->op1 == USE */
								case ICMD_ISTORE:
								case ICMD_LSTORE:
								case ICMD_FSTORE:
								case ICMD_DSTORE:
								case ICMD_ASTORE:
								case ICMD_IINC: /* iptr->op1 == USE */
									if (iptr->op1 == lt2->v_index)
										iptr->op1 = lt1->v_index;
									break;
									/* 							default: */
									/* could be in another than the top stackslot */
									/* too! */
									/* 								_LT_ASSERT((iptr-1)->dst->varnum == */
									/* 										   lt1->v_index); */
								}
							}
						}
						/* uses in stackslots are already changed above by */
						/* renameing and copying of local_ss */
					}
				} /* for(s = lt2->use; s != NULL; s = s->next) */
				if (lt2->v_index >= 0) {
					/* change def site */
					_LT_ASSERT(lt2->def != NULL);
					/* no SSA Anymore -> cyle through all def sites */
					/* 						_LT_ASSERT(lt2->def->next == NULL); */

					for(s = lt2->def; s != NULL; s = s->next) {
						if (s->iindex < 0) {
							/* change phi */
							if (ls->phi[s->b_index][-s->iindex-1][0] == lt2->v_index)
								ls->phi[s->b_index][-s->iindex-1][0] = lt1->v_index;
							pred=graph_get_first_predecessor(gd, s->b_index, &iter);
							for (; pred != -1; pred = graph_get_next(&iter)) {
								/* change in appropriate phi_move, too */
								for(k=0; k<ls->num_phi_moves[pred]; k++) {
									if (ls->phi_moves[pred][k][0] ==
										lt2->v_index) {
										ls->phi_moves[pred][k][0]=lt1->v_index;
									}
								}
							}
						} else {
							/* change ICMD */

							iptr = ls->basicblocks[s->b_index]->iinstr
								+ s->iindex;
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
							case ICMD_ASTORE: /* iptr->op1 == DEF */
								if(iptr->op1 == lt2->v_index)
									iptr->op1 = lt1->v_index;
								break;
							case ICMD_IINC: /* iptr->val._i.op1_t == DEF */
								if (iptr->val._i.op1_t == lt2->v_index)
									iptr->val._i.op1_t = lt1->v_index;
								break;
								/* 							default: */
								/* could be in another than the top stackslot */
								/* too! */
								/* 								_LT_ASSERT((iptr->dst->varnum == lt1->v_index)); */
							}
						}
					}
				}
				
				/* combine def sites (SSA is dead now)*/
				_LT_ASSERT(lt2->def != NULL);
				for(s = lt2->def; s->next != NULL; s = s->next);
				s->next = lt1->def;
				lt1->def = lt2->def;

				/* combine use sites */
				_LT_ASSERT(lt2->use != NULL);
				for(s = lt2->use; s->next != NULL; s = s->next);
				s->next = lt1->use;
				lt1->use = lt2->use;

				/* combine first_def and last_use */
				if (lt1->bb_first_def == lt2->bb_first_def) {
					lt1->i_first_def = min(lt1->i_first_def, lt2->i_first_def);
				} else if (lt1->bb_first_def > lt2->bb_first_def) {
					lt1->bb_first_def = lt2->bb_first_def;
					lt1->i_first_def  = lt2->i_first_def;
				}
				if (lt1->bb_last_use == lt2->bb_last_use) {
					lt1->i_last_use = max(lt1->i_last_use, lt2->i_last_use);
				} else if (lt1->bb_last_use < lt2->bb_last_use) {
					lt1->bb_last_use = lt2->bb_last_use;
					lt1->i_last_use  = lt2->i_last_use;
				}

				/* combine savedvar flags */
				lt1->savedvar |= lt2->savedvar;
				_LT_ASSERT(lt1->type == lt2->type);
				lt2->type = -1;
				/* change the var in all future references of ls->igraph */
				/* TODO: do something against this!! */
/* 				for(j = i + 1; j < ls->igraph_top; j++) { */
/* 					if (ls->igraph[j].inter == NULL) { */
/* 						for(var1 = ls->igraph[j].vars; var1 != NULL;  */
/* 							var1 = var1->next) { */
/* 							if (var1->v == lt2->v_index) */
/* 								var1->v = lt1->v_index; */
/* 						} */
/* 					} */
#if 0
					/* not needed by now, since only for phi functions */
					/* with absolutely no interference is checked */
					else {
						inter = ls->igraph[j].inter;
						for(;inter != NULL; inter = inter->next) {
							if (inter->v1 == lt2->v_index)
								inter->v1 = lt1->v_index;
							if (inter->v2 == lt2->v_index)
								inter->v2 = lt1->v_index;
						}
					}
#endif
/* 				} */

				/* look through all phi functions */
				for(j = 0; j < ls->basicblockcount; j++) {
					for(k = 0; k < ls->max_vars; k++) {
						if (ls->phi[j][k] != NULL) {
							if (ls->phi[j][k][0] == lt2->v_index)
								ls->phi[j][k][0] = lt1->v_index;	
							for (l = 1; l < graph_get_num_predecessor(gd, j); l++) {
								if (ls->phi[j][k][l] == lt2->v_index)
									ls->phi[j][k][l] = lt1->v_index;	
							}
						}
					}
					/* change in phi move, too */
					for(k = 0; k < ls->num_phi_moves[j]; k++) {
						if (ls->phi_moves[j][k][1] == lt2->v_index)
							ls->phi_moves[j][k][1] = lt1->v_index;
						if (ls->phi_moves[j][k][0] == lt2->v_index)
							ls->phi_moves[j][k][0] = lt1->v_index;
					}
				}
			} /* for(var = ls->igraph[i].vars; var != NULL; var = var->next) */
		} /* if (ls->igraph[i].inter == NULL) */
	} /* for(i = 0; i < ls->igraph_top; i++) */
	}
#endif
}

void LifeOutAtBlock(lsradata *ls, graphdata *gd, int *M, int n, 
					struct lifetime *lt) {
	/* lt->v_index is life at Block n */
	if (!bv_get_bit(M,n)) { /* n no Element of M */
		bv_set_bit(M,n);

		/* lt->v_index is life at last Statement of n */
		if (n != 0) {
			int i;
			i = ls->basicblocks[n]->icount - 1;
			for (;((i>0) && (ls->basicblocks[n]->iinstr+i == ICMD_NOP)); i--);
			LifeOutAtStatement(ls, gd, M, n, i,lt);
		}
		else
			LifeOutAtStatement(ls, gd, M, n, 0, lt);
	}
}

void LifeInAtStatement(lsradata *ls, graphdata *gd, int *M, int b_index, 
					   int iindex, struct lifetime *lt) {
	/* lt->v_index is live-in at s */
	int prev_iindex; /* Statement before iindex */
	int pred;
	graphiterator pred_iter;
	
	lt_is_live(ls, lt, b_index, iindex);

	prev_iindex = iindex - 1;
	if (prev_iindex < 0)
		/* look through phi functions */
		for(; prev_iindex > -ls->max_vars-1; 
			prev_iindex--)
			if (ls->phi[b_index][-prev_iindex-1] != NULL)
				break;

	if (iindex == -ls->max_vars-1) { 
		/* iindex is the first statement of b_index */
		/* Statements -ls->max_vars-1 .. -1 are possible phi functions */
		/* lt->v_index is live-in at b_index */
		
		pred = graph_get_first_predecessor(gd, b_index, &pred_iter);
		for(; pred != -1; pred = graph_get_next(&pred_iter))
			LifeOutAtBlock(ls, gd, M, pred, lt);
	} else
		LifeOutAtStatement(ls, gd, M, b_index, prev_iindex, lt);
}

void LifeOutAtStatement(lsradata *ls, graphdata *gd, int *M, int b_index, 
						int iindex, struct lifetime *lt) {
	instruction *iptr;
	int igraph_index;
	int var;
	/* lt->v_index is life-out at s */

	/* for each variable w defined at s except v=lt->v_index, add a edge */
	/* (v,w) to the interference graph, once one is needed */

#if defined(JOIN_PHI_LT)
	/* Build interference Graph for variables involved in phi functions */
	/* it is essential, that these variables get merged, if possible!   */
	build_interference(ls, b_index, iindex, lt);
#endif
	if (!v_is_defined_at_s(ls, b_index, iindex, lt)) {
		/* v is life in at out of statement -> check if the SAVEDVAR */
		/* flag is needed to be set */
		if ((iindex >= 0) && (b_index != 0)) {
			/* real ICMD */
			_LT_ASSERT(ls->basicblocks[b_index]->iinstr != NULL);
			iptr = ls->basicblocks[b_index]->iinstr + iindex;
			if (icmd_table[iptr->opc].flags & ICMDTABLE_CALLS)
				lt->savedvar = SAVEDVAR;
		}
		LifeInAtStatement(ls, gd, M, b_index, iindex, lt);
	} else
		lt_is_live(ls, lt, b_index, iindex);
}

struct stackslot *lsra_make_ss(stackptr s, int bb_index)
{
	struct stackslot *ss;

	ss=DNEW(struct stackslot);
	ss->bb=bb_index;
	ss->s=s;
	return ss;
}

void lsra_add_ss(struct lifetime *lt, stackptr s) {
	struct stackslot *ss;
	bool  insert_s;
	/* Stackslot noch nicht eingetragen? */

	insert_s = true;

	for (ss = lt->local_ss; (!insert_s) && (ss != NULL); ss = ss->next)
		insert_s = (ss->s == s);
		

	/* local_ss == NULL -> stack lt was set in ssa -> create Stack entry */
	if ((lt->local_ss == NULL) || (insert_s)) {
		ss = DNEW(struct stackslot);
		ss->s = s;
		ss->s->varnum = lt->v_index;
		ss->next = lt->local_ss;
		lt->local_ss = ss;
		if (s != NULL) lt->savedvar |= s->flags & SAVEDVAR;
		if (s != NULL) lt->type = s->type;
	}
}

void move_stackslots(struct lifetime *from, struct lifetime *to) {
	struct stackslot *ss;
	
	if (from->local_ss == NULL)
		/* nothing to move */
		return;

	for(ss = from->local_ss; ss->next != NULL; ss = ss->next);

	ss->next = to->local_ss;
	to->local_ss = from->local_ss;
	from->local_ss = NULL;
}
void move_use_sites(struct lifetime *from, struct lifetime *to) {
	struct site *s;

	_LT_ASSERT(from->use != NULL);
	if (from->use == NULL)
		return;
	for(s = from->use; s->next != NULL; s = s->next);

	s->next = to->use;
	to->use = from->use;
	from->use = NULL;
}

void add_use_site(struct lifetime *lt, int block, int iindex) {
	struct site *n;

	n = DNEW(struct site);
	n->b_index = block;
	n->iindex = iindex;
	n->next = lt->use;
	lt->use = n;

	/* CFG is analysed from the end to the start -> so first found use site */
	/* is the last use of the Local Var */
	if (lt->last_use == NULL)
		lt->last_use = n;
}

void remove_use_site(struct lifetime *lt, int block, int iindex) {
	struct site *n;

	/* check lt->use itself */
	if ((lt->use->b_index == block) && (lt->use->iindex == iindex)) {
		/* found */
		lt->use = lt->use->next;
	} else {
		/* look through list */
		for (n = lt->use; (n->next != NULL) && ((n->next->b_index != block) ||
									(n->next->iindex != iindex)); n = n->next);
		/* assert, that lt was found */
		_LT_ASSERT(n->next != NULL);
		_LT_ASSERT(n->next->b_index == block);
		_LT_ASSERT(n->next->iindex == iindex);

		n->next = n->next->next;
	}
}

void add_def_site(struct lifetime *lt, int block, int iindex) {
	struct site *n;

	/* SSA <-> only one definition per lifetime! */
	_LT_ASSERT(lt->def == NULL);
	n = DNEW(struct site);
	n->b_index = block;
	n->iindex = iindex;
	n->next = NULL;
	lt->def = n;
}

struct lifetime *get_ss_lifetime(lsradata *ls, stackptr s) {
	struct lifetime *n;
	
	if (s->varnum >= 0) { /* new stackslot lifetime */
		if (-ls->v_index - 1 >= ls->maxlifetimes) {
			printf("%i %i\n", -ls->v_index - 1, ls->maxlifetimes);
		}
		_LT_ASSERT(-ls->v_index - 1 < ls->maxlifetimes);

		n = &(ls->lifetime[-ls->v_index - 1]);
		n->type = s->type;
		n->v_index = ls->v_index--;
		n->usagecount = 0;
		
		n->bb_last_use = -1;
		n->bb_first_def = -1;
		n->local_ss = NULL;
		n->savedvar = 0;
		n->flags = 0;
		
		n->use = NULL;
		n->def = NULL;
		n->last_use = NULL;
	} else {
		_LT_ASSERT(-s->varnum - 1 < ls->maxlifetimes);
		n = &(ls->lifetime[-s->varnum - 1]);
	}

	lsra_add_ss( n, s);
	return n;
}

#define lsra_new_stack(ls, s, block, instr) \
	if ((s)->varkind != ARGVAR) _lsra_new_stack(ls, s, block, instr, LSRA_STORE)
void _lsra_new_stack(lsradata *ls, stackptr s, int block, int instr, int store)
{
	struct lifetime *n;

	if (s->varkind == LOCALVAR) {
/* 		_LT_ASSERT(0); */
		lsra_usage_local(ls, s->varnum, s->type, block, instr, LSRA_LOAD);
	} else {
		

		n=get_ss_lifetime( ls, s );
		if (store == LSRA_STORE) {
			/* for LSRA_BB_[IN|OUT] do not add a def site, just add s to */
			/* local_ss */
			add_def_site(n, block, instr);
		}
	}
}

#define lsra_from_stack(ls, s, block, instr) \
	if ((s)->varkind != ARGVAR) _lsra_from_stack(ls, s, block, instr, LSRA_LOAD)
#define lsra_pop_from_stack(ls, s, block, instr) \
	if ((s)->varkind != ARGVAR) _lsra_from_stack(ls, s, block, instr, LSRA_POP)
void _lsra_from_stack(lsradata *ls, stackptr s, int block, int instr, int store)
{
	struct lifetime *n;

	if (s->varkind == LOCALVAR) {
/* 		_LT_ASSERT(0); */
		lsra_usage_local(ls, s->varnum, s->type, block, instr, LSRA_LOAD);
	} else /* if (s->varkind != ARGVAR) */ {
		if (s->varkind == STACKVAR ) {
/* 			_LT_ASSERT(0); */
			printf("---------STACKVAR left over! \n");
			/* No STACKVARS possible with lsra! */
			s->varkind = TEMPVAR;
		}
	}

		n=get_ss_lifetime( ls, s );
		
		/* LSRA_POP -> invalidate Stackslot ?! */
#ifdef USAGE_COUNT
		n->usagecount += ls->nesting[block];
#endif

		add_use_site(n, block, instr);

}

void lsra_usage_local(lsradata *ls, s4 v_index, int type, int block, int instr,
					  int store)
{
	struct lifetime *n;

	n = &(ls->lifetime[ls->maxlifetimes + v_index]);

	if (n->type == -1) { /* new local lifetime */
		n->local_ss=NULL;
		n->v_index=v_index;
		n->type=type;
		n->savedvar = SAVEDVAR;
		n->flags = 0;
		n->usagecount = 0;

		n->bb_last_use = -1;
		n->bb_first_def = -1;

		n->use = NULL;
		n->def = NULL;
		n->last_use = NULL;
	}
 	_LT_ASSERT(type == n->type);

	/* add access at (block, instr) to instruction list */
	/* remember last USE, so only write, if USE Field is undefined (==-1)   */
	/* count store as use, too -> defined and not used vars would overwrite */
	/* other vars */
	if (store == LSRA_LOAD) {
#ifdef USAGE_COUNT
		n->usagecount += ls->nesting[block];
#endif
		add_use_site(n, block, instr);
	}
	if (store == LSRA_STORE) {
		add_def_site(n, block, instr);
	}
}	

/***************************************************************************
use sites: dead code elemination, LifenessAnalysis
def sites: dead code elemination
***************************************************************************/
void _scan_lifetimes(registerdata *rd, lsradata *ls, graphdata *gd,
					basicblock *bptr)
{
/* 	methodinfo         *lm; */
	builtintable_entry *bte;
	methoddesc         *md;
	int i, j, t, v;
	int opcode;
	int iindex, b_index;
	stackptr    src;
	stackptr    dst;
	instruction *iptr;
   
	struct lifetime *lt;


	if (bptr->flags >= BBREACHED) {

		b_index = bptr->nr;

		/* get instruction count for BB */
		iindex = bptr->icount - 1;
		/* regard not setup new BB with maybee just in and outstack */
		if (iindex < 0) iindex = 0;

		/* Regard phi_functions (Definition of target, Use of source) */
		for(i = 0; i < ls->max_vars; i++) {
			if (ls->phi[b_index][i] != NULL) {
				/* Phi Function for var i at b_index exists */
				v = ls->phi[b_index][i][0];
				_LT_ASSERT( v != ls->max_vars_with_indices);
				if (v >= 0) {
					/* Local Var */
					for(t = 0;(t < 5) && (rd->locals[v][t].type==-1); t++);
					_LT_ASSERT(t != 5);
					/* Add definition of target add - phi index -1*/
					lsra_usage_local(ls, v, t, b_index, -i-1,
									 LSRA_STORE);
					/* Add Use of sources */
					for (j = 1; j <= graph_get_num_predecessor(gd, b_index);
						 j++) {
						if (ls->phi[b_index][i][j] != ls->max_vars_with_indices)
							lsra_usage_local(ls, ls->phi[b_index][i][j], t,
											 b_index, -i-1, LSRA_LOAD);
					}
				} else {
					/* Interface Stackslot */
					/* Add definition of target */
					lt = &(ls->lifetime[-v-1]);
					add_def_site(lt, b_index, -i-1);
					/* add use of sources */
					for (j = 1; j <= graph_get_num_predecessor(gd, b_index);
						 j++) {
						if (ls->phi[b_index][i][j] != ls->max_vars_with_indices)
						{
							lt = &(ls->lifetime[-ls->phi[b_index][i][j]-1]);
							add_use_site(lt, b_index, -i-1);
						}
					}
				}
			} 
		}

		src = bptr->instack;
		if (bptr->type != BBTYPE_STD) {
#ifdef LT_DEBUG_CHECK
			if (src == NULL) {
			   log_text("No Incoming Stackslot for Exception/Subroutine BB\n");
				_LT_ASSERT(0);
			}
#endif
			_LT_ASSERT(src != NULL);
			lsra_new_stack(ls, src, b_index, 0);
			if (src->varkind == STACKVAR)
				src->varkind = TEMPVAR;
			src = src->prev;
		}
		for (;src != NULL; src=src->prev) {
			/* no ARGVAR possible at BB Boundaries with LSRA! */
			/* -> change to TEMPVAR                           */

			if (src->varkind == ARGVAR ) {
				src->varkind = TEMPVAR;
				/* On Architectures with own return registers a return    */
				/* stackslot is marked as varkind=ARGVAR with varnum=-1   */
				/* but for lsra a varkind==TEMPVAR, varnum=-1 would mean, */
				/* that already a lifetime was allocated! */
				if (src->varnum < 0) src->varnum = 0;
			}
			else if (src->varkind == LOCALVAR )
				/* only allowed for topmost ss at sbr or exh entries! */
				{ log_text("LOCALVAR at basicblock instack\n"); exit(1); } 
			else {
				/* no Interfaces (STACKVAR) at BB Boundaries with LSRA! */
				/* -> change to TEMPVAR                      */
				if (src->varkind == STACKVAR )
					src->varkind = TEMPVAR;
/* 				_lsra_new_stack(ls, src, b_index, 0, LSRA_BB_IN); */
				_lsra_from_stack(ls, src, b_index, 0, LSRA_BB_OUT);
			}
		}
		/* Should not be necessary to check out stackslots, too */
		/* either they are identical to the instacks or handled */
		/* by their phi functions */
		src = bptr->outstack;
		for (;src != NULL; src=src->prev) {
			if (src->varkind == ARGVAR )  
				{ log_text("ARGVAR at basicblock outstack\n"); exit(1); }
			else if (src->varkind == LOCALVAR )
				{ log_text("LOCALVAR at basicblock outstack\n"); exit(1); }
			else {
				/* no Interfaces at BB Boundaries with LSRA! */
				/* -> change to TEMPVAR                      */
				if (src->varkind == STACKVAR )
					src->varkind = TEMPVAR;
/* 				_lsra_from_stack(ls, src, b_index, iindex, LSRA_BB_OUT); */
			}
		}

		if (bptr->iinstr != NULL) {
			/* set iptr to last instruction of BB */
			iptr = bptr->iinstr + iindex;
		} else
			iindex = -1;

		for (;iindex >= 0; iindex--, iptr--)  {
			/* Get source and destination Stack for the current           */
			/* instruction. Destination stack is available as iptr->dst   */
			dst = iptr->dst;
			/* source stack is either the destination stack of the previos*/
			/* instruction, or the basicblock instack for the             */
			/* first instruction */
			if (iindex) /* != 0 is > 0 here, since iindex ist always >= 0 */
				src=(iptr-1)->dst;
			else
				src=bptr->instack;

			opcode = iptr->opc;
			switch (opcode) {

				/* pop 0 push 0 */
			case ICMD_RET:
				/* local read (return adress) */
				lsra_usage_local(ls, iptr->op1, TYPE_ADR, b_index, iindex,
								 LSRA_LOAD);
				break;
			case ICMD_NOP:
/* 			case ICMD_ELSE_ICONST: */
			case ICMD_CHECKNULL:
			case ICMD_JSR:
			case ICMD_RETURN:
			case ICMD_GOTO:
			case ICMD_PUTSTATICCONST:
			case ICMD_INLINE_START:
			case ICMD_INLINE_END:
			case ICMD_INLINE_GOTO:
				break;
                             
			case ICMD_IINC:
				/* local = local+<const> */
				lsra_usage_local(ls, iptr->op1, TYPE_INT, b_index, iindex, 
								 LSRA_LOAD);
				lsra_usage_local(ls, iptr->val._i.op1_t, TYPE_INT, b_index,
								 iindex, LSRA_STORE);
				break;

				/* pop 0 push 1 const: const->stack */
			case ICMD_ICONST:
			case ICMD_LCONST:
			case ICMD_FCONST:
			case ICMD_DCONST:
			case ICMD_ACONST:
				/* new stack slot */
				lsra_new_stack(ls, dst, b_index, iindex);
				break;

				/* pop 0 push 1 load: local->stack */
			case ICMD_ILOAD:
			case ICMD_LLOAD:
			case ICMD_FLOAD:
			case ICMD_DLOAD:
			case ICMD_ALOAD:
				if (dst->varkind != LOCALVAR) {
					/* local->value on stack */
					lsra_usage_local(ls, iptr->op1, opcode - ICMD_ILOAD,
									 b_index,  iindex, LSRA_LOAD);
					lsra_new_stack(ls, dst, b_index, iindex); 
				} else /* if (dst->varnum != iptr->op1) */ {
					/* local -> local */
					lsra_usage_local(ls, iptr->op1, opcode - ICMD_ILOAD,
									 b_index,  iindex,LSRA_LOAD); 
					lsra_usage_local(ls, dst->varnum, opcode - ICMD_ILOAD,
									 b_index, iindex, LSRA_STORE);
				}

				break;

				/* pop 2 push 1 */
				/* Stack(arrayref,index)->stack */
			case ICMD_IALOAD:
			case ICMD_LALOAD:
			case ICMD_FALOAD:
			case ICMD_DALOAD:
			case ICMD_AALOAD:

			case ICMD_BALOAD:
			case ICMD_CALOAD:
			case ICMD_SALOAD:
				/* stack->index */
				lsra_from_stack(ls, src, b_index, iindex); 
				/* stack->arrayref */
				lsra_from_stack(ls, src->prev, b_index, iindex); 
				/* arrayref[index]->stack */
				lsra_new_stack(ls, dst, b_index, iindex); 
				break;

				/* pop 3 push 0 */
				/* stack(arrayref,index,value)->arrayref[index]=value */
			case ICMD_IASTORE:
			case ICMD_LASTORE:
			case ICMD_FASTORE:
			case ICMD_DASTORE:
			case ICMD_AASTORE:

			case ICMD_BASTORE:
			case ICMD_CASTORE:
			case ICMD_SASTORE:

				lsra_from_stack(ls, src,b_index, iindex);
				lsra_from_stack(ls, src->prev, b_index, iindex);
				lsra_from_stack(ls, src->prev->prev, b_index, iindex); 
				break;

				/* pop 1 push 0 store: stack -> local */
			case ICMD_ISTORE:
			case ICMD_LSTORE:
			case ICMD_FSTORE:
			case ICMD_DSTORE:
			case ICMD_ASTORE:
				if (src->varkind != LOCALVAR) {
					lsra_from_stack(ls, src, b_index, iindex);
					lsra_usage_local(ls, iptr->op1, opcode-ICMD_ISTORE,
									 b_index, iindex, LSRA_STORE);
				} else /* if (src->varnum != iptr->op1) */ {
					lsra_usage_local(ls, iptr->op1, opcode-ICMD_ISTORE,
									 b_index, iindex, LSRA_STORE);
					lsra_usage_local(ls, src->varnum, opcode-ICMD_ISTORE,
									 b_index,  iindex, LSRA_LOAD); 
				}
				break;

				/* pop 1 push 0 */
			case ICMD_POP: /* throw away a stackslot */
				/* TODO: check if used anyway (DUP...) and change codegen */
				/* to ignore this stackslot */
#if 0
				lsra_pop_from_stack(ls, src, b_index, iindex);
#endif
			break;

				/* pop 1 push 0 */
			case ICMD_IRETURN:
			case ICMD_LRETURN:
			case ICMD_FRETURN:
			case ICMD_DRETURN:
			case ICMD_ARETURN: /* stack(value) -> [empty]    */

			case ICMD_ATHROW:  /* stack(objref) -> undefined */

			case ICMD_PUTSTATIC: /* stack(value) -> static_field */
			case ICMD_PUTFIELDCONST:

				/* pop 1 push 0 branch */
			case ICMD_IFNULL: /* stack(value) -> branch? */
			case ICMD_IFNONNULL:

			case ICMD_IFEQ:
			case ICMD_IFNE:
			case ICMD_IFLT:
			case ICMD_IFGE:
			case ICMD_IFGT:
			case ICMD_IFLE:

			case ICMD_IF_LEQ:
			case ICMD_IF_LNE:
			case ICMD_IF_LLT:
			case ICMD_IF_LGE:
			case ICMD_IF_LGT:
			case ICMD_IF_LLE:

				/* pop 1 push 0 table branch */
			case ICMD_TABLESWITCH:
			case ICMD_LOOKUPSWITCH:

			case ICMD_MONITORENTER:
			case ICMD_MONITOREXIT:
				lsra_from_stack(ls, src, b_index, iindex);
				break;

				/* pop 2 push 0 */
			case ICMD_POP2: /* throw away 2 stackslots */
#if 0
				/* TODO: check if used anyway (DUP...) and change codegen */
				/* to ignore this stackslot */
				lsra_pop_from_stack(ls, src, b_index, iindex);
				lsra_pop_from_stack(ls, src->prev, b_index, iindex);
#endif
				break;

				/* pop 2 push 0 branch */

			case ICMD_IF_ICMPEQ: /* stack (v1,v2) -> branch(v1,v2) */
			case ICMD_IF_ICMPNE:
			case ICMD_IF_ICMPLT:
			case ICMD_IF_ICMPGE:
			case ICMD_IF_ICMPGT:
			case ICMD_IF_ICMPLE:

			case ICMD_IF_LCMPEQ:
			case ICMD_IF_LCMPNE:
			case ICMD_IF_LCMPLT:
			case ICMD_IF_LCMPGE:
			case ICMD_IF_LCMPGT:
			case ICMD_IF_LCMPLE:

			case ICMD_IF_ACMPEQ:
			case ICMD_IF_ACMPNE:

				/* pop 2 push 0 */
			case ICMD_PUTFIELD: /* stack(objref,value) -> objref = value */

			case ICMD_IASTORECONST:
			case ICMD_LASTORECONST:
			case ICMD_AASTORECONST:
			case ICMD_BASTORECONST:
			case ICMD_CASTORECONST:
			case ICMD_SASTORECONST:
				lsra_from_stack(ls, src, b_index, iindex); 	 
				lsra_from_stack(ls, src->prev, b_index, iindex); 
				break;

				/* pop 0 push 1 dup */
			case ICMD_DUP: 
				/* src == dst->prev */
				/* ---------------- */
				/* src -> dst       */

				/* Add the use site for src==dst */
				lsra_from_stack(ls, src, b_index, iindex);

				lsra_new_stack(ls, dst, b_index, iindex);

				break;

				/* pop 0 push 2 dup */
			case ICMD_DUP2:
				/* src       == dst->prev->prev       */
				/* src->prev == dst->prev->prev->prev */
				/* ---------------- */
				/* src       -> dst                   */
				/* src->prev -> dst->prev             */
				/* src & src->prev "continue" living -> so no conflicts */
				/* with dst and dst->prec possible                      */
				
				/* add the use site for src == dst->prev->prev */
				lsra_from_stack(ls, src, b_index, iindex);
				/* add the use site for src->prev == dst->prev->prev->prev */
				lsra_from_stack(ls, src->prev, b_index, iindex);

			
				lsra_new_stack(ls, dst->prev, b_index, iindex);
				lsra_new_stack(ls, dst, b_index, iindex); 

				break;

				/* pop 2 push 3 dup */
			case ICMD_DUP_X1:
				/* src       -> dst             */
				/* src->prev -> dst->prev       */
				/* src       -> dst->prev->prev */
				/* !!!!!!!!!!!!!!!!!!!!!!!!!!!! */
				/* Copy Conflicts possible!     */
				/* -> instack [    t1 t0 ]      */
				/* -> outstack[ t0 t1 t3 ]      */
				/* -> t1->t0, t0->t1, t1->t3 !! */
				/* -> Remove src->prev on iindex+1 instead of iindex! */
				lsra_from_stack(ls, src, b_index, iindex); 
				lsra_from_stack(ls, src->prev, b_index, iindex);
				lsra_new_stack(ls, dst->prev->prev, b_index, iindex);
				lsra_new_stack(ls, dst->prev, b_index, iindex);
				lsra_new_stack(ls, dst, b_index, iindex); 

				break;

				/* pop 3 push 4 dup */
			case ICMD_DUP_X2:
				/* src             -> dst  */
				/* src             -> dst->prev->prev->prev */
				/* src->prev       -> dst->prev */
				/* src->prev->prev -> dst->prev->prev */
				/* Conflicts possible! -> remove srces at iindex + 1 */
				lsra_from_stack(ls, src,b_index, iindex); 
				lsra_from_stack(ls, src->prev, b_index, iindex);
				lsra_from_stack(ls, src->prev->prev, b_index, iindex);
				lsra_new_stack(ls, dst->prev->prev->prev, b_index, iindex);
				lsra_new_stack(ls, dst->prev->prev, b_index, iindex);
				lsra_new_stack(ls, dst->prev, b_index, iindex);
				lsra_new_stack(ls, dst, b_index, iindex); 

				break;

				/* pop 3 push 5 dup */
			case ICMD_DUP2_X1:
				/* src             -> dst  */
				/* src             -> dst->prev->prev->prev */
				/* src->prev       -> dst->prev->prev->prev->prev */
				/* src->prev       -> dst->prev */
				/* src->prev->prev -> dst->prev->prev */
				/* Conflicts possible! -> remove srces at iindex + 1 */
				lsra_from_stack(ls, src, b_index, iindex); 
				lsra_from_stack(ls, src->prev, b_index, iindex); 
				lsra_from_stack(ls, src->prev->prev, b_index, iindex); 
				lsra_new_stack(ls, dst->prev->prev->prev->prev, b_index,
							   iindex);
				lsra_new_stack(ls, dst->prev->prev->prev, b_index, iindex);
				lsra_new_stack(ls, dst->prev->prev, b_index, iindex);
				lsra_new_stack(ls, dst->prev, b_index, iindex);
				lsra_new_stack(ls, dst, b_index, iindex); 


				break;

				/* pop 4 push 6 dup */
			case ICMD_DUP2_X2:
				/* src                   -> dst  */
				/* src                   -> dst->prev->prev->prev->prev */
				/* src->prev         -> dst->prev->prev->prev->prev->prev */
				/* src->prev             -> dst->prev */
				/* src->prev->prev       -> dst->prev->prev */
				/* src->prev->prev->prev -> dst->prev->prev->prev */
				/* Conflicts possible! -> remove srcs at iindex + 1 */
				lsra_from_stack(ls, src, b_index, iindex); 
				lsra_from_stack(ls, src->prev, b_index, iindex); 
				lsra_from_stack(ls, src->prev->prev, b_index, iindex); 
				lsra_from_stack(ls, src->prev->prev->prev, b_index, iindex); 
				lsra_new_stack(ls, dst->prev->prev->prev->prev->prev,
							   b_index, iindex);
				lsra_new_stack(ls, dst->prev->prev->prev->prev, b_index,
							   iindex);
				lsra_new_stack(ls, dst->prev->prev->prev, b_index, iindex);
				lsra_new_stack(ls, dst->prev->prev, b_index, iindex);
				lsra_new_stack(ls, dst->prev, b_index, iindex);
				lsra_new_stack(ls, dst, b_index, iindex); 

				break;

				/* pop 2 push 2 swap */
			case ICMD_SWAP:
				/* src                   -> dst->prev  */
				/* src->prev             -> dst */
				/* Conflicts possible -> remove src at iindex + 1 */
				lsra_from_stack(ls, src, b_index, iindex); 
				lsra_from_stack(ls, src->prev, b_index, iindex);
				lsra_new_stack(ls, dst->prev, b_index, iindex);
				lsra_new_stack(ls, dst, b_index, iindex);
				break;

				/* pop 2 push 1 */
					
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
				lsra_from_stack(ls, src, b_index, iindex);
				lsra_from_stack(ls, src->prev, b_index, iindex);
				lsra_new_stack(ls, dst, b_index, iindex);
				break;

			case ICMD_ISUB:
				lsra_from_stack(ls, src, b_index, iindex);
				lsra_from_stack(ls, src->prev,b_index,iindex);
				lsra_new_stack(ls, dst, b_index, iindex);
				break;

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
				lsra_from_stack(ls, src, b_index, iindex);
				lsra_from_stack(ls, src->prev, b_index, iindex);
				lsra_new_stack(ls, dst, b_index, iindex);
				break;

				/* pop 1 push 1 */
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

/* 			case ICMD_IFEQ_ICONST: */
/* 			case ICMD_IFNE_ICONST: */
/* 			case ICMD_IFLT_ICONST: */
/* 			case ICMD_IFGE_ICONST: */
/* 			case ICMD_IFGT_ICONST: */
/* 			case ICMD_IFLE_ICONST: */

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
				lsra_from_stack(ls, src, b_index, iindex);
				lsra_new_stack(ls, dst, b_index, iindex);
				break;

			case ICMD_ARRAYLENGTH:
			case ICMD_INSTANCEOF:

			case ICMD_NEWARRAY:
			case ICMD_ANEWARRAY:

			case ICMD_GETFIELD:
				lsra_from_stack(ls, src, b_index, iindex);
				lsra_new_stack(ls, dst, b_index, iindex);
				break;

				/* pop 0 push 1 */
			case ICMD_GETSTATIC:

			case ICMD_NEW:
				lsra_new_stack(ls, dst, b_index, iindex);
				break;

				/* pop many push any */

			case ICMD_INVOKESTATIC:
			case ICMD_INVOKESPECIAL:
			case ICMD_INVOKEVIRTUAL:
			case ICMD_INVOKEINTERFACE:
				INSTRUCTION_GET_METHODDESC(iptr,md);
				i = md->paramcount;
				while (--i >= 0) {
					lsra_from_stack(ls, src, b_index, iindex);
					src = src->prev;
				}
				if (md->returntype.type != TYPE_VOID)
					lsra_new_stack(ls, dst, b_index, iindex);
				break;

			case ICMD_BUILTIN:
				bte = iptr->val.a;
				md = bte->md;
				i = md->paramcount;
				while (--i >= 0) {
					lsra_from_stack(ls, src, b_index, iindex);
					src = src->prev;
				}
				if (md->returntype.type != TYPE_VOID)
					lsra_new_stack(ls, dst, b_index, iindex);
				break;

			case ICMD_MULTIANEWARRAY:
				i = iptr->op1;
				while (--i >= 0) {
					lsra_from_stack(ls, src, b_index, iindex);
					src = src->prev;
				}
				lsra_new_stack(ls, dst, b_index, iindex);
				break;

			default:
/* 				assert(0); */
				throw_cacao_exception_exit(string_java_lang_InternalError,
										   "Unknown ICMD %d during register allocation", iptr->opc);
			} /* switch */
		} /* for (;iindex >= 0; iindex--, iptr--) */
	} /* if (bptr->flags >= BBREACHED) */
} /* scan_lifetimes */


#ifdef USAGE_COUNT
/*******************************************************************************
true, if i dominates j
*******************************************************************************/
bool dominates(dominatordata *dd, int i, int j) {
	bool dominates = false;

	while(!dominates && (dd->idom[j] != -1)) {
		dominates = (i == dd->idom[j]);
		j = dd->idom[j];
	}
	return dominates;
}

/*******************************************************************************
lt_get_nesting

Look for loops in the CFG and set the nesting depth of all Basicblocks in 
gd->nesting:

The Loop Header BB h is an element of DF[n] for all Basicblocks n of this loop
So Look through all x element of DF[n] for a backedge n->x. If this
exists, increment nesting for all n with x in DF[n]
*******************************************************************************/
void lt_get_nesting(lsradata *ls, graphdata *gd, dominatordata *dd) {
	int i, j, lh;
	bitvector loop_header;
	worklist *loop, *loop1;

	int succ;
	graphiterator iter;

	int num_loops;

	int *loop_parent;
	int lh_p;

	/* init nesting to 1 and get loop_headers */
	ls->nesting = DMNEW(long, ls->basicblockcount);
	loop_header = bv_new(ls->basicblockcount);
	loop = wl_new(ls->basicblockcount);
	num_loops = 0;
	for(i = 0; i < ls->basicblockcount; i++) {
		ls->nesting[i] = 1;

		for(succ = graph_get_first_successor(gd, i, &iter); succ != -1;
			succ = graph_get_next(&iter)) {
			for (j = 0; j < dd->num_DF[i]; j++) {
				if (succ == dd->DF[i][j]) {
					/* There is an edge from i to DF[i][j] */

					/* look if DF[i][j] dominates i -> backedge */
					if (dominates(dd, dd->DF[i][j], i)) {
						/* this edge is a backedge */
						/* -> DF[i][j] is a loop header */
						_LT_CHECK_BOUNDS(dd->DF[i][j], 0, ls->basicblockcount);
						if (!bv_get_bit(loop_header, dd->DF[i][j])) {
							/* new loop_header found */
							num_loops++;
							bv_set_bit(loop_header, dd->DF[i][j]);
							ls->nesting[dd->DF[i][j]] = 10;
						}
						wl_add(loop, dd->DF[i][j]);
					}
				}
			}
		}
	}

	loop_parent = DMNEW(int , ls->basicblockcount);
	loop1 = wl_new(ls->basicblockcount);

	/* look for direct parents of nested loopheaders */
	/* (DF[loop_header[i]] has the element loop_header[j] with i != j */
	/* TODO: BULLSHIT:unfortunately not such an easy condition ;( */
	while(!wl_is_empty(loop)) {
		lh = wl_get(loop);
		wl_add(loop1, lh);

		loop_parent[lh] = -1;

		for (j = 0; j < dd->num_DF[lh]; j++) {
			_LT_CHECK_BOUNDS(dd->DF[lh][j], 0, ls->basicblockcount);
			if (lh != dd->DF[lh][j]) {
				if (bv_get_bit(loop_header, dd->DF[lh][j])) {
#ifdef LT_DEBUG_VERBOSE
					if (compileverbose)
						if (loop_parent[lh] != -1)
							printf("Warning: LoopHeader has more than one parent\n");
#endif
/* 					_LT_ASSERT( loop_parent[lh] == -1); */
					loop_parent[lh] = dd->DF[lh][j];
				}
			}
		}
	}

	/* create nesting for loopheaders */
	while(!wl_is_empty(loop1)) {
		lh = wl_get(loop1);
		for (lh_p = lh; lh_p != -1; lh_p = loop_parent[lh_p]) {
			ls->nesting[lh] *= 10;
		}
	}


	/* copy loopheader nesting to loop body */
	for(i = 0; i < ls->basicblockcount; i++) {
		if (!bv_get_bit(loop_header, i)) {
			/* Do not touch the nesting of a loopheader itself */
			for(j = 0; j < dd->num_DF[i]; j++) {
				_LT_CHECK_BOUNDS(dd->DF[i][j], 0, ls->basicblockcount);
				if (bv_get_bit(loop_header, dd->DF[i][j])) {
					/* DF[i][j] is a loop header -> copy nesting for i */
#ifdef LT_DEBUG_VERBOSE
					if (compileverbose)
						if (ls->nesting[i] != 1)
							printf("Warning: More than one loopheader for one BB\n");
/* 					_LT_ASSERT(ls->nesting[i] == 1); */
#endif
					ls->nesting[i] = ls->nesting[dd->DF[i][j]];
				}
			}
		}
	}

#ifdef LT_DEBUG_VERBOSE
	if (compileverbose) {
		printf("Num Loops: %3i\n",num_loops);
		for(i = 0; i < ls->basicblockcount; i++)
			printf("(BB%3i->N%3li) ",i, ls->nesting[i]);
		printf("\n");
	}
#endif
}
#endif


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

/* src/vm/jit/loop/analyze.c - bound check removal functions

   Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
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

   Contact: cacao@cacaojvm.org

   Authors: Christopher Kruegel

   Changes: Christian Thalinger

   Contains the functions which perform the bound check removals. With
   the loops identified, these functions scan the code for array
   accesses that take place in loops and try to guarantee that their
   bounds are never violated. The function to call is
   optimize_loops().

   $Id: analyze.c 5925 2006-11-05 23:11:27Z edwin $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm/types.h"

#include "mm/memory.h"
#include "toolbox/logging.h"
#include "vm/jit/jit.h"
#include "vm/jit/loop/analyze.h"
#include "vm/jit/loop/graph.h"
#include "vm/jit/loop/loop.h"
#include "vm/jit/loop/tracing.h"

 
#ifdef LOOP_DEBUG

/*	Test functions -> will be removed in final release
*/

void show_trace(struct Trace *trace)
{
	if (trace != NULL) {
		switch (trace->type) {
		case TRACE_IVAR:
			printf("int-var");
			printf("\nNr.:\t%d", trace->var);
			printf("\nValue:\t%d", trace->constant);
			break;
      
		case TRACE_AVAR:
			printf("object-var");
			printf("\nNr.:\t%d", trace->var);
			break;
      
		case TRACE_ALENGTH:
			printf("array-length");
			printf("\nNr.:\t%d", trace->var);
			printf("\nValue:\t%d", trace->constant);
			break;

		case TRACE_ICONST:
			printf("int-const");
			printf("\nValue:\t%d", trace->constant);
			break;
      
		case TRACE_UNKNOWN:
			printf("unknown");
			break;
			}
		}
	else
		printf("Trace is null");
	
	printf("\n");
}


void show_change(struct Changes *c)
{
	printf("*** Changes ***\n");
	if (c != NULL)
		printf("Lower/Upper Bound:\t%d/%d\n", c->lower_bound, c->upper_bound);
	else
		printf("Unrestricted\n");
}

show_varinfo(struct LoopVar *lv)
{
	printf("   *** Loop Info ***\n");
	printf("Value:\t%d\n", lv->value);
	printf("Static:\t\t%d/%d\n", lv->static_l, lv->static_u);
	printf("D-Valid:\t%d/%d\n", lv->dynamic_l_v, lv->dynamic_u_v);
	printf("Dynamic\t\t%d/%d\n", lv->dynamic_l, lv->dynamic_u);
}

void show_right_side(methodinfo *m)
{
	int i;
	printf("\n   *** Head ***   \nType:\t");
	show_trace(m->loopdata->c_rightside);

	printf("\n   *** Nested Loops: ***\n");
	for (i=0; i<m->basicblockcount; ++i) 
		printf("%d\t", m->loopdata->c_nestedLoops[i]);
	printf("\n");

	printf("\n   *** Hierarchie: ***\n");	
	for (i=0; i<m->basicblockcount; ++i) 
		printf("%d\t", m->loopdata->c_hierarchie[i]);
	printf("\n");
	

	printf("\n   *** Current Loop ***\n");
	for (i=0; i<m->basicblockcount; ++i)
	    printf("%d\t", m->loopdata->c_current_loop[i]);
	printf("\n");
}

void resultPass3(methodinfo *m)
{
	int i;
	struct LoopContainer *lc = m->loopdata->c_allLoops;
  
	printf("\n\n****** PASS 3 ******\n\n");
  
	while (lc != NULL) {
		printf("Loop Analysis:\n");
		printf("Optimize:\t%d\n", lc->toOpt);
		printf("Modified Vars: ");
		/*
		for (i=0; i<lc->num_vars; ++i)
		  printf("%d ", lc->vars[i]);
		printf("\n\n");
		*/
		lc = lc->next;
		}

	printf("\nNested Loops:\n");
	for (i=0; i<m->basicblockcount; ++i)
	    printf("%d ", m->loopdata->c_nestedLoops[i]);
	printf("\n");
	for (i=0; i<m->basicblockcount; ++i) 
		printf("%d ", m->loopdata->c_hierarchie[i]);
	printf("\n");
	fflush(stdout);
}

void show_tree(struct LoopContainer *lc, int tabs) 
{
	int cnt;

	while (lc != NULL) {
		for (cnt = 0; cnt < tabs; ++cnt)
			printf("  ");
		printf("%d\n", lc->loop_head);

		show_tree(lc->tree_down, tabs+1);

		lc = lc->tree_right;
	}
}

#endif

#ifdef ENABLE_STATISTICS

void show_loop_statistics(loopdata *ld)
{
	printf("\n\n****** LOOP STATISTICS ****** \n\n");
	if (ld->c_stat_or) 
	    printf("Optimization cancelled by or\n");
	else if (ld->c_stat_exception)
	    printf("Optimization cancelled by exception\n");
	else {
		printf("Number of array accesses:\t%d\n", ld->c_stat_array_accesses);
		if (ld->c_stat_array_accesses) {
			printf("\nFully optimized:\t%d\n", ld->c_stat_full_opt);
			printf("Not optimized:\t\t%d\n", ld->c_stat_no_opt);
			printf("Upper optimized:\t%d\n", ld->c_stat_upper_opt);
			printf("Lower optimized:\t%d\n", ld->c_stat_lower_opt);
			}
		}
}

void show_procedure_statistics(loopdata *ld)
{
	printf("\n\n****** PROCEDURE STATISTICS ****** \n\n");
	printf("Number of loops:\t\t%d\n", ld->c_stat_num_loops);
	printf("Number of array accesses:\t%d\n", ld->c_stat_sum_accesses);
	if (ld->c_stat_sum_accesses) {
		printf("\nFully optimized:\t%d\n", ld->c_stat_sum_full);
		printf("Not optimized:\t\t%d\n", ld->c_stat_sum_no);
		printf("Upper optimized:\t%d\n", ld->c_stat_sum_upper);
		printf("Lower optimized:\t%d\n", ld->c_stat_sum_lower);
		}
	printf("Opt. cancelled by or:\t\t%d\n", ld->c_stat_sum_or);
	printf("Opt. cancelled by exception:\t%d\n", ld->c_stat_sum_exception);
}

#endif


/*	This function is used to merge two loops with the same header together.
	A simple merge sort of the lists nodes of both loops is performed.
*/
void analyze_merge(struct LoopContainer *l1, struct LoopContainer *l2)
{
	struct LoopElement *start, *last, *le1, *le2; 
	/* start and last are pointers to the newly built list, le1 and le2 step  */
	/* step through the lists, that have to be merged.                        */

	le1 = l1->nodes;
	le2 = l2->nodes;

	/* start a simple merge sort of the nodes of both loops. These lists are  */
	/* already sorted, so merging is easy.                                    */
	if (le1->node < le2->node) {
		start = last = le1;
		le1 = le1->next;
		}
	else if (le1->node == le2->node) {
		start = last = le1;
		le1 = le1->next;
		le2 = le2->next;
		}
	else {
		start = last = le2;
		le2 = le2->next;
		}

	/* while the first loop != NULL, depending of the first element of second */
	/* loop, add new node to result list                                      */
	while (le1 != NULL) {

		if (le2 == NULL) {
			last->next = le1;
			break;
			}
		if (le1->node < le2->node) {
			last->next = le1;
			le1 = le1->next;
			}
		else if (le1->node == le2->node) {
			last->next = le1;
			le1 = le1->next;
			le2 = le2->next;
			last = last->next;
			}
		else {
			last->next = le2;
			le2 = le2->next;
			last = last->next;
			}
		}

	last->next = le2;			
}


/*	This function is used to merge loops with the same header node to a single 
	one. O(n^2) of number of loops. This merginig is necessary, because the loop
	finding algorith sometimes (eg. when loopbody ends with a if-else construct)
	reports a single loop as two loops with the same header node.
*/
void analyze_double_headers(loopdata *ld)
{
	int toCheck;
	LoopContainer *t1, *t2, *t3;

	t1 = ld->c_allLoops;

	while (t1 != NULL)	{			/* for all loops do							*/
		toCheck = t1->loop_head;	/* get header node							*/
		t2 = t1->next;

		while (t2 != NULL) {		/* compare it to headers of rest			*/
			if (t2->loop_head == toCheck) {

				/* found overlapping loops -> merge them together				*/
				/* printf("C_INFO: found overlapping loops - merging");         */
				analyze_merge(t1, t2);
				
				/* remove second loop from the list	of all loops				*/
				t3 = t1;       
				while (t3->next != t2)
					t3 = t3->next;
				t3->next = t2->next;
				}
			t2 = t2->next;
		    }

		t1 = t1->next;
	    }
}


/* After the hierarchie of loops has been built, we have to insert the exceptions
   into this tree. The exception ex is inserted into the subtree pointed to by
   LoopContainer lc.
*/
void insert_exception(methodinfo *m, struct LoopContainer *lc, exceptiontable *ex)
{
	struct LoopContainer *temp;
	struct LoopElement *le;

#ifdef LOOP_DEBUG
	/* printf("insert_exception called with %d-%d and loop %d\n", ex->start->nr, ex->end->nr, lc->loop_head); */
#endif
	
	/* if child node is reached immediately insert exception into the tree    */
	if (lc->tree_down == NULL) {
		ex->next = lc->exceptions;
		lc->exceptions = ex;
	    }
	else {
	/* if we are inside the tree, there are two possibilities:                */
	/* 1. the exception is inside a nested loop or                            */
	/* 2. in the loop body of the current loop                                */

		/* check all children (= nested loops)                                */
		temp = lc->tree_down;
		
		while (temp != NULL) {
			
			le = temp->nodes;
			while (le != NULL) {

#ifdef LOOP_DEBUG
				printf("%d.%d\n", le->node, block_index[ex->startpc]);
#endif
				/* if the start of the exception is part of the loop, the     */
				/* whole exception must be part of the loop                   */
				if (le->node == m->basicblockindex[ex->startpc])
					break;
				le = le->next;
			    }
			
			/* Exception is part of a nested loop (Case 1) -> insert it there */
			if (le != NULL) {
				insert_exception(m, temp, ex);
				return;
			    }
			else if ((temp->loop_head >= m->basicblockindex[ex->startpc]) && (temp->loop_head < m->basicblockindex[ex->endpc])) {
				
				/* optimization: if nested loop is part of the exception, the */
				/* exception cannot be part of a differnet nested loop.       */
				ex->next = lc->exceptions;
				lc->exceptions = ex;
				return;
			    }
			else
				temp = temp->tree_right;
		    }
		    
		/* Exception is not contained in any nested loop (Case 2)             */
		if (temp == NULL) {
			ex->next = lc->exceptions;
			lc->exceptions = ex;
		    }
	    } 
}


/*	This function builds a loop hierarchie. The header node of the innermost loop,
	each basic block belongs to, is stored in the array c_nestedLoops. The array
	c_hierarchie stores the relationship between differnt loops in as follows: 
    Each loop, that is a nested loop, stores its direct surrounding loop as a 
    parent. Top level loops have no parents.
*/

void analyze_nested(methodinfo *m, codegendata *cd, loopdata *ld)
{
	/* i/count/tmp are counters                                               */
	/* toOverwrite is used while loop hierarchie is built (see below)         */
	int i, header, toOverwrite, tmp, len;

	/* first/last are used during topological sort to build ordered loop list */
	struct LoopContainer *first, *last, *start, *t, *temp;

	/* Used to step through all nodes of a loop.                              */
	struct LoopElement *le; 

	/* init global structures                                                 */
	ld->c_nestedLoops = DMNEW(int, m->basicblockcount);
	ld->c_hierarchie = DMNEW(int, m->basicblockcount); 	
	for (i=0; i<m->basicblockcount; ++i) {
		ld->c_nestedLoops[i] = -1;
		ld->c_hierarchie[i] = -1;
	    }

	/* if there are no optimizable loops -> return                            */
	if (ld->c_allLoops == NULL)
		return;

	temp = ld->c_allLoops;
	while (temp != NULL) {              /* for all loops, do                  */
		header = temp->loop_head;

		/* toOverwrite is number of current parent loop (-1 if none)          */
		toOverwrite = ld->c_nestedLoops[header];	

		ld->c_hierarchie[header] = toOverwrite;

		if (toOverwrite == header)      /* check for loops with same header   */
			printf("C_ERROR: Loops have same header\n");

		le = temp->nodes;
		while (le != NULL) {            /* for all loop nodes, do             */
			tmp = ld->c_nestedLoops[le->node];

		    /* if node is part of parent loop -> overwrite it with nested     */
			if (tmp == toOverwrite)
				ld->c_nestedLoops[le->node] = header;
			else {
				ld->c_hierarchie[tmp] = header;
#ifdef LOOP_DEBUG
				/* printf("set head of %d to %d", tmp, header);               */
#endif
			    }

			le = le->next;
			}

		temp = temp->next;
		}

	/* init root of hierarchie tree                                           */
	ld->root = DMNEW(struct LoopContainer, 1);
	LoopContainerInit(m, ld->root, -1);

    /* obtain parent pointer and build hierarchie tree                        */
    start = ld->c_allLoops;    
    while (start != NULL) {
		
		/* look for parent of loop pointed at by start                        */
		first = ld->c_allLoops;
		while (first != NULL) {

			/* the parent of the loop, pointed at by start has been found     */
			if (first->loop_head == ld->c_hierarchie[start->loop_head]) {
#ifdef LOOP_DEBUG
				/* printf("set parent to pointer\n");                         */
#endif

				start->parent = first;
				start->tree_right = first->tree_down;
				first->tree_down = start;

				break;
			    }
			first = first->next;
		    }

		/* no parent loop found, set parent to root                           */
		if (first == NULL) {
#ifdef LOOP_DEBUG
			/* printf("set parent to root\n");                                */
#endif
 
			start->parent = ld->root;
			start->tree_right = ld->root->tree_down;
			ld->root->tree_down = start;		
		    }
		/* if a parent exists, increase this nodes indegree                   */
		else
			start->parent->in_degree += 1;

		start = start->next;
	    }

	/* insert exceptions into tree                                            */
#ifdef LOOP_DEBUG
	printf("--- Showing tree ---\n");
	show_tree(ld->root, 0);
	printf(" --- End ---\n");
#endif
	for (len = 0; len < jd->exceptiontablelength; ++len) 
		insert_exception(m, ld->root, jd->exceptiontable + len);


	/* determine sequence of loops for optimization by topological sort       */

	/* init queue                                                             */
	start = NULL;
	temp = ld->c_allLoops;
	while (temp != NULL) {

		/* a loops with indegree == 0 are pushed onto the stack               */
		if (temp->in_degree == 0) {
			t = temp->next;
			temp->next = start;
			start = temp;
			}
		else 
			t = temp->next;
		    
		temp = t;
		}

	/* sort loops                                                             */
	first = last = start;
	start = start->next;

	if (last == NULL) {
		printf("C_ERROR: loops are looped\n");
		exit(-1);
	    }

	/* pop each node from the stack and decrease its parents indegree by one  */
	/* when the parents indegree reaches zero, push it onto the stack as well */
	if ((last->parent != ld->root) && (--last->parent->in_degree == 0)) {
		last->parent->next = start;
		start = last->parent;
		}
	while (start != NULL) {

		last->next = start;

		start = start->next;
		last = last->next;
		
		if ((last->parent != ld->root) && (--last->parent->in_degree == 0)) {
			last->parent->next = start;
			start = last->parent;
			}
		}

	last->next = NULL;
	ld->c_allLoops = first;

#ifdef LOOP_DEBUG
	printf("*** Hierarchie Results \n");
	while (first != NULL) {
		printf("%d ", first->loop_head);
		first = first->next;
	    }
	printf("\n");
	fflush(stdout);
#endif 
}


/*	This function is used to add variables that occur as index variables in
	array accesses (ARRAY_INDEX) or as variables, that change their value (VAR_MOD)
	to the list of interesting vars (c_loopvars) for the current loop.
*/

void add_to_vars(loopdata *ld, int var, int type, int direction)
{
	struct LoopVar *lv;	

	/* printf("Added to vars %d %d %d\n", var, type, direction);              */
	lv = ld->c_loopvars;
	while (lv != NULL) {            /* check if var has been previously added */
		if (lv->value == var) {
			if (type == ARRAY_INDEX)
				lv->index = 1;              /* var is used as index           */
			else if (type == VAR_MOD) {
				lv->modified = 1;           /* var is used in assignment      */
				switch (direction) {        /* how was var modified ?         */
				case D_UP:
					lv->static_u = 0;       /* incremented, no static upper   */
					break;                  /* bound can be guaranteeed       */
				case D_DOWN:
					lv->static_l = 0;       /* decremented, no static lower   */
					break;                  /* bound can be guaranteeed       */
				case D_UNKNOWN:
					lv->static_u = lv->static_l = 0;
					break;                  /* no info at all                 */
				default:
					printf("C_ERROR: unknown direction\n");
					break;
					}
				}
			return;
			}
		lv = lv->next;
		}

	/* variable is not found in list -> add variable to list					*/
	lv = DNEW(struct LoopVar);

	lv->modified = lv->index = 0;
	lv->value = var;

	if (type == ARRAY_INDEX) {
		lv->index = 1;
		lv->static_u = lv->static_l = 1;    /* arrayindex -> var not modified */
		}
	else if (type == VAR_MOD) {
		lv->modified = 1;
		switch (direction) {                /* var used in assignment -> set  */
		case D_UP:                          /* proper static bounds           */
			lv->static_u = 0; lv->static_l = 1;
			break;
		case D_DOWN:
			lv->static_u = 1; lv->static_l = 0;
			break;
		case D_UNKNOWN:
			lv->static_u = lv->static_l = 0;
			break;
		default:
			printf("C_ERROR: unknown direction\n");
			break;
			}
		}

	/* no dynamic bounds have been determined so far                          */
	lv->dynamic_l = lv->dynamic_l_v = lv->dynamic_u = lv->dynamic_u_v = 0;

	lv->next = ld->c_loopvars;                  /* add var to list                */
	ld->c_loopvars = lv;
}


/*	This function checks, whether a given loop with header node contains array
	accesses. If so, it returns 1, else it returns 0 and the loops needs no
	further consideration in the optimization process. When array accesses are 
	found, a list of all variables, that are used as array index, is built and 
	stored in c_loopvars. For all variables (integer), which values are changed, 
	a flag in c_var_modified is set.
*/

int analyze_for_array_access(methodinfo *m, loopdata *ld, int node)
{
	basicblock bp;
	instruction *ip;
	int ic, i, access;
	struct depthElement *d;
	struct Trace *t;

	if (ld->c_toVisit[node] > 0) {          /* node has not been visited yet      */
		ld->c_toVisit[node] = 0;
   
		bp = m->basicblocks[node];               /* prepare an instruction scan        */
		ip = bp.iinstr;
		ic = bp.icount;

		access = 0;                     /* number of array accesses in loop   */

		for (i=0; i<ic; ++i, ++ip) {    /* for each instruction, check opcode */
			switch (ip->opc) {
			case ICMD_IASTORE:          /* array store                        */
			case ICMD_LASTORE:          
			case ICMD_FASTORE:          
			case ICMD_DASTORE:          
			case ICMD_AASTORE:          
			case ICMD_BASTORE:          
			case ICMD_CASTORE:          
			case ICMD_SASTORE:
				t = tracing(&bp, i-1, 1);   /* try to identify index variable */

				if (t->type == TRACE_IVAR) {
					/* if it is a variable, add it to list of index variables */
					add_to_vars(ld, t->var, ARRAY_INDEX, D_UNKNOWN);
					access++;				
				}
				else if (t->type == TRACE_ICONST)
					access++;
				break;
      
			case ICMD_IALOAD:				/* array load						*/
		    case ICMD_LALOAD:       
			case ICMD_FALOAD:
			case ICMD_DALOAD:
			case ICMD_AALOAD:
			case ICMD_BALOAD:
			case ICMD_CALOAD:
			case ICMD_SALOAD:
				t = tracing(&bp, i-1, 0);   /* try to identify index variable */
		
				if (t->type == TRACE_IVAR) {
					/* if it is a variable, add it to list of index variables */
					add_to_vars(ld, t->var, ARRAY_INDEX, D_UNKNOWN);
					access++;
					}
				else if (t->type == TRACE_ICONST)
					access++;
				break;

			case ICMD_ISTORE:				/* integer store					*/
				ld->c_var_modified[ip->op1] = 1;

				/* try to find out, how it was modified							*/
				t = tracing(&bp, i-1, 0);	
				if (t->type == TRACE_IVAR) {
					if ((t->constant > 0) && (t->var == ip->op1))
						/* a constant was added	to the same var					*/
						add_to_vars(ld, t->var, VAR_MOD, D_UP);
					else if (t->var == ip->op1)	
						/* a constant was subtracted from the same var			*/
						add_to_vars(ld, t->var, VAR_MOD, D_DOWN);
					else
						add_to_vars(ld, t->var, VAR_MOD, D_UNKNOWN);
					}
				else
					add_to_vars(ld, ip->op1, VAR_MOD, D_UNKNOWN);
				break;

			case ICMD_IINC:					/* simple add/sub of a constant		*/
				ld->c_var_modified[ip->op1] = 1;
		
				if (ip->val.i > 0)
					add_to_vars(ld, ip->op1, VAR_MOD, D_UP);
				else
					add_to_vars(ld, ip->op1, VAR_MOD, D_DOWN);
				break;

			case ICMD_LSTORE:
			case ICMD_FSTORE:
			case ICMD_DSTORE:
			case ICMD_ASTORE:
				ld->c_var_modified[ip->op1] = 1;
				break;
			}
		}

		d = ld->c_dTable[node];
		while (d != NULL) {					/* check all successors of block	*/
			access += analyze_for_array_access(m, ld, d->value);
			d = d->next;
			}

		return access;
		}
	else
		return 0;
}


/*	This function scans the exception graph structure to find modifications of
	array index variables of the current loop. If any modifications are found,
	1 is returned, else 0.
*/

int quick_scan(methodinfo *m, loopdata *ld, int node)
{
	basicblock bp;
	instruction *ip;
	int count, i;
	struct LoopVar *lv;
	struct depthElement *d;
 
	/*  printf("QS: %d - %d\n", node, ld->c_exceptionVisit[node]);					*/
   

	if (ld->c_exceptionVisit[node] > 0) {	/* node is part of exception graph		*/
		ld->c_exceptionVisit[node] = -1;
		
		bp = m->basicblocks[node];				/* setup scan of all instructions		*/
		ip = bp.iinstr;
		count = bp.icount;				

		for (i=0; i<count; ++i, ++ip) {	/* for each instruction do				*/
			switch (ip->opc) {
			case ICMD_ISTORE:
			case ICMD_IINC:				/* a variable is modified				*/
	
				lv = ld->c_loopvars;		/* is it an array index var ?			*/
				while (lv != NULL) {
					if ((lv->index) && (lv->value == ip->op1))
						return 1;		/* yes, so return 1						*/
					lv = lv->next;
					}
				break;
				}
			}
  
	    d = ld->c_exceptionGraph[node];		/* check all successor nodes			*/
		while (d != NULL) {
			if (quick_scan(m, ld, d->value) > 0)
				return 1;				/* if an access is found return 1		*/
			d = d->next;
			}

		return 0;						/* nothing found, so return 0			*/
		}
	else
		return 0;
}


/*	This function returns 1, when the condition of the loop contains 
	or statements or when an array index variable is modified in any
	catch block within the loop.
*/

int analyze_or_exceptions(methodinfo *m, codegendata *cd, loopdata *ld, int head, struct LoopContainer *lc)
{
	struct depthElement *d;
	int i, k, value, flag, count;
	struct LoopElement *le;

	d = ld->c_dTable[head];
	count = flag = 0;

	/* analyze for or-statements												*/
#ifdef LOOP_DEBUG
	printf("*** Analyze for OR ... ");										
	fflush(stdout);
#endif

	while (d != NULL) {				/* for all successor nodes check if they	*/
		value = d->value;			/* are part of the loop						*/

		le = lc->nodes;

		while (le != NULL) {
			if (le->node == value)
				break;
			le = le->next;
			}

		if (le == NULL)				/* node is not part of the loop				*/
			++flag;					

		d = d->next;
		++count;
		}

	if ((count > 1) && (flag == 0)){/* if all successors part of the loop, exit */
#ifdef ENABLE_STATISTICS
		ld->c_stat_or++;
#endif
		return 0;
		}

	/* check for exceptions */
	/* printf("done\n*** Analyze for EXCEPTIONS(%d) . ", jd->exceptiontablelength);	*/

	if (!jd->exceptiontablelength)		/* when there are no exceptions, exit		*/
		return 1;

	if ((ld->c_exceptionGraph = (struct depthElement **) malloc(sizeof(struct depthElement *) * m->basicblockcount)) == NULL)
		c_mem_error();
	if ((ld->c_exceptionVisit = (int *) malloc(sizeof(int) * m->basicblockcount)) == NULL)
		c_mem_error();
	
	for (k=0; k<m->basicblockcount; ++k) {
		ld->c_exceptionVisit[k] = -1;
		ld->c_exceptionGraph[k] = NULL;
		}


	/* for all nodes that start catch block check whether they are part of loop	*/
	for (i = 0; i < ld->c_old_xtablelength; i++) {	
		value = m->basicblockindex[jd->exceptiontable[i].startpc];
   
		le = lc->nodes;
		while (le != NULL) {

			if (le->node == value)	{			/* exception is in loop			*/
#ifdef LOOP_DEBUG
				printf("C_INFO: Loop contains exception\n");					
				fflush(stdout);
#endif

				/* build a graph structure, that contains all nodes that are	*/
				/* part of the catc block										*/
				dF_Exception(m, ld, -1, m->basicblockindex[jd->exceptiontable[i].handlerpc]);

				/* if array index variables are modified there, return 0		*/
				if (quick_scan(m, ld, m->basicblockindex[jd->exceptiontable[i].handlerpc]) > 0) {
#ifdef ENABLE_STATISTICS
					ld->c_stat_exception++;
#endif
					/* printf("C_INFO: loopVar modified in exception\n");		*/
					return 0;
					}
				}
			le = le->next;
			}
		}

#ifdef LOOP_DEBUG
	printf("none ... done\n");												
	fflush(stdout);
#endif
	return 1;
}


/*	This function sets a flag in c_var_modified for all variables that have
	been found as part of an assigment in the loop.
*/

void scan_global_list(loopdata *ld)
{
	struct LoopVar *lv;
	lv = ld->c_loopvars;

	while (lv != NULL) {
		if (lv->modified)
			ld->c_var_modified[lv->value] = 1;
		lv = lv->next;
		}
}


/*	This function analyses the condition in the loop header and trys to find
	out, whether some dynamic guarantees can be set up.
*/

void init_constraints(methodinfo *m, loopdata *ld, int head)
{
	basicblock bp;
	instruction *ip;
	int ic, l_mod, r_mod, changed, operand;
	struct Trace *left, *right, *th;
	struct LoopVar *lv_left, *lv_right, *lh;

	/* prevent some compiler warnings */

	operand = 0;
	lv_left = NULL;
	lv_right = NULL;

	bp = m->basicblocks[head];
	ic = bp.icount;
	ip = bp.iinstr+(ic-1);	/* set ip to last instruction in header node		*/

	switch (ip->opc) {		/* check op-code									*/
		
	/* comparison against constant value										*/
	case ICMD_IFEQ:			/* ..., value ==> ...								*/
	case ICMD_IFLT:         /* ..., value ==> ...								*/
	case ICMD_IFLE:         /* ..., value ==> ...								*/
	case ICMD_IFGT:         /* ..., value ==> ...								*/
	case ICMD_IFGE:         /* ..., value ==> ...								*/
							/* op1 = target JavaVM pc, val.i = constant			*/

		left = tracing(&bp, ic-2, 0);	/* analyse left arg., right is constant	*/
		right = create_trace(TRACE_ICONST, -1, ip->val.i, 0);
		break;

	/* standard comparison														*/
	case ICMD_IF_ICMPEQ:    /* ..., value, value ==> ...						*/
	case ICMD_IF_ICMPLT:    /* ..., value, value ==> ...						*/
	case ICMD_IF_ICMPGT:    /* ..., value, value ==> ...						*/
	case ICMD_IF_ICMPLE:    /* ..., value, value ==> ...						*/
	case ICMD_IF_ICMPGE:    /* ..., value, value ==> ...						*/
		
		left = tracing(&bp, ic-2, 1);	/* get left and right argument			*/
		right = tracing(&bp, ic-2, 0);
		break;
	
	/* other condition															*/
	default:
		left = create_trace(TRACE_UNKNOWN, -1, 0, 0);
		right = create_trace(TRACE_UNKNOWN, -1, 0, 0);
		break;
		}

	/* analyse left and right side of comparison								*/
	l_mod = r_mod = 0;

	if (left->type == TRACE_IVAR) {	/* is a loop variable on left side ?		*/
		lv_left = ld->c_loopvars;
		while (lv_left != NULL) {
			if (lv_left->value == left->var) {
				l_mod = lv_left->modified;	/* yes, but has it been modified ?	*/	 
				break;				
				}
			lv_left = lv_left->next;
			}
		}

	if (right->type == TRACE_IVAR){	/* is a loop variable on right side ?		*/
		lv_right = ld->c_loopvars;
		while (lv_right != NULL) {
			if (lv_right->value == right->var) {
				r_mod = lv_right->modified;	/* yes, but has it been modified ?	*/
				break;
				}
			lv_right = lv_right->next;
			}
		}

	if ((l_mod - r_mod) == 0) {		/* both 1 or both 0 -> no dynamic contraints*/
		ld->c_rightside = NULL;			/* possible									*/
		return;
		}

	/* to simplify processing, make the left side the one, that contains the	*/
	/* modified variable														*/
	if (r_mod > l_mod) {
		th = left;    left = right;        right = th;
		lh = lv_left; lv_left = lv_right;  lv_right = lh;
		changed = 1;				/* set changed to true						*/
		}
	else
		changed = 0;				/* no change needed							*/ 

	/* make sure that right side's value does not change during loop execution	*/ 
	if (right->type == TRACE_UNKNOWN) {
		ld->c_rightside = NULL;
		return;
		}

	/* determine operands:														*/
	/* for further explaination a is modified, b nonmodified var				*/
	switch (ip->opc) {		/* check opcode again								*/	
	case ICMD_IFEQ:         /* ..., value ==> ...								*/
	case ICMD_IF_ICMPEQ:    /* ..., value, value ==> ...						*/
		operand = OP_EQ;				/* a == b								*/
		break;

	case ICMD_IFLE:         /* ..., value ==> ...								*/
	case ICMD_IF_ICMPLE:    /* ..., value, value ==> ...						*/
		if (changed)
			operand = OP_GE;			/* b<=a		-> a>=b						*/
		else {
			operand = OP_LT;			/* a<=b		-> a<(b+1)					*/ 
			if (left->constant != 0)
				left->constant -= 1;
			else
				right->constant += 1;	
			}
		break;

	case ICMD_IFLT:         /* ..., value ==> ...								*/
	case ICMD_IF_ICMPLT:    /* ..., value, value ==> ...						*/
		if (changed) {
			operand = OP_GE;			/* b<a		-> a>=(b+1)					*/
			if (left->constant != 0)
				left->constant -= 1;
			else
				right->constant += 1;	
			}
		else
			operand = OP_LT;			/* a<b		-> a<b						*/
		break;

	case ICMD_IFGT:         /* ..., value ==> ...								*/
	case ICMD_IF_ICMPGT:    /* ..., value, value ==> ...						*/
		if (changed)
			operand = OP_LT;			/* b>a		-> a<b						*/
		else {
			operand = OP_GE;			/* a>b		->	a>=(b+1)				*/
			if (left->constant != 0)
				left->constant -= 1;
			else
				right->constant += 1;
			}
		break;
		
	case ICMD_IFGE:         /* ..., value ==> ...								*/
	case ICMD_IF_ICMPGE:    /* ..., value, value ==> ...						*/
		if (changed) {
			operand = OP_LT;			/* b>=a		-> a<(b+1)					*/
			if (left->constant != 0)
				left->constant -= 1;
			else
				right->constant += 1;
			}
		else
			operand = OP_GE;			/* a>=b		-> a>=b						*/
		break;

	default:
		printf("C_ERROR: debugging error 0x00\n");
		}


	/* NOW:	left/lv_left -> loopVar												*/
	/*		right/lv_right -> const, nonmod. var, arraylength					*/
	switch (operand) {					/* check operand						*/
	case OP_EQ:
		lv_left->dynamic_u_v = 1;		/* upper + lower bound tested			*/
		lv_left->dynamic_l_v = 1;
	
		lv_left->dynamic_l = lv_left->dynamic_u = left->constant;
		break;

	case OP_LT:
		lv_left->dynamic_u_v = 1;		/* upper bound tested					*/
	
		lv_left->dynamic_u = left->constant;
		break;

	case OP_GE:
		lv_left->dynamic_l_v = 1;		/* lower bound tested					*/
	
		lv_left->dynamic_l = left->constant;
		break;

	default:
		printf("C_ERROR: debugging error 0x01\n");
		}

	ld->c_rightside = right;

	switch (ld->c_rightside->type) {
	case TRACE_ICONST:
		ld->c_rs_needed_instr = 1;
		break;
	case TRACE_ALENGTH:
		ld->c_rs_needed_instr = 2;
		break;
	case TRACE_IVAR:
		ld->c_rs_needed_instr = 3;
		break;
	default:
		printf("C_ERROR: wrong right-side type\n");
		}
}


/*	This function is needed to add and record new static tests (before loop
	entry) of variables to make guaratees for index variables. type states
	the kind of the test. arrayRef is the array, which length is tested
	against, varRef is the variable, that is testes and constant is the
	constant value, that is tested.
*/

void add_new_constraint(methodinfo *m,  codegendata *cd, loopdata *ld, int type, int arrayRef, int varRef, int constant)
{
	struct Constraint *tc;

	switch (type) {
	case TEST_ZERO:					/* a variable is tested against a const		*/

		tc = ld->c_constraints[varRef];	/* does a test already exist for this var ?	*/
		while (tc != NULL) {
			if (tc->type == TEST_ZERO) {
				if (constant < tc->constant)
					tc->constant = constant;
				return;				/* yes. update constant and return			*/
				}
				tc = tc->next;
			}

		/* insert a new test for this variable									*/
		if ((tc = (struct Constraint *) malloc(sizeof(struct Constraint))) == NULL)
			c_mem_error();
		tc->type     = TEST_ZERO;
		tc->varRef   = varRef;
		tc->constant = constant;
		tc->next     = ld->c_constraints[varRef];
		ld->c_constraints[varRef] = tc;
		ld->c_needed_instr += 3;

		break;

	case TEST_ALENGTH:				/* variable is tested against array length	*/

		tc = ld->c_constraints[varRef];	/* does a test already exist for this var ?	*/
		while (tc != NULL) {
			if ((tc->type == TEST_ALENGTH) && (tc->arrayRef == arrayRef)) {
				if (constant > tc->constant)
					tc->constant = constant;
				return;				/* yes. update constant and return			*/
				}
			tc = tc->next;
			}

		/* insert a new test for this variable									*/
		if ((tc = (struct Constraint *) malloc(sizeof(struct Constraint))) == NULL)
			c_mem_error();
		tc->type	 = TEST_ALENGTH;
		tc->arrayRef = arrayRef;
		tc->varRef   = varRef;
		tc->constant = constant;
		tc->next     = ld->c_constraints[varRef];
		ld->c_constraints[varRef] = tc;
		ld->c_needed_instr += 6;

		/* if arrayRef is not already tested against null, insert that test     */
		if (!(ld->c_null_check[arrayRef])) {
			ld->c_null_check[arrayRef] = 1;
			ld->c_needed_instr +=2;
		    }
			
		break;

	case TEST_CONST_ZERO:		
		/* done earlier															*/
		break;

	case TEST_CONST_ALENGTH:		/* a const is tested against array length	*/

		/* does a test already exist for this array								*/
		tc = ld->c_constraints[jd->maxlocals];
		while (tc != NULL) {
			if ((tc->type == TEST_CONST_ALENGTH) && (tc->arrayRef == arrayRef)) {
				if (constant > tc->constant)
					tc->constant = constant;
				return;				/* yes. update constant and return			*/
				}
			tc = tc->next;
			}
		
		/* insert a new test for this array										*/
		if ((tc = (struct Constraint *) malloc(sizeof(struct Constraint))) == NULL)
			c_mem_error();
		tc->type	 = TEST_CONST_ALENGTH;
		tc->arrayRef = arrayRef;
		tc->constant = constant;
		tc->next     = ld->c_constraints[jd->maxlocals];
		ld->c_constraints[jd->maxlocals] = tc;
		ld->c_needed_instr += 4;

		/* if arrayRef is not already tested against null, insert that test     */
		if (!(ld->c_null_check[arrayRef])) {
			ld->c_null_check[arrayRef] = 1;
			ld->c_needed_instr +=2;
		    }

		break;

	case TEST_UNMOD_ZERO:			/* test unmodified var against constant		*/

		/* search if test already exists										*/
		tc = ld->c_constraints[varRef];
		while (tc != NULL) {
			if (tc->type == TEST_UNMOD_ZERO) {
				if (constant < tc->constant)
					tc->constant = constant;
				return;				/* yes, so update constant					*/
				}
			tc = tc->next;
			}
		
		/* else, a new test is inserted											*/		
		if ((tc = (struct Constraint *) malloc(sizeof(struct Constraint))) == NULL)
			c_mem_error();
		tc->type	 = TEST_UNMOD_ZERO;
		tc->varRef   = varRef;
		tc->constant = constant;
		tc->next     = ld->c_constraints[varRef];
		ld->c_constraints[varRef] = tc;
		ld->c_needed_instr += 3;

		break;
	
	case TEST_UNMOD_ALENGTH:		/* test unmodified var against array length	*/

		/* search if test alreay exists											*/
		tc = ld->c_constraints[varRef];
		while (tc != NULL) {
			if ((tc->type == TEST_UNMOD_ALENGTH) && (tc->arrayRef == arrayRef)) {
				if (constant > tc->constant)
					tc->constant = constant;	
				return;				/* yes, so modify constants					*/
				}
			tc = tc->next;
			}
		
		/* create new entry														*/
		if ((tc = (struct Constraint *) malloc(sizeof(struct Constraint))) == NULL)
			c_mem_error();
		tc->type	 = TEST_UNMOD_ALENGTH;
		tc->varRef   = varRef;
		tc->arrayRef = arrayRef;
		tc->constant = constant;
		tc->next     = ld->c_constraints[varRef];
		ld->c_constraints[varRef] = tc;
		ld->c_needed_instr += 6;

		/* if arrayRef is not already tested against null, insert that test     */
		if (!(ld->c_null_check[arrayRef])) {
			ld->c_null_check[arrayRef] = 1;
			ld->c_needed_instr +=2;
		    }

		break;
	
	case TEST_RS_ZERO:				/* test right side of the loop condition	*/
									/* against a constant - needed by dynamic	*/
									/* checks									*/
		/*!! varRef -> maxlocals */
		/* search if test already exists										*/
		tc = ld->c_constraints[jd->maxlocals];
		while (tc != NULL) {
			if (tc->type == TEST_RS_ZERO) {
				if (constant < tc->constant)
					tc->constant = constant;
				return;				/* yes, so modify constants					*/
				}
			tc = tc->next;
			}

		/* create new entry														*/
		if ((tc = (struct Constraint *) malloc(sizeof(struct Constraint))) == NULL)
			c_mem_error();
		tc->type     = TEST_RS_ZERO;
		tc->constant = constant;
		tc->next     = ld->c_constraints[jd->maxlocals];
		ld->c_constraints[jd->maxlocals] = tc;
		ld->c_needed_instr += (2 + ld->c_rs_needed_instr);

		/* if arrayRef on right side is not already tested against null,        */
		/* insert that test                                                     */
		if ((ld->c_rightside->type == TRACE_ALENGTH) && (!(ld->c_null_check[ld->c_rightside->var]))) {
			ld->c_null_check[ld->c_rightside->var] = 1;
			ld->c_needed_instr +=2;
		    }

		break;
		
	case TEST_RS_ALENGTH:			/* test right side of the loop condition	*/
									/* against array length - needed by dynamic	*/
									/* checks									*/
		/*!! varRef -> maxlocals */
		/* search if test already exists										*/
		tc = ld->c_constraints[jd->maxlocals];
		while (tc != NULL)
		{
			if ((tc->type == TEST_RS_ALENGTH) && (tc->arrayRef == arrayRef))
			{
				if (constant > tc->constant)
					tc->constant = constant;
				return;				/* yes, so modify constants					*/
			}
			tc = tc->next;
		}

		/* create new entry														*/
		if ((tc = (struct Constraint *) malloc(sizeof(struct Constraint))) == NULL)
			c_mem_error();
		tc->type	 = TEST_RS_ALENGTH;
		tc->arrayRef = arrayRef;
		tc->constant = constant;
		tc->next     = ld->c_constraints[jd->maxlocals];
		ld->c_constraints[jd->maxlocals] = tc;
		ld->c_needed_instr += (3 + ld->c_rs_needed_instr);

		/* if arrayRef is not already tested against null, insert that test     */
		if (!(ld->c_null_check[arrayRef])) {
			ld->c_null_check[arrayRef] = 1;
			ld->c_needed_instr +=2;
		    }

		/* if arrayRef on right side is not already tested against null,        */
		/* insert that test                                                     */
		if ((ld->c_rightside->type == TRACE_ALENGTH) && (!(ld->c_null_check[ld->c_rightside->var]))) {
			ld->c_null_check[ld->c_rightside->var] = 1;
			ld->c_needed_instr +=2;
		    }
		break;

	}
}


/*	This functions adds new static (before loop enry) tests of variables to the
	program to be able to guarantee certain values for index variables in array
	access (to safely remove bound checks).
*/

int insert_static(methodinfo *m, codegendata *cd, loopdata *ld, int arrayRef, struct Trace *index, struct Changes *varChanges, int special)
{
	struct LoopVar *lv;
	int varRef;
	int high, low;
	
	/* printf("insert static check - %d\n", arrayRef);
	   show_trace(index);
	   show_change(varChanges);
	*/

	if (varChanges == NULL) {			/* the variable hasn't changed / const	*/
		if ((varChanges = (struct Changes *) malloc(sizeof(struct Changes))) == NULL)
			c_mem_error();
		varChanges->lower_bound = varChanges->upper_bound = 0;
		}

	switch (index->type) {				/* check index type						*/
	case TRACE_IVAR:					/* it is a variable						*/
		if (index->neg < 0) {			/* if it's a negated var, return		*/
#ifdef ENABLE_STATISTICS
			ld->c_stat_no_opt++;			
#endif
			return OPT_NONE;
			}

		varRef = index->var;
		high = low = 0;

		if (ld->c_var_modified[varRef])	{	/* volatile var							*/
			
			lv = ld->c_loopvars;			/* get reference to loop variable		*/

			while ((lv != NULL) && (lv->value != varRef))
				lv = lv->next;
			if (lv == NULL)
			  printf("C_ERROR: debugging error 0x02\n");

			/* show_varinfo(lv);												*/
			
			/* check existing static bounds and add new contraints on variable	*/
			/* to possibly remove bound checks									*/
			if (lv->static_l) {
				/* the var is never decremented, so we add a static test againt	*/
				/* constant														*/
				if (varChanges->lower_bound > varChanges->upper_bound)
					add_new_constraint(m, cd, ld, TEST_ZERO, arrayRef, varRef, index->constant);
				else
					add_new_constraint(m, cd, ld, TEST_ZERO, arrayRef, varRef, varChanges->lower_bound+index->constant);
				low = 1;
				}
			else if ((lv->dynamic_l_v) && (!special)) {
				/* the variable is decremented, but it is checked against a		*/
				/* bound in the loop condition									*/
				if (varChanges->lower_bound <= varChanges->upper_bound) {
					add_new_constraint(m, cd, ld, TEST_RS_ZERO, arrayRef, varRef, varChanges->lower_bound+index->constant+lv->dynamic_l);
					low = 1;
					}
				}

			if (lv->static_u) {
				/* the var is never incremented, so we add a static test againt	*/
				/* constant														*/
				if (varChanges->lower_bound > varChanges->upper_bound)
					add_new_constraint(m, cd, ld, TEST_ALENGTH, arrayRef, varRef, index->constant);
				else
					add_new_constraint(m, cd, ld, TEST_ALENGTH, arrayRef, varRef, varChanges->upper_bound+index->constant);
				high = 1;
				}
			else if ((lv->dynamic_u_v) &&  (!special)) {
				/* the variable is decremented, but it is checked against a		*/
				/* bound in the loop condition									*/
				if (varChanges->lower_bound <= varChanges->upper_bound) {
					add_new_constraint(m, cd, ld, TEST_RS_ALENGTH, arrayRef, varRef, varChanges->upper_bound+index->constant+lv->dynamic_u);
					high = 1;
					}
				}
			}
		else {							/* the var is never modified at all		*/
			add_new_constraint(m, cd, ld, TEST_UNMOD_ZERO, arrayRef, index->var, index->constant);
			add_new_constraint(m, cd, ld, TEST_UNMOD_ALENGTH, arrayRef, index->var, index->constant);
			low = high = 1;
			}
		
		/* if the addition of new variable tests made guarantees possible,		*/
		/* return the best possible optimization								*/
		if ((high > 0) && (low > 0)) {
			/* printf("fully optimzed\n");										*/
#ifdef ENABLE_STATISTICS
			ld->c_stat_full_opt++;			
#endif
			return OPT_FULL;
			}
		else if (high > 0) {
			/* printf("upper optimzed\n");										*/
#ifdef ENABLE_STATISTICS
			ld->c_stat_upper_opt++;			
#endif
			return OPT_UPPER;
			}
		else if (low > 0) {
			/* printf("lower optimzed\n");										*/
#ifdef ENABLE_STATISTICS
			ld->c_stat_lower_opt++;			
#endif
			return OPT_LOWER;
			}
		else {
			/* printf("not optimzed\n");										*/
#ifdef ENABLE_STATISTICS
			ld->c_stat_no_opt++;			
#endif
			return OPT_NONE;
			}
		break;

	case TRACE_ICONST:			/* if it is a constant, optimization is easy	*/
		if (index->constant < 0) {
#ifdef ENABLE_STATISTICS
			ld->c_stat_no_opt++;			
#endif
			return OPT_NONE;	/* negative index -> bad						*/
			}
		else {
			add_new_constraint(m, cd, ld, TEST_CONST_ALENGTH, arrayRef, 0, index->constant);
#ifdef ENABLE_STATISTICS
			ld->c_stat_full_opt++;			
#endif
			return OPT_FULL;	/* else just test constant against array length	*/
			}
		break;

	case TRACE_ALENGTH:			/* else, no optimizations possible				*/
	case TRACE_UNKNOWN: 
	case TRACE_AVAR:    
#ifdef ENABLE_STATISTICS
		ld->c_stat_no_opt++;			
#endif
		return OPT_NONE;
	}

	/* keep compiler happy */
	return 0;
}



/*	copy a stack and return the start pointer of the newly created one
*/
stackptr copy_stack_from(stackptr source) { 
	stackptr current, top;

	if (source == NULL)
		return NULL;

	/* copy first element                                                       */
	current = DMNEW(stackelement, 1);
	current->type = source->type;
	current->flags = source->flags;
	current->varkind = source->varkind;
	current->varnum = source->varnum;
	current->regoff = source->regoff;
	
	top = current;

	/* if there exist more, then copy the rest                                  */
	while (source->prev != NULL) {
		source = source->prev;
		current->prev = DMNEW(stackelement, 1);
		current->type = source->type;
		current->flags = source->flags;
		current->varkind = source->varkind;
		current->varnum = source->varnum;
		current->regoff = source->regoff;
		current = current->prev;
		}

	current->prev = NULL;
	return top;
}


/* The following defines are used in the procedure void create_static_checks(...)
   They add a new instruction with its corresponding stack manipulation and
   are used to build the new loop header of an optimized loop, where we have
   to check certain variables and constants against values to guarantee that 
   index values in array accesses remain with array bounds.

   inst: pointer to the new instruction
   tos: stackpointer before this operation is executed
   newstack: temporary stackptr
   stackdepth: counts the current stackdepth
   original start: blockpointer to the head of the new, optimized loop 
*/

/* Load a local integer variable v                                              */
#define LOAD_VAR(v) { \
	inst->opc = ICMD_ILOAD; \
	inst->op1 = v; \
	newstack = DMNEW(stackelement, 1); \
    inst->dst = newstack; \
	newstack->prev = tos; \
	newstack->type = TYPE_INT; \
	newstack->flags = 0; \
	newstack->varkind = LOCALVAR; \
	newstack->varnum = v; \
	tos = newstack; \
	inst++; \
	stackdepth++; \
	}

/* Load a constant with value c                                                 */
#define LOAD_CONST(c) { \
	inst->opc = ICMD_ICONST; \
	inst->op1 = 0; \
	inst->val.i = (c); \
	newstack = DMNEW(stackelement, 1); \
	newstack->prev = tos; \
	newstack->type = TYPE_INT; \
	newstack->flags = 0; \
	newstack->varkind = UNDEFVAR; \
	newstack->varnum = stackdepth; \
	tos = newstack; \
	inst->dst = tos; \
	inst++; \
	stackdepth++; \
	}

/* Load a local reference (adress) variable a                                   */
#define LOAD_ADDR(a) { \
	inst->opc = ICMD_ALOAD; \
	inst->op1 = a; \
	newstack = DMNEW(stackelement, 1); \
	newstack->prev = tos; \
	newstack->type = TYPE_ADR; \
	newstack->flags = 0; \
	newstack->varkind = LOCALVAR; \
	newstack->varnum = a; \
	tos = newstack; \
	inst->dst = tos; \
	inst++; \
	stackdepth++; \
	}

/* Insert a compare greater-or-equal and jump to the unoptimized loop, if the   */
/* comparison is true                                                           */
#define GOTO_NOOPT_IF_GE { \
	inst->opc = ICMD_IF_ICMPGE; \
    inst->target = original_start->copied_to; \
	if (tos->varkind == UNDEFVAR) \
		tos->varkind = TEMPVAR;  \
    tos = tos->prev; \
    if (tos->varkind == UNDEFVAR) \
		tos->varkind = TEMPVAR;  \
    tos = tos->prev; \
	inst->dst = tos; \
	inst++; \
	stackdepth -= 2; \
	}

/* Insert a compare greater than and jump to the unoptimized loop, if the       */
/* comparison is true                                                           */
#define GOTO_NOOPT_IF_GT { \
	inst->opc = ICMD_IF_ICMPGT; \
    inst->target = original_start->copied_to; \
	if (tos->varkind == UNDEFVAR) \
		tos->varkind = TEMPVAR;  \
    tos = tos->prev; \
    if (tos->varkind == UNDEFVAR) \
		tos->varkind = TEMPVAR;  \
    tos = tos->prev; \
	inst->dst = tos; \
	inst++; \
	stackdepth -= 2; \
	}


/* Insert a compare less-than and jump to the unoptimized loop, if the          */
/* comparison is true                                                           */
#define GOTO_NOOPT_IF_LT { \
	inst->opc = ICMD_IF_ICMPLT; \
    inst->target = original_start->copied_to; \
	if(tos->varkind == UNDEFVAR) \
		tos->varkind = TEMPVAR;  \
    tos = tos->prev; \
    if(tos->varkind == UNDEFVAR) \
		tos->varkind = TEMPVAR;  \
    tos = tos->prev; \
	inst->dst = tos; \
	inst++; \
	stackdepth -= 2; \
	}

/* Insert a compare if-not-null and jump to the unoptimized loop, if the        */
/* comparison is true                                                           */
#define GOTO_NOOPT_IF_NULL { \
	inst->opc = ICMD_IFNULL; \
    inst->target = original_start->copied_to; \
    if(tos->varkind == UNDEFVAR) \
		tos->varkind = TEMPVAR;  \
    tos = tos->prev; \
	inst->dst = tos; \
	inst++; \
	stackdepth -= 1; \
	}

/* Insert an add instruction, that adds two integer values on top of the stack  */
/* together                                                                     */
#define ADD { \
	inst->opc = ICMD_IADD; \
	if(tos->varkind == UNDEFVAR) \
		tos->varkind = TEMPVAR;  \
    tos = tos->prev; \
    if(tos->varkind == UNDEFVAR) \
		tos->varkind = TEMPVAR;  \
    tos = tos->prev; \
	newstack = DMNEW(stackelement, 1); \
	newstack->prev = tos; \
	newstack->type = TYPE_INT; \
	newstack->flags = 0; \
	newstack->varkind = UNDEFVAR; \
	newstack->varnum = stackdepth; \
	tos = newstack; \
	inst->dst = tos; \
	inst++; \
	stackdepth--; \
	}
		
/* Insert instructions to load the arraylength of an array with reference a     */
/* fisrt, the reference must be loaded, then a null-pointer check is inserted   */
/* if not already done earlier. Finally an arraylength instruction is added     */
#define LOAD_ARRAYLENGTH(a) { \
    if (ld->c_null_check[a]) { \
		LOAD_ADDR(a); \
		GOTO_NOOPT_IF_NULL; \
		ld->c_null_check[a] = 0; \
	    }  \
	LOAD_ADDR(a); \
    inst->opc = ICMD_ARRAYLENGTH; \
	if(tos->varkind == UNDEFVAR) \
		tos->varkind = TEMPVAR;  \
    tos = tos->prev; \
	newstack = DMNEW(stackelement, 1); \
	newstack->prev = tos; \
	newstack->type = TYPE_INT; \
	newstack->flags = 0; \
	newstack->varkind = UNDEFVAR; \
	newstack->varnum = stackdepth; \
	tos = newstack; \
	inst->dst = tos; \
	inst++; \
	}	


/* Inserts the instructions to load the value of the right side of comparison   */
/* Depending of the type of the right side, the apropriate instructions are     */
/* created.                                                                     */
#define LOAD_RIGHT_SIDE { \
	switch (ld->c_rightside->type) { \
	case TRACE_ICONST: \
		LOAD_CONST(ld->c_rightside->constant); \
		break; \
	case TRACE_IVAR: \
		LOAD_VAR(ld->c_rightside->var); \
		LOAD_CONST(ld->c_rightside->constant); \
		ADD; \
		break; \
	case TRACE_ALENGTH: \
		LOAD_ARRAYLENGTH(ld->c_rightside->var); \
		break; \
	default: \
		log_text("C_ERROR: illegal trace on rightside of loop-header"); \
		assert(0); \
	} \
}

/*	Patch jumps in original loop and in copied loop, add gotos in copied loop.
	All jumps in the original loop to the loop head have to be redirected to
	the newly inserted one. For the copied loop, it is necessay to redirect all
	jumps inside that loop to the copied nodes. lc points to the current loop, 
	loop_head is a pointer to the newly inserted head and original start was
	the old head and is now the head of the optimized variant of the loop.
*/
void patch_jumps(basicblock *original_start, basicblock *loop_head, struct LoopContainer *lc)
{
	/* step through all nodes of a loop                                         */
	struct LoopElement *le;
	basicblock *bptr;
	instruction *inst, *temp_instr;
	int i;

	le = lc->nodes;
	while (le != NULL) {

		/* do nothing for new loop head                                         */
		if (le->block == loop_head) {
			le = le->next;
			continue;
		    }

		/* for original version                                                 */
		bptr = le->block;
		inst = bptr->iinstr;
		for (i = 0; i < bptr->icount; ++i, ++inst) {
			switch (inst->opc) {

			case ICMD_IF_ICMPEQ:
			case ICMD_IF_ICMPLT:
			case ICMD_IF_ICMPLE:
			case ICMD_IF_ICMPNE:
			case ICMD_IF_ICMPGT:
			case ICMD_IF_ICMPGE:

			case ICMD_IF_LCMPEQ:
			case ICMD_IF_LCMPLT:
			case ICMD_IF_LCMPLE:
			case ICMD_IF_LCMPNE:
			case ICMD_IF_LCMPGT:
			case ICMD_IF_LCMPGE:

			case ICMD_IF_ACMPEQ:
			case ICMD_IF_ACMPNE:

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

			case ICMD_GOTO:
			case ICMD_JSR:
			case ICMD_IFNULL:
			case ICMD_IFNONNULL:

				/* jump to newly inserted loopheader has to be redirected       */
				if (((basicblock *) inst->target) == loop_head)
					inst->target = (void *) original_start;
				break;

			case ICMD_TABLESWITCH:
				{
					s4 *s4ptr, l, i;
					void **tptr;

					tptr = (void **) inst->target;

					s4ptr = inst->val.a;
					l = s4ptr[1];                          /* low     */
					i = s4ptr[2];                          /* high    */

					i = i - l + 1;

					/* jump to newly inserted loopheader has to be redirected   */
					for (tptr = inst->target; i >= 0; --i, ++tptr) {
						if (((basicblock *) *tptr) == loop_head)
							tptr[0] = (void *) original_start;
						}
				}
				break;

			case ICMD_LOOKUPSWITCH:
				{
					s4 i, l, *s4ptr;
					void **tptr;

					tptr = (void **) inst->target;

					s4ptr = inst->val.a;
					l = s4ptr[0];                          /* default  */
					i = s4ptr[1];                          /* count    */

					/* jump to newly inserted loopheader has to be redirected   */
					for (tptr = inst->target; i >= 0; --i, ++tptr) {
						if (((basicblock *) *tptr) == loop_head)
							tptr[0] = (void *) original_start;
						}
				}
				break;
			}
		}

		/* if node is part of loop and has fall through to original start, that */
		/* must be redirected. Unfortunately the instructions have to be copied */

		if (bptr->next == loop_head) {
			temp_instr = DMNEW(instruction, bptr->icount + 1);
			memcpy(temp_instr, bptr->iinstr, sizeof(instruction)*bptr->icount);
			bptr->iinstr = temp_instr;

			bptr->iinstr[bptr->icount].opc = ICMD_GOTO;
			bptr->iinstr[bptr->icount].target = original_start;
			bptr->iinstr[bptr->icount].dst = NULL;
			++bptr->icount;
			}	
		
		/* for copied version - which gets the unoptimized variant              */
		bptr = le->block->copied_to;
		inst = bptr->iinstr;
		for (i = 0; i < bptr->icount; ++i, ++inst) {

			switch (inst->opc) {

			case ICMD_IASTORE:			/* array store							*/
			case ICMD_LASTORE:          
			case ICMD_FASTORE:          
			case ICMD_DASTORE:          
			case ICMD_AASTORE:          
			case ICMD_BASTORE:          
			case ICMD_CASTORE:          
			case ICMD_SASTORE:
			case ICMD_IALOAD:			/* array load						    */
		    case ICMD_LALOAD:       
			case ICMD_FALOAD:
			case ICMD_DALOAD:
			case ICMD_AALOAD:
			case ICMD_BALOAD:
			case ICMD_CALOAD:
			case ICMD_SALOAD:

				/* undo previous optimizations in new loop                      */
				inst->op1 = 0;
				break;

			case ICMD_IF_ICMPEQ:
			case ICMD_IF_ICMPLT:
			case ICMD_IF_ICMPLE:
			case ICMD_IF_ICMPNE:
			case ICMD_IF_ICMPGT:
			case ICMD_IF_ICMPGE:

			case ICMD_IF_LCMPEQ:
			case ICMD_IF_LCMPLT:
			case ICMD_IF_LCMPLE:
			case ICMD_IF_LCMPNE:
			case ICMD_IF_LCMPGT:
			case ICMD_IF_LCMPGE:

			case ICMD_IF_ACMPEQ:
			case ICMD_IF_ACMPNE:

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

			case ICMD_GOTO:
			case ICMD_JSR:
			case ICMD_IFNULL:
			case ICMD_IFNONNULL:

				/* jump to newly inserted loopheader has to be redirected       */
				if (((basicblock *) inst->target) == loop_head)
					inst->target = (void *) original_start->copied_to;
				/* jump to loop internal nodes has to be redirected             */
				else if (((basicblock *) inst->target)->lflags & LOOP_PART)
					inst->target = (void *) ((basicblock *) inst->target)->copied_to;
				break;
				
			case ICMD_TABLESWITCH:
				{
					s4 *s4ptr, l, i;
					
					void **copy_ptr, *base_ptr;
					void **tptr;

					tptr = (void **) inst->target;

					s4ptr = inst->val.a;
					l = s4ptr[1];                          /* low     */
					i = s4ptr[2];                          /* high    */

					i = i - l + 1;
					
					copy_ptr = (void**) DMNEW(void*, i+1);
					base_ptr = (void*) copy_ptr;

					/* Targets for switch instructions are stored in an extra   */
					/* that must be copied for new inserted loop.               */

					for (tptr = inst->target; i >= 0; --i, ++tptr, ++copy_ptr) {
						/* jump to newly inserted loopheader must be redirected */
 						if (((basicblock *) *tptr) == loop_head)
							copy_ptr[0] = (void *) original_start->copied_to;
						/* jump to loop internal nodes has to be redirected     */
						else if (((basicblock *) *tptr)->lflags & LOOP_PART)
							copy_ptr[0] = (void *) ((basicblock *) tptr[0])->copied_to;
						else
							copy_ptr[0] = tptr[0];
						}

					inst->target = base_ptr;
				}
				break;

			case ICMD_LOOKUPSWITCH:
				{
					s4 i, l, *s4ptr;

					void **copy_ptr, **base_ptr;
					void **tptr;

					tptr = (void **) inst->target;

					s4ptr = inst->val.a;
					l = s4ptr[0];                          /* default  */
					i = s4ptr[1];                          /* count    */

					copy_ptr = (void**) DMNEW(void*, i+1);
					base_ptr = (void*) copy_ptr;

					/* Targets for switch instructions are stored in an extra   */
					/* that must be copied for new inserted loop.               */

					for (tptr = inst->target; i >= 0; --i, ++tptr, ++copy_ptr) {
						/* jump to newly inserted loopheader must be redirected */
						if (((basicblock *) *tptr) == loop_head)
							copy_ptr[0] = (void *) original_start->copied_to;
						/* jump to loop internal nodes has to be redirected     */
						else if (((basicblock *) *tptr)->lflags & LOOP_PART)
							copy_ptr[0] = (void *) ((basicblock *) tptr[0])->copied_to;
						else 
							copy_ptr[0] = tptr[0];
						}

					inst->target = base_ptr;
				}
				break;
				
				}
			}

		/* if fall through exits loop, goto is needed                           */
		if (!(le->block->next->lflags & LOOP_PART)) {
			bptr->iinstr[bptr->icount].opc = ICMD_GOTO;
			bptr->iinstr[bptr->icount].dst = NULL;
			bptr->iinstr[bptr->icount].target = le->block->next;
			bptr->icount++;
			}
		
		le = le->next;
		}
}


/*	Add the new header node of a loop that has been duplicated to all parent 
    loops in nesting hierarchie.
*/

void header_into_parent_loops(loopdata *ld, struct LoopContainer *lc, basicblock *to_insert, basicblock *replace, basicblock *after)
{
	/* we have to insert the node to_insert before the node after and replace   */
	/* the pointer of to_insert by the node replace                             */

	struct LoopElement *le, *t;

	/* if the top of the tree is reached, then return                           */
	if ((lc == NULL) || (lc == ld->root))
		return;

	/* create new node, that should be inserted                                 */
	t = DMNEW(struct LoopElement, 1);
	t->block = to_insert;
	t->node = -1;

	/* first, find the node, that has to be replaced (= "to_insert") and        */
	/* replace it by the node "replace"                                         */
	le = lc->nodes;
	while (le->block != to_insert)
		le = le->next;
	le->block = replace;

	/* BUGFIX                                                                   */
	if (after == to_insert)
		after = replace;

	/* now find the node after and insert the newly create node before "after"  */
	le = lc->nodes;
	if (le->block == after) {
		t->next = lc->nodes;
		lc->nodes = t;
	    }
	else {
		while (le->next->block != after)
			le = le->next;

		t->next = le->next;
		le->next = t;
	    }

	/* go up one hierarchie level                                               */
	header_into_parent_loops(ld, lc->parent, to_insert, replace, after);
}


/*	Add a new node (not header) of a duplicated loop to all parent loops in 
    nesting hierarchie
*/

void node_into_parent_loops(loopdata *ld, struct LoopContainer *lc, basicblock *to_insert)
{
	struct LoopElement *le, *t;

	/* if the top of the tree is reached, then return                           */
	if ((lc == NULL) || (lc == ld->root))
		return;

	/* create new node, that should be inserted                                 */
	t = DNEW(LoopElement);
	t->block = to_insert;
	t->node = -1;

	le = lc->nodes;

	/* append new node to node list of loop                                     */
	while (le->next != NULL)
		le = le->next;

	t->next = le->next;
	le->next = t;

	/* go up one hierarchie level                                               */
	node_into_parent_loops(ld, NULL, to_insert);
}


/* void patch_handler(...) is very similar to parts of the function patch_jumps. 
   Its task is to redirect all jumps from the original head to the new head and
   to redirect internal jumps inside the exception handler to the newly
   created (copied) nodes.
*/

void patch_handler(struct LoopContainer *lc, basicblock *bptr, basicblock *original_head, basicblock *new_head)
{
	instruction *ip;
	int i;

	/* If node is not part of exception handler or has been visited, exit	    */
	if (!(bptr->lflags & HANDLER_PART) || (bptr->lflags & HANDLER_VISITED))
		return;

	/* mark block as visited                                                    */
	bptr->lflags |= HANDLER_VISITED;

	/* for all instructions in the copied block, do                             */
	for (i = 0, ip = bptr->copied_to->iinstr; i < bptr->copied_to->icount; ++i, ++ip) {
		switch (ip->opc) {
		case ICMD_RETURN:
		case ICMD_IRETURN:
		case ICMD_LRETURN:
		case ICMD_FRETURN:
		case ICMD_DRETURN:
		case ICMD_ARETURN:
		case ICMD_ATHROW:
			break;                                 

		case ICMD_IF_ICMPEQ:
		case ICMD_IF_ICMPLT:
		case ICMD_IF_ICMPLE:
		case ICMD_IF_ICMPNE:
		case ICMD_IF_ICMPGT:
		case ICMD_IF_ICMPGE:
			
		case ICMD_IF_LCMPEQ:
		case ICMD_IF_LCMPLT:
		case ICMD_IF_LCMPLE:
		case ICMD_IF_LCMPNE:
		case ICMD_IF_LCMPGT:
		case ICMD_IF_LCMPGE:

		case ICMD_IF_ACMPEQ:
		case ICMD_IF_ACMPNE:

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

		case ICMD_JSR:
		case ICMD_IFNULL:
		case ICMD_IFNONNULL:

			patch_handler(lc, bptr->next, original_head, new_head); 

			/* fall through */

		case ICMD_GOTO:

			patch_handler(lc, ip->target, original_head, new_head);

			/* jumps to old header have to be redirected                        */
			if (((basicblock *) ip->target) == original_head)
				ip->target = (void *) new_head->copied_to;
			/* jumps to handler internal nodes have to be redirected            */
			else if (((basicblock *) ip->target)->lflags & HANDLER_PART)
				ip->target = (void *) ((basicblock *) ip->target)->copied_to;
			/* jumps to loop internal nodes have to be redirected               */
			else if (((basicblock *) ip->target)->lflags & LOOP_PART)
				ip->target = (void *) ((basicblock *) ip->target)->copied_to;
		   
		   
			break;
				
		case ICMD_TABLESWITCH:
			{
				s4 *s4ptr, l, i;
				void **tptr;
				void **copy_ptr, **base_ptr;
 
				tptr = (void **) ip->target;
				s4ptr = ip->val.a;
				l = s4ptr[1];                          /* low                   */
				i = s4ptr[2];                          /* high                  */
				i = i - l + 1;
				
				copy_ptr = (void**) DMNEW(void*, i+1);
				base_ptr = (void*) copy_ptr;

				for (tptr = ip->target; i >= 0; --i, ++tptr, ++copy_ptr) {
					patch_handler(lc, ((basicblock *) *tptr), original_head, new_head);
					/* jumps to old header have to be redirected                */
					if (((basicblock *) *tptr) == original_head)
						copy_ptr[0] = (void *) new_head->copied_to;
					/* jumps to handler internal nodes have to be redirected    */
					else if (((basicblock *) *tptr)->lflags & HANDLER_PART)
						copy_ptr[0] = (void *) ((basicblock *) tptr[0])->copied_to;
					/* jumps to loop internal nodes have to be redirected       */
					else if (((basicblock *) ip->target)->lflags & LOOP_PART)
						copy_ptr[0] = (void *) ((basicblock *) tptr[0])->copied_to;
					else
						copy_ptr[0] = tptr[0];
				    }

				ip->target = base_ptr;
			}
			break;

		case ICMD_LOOKUPSWITCH:
			{
				s4 i, l, *s4ptr;

				void **tptr;
				void **copy_ptr, **base_ptr;

				tptr = (void **) ip->target;
				s4ptr = ip->val.a;
				l = s4ptr[0];                          /* default               */
				i = s4ptr[1];                          /* count                 */

				copy_ptr = (void**) DMNEW(void*, i+1);
				base_ptr = (void*) copy_ptr;

				for (tptr = ip->target; i >= 0; --i, ++tptr, ++copy_ptr) {

					patch_handler(lc, ((basicblock *) *tptr), original_head, new_head);
					/* jumps to old header have to be redirected                */
					if (((basicblock *) *tptr) == original_head)
						copy_ptr[0] = (void *) new_head->copied_to;
					/* jumps to handler internal nodes have to be redirected    */
					else if (((basicblock *) *tptr)->lflags & HANDLER_PART)
						copy_ptr[0] = (void *) ((basicblock *) tptr[0])->copied_to;
					/* jumps to loop internal nodes have to be redirected       */
					else if (((basicblock *) ip->target)->lflags & LOOP_PART)
						copy_ptr[0] = (void *) ((basicblock *) tptr[0])->copied_to;
					else
						copy_ptr[0] = tptr[0];
				    }

				ip->target = base_ptr;
			}
			break;
				
		    }   /* switch */
		   
	    }       /* for    */

		/* if fall through exits loop, goto is needed                           */
		if (!(bptr->next->lflags & HANDLER_PART)) {
			bptr->copied_to->iinstr[bptr->copied_to->icount].opc = ICMD_GOTO;
			bptr->copied_to->iinstr[bptr->copied_to->icount].dst = NULL;
			bptr->copied_to->iinstr[bptr->copied_to->icount].target = bptr->next;
			bptr->copied_to->icount++;
			}		
}


/* copy_handler ****************************************************************

   This function copys the exception handler and redirects all jumps from the
   original head to the new head in the original exception handler. All
   redirection in the copied exception handler is done in patch_handler(...).

*******************************************************************************/

void copy_handler(methodinfo *m, loopdata *ld, struct LoopContainer *lc, basicblock *bptr, basicblock *original_head, basicblock *new_head)
{
	instruction *ip;
	s4 *s4ptr;
	void **tptr;
	int high, low, count;
	struct LoopElement *le;
	basicblock *new, *temp;

	/* If this node has already been copied, return                           */
	if (bptr->lflags & HANDLER_PART)
		return;

	/* The exception handler exists, when control flow enters loop again      */

	if (bptr->lflags & LOOP_PART)
		return;
	for (le = lc->nodes; le != NULL; le = le->next) {
		if (le->block == bptr) {
			printf("C_PANIC: should not happen\n");
			exit(-1);
            }
	    }

	/* mark block as part of handler                                          */
	bptr->lflags |= HANDLER_PART;

	/* copy node                                                              */
	new = DMNEW(basicblock, 1);    
	memcpy(new, bptr, sizeof(basicblock));
	new->nr = -1;

	ld->c_last_block_copied = new;

	/* copy instructions and allow one more slot for possible GOTO            */
	new->iinstr = DMNEW(instruction, bptr->icount + 1);
	memcpy(new->iinstr, bptr->iinstr, bptr->icount * sizeof(instruction));

	/* update original block                                                  */
	bptr->copied_to = new;

	/* append block to global list of basic blocks                            */
	temp = m->basicblocks;

	while (temp->next)
		temp = temp->next;

	temp->next = new;
	new->next = NULL;


	/* find next block to copy, depending on last instruction of BB             */
	if (bptr->icount == 0) {
		copy_handler(m, ld, lc, bptr->next, original_head, new_head);
		return;
	}

	ip = bptr->iinstr + (bptr->icount - 1);
	
	switch (ip->opc) {
	case ICMD_RETURN:
	case ICMD_IRETURN:
	case ICMD_LRETURN:
	case ICMD_FRETURN:
	case ICMD_DRETURN:
	case ICMD_ARETURN:
	case ICMD_ATHROW:
		break;                                 
		
	case ICMD_IFEQ:
	case ICMD_IFNE:
	case ICMD_IFLT:
	case ICMD_IFGE:
	case ICMD_IFGT:
	case ICMD_IFLE:
			
	case ICMD_IF_LCMPEQ:
	case ICMD_IF_LCMPLT:
	case ICMD_IF_LCMPLE:
	case ICMD_IF_LCMPNE:
	case ICMD_IF_LCMPGT:
	case ICMD_IF_LCMPGE:
			
	case ICMD_IF_LEQ:
	case ICMD_IF_LNE:
	case ICMD_IF_LLT:
	case ICMD_IF_LGE:
	case ICMD_IF_LGT:
	case ICMD_IF_LLE:
			
	case ICMD_IFNULL:
	case ICMD_IFNONNULL:
			
	case ICMD_IF_ICMPEQ:
	case ICMD_IF_ICMPNE:
	case ICMD_IF_ICMPLT:
	case ICMD_IF_ICMPGE:
	case ICMD_IF_ICMPGT:
	case ICMD_IF_ICMPLE:
	case ICMD_IF_ACMPEQ:
	case ICMD_IF_ACMPNE:
		copy_handler(m, ld, lc, bptr->next, original_head, new_head);
		/* fall through */
	  
	case ICMD_GOTO:

		/* redirect jump from original_head to new_head                    */
		if ((basicblock *) ip->target == original_head)
			ip->target = (void *) new_head;
				
		copy_handler(m, ld, lc, (basicblock *) (ip->target), original_head, new_head);
			
		break;
	  
	case ICMD_TABLESWITCH:
		s4ptr = ip->val.a;
		tptr = (void **) ip->target;
			
		/* default branch */
		if (((basicblock *) *tptr) == original_head)
			tptr[0] = (void *) new_head;
			
		copy_handler(m, ld, lc, (basicblock *) *tptr, original_head, new_head);
			
		s4ptr++;
		low = *s4ptr;
		s4ptr++;
		high = *s4ptr;
			
		count = (high-low+1);
			
		while (--count >= 0) {
			tptr++;
			/* redirect jump from original_head to new_head                 */
			if (((basicblock *) *tptr) == original_head)
				tptr[0] = (void *) new_head;
			copy_handler(m, ld, lc, (basicblock *) *tptr, original_head, new_head);
		}
		break;

	case ICMD_LOOKUPSWITCH:
		s4ptr = ip->val.a;
		tptr = (void **) ip->target;
			
		/* default branch */
		if (((basicblock *) *tptr) == original_head)
			tptr[0] = (void *) new_head;
			
		copy_handler(m, ld, lc, (basicblock *) *tptr, original_head, new_head);
			
		++s4ptr;
		count = *s4ptr;
			
		while (--count >= 0) {
			++tptr;
			/* redirect jump from original_head to new_head                 */
			if (((basicblock *) *tptr) == original_head)
				tptr[0] = (void *) new_head;
			copy_handler(m, ld, lc, (basicblock *) *tptr, original_head, new_head);
		}  
		break;

	case ICMD_JSR:
		ld->c_last_target = bptr;
		copy_handler(m, ld, lc, (basicblock *) (ip->target), original_head, new_head);         
		break;
			
	case ICMD_RET:
		copy_handler(m, ld, lc, ld->c_last_target->next, original_head, new_head);
		break;
			
	default:
		copy_handler(m, ld, lc, bptr->next, original_head, new_head);
		break;	
	} 
}           


/* If a loop is duplicated, all exceptions, that are contained in this loops' body
   have to be duplicated as well. The following function together with the
   two helper functions copy_handler and patch_handler perform this task.
*/

void update_internal_exceptions(methodinfo *m, codegendata *cd, loopdata *ld, struct LoopContainer *lc, basicblock *original_head, basicblock *new_head)
{
	exceptiontable *ex, *new;
	struct LoopContainer *l;

	/* Bottom of tree reached -> return                                         */
	if (lc == NULL)
		return;

	/* Call update_internal for all nested (=child) loops                       */
	l = lc->tree_down;
	while (l != NULL) {
		update_internal_exceptions(m, cd, ld, l, original_head, new_head);
		l = l->tree_right;
	    }

	/* For all exceptions of this loop, do                                      */
	ex = lc->exceptions;
	while (ex != NULL) {
		
		/* Copy the exception and patch the jumps                               */
		copy_handler(m, ld, lc, ex->handler, original_head, new_head);
		patch_handler(lc, ex->handler, original_head, new_head);		

		/* Insert a new exception into the global exception table               */
		new = DNEW(exceptiontable);
		memcpy(new, ex, sizeof(exceptiontable));

		/* Increase number of exceptions                                        */
		++jd->exceptiontablelength;

		ex->next = new;
		ex->down = new;

		/* Set new start and end point of this exception                        */
		new->start = ex->start->copied_to;
		new->end = ex->end->copied_to;

		/* Set handler pointer to copied exception handler                      */
		new->handler = ex->handler->copied_to;

		ex = new->next;
	    }

}


/* If a loop is duplicated, all exceptions that contain this loop have to be
   extended to the copied nodes as well. The following function checks for
   all exceptions of all parent loops, whether they contain the loop pointed to
   by lc. If so, the exceptions are extended to contain all newly created nodes.
*/

void update_external_exceptions(methodinfo *m, codegendata *cd, loopdata *ld, struct LoopContainer *lc, int loop_head)
{
	exceptiontable *ex, *new;

	/* Top of tree reached -> return                                            */
	if (lc == NULL)
		return;
	
	ex = lc->exceptions;

	/* For all exceptions of this loop do                                       */
	while (ex != NULL) {
		   
		/* It is possible that the loop contains exceptions that do not protect */
		/* the loop just duplicated. It must be checked, if this is the case    */
		if ((loop_head >= m->basicblockindex[ex->startpc]) && (loop_head < m->basicblockindex[ex->endpc])) {

			/* loop is really inside exception, so create new exception entry   */
			/* in global exception list                                         */
			new = DNEW(exceptiontable);
			memcpy(new, ex, sizeof(exceptiontable));


			/* Increase number of exceptions                                    */
			++jd->exceptiontablelength;

			ex->next = new;
			ex->down = new;

			/* Set new start and end point of this exception                    */
			new->start = ld->c_first_block_copied;
			new->end = ld->c_last_block_copied;

			ex = new->next;
	        }
		/* exception does not contain the duplicated loop -> do nothing         */
		else
			ex = ex->next;
	    }

	/* Call update_external for parent node                                     */
	update_external_exceptions(m, cd, ld, lc->parent, loop_head);
}
	

/*	This function is needed to insert the static checks, stored in c_constraints
	into the intermediate code.
*/

void create_static_checks(methodinfo *m, codegendata *cd, loopdata *ld, struct LoopContainer *lc)
{
	int i, stackdepth, cnt;
	struct Constraint *tc1;
	struct LoopElement *le;	

	/* loop_head points to the newly inserted loop_head, original_start to      */
	/* the old loop header                                                      */
	basicblock *bptr, *loop_head, *original_start, *temp;
	instruction *inst, *tiptr;

	/* tos and newstack are needed by the macros, that insert instructions into */
	/* the new loop head                                                        */
	stackptr newstack, tos;
	exceptiontable *ex;

	/* prevent some compiler warnings */

	bptr = NULL;

#ifdef ENABLE_STATISTICS
	/* show_loop_statistics(l); */ 
#endif

	loop_head = &m->basicblocks[ld->c_current_head];
	ld->c_first_block_copied = ld->c_last_block_copied = NULL;

	/* the loop nodes are copied                                                */
	le = lc->nodes;
	while (le != NULL)
	{
		bptr = DMNEW(basicblock, 1);    
		memcpy(bptr, le->block, sizeof(basicblock));
		bptr->nr = -1;

		/* determine beginning of copied loop to extend exception handler, that */
		/* protect this loop                                                    */
		if (ld->c_first_block_copied == NULL)
			ld->c_first_block_copied = bptr;

		/* copy instructions and add one more slot for possible GOTO            */
		bptr->iinstr = DMNEW(instruction, bptr->icount + 1);

		memcpy(bptr->iinstr, le->block->iinstr, (bptr->icount+1)*sizeof(instruction));

		le->block->copied_to = bptr;

		/* add block to global list of BBs                                      */
		temp = m->basicblocks;

		while (temp->next)
			temp = temp->next;

		temp->next = bptr;
		bptr->next = NULL;

		node_into_parent_loops(ld, lc->parent, bptr);
		le = le->next;
	}

	ld->c_last_block_copied = bptr;

	/* create an additional basicblock for dynamic checks                       */
	original_start = bptr = DMNEW(basicblock, 1);    

	/* copy current loop header to new basic block                              */
	memcpy(bptr, loop_head, sizeof(basicblock));
    bptr->nr = -1;

	/* insert the new basic block and move header before first loop node        */
	le = lc->nodes;
	temp = le->block;

	/* if header is first node of loop, insert original header after it         */
	if (temp == loop_head)
		loop_head->next = bptr;
	else {
	/* else, we have to find the predecessor of loop header                     */
		while (temp->next != loop_head)
			temp = temp->next;

		/* insert original header after newly created block                     */
		temp->next = bptr;

		/* if predecessor is not loop part, insert a goto                       */
		if (!(temp->lflags & LOOP_PART)) {

			/* copy instructions and add an additional slot                     */
			tiptr = DMNEW(instruction, temp->icount + 1);
			memcpy(tiptr, temp->iinstr, sizeof(instruction)*temp->icount);
			
			temp->iinstr = tiptr;
			tiptr = temp->iinstr + temp->icount;
			
			/* add goto to loop header. If node is part of exception handler    */
			/* jmp is automagically redirected during patch_handler and works   */
			/* correct                                                          */
			tiptr->opc = ICMD_GOTO;
			tiptr->dst = NULL;
			tiptr->target = (void*) loop_head;
			
			++temp->icount;
		    }
		
		
		temp = m->basicblocks;
		/* if first loop block is first BB of global list, insert loop_head at  */
		/* beginning of global BB list                                          */
		if (temp == le->block) {
			if (ld->c_newstart == NULL) {
				ld->c_needs_redirection = true;
				ld->c_newstart = loop_head;
				loop_head->next = m->basicblocks;
		 	    }
			else {
				loop_head->next = ld->c_newstart;
				ld->c_newstart = loop_head;
			    }
		    }
		else {
	   
			while (temp->next != le->block)
				temp = temp->next;

			loop_head->next = temp->next;
			temp->next = loop_head;
		
			/* to be on the safe side insert a jump from the previous instr     */
			/* over thr new inserted node                                       */
	
			/* special case - jump from node to loop_head: then remove          */
			/* goto / happens rather often due to loop layout                   */
			tiptr = temp->iinstr + (temp->icount-1);
		
			if ((tiptr->opc == ICMD_GOTO) && (tiptr->target == loop_head)) {
				tiptr->opc = ICMD_NOP;
				tiptr->dst = NULL;
		        }
			else {

				tiptr = DMNEW(instruction, temp->icount + 1);
				memcpy(tiptr, temp->iinstr, sizeof(instruction)*temp->icount);

				temp->iinstr = tiptr;
				tiptr = temp->iinstr + temp->icount;

				tiptr->opc = ICMD_GOTO;
				tiptr->dst = NULL;
				tiptr->target = (void*) loop_head->next;

				++temp->icount;
	            }
		    }
	    }

	/* adjust exceptions                                                        */
	ex = jd->exceptiontable;
	while (ex != NULL) {

		/* if an exception covers whole loop and starts at first loop node, it  */
		/* has to be extended to cover the new first node as well               */
		if (ex->start == le->block) {
			
			if ((lc->loop_head >= m->basicblockindex[ex->startpc]) && (lc->loop_head < m->basicblockindex[ex->endpc])) 
				ex->start = loop_head;
		    }

		/* an exception that ended at the old loop header now must contains the */
		/* new loop header as well                                              */
		if (ex->end == loop_head)
			ex->end = original_start;

		ex = ex->down;
	    }
	

	/* insert new header node into nodelists of all enclosing loops             */
	header_into_parent_loops(ld, lc, loop_head, original_start, le->block);

	/* prepare instruction array to insert checks                               */
	inst = loop_head->iinstr = DMNEW(instruction, ld->c_needed_instr + 2); 
	loop_head->icount = ld->c_needed_instr + 1;

	/* init instruction array                                                   */
	for (cnt=0; cnt<ld->c_needed_instr + 1; ++cnt) {
		inst[0].opc = ICMD_NOP;
		inst[0].dst = NULL;
	    }

	loop_head->copied_to = NULL; 

	/* prepare stack                                                            */
	loop_head->instack = copy_stack_from(bptr->instack);
	loop_head->outstack = copy_stack_from(bptr->instack);
	
	tos = loop_head->instack;
	stackdepth = loop_head->indepth;
	
	/* step through all inserted checks and create instructions for them        */
	for (i=0; i<jd->maxlocals+1; ++i)
	{
		tc1 = ld->c_constraints[i];
		while (tc1 != NULL)
		{
			switch (tc1->type)
			{
			
				/* check a variable against a constant                          */
			case TEST_ZERO:
			case TEST_UNMOD_ZERO: 

#ifdef LOOP_DEBUG
				printf("insert ZERO-test\n");
				fflush(stdout);
#endif

				/* optimize if tc1->varRef >= tc1->constant                     */
				LOAD_VAR(tc1->varRef);
				LOAD_CONST(tc1->constant);
				GOTO_NOOPT_IF_LT;
				break;

				/* check a variable against an array length                     */
			case TEST_ALENGTH:       
			case TEST_UNMOD_ALENGTH:
				
				/* optimize if                                                  */
 				/* tc1->varRef + tc1->constant < lengthOf(tc1->arrayRef)        */
#ifdef LOOP_DEBUG
				printf("insert ALENGTH-test\n");
				fflush(stdout);
#endif

				LOAD_VAR(tc1->varRef);
				LOAD_CONST(tc1->constant);
				ADD;
				LOAD_ARRAYLENGTH(tc1->arrayRef);
				GOTO_NOOPT_IF_GE;
				break;
				
				/* test right side of comparison against constant               */
			case TEST_RS_ZERO:      

#ifdef LOOP_DEBUG
				printf("insert RS-ZERO-test\n");
				fflush(stdout);
#endif

				/* optimize if right-side >= tc1->constant                      */
				LOAD_RIGHT_SIDE;
				LOAD_CONST(tc1->constant);
				GOTO_NOOPT_IF_LT;
				break;
				
				/* test right side of comparison against array length           */
			case TEST_RS_ALENGTH: 

#ifdef LOOP_DEBUG
				printf("insert RS-ALENGTH-test\n");
				fflush(stdout);
#endif
				/* optimize if right-side < lengthOf(arrayRef)                  */
				LOAD_RIGHT_SIDE;
				LOAD_ARRAYLENGTH(tc1->arrayRef);
				GOTO_NOOPT_IF_GT;
				break;
				
				/* test unmodified variable against arraylength                 */
			case TEST_CONST_ALENGTH:

#ifdef LOOP_DEBUG
				printf("insert CONST ALENGTH-test\n");
				fflush(stdout);
#endif

				/* optimize if tc1->constant < lengthOf(tc1->arrayRef)          */
				LOAD_CONST(tc1->constant);
				LOAD_ARRAYLENGTH(tc1->arrayRef);
				GOTO_NOOPT_IF_GE;
				break;		    
			}
			
			tc1 = tc1->next;
		}
		ld->c_constraints[i] = NULL;
	}
   
	/* if all tests succeed, jump to optimized loop header                      */
	if (loop_head->next != original_start) {
		inst->opc = ICMD_GOTO;
		inst->dst = NULL;
		inst->target = original_start;
	    }

	/* redirect jumps from original loop head to newly inserted one             */
	patch_jumps(original_start, loop_head, lc); 

	/* if exceptions have to be correct due to loop duplication these two       */
	/* functions perform this task.                                             */
	update_internal_exceptions(m, cd, ld, lc, loop_head, original_start);
	update_external_exceptions(m, cd, ld, lc->parent, lc->loop_head);
}


/*	This function performs an update between two arrays of struct Changes (that
	reflect variable changes). The merge is performed unrstricted in the way, that
	all variable changes in c1 took place in a nested loop and therefore are
	considered to be without limit. Beside that, the merge is a simple union of the
	changes recorded in both arrays. A variable, which limits are undefinied, is
	represented by its lower bound being higher than the upper bound. The result 
	of the union is stored in c1.
*/
struct Changes ** constraints_unrestricted_merge(codegendata *cd, struct Changes **c1, struct Changes **c2)
{
	int i, changed;

	if ((c1 == NULL) || (c2 == NULL))
		printf("C_ERROR: debugging error 0x03\n");

	changed = 0;
	for (i=0; i<jd->maxlocals; ++i) {
		if (c1[i] == NULL) {
			if (c2[i] != NULL) {		/* a change in c2 is updated in c1		*/
				changed = 1;
				c1[i] = c2[i];
				c1[i]->lower_bound = c1[i]->upper_bound+1;
				}
			}
		else {
			if (c1[i]->lower_bound > c1[i]->upper_bound)
				continue;				/* variable's bounds already undefined	*/

			if (c2[i] == NULL) {		/* variable changed in c1 -> now undef.	*/
				changed = 1;
				c1[i]->lower_bound = c1[i]->upper_bound+1;
				}
			else {
				if ((c1[i]->lower_bound == c2[i]->lower_bound) &&
					(c1[i]->upper_bound == c2[i]->upper_bound))
					continue;			/* variable's bounds remain the same	*/
				else {
					changed = 1;
					c1[i]->lower_bound = c1[i]->upper_bound+1;
					}					/* variable changed in c1 -> now undef.	*/
				}
			}
		}
	
	if (changed)
		return c1;
	else
		return NULL;
}

/*	This function performs an update between two arrays of struct Changes (that
	reflect variable changes). The merge is a simple union of the bounds
	changes recorded in both arrays. A variable, which limits are undefinied, is
	represented by its lower bound being higher than the upper bound. The result 
	of the union is stored in c1.
*/
struct Changes ** constraints_merge(codegendata *cd, struct Changes **c1, struct Changes **c2)
{
	int i, changed;

	if ((c1 == NULL) || (c2 == NULL))
		printf("C_ERROR: debugging error 0x04\n");

	changed = 0;

	for (i=0; i<jd->maxlocals; ++i) {
		if (c1[i] == NULL) {
			if (c2[i] != NULL) {		/* update changes in c2 in c1			*/
				if ((c1[i] = (struct Changes *) malloc (sizeof(struct Changes))) == NULL)
					c_mem_error();

					c1[i]->lower_bound = c2[i]->lower_bound; 
					c1[i]->upper_bound = c2[i]->upper_bound;
					changed = 1;
				}	
      		}
		else {
			if (c2[i] != NULL) {

				if (c1[i]->lower_bound > c1[i]->upper_bound)
					continue;			/* var in c1 is unrestricted			*/

				if (c1[i]->lower_bound > c2[i]->lower_bound) {
					c1[i]->lower_bound = c2[i]->lower_bound;
					changed = 1;		/* write new lower bound				*/
					}
				if (c1[i]->upper_bound < c2[i]->upper_bound) {
					c1[i]->upper_bound = c2[i]->upper_bound;
					changed = 1;		/* write new higher bound				*/
					}
				}
			}
		}

	if (changed)
		return c1;
	else
		return NULL;
}


/*	This function simply copies an array of changes 
*/
struct Changes** constraints_clone(codegendata *cd, struct Changes **c)
{
	int i;
	struct Changes **t;
       
	if ((t = (struct Changes **) malloc(jd->maxlocals * sizeof(struct Changes *))) == NULL)
		c_mem_error();

	for (i=0; i<jd->maxlocals; ++i) {		/* for all array elements (vars) do		*/
		if (c[i] == NULL)
			t[i] = NULL;
		else {
			if ((t[i] = (struct Changes *) malloc(sizeof(struct Changes))) == NULL)
				c_mem_error();
			t[i]->lower_bound = c[i]->lower_bound;
			t[i]->upper_bound = c[i]->upper_bound;
			}
		}
	
	return t;
}

/*	This function is used to reset the changes of a variable to the time, it was
	pushed onto the stack. If a variable has been modified between an instruction
	and a previous push onto the stack, its value in the changes array does not
	correctly reflect its bounds the time, it was pushed onto the stack. This 
	function corrects the situation.
	*/
struct Changes* backtrack_var(methodinfo *m, int node, int from, int to, int varRef, struct Changes *changes)
{
	struct Changes *tmp;
	basicblock bp;
	instruction *ip;
	struct Trace *t;

	if (changes == NULL)	/* if there are no changes, immediately return		*/
		return NULL;
	else {					/* init a Changes structure with current values		*/
		if ((tmp = (struct Changes *) malloc(sizeof(struct Changes))) == NULL)
			c_mem_error();
		
		tmp->upper_bound = changes->upper_bound;
		tmp->lower_bound = changes->lower_bound;
		}

	if (tmp->upper_bound < tmp->lower_bound)
		return tmp;			/* if it is unrestricted no backtracking can happen	*/

	bp = m->basicblocks[node];
	ip = bp.iinstr + to;

	for (; from < to; --to, --ip) {		/* scan instructions backwards			*/
		switch (ip->opc) {
		case ICMD_IINC:					/* a var has been modified				*/
			if (varRef != ip->op1)		/* not the one, we are interested in	*/
				break;
			tmp->upper_bound -= ip->val.i;		/* take back modifications		*/
			tmp->lower_bound -= ip->val.i;
			break;

		case ICMD_ISTORE:				/* a var has been modified				*/
			if (varRef != ip->op1)		/* not the one, we are interested in	*/
				break;

			/* it is our variable, so trace its origin							*/
			t = tracing(&m->basicblocks[node],to, 0);		
	
			switch (t->type) {
				case TRACE_IVAR:  
					if ((t->var = ip->op1) && (t->neg > 0)) {
						/* it was the same var -> take back modifications		*/
						tmp->upper_bound -= t->constant;
						tmp->lower_bound -= t->constant;
						}		
					else
						tmp->lower_bound = tmp->upper_bound+1;	/* unknown		*/
					break;

				/* cannot restore it -> invalidate t							*/
				case TRACE_ICONST:
				case TRACE_ALENGTH:   
				case TRACE_UNKNOWN: 
				case TRACE_AVAR: 
					tmp->lower_bound = tmp->upper_bound+1;   
					break;
				}

			break;
			}
		}
	return tmp;
}


/*	This function performs the main task of bound check removal. It removes
	all bound-checks in node. change is a pointer to an array of struct Changes
	that reflect for all local variables, how their values have changed from
	the start of the loop. The special flag is needed to deal with the header
	node.
*/

void remove_boundchecks(methodinfo *m, codegendata *cd, loopdata *ld, int node, int from, struct Changes **change, int special)
{
	basicblock bp;
	instruction *ip;
	int i, count, ignore, degrade_checks, opt_level;
	struct depthElement *d;
	struct Changes **t1, **tmp, *t;
	struct Trace *t_array, *t_index;

	/* prevent some compiler warnings */

	t_array = NULL;
	t_index = NULL;

	/* printf("remove_bc called: %d - %d - %d\n", node, from, special);			*/
	   
	/* a flag, that is set, when previous optimzations have to be taken back	*/
	degrade_checks = 0;			

	if (ld->c_current_loop[node] > 0) {		/* this node is part of the loop		*/
		if (ld->c_current_loop[node] > 1) {	/* it is not the header node			*/

			/* get variable changes, already recorded for this node				*/
			t1 = ld->c_dTable[node]->changes;
			
			if (t1 != NULL)	{			/* it is not the first visit			*/
				if ((ld->c_nestedLoops[node] != ld->c_current_head) && (ld->c_nestedLoops[node] == ld->c_nestedLoops[from])) {
				/* we are looping in a nested loop, so made optimizations		*/
				/* need to be reconsidered										*/
					degrade_checks = 1;
					if (constraints_unrestricted_merge(cd, t1, change) == NULL)	
						return;			/* no changes since previous visit		*/
						/* if there have been changes, they are updated by		*/
						/* constraints_unrestricted_merge in t1					*/
					}
				else {
					if (constraints_merge(cd, t1, change) == NULL)
						return;			/* no changes since previous visit		*/
						/* if there have been changes, they are updated by		*/
						/* constraints_merge in t1								*/
					}
				}
			else {						/* first visit							*/
				/* printf("first visit - constraints cloned\n");				*/
				ld->c_dTable[node]->changes = constraints_clone(cd, change);
				}

			/* tmp now holds a copy of the updated variable changes				*/
			tmp = constraints_clone(cd, ld->c_dTable[node]->changes);	
			}
		else if (special) {				/* header and need special traetment	*/
			/* printf("special treatment called\n");							*/
			/* tmp now holds a copy of the current new variable changes			*/
			tmp = constraints_clone(cd, change);
			}
		else
			return;

		bp = m->basicblocks[node];				/* scan all instructions				*/
		count = bp.icount;
		ip = bp.iinstr;
		ignore = 0;

		for (i=0; i<count; ++i, ++ip) {
			switch (ip->opc) {
			case ICMD_IASTORE:			/* found an array store					*/
			case ICMD_LASTORE:          
			case ICMD_FASTORE:          
			case ICMD_DASTORE:          
			case ICMD_AASTORE:          
			case ICMD_BASTORE:          
			case ICMD_CASTORE:          
			case ICMD_SASTORE:

				t_index = tracing(&bp, i-1, 1);	/* get index					*/
				t_array = tracing(&bp, i-1, 2);	/* get array refernce			*/
				ignore = 1;
				/* fall through */

			case ICMD_IALOAD:			/* found an array load					*/
			case ICMD_LALOAD:       
			case ICMD_FALOAD:
			case ICMD_DALOAD:
			case ICMD_AALOAD:
			case ICMD_BALOAD:
			case ICMD_CALOAD:
			case ICMD_SALOAD:
				if (!ignore) {
					t_index = tracing(&bp, i-1, 0);	/* get index				*/
					t_array = tracing(&bp, i-1, 1);	/* get array refernce		*/
					ignore = 0;
					}

				/* printf("Array access with params:\n");
				printf("Array:\n");
				show_trace(t_array);
				printf("Index:\n");
				show_trace(t_index);
				*/

#ifdef ENABLE_STATISTICS
				if (ip->op1 == OPT_UNCHECKED) {		/* found new access			*/
				   ld->c_stat_array_accesses++;
				   ip->op1 = OPT_NONE;
				   ld->c_stat_no_opt++;
				   }
#endif

				/* can only optimize known arrays that do not change			*/
				if ((t_array->type != TRACE_AVAR) || (ld->c_var_modified[t_array->var])) 
					break;
				
				switch (t_index->type) {	/* now we look at the index			*/
				case TRACE_ICONST:			/* it is a constant value or an		*/
				case TRACE_ALENGTH:			/* array length						*/
#ifdef ENABLE_STATISTICS
					switch (ip->op1) {		/* take back old optimzation		*/
					case OPT_UNCHECKED:
						break;
					case OPT_NONE:
						ld->c_stat_no_opt--;
						break;
					case OPT_FULL:
						ld->c_stat_full_opt--;
						break;
					case OPT_UPPER:
						ld->c_stat_upper_opt--;
						break;
					case OPT_LOWER:
						ld->c_stat_lower_opt--;
						break;
						}
#endif
					if (degrade_checks)		/* replace existing optimization	*/
						ip->op1 = insert_static(m, cd, ld, t_array->var, t_index, NULL, special);
					else {
						/* Check current optimization and try to improve it	by	*/
						/* inserting new checks									*/
						switch (ip->op1) {	
						case OPT_UNCHECKED:
							ip->op1 = insert_static(m, cd, ld, t_array->var, t_index, NULL, special);
							break;
						case OPT_NONE:		
							ip->op1 = insert_static(m, cd, ld, t_array->var, t_index, NULL, special);
							break;
						case OPT_UPPER:		
							opt_level = insert_static(m, cd, ld, t_array->var, t_index, NULL, special);
							if ((opt_level == OPT_FULL) || (opt_level == OPT_LOWER))
								ip->op1 = OPT_FULL;
							break;
						case OPT_LOWER:	
							opt_level = insert_static(m, cd, ld, t_array->var, t_index, NULL, special);
							if ((opt_level == OPT_FULL) || (opt_level == OPT_UPPER))
								ip->op1 = OPT_FULL;
							break;
						case OPT_FULL:
#ifdef ENABLE_STATISTICS
							ld->c_stat_full_opt++;
#endif
							break;
							}
						}
					break;

				case TRACE_IVAR:			/* it's a variable					*/

					/* if the variable is changed between its usage as an index	*/
					/* of the array access and its push onto the stack, we have	*/
					/* to set the changes back to the time, it is pushed onto	*/
					/* the stack as an index variable.							*/
					t = backtrack_var(m, node, t_index->nr, i-1, t_index->var, tmp[t_index->var]);
#ifdef ENABLE_STATISTICS
					switch (ip->op1) {		/* take back old optimzation		*/
					case OPT_UNCHECKED:
						break;
					case OPT_NONE:
						ld->c_stat_no_opt--;
						break;
					case OPT_FULL:
						ld->c_stat_full_opt--;
						break;
					case OPT_UPPER:
						ld->c_stat_upper_opt--;
						break;
					case OPT_LOWER:
						ld->c_stat_lower_opt--;
						break;
						}
#endif
					if (degrade_checks)
						ip->op1 = insert_static(m, cd, ld, t_array->var, t_index, t, special);
					else {
						/* Check current optimization and try to improve it	by	*/
						/* insert new check. t reflects var changes for index	*/
						switch (ip->op1) {
						case OPT_UNCHECKED:
							ip->op1 = insert_static(m, cd, ld, t_array->var, t_index, t, special);
							break;
						case OPT_NONE:
							ip->op1 = insert_static(m, cd, ld, t_array->var, t_index, t, special);
							break;
						case OPT_UPPER:
							opt_level = insert_static(m, cd, ld, t_array->var, t_index, t, special);
							if ((opt_level == OPT_FULL) || (opt_level == OPT_LOWER))
								ip->op1 = OPT_FULL;
							break;
						case OPT_LOWER:	
							opt_level = insert_static(m, cd, ld, t_array->var, t_index, t, special);
							if ((opt_level == OPT_FULL) || (opt_level == OPT_UPPER))
								ip->op1 = OPT_FULL;
							break;
						case OPT_FULL:
#ifdef ENABLE_STATISTICS
							ld->c_stat_full_opt++;
#endif
							break;
							}
						}
					break;

				case TRACE_UNKNOWN: 
				case TRACE_AVAR:    
					break;
					}
				break;
     		
			case ICMD_ISTORE:		/* an integer value is stored				*/
				t_index = tracing(&bp, i-1, 0);	/* trace back its origin		*/

				/* the struct Changes for this variable needs to be updated		*/
				t = tmp[ip->op1];
				if (t == NULL) {	/* if it's the first one, create new entry	*/
					if ((t = (struct Changes *) malloc(sizeof(struct Changes))) == NULL)
						c_mem_error();
					t->upper_bound = t->lower_bound = 0;
					tmp[ip->op1] = t;
					}

				switch (t_index->type) {		/* check origin of store		*/

				case TRACE_ICONST:	/* constant -> set bounds to const value	*/
					t->upper_bound = t->lower_bound = t_index->constant;
					break;	

				case TRACE_IVAR:	/* if it's the same variable, update consts	*/  
					if ((t_index->var = ip->op1) && (t_index->neg > 0)) {
						t->upper_bound += t_index->constant;
						t->lower_bound += t_index->constant;
						}
					else
						t->lower_bound = t->upper_bound+1;
					break;

				case TRACE_ALENGTH:   /* else -> unknown						*/
				case TRACE_UNKNOWN: 
				case TRACE_AVAR: 
					t->lower_bound = t->upper_bound+1;   
					break;
					}

				break;

			case ICMD_IINC:			

				/* the struct Changes for this variable needs to be updated		*/
				if ((t = tmp[ip->op1]) == NULL) {	/* first one -> create new	*/
					if ((t = (struct Changes *) malloc(sizeof(struct Changes))) == NULL)
						c_mem_error();
					t->upper_bound = t->lower_bound = ip->val.i;
					tmp[ip->op1] = t;
					}  
				else {				/* update changes, made by iinc				*/
					t->upper_bound += ip->val.i;
					t->lower_bound += ip->val.i;
					}
				break;
				}	/* switch */
			}		/* for	  */
		
		if (!special) {				/* we are not interested in only the header	*/
			d = ld->c_dTable[node];
			while (d != NULL) {		/* check all sucessors of current node		*/
				remove_boundchecks(m, cd, ld, d->value, node, tmp, special);	
				d = d->next;
				}
			}
	    }	/* if */
}


/*	This function calls the bound-check removal function for the header node
	with a special flag. It is important to notice, that no dynamic 
	constraint hold in the header node (because the comparison is done at
	block end).
*/

void remove_header_boundchecks(methodinfo *m, codegendata *cd, loopdata *ld, int node, struct Changes **changes)
{
	remove_boundchecks(m, cd, ld, node, -1, changes, BOUNDCHECK_SPECIAL);
}


/*	Marks all basicblocks that are part of the loop
*/

void mark_loop_nodes(struct LoopContainer *lc)
{
	struct LoopElement *le = lc->nodes;

	while (le != NULL) {
		le->block->lflags |= LOOP_PART;
		le = le->next;
		}
}


/*	Clears mark for all basicblocks that are part of the loop
*/
void unmark_loop_nodes(LoopContainer *lc)
{
	LoopElement *le = lc->nodes;

	while (le != NULL) {
		le->block->lflags = 0;
		le = le->next;
	}
}


/*	This function performs the analysis of code in detected loops and trys to
	identify array accesses suitable for optimization (bound check removal). The
	intermediate code is then modified to reflect these optimizations.
*/
void optimize_single_loop(methodinfo *m, codegendata *cd, loopdata *ld, LoopContainer *lc)
{
	struct LoopElement *le;
	struct depthElement *d;
	int i, head, node;
	struct Changes **changes;

	if ((changes = (struct Changes **) malloc(jd->maxlocals * sizeof(struct Changes *))) == NULL)
		c_mem_error();

    head = ld->c_current_head = lc->loop_head;
	ld->c_needed_instr = ld->c_rs_needed_instr = 0;

	/* init array for null ptr checks */
	for (i=0; i<jd->maxlocals; ++i) 
		ld->c_null_check[i] = 0;


	/* don't optimize root node (it is the main procedure, not a loop)			*/
	if (head < 0)
		return;

	/* setup variables with initial values										*/
	ld->c_loopvars = NULL;
	for (i=0; i < m->basicblockcount; ++i) {
		ld->c_toVisit[i] = 0;
		ld->c_current_loop[i] = -1;
		if ((d = ld->c_dTable[i]) != NULL)
			d->changes = NULL;
		}

	for (i=0; i < jd->maxlocals; ++i) {
		ld->c_var_modified[i] = 0;
		if (changes[i] != NULL) {
			changes[i] = NULL;
			}
		}

	for (i=0; i < (jd->maxlocals+1); ++i) {
		if (ld->c_constraints[i] != NULL) {
		    ld->c_constraints[i] = NULL;
			}
		}

	le = lc->nodes;
	while (le != NULL) {
		node = le->node;

		if (node == head)
			ld->c_current_loop[node] = 1;   /* the header node gets 1               */
		else if (ld->c_nestedLoops[node] == head)
			ld->c_current_loop[node] = 2;	/* top level nodes get 2				*/
		else
			ld->c_current_loop[node] = 3;	/* nodes, part of nested loop get 3		*/
		
		ld->c_toVisit[node] = 1;
		le = le->next;
		}

	/* After setup work has been performed, start to analyse					*/
#ifdef LOOP_DEBUG
	printf("****** Starting analysis (%d)******\n", head);                  
	fflush(stdout);
#endif

	if (analyze_for_array_access(m, ld, head) > 0) {/* loop contains array access		*/

#ifdef LOOP_DEBUG
		struct LoopVar *lv;

		printf("analyze for array access finished and found\n");	
		fflush(stdout);
		lv = ld->c_loopvars;
		while (lv != NULL) {
			if (lv->modified)
				printf("Var --> %d\n", lv->value);
			lv = lv->next;
		}
#endif

		/* for performance reasons the list of all interesting loop vars is		*/
		/* scaned and for all modified vars a flag in c_var_modified is set		*/
		scan_global_list(ld);					

#ifdef LOOP_DEBUG
		printf("global list scanned\n");
		fflush(stdout);
#endif

		/* if the loop header contains or-conditions or an index variable		*/
		/* is modified in the catch-block within the loop, a conservative		*/
		/* approach is taken and optimizations are cancelled					*/
		if (analyze_or_exceptions(m, cd, ld, head, lc) > 0) {

#ifdef LOOP_DEBUG
			printf("Analyzed for or/exception - no problems \n");            
			fflush(stdout);
#endif

			init_constraints(m, ld, head);	/* analyze dynamic bounds in header			*/

#ifdef LOOP_DEBUG			
			show_right_side();
#endif    											

			if (ld->c_rightside == NULL)
				return;

			/* single pass bound check removal - for all successors, do			*/
			remove_header_boundchecks(m, cd, ld, head, changes);

			d = ld->c_dTable[head];
			while (d != NULL) {
				remove_boundchecks(m, cd, ld, d->value, -1, changes, BOUNDCHECK_REGULAR);
   				d = d->next;
				}
	    
#ifdef LOOP_DEBUG
			printf("Array-bound checks finished\n");							
			fflush(stdout);
#endif

			mark_loop_nodes(lc);

#ifdef LOOP_DEBUG			
			printf("START: create static checks\n");
			fflush(stdout);
#endif

			create_static_checks(m, cd, ld, lc);	/* create checks	  					*/

#ifdef LOOP_DEBUG
			printf("END: create static checks\n");
			fflush(stdout);
#endif
			unmark_loop_nodes(lc);
			}
		}
	/* else
		printf("No array accesses found\n");									*/

#ifdef ENABLE_STATISTICS
	ld->c_stat_num_loops++;		/* increase number of loops							*/	

	ld->c_stat_sum_accesses += ld->c_stat_array_accesses;
	ld->c_stat_sum_full += ld->c_stat_full_opt;
	ld->c_stat_sum_no += ld->c_stat_no_opt;
	ld->c_stat_sum_lower += ld->c_stat_lower_opt;
	ld->c_stat_sum_upper += ld->c_stat_upper_opt;
	ld->c_stat_sum_or += ld->c_stat_or;
	ld->c_stat_sum_exception += ld->c_stat_exception;

	ld->c_stat_array_accesses = 0;		
	ld->c_stat_full_opt = 0;
	ld->c_stat_no_opt = 0;
	ld->c_stat_lower_opt = 0;
	ld->c_stat_upper_opt = 0;	
	ld->c_stat_or = ld->c_stat_exception = 0;
#endif
	
}


/* optimize_loops **************************************************************

   This function preforms necessary setup work, before the recursive
   function optimize_single loop can be called.

*******************************************************************************/

void optimize_loops(jitdata *jd)
{
	methodinfo    *m;
	codegendata   *cd;
	loopdata      *ld;
	LoopContainer *lc;

	/* get required compiler data */

	m  = jd->m;
	cd = jd->cd;
	ld = jd->ld;

	lc = ld->c_allLoops;

	/* first, merge loops with same header node - all loops with the same		*/
	/* header node are optimizied in one pass, because they all depend on the	*/
	/* same dynamic loop condition												*/

#ifdef LOOP_DEBUG
	printf("start analyze_double_headers\n");
	fflush(stdout);
#endif

	analyze_double_headers(ld);

	/* create array with loop nesting levels - nested loops cause problems,		*/
	/* especially, when they modify index variables used in surrounding	loops	*/
	/* store results in array c_nestedLoops and c_hierarchie					*/

#ifdef LOOP_DEBUG
	printf("double done\n");
	fflush(stdout);
#endif

	analyze_nested(m,cd, ld);

#ifdef LOOP_DEBUG
	printf("analyze nested done\n");
	fflush(stdout);
#endif

	/* create array with entries for current loop								*/
	ld->c_current_loop = DMNEW(int, m->basicblockcount);	
	ld->c_toVisit = DMNEW(int, m->basicblockcount);
	ld->c_var_modified = DMNEW(int, jd->maxlocals);
	ld->c_null_check = DMNEW(int, jd->maxlocals);

	if ((ld->c_constraints = (struct Constraint **) malloc((jd->maxlocals+1) * sizeof(struct Constraint *))) == NULL)
		c_mem_error();

#ifdef ENABLE_STATISTICS
	ld->c_stat_num_loops = 0;		/* set statistic vars to zero					*/
	ld->c_stat_array_accesses = ld->c_stat_sum_accesses = 0;		
	ld->c_stat_full_opt = ld->c_stat_sum_full = 0;
	ld->c_stat_no_opt = ld->c_stat_sum_no = 0;
	ld->c_stat_lower_opt = ld->c_stat_sum_lower = 0;
	ld->c_stat_upper_opt = ld->c_stat_sum_upper = 0;
	ld->c_stat_or = ld->c_stat_sum_or = 0;
	ld->c_stat_exception = ld->c_stat_sum_exception = 0;
#endif
 
	/* init vars needed by all loops                                            */
	ld->c_needs_redirection = false;
	ld->c_newstart = NULL;
	ld->c_old_xtablelength = jd->exceptiontablelength;

	/* loops have been topologically sorted                                     */
	lc = ld->c_allLoops;
	while (lc != NULL) {
		optimize_single_loop(m, cd, ld, lc);

#ifdef LOOP_DEBUG
		printf(" *** Optimized loop *** \n");
		fflush(stdout);
#endif

		lc = lc->next;
		}
#ifdef LOOP_DEBUG
	printf("---*** All loops finished ***---\n");
	fflush(stdout);
#endif

	/* if global BB list start is modified, set block to new start              */
	if (ld->c_needs_redirection == true)
		m->basicblocks = ld->c_newstart;

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

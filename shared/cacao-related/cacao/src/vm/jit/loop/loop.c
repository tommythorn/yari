/* src/vm/jit/loop/loop.c - array bound removal

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

   The loop detection is performed according to Lengauer-Tarjan
   algorithm that uses dominator trees (found eg. in modern compiler
   implementation by a.w. appel)

   $Id: loop.c 4699 2006-03-28 14:52:32Z twisti $

*/


#include <assert.h>
/*  #include <stdio.h> */
#include <stdlib.h>

#include "mm/memory.h"
#include "toolbox/logging.h"
#include "vm/global.h"	
#include "vm/jit/jit.h"	
#include "vm/jit/loop/loop.h"
#include "vm/jit/loop/graph.h"
#include "vm/jit/loop/tracing.h"


/*	
   This function allocates and initializes variables, that are used by the
   loop detection algorithm
*/
void setup(methodinfo *m, loopdata *ld)
{
	int i;

	ld->c_semi_dom = DMNEW(int, m->basicblockcount);
	ld->c_idom = DMNEW(int, m->basicblockcount);
	ld->c_same_dom = DMNEW(int, m->basicblockcount);
	ld->c_numBucket = DMNEW(int, m->basicblockcount);
	ld->c_ancestor = DMNEW(int, m->basicblockcount);
	ld->c_contains = DMNEW(int, m->basicblockcount);
	ld->c_stack = DMNEW(int, m->basicblockcount);
	ld->c_bucket = DMNEW(int*, m->basicblockcount);
  
	for (i = 0; i < m->basicblockcount; ++i) {
		ld->c_numBucket[i] = 0;
		ld->c_stack[i] = ld->c_ancestor[i] = ld->c_semi_dom[i] = ld->c_same_dom[i] = ld->c_idom[i] = -1;
	  
		ld->c_bucket[i] = DMNEW(int, m->basicblockcount);
	}
}


/*	This function is a helper function for dominators and has to find the 
	ancestor of the node v in the control graph, which semi-dominator has the  
	lowest def-num.
*/

int findLowAnc(loopdata *ld, int v)
{
	int u = v;			/* u is the node which has the current lowest semi-dom	*/
  
	while (ld->c_ancestor[v] != -1) {	/* as long as v has an ancestor, continue	*/
		if (ld->c_defnum[ld->c_semi_dom[v]] < ld->c_defnum[ld->c_semi_dom[u]])	
									/* if v's semi-dom is smaller				*/
			u = v;					/* it gets the new current node u			*/
		v = ld->c_ancestor[v];			/* climb one step up in the tree			*/
		}
	return u;			/* return node with the lowest semi-dominator def-num	*/
}


/*	This function builds the dominator tree out of a given control flow graph and 
	stores its results in c_idom[]. It first calculates the number of possible
	dominators in c_bucket and eventually determines the single dominator in a 
	final pass.
*/

void dominators(loopdata *ld)
{
	int i, j, semi, s, n, v, actual, p, y;
  
	for (n=(ld->c_globalCount-1); n>0; --n) {	/* for all nodes (except last), do	*/
		actual = ld->c_reverse[n];
		semi = p = ld->c_parent[actual];		
	
		/* for all predecessors of current node, do								*/
		for (i=0; i<ld->c_numPre[actual]; ++i) {
			v = ld->c_pre[actual][i];
      
			if (ld->c_defnum[v] <= ld->c_defnum[actual])
				s = v;			/* if predecessor has lower def-num	than node	*/
								/* it becomes candidate for semi dominator		*/
			else
				s = ld->c_semi_dom[findLowAnc(ld, v)];
								/* else the semi-dominator of it's ancestor		*/
								/* with lowest def-num becomes candidate		*/
			
			if (ld->c_defnum[s] < ld->c_defnum[semi])
				semi = s;		/* if the def-num of the new candidate is lower */
								/* than old one, it gets new semi dominator		*/
			}
    
		/* write semi dominator -> according to	SEMIDOMINATOR THEOREM			*/
		ld->c_semi_dom[actual] = semi;				
		ld->c_ancestor[actual] = p;					
    
		ld->c_bucket[semi][ld->c_numBucket[semi]] = actual;
		ld->c_numBucket[semi]++;	/* defer calculation of dominator to final pass */
      

		/* first clause of DOMINATOR THEOREM, try to find dominator now			*/
		for (j=0; j<ld->c_numBucket[p]; ++j) {
			v = ld->c_bucket[p][j];
			y = findLowAnc(ld, v);
      
			if (ld->c_semi_dom[y] == ld->c_semi_dom[v])	
				ld->c_idom[v] = p;			/* if y's dominator is already known	*/
										/* found it and write to c_idom			*/
			else
				ld->c_same_dom[v] = y;		/* wait till final pass					*/
			}
		
		ld->c_numBucket[p] = 0;
		}
  
	/* final pass to get missing dominators ->second clause of DOMINATOR THEORM	*/
	for (j=1; j<(ld->c_globalCount-1); ++j) {		
		if (ld->c_same_dom[ld->c_reverse[j]] != -1)	
			ld->c_idom[ld->c_reverse[j]] = ld->c_idom[ld->c_same_dom[ld->c_reverse[j]]];
		}
}


/*	
   A helper function needed by detectLoops() that checks, whether a given 
   connection between two nodes in the control flow graph is possibly part
   of a loop (is a backEdge).
*/

int isBackEdge(loopdata *ld, int from, int to)
{
	int tmp = ld->c_idom[to];	/* speed optimization: if the to-node is dominated	*/
	while (tmp != -1) {		/* by the from node as it is most of the time, 		*/
		if (tmp == from)	/* there is no backEdge								*/
			return 0;
		tmp = ld->c_idom[tmp];
		}

	tmp = ld->c_idom[from];		/* if from-node doesn't dominate to-node, we have	*/
	while (tmp != -1) {		/* to climb all the way up from the from-node to	*/
		if (tmp == to)		/* the top to check, whether it is dominated by to	*/
			return 1;		/* if so, return a backedge							*/
		tmp = ld->c_idom[tmp];
		}

	return 0;				/* else, there is no backedge						*/
}


/*	
   These stack functions are helper functions for createLoop(int, int)  
   to manage the set of nodes in the current loop.
*/

void push(methodinfo *m, loopdata *ld, int i, struct LoopContainer *lc)
{
	struct LoopElement *le = lc->nodes, *t;

	if (!ld->c_contains[i])	{
		t = DMNEW(struct LoopElement, 1);
		
		t->node = i;
		t->block = &m->basicblocks[i];

		ld->c_contains[i] = 1;

		if (i < le->node)
		{
			t->next = lc->nodes;
			lc->nodes = t;
		}
		else
		{
			while ((le->next != NULL) && (le->next->node < i))
				le = le->next;

			t->next = le->next;
			le->next = t;
		}

		ld->c_stack[ld->c_stackPointer++] = i;
	}
}


int pop(loopdata *ld)
{
	return (ld->c_stack[--ld->c_stackPointer]);
}


int isFull(loopdata *ld)
{
	return (ld->c_stackPointer);
}


/*	
   This function is a helper function, that finds all nodes, that belong to 
   the loop with a known header node and a member node of the loop (and a 
   back edge between these two nodes).
*/

void createLoop(methodinfo *m, loopdata *ld, int header, int member)
{
	int i, nextMember;

	struct LoopContainer *currentLoop = (struct LoopContainer *) malloc(sizeof(struct LoopContainer));
	LoopContainerInit(m, currentLoop, header);		/* set up loop structure		*/
	
	for (i=0; i<m->basicblockcount; ++i)
		ld->c_contains[i] = 0;
	ld->c_contains[header] = 1;

	ld->c_stackPointer = 0;				/* init stack with first node of the loop	*/
	push(m, ld, member, currentLoop);

	while (isFull(ld)) {				/* while there are still unvisited nodes	*/
		nextMember = pop(ld);
		
		/* push all predecessors, while they are not equal to loop header		*/
		for (i=0; i<ld->c_numPre[nextMember]; ++i)			
			push(m, ld, ld->c_pre[nextMember][i], currentLoop);		
		}

	currentLoop->next = ld->c_allLoops;
	ld->c_allLoops = currentLoop;
}


/*	After all dominators have been calculated, the loops can be detected and
	 added to the global list c_allLoops.
*/

void detectLoops(methodinfo *m, loopdata *ld)
{
	int i;
	struct depthElement *h;

	/* for all edges in the control flow graph do								*/
	for (i=0; i<m->basicblockcount; ++i) {			
		h = ld->c_dTable[i];

		while (h != NULL) {
			/* if it's a backedge, than add a new loop to list					*/
			if (isBackEdge(ld, i, h->value))	 
				createLoop(m, ld, h->value, i);
			h = h->next;
			}
		}
}


/*	
   This function is called by higher level routines to perform the loop 
   detection and set up the c_allLoops list.
*/

void analyseGraph(jitdata *jd)
{
	methodinfo *m;
	loopdata   *ld;

	/* get required compiler data */

	m  = jd->m;
	ld = jd->ld;

	setup(m, ld);
	dominators(ld);
	detectLoops(m, ld);
}


/*
   Test function -> will be removed in final release
*/

void resultPass2(loopdata *ld)
{
  int i;
  struct LoopContainer *lc = ld->c_allLoops;
  struct LoopElement *le;
  
  printf("\n\n****** PASS 2 ******\n\n");
  
  printf("Loops:\n\n");
  
  i = 0;
  while (lc != NULL) {
	  printf("Loop [%d]: ", ++i);

  	  le = lc->nodes;
	  while (le != NULL) {
	    printf("%d ", le->node);
		printf("\n");
		le = le->next;
	  }

	  lc = lc->next;
  }

  printf("\n");

}


void c_mem_error()
{
	log_text("C_ERROR: Not enough memeory");
	assert(0);
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

/* src/vm/jit/optimizing/graph.c - CFG

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

   $Id: graph.c$

*/

#include "mm/memory.h"

#include "toolbox/bitvector.h"

#include "vm/jit/optimizing/lsra.h"
#include "vm/jit/optimizing/ssa.h"
#include "vm/jit/optimizing/graph.h"

#ifdef GRAPH_DEBUG_VERBOSE
#include "vm/options.h"
#endif


/* Helpers for graph_make_cfg */
void graph_add_jsr( methodinfo *m, graphdata *gd, struct _sbr *sbr,int from,
					int to);
void graph_add_cfg( methodinfo *m, graphdata *gd, int from, int to);
void graph_add_exceptions(methodinfo *m, codegendata *cd, graphdata *gd); 
void graph_add_subs(methodinfo *m, graphdata *gd, struct _sbr *sbr);
void graph_add_edge( graphdata *gd, int from, int to );
/* Helper for graph_make_subs */
void graph_add_sub( methodinfo *m, graphdata *gd, int b_index,
					graph_element *ret, bool *visited );
/* Helper for graph_get_first_* */
int graph_get_first_(graph_element *ge, graphiterator *i);
void transform_CFG(jitdata *, graphdata *);

#ifdef GRAPH_DEBUG_VERBOSE
void graph_print(lsradata *ls, graphdata *gd);
#endif


graphdata *graph_init(int basicblockcount) {
	graphdata *gd;
	int i;

	gd = NEW(graphdata);
#ifdef GRAPH_DEBUG_CHECK
	/* Remember basicblockcount, so Array Bound checks can be made */
	gd->basicblockcount = basicblockcount;
#endif

	gd->num_succ = DMNEW(int, basicblockcount);
	gd->successor = DMNEW(graph_element *, basicblockcount);
	
	gd->num_pred = DMNEW(int, basicblockcount);
	gd->predecessor = DMNEW(graph_element *, basicblockcount);

	for(i = 0; i < basicblockcount; i++) {
		gd->num_succ[i] = gd->num_pred[i] = 0;
		gd->successor[i] = NULL;
		gd->predecessor[i] = NULL;
	}
	return gd;
}

int graph_get_num_predecessor(graphdata *gd, int b_index) {
	_GRAPH_CHECK_BOUNDS(b_index, 0, gd->basicblockcount);
	return gd->num_pred[b_index];
}

int graph_get_num_successor(graphdata *gd, int b_index) {
	_GRAPH_CHECK_BOUNDS(b_index, 0, gd->basicblockcount);
	return gd->num_succ[b_index];
}

int graph_get_first_successor(graphdata *gd, int b_index, graphiterator *i) {
	_GRAPH_CHECK_BOUNDS(b_index, 0, gd->basicblockcount);
	return graph_get_first_(gd->successor[b_index], i);
}

int graph_get_first_predecessor(graphdata *gd, int b_index, graphiterator *i) {
	_GRAPH_CHECK_BOUNDS(b_index, 0, gd->basicblockcount);
	return graph_get_first_(gd->predecessor[b_index], i);
}

int graph_get_next(graphiterator *i) {
	if ((*i) == NULL)
		return -1;
	(*i) = (*i)->next;
	if ( (*i) == NULL )
		return -1;
	else
		return (*i)->value;
}

int graph_get_first_(graph_element *ge, graphiterator *i) {
	if ( ((*i) = ge) == NULL)
		return -1;
	else
		return (*i)->value;
}

bool graph_has_multiple_predecessors( graphdata *gd, int b_index) {
	_GRAPH_CHECK_BOUNDS(b_index, 0, gd->basicblockcount);
	return (gd->num_pred[b_index] > 1);
}

bool graph_has_multiple_successors( graphdata *gd, int b_index) {
	_GRAPH_CHECK_BOUNDS(b_index, 0, gd->basicblockcount);
	return (gd->num_succ[b_index] > 1);
}

void graph_add_edge( graphdata *gd, int from, int to ) {
	graphiterator i;
	graph_element *n;
	int b;

	/* prevent multiple similar edges from TABLESWITCH and */
	/* LOOKUPSWITCH */
	b = graph_get_first_successor(gd, from, &i);
	for( ; (b != -1) && ( b != to); b = graph_get_next(&i));
	if (b != -1) /* edge from->to already existing */
		return;

	/* We need two new graph_elements. One for successors and one for */
	/* predecessors */
	n = DMNEW(graph_element, 2);
			
	n->value=to;
	n->next=gd->successor[from];
	gd->successor[from]=n;
	gd->num_succ[from]++;

	n++; /* take next allocated graph_element */
	n->value=from;
	n->next=gd->predecessor[to];
	gd->predecessor[to]=n;
	gd->num_pred[to]++;
}

/* split the edge from BB from shown by iterator i wiht new_block */
void graph_split_edge(graphdata *gd, int from, graphiterator *i, int new_block) {
	graphiterator i_pred;
	graph_element *n;
	int l, succ;

	/* i->value is the BB index of the "old" successor */
	succ = (*i)->value;
	/* search for iterator showing predecessor edge from BB succ back to */
	/* from */
	l = graph_get_first_predecessor(gd, succ, &i_pred);
	for(; (l != -1) && (l != from); l = graph_get_next(&i_pred));
	_GRAPH_ASSERT(l == from);

	/* change CFG entries */
	(*i)->value = new_block;
	i_pred->value = new_block;

	/* and insert the CFG successor and predecesser entries for new_block */
	/* 2 entries needed */
	n = DMNEW(graph_element, 2);
	/* make the successor entry for new_block */
	n->value = succ;
	n->next = gd->successor[new_block];
	gd->successor[new_block] = n;
	gd->num_succ[new_block]++;

	n++;
	/* make the predecessor entry for new_block */
	n->value = from;
	n->next = gd->predecessor[new_block];
	gd->predecessor[new_block] = n;
	gd->num_pred[new_block]++;
}

/***********************************************
Generate the Control Flow Graph             
( pred,succ,num_pred of lsradata structure) 
************************************************/
void graph_make_cfg(jitdata *jd,graphdata *gd) {
	instruction *ip;
	s4 *s4ptr;
	int high, low, count;
	int b_index, len;
	bool fall_through;
	struct _sbr sbr; /* list of subroutines, sorted by header */
	methodinfo *m;
	codegendata *cd;
	registerdata *rd;
	lsradata *ls;

	m = jd->m;
	cd = jd->cd;
	rd = jd->rd;
	ls = jd->ls;

	sbr.next = NULL;

	/* Add edge from new Basic Block 0 (parameter initialization) */
	graph_add_edge(gd, 0, 1);

	b_index=0;
	while (b_index < m->basicblockcount ) {
		if (m->basicblocks[b_index].flags >= BBREACHED) {
			fall_through = false;
									
			if ((len = m->basicblocks[b_index].icount)) {
				/* set ip to last non NOP instruction	*/
				ip = m->basicblocks[b_index].iinstr +
					m->basicblocks[b_index].icount -1;
				while ((len>0) && (ip->opc == ICMD_NOP)) {
					len--;
					ip--;
				}
				/* block contains instructions	*/
				switch (ip->opc) {			/* check type of last instruction */
				case ICMD_RETURN:
				case ICMD_IRETURN:
				case ICMD_LRETURN:
				case ICMD_FRETURN:
				case ICMD_DRETURN:
				case ICMD_ARETURN:
				case ICMD_ATHROW:
/* 					graph_add_cfg(m, gd, b_index, m->basicblockcount); */
					break;				  /* function returns -> end of graph */

				case ICMD_IFNULL:
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
				case ICMD_IF_ICMPEQ:
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
				case ICMD_IF_ACMPNE:		    /* branch -> add next block */
					fall_through = true;
/* 					graph_add_cfg(m, gd, b_index, b_index+1); */
					/* fall throu -> add branch target */
			   
				case ICMD_GOTO:
					graph_add_cfg(m, gd, b_index,  m->basicblockindex[ip->op1]);
					if (fall_through)
						graph_add_cfg(m, gd, b_index, b_index+1);
					break;					/* visit branch (goto) target	*/
				
				case ICMD_TABLESWITCH:		/* switch statement				*/
					s4ptr = ip->val.a;
				
					graph_add_cfg(m, gd, b_index,  m->basicblockindex[*s4ptr]);
				
					s4ptr++;
					low = *s4ptr;
					s4ptr++;
					high = *s4ptr;
				
					count = (high-low+1);
				
					while (--count >= 0) {
						s4ptr++;
						graph_add_cfg(m, gd, b_index, 
									 m->basicblockindex[*s4ptr]);
				    }
					break;
				
				case ICMD_LOOKUPSWITCH:		/* switch statement				*/
					s4ptr = ip->val.a;
			   
					graph_add_cfg(m, gd, b_index,  m->basicblockindex[*s4ptr]);
				
					++s4ptr;
					count = *s4ptr++;
				
					while (--count >= 0) {
						graph_add_cfg(m, gd, b_index,
									  m->basicblockindex[s4ptr[1]]);
						s4ptr += 2;
				    }
					break;

				case ICMD_JSR:
					graph_add_jsr(m, gd, &sbr, b_index, m->basicblockindex[ip->op1]);
					break;
				
				case ICMD_RET:
					break;
				
				default:
					graph_add_cfg(m, gd, b_index, b_index + 1 );
					break;	
			    } /* switch (ip->opc)*/                        
		    }     /* if (m->basicblocks[blockIndex].icount) */
	    }         /* if (m->basicblocks[b_index].flags >= BBREACHED) */
		b_index++;
	}             /* while (b_index < m->basicblockcount ) */

	/* add subroutines before exceptions! They "destroy" the CFG */
	graph_add_subs(m, gd, &sbr); 
 	graph_add_exceptions(m, cd, gd);
	transform_CFG(jd, gd);
#ifdef GRAPH_DEBUG_VERBOSE
	if (compileverbose)
		graph_print(ls, gd);
#endif
}

/*****************************************************************
add Edges from guarded Areas to Exception handlers in the CFG
*****************************************************************/
void graph_add_exceptions(methodinfo *m, codegendata *cd, graphdata *gd) { 
	int i;

	/* add cfg edges from all bb of a try block to the start of the according */
	/* exception handler to ensure the right order after depthfirst search    */
	exceptiontable *ex;
	ex=jd->exceptiontable;
#ifdef GRAPH_DEBUG_VERBOSE
	if (compileverbose)
		printf("ExTable(%i): ", jd->exceptiontablelength);
#endif

	for (; ex != NULL; ex = ex->down) {

#ifdef GRAPH_DEBUG_VERBOSE
		if (compileverbose)
			printf("[%i-%i]->%i ",ex->start->nr, ex->end->nr,
				   ex->handler->nr);
#endif
		_GRAPH_ASSERT(ex->handler->nr < m->basicblockcount);
		_GRAPH_ASSERT(m->basicblocks[ex->handler->nr].flags >= BBREACHED);
		_GRAPH_ASSERT(ex->start->nr <= ex->end->nr);

		/* loop all valid Basic Blocks of the guarded area and add CFG edges  */
		/* to the appropriate handler */
		for (i=ex->start->nr; (i <= ex->end->nr) &&
				 (i < m->basicblockcount); i++)
			if (m->basicblocks[i].flags >= BBREACHED)
				graph_add_cfg(m, gd, i, ex->handler->nr);
	}
#ifdef GRAPH_DEBUG_VERBOSE
	if (compileverbose)
		printf("\n");
#endif
}

/**************************************************
Add subroutines from ls->sbr list to CFG
**************************************************/
void graph_add_subs(methodinfo *m, graphdata *gd, struct _sbr *sbr) {
	struct _sbr *_sbr;
	bool *visited;
	int i;
#ifdef GRAPH_DEBUG_VERBOSE
	graph_element *ret;
#endif

	visited = (bool *)DMNEW(int, m->basicblockcount + 1);
	for (i=0; i <= m->basicblockcount; i++) visited[i] = false;
	for (_sbr = sbr->next; _sbr != NULL; _sbr=_sbr->next) {
#ifdef GRAPH_DEBUG_VERBOSE
		if (compileverbose) {
			printf("Subroutine Header: %3i Return Adresses:",_sbr->header);
			for (ret = _sbr->ret; ret != NULL; ret = ret->next)
				printf(" %3i", ret->value);
			printf("\n");
		}
#endif
		graph_add_sub( m, gd, _sbr->header, _sbr->ret, visited );

	}
}

/******************************************************************
Add the CFG Edge into next und succ
******************************************************************/
void graph_add_cfg( methodinfo *m, graphdata *gd, int from, int to) {

	/* ignore Empty, Deleted,... Basic Blocks as target */
	/* TODO: Setup BasicBlock array before to avoid this */
	/*       best together with using the basicblock list, so lsra works */
	/*       with opt_loops, too */
	for (;(to < m->basicblockcount) && (m->basicblocks[to].flags < BBREACHED);
		 to++);

	/* add one to from and to, so the to be inserted Basic Block 0 is */
	/* already regarded */
	graph_add_edge( gd, from + 1, to + 1);
}

/*******************************************************************
Remember Subroutine "jumps" in ls->sbr and add the CFG Edge of the 
"jump" into succ and pred 
*******************************************************************/
void graph_add_jsr( methodinfo *m, graphdata *gd, struct _sbr *sbr, int from,
					int to) {
	struct _sbr *_sbr, *n;
	graph_element *ret;

	/* ignore Empty, Deleted,... Basic Blocks as target */
	/* TODO: Setup BasicBlock array before to avoid this */
	/*       best together with using the basicblock list, so lsra works */
	/*       with opt_loops, too */
	for (; (to < m->basicblockcount) && (m->basicblocks[to].flags < BBREACHED);
		 to++);
	_GRAPH_ASSERT( to != m->basicblockcount );

	graph_add_cfg(m, gd, from, to);

	/* from + 1 ist the return Basic Block Index */
	for (from++; (from < m->basicblockcount) &&
			 (m->basicblocks[from].flags < BBREACHED); from++);
	_GRAPH_ASSERT(from != m->basicblockcount);

	/* add subroutine info in ls->sbr.next */

	/* search for right place to insert */
	for (_sbr = sbr; (_sbr->next != NULL) && (_sbr->next->header < to);
		 _sbr=_sbr->next);
	
	if ((_sbr->next!= NULL) && (_sbr->next->header == to)) {
		/* Entry for this sub already exist */
		_sbr = _sbr->next;
	} else {
		/* make new Entry and insert it in ls->sbr.next */
		n = DNEW( struct _sbr );
		n->header = to;
		n->ret = NULL;

		n->next = _sbr->next;
		_sbr->next = n;

		_sbr = n;
	}

	/* now insert return adress in sbr->ret */
	ret = DNEW(graph_element);
	ret->value = from;
	ret->next = _sbr->ret;
	_sbr->ret = ret;
}

/**************************************************
Add a subroutine to CFG
**************************************************/
void graph_add_sub(methodinfo *m, graphdata *gd, int b_index,
				   graph_element *ret, bool *visited ) {
	graph_element *l;
	instruction *ip;
	bool next_block;
	graphiterator i;
	int s;

	/* break at virtual End Block */
	if (b_index != m->basicblockcount) {
		visited[b_index] = true;
		next_block = false;

		if (m->basicblocks[b_index].flags < BBREACHED)
			next_block = true;
		if (!next_block && !(m->basicblocks[b_index].icount))
			next_block = true;

		if (!next_block) {
			ip = m->basicblocks[b_index].iinstr +
				m->basicblocks[b_index].icount - 1;
		
			if (ip->opc == ICMD_JSR) /* nested Subroutines */
				next_block = true;
		}

		if (!next_block) {
			if (ip->opc == ICMD_RET) {
				/* subroutine return found -> add return adresses to CFG */
				for (l = ret; l != NULL; l = l->next)
					graph_add_cfg( m, gd, b_index, l->value);
			} else { /* follow CFG */
				s = graph_get_first_successor(gd, b_index, &i); 
				for(; s != -1; s = graph_get_next(&i))
					if (!visited[s])
						graph_add_sub( m, gd, l->value, ret, visited);
			}
		} else { /* fall through to next block */
			if (b_index + 1 < m->basicblockcount)
				if (!visited[b_index + 1])
					graph_add_sub(m, gd, b_index + 1, ret, visited);
		}
	}
}

/*****************************************************************
Sort Basic Blocks using Depth First search in reverse post order
In: ls->basicblockcount, ls->basicblocks[], gd->
Out: ls->sorted, ls->sorted_rev
*****************************************************************/

void graph_DFS(lsradata *ls, graphdata *gd) {
	int *stack;
	int *visited;
	int stack_top;
	bool not_finished;
	int i,p,s, num_pred;
	graphiterator iter;

	ls->sorted = DMNEW(int, ls->basicblockcount);
	ls->sorted_rev = DMNEW(int, ls->basicblockcount);

	stack = DMNEW( int, ls->basicblockcount);
	visited = (int *)DMNEW( bool, ls->basicblockcount);
	for (i = 0; i < ls->basicblockcount; i++) {
		visited[i] = 0;
		ls->sorted[i]=-1;
		ls->sorted_rev[i]=-1;
	}

    stack[0] = 0; /* start with Block 0 */
	stack_top = 1;
	/* Start Block is handled right and can be put in sorted */
	visited[0] = graph_get_num_predecessor(gd , 0);
	p = 0; 
	not_finished = true;
	while (not_finished) {
		while (stack_top != 0) {
			stack_top--;
			i = stack[stack_top];
			ls->sorted[p]=i;
			p++;
			s = graph_get_first_successor(gd, i, &iter);
			for (; s != -1; s = graph_get_next(&iter)) {
				visited[s]++;
				if (visited[s] ==  graph_get_num_predecessor(gd, s)) {
					/* push the node on the stack, only if all ancestors have */
					/* been visited */
					stack[stack_top] = s;
					stack_top++;
				}
			}
		}
		not_finished = false;
		for (i=1; i < ls->basicblockcount; i++) {
			/* search for visited blocks, which have not reached the num_pre */
			/* and put them on the stack -> happens with backedges */
			num_pred = graph_get_num_predecessor(gd, i);
			if ((visited[i] != 0) && (visited[i] < num_pred)) {
				stack[stack_top] = i;
				stack_top++;
				visited[i] = num_pred;
				not_finished=true;
				break;
			}
		}
	}

	for(i = 0; i < ls->basicblockcount; i++)
		if (ls->sorted[i] != -1)
			 ls->sorted_rev[ls->sorted[i]] = i;
}


void graph_init_basicblock( basicblock *bptr, int b_index) {
		bptr->nr = b_index;
		bptr->icount = 0;
		bptr->iinstr = NULL;
		bptr->type = BBTYPE_STD;
		bptr->flags = BBFINISHED;
		bptr->instack = NULL;
		bptr->outstack = NULL;
		bptr->indepth = 0;
		bptr->outdepth = 0;
		bptr->branchrefs = NULL;
		bptr->mpc = -1;
		bptr->next = NULL;
}

/*********************************************************************+
After the "original" CFG has been created, it has to be adopted for
SSA. (inluding insertion of new Basic Blocks)

TODO: Do not insert blocks right now - just adopt the call graph!
      After the phi moves are determined create only the needed Blocks 
**********************************************************************/
void transform_CFG(jitdata *jd, graphdata *gd) {
	int i, j, k, n, num_new_blocks;
	int **var_def;
	basicblock *tmp;
	stackptr in, out, new_stack;
	graphiterator iter;
	int *num_succ;
	struct graph_element **successor;
	int *num_pred;
	struct graph_element **predecessor;
	methodinfo *m;
	codegendata *cd;
	registerdata *rd;
	lsradata *ls;

	m = jd->m;
	cd = jd->cd;
	rd = jd->rd;
	ls = jd->ls;

	/* with SSA a new Basic Block 0 is inserted for parameter initialisation  */
	/* & look for nodes with multiple successors leading to nodes with        */
	/* multiple predecessor -> if found insert a new block between to split   */
	/* this edge. As first step count how many blocks have to be inserted.    */

	num_new_blocks = 0;
	for(i = 0; i< m->basicblockcount + 1; i++) {
		if (graph_has_multiple_successors(gd, i)) {
			k = graph_get_first_successor(gd, i, &iter);
			for(; k != -1; k = graph_get_next(&iter)) {
				/* check for successor blocks with more than one       */
				/* predecessor. For each found incremet num_new_blocks */
				if (graph_has_multiple_predecessors(gd, k))
					num_new_blocks++;
			}
		}
	}
	
	/* increase now basicblockcount accordingly. */
	ls->basicblockcount = m->basicblockcount + num_new_blocks + 1;

	ls->basicblocks = DMNEW(basicblock *, ls->basicblockcount);
	/* copy Basic Block References to ls->basicblocks */
	for(i = 0; i< m->basicblockcount; i++) {
		ls->basicblocks[i+1] = &(m->basicblocks[i]);
		ls->basicblocks[i+1]->nr = i+1;
	}
	
	/* Create new Basic Blocks: 0, [m->basicblockcount..ls->basicblockcount[ */
	/* num_new_blocks + 1 have to be inserted*/
	tmp = DMNEW( basicblock, num_new_blocks + 1);
	ls->basicblocks[0] = tmp;
	graph_init_basicblock( tmp, 0);
	tmp++;
	ls->basicblocks[0]->next = ls->basicblocks[1];

	if (ls->basicblockcount > m->basicblockcount + 1) {
		/* new Blocks have to be inserted                   */
		num_succ = DMNEW(int, ls->basicblockcount);
		successor = DMNEW(graph_element *, ls->basicblockcount);
	
		num_pred = DMNEW(int, ls->basicblockcount);
		predecessor = DMNEW(graph_element *, ls->basicblockcount);

		/* regard the + 1 for the already regarded new BB 0 */
		/* So recreate ls->var_def                          */
		var_def = DMNEW(int *, ls->basicblockcount);
		for(i = 0; i < m->basicblockcount + 1; i++) {
			var_def[i] = ls->var_def[i];
			num_succ[i] = gd->num_succ[i];
			num_pred[i] = gd->num_pred[i];
			successor[i] = gd->successor[i];
			predecessor[i] = gd->predecessor[i];
		}
		for(i = m->basicblockcount + 1; i < ls->basicblockcount; i++) {
			var_def[i] = bv_new(ls->max_vars);
			graph_init_basicblock( tmp, i);
			ls->basicblocks[i] = tmp;
			tmp++;

			num_succ[i] = 0;
			num_pred[i] = 0;
			successor[i] = NULL;
			predecessor[i] = NULL;
		}
		ls->var_def = var_def;
		gd->num_succ = num_succ;
		gd->num_pred = num_pred;
		gd->successor = successor;
		gd->predecessor = predecessor;
#ifdef GRAPH_DEBUG_CHECK
		/* Remember basicblockcount, so Array Bound checks can be made */
		gd->basicblockcount = ls->basicblockcount;
#endif
	}

	/* Now Split the edges */
	num_new_blocks = m->basicblockcount + 1; /* first free new block index */
	for(i = 0; i < m->basicblockcount + 1; i++) {
		if (graph_has_multiple_successors(gd, i)) {/* more than one successor */
			j = graph_get_first_successor( gd, i, &iter);
			for(; j != -1; j = graph_get_next(&iter)) {
				if (graph_has_multiple_predecessors(gd, j)) {
					/* and more than one predecessor -> split edge */
					_GRAPH_ASSERT(num_new_blocks < ls->basicblockcount);

					/* insert new Block num_new_blocks */

					/* splite the edge from BB i to j with the new BB */
					/* num_new_blocks ( i->j --> i->nnb->j)*/
					/* iter_succ shows the edge from i to j */
					graph_split_edge(gd, i, &iter, num_new_blocks);

					ls->basicblocks[num_new_blocks]->indepth =
						ls->basicblocks[i]->outdepth;
					out = ls->basicblocks[i]->outstack;
					ls->basicblocks[num_new_blocks]->instack = in = NULL;

					if (ls->basicblocks[num_new_blocks]->indepth > 0) 
						new_stack = DMNEW( stackelement,
									  ls->basicblocks[num_new_blocks]->indepth);
					for(n=0; n<ls->basicblocks[num_new_blocks]->indepth; n++) {
						if (in == NULL) {
							in = new_stack++;
						} else {
							in->prev = new_stack++;
							in = in->prev;
						}
						in->type = out->type;
						in->varkind = STACKVAR;
						in->varnum = 0;
						in->regoff = 0;
						in->flags = 0;
						in->prev = NULL;
						/* Add Definition */
						if (n == 0)
							ls->basicblocks[num_new_blocks]->instack = in;

						out = out->prev;
					}

					/* Create Outstack */
					ls->basicblocks[num_new_blocks]->outstack =
						ls->basicblocks[num_new_blocks]->instack;
					ls->basicblocks[num_new_blocks]->outdepth = 
						ls->basicblocks[num_new_blocks]->indepth;

					_GRAPH_ASSERT(ls->basicblocks[num_new_blocks]->outdepth == 
							ls->basicblocks[j]->indepth );

					/* Add Definition */
					/* decrease nr temporarly, because ssa_set_interface*/
					/* adds 1 since it is called from stack.c, where there is */
					/* no new BB 0 inserted like now */
					ls->basicblocks[num_new_blocks]->nr--;
					ssa_set_interface(cd, ls, ls->basicblocks[num_new_blocks]);
					ls->basicblocks[num_new_blocks]->nr++;
					num_new_blocks++;
				}
			}
		}
	}
}

void transform_BB(jitdata *jd, graphdata *gd) {
	int i, len;
	int pred, succ;
	basicblock *last_block;
	instruction *ip;
	graphiterator iter;
	methodinfo *m;
	lsradata *ls;

	m = jd->m;
	ls = jd->ls;

	/* the "real" last Block is always an empty block        */
	/* so take the one before, to insert new blocks after it */
	last_block = &(m->basicblocks[m->basicblockcount - 1]);
	_GRAPH_ASSERT(last_block->next->next == NULL);
	_GRAPH_ASSERT(last_block->next->flags <= BBREACHED);
	last_block->next->nr = ls->basicblockcount;

	/* look through new blocks */
	for(i = m->basicblockcount + 1; i < ls->basicblockcount ; i++) {
		/* if a phi move happens at this block, we need this block */
		/* if not, remove him from the CFG */
		if (ls->num_phi_moves[i] > 0) {
			/* i can only have one predecessor and one successor! */
			_GRAPH_ASSERT( graph_has_multiple_predecessors(gd,i) == false);
			_GRAPH_ASSERT( graph_has_multiple_successors(gd,i) == false);
			
			succ = graph_get_first_successor(gd, i, &iter);
			pred = graph_get_first_predecessor(gd, i, &iter);

			/* set ip to last instruction			      */
			len = ls->basicblocks[pred]->icount;
			ip = ls->basicblocks[pred]->iinstr + len - 1;
			while ((len>0) && (ip->opc == ICMD_NOP)) {
				len--;
				ip--;
			}

				
			/* with JSR there can not be multiple successors  */
			_GRAPH_ASSERT(ip->opc != ICMD_JSR);
			/* If the return Statment has more successors and  */
			/* one of these has more predecessor, we are in    */
			/* troubles - one would have to insert a new Block */
			/* after the one which executes the ICMD_JSR       */
			/* !!TODO!! if subroutines will not be inlined     */
			_GRAPH_ASSERT(ip->opc != ICMD_RET);

			/* link new block into basicblocks list */
			/* if edge to split is the "fallthrough" path of the */
			/* conditional, then link the new block inbetween    */
			/* and generate no ICMD */
			/* else if edge to split is the branch, generate a   */
			/* ICMD_GOTO and add new BB at and of BB List        */
			if ((ls->basicblocks[pred]->next == ls->basicblocks[succ]) 
				&& (ip->opc != ICMD_LOOKUPSWITCH)
				&& (ip->opc != ICMD_TABLESWITCH)
				&& (ip->opc != ICMD_GOTO)) {
				/* GOTO, *SWITCH have no fallthrough path */

				/* link into fallthrough path */
				
						
				ls->basicblocks[i]->next =
					ls->basicblocks[pred]->next;
				ls->basicblocks[pred]->next =
					ls->basicblocks[i];
				/* generate no instructions */
				ls->basicblocks[i]->icount = 1; 
				ls->basicblocks[i]->iinstr = NEW(instruction);
				ls->basicblocks[i]->iinstr[0].opc =	ICMD_NOP;
			} else {
				/* Block i is in the Branch path */
				/* link Block at the end */
				ls->basicblocks[i]->next =last_block->next;
				last_block->next = ls->basicblocks[i];
				last_block = ls->basicblocks[i];

				/* change the Branch Target to BB i */


	
				switch(ip->opc) {
				case ICMD_LOOKUPSWITCH:
					{
						s4 k, l, *s4ptr;
						void **tptr;
							
						tptr = (void **) ip->target;
						if ((basicblock*)tptr[0] == ls->basicblocks[succ]) {
							/* target found -> change*/
							tptr[0] = ls->basicblocks[i];
						}

						s4ptr = ip->val.a;
						l = s4ptr[0];                  /* default  */
						k = s4ptr[1];                  /* count    */
				
						while (--k >= 0) {
							s4ptr += 2;
							++tptr;

							if ((basicblock*)tptr[0] == ls->basicblocks[succ])
							{
								/* target found -> change */
								tptr[0] = ls->basicblocks[i];
							}
						}
					}
					break;
				case ICMD_TABLESWITCH:
					{
						s4 k, l, *s4ptr;
						void **tptr;

						tptr = (void **) ip->target;
						
						s4ptr = ip->val.a;
						l = s4ptr[1];              /* low     */
						k = s4ptr[2];              /* high    */

						k = k - l + 1;

						if ((basicblock*)tptr[0] == ls->basicblocks[succ])
						{
							/* target found -> change*/
							tptr[0] = ls->basicblocks[i];
						}
						tptr += k;
							
						while (--k >= 0) {
							if ((basicblock*)tptr[0] == ls->basicblocks[succ])
							{
								/* target found -> change*/
								tptr[0] = ls->basicblocks[i];
							}
							--tptr;
						}
					}
					break;
				case ICMD_IFNULL:
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
				case ICMD_IF_ICMPEQ:
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
				case ICMD_GOTO:
					_GRAPH_ASSERT(ip->target == ls->basicblocks[succ]);
					ip->target = ls->basicblocks[i];
					break;
				default:
					/* Exception Edge to split */
					/* not handled by now -> fallback to regalloc */
					exit(1);
				}

				/* Generate the ICMD_GOTO */
				ls->basicblocks[i]->icount = 1; 
				ls->basicblocks[i]->iinstr =
					DNEW(instruction);
				ls->basicblocks[i]->iinstr->opc =
					ICMD_GOTO;
				ls->basicblocks[i]->iinstr->target =
					ls->basicblocks[succ];
				ls->basicblocks[i]->iinstr->op1 = succ;
			}
			ls->basicblocks[i]->iinstr[0].dst = 
				ls->basicblocks[i]->outstack;
		}
	}
}

#ifdef GRAPH_DEBUG_VERBOSE
void graph_print(lsradata *ls, graphdata *gd) {
	int i,j;
	graphiterator iter;
	printf("graph_print: basicblockcount %3i\n", ls->basicblockcount);

	printf("CFG: \n");
	for(i = 0; i < ls->basicblockcount; i++) {
		printf("%3i: ",i);
		j = graph_get_first_successor(gd, i, &iter);
		for(; j != -1; j = graph_get_next(&iter)) {
			printf("%3i ",j);
		}
		printf("\n");
	}
	printf("Pred: \n");
	for(i = 0; i < ls->basicblockcount; i++) {
		printf("%3i: ",i);
		j = graph_get_first_predecessor(gd, i, &iter);
		for(; j != -1; j = graph_get_next(&iter)) {
			printf("%3i ", j);
		}
		printf("\n");
	}
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

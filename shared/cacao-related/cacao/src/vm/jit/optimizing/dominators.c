/* src/vm/jit/optimizing/dominators.c - dominators and dominance frontier

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
#include "mm/memory.h"

#include "toolbox/bitvector.h"

#include "vm/jit/optimizing/graph.h"
#include "vm/jit/optimizing/dominators.h"

/* function prototypes */
void dom_Dominators_init(dominatordata *dd, int basicblockcount);
#ifdef DOM_DEBUG_CHECK
int dom_AncestorWithLowestSemi(dominatordata *dd, int v, int basicblockcount);
void dom_Link(dominatordata *dd, int p, int n, int basicblockcount);
void dom_DFS(graphdata *gd, dominatordata *dd, int p, int n, int *N,
			 int basicblockcount);
#else
int dom_AncestorWithLowestSemi(dominatordata *dd, int v);
void dom_Link(dominatordata *dd, int p, int n);
void dom_DFS(graphdata *gd, dominatordata *dd, int p, int n, int *N);
#endif

/*************************************
Calculate Dominators
*************************************/
dominatordata *compute_Dominators(graphdata *gd, int basicblockcount) {
	int i,j,n,N,p,s,s_,v,y;
	graphiterator iter;
	dominatordata *dd;

	dd = DNEW(dominatordata);

	dom_Dominators_init(dd, basicblockcount);
	
	N=0;

	/* 1 ist the root node of the method                    */
	/* 0 is the artificial parent, where locals are set to their parameters */
	dom_DFS(gd, dd, -1, 0, &N
#ifdef DOM_DEBUG_CHECK
			,basicblockcount
#endif
			);

	for(i = N-1; i > 0; i--) {
		_DOM_CHECK_BOUNDS(i, 0, basicblockcount);
		n = dd->vertex[i];
		_DOM_CHECK_BOUNDS(n, 0, basicblockcount);
		p = dd->parent[n];
		s = p;
		j = graph_get_first_predecessor(gd, n, &iter);
		for (; j != -1; j = graph_get_next(&iter)) {
		_DOM_CHECK_BOUNDS(j, 0, basicblockcount);
			if (dd->dfnum[j] <= dd->dfnum[n])
				s_ = j;
			else
				s_ = dd->semi[dom_AncestorWithLowestSemi(dd, j
#ifdef DOM_DEBUG_CHECK
														 ,basicblockcount
#endif
														 )];
		_DOM_CHECK_BOUNDS(s_, 0, basicblockcount);
		_DOM_CHECK_BOUNDS(s, 0, basicblockcount);
			if (dd->dfnum[s_] < dd->dfnum[s])
				s = s_;
		}
		dd->semi[n] = s;
		_DOM_CHECK_BOUNDS(dd->num_bucket[s], 0, basicblockcount);
		dd->bucket[s][dd->num_bucket[s]] = n;
		dd->num_bucket[s]++;
		dom_Link(dd, p, n
#ifdef DOM_DEBUG_CHECK
				 , basicblockcount
#endif
				 );
		_DOM_CHECK_BOUNDS(p, 0, basicblockcount);
		for(j = 0; j < dd->num_bucket[p]; j++) {
		_DOM_CHECK_BOUNDS(j, 0, basicblockcount);
			v = dd->bucket[p][j];
			y = dom_AncestorWithLowestSemi(dd, v
#ifdef DOM_DEBUG_CHECK
										   , basicblockcount
#endif
										   );
		_DOM_CHECK_BOUNDS(y, 0, basicblockcount);
		_DOM_CHECK_BOUNDS(v, 0, basicblockcount);
	    if (dd->semi[y] == dd->semi[v])
				dd->idom[v] = p;
			else
				dd->samedom[v] = y;
		}
		dd->num_bucket[p] = 0;
	}
	for(i = 1; i < N; i++) {
		n = dd->vertex[i];
		_DOM_CHECK_BOUNDS(n, 0, basicblockcount);
	    if (dd->samedom[n] != -1) {
			_DOM_CHECK_BOUNDS(dd->samedom[n], 0, basicblockcount);
			dd->idom[n] = dd->idom[dd->samedom[n]];
		}
	}
	return dd;
}

/********************************************
compute Dominace Frontier
********************************************/
void computeDF(graphdata *gd, dominatordata *dd, int basicblockcount, int n) {
	int c,i,j;
	bool *_S;
	graphiterator iter;

	_S = DMNEW(bool, basicblockcount);
	for(i = 0; i < basicblockcount; i++)
		_S[i] = false;
	i = graph_get_first_successor(gd, n, &iter);
	for (; i != -1; i = graph_get_next(&iter)) {
		_DOM_CHECK_BOUNDS(i, 0, basicblockcount);
		if (dd->idom[i] != n)
			_S[i] = true;
	}
	for(c=0; c < basicblockcount; c++) {
		if (dd->idom[c] == n) {
			computeDF(gd, dd, basicblockcount, c);
			for(j=0; j < dd->num_DF[c]; j++) {
		_DOM_CHECK_BOUNDS(dd->DF[c][j], 0, basicblockcount);
	            if (n != dd->idom[dd->DF[c][j]])
					/* n does not dominate DF[c][j] -> traverse idom list? */
					_S[dd->DF[c][j]] = true;
			}
		}
	}
	for(i = 0; i < basicblockcount; i++)
		if (_S[i]) {
		_DOM_CHECK_BOUNDS(dd->num_DF[n], 0, basicblockcount);
			dd->DF[n][dd->num_DF[n]] = i;
			dd->num_DF[n]++;
		}
}


void dom_Dominators_init(dominatordata *dd, int basicblockcount) {
	int i;

	dd->dfnum  = DMNEW(int, basicblockcount);
	dd->vertex = DMNEW(int, basicblockcount);
	dd->parent = DMNEW(int, basicblockcount);
	dd->semi   = DMNEW(int, basicblockcount);
	dd->ancestor = DMNEW(int, basicblockcount);
	dd->idom     = DMNEW(int, basicblockcount);
	dd->samedom  = DMNEW(int, basicblockcount);
	dd->bucket   = DMNEW(int*, basicblockcount);
	dd->num_bucket = DMNEW(int, basicblockcount);
	dd->DF       = DMNEW(int*, basicblockcount);
	dd->num_DF   = DMNEW(int, basicblockcount);
	dd->best     = DMNEW(int, basicblockcount);
	for (i=0; i < basicblockcount; i++) {
		dd->dfnum[i] = -1;
		dd->semi[i] = dd->ancestor[i] = dd->idom[i] = dd->samedom[i] = -1;
		dd->num_bucket[i] = 0;
		dd->bucket[i] = DMNEW(int, basicblockcount);
		dd->num_DF[i] = 0;
		dd->DF[i] = DMNEW(int, basicblockcount);
	}
}

/**************************************
Create Depth First Spanning Tree
**************************************/
#ifdef DOM_DEBUG_CHECK
void dom_DFS(graphdata *gd, dominatordata *dd, int p, int n, int *N,
			 int basicblockcount) {
#else
void dom_DFS(graphdata *gd, dominatordata *dd, int p, int n, int *N) {
#endif
    int i;
	graphiterator iter;

	_DOM_CHECK_BOUNDS(n,0,basicblockcount);
	if (dd->dfnum[n] == -1) { /* not visited till now? */
		dd->dfnum[n] = *N;
		_DOM_CHECK_BOUNDS(*N,0,basicblockcount);
		dd->vertex[*N] = n;
		dd->parent[n] = p;
		(*N)++;
		i = graph_get_first_successor(gd, n, &iter);
		for (; i != -1; i = graph_get_next(&iter)) {
			dom_DFS(gd, dd, n, i, N
#ifdef DOM_DEBUG_CHECK
					, basicblockcount
#endif
					);
		}
	}
}

#ifdef DOM_DEBUG_CHECK
int dom_AncestorWithLowestSemi(dominatordata *dd, int v, int basicblockcount) {
#else
int dom_AncestorWithLowestSemi(dominatordata *dd, int v) {
#endif
	int a,b;

	_DOM_CHECK_BOUNDS(v, 0, basicblockcount);
	a = dd->ancestor[v];
	_DOM_CHECK_BOUNDS(a,0,basicblockcount);
	if (dd->ancestor[a] != -1) {
		b = dom_AncestorWithLowestSemi(dd, a
#ifdef DOM_DEBUG_CHECK
									   , basicblockcount
#endif
									   );
		dd->ancestor[v] = dd->ancestor[a];
		_DOM_CHECK_BOUNDS(b,0,basicblockcount);
		_DOM_CHECK_BOUNDS(dd->best[v],0,basicblockcount);
		_DOM_CHECK_BOUNDS(dd->semi[dd->best[v]],0,basicblockcount);
		if (dd->dfnum[dd->semi[b]] < dd->dfnum[dd->semi[dd->best[v]]])
			dd->best[v] = b;
	}
	return dd->best[v];
}

#ifdef DOM_DEBUG_CHECK
void dom_Link(dominatordata *dd, int p, int n, int basicblockcount) {
#else
void dom_Link(dominatordata *dd, int p, int n) {
#endif
	_DOM_CHECK_BOUNDS(n,0,basicblockcount);
	dd->ancestor[n] = p;
	dd->best[n] = n;
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

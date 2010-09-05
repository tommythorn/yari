/* src/vm/jit/loop/graph.c - control flow graph

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

   Contains the functions which build a list, that represents the
   control flow graph of the procedure, that is being analyzed.

   $Id: graph.c 4699 2006-03-28 14:52:32Z twisti $

*/


#include <assert.h>

#include "mm/memory.h"
#include "toolbox/logging.h"
#include "vm/jit/jit.h"
#include "vm/jit/loop/graph.h"
#include "vm/jit/loop/loop.h"


void LoopContainerInit(methodinfo *m, struct LoopContainer *lc, int i)
{
	struct LoopElement *le = DMNEW(struct LoopElement, 1);

	le->next = NULL;

	lc->parent = NULL;

	lc->tree_right = NULL;
	lc->tree_down = NULL;

	lc->exceptions = NULL;

	lc->in_degree = 0;
	le->node = lc->loop_head = i;
	le->block = &m->basicblocks[i];

	/* lc->nodes = (int *) malloc(sizeof(int)*m->basicblockcount);
	lc->nodes[0] = i; */

	lc->nodes = le;
}
	

/*
   depthFirst() builds the control flow graph out of the intermediate code of  
   the procedure, that is to be optimized and stores the list in the global 
   variable c_dTable 
*/

void depthFirst(jitdata *jd)
{
	methodinfo *m;
	loopdata   *ld;
	int i;

	/* get required compiler data */

	m  = jd->m;
	ld = jd->ld;

/*	allocate memory and init gobal variables needed by function dF(m, int, int)	*/
  
	ld->c_defnum = DMNEW(int, m->basicblockcount);
	ld->c_numPre = DMNEW(int, m->basicblockcount);
	ld->c_parent = DMNEW(int, m->basicblockcount);
	ld->c_reverse = DMNEW(int, m->basicblockcount);
	ld->c_pre = DMNEW(int *, m->basicblockcount);
	ld->c_dTable = DMNEW(struct depthElement *, m->basicblockcount);
	
	for (i = 0; i < m->basicblockcount; ++i) {
		ld->c_defnum[i] = ld->c_parent[i] = -1;
		ld->c_numPre[i] = ld->c_reverse[i] = 0;

		ld->c_pre[i] = DMNEW(int, m->basicblockcount);
		ld->c_dTable[i] = NULL;
	}
  
	ld->c_globalCount = 0;
	ld->c_allLoops = NULL;
  
	dF(m, ld, -1, 0);	/* call helper function dF that traverses basic block structure	*/
}


/*	
   dF starts from the first block of the given procedure and traverses the 
   control flow graph in a depth-first order, thereby building up the adeacency
   list c_dTable
*/ 

void dF(methodinfo *m, loopdata *ld, int from, int blockIndex)
{
	instruction *ip;
	s4 *s4ptr;
	int high, low, count;
	struct depthElement *hp;
	struct LoopContainer *tmp; 
	int cnt, *ptr;
	
	if (from >= 0) {	
/*	the current basic block has a predecessor (ie. is not the first one)		*/
		hp = DNEW(struct depthElement);/* create new depth element					*/

		hp->next = ld->c_dTable[from];	/* insert values							*/
		hp->value = blockIndex;
		hp->changes = NULL;
		
		ld->c_dTable[from] = hp;	/* insert into table							*/
	}
  
	if (from == blockIndex) {	/* insert one node loops into loop container	*/
		tmp = DNEW(struct LoopContainer);
		LoopContainerInit(m, tmp, blockIndex);
		tmp->next = ld->c_allLoops;
		ld->c_allLoops = tmp;
	}

#ifdef C_DEBUG
	if (blockIndex > m->basicblockcount) {
		log_text("DepthFirst: BlockIndex exceeded\n");
		assert(0);
	}		
#endif

	ip = m->basicblocks[blockIndex].iinstr + m->basicblocks[blockIndex].icount -1;
										/* set ip to last instruction			*/
									
	if (ld->c_defnum[blockIndex] == -1) {	/* current block has not been visited	*/
	    ld->c_defnum[blockIndex] = ld->c_globalCount;	/* update global count			*/
	    ld->c_parent[blockIndex] = from;	/* write parent block of current one	*/
		ld->c_reverse[ld->c_globalCount] = blockIndex;
		++ld->c_globalCount;
		
		if (!m->basicblocks[blockIndex].icount) {
										/* block does not contain instructions	*/
			dF(m, ld, blockIndex, blockIndex+1);
		    }
		else { 							/* for all successors, do				*/
			switch (ip->opc) {			/* check type of last instruction		*/
			case ICMD_RETURN:
			case ICMD_IRETURN:
			case ICMD_LRETURN:
			case ICMD_FRETURN:
			case ICMD_DRETURN:
			case ICMD_ARETURN:
			case ICMD_ATHROW:
				break;					/* function returns -> end of graph		*/        
				
			case ICMD_IFEQ:
			case ICMD_IFNE:
			case ICMD_IFLT:
			case ICMD_IFGE:
			case ICMD_IFGT:
			case ICMD_IFLE:
			case ICMD_IFNULL:
			case ICMD_IFNONNULL:
			case ICMD_IF_ICMPEQ:
			case ICMD_IF_ICMPNE:
			case ICMD_IF_ICMPLT:
			case ICMD_IF_ICMPGE:
			case ICMD_IF_ICMPGT:
			case ICMD_IF_ICMPLE:
			case ICMD_IF_LEQ:
			case ICMD_IF_LNE:
			case ICMD_IF_LLT:
			case ICMD_IF_LGE:
			case ICMD_IF_LGT:
			case ICMD_IF_LLE:
			case ICMD_IF_LCMPEQ:
			case ICMD_IF_LCMPNE:
			case ICMD_IF_LCMPLT:
			case ICMD_IF_LCMPGE:
			case ICMD_IF_LCMPGT:
			case ICMD_IF_LCMPLE:
			case ICMD_IF_ACMPEQ:
			case ICMD_IF_ACMPNE:				/* branch -> check next block	*/
			   dF(m, ld, blockIndex, blockIndex + 1);
			   /* fall throu */
			   
			case ICMD_GOTO:
				dF(m, ld, blockIndex, m->basicblockindex[ip->op1]);         
				break;							/* visit branch (goto) target	*/
				
			case ICMD_TABLESWITCH:				/* switch statement				*/
				s4ptr = ip->val.a;
				
				dF(m, ld, blockIndex, m->basicblockindex[*s4ptr]);	/* default branch		*/
				
				s4ptr++;
				low = *s4ptr;
				s4ptr++;
				high = *s4ptr;
				
				count = (high-low+1);
				
				while (--count >= 0) {
					s4ptr++;
					dF(m, ld, blockIndex, m->basicblockindex[*s4ptr]);
				    }
				break;
				
			case ICMD_LOOKUPSWITCH:				/* switch statement				*/
				s4ptr = ip->val.a;
			   
				dF(m, ld, blockIndex, m->basicblockindex[*s4ptr]);	/* default branch		*/
				
				++s4ptr;
				count = *s4ptr++;
				
				while (--count >= 0) {
					dF(m, ld, blockIndex, m->basicblockindex[s4ptr[1]]);
					s4ptr += 2;
				    }
				break;

			case ICMD_JSR:
				ld->c_last_jump = blockIndex;
				dF(m, ld, blockIndex, m->basicblockindex[ip->op1]);         
				break;
				
			case ICMD_RET:
				dF(m, ld, blockIndex, ld->c_last_jump+1);
				break;
				
			default:
				dF(m, ld, blockIndex, blockIndex + 1);
				break;	
			    }                         
		    }
	    } 

	for (ptr = ld->c_pre[blockIndex], cnt = 0; cnt < ld->c_numPre[blockIndex]; ++cnt, ++ptr)
	{
		if (*ptr == from)
			break;
	}

	if (cnt >= ld->c_numPre[blockIndex]) {	
		ld->c_pre[blockIndex][ld->c_numPre[blockIndex]] = from;
		                                    /* add predeccessors to list c_pre 		*/
		ld->c_numPre[blockIndex]++;				/* increase number of predecessors      */		
	}
    
}


/* 
   a slightly modified version of dF(m, ld, int, int) that is used to traverse the part 
   of the control graph that is not reached by normal program flow but by the 
   raising of exceptions (code of catch blocks)
*/

void dF_Exception(methodinfo *m, loopdata *ld, int from, int blockIndex)
{
	instruction *ip;
	s4 *s4ptr;
	int high, low, count;
	struct depthElement *hp;

	if (ld->c_exceptionVisit[blockIndex] < 0)	/* has block been visited, return	*/
		ld->c_exceptionVisit[blockIndex] = 1;
	else
		return;

	if (ld->c_dTable[blockIndex] != NULL)		/* back to regular code section 	*/
		return;

	if (from >= 0) {		/* build exception graph (in c_exceptionGraph) 	   	*/
	    hp = DNEW(struct depthElement);
		hp->next = ld->c_exceptionGraph[from];
		hp->value = blockIndex;
		hp->changes = NULL;

		ld->c_exceptionGraph[from] = hp;
	}
	
#ifdef C_DEBUG
	if (blockIndex > m->basicblockcount) {
		log_text("DepthFirst: BlockIndex exceeded");
		assert(0);
	}
#endif

	ip = m->basicblocks[blockIndex].iinstr + m->basicblocks[blockIndex].icount -1;
	
	if (!m->basicblocks[blockIndex].icount)
		dF_Exception(m, ld, blockIndex, blockIndex+1);
	else {
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
		case ICMD_IFNULL:
		case ICMD_IFNONNULL:
		case ICMD_IF_ICMPEQ:
		case ICMD_IF_ICMPNE:
		case ICMD_IF_ICMPLT:
		case ICMD_IF_ICMPGE:
		case ICMD_IF_ICMPGT:
		case ICMD_IF_ICMPLE:
		case ICMD_IF_LEQ:
		case ICMD_IF_LNE:
		case ICMD_IF_LLT:
		case ICMD_IF_LGE:
		case ICMD_IF_LGT:
		case ICMD_IF_LLE:
		case ICMD_IF_LCMPEQ:
		case ICMD_IF_LCMPNE:
		case ICMD_IF_LCMPLT:
		case ICMD_IF_LCMPGE:
		case ICMD_IF_LCMPGT:
		case ICMD_IF_LCMPLE:
		case ICMD_IF_ACMPEQ:
		case ICMD_IF_ACMPNE:                    /* branch -> check next block	*/
			dF_Exception(m, ld, blockIndex, blockIndex + 1);
			/* fall throu */
	  
		case ICMD_GOTO:
			dF_Exception(m, ld, blockIndex, m->basicblockindex[ip->op1]);         
			break;
	  
		case ICMD_TABLESWITCH:
			s4ptr = ip->val.a;
			
			/* default branch */
			dF_Exception(m, ld, blockIndex, m->basicblockindex[*s4ptr]);
			
			s4ptr++;
			low = *s4ptr;
			s4ptr++;
			high = *s4ptr;
			
			count = (high-low+1);

			while (--count >= 0) {
				s4ptr++;
				dF_Exception(m, ld, blockIndex, m->basicblockindex[*s4ptr]);
			    }
			break;

		case ICMD_LOOKUPSWITCH:
			s4ptr = ip->val.a;
 
			/* default branch */
			dF_Exception(m, ld, blockIndex, m->basicblockindex[*s4ptr]);
			
			++s4ptr;
			count = *s4ptr++;

			while (--count >= 0) {
				dF_Exception(m, ld, blockIndex, m->basicblockindex[s4ptr[1]]);
				s4ptr += 2;
			    }  
			break;

		case ICMD_JSR:
			ld->c_last_jump = blockIndex;
			dF_Exception(m, ld, blockIndex, m->basicblockindex[ip->op1]);
			break;
	
		case ICMD_RET:
			dF_Exception(m, ld, blockIndex, ld->c_last_jump+1);
			break;
			
		default:
			dF_Exception(m, ld, blockIndex, blockIndex + 1);
			break;	
		    }                         
        }
}


#if 0
/*
  Test function -> will be removed in final release
*/
void resultPass1(methodinfo *m)
{
	int i, j;
	struct depthElement *hp;

	printf("\n\n****** PASS 1 ******\n\n");
	printf("Number of Nodes: %d\n\n", ld->c_globalCount);
 
	printf("Predecessors:\n");
	for (i=0; i<m->basicblockcount; ++i) {
		printf("Block %d:\t", i);
		for (j=0; j<ld->c_numPre[i]; ++j)
			printf("%d ", ld->c_pre[i][j]);
		printf("\n");
	}
	printf("\n");

	printf("Graph:\n");
	for (i=0; i<m->basicblockcount; ++i) {
		printf("Block %d:\t", i);
		hp = ld->c_dTable[i];
		
		while (hp != NULL) {
			printf("%d ", hp->value);
			hp = hp->next;
		    }
		printf("\n");
	    }
	printf("\n");
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

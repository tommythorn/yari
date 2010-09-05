/* src/vm/jit/loop/loop.h - array bound removal header

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

   $Id: loop.h 7246 2007-01-29 18:49:05Z twisti $

*/


#ifndef _LOOP_H
#define _LOOP_H

#include "config.h"
#include "vm/types.h"

#include "vm/global.h"

#include "vm/jit/jit.h"

#include "vmcore/method.h"


/*	Different types for struct Trace										*/
#define TRACE_UNKNOWN 0			/* unknown									*/
#define TRACE_ICONST  1			/* integer constant value					*/
#define TRACE_ALENGTH 2			/* array length value						*/
#define TRACE_IVAR    3			/* integer variable reference				*/
#define TRACE_AVAR    4			/* object (array) reference					*/

/*	The ways a variable can be used in a loop								*/
#define ARRAY_INDEX	  0			/* var used as array index					*/
#define VAR_MOD		  1			/* var changes its value					*/

/*	The way, integer variables change their values							*/
#define D_UP		  0			/* var is only increased					*/
#define D_DOWN		  1			/* var is only decreased					*/
#define D_UNKNOWN	  2			/* not known								*/

/*	The different types of operators in loop conditions						*/
#define OP_EQ		  0			/* operator:	==							*/
#define OP_LT         1			/* operator:	<							*/
#define OP_GE	      2			/* operator:	>=							*/
#define OP_UNKNOWN	  3			/* operator:	unknown						*/

/*	Possible types of static tests (constraints) in	struct Constraint		*/
 
#define TEST_ZERO			0	/* check variable against const. lower bound*/
#define TEST_ALENGTH		1	/* check variable against array length		*/
#define TEST_CONST_ZERO		2	/* check constant against const. lower bound*/
#define TEST_CONST_ALENGTH	3	/* check variable against array length		*/
#define TEST_UNMOD_ZERO		4	/* check var. that is constant in loop against*/
								/* constant lower bound						*/
#define TEST_UNMOD_ALENGTH	5	/* check var. that is constant in loop against*/
								/* array length								*/
#define TEST_RS_ZERO		6	/* check constant part of loop condition against*/
								/* constant lower bound							*/
#define TEST_RS_ALENGTH		7	/* check constant part of loop condition against*/
								/* array length									*/

/*	Possible types of bound check optimizations									*/
#define OPT_UNCHECKED	0		/* access not checked yet - first visit			*/
#define OPT_NONE		1		/* no optimization								*/
#define OPT_FULL		2		/* fully remove bound check						*/
#define OPT_LOWER		3		/* remove check againt zero						*/
#define OPT_UPPER		4		/* remove check against array length			*/

/*	The different ways, remove_boundcheck(.) can be called						*/ 
#define BOUNDCHECK_REGULAR	0	/* perform regular optimization					*/
#define BOUNDCHECK_SPECIAL	1	/* only optimize header node - and ignore		*/
								/* information from loop condition				*/

#define LOOP_PART       0x1     /* a flag that marks a BB part of a loop        */
#define HANDLER_PART    0x2     /* a flag that marks a BB part of ex-handler    */
#define HANDLER_VISITED 0x4     /* flag to prevent loop if copying catch blocks */


typedef struct LoopElement LoopElement;
typedef struct LoopContainer LoopContainer;
typedef struct loopdata loopdata;


/*	This struct records information about interesting vars (vars that are modified
	or used as an array index in loops.
*/
struct LoopVar {
	int value;					/* reference to array of local variables		*/
	int modified;				/* set if value of var is changed				*/
	int index;					/* set if var is used as array index			*/
	int static_l;				/* var is never decremented -> static lower		*/
								/* bound possible								*/
	int static_u;				/* var is never incremented -> static upper		*/
								/* bound possible								*/
	int dynamic_l;
	int dynamic_l_v;			/* variable is left side of loop condition in	*/
								/* variable + dynamic_l >= right side			*/
	int dynamic_u;
	int dynamic_u_v;			/* variable is left side of loop condition in	*/
								/* variable + dynamic_u < right side			*/
	struct LoopVar *next;		/* list pointer									*/
};


/*	This struct records the needed static test of variables before loop entry.	*/
struct Constraint {
	int type;					/* type of test to perform						*/
	int arrayRef;				/* array reference involved in test (if any)	*/
	int varRef;					/* which variable to test (if involved)			*/
	int constant;				/* which constant to test (if involved)			*/
	struct Constraint *next;	/* list pointer									*/
};


/* This structure is used to record variables that change their value in loops.	*/
struct Changes {
	int var;					/* variable involved							*/
	int lower_bound;			/* a minimum lower bound that is guaranteed		*/
	int upper_bound;			/* a maximum upper bound that is guaranteed		*/
								/* IMPORTANT: if lower_bound > upper_bound		*/
								/* there are no	guarantees at all				*/
};


/*	This struct is used to build the control flow graph and stores the variable	
	changes at the beginning of each basic block.
*/
struct depthElement {
	int value;					/* number of successor of this block			*/
	struct depthElement *next;	/* list pointer									*/
	struct Changes **changes;	/* pointer to array of variable changes			*/
};


/*	Used to build a list of all basicblock, the loop consists of              */

struct LoopElement {
	s4           node;
	basicblock  *block;
	LoopElement *next;
};


/*
   This structure stores informations about a single loop
*/
struct LoopContainer {
	s4              toOpt;              /* does this loop need optimization   */
	LoopElement    *nodes;              /* list of BBs this loop consists of  */
	s4              loop_head;
	s4              in_degree;          /* needed to topological sort loops to*/
	                                    /* get the order of optimizing them   */
	LoopContainer  *next;               /* list pointer                       */
	LoopContainer  *parent;             /* points to parent loop, if this BB  */
										/* is head of a loop                  */
	LoopContainer  *tree_right;         /* used for tree hierarchie of loops  */
	LoopContainer  *tree_down;
	exception_entry *exceptions;        /* list of exception in that loop     */
};


struct loopdata {
	/* modified by graph.c															*/

	int *c_defnum;					/* array that stores a number for each node	when*/
	    							/* control flow graph is traveres depth first	*/
	int *c_parent;					/* for each node that array stores its parent	*/
	int *c_reverse;					/* for each def number that array stores the	*/
    								/* corresponding node							*/
	int c_globalCount;				/* counter for def numbering					*/
	int *c_numPre;					/* array that stores for each node its number	*/
    								/* predecessors									*/
	int **c_pre;					/* array of array that stores predecessors		*/
	int c_last_jump;				/* stores the source node of the last jsr instr	*/
	struct basicblock *c_last_target;      /* stores the source BB of the last jsr instr	*/

	struct depthElement **c_dTable;	/* adjacency list for control flow graph		*/
	struct depthElement **c_exceptionGraph;	/* adjacency list for exception graph	*/

	struct LoopContainer *c_allLoops;		/* list of all loops					*/
	struct LoopContainer *c_loop_root;		/* root of loop hierarchie tree			*/

	int *c_exceptionVisit;			/* array that stores a flag for each node part	*/
	    							/* of the exception graph						*/

	/* modified by loop.c															*/

	int *c_semi_dom;				/* store for each node its semi dominator		*/
	int *c_idom;					/* store for each node its dominator			*/
	int *c_same_dom;				/* temp array to hold nodes with same dominator	*/
	int *c_ancestor;				/* store for each node its ancestor with lowest	*/
    								/* semi dominator								*/
	int *c_numBucket;				
	int **c_bucket;
	
	int *c_contains;				/* store for each node whether it's part of loop*/
	int *c_stack;					/* a simple stack as array						*/
	int c_stackPointer;				/* stackpointer									*/


	/* modified by analyze.c														*/

	struct LoopContainer *root;     /* the root pointer for the hierarchie tree of  */
                                    /* all loops in that procedure                  */

	int c_needed_instr;				/* number of instructions that have to be		*/
	    							/* inserted before loop header to make sure		*/
		    						/* array optimization is legal					*/
	int c_rs_needed_instr;			/* number of instructions needed to load the	*/
    								/* value ofthe right side of the loop condition	*/
	int *c_nestedLoops;				/* store for each node the header node of the	*/
    								/* loop this node belongs to, -1 for none		*/
	int *c_hierarchie;              /* store a loop hierarchie                      */
	int *c_toVisit;					/* set for each node that is part of the loop	*/

	int *c_current_loop;			/* for each node:                               */
    	 							/* store 0:	node is not part of loop			*/
    								/* store 1:	node is loop header					*/
    								/* store 2:	node is in loop but not part of any	*/
    								/*			nested loop                         */
    								/* store 3:	node is part of nested loop			*/

	int c_current_head;				/* store number of node that is header of loop	*/
	int *c_var_modified;			/* store for each local variable whether its	*/
    								/* value is changed in the loop					*/

	struct Trace *c_rightside;		/* right side of loop condition					*/
	struct Constraint **c_constraints;
    								/* array that stores for each variable a list	*/
    								/* static tests (constraints) that have to be	*/
    								/* performed before loop entry					*/
    								/* IMPORTANT: c_constraints[maxlocals] stores	*/
    								/*			  the tests for constants and the	*/
    								/*			  right side of loop condition		*/
	
	struct LoopVar *c_loopvars;		/* a list of all intersting variables of the	*/
    								/* current loop (variables that are modified or	*/
    								/* used as array index							*/

	struct basicblock *c_first_block_copied; /* pointer to the first block, that is copied */
                                    /* during loop duplication                    */

	struct basicblock *c_last_block_copied;  /* last block, that is copied during loop     */
                                    /* duplication                                */

	int *c_null_check;              /* array to store for local vars, whether they  */
                                    /* need to be checked against the null reference*/
                                    /* in the loop head                             */

	bool c_needs_redirection;       /* if a loop header is inserted as first block  */
                                    /* into the global BB list, this is set to true */
                                 
	struct basicblock *c_newstart;         /* if a loop header is inserted as first block  */
                                    /* into the gloal BB list, this pointer is the  */
                                    /* new start                                    */
	int c_old_xtablelength;         /* used to store the original tablelength       */

	/* set debug mode																*/
#define C_DEBUG


	/* declare statistic variables													*/
#ifdef ENABLE_STATISTICS

	int c_stat_num_loops;			/* number of loops								*/

	/* statistics per loop															*/
	int c_stat_array_accesses;		/* number of array accesses						*/

	int c_stat_full_opt;			/* number of fully optimized accesses			*/
	int c_stat_no_opt;				/* number of not optimized accesses				*/
	int c_stat_lower_opt;			/* number of accesses where check against zero	*/
    								/* is removed									*/
	int c_stat_upper_opt;			/* number of accesses where check against array	*/
    								/* lengh is removed								*/
	int c_stat_or;					/* set if optimization is cancelled because of	*/
    								/* or in loop condition							*/
	int c_stat_exception;			/* set if optimization is cancelled because of	*/
    								/* index var modified in catch block			*/

	/* statistics per procedure														*/
	int c_stat_sum_accesses;		/* number of array accesses						*/

	int c_stat_sum_full;			/* number of fully optimized accesses			*/
	int c_stat_sum_no;				/* number of not optimized accesses				*/
	int c_stat_sum_lower;			/* number of accesses where check against zero	*/
    								/* is removed									*/
	int c_stat_sum_upper;			/* number of accesses where check against array	*/
    								/* lengh is removed								*/
	int c_stat_sum_or;				/* set if optimization is cancelled because of	*/
    								/* or in loop condition							*/
	int c_stat_sum_exception;		/* set if optimization is cancelled because of	*/

#endif
};


/* function prototypes ********************************************************/

void analyseGraph(jitdata *jd);
void c_mem_error(void);

#endif /* _LOOP_H */


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

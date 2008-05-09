/* src/vmcore/statistics.h - exports global varables for statistics

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

   $Id: statistics.h 7551 2007-03-21 14:26:09Z twisti $

*/


#ifndef _STATISTICS_H
#define _STATISTICS_H

#include "config.h"
#include "vm/types.h"

#include "vm/global.h"


/* statistic macros ***********************************************************/

#if defined(ENABLE_STATISTICS)
#define STATISTICS(x) \
    do { \
        if (opt_stat) { \
            x; \
        } \
    } while (0)
#else
#define STATISTICS(x)    /* nothing */
#endif

/* in_  inline statistics */

#define IN_MAX                  9
#define IN_UNIQUEVIRT           0x0000 
#define IN_UNIQUE_INTERFACE     0x0001
#define IN_OUTSIDERS            0x0004
#define IN_MAXDEPTH             0x0008
#define IN_MAXCODE              0x0010
#define IN_JCODELENGTH          0x0020
#define IN_EXCEPTION            0x0040
#define IN_NOT_UNIQUE_VIRT      0x0080
#define IN_NOT_UNIQUE_INTERFACE 0x0100

#define N_UNIQUEVIRT            0
#define N_UNIQUE_INTERFACE      1
#define N_OUTSIDERS             2
#define N_MAXDEPTH		3	
#define N_MAXCODE               4 
#define N_JCODELENGTH           5 
#define N_EXCEPTION            6 
#define N_NOT_UNIQUE_VIRT       7 
#define N_NOT_UNIQUE_INTERFACE  8 


/* global variables ***********************************************************/

extern s4 codememusage;
extern s4 maxcodememusage;

extern s4 memoryusage;
extern s4 maxmemusage;

extern s4 maxdumpsize;

extern s4 globalallocateddumpsize;
extern s4 globaluseddumpsize;


/* variables for measurements *************************************************/

extern s4 size_classinfo;
extern s4 size_fieldinfo;
extern s4 size_methodinfo;
extern s4 size_lineinfo;
extern s4 size_codeinfo;

extern s4 size_stack_map;
extern s4 size_string;

extern s4 size_threadobject;

extern s4 size_lock_record_pool;
extern s4 size_lock_hashtable;
extern s4 size_lock_waiter;

extern int count_const_pool_len;
extern int count_classref_len;
extern int count_parsed_desc_len;
extern int count_vftbl_len;
extern int count_all_methods;
extern int count_methods_marked_used;  /*RTA*/
extern int count_vmcode_len;
extern int count_extable_len;
extern int count_class_loads;
extern int count_class_inits;

extern int count_utf_len;               /* size of utf hash                   */
extern int count_utf_new;
extern int count_utf_new_found;

extern int count_locals_conflicts;
extern int count_locals_spilled;
extern int count_locals_register;
extern int count_ss_spilled;
extern int count_ss_register;
extern int count_methods_allocated_by_lsra;
extern int count_mem_move_bb;
extern int count_interface_size;
extern int count_argument_mem_ss;
extern int count_argument_reg_ss;
extern int count_method_in_register;
extern int count_mov_reg_reg;
extern int count_mov_mem_reg;
extern int count_mov_reg_mem;
extern int count_mov_mem_mem;

extern int count_jit_calls;
extern int count_methods;
extern int count_spills;
extern int count_spills_read;
extern int count_pcmd_activ;
extern int count_pcmd_drop;
extern int count_pcmd_zero;
extern int count_pcmd_const_store;
extern int count_pcmd_const_alu;
extern int count_pcmd_const_bra;
extern int count_pcmd_load;
extern int count_pcmd_move;
extern int count_load_instruction;
extern int count_pcmd_store;
extern int count_pcmd_store_comb;
extern int count_dup_instruction;
extern int count_pcmd_op;
extern int count_pcmd_mem;
extern int count_pcmd_met;
extern int count_pcmd_bra;
extern int count_pcmd_table;
extern int count_pcmd_return;
extern int count_pcmd_returnx;
extern int count_check_null;
extern int count_check_bound;
extern int count_max_basic_blocks;
extern int count_basic_blocks;
extern int count_max_javainstr;
extern int count_javainstr;
extern int count_javacodesize;
extern int count_javaexcsize;
extern int count_calls;
extern int count_tryblocks;
extern int count_code_len;
extern int count_data_len;
extern int count_cstub_len;
extern int count_nstub_len;
extern int count_max_new_stack;
extern int count_upper_bound_new_stack;

extern s4 count_branches_resolved;
extern s4 count_branches_unresolved;

extern int *count_block_stack;
extern int *count_analyse_iterations;
extern int *count_method_bb_distribution;
extern int *count_block_size_distribution;
extern int *count_store_length;
extern int *count_store_depth;
                                /* in_  inline statistics */
extern int count_in;
extern int count_in_uniqVirt;
extern int count_in_uniqIntf;
extern int count_in_rejected;
extern int count_in_rejected_mult;
extern int count_in_outsiders;
extern int count_in_uniqueVirt_not_inlined;
extern int count_in_uniqueInterface_not_inlined;
extern int count_in_maxDepth;
extern int count_in_maxMethods;

extern u2 count_in_not   [512];

/* instruction scheduler statistics *******************************************/

extern s4 count_schedule_basic_blocks;
extern s4 count_schedule_nodes;
extern s4 count_schedule_leaders;
extern s4 count_schedule_max_leaders;
extern s4 count_schedule_critical_path;


/* function prototypes ********************************************************/

s8 getcputime(void);

void loadingtime_start(void);
void loadingtime_stop(void);
void compilingtime_start(void);
void compilingtime_stop(void);

void print_times(void);
void print_stats(void);

void statistics_print_memory_usage(void);
void statistics_print_gc_memory_usage(void);

void mem_usagelog(bool givewarnings);

void nativeinvokation(void);
void compiledinvokation(void);
void jnicallXmethodnvokation(void);
void jniinvokation(void);

#endif /* _STATISTICS_H */


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

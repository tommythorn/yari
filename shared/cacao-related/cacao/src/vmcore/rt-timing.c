/* src/vmcore/rt-timing.c - POSIX real-time timing utilities

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

   $Id$

*/


#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "vm/global.h"

#include "vmcore/rt-timing.h"


struct rt_timing_stat {
	int index;
	int totalindex;
	const char *name;
};

static struct rt_timing_stat rt_timing_stat_defs[] = {
    { RT_TIMING_JIT_CHECKS      ,RT_TIMING_JIT_TOTAL , "checks at beginning" },
    { RT_TIMING_JIT_PARSE       ,RT_TIMING_JIT_TOTAL , "parse" },
    { RT_TIMING_JIT_STACK       ,RT_TIMING_JIT_TOTAL , "analyse_stack" },
    { RT_TIMING_JIT_TYPECHECK   ,RT_TIMING_JIT_TOTAL , "typecheck" },
    { RT_TIMING_JIT_LOOP        ,RT_TIMING_JIT_TOTAL , "loop" },
    { RT_TIMING_JIT_IFCONV      ,RT_TIMING_JIT_TOTAL , "if conversion" },
    { RT_TIMING_JIT_ALLOC       ,RT_TIMING_JIT_TOTAL , "register allocation" },
    { RT_TIMING_JIT_RPLPOINTS   ,RT_TIMING_JIT_TOTAL , "replacement point generation" },
    { RT_TIMING_JIT_CODEGEN     ,RT_TIMING_JIT_TOTAL , "codegen" },
    { RT_TIMING_JIT_TOTAL       ,-1                  , "total compile time" },
    { -1                        ,-1                  , "" },

    { RT_TIMING_LINK_RESOLVE    ,RT_TIMING_LINK_TOTAL, "link: resolve superclass/superinterfaces"},
    { RT_TIMING_LINK_C_VFTBL    ,RT_TIMING_LINK_TOTAL, "link: compute vftbl length"},
    { RT_TIMING_LINK_ABSTRACT   ,RT_TIMING_LINK_TOTAL, "link: handle abstract methods"},
    { RT_TIMING_LINK_C_IFTBL    ,RT_TIMING_LINK_TOTAL, "link: compute interface table"},
    { RT_TIMING_LINK_F_VFTBL    ,RT_TIMING_LINK_TOTAL, "link: fill vftbl"},
    { RT_TIMING_LINK_OFFSETS    ,RT_TIMING_LINK_TOTAL, "link: set offsets"},
    { RT_TIMING_LINK_F_IFTBL    ,RT_TIMING_LINK_TOTAL, "link: fill interface table"},
    { RT_TIMING_LINK_FINALIZER  ,RT_TIMING_LINK_TOTAL, "link: set finalizer"},
    { RT_TIMING_LINK_EXCEPTS    ,RT_TIMING_LINK_TOTAL, "link: resolve exception classes"},
    { RT_TIMING_LINK_SUBCLASS   ,RT_TIMING_LINK_TOTAL, "link: re-calculate subclass indices"},
    { RT_TIMING_LINK_TOTAL      ,-1                  , "total link time" },
    { -1                        ,-1                  , "" },
	
	{ RT_TIMING_LOAD_CHECKS     ,RT_TIMING_LOAD_TOTAL, "load: initial checks"},
	{ RT_TIMING_LOAD_NDPOOL     ,RT_TIMING_LOAD_TOTAL, "load: new descriptor pool"},
	{ RT_TIMING_LOAD_CPOOL      ,RT_TIMING_LOAD_TOTAL, "load: load constant pool"},
	{ RT_TIMING_LOAD_SETUP      ,RT_TIMING_LOAD_TOTAL, "load: class setup"},
	{ RT_TIMING_LOAD_FIELDS     ,RT_TIMING_LOAD_TOTAL, "load: load fields"},
	{ RT_TIMING_LOAD_METHODS    ,RT_TIMING_LOAD_TOTAL, "load: load methods"},
	{ RT_TIMING_LOAD_CLASSREFS  ,RT_TIMING_LOAD_TOTAL, "load: create classrefs"},
	{ RT_TIMING_LOAD_DESCS      ,RT_TIMING_LOAD_TOTAL, "load: allocate descriptors"},
	{ RT_TIMING_LOAD_SETREFS    ,RT_TIMING_LOAD_TOTAL, "load: set classrefs"},
	{ RT_TIMING_LOAD_PARSEFDS   ,RT_TIMING_LOAD_TOTAL, "load: parse field descriptors"},
	{ RT_TIMING_LOAD_PARSEMDS   ,RT_TIMING_LOAD_TOTAL, "load: parse method descriptors"},
	{ RT_TIMING_LOAD_PARSECP    ,RT_TIMING_LOAD_TOTAL, "load: parse descriptors in constant pool"},
	{ RT_TIMING_LOAD_VERIFY     ,RT_TIMING_LOAD_TOTAL, "load: verifier checks"},
	{ RT_TIMING_LOAD_ATTRS      ,RT_TIMING_LOAD_TOTAL, "load: load attributes"},
	{ RT_TIMING_LOAD_TOTAL      ,-1                  , "total load time (from classbuffer)"},
    { -1                        ,-1                  , "" },

	{ RT_TIMING_LOAD_BOOT_LOOKUP,-1                       , "boot: lookup in classcache"},
	{ RT_TIMING_LOAD_BOOT_ARRAY ,RT_TIMING_LOAD_BOOT_TOTAL, "boot: load array classes"},
	{ RT_TIMING_LOAD_BOOT_SUCK  ,RT_TIMING_LOAD_BOOT_TOTAL, "boot: suck class files"},
	{ RT_TIMING_LOAD_BOOT_LOAD  ,RT_TIMING_LOAD_BOOT_TOTAL, "boot: load from class buffer"},
	{ RT_TIMING_LOAD_BOOT_CACHE ,RT_TIMING_LOAD_BOOT_TOTAL, "boot: store in classcache"},
	{ RT_TIMING_LOAD_BOOT_TOTAL ,-1                       , "total bootstrap loader time"},
    { -1                        ,-1                       , "" },

	{ RT_TIMING_LOAD_CL_LOOKUP  ,-1                       , "classloader: lookup in classcache" },
	{ RT_TIMING_LOAD_CL_PREPARE ,-1                       , "classloader: prepare loader call" },
	{ RT_TIMING_LOAD_CL_JAVA    ,-1                       , "classloader: loader Java code" },
	{ RT_TIMING_LOAD_CL_CACHE   ,-1                       , "classloader: store in classcache" },
    { -1                        ,-1                       , "" },

	{ RT_TIMING_NEW_OBJECT      ,-1                       , "builtin_new time" },
	{ RT_TIMING_NEW_ARRAY       ,-1                       , "builtin_newarray time" },
    { -1                        ,-1                       , "" },

    { 0                         ,-1                       , NULL }
};

static long long rt_timing_sum[RT_TIMING_N] = { 0 };

void rt_timing_gettime(struct timespec *ts)
{
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID,ts) != 0) {
		fprintf(stderr,"could not get time by clock_gettime: %s\n",strerror(errno));
		abort();
	}
}

long rt_timing_diff_usec(struct timespec *a,struct timespec *b)
{
	long diff;
	time_t atime;

	diff = (b->tv_nsec - a->tv_nsec) / 1000;
	atime = a->tv_sec;
	while (atime < b->tv_sec) {
		atime++;
		diff += 1000000;
	}
	return diff;
}

void rt_timing_time_diff(struct timespec *a,struct timespec *b,int index)
{
	long diff;

	diff = rt_timing_diff_usec(a,b);
	rt_timing_sum[index] += diff;
}

void rt_timing_print_time_stats(FILE *file)
{
	struct rt_timing_stat *stats;
	double total;

	for (stats = rt_timing_stat_defs; stats->name; ++stats) {
		if (stats->index < 0) {
			fprintf(file,"%s\n",stats->name);
			continue;
		}
		
		if (stats->totalindex >= 0) {
			total = rt_timing_sum[stats->totalindex];
			fprintf(file,"%12lld usec %3.0f%% %s\n",
					rt_timing_sum[stats->index],
					(total != 0.0) ? rt_timing_sum[stats->index] / total * 100.0 : 0.0,
					stats->name);
		}
		else {
			fprintf(file,"%12lld usec      %s\n",
					rt_timing_sum[stats->index],
					stats->name);
		}
	}
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

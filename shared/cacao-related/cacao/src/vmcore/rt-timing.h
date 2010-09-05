/* src/vmcore/rt-timing.h - POSIX real-time timing utilities

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


#ifndef _RT_TIMING_H
#define _RT_TIMING_H

#include "config.h"

#if defined(ENABLE_RT_TIMING)

#include <time.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "vm/global.h"


#define RT_TIMING_GET_TIME(ts) \
	rt_timing_gettime(&(ts));

#define RT_TIMING_TIME_DIFF(a,b,index) \
	rt_timing_time_diff(&(a),&(b),(index));

#define RT_TIMING_JIT_CHECKS       0
#define RT_TIMING_JIT_PARSE        1
#define RT_TIMING_JIT_STACK        2
#define RT_TIMING_JIT_TYPECHECK    3
#define RT_TIMING_JIT_LOOP         4
#define RT_TIMING_JIT_IFCONV       5
#define RT_TIMING_JIT_ALLOC        6
#define RT_TIMING_JIT_RPLPOINTS    7
#define RT_TIMING_JIT_CODEGEN      8
#define RT_TIMING_JIT_TOTAL        9

#define RT_TIMING_LINK_RESOLVE     10
#define RT_TIMING_LINK_C_VFTBL     11
#define RT_TIMING_LINK_ABSTRACT    12
#define RT_TIMING_LINK_C_IFTBL     13
#define RT_TIMING_LINK_F_VFTBL     14
#define RT_TIMING_LINK_OFFSETS     15
#define RT_TIMING_LINK_F_IFTBL     16
#define RT_TIMING_LINK_FINALIZER   17
#define RT_TIMING_LINK_EXCEPTS     18
#define RT_TIMING_LINK_SUBCLASS    19
#define RT_TIMING_LINK_TOTAL       20

#define RT_TIMING_LOAD_CHECKS      21
#define RT_TIMING_LOAD_NDPOOL      22
#define RT_TIMING_LOAD_CPOOL       23
#define RT_TIMING_LOAD_SETUP       24
#define RT_TIMING_LOAD_FIELDS      25
#define RT_TIMING_LOAD_METHODS     26
#define RT_TIMING_LOAD_CLASSREFS   27
#define RT_TIMING_LOAD_DESCS       28
#define RT_TIMING_LOAD_SETREFS     29
#define RT_TIMING_LOAD_PARSEFDS    30
#define RT_TIMING_LOAD_PARSEMDS    31
#define RT_TIMING_LOAD_PARSECP     32
#define RT_TIMING_LOAD_VERIFY      33
#define RT_TIMING_LOAD_ATTRS       34
#define RT_TIMING_LOAD_TOTAL       35

#define RT_TIMING_LOAD_BOOT_LOOKUP 36
#define RT_TIMING_LOAD_BOOT_ARRAY  37
#define RT_TIMING_LOAD_BOOT_SUCK   38
#define RT_TIMING_LOAD_BOOT_LOAD   39
#define RT_TIMING_LOAD_BOOT_CACHE  40
#define RT_TIMING_LOAD_BOOT_TOTAL  41

#define RT_TIMING_LOAD_CL_LOOKUP   42
#define RT_TIMING_LOAD_CL_PREPARE  43
#define RT_TIMING_LOAD_CL_JAVA     44
#define RT_TIMING_LOAD_CL_CACHE    45

#define RT_TIMING_NEW_OBJECT       46
#define RT_TIMING_NEW_ARRAY        47

#define RT_TIMING_N                48

void rt_timing_gettime(struct timespec *ts);

void rt_timing_time_diff(struct timespec *a,struct timespec *b,int index);

long rt_timing_diff_usec(struct timespec *a,struct timespec *b);

void rt_timing_print_time_stats(FILE *file);

#else /* !defined(ENABLE_RT_TIMING) */

#define RT_TIMING_GET_TIME(ts)
#define RT_TIMING_TIME_DIFF(a,b,index)

#endif /* defined(ENABLE_RT_TIMING) */

#endif /* _RT_TIMING_H */

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

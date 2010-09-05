/* src/vm/builtintable.inc - tables of builtin functions

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

   $Id: builtintable.inc 7745 2007-04-17 20:59:52Z twisti $

*/


#include "config.h"

#include "arch.h"

#if defined(ENABLE_THREADS)
# include "threads/native/lock.h"
#endif

#include "vm/builtin.h"
#include "vm/jit/jit.h"


/* internal and not automatically replaced functions **************************/

static builtintable_entry builtintable_internal[] = {

#if defined(__ALPHA__) || defined(DISABLE_GC)
	{
		ICMD_F2L,
		BUILTIN_f2l,
		NULL,
		"f2l",
		"(F)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_D2L,
		BUILTIN_d2l,
		NULL,
		"d2l",
		"(D)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_F2I,
		BUILTIN_f2i,
		NULL,
		"f2i",
		"(F)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_D2I,
		BUILTIN_d2i,
		NULL,
		"d2i",
		"(D)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !SUPPORT_DIVISION || defined(DISABLE_GC)
	{
		ICMD_IDIV,
		BUILTIN_idiv,
		NULL,
		"idiv",
		"(II)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_IREM,
		BUILTIN_irem,
		NULL,
		"irem",
		"(II)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !(SUPPORT_DIVISION && SUPPORT_LONG && SUPPORT_LONG_DIV) || defined(DISABLE_GC)
	{
		ICMD_LDIV,
		BUILTIN_ldiv,
		NULL,
		"ldiv",
		"(JJ)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_LREM,
		BUILTIN_lrem,
		NULL,
		"lrem",
		"(JJ)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

	{
		ICMD_FREM,
		BUILTIN_frem,
		NULL,
		"frem",
		"(FF)F",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_DREM,
		BUILTIN_drem,
		NULL,
		"drem",
		"(DD)D",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},


	/* internal functions *****************************************************/

	{
		ICMD_NEW,
		BUILTIN_new,
		NULL,
		"new",
		"(Ljava/lang/Class;)Ljava/lang/Object;",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_ANEWARRAY,
		BUILTIN_newarray,
		NULL,
		"newarray",
        "(ILjava/lang/Class;)[Ljava/lang/Object;",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_NEWARRAY,
		BUILTIN_newarray_boolean,
		NULL,
		"newarray_boolean",
		"(I)[Z",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_NEWARRAY,
		BUILTIN_newarray_byte,
		NULL,
		"newarray_byte",
		"(I)[B",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_NEWARRAY,
		BUILTIN_newarray_char,
		NULL,
		"newarray_char",
		"(I)[C",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_NEWARRAY,
		BUILTIN_newarray_short,
		NULL,
		"newarray_short",
		"(I)[S",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_NEWARRAY,
		BUILTIN_newarray_int,
		NULL,
		"newarray_int",
		"(I)[I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_NEWARRAY,
		BUILTIN_newarray_long,
		NULL,
		"newarray_long",
		"(I)[J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_NEWARRAY,
		BUILTIN_newarray_float,
		NULL,
		"newarray_float",
		"(I)[F",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_NEWARRAY,
		BUILTIN_newarray_double,
		NULL,
		"newarray_double",
		"(I)[D",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_CHECKCAST,
		BUILTIN_arraycheckcast,
		NULL,
		"arraycheckcast",
		"([Ljava/lang/Object;[Ljava/lang/Object;)[Ljava/lang/Object;",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_INSTANCEOF,
		BUILTIN_arrayinstanceof,
		NULL,
		"arrayinstanceof",
		"([Ljava/lang/Object;[Ljava/lang/Object;)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_AASTORE,
		BUILTIN_canstore,
		NULL,
		"canstore",
		"([Ljava/lang/Object;Ljava/lang/Object;)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},

#if defined(ENABLE_THREADS)
	{
		ICMD_MONITORENTER,
		LOCK_monitor_enter,
		NULL,
		"monitorenter",
		"(Ljava/lang/Object;)V",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_MONITOREXIT,
		LOCK_monitor_exit,
		NULL,
		"monitorexit",
		"(Ljava/lang/Object;)V",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif


	/* stop entry */

	{
		255,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
};


/* automatically replaced functions *******************************************/

static builtintable_entry builtintable_automatic[] = {

#if !(SUPPORT_LONG && SUPPORT_LONG_CMP)
	{
		ICMD_LCMP,
		BUILTIN_lcmp,
		NULL,
		"lcmp",
		"(JJ)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !(SUPPORT_LONG && SUPPORT_LONG_LOGICAL)
	{
		ICMD_LAND,
		BUILTIN_land,
		NULL,
		"land",
		"(JJ)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_LOR,
		BUILTIN_lor,
		NULL,
		"lor",
		"(JJ)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_LXOR,
		BUILTIN_lxor,
		NULL,
		"lxor",
		"(JJ)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif /* !(SUPPORT_LONG && SUPPORT_LONG_LOGICAL) */

#if !(SUPPORT_LONG && SUPPORT_LONG_SHIFT)
	{
		ICMD_LSHL,
		BUILTIN_lshl,
		NULL,
		"lshl",
		"(JI)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_LSHR,
		BUILTIN_lshr,
		NULL,
		"lshr",
		"(JI)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_LUSHR,
		BUILTIN_lushr,
		NULL,
		"lushr",
		"(JI)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif /* !(SUPPORT_LONG && SUPPORT_LONG_SHIFT) */

#if !(SUPPORT_LONG && SUPPORT_LONG_ADD)
	{
		ICMD_LADD,
		BUILTIN_ladd,
		NULL,
		"ladd",
		"(JJ)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_LSUB,
		BUILTIN_lsub,
		NULL,
		"lsub",
		"(JJ)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_LNEG,
		BUILTIN_lneg,
		NULL,
		"lneg",
		"(J)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif /* !(SUPPORT_LONG && SUPPORT_LONG_ADD) */

#if !(SUPPORT_LONG && SUPPORT_LONG_MUL)
	{
		ICMD_LMUL,
		BUILTIN_lmul,
		NULL,
		"lmul",
		"(JJ)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !(SUPPORT_FLOAT && SUPPORT_I2F)
	{
		ICMD_I2F,
		BUILTIN_i2f,
		NULL,
		"i2f",
		"(I)F",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !(SUPPORT_DOUBLE && SUPPORT_I2D)
	{
		ICMD_I2D,
		BUILTIN_i2d,
		NULL,
		"i2d",
		"(I)D",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !(SUPPORT_LONG && SUPPORT_FLOAT && SUPPORT_L2F)
	{
		ICMD_L2F,
		BUILTIN_l2f,
		NULL,
		"l2f",
		"(J)F",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !(SUPPORT_LONG && SUPPORT_DOUBLE && SUPPORT_L2D)
	{
		ICMD_L2D,
		BUILTIN_l2d,
		NULL,
		"l2d",
		"(J)D",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !(SUPPORT_FLOAT && SUPPORT_F2I)
	{
		ICMD_F2I,
		BUILTIN_f2i,
		NULL,
		"f2i",
		"(F)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !(SUPPORT_FLOAT && SUPPORT_LONG && SUPPORT_F2L)
	{
		ICMD_F2L,
		BUILTIN_f2l,
		NULL,
		"f2l",
		"(F)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !(SUPPORT_DOUBLE && SUPPORT_D2I)
	{
		ICMD_D2I,
		BUILTIN_d2i,
		NULL,
		"d2i",
		"(D)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif

#if !(SUPPORT_DOUBLE && SUPPORT_LONG && SUPPORT_D2L)
	{
		ICMD_D2L,
		BUILTIN_d2l,
		NULL,
		"d2l",
		"(D)J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif


	/* float functions ********************************************************/

#if !SUPPORT_FLOAT
	{
		ICMD_FADD,
		BUILTIN_fadd,
		NULL,
		"fadd",
		"(FF)F",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_FSUB,
		BUILTIN_fsub,
		NULL,
		"fsub",
		"(FF)F",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_FMUL,
		BUILTIN_fmul,
		NULL,
		"fmul",
		"(FF)F",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_FDIV,
		BUILTIN_fdiv,
		NULL,
		"fdiv",
		"(FF)F",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_FNEG,
		BUILTIN_fneg,
		NULL,
		"fneg",
		"(F)F",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif /* !SUPPORT_FLOAT */

#if !SUPPORT_FLOAT || !SUPPORT_FLOAT_CMP
	{
		ICMD_FCMPL,
		BUILTIN_fcmpl,
		NULL,
		"fcmpl",
		"(FF)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_FCMPG,
		BUILTIN_fcmpg,
		NULL,
		"fcmpg",
		"(FF)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif /* !SUPPORT_FLOAT || !SUPPORT_FLOAT_CMP */


	/* double functions *******************************************************/

#if !SUPPORT_DOUBLE
	{
		ICMD_DADD,
		BUILTIN_dadd,
		NULL,
		"dadd",
		"(DD)D",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_DSUB,
		BUILTIN_dsub,
		NULL,
		"dsub",
		"(DD)D",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_DMUL,
		BUILTIN_dmul,
		NULL,
		"dmul",
		"(DD)D",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_DDIV,
		BUILTIN_ddiv,
		NULL,
		"ddiv",
		"(DD)D",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_DNEG,
		BUILTIN_dneg,
		NULL,
		"dneg",
		"(D)D",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif /* !SUPPORT_DOUBLE */

#if !SUPPORT_DOUBLE || !SUPPORT_DOUBLE_CMP
	{
		ICMD_DCMPL,
		BUILTIN_dcmpl,
		NULL,
		"dcmpl",
		"(DD)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_DCMPG,
		BUILTIN_dcmpg,
		NULL,
		"dcmpg",
		"(DD)I",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif /* !SUPPORT_DOUBLE || !SUPPORT_DOUBLE_CMP */


	/* float/double functions *************************************************/

#if !(SUPPORT_FLOAT && SUPPORT_DOUBLE)
	{
		ICMD_F2D,
		BUILTIN_f2d,
		NULL,
		"f2d",
		"(F)D",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
	{
		ICMD_D2F,
		BUILTIN_d2f,
		NULL,
		"d2f",
		"(D)F",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
#endif /* !(SUPPORT_FLOAT && SUPPORT_DOUBLE) */


	/* prevent a compiler warning if everything is supported (e.g. i386) */

	{
		255,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
};


/* automatically replaced functions *******************************************/

static builtintable_entry builtintable_function[] = {
#if defined(ENABLE_JIT)

	/* java.lang.VMSystem.arraycopy(Ljava/lang/Object;ILjava/lang/Object;II)V STATIC NATIVE */

#if defined(__MIPS__) && (SIZEOF_VOID_P != 4)
	/* We can't use this optimization on MIPS32, since it has only 4
	   argument registers and we use 8-byte stack slots.  The code
	   generated does not access the correct 5th argument (hint: *8
	   instead of *4). */

	{
		ICMD_BUILTIN,
		BUILTIN_arraycopy,
		"java/lang/VMSystem",
		"arraycopy",
		"(Ljava/lang/Object;ILjava/lang/Object;II)V",
		NULL,
		NULL,
		NULL,
		true,
		NULL
	},
#endif

	/* java.lang.System.arraycopy(Ljava/lang/Object;ILjava/lang/Object;II)V PUBLIC STATIC */

#if 0
	/* We disable this one until we have a fix for the stacktrace
	   issue. */

	{
		ICMD_BUILTIN,
		BUILTIN_arraycopy,
		"java/lang/System",
		"arraycopy",
		"(Ljava/lang/Object;ILjava/lang/Object;II)V",
		NULL,
		NULL,
		NULL,
		true,
		NULL
	},
#endif

	/* java.lang.VMSystem.currentTimeMillis()J PUBLIC STATIC */

	{
		ICMD_BUILTIN,
		BUILTIN_currenttimemillis,
		"java/lang/VMSystem",
		"currentTimeMillis",
		"()J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},

	/* java.lang.System.currentTimeMillis()J PUBLIC STATIC */

	{
		ICMD_BUILTIN,
		BUILTIN_currenttimemillis,
		"java/lang/System",
		"currentTimeMillis",
		"()J",
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},

#endif /* defined(ENABLE_JIT) */

	/* stop entry */

	{
		255,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		false,
		NULL
	},
};


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

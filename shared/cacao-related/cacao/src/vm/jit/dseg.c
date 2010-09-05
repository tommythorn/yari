/* src/vm/jit/dseg.c - data segment handling stuff

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

   Contact: cacao@cacaojvm.org

   Authors: Reinhard Grafl
            Andreas  Krall
            Christian Thalinger
            Joseph Wenninger
            Edwin Steiner

   $Id: dseg.c 7596 2007-03-28 21:05:53Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "mm/memory.h"
#include "vm/jit/codegen-common.h"
#include "vm/jit/methodheader.h"
#include "vmcore/options.h"


/* dseg_finish *****************************************************************

   Fills the data segment with the values stored.

*******************************************************************************/

void dseg_finish(jitdata *jd)
{
	codeinfo    *code;
	codegendata *cd;
	dsegentry   *de;

	/* get required compiler data */

	code = jd->code;
	cd   = jd->cd;

	/* process all data segment entries */

	for (de = cd->dseg; de != NULL; de = de->next) {
		switch (de->type) {
		case TYPE_INT:
			*((s4 *)     (code->entrypoint + de->disp)) = de->val.i;
			break;

		case TYPE_LNG:
			*((s8 *)     (code->entrypoint + de->disp)) = de->val.l;
			break;

		case TYPE_FLT:
			*((float *)  (code->entrypoint + de->disp)) = de->val.f;
			break;

		case TYPE_DBL:
			*((double *) (code->entrypoint + de->disp)) = de->val.d;
			break;

		case TYPE_ADR:
			*((void **)  (code->entrypoint + de->disp)) = de->val.a;
			break;
		}
	}
}


static s4 dseg_find_s4(codegendata *cd, s4 value)
{
	dsegentry *de;

	/* search all data segment entries for a matching entry */

	for (de = cd->dseg; de != NULL; de = de->next) {
		if (IS_INT_TYPE(de->type))
			if (de->flags & DSEG_FLAG_READONLY)
				if (de->val.i == value)
					return de->disp;
	}

	/* no matching entry was found */

	return 0;
}


static s4 dseg_find_s8(codegendata *cd, s8 value)
{
	dsegentry *de;

	/* search all data segment entries for a matching entry */

	for (de = cd->dseg; de != NULL; de = de->next) {
		if (IS_LNG_TYPE(de->type))
			if (de->flags & DSEG_FLAG_READONLY)
				if (de->val.l == value)
					return de->disp;
	}

	/* no matching entry was found */

	return 0;
}


static s4 dseg_find_float(codegendata *cd, float value)
{
	dsegentry *de;
	imm_union  val;

	/* we compare the hex value of the float as 0.0 == -0.0 */

	val.f = value;

	/* search all data segment entries for a matching entry */

	for (de = cd->dseg; de != NULL; de = de->next) {
		if (IS_FLT_TYPE(de->type))
			if (de->flags & DSEG_FLAG_READONLY)
				if (de->val.i == val.i)
					return de->disp;
	}

	/* no matching entry was found */

	return 0;
}


static s4 dseg_find_double(codegendata *cd, double value)
{
	dsegentry *de;
	imm_union  val;

	/* we compare the hex value of the double as 0.0 == -0.0 */

	val.d = value;

	/* search all data segment entries for a matching entry */

	for (de = cd->dseg; de != NULL; de = de->next) {
		if (IS_DBL_TYPE(de->type))
			if (de->flags & DSEG_FLAG_READONLY)
				if (de->val.l == val.l)
					return de->disp;
	}

	/* no matching entry was found */

	return 0;
}


static s4 dseg_find_address(codegendata *cd, void *value)
{
	dsegentry *de;

	/* search all data segment entries for a matching entry */

	for (de = cd->dseg; de != NULL; de = de->next) {
		if (IS_ADR_TYPE(de->type))
			if (de->flags & DSEG_FLAG_READONLY)
				if (de->val.a == value)
					return de->disp;
	}

	/* no matching entry was found */

	return 0;
}


/* dseg_add_s4_intern **********************************************************

   Internal function to add an s4 value to the data segment.

*******************************************************************************/

static s4 dseg_add_s4_intern(codegendata *cd, s4 value, u4 flags)
{
	dsegentry *de;

	/* Increase data segment size, which is also the displacement into
	   the data segment. */

	cd->dseglen += 4;

	/* allocate new entry */

	de = DNEW(dsegentry);

	de->type  = TYPE_INT;
	de->flags = flags;
	de->disp  = -(cd->dseglen);
	de->val.i = value;
	de->next  = cd->dseg;

	/* insert into the chain */

	cd->dseg = de;

	return de->disp;
}


/* dseg_add_unique_s4 **********************************************************

   Adds uniquely an s4 value to the data segment.

*******************************************************************************/

s4 dseg_add_unique_s4(codegendata *cd, s4 value)
{
	s4 disp;

	disp = dseg_add_s4_intern(cd, value, DSEG_FLAG_UNIQUE);

	return disp;
}


/* dseg_add_s4 *****************************************************************

   Adds an s4 value to the data segment. It tries to reuse previously
   added values.

*******************************************************************************/

s4 dseg_add_s4(codegendata *cd, s4 value)
{
	s4 disp;

	/* search the data segment if the value is already stored */

	disp = dseg_find_s4(cd, value);

	if (disp != 0)
		return disp;
		
	disp = dseg_add_s4_intern(cd, value, DSEG_FLAG_READONLY);

	return disp;
}


/* dseg_add_s8_intern **********************************************************

   Internal function to add an s8 value to the data segment.

*******************************************************************************/

static s4 dseg_add_s8_intern(codegendata *cd, s8 value, u4 flags)
{
	dsegentry *de;

	/* Increase data segment size, which is also the displacement into
	   the data segment. */

	cd->dseglen = MEMORY_ALIGN(cd->dseglen + 8, 8);

	/* allocate new entry */

	de = DNEW(dsegentry);

	de->type  = TYPE_LNG;
	de->flags = flags;
	de->disp  = -(cd->dseglen);
	de->val.l = value;
	de->next  = cd->dseg;

	/* insert into the chain */

	cd->dseg = de;

	return de->disp;
}


/* dseg_add_unique_s8 **********************************************************

   Adds uniquely an s8 value to the data segment.

*******************************************************************************/

s4 dseg_add_unique_s8(codegendata *cd, s8 value)
{
	s4 disp;

	disp = dseg_add_s8_intern(cd, value, DSEG_FLAG_UNIQUE);

	return disp;
}


/* dseg_add_s8 *****************************************************************

   Adds an s8 value to the data segment. It tries to reuse previously
   added values.

*******************************************************************************/

s4 dseg_add_s8(codegendata *cd, s8 value)
{
	s4 disp;

	/* search the data segment if the value is already stored */

	disp = dseg_find_s8(cd, value);

	if (disp != 0)
		return disp;
		
	disp = dseg_add_s8_intern(cd, value, DSEG_FLAG_READONLY);

	return disp;
}


/* dseg_add_float_intern *******************************************************

   Internal function to add a float value to the data segment.

*******************************************************************************/

static s4 dseg_add_float_intern(codegendata *cd, float value, u4 flags)
{
	dsegentry *de;
		
	/* Increase data segment size, which is also the displacement into
	   the data segment. */

	cd->dseglen += 4;

	/* allocate new entry */

	de = DNEW(dsegentry);

	de->type  = TYPE_FLT;
	de->flags = flags;
	de->disp  = -(cd->dseglen);
	de->val.f = value;
	de->next  = cd->dseg;

	/* insert into the chain */

	cd->dseg = de;

	return de->disp;
}


/* dseg_add_unique_float *******************************************************

   Adds uniquely an float value to the data segment.

*******************************************************************************/

s4 dseg_add_unique_float(codegendata *cd, float value)
{
	s4 disp;

	disp = dseg_add_float_intern(cd, value, DSEG_FLAG_UNIQUE);

	return disp;
}


/* dseg_add_float **************************************************************

   Adds an float value to the data segment. It tries to reuse
   previously added values.

*******************************************************************************/

s4 dseg_add_float(codegendata *cd, float value)
{
	s4 disp;

	/* search the data segment if the value is already stored */

	disp = dseg_find_float(cd, value);

	if (disp != 0)
		return disp;
		
	disp = dseg_add_float_intern(cd, value, DSEG_FLAG_READONLY);

	return disp;
}


/* dseg_add_double_intern ******************************************************

   Internal function to add a double value to the data segment.

*******************************************************************************/

static s4 dseg_add_double_intern(codegendata *cd, double value, u4 flags)
{
	dsegentry *de;
		
	/* Increase data segment size, which is also the displacement into
	   the data segment. */

	cd->dseglen = MEMORY_ALIGN(cd->dseglen + 8, 8);

	/* allocate new entry */

	de = DNEW(dsegentry);

	de->type  = TYPE_DBL;
	de->flags = flags;
	de->disp  = -(cd->dseglen);
	de->val.d = value;
	de->next  = cd->dseg;

	/* insert into the chain */

	cd->dseg = de;

	return de->disp;
}


/* dseg_add_unique_double ******************************************************

   Adds uniquely a double value to the data segment.

*******************************************************************************/

s4 dseg_add_unique_double(codegendata *cd, double value)
{
	s4 disp;

	disp = dseg_add_double_intern(cd, value, DSEG_FLAG_UNIQUE);

	return disp;
}


/* dseg_add_double *************************************************************

   Adds a double value to the data segment. It tries to reuse
   previously added values.

*******************************************************************************/

s4 dseg_add_double(codegendata *cd, double value)
{
	s4 disp;

	/* search the data segment if the value is already stored */

	disp = dseg_find_double(cd, value);

	if (disp != 0)
		return disp;
		
	disp = dseg_add_double_intern(cd, value, DSEG_FLAG_READONLY);

	return disp;
}


/* dseg_add_address_intern *****************************************************

   Internal function to add an address pointer to the data segment.

*******************************************************************************/

static s4 dseg_add_address_intern(codegendata *cd, void *value, u4 flags)
{
	dsegentry *de;

	/* Increase data segment size, which is also the displacement into
	   the data segment. */

#if SIZEOF_VOID_P == 8
	cd->dseglen = MEMORY_ALIGN(cd->dseglen + 8, 8);
#else
	cd->dseglen += 4;
#endif

	/* allocate new entry */

	de = DNEW(dsegentry);

	de->type  = TYPE_ADR;
	de->flags = flags;
	de->disp  = -(cd->dseglen);
	de->val.a = value;
	de->next  = cd->dseg;

	/* insert into the chain */

	cd->dseg = de;

	return de->disp;
}


/* dseg_add_unique_address *****************************************************

   Adds uniquely an address value to the data segment.

*******************************************************************************/

s4 dseg_add_unique_address(codegendata *cd, void *value)
{
	s4 disp;

	disp = dseg_add_address_intern(cd, value, DSEG_FLAG_UNIQUE);

	return disp;
}


/* dseg_add_address ************************************************************

   Adds an address value to the data segment. It tries to reuse
   previously added values.

*******************************************************************************/

s4 dseg_add_address(codegendata *cd, void *value)
{
	s4 disp;

	/* search the data segment if the value is already stored */

	disp = dseg_find_address(cd, value);

	if (disp != 0)
		return disp;
		
	disp = dseg_add_address_intern(cd, value, DSEG_FLAG_READONLY);

	return disp;
}


/* dseg_add_target *************************************************************

   XXX

*******************************************************************************/

void dseg_add_target(codegendata *cd, basicblock *target)
{
	jumpref *jr;

	jr = DNEW(jumpref);

	jr->tablepos = dseg_add_unique_address(cd, NULL);
	jr->target   = target;
	jr->next     = cd->jumpreferences;

	cd->jumpreferences = jr;
}


/* dseg_addlinenumbertablesize *************************************************

   XXX

*******************************************************************************/

void dseg_addlinenumbertablesize(codegendata *cd)
{
#if SIZEOF_VOID_P == 8
	/* 4-byte ALIGNMENT PADDING */

	dseg_add_unique_s4(cd, 0);
#endif

	cd->linenumbertablesizepos  = dseg_add_unique_address(cd, NULL);
	cd->linenumbertablestartpos = dseg_add_unique_address(cd, NULL);

#if SIZEOF_VOID_P == 8
	/* 4-byte ALIGNMENT PADDING */

	dseg_add_unique_s4(cd, 0);
#endif
}


/* dseg_addlinenumber **********************************************************

   Add a line number reference.

   IN:
      cd.............current codegen data
      linenumber.....number of line that starts with the given mcodeptr
      mcodeptr.......start mcodeptr of line

*******************************************************************************/

void dseg_addlinenumber(codegendata *cd, u2 linenumber)
{
	linenumberref *lr;

	lr = DNEW(linenumberref);

	lr->linenumber = linenumber;
	lr->tablepos   = 0;
	lr->targetmpc  = cd->mcodeptr - cd->mcodebase;
	lr->next       = cd->linenumberreferences;

	cd->linenumberreferences = lr;
}


/* dseg_addlinenumber_inline_start *********************************************

   Add a marker to the line number table indicating the start of an inlined
   method body. (see doc/inlining_stacktrace.txt)

   IN:
      cd.............current codegen data
      iptr...........the ICMD_INLINE_BODY instruction
      mcodeptr.......start mcodeptr of inlined body

*******************************************************************************/

void dseg_addlinenumber_inline_start(codegendata *cd, instruction *iptr)
{
	linenumberref *lr;
	insinfo_inline *insinfo;
	ptrint mpc;

	lr = DNEW(linenumberref);

	lr->linenumber = (-2); /* marks start of inlined method */
	lr->tablepos   = 0;
	lr->targetmpc  = (mpc = (u1 *) cd->mcodeptr - cd->mcodebase);
	lr->next       = cd->linenumberreferences;

	cd->linenumberreferences = lr;

	insinfo = iptr->sx.s23.s3.inlineinfo;

	insinfo->startmpc = mpc; /* store for corresponding INLINE_END */
}


/* dseg_addlinenumber_inline_end ***********************************************

   Add a marker to the line number table indicating the end of an inlined
   method body. (see doc/inlining_stacktrace.txt)

   IN:
      cd.............current codegen data
      iptr...........the ICMD_INLINE_END instruction

   Note:
      iptr->method must point to the inlined callee.

*******************************************************************************/

void dseg_addlinenumber_inline_end(codegendata *cd, instruction *iptr)
{
	linenumberref *lr;
	linenumberref *prev;
	insinfo_inline *insinfo;

	insinfo = iptr->sx.s23.s3.inlineinfo;

	assert(insinfo);

	lr = DNEW(linenumberref);

	/* special entry containing the methodinfo * */
	lr->linenumber = (-3) - iptr->line;
	lr->tablepos   = 0;
	lr->targetmpc  = (ptrint) insinfo->method;
	lr->next       = cd->linenumberreferences;

	prev = lr;
	lr = DNEW(linenumberref);

	/* end marker with PC of start of body */
	lr->linenumber = (-1);
	lr->tablepos   = 0;
	lr->targetmpc  = insinfo->startmpc;
	lr->next       = prev;

	cd->linenumberreferences = lr;
}


/* dseg_createlinenumbertable **************************************************

   Creates a line number table in the data segment from the created
   entries in linenumberreferences.

*******************************************************************************/

void dseg_createlinenumbertable(codegendata *cd)
{
	linenumberref *lr;

	for (lr = cd->linenumberreferences; lr != NULL; lr = lr->next) {
		lr->tablepos = dseg_add_unique_address(cd, NULL);

		if (cd->linenumbertab == 0)
			cd->linenumbertab = lr->tablepos;

#if SIZEOF_VOID_P == 8
		/* This is for alignment and easier usage. */
		(void) dseg_add_unique_s8(cd, lr->linenumber);
#else
		(void) dseg_add_unique_s4(cd, lr->linenumber);
#endif
	}
}


/* dseg_get_linenumber_from_pc_intern ******************************************

   This function search the line number table for the line
   corresponding to a given pc. The function recurses for inlined
   methods.

*******************************************************************************/

static s4 dseg_get_linenumber_from_pc_intern(methodinfo **pm, linenumbertable_entry *lntentry, s4 lntsize, u1 *pc)
{
	linenumbertable_entry *lntinline;     /* special entry for inlined method */

	for (; lntsize > 0; lntsize--, lntentry--) {
		/* Note: In case of inlining this may actually compare the pc
		   against a methodinfo *, yielding a non-sensical
		   result. This is no problem, however, as we ignore such
		   entries in the switch below. This way we optimize for the
		   common case (ie. a real pc in lntentry->pc). */

		if (pc >= lntentry->pc) {
			/* did we reach the current line? */

			if ((s4) lntentry->line >= 0)
				return (s4) lntentry->line;

			/* we found a special inline entry (see
			   doc/inlining_stacktrace.txt for details */

			switch (lntentry->line) {
			case -1: 
				/* begin of inlined method (ie. INLINE_END
				   instruction) */

				lntinline = --lntentry;/* get entry with methodinfo * */
				lntentry--;            /* skip the special entry      */
				lntsize -= 2;

				/* search inside the inlined method */

				if (dseg_get_linenumber_from_pc_intern(pm, lntentry, lntsize,
													   pc))
				{
					/* the inlined method contained the pc */

					*pm = (methodinfo *) lntinline->pc;

					assert(lntinline->line <= -3);

					return (-3) - lntinline->line;
				}

				/* pc was not in inlined method, continue search.
				   Entries inside the inlined method will be skipped
				   because their lntentry->pc is higher than pc.  */
				break;

			case -2: 
				/* end of inlined method */

				return 0;

				/* default: is only reached for an -3-line entry after
				   a skipped -2 entry. We can safely ignore it and
				   continue searching.  */
			}
		}
	}

	/* not found, return 0 */

	return 0;
}


/* dseg_get_linenumber_from_pc *************************************************

   A wrapper for dseg_get_linenumber_from_pc_intern, so we don't have
   to evaluate the method header on every call.

*******************************************************************************/

s4 dseg_get_linenumber_from_pc(methodinfo **pm, u1 *pv, u1 *pc)
{
	ptrint                 lntsize;     /* size of line number table          */
	u1                    *lntstart;    /* start of line number table         */
	linenumbertable_entry *lntentry;    /* points to last entry in the table  */
	s4                     linenumber;

	/* get size of line number table */

	lntsize  = *((ptrint *) (pv + LineNumberTableSize));
	lntstart = *((u1 **)    (pv + LineNumberTableStart));

	/* Subtract the size of the line number entry of the structure,
	   since the line number table start points to the pc. */

	lntentry = (linenumbertable_entry *) (lntstart - SIZEOF_VOID_P);

	/* get the line number */

	linenumber = dseg_get_linenumber_from_pc_intern(pm, lntentry, lntsize, pc);

	return linenumber;
}


/* dseg_adddata ****************************************************************

   Adds a data segment reference to the codegendata.

*******************************************************************************/

#if defined(__I386__) || defined(__X86_64__) || defined(__S390__) || defined(__XDSPCORE__) || defined(__M68K__) || defined(ENABLE_INTRP)
void dseg_adddata(codegendata *cd)
{
	dataref *dr;

	dr = DNEW(dataref);

	dr->datapos = cd->mcodeptr - cd->mcodebase;
	dr->next    = cd->datareferences;

	cd->datareferences = dr;
}
#endif


/* dseg_resolve_datareferences *************************************************

   Resolve data segment references.

*******************************************************************************/

#if defined(__I386__) || defined(__X86_64__) || defined(__XDSPCORE__) || defined(__M68K__) || defined(ENABLE_INTRP)
void dseg_resolve_datareferences(jitdata *jd)
{
	codeinfo    *code;
	codegendata *cd;
	dataref     *dr;

	/* get required compiler data */

	code = jd->code;
	cd   = jd->cd;

	/* data segment references resolving */

	for (dr = cd->datareferences; dr != NULL; dr = dr->next)
		*((u1 **) (code->entrypoint + dr->datapos - SIZEOF_VOID_P)) = code->entrypoint;
}
#endif


/* dseg_display ****************************************************************

   Displays the content of the methods' data segment.

*******************************************************************************/

#if !defined(NDEBUG)
void dseg_display(jitdata *jd)
{
	codeinfo    *code;
	codegendata *cd;
	dsegentry   *de;
	imm_union   val;

	/* get required compiler data */

	code = jd->code;
	cd   = jd->cd;

	if (opt_debugcolor)
		printf("\033[34m");	/* blue */

	printf("  --- dump of datasegment\n");

	/* process all data segment entries */

	for (de = cd->dseg; de != NULL; de = de->next) {
#if SIZEOF_VOID_P == 8
		printf("0x%016lx:", (ptrint) (code->entrypoint + de->disp));
#else
		printf("0x%08x:", (ptrint) (code->entrypoint + de->disp));
#endif

		printf("    %6x (%6d): ", de->disp, de->disp);

		/* We read the values from the data segment as some values,
		   like the line number table, have been written directly to
		   the data segment. */

		switch (de->type) {
		case TYPE_INT:
			val.i = *((s4 *) (code->entrypoint + de->disp));
			printf("(INT) %d (0x%08x)", val.i, val.i);
			break;

		case TYPE_LNG:
			val.l = *((s8 *) (code->entrypoint + de->disp));
#if SIZEOF_VOID_P == 8
			printf("(LNG) %ld (0x%016lx)", val.l, val.l);
#else
			printf("(LNG) %lld (0x%016llx)", val.l, val.l);
#endif
			break;

		case TYPE_FLT:
			val.f = *((float *) (code->entrypoint + de->disp));
			printf("(FLT) %g (0x%08x)", val.f, val.i);
			break;

		case TYPE_DBL:
			val.d = *((double *) (code->entrypoint + de->disp));
#if SIZEOF_VOID_P == 8
			printf("(DBL) %g (0x%016lx)", val.d, val.l);
#else
			printf("(DBL) %g (0x%016llx)", val.d, val.l);
#endif
			break;

		case TYPE_ADR:
			val.a = *((void **) (code->entrypoint + de->disp));
#if SIZEOF_VOID_P == 8
			printf("(ADR) %016lx", (ptrint) val.a);
#else
			printf("(ADR) %08x", (ptrint) val.a);
#endif
			break;
		}

		printf("\n");
	}

	printf("  --- begin of data segment: ");
#if SIZEOF_VOID_P == 8
	printf("0x%016lx\n", (ptrint) code->entrypoint);
#else
	printf("0x%08x\n", (ptrint) code->entrypoint);
#endif

	if (opt_debugcolor)
		printf("\033[m");
}
#endif /* !defined(NDEBUG) */


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

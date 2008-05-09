/* src/vm/builtin.c - functions for unsupported operations

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

   Contains C functions for JavaVM Instructions that cannot be
   translated to machine language directly. Consequently, the
   generated machine code for these instructions contains function
   calls instead of machine instructions, using the C calling
   convention.

   $Id: builtin.c 7813M 2007-05-07 19:35:48Z (local) $

*/


#include "config.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "vm/types.h"

#include "arch.h"
#include "md-abi.h"

#include "fdlibm/fdlibm.h"
#if defined(__CYGWIN__) && defined(Bias)
# undef Bias
#endif

#include "mm/gc-common.h"
#include "mm/memory.h"

#include "native/jni.h"
#include "native/include/java_lang_String.h"
#include "native/include/java_lang_Throwable.h"

#include "threads/lock-common.h"

#include "toolbox/logging.h"
#include "toolbox/util.h"

#include "vm/builtin.h"
#include "vm/cycles-stats.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/initialize.h"
#include "vm/stringlocal.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/patcher.h"

#include "vmcore/class.h"
#include "vmcore/loader.h"
#include "vmcore/options.h"
#include "vmcore/rt-timing.h"

#if defined(ENABLE_VMLOG)
#include <vmlog_cacao.h>
#endif


/* include builtin tables *****************************************************/

#include "vm/builtintable.inc"


CYCLES_STATS_DECLARE(builtin_new         ,100,5)
CYCLES_STATS_DECLARE(builtin_overhead    , 80,1)

/* builtintable_init ***********************************************************

   Parse the descriptors of builtin functions and create the parsed
   descriptors.

*******************************************************************************/

static bool builtintable_init(void)
{
	descriptor_pool    *descpool;
	s4                  dumpsize;
	builtintable_entry *bte;

	/* mark start of dump memory area */

	dumpsize = dump_size();

	/* create a new descriptor pool */

	descpool = descriptor_pool_new(class_java_lang_Object);

	/* add some entries we need */

	if (!descriptor_pool_add_class(descpool, utf_java_lang_Object))
		return false;

	if (!descriptor_pool_add_class(descpool, utf_java_lang_Class))
		return false;

	/* first add all descriptors to the pool */

	for (bte = builtintable_internal; bte->fp != NULL; bte++) {
		/* create a utf8 string from descriptor */

		bte->descriptor = utf_new_char(bte->cdescriptor);

		if (!descriptor_pool_add(descpool, bte->descriptor, NULL)) {
			/* release dump area */

			dump_release(dumpsize);

			return false;
		}
	}

	for (bte = builtintable_automatic; bte->fp != NULL; bte++) {
		bte->descriptor = utf_new_char(bte->cdescriptor);

		if (!descriptor_pool_add(descpool, bte->descriptor, NULL)) {
			dump_release(dumpsize);
			return false;
		}
	}

	for (bte = builtintable_function; bte->fp != NULL; bte++) {
		bte->classname  = utf_new_char(bte->cclassname);
		bte->name       = utf_new_char(bte->cname);
		bte->descriptor = utf_new_char(bte->cdescriptor);

		if (!descriptor_pool_add(descpool, bte->descriptor, NULL)) {
			dump_release(dumpsize);
			return false;
		}
	}

	/* create the class reference table */

	(void) descriptor_pool_create_classrefs(descpool, NULL);

	/* allocate space for the parsed descriptors */

	descriptor_pool_alloc_parsed_descriptors(descpool);

	/* now parse all descriptors */

	for (bte = builtintable_internal; bte->fp != NULL; bte++) {
		/* parse the descriptor, builtin is always static (no `this' pointer) */

		bte->md = descriptor_pool_parse_method_descriptor(descpool,
														  bte->descriptor,
														  ACC_STATIC, NULL);
	}

	for (bte = builtintable_automatic; bte->fp != NULL; bte++) {
		bte->md = descriptor_pool_parse_method_descriptor(descpool,
														  bte->descriptor,
														  ACC_STATIC, NULL);
	}

	for (bte = builtintable_function; bte->fp != NULL; bte++) {
		bte->md = descriptor_pool_parse_method_descriptor(descpool,
														  bte->descriptor,
														  ACC_STATIC, NULL);
	}

	/* release dump area */

	dump_release(dumpsize);

	return true;
}


/* builtintable_comparator *****************************************************

   qsort comparator for the automatic builtin table.

*******************************************************************************/

static int builtintable_comparator(const void *a, const void *b)
{
	builtintable_entry *bte1;
	builtintable_entry *bte2;

	bte1 = (builtintable_entry *) a;
	bte2 = (builtintable_entry *) b;

	return (bte1->opcode < bte2->opcode) ? -1 : (bte1->opcode > bte2->opcode);
}


/* builtintable_sort_automatic *************************************************

   Sorts the automatic builtin table.

*******************************************************************************/

static void builtintable_sort_automatic(void)
{
	s4 entries;

	/* calculate table size statically (`- 1' comment see builtintable.inc) */

	entries = sizeof(builtintable_automatic) / sizeof(builtintable_entry) - 1;

	qsort(builtintable_automatic, entries, sizeof(builtintable_entry),
		  builtintable_comparator);
}


/* builtin_init ****************************************************************

   Initialize the global table of builtin functions.

*******************************************************************************/

bool builtin_init(void)
{
	/* initialize the builtin tables */

	if (!builtintable_init())
		return false;

	/* sort builtin tables */

	builtintable_sort_automatic();

	return true;
}


/* builtintable_get_internal ***************************************************

   Finds an entry in the builtintable for internal functions and
   returns the a pointer to the structure.

*******************************************************************************/

builtintable_entry *builtintable_get_internal(functionptr fp)
{
	builtintable_entry *bte;

	for (bte = builtintable_internal; bte->fp != NULL; bte++) {
		if (bte->fp == fp)
			return bte;
	}

	return NULL;
}


/* builtintable_get_automatic **************************************************

   Finds an entry in the builtintable for functions which are replaced
   automatically and returns the a pointer to the structure.

*******************************************************************************/

builtintable_entry *builtintable_get_automatic(s4 opcode)
{
	builtintable_entry *first;
	builtintable_entry *last;
	builtintable_entry *middle;
	s4                  half;
	s4                  entries;

	/* calculate table size statically (`- 1' comment see builtintable.inc) */

	entries = sizeof(builtintable_automatic) / sizeof(builtintable_entry) - 1;

	first = builtintable_automatic;
	last = builtintable_automatic + entries;

	while (entries > 0) {
		half = entries / 2;
		middle = first + half;

		if (middle->opcode < opcode) {
			first = middle + 1;
			entries -= half + 1;
		} else
			entries = half;
	}

	return (first != last ? first : NULL);
}


/* builtintable_replace_function ***********************************************

   XXX

*******************************************************************************/

#if defined(ENABLE_JIT)
bool builtintable_replace_function(void *iptr_)
{
	constant_FMIref    *mr;
	builtintable_entry *bte;
	instruction        *iptr;

	iptr = (instruction *) iptr_; /* twisti will kill me ;) */

	/* get name and descriptor of the function */

	switch (iptr->opc) {
	case ICMD_INVOKESTATIC:
		/* The instruction MUST be resolved, otherwise we run into
		   lazy loading troubles.  Anyway, we should/can only replace
		   very VM-close functions. */

		if (INSTRUCTION_IS_UNRESOLVED(iptr))
			return false;

		mr = iptr->sx.s23.s3.fmiref;
		break;	

	default:
		return false;
	}

	/* search the function table */

	for (bte = builtintable_function; bte->fp != NULL; bte++) {
		if ((METHODREF_CLASSNAME(mr) == bte->classname) &&
			(mr->name             == bte->name) &&
			(mr->descriptor       == bte->descriptor)) {

			/* set the values in the instruction */

			iptr->opc   = bte->opcode;
			iptr->sx.s23.s3.bte = bte;
			if (bte->checkexception)
				iptr->flags.bits |= INS_FLAG_CHECK;
			else
				iptr->flags.bits &= ~INS_FLAG_CHECK;

			return true;
		}
	}

	return false;
}
#endif /* defined(ENABLE_JIT) */


/*****************************************************************************
								TYPE CHECKS
*****************************************************************************/

/* builtin_instanceof **********************************************************

   Checks if an object is an instance of some given class (or subclass
   of that class). If class is an interface, checks if the interface
   is implemented.

   Return value: 1 ... o is an instance of class or implements the interface
                 0 ... otherwise or if o == NULL
			 
*******************************************************************************/

s4 builtin_instanceof(java_objectheader *o, classinfo *class)
{
	if (o == NULL)
		return 0;

	return class_isanysubclass(o->vftbl->class, class);
}



/* builtin_checkcast ***********************************************************

   The same as builtin_instanceof except that 1 is returned when o ==
   NULL.
			  
*******************************************************************************/

s4 builtin_checkcast(java_objectheader *o, classinfo *class)
{
	if (o == NULL)
		return 1;

	if (class_isanysubclass(o->vftbl->class, class))
		return 1;

	return 0;
}


/* builtin_descriptorscompatible ***********************************************

   Checks if two array type descriptors are assignment compatible

   Return value: 1 ... target = desc is possible
                 0 ... otherwise
			
*******************************************************************************/

static s4 builtin_descriptorscompatible(arraydescriptor *desc,
										arraydescriptor *target)
{
	if (desc == target)
		return 1;

	if (desc->arraytype != target->arraytype)
		return 0;

	if (desc->arraytype != ARRAYTYPE_OBJECT)
		return 1;
	
	/* {both arrays are arrays of references} */

	if (desc->dimension == target->dimension) {
		/* an array which contains elements of interface types is
           allowed to be casted to Object (JOWENN)*/

		if ((desc->elementvftbl->baseval < 0) &&
			(target->elementvftbl->baseval == 1))
			return 1;

		return class_isanysubclass(desc->elementvftbl->class,
								   target->elementvftbl->class);
	}

	if (desc->dimension < target->dimension)
		return 0;

	/* {desc has higher dimension than target} */

	return class_isanysubclass(pseudo_class_Arraystub,
							   target->elementvftbl->class);
}


/* builtin_arraycheckcast ******************************************************

   Checks if an object is really a subtype of the requested array
   type.  The object has to be an array to begin with. For simple
   arrays (int, short, double, etc.) the types have to match exactly.
   For arrays of objects, the type of elements in the array has to be
   a subtype (or the same type) of the requested element type. For
   arrays of arrays (which in turn can again be arrays of arrays), the
   types at the lowest level have to satisfy the corresponding sub
   class relation.
	
*******************************************************************************/

s4 builtin_arraycheckcast(java_objectheader *o, classinfo *targetclass)
{
	arraydescriptor *desc;

	if (o == NULL)
		return 1;

	desc = o->vftbl->arraydesc;

	if (desc == NULL)
		return 0;
 
	return builtin_descriptorscompatible(desc, targetclass->vftbl->arraydesc);
}


s4 builtin_arrayinstanceof(java_objectheader *o, classinfo *targetclass)
{
	if (o == NULL)
		return 0;

	return builtin_arraycheckcast(o, targetclass);
}


/* builtin_throw_exception *****************************************************

   Sets the exceptionptr with the thrown exception and prints some
   debugging information.  Called from asm_vm_call_method.

*******************************************************************************/

void *builtin_throw_exception(java_objectheader *xptr)
{
#if !defined(NDEBUG)
    java_lang_Throwable *t;
	char                *logtext;
	s4                   logtextlen;
	s4                   dumpsize;

	if (opt_verbose) {
		t = (java_lang_Throwable *) xptr;

		/* calculate message length */

		logtextlen = strlen("Builtin exception thrown: ") + strlen("0");

		if (t) {
			logtextlen +=
				utf_bytes(xptr->vftbl->class->name);
			if (t->detailMessage) {
				logtextlen += strlen(": ") +
					u2_utflength(t->detailMessage->value->data 
									+ t->detailMessage->offset,
						     	 t->detailMessage->count);
			}
		} 
		else {
			logtextlen += strlen("(nil)");
		}

		/* allocate memory */

		dumpsize = dump_size();

		logtext = DMNEW(char, logtextlen);

		strcpy(logtext, "Builtin exception thrown: ");

		if (t) {
			utf_cat_classname(logtext, xptr->vftbl->class->name);

			if (t->detailMessage) {
				char *buf;

				buf = javastring_tochar((java_objectheader *) t->detailMessage);
				strcat(logtext, ": ");
				strcat(logtext, buf);
				MFREE(buf, char, strlen(buf) + 1);
			}

		} else {
			strcat(logtext, "(nil)");
		}

		log_text(logtext);

		/* release memory */

		dump_release(dumpsize);
	}
#endif /* !defined(NDEBUG) */

	/* actually set the exception */

	exceptions_set_exception(xptr);

	/* Return a NULL pointer.  This is required for vm_call_method to
	   check for an exception.  This is for convenience. */

	return NULL;
}


/* builtin_canstore ************************************************************

   Checks, if an object can be stored in an array.

   Return value: 1 ... possible
                 0 ... otherwise (throws an ArrayStoreException)

*******************************************************************************/

s4 builtin_canstore(java_objectarray *oa, java_objectheader *o)
{
	arraydescriptor *desc;
	arraydescriptor *valuedesc;
	vftbl_t         *componentvftbl;
	vftbl_t         *valuevftbl;
	s4               base;
	castinfo         classvalues;
	s4               result;

	if (o == NULL)
		return 1;

	/* The following is guaranteed (by verifier checks):
	 *
	 *     *) oa->...vftbl->arraydesc != NULL
	 *     *) oa->...vftbl->arraydesc->componentvftbl != NULL
	 *     *) o->vftbl is not an interface vftbl
	 */
	
	desc           = oa->header.objheader.vftbl->arraydesc;
	componentvftbl = desc->componentvftbl;
	valuevftbl     = o->vftbl;
	valuedesc      = valuevftbl->arraydesc;

	if ((desc->dimension - 1) == 0) {
		/* {oa is a one-dimensional array} */
		/* {oa is an array of references} */
		
		if (valuevftbl == componentvftbl)
			return 1;

		ASM_GETCLASSVALUES_ATOMIC(componentvftbl, valuevftbl, &classvalues);

		base = classvalues.super_baseval;

		if (base <= 0) {
			/* an array of interface references */

			result = ((valuevftbl->interfacetablelength > -base) &&
					(valuevftbl->interfacetable[base] != NULL));
		}
		else {
			result = ((unsigned) (classvalues.sub_baseval - classvalues.super_baseval)
				   <= (unsigned) classvalues.super_diffval);
		}
	}
	else if (valuedesc == NULL) {
		/* {oa has dimension > 1} */
		/* {componentvftbl->arraydesc != NULL} */

		/* check if o is an array */

		return 0;
	}
	else {
		/* {o is an array} */

		result = builtin_descriptorscompatible(valuedesc, componentvftbl->arraydesc);
	}

	/* if not possible, throw an exception */

	if (result == 0)
		exceptions_throw_arraystoreexception();

	/* return result */

	return result;
}


/* This is an optimized version where a is guaranteed to be one-dimensional */
s4 builtin_canstore_onedim (java_objectarray *a, java_objectheader *o)
{
	arraydescriptor *desc;
	vftbl_t *elementvftbl;
	vftbl_t *valuevftbl;
	s4 res;
	int base;
	castinfo classvalues;
	
	if (!o) return 1;

	/* The following is guaranteed (by verifier checks):
	 *
	 *     *) a->...vftbl->arraydesc != NULL
	 *     *) a->...vftbl->arraydesc->elementvftbl != NULL
	 *     *) a->...vftbl->arraydesc->dimension == 1
	 *     *) o->vftbl is not an interface vftbl
	 */

	desc = a->header.objheader.vftbl->arraydesc;
    elementvftbl = desc->elementvftbl;
	valuevftbl = o->vftbl;

	/* {a is a one-dimensional array} */
	
	if (valuevftbl == elementvftbl)
		return 1;

	ASM_GETCLASSVALUES_ATOMIC(elementvftbl, valuevftbl, &classvalues);

	if ((base = classvalues.super_baseval) <= 0)
		/* an array of interface references */
		return (valuevftbl->interfacetablelength > -base &&
				valuevftbl->interfacetable[base] != NULL);

	res = (unsigned) (classvalues.sub_baseval - classvalues.super_baseval)
		<= (unsigned) classvalues.super_diffval;

	return res;
}


/* This is an optimized version where a is guaranteed to be a
 * one-dimensional array of a class type */
s4 builtin_canstore_onedim_class(java_objectarray *a, java_objectheader *o)
{
	vftbl_t *elementvftbl;
	vftbl_t *valuevftbl;
	s4 res;
	castinfo classvalues;
	
	if (!o) return 1;

	/* The following is guaranteed (by verifier checks):
	 *
	 *     *) a->...vftbl->arraydesc != NULL
	 *     *) a->...vftbl->arraydesc->elementvftbl != NULL
	 *     *) a->...vftbl->arraydesc->elementvftbl is not an interface vftbl
	 *     *) a->...vftbl->arraydesc->dimension == 1
	 *     *) o->vftbl is not an interface vftbl
	 */

    elementvftbl = a->header.objheader.vftbl->arraydesc->elementvftbl;
	valuevftbl = o->vftbl;

	/* {a is a one-dimensional array} */
	
	if (valuevftbl == elementvftbl)
		return 1;

	ASM_GETCLASSVALUES_ATOMIC(elementvftbl, valuevftbl, &classvalues);

	res = (unsigned) (classvalues.sub_baseval - classvalues.super_baseval)
		<= (unsigned) classvalues.super_diffval;

	return res;
}


/* builtin_new *****************************************************************

   Creates a new instance of class c on the heap.

   Return value: pointer to the object or NULL if no memory is
   available
			
*******************************************************************************/

java_objectheader *builtin_new(classinfo *c)
{
	java_objectheader *o;
#if defined(ENABLE_RT_TIMING)
	struct timespec time_start, time_end;
#endif
#if defined(ENABLE_CYCLES_STATS)
	u8 cycles_start, cycles_end;
#endif

	RT_TIMING_GET_TIME(time_start);
	CYCLES_STATS_GET(cycles_start);

	/* is the class loaded */

	assert(c->state & CLASS_LOADED);

	/* check if we can instantiate this class */

	if (c->flags & ACC_ABSTRACT) {
		exceptions_throw_instantiationerror(c);
		return NULL;
	}

	/* is the class linked */

	if (!(c->state & CLASS_LINKED))
		if (!link_class(c))
			return NULL;

	if (!(c->state & CLASS_INITIALIZED)) {
#if !defined(NDEBUG)
		if (initverbose)
			log_message_class("Initialize class (from builtin_new): ", c);
#endif

		if (!initialize_class(c))
			return NULL;
	}

	o = heap_allocate(c->instancesize, c->flags & ACC_CLASS_HAS_POINTERS,
					  c->finalizer);

	if (!o)
		return NULL;

	o->vftbl = c->vftbl;

#if defined(ENABLE_THREADS)
	lock_init_object_lock(o);
#endif

	CYCLES_STATS_GET(cycles_end);
	RT_TIMING_GET_TIME(time_end);

	CYCLES_STATS_COUNT(builtin_new,cycles_end - cycles_start);
	RT_TIMING_TIME_DIFF(time_start, time_end, RT_TIMING_NEW_OBJECT);

	return o;
}


/* builtin_newarray ************************************************************

   Creates an array with the given vftbl on the heap. This function
   takes as class argument an array class.

   Return value: pointer to the array or NULL if no memory is available

*******************************************************************************/

java_arrayheader *builtin_newarray(s4 size, classinfo *arrayclass)
{
	arraydescriptor  *desc;
	s4                dataoffset;
	s4                componentsize;
	s4                actualsize;
	java_arrayheader *a;
#if defined(ENABLE_RT_TIMING)
	struct timespec time_start, time_end;
#endif

	RT_TIMING_GET_TIME(time_start);

	desc          = arrayclass->vftbl->arraydesc;
	dataoffset    = desc->dataoffset;
	componentsize = desc->componentsize;

	if (size < 0) {
		exceptions_throw_negativearraysizeexception();
		return NULL;
	}

	actualsize = dataoffset + size * componentsize;

	/* check for overflow */

	if (((u4) actualsize) < ((u4) size)) {
		exceptions_throw_outofmemoryerror();
		return NULL;
	}

	a = heap_allocate(actualsize, (desc->arraytype == ARRAYTYPE_OBJECT), NULL);

	if (a == NULL)
		return NULL;

	a->objheader.vftbl = arrayclass->vftbl;

#if defined(ENABLE_THREADS)
	lock_init_object_lock(&a->objheader);
#endif

	a->size = size;

	RT_TIMING_GET_TIME(time_end);
	RT_TIMING_TIME_DIFF(time_start, time_end, RT_TIMING_NEW_ARRAY);

	return a;
}


/* builtin_anewarray ***********************************************************

   Creates an array of references to the given class type on the heap.

   Return value: pointer to the array or NULL if no memory is
   available

*******************************************************************************/

java_objectarray *builtin_anewarray(s4 size, classinfo *componentclass)
{
	classinfo *arrayclass;
	
	/* is class loaded */

	assert(componentclass->state & CLASS_LOADED);

	/* is class linked */

	if (!(componentclass->state & CLASS_LINKED))
		if (!link_class(componentclass))
			return NULL;

	arrayclass = class_array_of(componentclass, true);

	if (!arrayclass)
		return NULL;

	return (java_objectarray *) builtin_newarray(size, arrayclass);
}


/* builtin_newarray_boolean ****************************************************

   Creates an array of bytes on the heap. The array is designated as
   an array of booleans (important for casts)
	
   Return value: pointer to the array or NULL if no memory is
   available

*******************************************************************************/

java_booleanarray *builtin_newarray_boolean(s4 size)
{
	return (java_booleanarray *)
		builtin_newarray(size,
						 primitivetype_table[ARRAYTYPE_BOOLEAN].arrayclass);
}


/* builtin_newarray_byte *******************************************************

   Creates an array of 8 bit Integers on the heap.

   Return value: pointer to the array or NULL if no memory is
   available

*******************************************************************************/

java_bytearray *builtin_newarray_byte(s4 size)
{
	return (java_bytearray *)
		builtin_newarray(size, primitivetype_table[ARRAYTYPE_BYTE].arrayclass);
}


/* builtin_newarray_char *******************************************************

   Creates an array of characters on the heap.

   Return value: pointer to the array or NULL if no memory is
   available

*******************************************************************************/

java_chararray *builtin_newarray_char(s4 size)
{
	return (java_chararray *)
		builtin_newarray(size, primitivetype_table[ARRAYTYPE_CHAR].arrayclass);
}


/* builtin_newarray_short ******************************************************

   Creates an array of 16 bit Integers on the heap.

   Return value: pointer to the array or NULL if no memory is
   available

*******************************************************************************/

java_shortarray *builtin_newarray_short(s4 size)
{
	return (java_shortarray *)
		builtin_newarray(size, primitivetype_table[ARRAYTYPE_SHORT].arrayclass);
}


/* builtin_newarray_int ********************************************************

   Creates an array of 32 bit Integers on the heap.

   Return value: pointer to the array or NULL if no memory is
   available

*******************************************************************************/

java_intarray *builtin_newarray_int(s4 size)
{
	return (java_intarray *)
		builtin_newarray(size, primitivetype_table[ARRAYTYPE_INT].arrayclass);
}


/* builtin_newarray_long *******************************************************

   Creates an array of 64 bit Integers on the heap.

   Return value: pointer to the array or NULL if no memory is
   available

*******************************************************************************/

java_longarray *builtin_newarray_long(s4 size)
{
	return (java_longarray *)
		builtin_newarray(size, primitivetype_table[ARRAYTYPE_LONG].arrayclass);
}


/* builtin_newarray_float ******************************************************

   Creates an array of 32 bit IEEE floats on the heap.

   Return value: pointer to the array or NULL if no memory is
   available

*******************************************************************************/

java_floatarray *builtin_newarray_float(s4 size)
{
	return (java_floatarray *)
		builtin_newarray(size, primitivetype_table[ARRAYTYPE_FLOAT].arrayclass);
}


/* builtin_newarray_double *****************************************************

   Creates an array of 64 bit IEEE floats on the heap.

   Return value: pointer to the array or NULL if no memory is
   available

*******************************************************************************/

java_doublearray *builtin_newarray_double(s4 size)
{
	return (java_doublearray *)
		builtin_newarray(size,
						 primitivetype_table[ARRAYTYPE_DOUBLE].arrayclass);
}


/* builtin_multianewarray_intern ***********************************************

   Creates a multi-dimensional array on the heap. The dimensions are
   passed in an array of longs.

   Arguments:
       n............number of dimensions to create
       arrayvftbl...vftbl of the array class
       dims.........array containing the size of each dimension to create

   Return value: pointer to the array or NULL if no memory is
   available

******************************************************************************/

static java_arrayheader *builtin_multianewarray_intern(int n,
													   classinfo *arrayclass,
													   long *dims)
{
	s4                size;
	java_arrayheader *a;
	classinfo        *componentclass;
	s4                i;

	/* create this dimension */

	size = (s4) dims[0];
  	a = builtin_newarray(size, arrayclass);

	if (!a)
		return NULL;

	/* if this is the last dimension return */

	if (!--n)
		return a;

	/* get the class of the components to create */

	componentclass = arrayclass->vftbl->arraydesc->componentvftbl->class;

	/* The verifier guarantees that the dimension count is in the range. */

	/* create the component arrays */

	for (i = 0; i < size; i++) {
		java_arrayheader *ea =
#if defined(__MIPS__) && (SIZEOF_VOID_P == 4)
			/* we save an s4 to a s8 slot, 8-byte aligned */

			builtin_multianewarray_intern(n, componentclass, dims + 2);
#else
			builtin_multianewarray_intern(n, componentclass, dims + 1);
#endif

		if (!ea)
			return NULL;
		
		((java_objectarray *) a)->data[i] = (java_objectheader *) ea;
	}

	return a;
}


/* builtin_multianewarray ******************************************************

   Wrapper for builtin_multianewarray_intern which checks all
   dimensions before we start allocating.

******************************************************************************/

java_arrayheader *builtin_multianewarray(int n, classinfo *arrayclass,
										 long *dims)
{
	s4 i;
	s4 size;

	/* check all dimensions before doing anything */

	for (i = 0; i < n; i++) {
#if defined(__MIPS__) && (SIZEOF_VOID_P == 4)
		/* we save an s4 to a s8 slot, 8-byte aligned */
		size = (s4) dims[i * 2];
#else
		size = (s4) dims[i];
#endif

		if (size < 0) {
			exceptions_throw_negativearraysizeexception();
			return NULL;
		}
	}

	/* now call the real function */

	return builtin_multianewarray_intern(n, arrayclass, dims);
}


/*****************************************************************************
					  METHOD LOGGING

	Various functions for printing a message at method entry or exit (for
	debugging)
	
*****************************************************************************/

#if !defined(NDEBUG)
static s4 methodindent = 0;
static u4 callcount = 0;

java_objectheader *builtin_trace_exception(java_objectheader *xptr,
										   methodinfo *m,
										   void *pos,
										   s4 indent)
{
	char *logtext;
	s4    logtextlen;
	s4    dumpsize;
	codeinfo *code;

#if defined(ENABLE_VMLOG)
	return xptr;
#endif

	if (opt_verbosecall && indent)
		methodindent--;

	/* calculate message length */

	if (xptr) {
		logtextlen =
			strlen("Exception ") + utf_bytes(xptr->vftbl->class->name);
	} 
	else {
		logtextlen = strlen("Some Throwable");
	}

	logtextlen += strlen(" thrown in ");

	if (m) {
		logtextlen +=
			utf_bytes(m->class->name) +
			strlen(".") +
			utf_bytes(m->name) +
			utf_bytes(m->descriptor) +
			strlen("(NOSYNC,NATIVE");

#if SIZEOF_VOID_P == 8
		logtextlen +=
			strlen(")(0x123456789abcdef0) at position 0x123456789abcdef0 (");
#else
		logtextlen += strlen(")(0x12345678) at position 0x12345678 (");
#endif

		if (m->class->sourcefile == NULL)
			logtextlen += strlen("<NO CLASSFILE INFORMATION>");
		else
			logtextlen += utf_bytes(m->class->sourcefile);

		logtextlen += strlen(":65536)");

	} 
	else {
		logtextlen += strlen("call_java_method");
	}

	logtextlen += strlen("0");

	/* allocate memory */

	dumpsize = dump_size();

	logtext = DMNEW(char, logtextlen);

	if (xptr) {
		strcpy(logtext, "Exception ");
		utf_cat_classname(logtext, xptr->vftbl->class->name);

	} else {
		strcpy(logtext, "Some Throwable");
	}

	strcat(logtext, " thrown in ");

	if (m) {
		utf_cat_classname(logtext, m->class->name);
		strcat(logtext, ".");
		utf_cat(logtext, m->name);
		utf_cat(logtext, m->descriptor);

		if (m->flags & ACC_SYNCHRONIZED)
			strcat(logtext, "(SYNC");
		else
			strcat(logtext, "(NOSYNC");

		if (m->flags & ACC_NATIVE) {
			strcat(logtext, ",NATIVE");

			code = m->code;

#if SIZEOF_VOID_P == 8
			sprintf(logtext + strlen(logtext),
					")(0x%016lx) at position 0x%016lx",
					(ptrint) code->entrypoint, (ptrint) pos);
#else
			sprintf(logtext + strlen(logtext),
					")(0x%08x) at position 0x%08x",
					(ptrint) code->entrypoint, (ptrint) pos);
#endif

		} else {

			/* XXX preliminary: This should get the actual codeinfo */
			/* in which the exception happened.                     */
			code = m->code;
			
#if SIZEOF_VOID_P == 8
			sprintf(logtext + strlen(logtext),
					")(0x%016lx) at position 0x%016lx (",
					(ptrint) code->entrypoint, (ptrint) pos);
#else
			sprintf(logtext + strlen(logtext),
					")(0x%08x) at position 0x%08x (",
					(ptrint) code->entrypoint, (ptrint) pos);
#endif

			if (m->class->sourcefile == NULL)
				strcat(logtext, "<NO CLASSFILE INFORMATION>");
			else
				utf_cat(logtext, m->class->sourcefile);

			sprintf(logtext + strlen(logtext), ":%d)", 0);
		}

	} else
		strcat(logtext, "call_java_method");

	log_text(logtext);

	/* release memory */

	dump_release(dumpsize);

	return xptr;
}
#endif /* !defined(NDEBUG) */


/* builtin_print_argument ******************************************************

   Prints arguments and return values for the call trace.

*******************************************************************************/

#if !defined(NDEBUG)
static char *builtin_print_argument(char *logtext, s4 *logtextlen,
									typedesc *paramtype, s8 value)
{
	imm_union          imu;
	java_objectheader *o;
	classinfo         *c;
	utf               *u;
	u4                 len;

	switch (paramtype->type) {
	case TYPE_INT:
		imu.i = (s4) value;
		sprintf(logtext + strlen(logtext), "%d (0x%08x)", imu.i, imu.i);
		break;

	case TYPE_LNG:
		imu.l = value;
#if SIZEOF_VOID_P == 4
		sprintf(logtext + strlen(logtext), "%lld (0x%016llx)", imu.l, imu.l);
#else
		sprintf(logtext + strlen(logtext), "%ld (0x%016lx)", imu.l, imu.l);
#endif
		break;

	case TYPE_FLT:
		imu.i = (s4) value;
		sprintf(logtext + strlen(logtext), "%g (0x%08x)", imu.f, imu.i);
		break;

	case TYPE_DBL:
		imu.l = value;
#if SIZEOF_VOID_P == 4
		sprintf(logtext + strlen(logtext), "%g (0x%016llx)", imu.d, imu.l);
#else
		sprintf(logtext + strlen(logtext), "%g (0x%016lx)", imu.d, imu.l);
#endif
		break;

	case TYPE_ADR:
#if SIZEOF_VOID_P == 4
		sprintf(logtext + strlen(logtext), "0x%08x", (ptrint) value);
#else
		sprintf(logtext + strlen(logtext), "0x%016lx", (ptrint) value);
#endif

		/* cast to java.lang.Object */

		o = (java_objectheader *) (ptrint) value;

		/* check return argument for java.lang.Class or java.lang.String */

		if (o != NULL) {
			if (o->vftbl->class == class_java_lang_String) {
				/* get java.lang.String object and the length of the
				   string */

				u = javastring_toutf(o, false);

				len = strlen(" (String = \"") + utf_bytes(u) + strlen("\")");

				/* realloc memory for string length */

				logtext = DMREALLOC(logtext, char, *logtextlen, *logtextlen + len);
				*logtextlen += len;

				/* convert to utf8 string and strcat it to the logtext */

				strcat(logtext, " (String = \"");
				utf_cat(logtext, u);
				strcat(logtext, "\")");
			}
			else {
				if (o->vftbl->class == class_java_lang_Class) {
					/* if the object returned is a java.lang.Class
					   cast it to classinfo structure and get the name
					   of the class */

					c = (classinfo *) o;

					u = c->name;
				}
				else {
					/* if the object returned is not a java.lang.String or
					   a java.lang.Class just print the name of the class */

					u = o->vftbl->class->name;
				}

				len = strlen(" (Class = \"") + utf_bytes(u) + strlen("\")");

				/* realloc memory for string length */

				logtext = DMREALLOC(logtext, char, *logtextlen, *logtextlen + len);
				*logtextlen += len;

				/* strcat to the logtext */

				strcat(logtext, " (Class = \"");
				utf_cat_classname(logtext, u);
				strcat(logtext, "\")");
			}
		}
	}

	return logtext;
}
#endif /* !defined(NDEBUG) */


/* builtin_verbosecall_enter ***************************************************

   Print method call with arguments for -verbose:call.

*******************************************************************************/

#if !defined(NDEBUG)

#ifdef TRACE_ARGS_NUM
void builtin_verbosecall_enter(s8 a0, s8 a1,
# if TRACE_ARGS_NUM >= 4
							   s8 a2, s8 a3,
# endif
# if TRACE_ARGS_NUM >= 6
							   s8 a4, s8 a5,
# endif
# if TRACE_ARGS_NUM == 8
							   s8 a6, s8 a7,
# endif
							   methodinfo *m)
{
	methoddesc *md;
	char       *logtext;
	s4          logtextlen;
	s4          dumpsize;
	s4          i;
	s4          pos;

#if defined(ENABLE_VMLOG)
	vmlog_cacao_enter_method(m);
	return;
#endif

	md = m->parseddesc;

	/* calculate message length */

	logtextlen =
		strlen("4294967295 ") +
		strlen("-2147483647-") +        /* INT_MAX should be sufficient       */
		methodindent +
		strlen("called: ") +
		utf_bytes(m->class->name) +
		strlen(".") +
		utf_bytes(m->name) +
		utf_bytes(m->descriptor);

	/* Actually it's not possible to have all flags printed, but:
	   safety first! */

	logtextlen +=
		strlen(" PUBLIC") +
		strlen(" PRIVATE") +
		strlen(" PROTECTED") +
		strlen(" STATIC") +
		strlen(" FINAL") +
		strlen(" SYNCHRONIZED") +
		strlen(" VOLATILE") +
		strlen(" TRANSIENT") +
		strlen(" NATIVE") +
		strlen(" INTERFACE") +
		strlen(" ABSTRACT");

	/* add maximal argument length */

	logtextlen +=
		strlen("(") +
		strlen("-9223372036854775808 (0x123456789abcdef0), ") * TRACE_ARGS_NUM +
		strlen("...(255)") +
		strlen(")");

	/* allocate memory */

	dumpsize = dump_size();

	logtext = DMNEW(char, logtextlen);

	callcount++;

	sprintf(logtext, "%10d ", callcount);
	sprintf(logtext + strlen(logtext), "-%d-", methodindent);

	pos = strlen(logtext);

	for (i = 0; i < methodindent; i++)
		logtext[pos++] = '\t';

	strcpy(logtext + pos, "called: ");

	utf_cat_classname(logtext, m->class->name);
	strcat(logtext, ".");
	utf_cat(logtext, m->name);
	utf_cat(logtext, m->descriptor);

	if (m->flags & ACC_PUBLIC)       strcat(logtext, " PUBLIC");
	if (m->flags & ACC_PRIVATE)      strcat(logtext, " PRIVATE");
	if (m->flags & ACC_PROTECTED)    strcat(logtext, " PROTECTED");
   	if (m->flags & ACC_STATIC)       strcat(logtext, " STATIC");
   	if (m->flags & ACC_FINAL)        strcat(logtext, " FINAL");
   	if (m->flags & ACC_SYNCHRONIZED) strcat(logtext, " SYNCHRONIZED");
   	if (m->flags & ACC_VOLATILE)     strcat(logtext, " VOLATILE");
   	if (m->flags & ACC_TRANSIENT)    strcat(logtext, " TRANSIENT");
   	if (m->flags & ACC_NATIVE)       strcat(logtext, " NATIVE");
   	if (m->flags & ACC_INTERFACE)    strcat(logtext, " INTERFACE");
   	if (m->flags & ACC_ABSTRACT)     strcat(logtext, " ABSTRACT");

	strcat(logtext, "(");

	if (md->paramcount >= 1) {
		logtext = builtin_print_argument(logtext, &logtextlen,
										 &md->paramtypes[0], a0);
	}

	if (md->paramcount >= 2) {
		strcat(logtext, ", ");

		logtext = builtin_print_argument(logtext, &logtextlen,
										 &md->paramtypes[1], a1);
	}

#if TRACE_ARGS_NUM >= 4
	if (md->paramcount >= 3) {
		strcat(logtext, ", ");

		logtext = builtin_print_argument(logtext, &logtextlen,
										 &md->paramtypes[2], a2);
	}

	if (md->paramcount >= 4) {
		strcat(logtext, ", ");

		logtext = builtin_print_argument(logtext, &logtextlen,
										 &md->paramtypes[3], a3);
	}
#endif

#if TRACE_ARGS_NUM >= 6
	if (md->paramcount >= 5) {
		strcat(logtext, ", ");

		logtext = builtin_print_argument(logtext, &logtextlen,
										 &md->paramtypes[4], a4);
	}

	if (md->paramcount >= 6) {
		strcat(logtext, ", ");

		logtext = builtin_print_argument(logtext, &logtextlen,
										 &md->paramtypes[5], a5);
	}
#endif

#if TRACE_ARGS_NUM == 8
	if (md->paramcount >= 7) {
		strcat(logtext, ", ");

		logtext = builtin_print_argument(logtext, &logtextlen,
										 &md->paramtypes[6], a6);
	}

	if (md->paramcount >= 8) {
		strcat(logtext, ", ");

		logtext = builtin_print_argument(logtext, &logtextlen,
										 &md->paramtypes[7], a7);
	}
#endif

	if (md->paramcount > 8) {
		sprintf(logtext + strlen(logtext), ", ...(%d)",
				md->paramcount - TRACE_ARGS_NUM);
	}

	strcat(logtext, ")");

	log_text(logtext);

	/* release memory */

	dump_release(dumpsize);

	methodindent++;
}
#endif
#endif /* !defined(NDEBUG) */


/* builtin_verbosecall_exit ****************************************************

   Print method exit for -verbose:call.

*******************************************************************************/

#if !defined(NDEBUG)
void builtin_verbosecall_exit(s8 l, double d, float f, methodinfo *m)
{
	methoddesc *md;
	char       *logtext;
	s4          logtextlen;
	s4          dumpsize;
	s4          i;
	s4          pos;
	imm_union   val;

#if defined(ENABLE_VMLOG)
	vmlog_cacao_leave_method(m);
	return;
#endif

	md = m->parseddesc;

	/* calculate message length */

	logtextlen =
		strlen("4294967295 ") +
		strlen("-2147483647-") +        /* INT_MAX should be sufficient       */
		methodindent +
		strlen("finished: ") +
		utf_bytes(m->class->name) +
		strlen(".") +
		utf_bytes(m->name) +
		utf_bytes(m->descriptor) +
		strlen(" SYNCHRONIZED") + strlen("(") + strlen(")");

	/* add maximal argument length */

	logtextlen += strlen("->0.4872328470301428 (0x0123456789abcdef)");

	/* allocate memory */

	dumpsize = dump_size();

	logtext = DMNEW(char, logtextlen);

	/* outdent the log message */

	if (methodindent)
		methodindent--;
	else
		log_text("WARNING: unmatched methodindent--");

	/* generate the message */

	sprintf(logtext, "           ");
	sprintf(logtext + strlen(logtext), "-%d-", methodindent);

	pos = strlen(logtext);

	for (i = 0; i < methodindent; i++)
		logtext[pos++] = '\t';

	strcpy(logtext + pos, "finished: ");
	utf_cat_classname(logtext, m->class->name);
	strcat(logtext, ".");
	utf_cat(logtext, m->name);
	utf_cat(logtext, m->descriptor);

	if (!IS_VOID_TYPE(md->returntype.type)) {
		strcat(logtext, "->");

		switch (md->returntype.type) {
		case TYPE_INT:
		case TYPE_LNG:
		case TYPE_ADR:
			val.l = l;
			break;

		case TYPE_FLT:
			val.f = f;
			break;

		case TYPE_DBL:
			val.d = d;
			break;
		}

		logtext =
			builtin_print_argument(logtext, &logtextlen, &md->returntype, val.l);
	}

	log_text(logtext);

	/* release memory */

	dump_release(dumpsize);
}
#endif /* !defined(NDEBUG) */


#if defined(ENABLE_CYCLES_STATS)
void builtin_print_cycles_stats(FILE *file)
{
	fprintf(file,"builtin cylce count statistics:\n");

	CYCLES_STATS_PRINT_OVERHEAD(builtin_overhead,file);
	CYCLES_STATS_PRINT(builtin_new         ,file);

	fprintf(file,"\n");
}
#endif /* defined(ENABLE_CYCLES_STATS) */


/*****************************************************************************
			  MISCELLANEOUS HELPER FUNCTIONS
*****************************************************************************/



/*********** Functions for integer divisions *****************************
 
	On some systems (eg. DEC ALPHA), integer division is not supported by the
	CPU. These helper functions implement the missing functionality.

******************************************************************************/

#if !SUPPORT_DIVISION || defined(DISABLE_GC)
s4 builtin_idiv(s4 a, s4 b)
{
	s4 c;

	c = a / b;

	return c;
}

s4 builtin_irem(s4 a, s4 b)
{
	s4 c;

	c = a % b;

	return c;
}
#endif /* !SUPPORT_DIVISION || defined(DISABLE_GC) */


/* functions for long arithmetics **********************************************

   On systems where 64 bit Integers are not supported by the CPU,
   these functions are needed.

******************************************************************************/

#if !(SUPPORT_LONG && SUPPORT_LONG_ADD)
s8 builtin_ladd(s8 a, s8 b)
{
	s8 c;

#if U8_AVAILABLE
	c = a + b; 
#else
	c = builtin_i2l(0);
#endif

	return c;
}

s8 builtin_lsub(s8 a, s8 b)
{
	s8 c;

#if U8_AVAILABLE
	c = a - b; 
#else
	c = builtin_i2l(0);
#endif

	return c;
}

s8 builtin_lneg(s8 a)
{
	s8 c;

#if U8_AVAILABLE
	c = -a;
#else
	c = builtin_i2l(0);
#endif

	return c;
}
#endif /* !(SUPPORT_LONG && SUPPORT_LONG_ADD) */


#if !(SUPPORT_LONG && SUPPORT_LONG_MUL)
s8 builtin_lmul(s8 a, s8 b)
{
	s8 c;

#if U8_AVAILABLE
	c = a * b; 
#else
	c = builtin_i2l(0);
#endif

	return c;
}
#endif /* !(SUPPORT_LONG && SUPPORT_LONG_MUL) */


#if !(SUPPORT_DIVISION && SUPPORT_LONG && SUPPORT_LONG_DIV)
s8 builtin_ldiv(s8 a, s8 b)
{
	s8 c;

#if U8_AVAILABLE
	c = a / b; 
#else
	c = builtin_i2l(0);
#endif

	return c;
}

s8 builtin_lrem(s8 a, s8 b)
{
	s8 c;

#if U8_AVAILABLE
	c = a % b; 
#else
	c = builtin_i2l(0);
#endif

	return c;
}
#endif /* !(SUPPORT_DIVISION && SUPPORT_LONG && SUPPORT_LONG_DIV) */


#if !(SUPPORT_LONG && SUPPORT_LONG_SHIFT)
s8 builtin_lshl(s8 a, s4 b)
{
	s8 c;

#if U8_AVAILABLE
	c = a << (b & 63);
#else
	c = builtin_i2l(0);
#endif

	return c;
}

s8 builtin_lshr(s8 a, s4 b)
{
	s8 c;

#if U8_AVAILABLE
	c = a >> (b & 63);
#else
	c = builtin_i2l(0);
#endif

	return c;
}

s8 builtin_lushr(s8 a, s4 b)
{
	s8 c;

#if U8_AVAILABLE
	c = ((u8) a) >> (b & 63);
#else
	c = builtin_i2l(0);
#endif

	return c;
}
#endif /* !(SUPPORT_LONG && SUPPORT_LONG_SHIFT) */


#if !(SUPPORT_LONG && SUPPORT_LONG_LOGICAL)
s8 builtin_land(s8 a, s8 b)
{
	s8 c;

#if U8_AVAILABLE
	c = a & b; 
#else
	c = builtin_i2l(0);
#endif

	return c;
}

s8 builtin_lor(s8 a, s8 b)
{
	s8 c;

#if U8_AVAILABLE
	c = a | b; 
#else
	c = builtin_i2l(0);
#endif

	return c;
}

s8 builtin_lxor(s8 a, s8 b) 
{
	s8 c;

#if U8_AVAILABLE
	c = a ^ b; 
#else
	c = builtin_i2l(0);
#endif

	return c;
}
#endif /* !(SUPPORT_LONG && SUPPORT_LONG_LOGICAL) */


#if !(SUPPORT_LONG && SUPPORT_LONG_CMP)
s4 builtin_lcmp(s8 a, s8 b)
{ 
#if U8_AVAILABLE
	if (a < b)
		return -1;

	if (a > b)
		return 1;

	return 0;
#else
	return 0;
#endif
}
#endif /* !(SUPPORT_LONG && SUPPORT_LONG_CMP) */


/* functions for unsupported floating instructions ****************************/

/* used to convert FLT_xxx defines into float values */

static inline float intBitsToFloat(s4 i)
{
	imm_union imb;

	imb.i = i;
	return imb.f;
}


/* used to convert DBL_xxx defines into double values */

static inline float longBitsToDouble(s8 l)
{
	imm_union imb;

	imb.l = l;
	return imb.d;
}


#if !SUPPORT_FLOAT
float builtin_fadd(float a, float b)
{
	if (isnanf(a)) return intBitsToFloat(FLT_NAN);
	if (isnanf(b)) return intBitsToFloat(FLT_NAN);
	if (finitef(a)) {
		if (finitef(b))
			return a + b;
		else
			return b;
	}
	else {
		if (finitef(b))
			return a;
		else {
			if (copysignf(1.0, a) == copysignf(1.0, b))
				return a;
			else
				return intBitsToFloat(FLT_NAN);
		}
	}
}


float builtin_fsub(float a, float b)
{
	return builtin_fadd(a, builtin_fneg(b));
}


float builtin_fmul(float a, float b)
{
	if (isnanf(a)) return intBitsToFloat(FLT_NAN);
	if (isnanf(b)) return intBitsToFloat(FLT_NAN);
	if (finitef(a)) {
		if (finitef(b)) return a * b;
		else {
			if (a == 0) return intBitsToFloat(FLT_NAN);
			else return copysignf(b, copysignf(1.0, b)*a);
		}
	}
	else {
		if (finitef(b)) {
			if (b == 0) return intBitsToFloat(FLT_NAN);
			else return copysignf(a, copysignf(1.0, a)*b);
		}
		else {
			return copysignf(a, copysignf(1.0, a)*copysignf(1.0, b));
		}
	}
}


/* builtin_ddiv ****************************************************************

   Implementation as described in VM Spec.

*******************************************************************************/

float builtin_fdiv(float a, float b)
{
	if (finitef(a)) {
		if (finitef(b)) {
			/* If neither value1' nor value2' is NaN, the sign of the result */
			/* is positive if both values have the same sign, negative if the */
			/* values have different signs. */

			return a / b;

		} else {
			if (isnanf(b)) {
				/* If either value1' or value2' is NaN, the result is NaN. */

				return intBitsToFloat(FLT_NAN);

			} else {
				/* Division of a finite value by an infinity results in a */
				/* signed zero, with the sign-producing rule just given. */

				/* is sign equal? */

				if (copysignf(1.0, a) == copysignf(1.0, b))
					return 0.0;
				else
					return -0.0;
			}
		}

	} else {
		if (isnanf(a)) {
			/* If either value1' or value2' is NaN, the result is NaN. */

			return intBitsToFloat(FLT_NAN);

		} else if (finitef(b)) {
			/* Division of an infinity by a finite value results in a signed */
			/* infinity, with the sign-producing rule just given. */

			/* is sign equal? */

			if (copysignf(1.0, a) == copysignf(1.0, b))
				return intBitsToFloat(FLT_POSINF);
			else
				return intBitsToFloat(FLT_NEGINF);

		} else {
			/* Division of an infinity by an infinity results in NaN. */

			return intBitsToFloat(FLT_NAN);
		}
	}
}


float builtin_fneg(float a)
{
	if (isnanf(a)) return a;
	else {
		if (finitef(a)) return -a;
		else return copysignf(a, -copysignf(1.0, a));
	}
}
#endif /* !SUPPORT_FLOAT */


#if !SUPPORT_FLOAT || !SUPPORT_FLOAT_CMP || defined(ENABLE_INTRP)
s4 builtin_fcmpl(float a, float b)
{
	if (isnanf(a))
		return -1;

	if (isnanf(b))
		return -1;

	if (!finitef(a) || !finitef(b)) {
		a = finitef(a) ? 0 : copysignf(1.0,	a);
		b = finitef(b) ? 0 : copysignf(1.0, b);
	}

	if (a > b)
		return 1;

	if (a == b)
		return 0;

	return -1;
}


s4 builtin_fcmpg(float a, float b)
{
	if (isnanf(a)) return 1;
	if (isnanf(b)) return 1;
	if (!finitef(a) || !finitef(b)) {
		a = finitef(a) ? 0 : copysignf(1.0, a);
		b = finitef(b) ? 0 : copysignf(1.0, b);
	}
	if (a > b) return 1;
	if (a == b) return 0;
	return -1;
}
#endif /* !SUPPORT_FLOAT || !SUPPORT_FLOAT_CMP || defined(ENABLE_INTRP) */


float builtin_frem(float a, float b)
{
	return fmodf(a, b);
}


/* functions for unsupported double instructions ******************************/

#if !SUPPORT_DOUBLE
double builtin_dadd(double a, double b)
{
	if (isnan(a)) return longBitsToDouble(DBL_NAN);
	if (isnan(b)) return longBitsToDouble(DBL_NAN);
	if (finite(a)) {
		if (finite(b)) return a + b;
		else return b;
	}
	else {
		if (finite(b)) return a;
		else {
			if (copysign(1.0, a)==copysign(1.0, b)) return a;
			else return longBitsToDouble(DBL_NAN);
		}
	}
}


double builtin_dsub(double a, double b)
{
	return builtin_dadd(a, builtin_dneg(b));
}


double builtin_dmul(double a, double b)
{
	if (isnan(a)) return longBitsToDouble(DBL_NAN);
	if (isnan(b)) return longBitsToDouble(DBL_NAN);
	if (finite(a)) {
		if (finite(b)) return a * b;
		else {
			if (a == 0) return longBitsToDouble(DBL_NAN);
			else return copysign(b, copysign(1.0, b) * a);
		}
	}
	else {
		if (finite(b)) {
			if (b == 0) return longBitsToDouble(DBL_NAN);
			else return copysign(a, copysign(1.0, a) * b);
		}
		else {
			return copysign(a, copysign(1.0, a) * copysign(1.0, b));
		}
	}
}


/* builtin_ddiv ****************************************************************

   Implementation as described in VM Spec.

*******************************************************************************/

double builtin_ddiv(double a, double b)
{
	if (finite(a)) {
		if (finite(b)) {
			/* If neither value1' nor value2' is NaN, the sign of the result */
			/* is positive if both values have the same sign, negative if the */
			/* values have different signs. */

			return a / b;

		} else {
			if (isnan(b)) {
				/* If either value1' or value2' is NaN, the result is NaN. */

				return longBitsToDouble(DBL_NAN);

			} else {
				/* Division of a finite value by an infinity results in a */
				/* signed zero, with the sign-producing rule just given. */

				/* is sign equal? */

				if (copysign(1.0, a) == copysign(1.0, b))
					return 0.0;
				else
					return -0.0;
			}
		}

	} else {
		if (isnan(a)) {
			/* If either value1' or value2' is NaN, the result is NaN. */

			return longBitsToDouble(DBL_NAN);

		} else if (finite(b)) {
			/* Division of an infinity by a finite value results in a signed */
			/* infinity, with the sign-producing rule just given. */

			/* is sign equal? */

			if (copysign(1.0, a) == copysign(1.0, b))
				return longBitsToDouble(DBL_POSINF);
			else
				return longBitsToDouble(DBL_NEGINF);

		} else {
			/* Division of an infinity by an infinity results in NaN. */

			return longBitsToDouble(DBL_NAN);
		}
	}
}


/* builtin_dneg ****************************************************************

   Implemented as described in VM Spec.

*******************************************************************************/

double builtin_dneg(double a)
{
	if (isnan(a)) {
		/* If the operand is NaN, the result is NaN (recall that NaN has no */
		/* sign). */

		return a;

	} else {
		if (finite(a)) {
			/* If the operand is a zero, the result is the zero of opposite */
			/* sign. */

			return -a;

		} else {
			/* If the operand is an infinity, the result is the infinity of */
			/* opposite sign. */

			return copysign(a, -copysign(1.0, a));
		}
	}
}
#endif /* !SUPPORT_DOUBLE */


#if !SUPPORT_DOUBLE || !SUPPORT_DOUBLE_CMP || defined(ENABLE_INTRP)
s4 builtin_dcmpl(double a, double b)
{
	if (isnan(a))
		return -1;

	if (isnan(b))
		return -1;

	if (!finite(a) || !finite(b)) {
		a = finite(a) ? 0 : copysign(1.0, a);
		b = finite(b) ? 0 : copysign(1.0, b);
	}

	if (a > b)
		return 1;

	if (a == b)
		return 0;

	return -1;
}


s4 builtin_dcmpg(double a, double b)
{
	if (isnan(a))
		return 1;

	if (isnan(b))
		return 1;

	if (!finite(a) || !finite(b)) {
		a = finite(a) ? 0 : copysign(1.0, a);
		b = finite(b) ? 0 : copysign(1.0, b);
	}

	if (a > b)
		return 1;

	if (a == b)
		return 0;

	return -1;
}
#endif /* !SUPPORT_DOUBLE || !SUPPORT_DOUBLE_CMP || defined(ENABLE_INTRP) */


double builtin_drem(double a, double b)
{
	return fmod(a, b);
}


/* conversion operations ******************************************************/

#if 0
s8 builtin_i2l(s4 i)
{
#if U8_AVAILABLE
	return i;
#else
	s8 v;
	v.high = 0;
	v.low = i;
	return v;
#endif
}

s4 builtin_l2i(s8 l)
{
#if U8_AVAILABLE
	return (s4) l;
#else
	return l.low;
#endif
}
#endif


#if !(SUPPORT_FLOAT && SUPPORT_I2F)
float builtin_i2f(s4 a)
{
	float f = (float) a;
	return f;
}
#endif /* !(SUPPORT_FLOAT && SUPPORT_I2F) */


#if !(SUPPORT_DOUBLE && SUPPORT_I2D)
double builtin_i2d(s4 a)
{
	double d = (double) a;
	return d;
}
#endif /* !(SUPPORT_DOUBLE && SUPPORT_I2D) */


#if !(SUPPORT_LONG && SUPPORT_FLOAT && SUPPORT_L2F)
float builtin_l2f(s8 a)
{
#if U8_AVAILABLE
	float f = (float) a;
	return f;
#else
	return 0.0;
#endif
}
#endif /* !(SUPPORT_LONG && SUPPORT_FLOAT && SUPPORT_L2F) */


#if !(SUPPORT_LONG && SUPPORT_DOUBLE && SUPPORT_L2D)
double builtin_l2d(s8 a)
{
#if U8_AVAILABLE
	double d = (double) a;
	return d;
#else
	return 0.0;
#endif
}
#endif /* !(SUPPORT_LONG && SUPPORT_DOUBLE && SUPPORT_L2D) */


#if !(SUPPORT_FLOAT && SUPPORT_F2I) || defined(ENABLE_INTRP) || defined(DISABLE_GC)
s4 builtin_f2i(float a) 
{
	s4 i;

	i = builtin_d2i((double) a);

	return i;

	/*	float f;
	
		if (isnanf(a))
		return 0;
		if (finitef(a)) {
		if (a > 2147483647)
		return 2147483647;
		if (a < (-2147483648))
		return (-2147483648);
		return (s4) a;
		}
		f = copysignf((float) 1.0, a);
		if (f > 0)
		return 2147483647;
		return (-2147483648); */
}
#endif /* !(SUPPORT_FLOAT && SUPPORT_F2I) || defined(ENABLE_INTRP) || defined(DISABLE_GC) */


#if !(SUPPORT_FLOAT && SUPPORT_LONG && SUPPORT_F2L)
s8 builtin_f2l(float a)
{
	s8 l;

	l = builtin_d2l((double) a);

	return l;

	/*	float f;
	
		if (finitef(a)) {
		if (a > 9223372036854775807L)
		return 9223372036854775807L;
		if (a < (-9223372036854775808L))
		return (-9223372036854775808L);
		return (s8) a;
		}
		if (isnanf(a))
		return 0;
		f = copysignf((float) 1.0, a);
		if (f > 0)
		return 9223372036854775807L;
		return (-9223372036854775808L); */
}
#endif /* !(SUPPORT_FLOAT && SUPPORT_LONG && SUPPORT_F2L) */


#if !(SUPPORT_DOUBLE && SUPPORT_D2I) || defined(ENABLE_INTRP) || defined(DISABLE_GC)
s4 builtin_d2i(double a) 
{ 
	double d;
	
	if (finite(a)) {
		if (a >= 2147483647)
			return 2147483647;
		if (a <= (-2147483647-1))
			return (-2147483647-1);
		return (s4) a;
	}
	if (isnan(a))
		return 0;
	d = copysign(1.0, a);
	if (d > 0)
		return 2147483647;
	return (-2147483647-1);
}
#endif /* !(SUPPORT_DOUBLE && SUPPORT_D2I) || defined(ENABLE_INTRP) || defined(DISABLE_GC) */


#if !(SUPPORT_DOUBLE && SUPPORT_LONG && SUPPORT_D2L)
s8 builtin_d2l(double a)
{
	double d;
	
	if (finite(a)) {
		if (a >= 9223372036854775807LL)
			return 9223372036854775807LL;
		if (a <= (-9223372036854775807LL-1))
			return (-9223372036854775807LL-1);
		return (s8) a;
	}
	if (isnan(a))
		return 0;
	d = copysign(1.0, a);
	if (d > 0)
		return 9223372036854775807LL;
	return (-9223372036854775807LL-1);
}
#endif /* !(SUPPORT_DOUBLE && SUPPORT_LONG && SUPPORT_D2L) */


#if !(SUPPORT_FLOAT && SUPPORT_DOUBLE)
double builtin_f2d(float a)
{
	if (finitef(a)) return (double) a;
	else {
		if (isnanf(a))
			return longBitsToDouble(DBL_NAN);
		else
			return copysign(longBitsToDouble(DBL_POSINF), (double) copysignf(1.0, a) );
	}
}

float builtin_d2f(double a)
{
	if (finite(a))
		return (float) a;
	else {
		if (isnan(a))
			return intBitsToFloat(FLT_NAN);
		else
			return copysignf(intBitsToFloat(FLT_POSINF), (float) copysign(1.0, a));
	}
}
#endif /* !(SUPPORT_FLOAT && SUPPORT_DOUBLE) */


/* builtin_arraycopy ***********************************************************

   Builtin for java.lang.System.arraycopy.

   ATTENTION: This builtin function returns a boolean value to signal
   the ICMD_BUILTIN if there was an exception.

*******************************************************************************/

bool builtin_arraycopy(java_arrayheader *src, s4 srcStart,
					   java_arrayheader *dest, s4 destStart, s4 len)
{
	arraydescriptor *sdesc;
	arraydescriptor *ddesc;
	s4               i;

	if ((src == NULL) || (dest == NULL)) { 
		exceptions_throw_nullpointerexception();
		return false;
	}

	sdesc = src->objheader.vftbl->arraydesc;
	ddesc = dest->objheader.vftbl->arraydesc;

	if (!sdesc || !ddesc || (sdesc->arraytype != ddesc->arraytype)) {
		exceptions_throw_arraystoreexception();
		return false;
	}

	/* we try to throw exception with the same message as SUN does */

	if ((len < 0) || (srcStart < 0) || (destStart < 0) ||
		(srcStart  + len < 0) || (srcStart  + len > src->size) ||
		(destStart + len < 0) || (destStart + len > dest->size)) {
		exceptions_throw_arrayindexoutofboundsexception();
		return false;
	}

	if (sdesc->componentvftbl == ddesc->componentvftbl) {
		/* We copy primitive values or references of exactly the same type */

		s4 dataoffset = sdesc->dataoffset;
		s4 componentsize = sdesc->componentsize;

		memmove(((u1 *) dest) + dataoffset + componentsize * destStart,
				((u1 *) src)  + dataoffset + componentsize * srcStart,
				(size_t) len * componentsize);
	}
	else {
		/* We copy references of different type */

		java_objectarray *oas = (java_objectarray *) src;
		java_objectarray *oad = (java_objectarray *) dest;
                
		if (destStart <= srcStart) {
			for (i = 0; i < len; i++) {
				java_objectheader *o = oas->data[srcStart + i];

				if (!builtin_canstore(oad, o))
					return false;

				oad->data[destStart + i] = o;
			}
		}
		else {
			/* XXX this does not completely obey the specification!
			   If an exception is thrown only the elements above the
			   current index have been copied. The specification
			   requires that only the elements *below* the current
			   index have been copied before the throw. */

			for (i = len - 1; i >= 0; i--) {
				java_objectheader *o = oas->data[srcStart + i];

				if (!builtin_canstore(oad, o))
					return false;

				oad->data[destStart + i] = o;
			}
		}
	}

	return true;
}


/* builtin_currenttimemillis ***************************************************

   Return the current time in milliseconds.

*******************************************************************************/

static s8 millis = 0;

s8 builtin_currenttimemillis(void)
{
/*	struct timeval tv;
	s8             result;

	if (gettimeofday(&tv, NULL) == -1)
		vm_abort("gettimeofday failed: %s", strerror(errno));

	result = (s8) tv.tv_sec;
	result *= 1000;
	result += (tv.tv_usec / 1000);

	return result; */
	s8 c = clock();
        return c;
}


/* builtin_clone ***************************************************************

   Function for cloning objects or arrays.

*******************************************************************************/

java_objectheader *builtin_clone(void *env, java_objectheader *o)
{
	arraydescriptor   *ad;
	java_arrayheader  *ah;
	u4                 size;
	classinfo         *c;
	java_objectheader *co;              /* cloned object header               */

	/* get the array descriptor */

	ad = o->vftbl->arraydesc;

	/* we are cloning an array */

	if (ad != NULL) {
		ah = (java_arrayheader *) o;

		size = ad->dataoffset + ad->componentsize * ah->size;
        
		co = heap_allocate(size, (ad->arraytype == ARRAYTYPE_OBJECT), NULL);

		if (co == NULL)
			return NULL;

		MCOPY(co, o, u1, size);

#if defined(ENABLE_GC_CACAO)
		heap_init_objectheader(co, size);
#endif

#if defined(ENABLE_THREADS)
		lock_init_object_lock(co);
#endif

		return co;
	}
    
    /* we are cloning a non-array */

    if (!builtin_instanceof(o, class_java_lang_Cloneable)) {
        exceptions_throw_clonenotsupportedexception();
        return NULL;
    }

	/* get the class of the object */

    c = o->vftbl->class;

	/* create new object */

    co = builtin_new(c);

    if (co == NULL)
        return NULL;

    MCOPY(co, o, u1, c->instancesize);

#if defined(ENABLE_GC_CACAO)
	heap_init_objectheader(co, c->instancesize);
#endif

#if defined(ENABLE_THREADS)
	lock_init_object_lock(co);
#endif

    return co;
}

#if defined(ENABLE_VMLOG)
#define NDEBUG
#include <vmlog_cacao.c>
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
 * vim:noexpandtab:sw=4:ts=4:
 */

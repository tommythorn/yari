/* src/vmcore/linker.c - class linker functions

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

   $Id: linker.c 7870 2007-05-05 13:46:11Z tbfg $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "native/native.h"

#include "threads/lock-common.h"

#include "toolbox/logging.h"

#include "vm/access.h"
#include "vm/exceptions.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit_interface.h"

#include "vmcore/class.h"
#include "vmcore/classcache.h"
#include "vmcore/loader.h"
#include "vmcore/options.h"
#include "vmcore/rt-timing.h"

/* #include "vm/resolve.h" */
/* copied prototype to avoid bootstrapping problem: */
classinfo *resolve_classref_or_classinfo_eager(classref_or_classinfo cls, bool checkaccess);

#if defined(ENABLE_STATISTICS)
# include "vmcore/statistics.h"
#endif

#if !defined(NDEBUG) && defined(ENABLE_INLINING)
#define INLINELOG(code)  do { if (opt_inline_debug_log) { code } } while (0)
#else
#define INLINELOG(code)
#endif


/* global variables ***********************************************************/

static s4 interfaceindex;       /* sequential numbering of interfaces         */
static s4 classvalue;

java_objectheader *linker_classrenumber_lock;


/* primitivetype_table *********************************************************

   Structure for primitive classes: contains the class for wrapping
   the primitive type, the primitive class, the name of the class for
   wrapping, the one character type signature and the name of the
   primitive class.
 
   CAUTION: Don't change the order of the types. This table is indexed
   by the ARRAYTYPE_ constants (except ARRAYTYPE_OBJECT).

*******************************************************************************/

primitivetypeinfo primitivetype_table[PRIMITIVETYPE_COUNT] = {
	{ NULL, NULL, "java/lang/Integer",   'I', "int"     , "[I", NULL, NULL },
	{ NULL, NULL, "java/lang/Long",      'J', "long"    , "[J", NULL, NULL },
	{ NULL, NULL, "java/lang/Float",     'F', "float"   , "[F", NULL, NULL },
	{ NULL, NULL, "java/lang/Double",    'D', "double"  , "[D", NULL, NULL },
	{ NULL, NULL, NULL,                   0 , NULL      , NULL, NULL, NULL },
	{ NULL, NULL, "java/lang/Byte",	     'B', "byte"    , "[B", NULL, NULL },
	{ NULL, NULL, "java/lang/Character", 'C', "char"    , "[C", NULL, NULL },
	{ NULL, NULL, "java/lang/Short",     'S', "short"   , "[S", NULL, NULL },
	{ NULL, NULL, "java/lang/Boolean",   'Z', "boolean" , "[Z", NULL, NULL },
	{ NULL, NULL, NULL,                   0 , NULL      , NULL, NULL, NULL },
#if defined(ENABLE_JAVASE)
   	{ NULL, NULL, "java/lang/Void",	     'V', "void"    , NULL, NULL, NULL }
#else
	{ NULL, NULL, NULL,                   0 , NULL      , NULL, NULL, NULL },
#endif
};


/* private functions **********************************************************/

static bool link_primitivetype_table(void);
static classinfo *link_class_intern(classinfo *c);
static arraydescriptor *link_array(classinfo *c);
static void linker_compute_class_values(classinfo *c);
static void linker_compute_subclasses(classinfo *c);
static bool linker_addinterface(classinfo *c, classinfo *ic);
static s4 class_highestinterface(classinfo *c);


/* linker_init *****************************************************************

   Initializes the linker subsystem.

*******************************************************************************/

bool linker_init(void)
{
	/* reset interface index */

	interfaceindex = 0;

#if defined(ENABLE_THREADS)
	/* create the global lock object */

	linker_classrenumber_lock = NEW(java_objectheader);

	LOCK_INIT_OBJECT_LOCK(linker_classrenumber_lock);
#endif

	/* link java.lang.Class as first class of the system, because we
       need it's vftbl for all other classes so we can use a class as
       object */

	if (!link_class(class_java_lang_Class))
		return false;

	/* now set the header.vftbl of all classes which were created
       before java.lang.Class was linked */

	class_postset_header_vftbl();


	/* link important system classes */

	if (!link_class(class_java_lang_Object))
		return false;

	if (!link_class(class_java_lang_String))
		return false;

#if defined(ENABLE_JAVASE)
	if (!link_class(class_java_lang_Cloneable))
		return false;

	if (!link_class(class_java_io_Serializable))
		return false;
#endif


	/* link classes for wrapping primitive types */

#if defined(ENABLE_JAVASE)
	if (!link_class(class_java_lang_Void))
		return false;
#endif

	if (!link_class(class_java_lang_Boolean))
		return false;

	if (!link_class(class_java_lang_Byte))
		return false;

	if (!link_class(class_java_lang_Character))
		return false;

	if (!link_class(class_java_lang_Short))
		return false;

	if (!link_class(class_java_lang_Integer))
		return false;

	if (!link_class(class_java_lang_Long))
		return false;

	if (!link_class(class_java_lang_Float))
		return false;

	if (!link_class(class_java_lang_Double))
		return false;


	/* load some other important classes */

#if defined(ENABLE_JAVASE)
	if (!link_class(class_java_lang_ClassLoader))
		return false;

	if (!link_class(class_java_lang_SecurityManager))
		return false;
#endif

	if (!link_class(class_java_lang_System))
		return false;

	if (!link_class(class_java_lang_Thread))
		return false;

#if defined(ENABLE_JAVASE)
	if (!link_class(class_java_lang_ThreadGroup))
		return false;
#endif

#if defined(WITH_CLASSPATH_GNU)
	if (!link_class(class_java_lang_VMSystem))
		return false;

	if (!link_class(class_java_lang_VMThread))
		return false;
#endif


	/* some classes which may be used more often */

#if defined(ENABLE_JAVASE)
	if (!link_class(class_java_lang_StackTraceElement))
		return false;

	if (!link_class(class_java_lang_reflect_Constructor))
		return false;

	if (!link_class(class_java_lang_reflect_Field))
		return false;

	if (!link_class(class_java_lang_reflect_Method))
		return false;

	if (!link_class(class_java_security_PrivilegedAction))
		return false;

	if (!link_class(class_java_util_Vector))
		return false;

	if (!link_class(arrayclass_java_lang_Object))
		return false;
#endif


	/* create pseudo classes used by the typechecker */

    /* pseudo class for Arraystubs (extends java.lang.Object) */

	pseudo_class_Arraystub =
		class_create_classinfo(utf_new_char("$ARRAYSTUB$"));
	pseudo_class_Arraystub->state            |= CLASS_LOADED;
	pseudo_class_Arraystub->super.cls         = class_java_lang_Object;

#if defined(ENABLE_JAVASE)
	pseudo_class_Arraystub->interfacescount   = 2;
	pseudo_class_Arraystub->interfaces        = MNEW(classref_or_classinfo, 2);
	pseudo_class_Arraystub->interfaces[0].cls = class_java_lang_Cloneable;
	pseudo_class_Arraystub->interfaces[1].cls = class_java_io_Serializable;
#elif defined(ENABLE_JAVAME_CLDC1_1)
	pseudo_class_Arraystub->interfacescount   = 0;
	pseudo_class_Arraystub->interfaces        = NULL;
#endif

	if (!classcache_store_unique(pseudo_class_Arraystub)) {
		log_text("could not cache pseudo_class_Arraystub");
		assert(0);
	}

	if (!link_class(pseudo_class_Arraystub))
		return false;

	/* pseudo class representing the null type */

	pseudo_class_Null = class_create_classinfo(utf_new_char("$NULL$"));
	pseudo_class_Null->state |= CLASS_LOADED;
	pseudo_class_Null->super.cls = class_java_lang_Object;

	if (!classcache_store_unique(pseudo_class_Null))
		vm_abort("linker_init: could not cache pseudo_class_Null");

	if (!link_class(pseudo_class_Null))
		return false;

	/* pseudo class representing new uninitialized objects */
    
	pseudo_class_New = class_create_classinfo(utf_new_char("$NEW$"));
	pseudo_class_New->state |= CLASS_LOADED;
	pseudo_class_New->state |= CLASS_LINKED; /* XXX is this allright? */
	pseudo_class_New->super.cls = class_java_lang_Object;

	if (!classcache_store_unique(pseudo_class_New))
		vm_abort("linker_init: could not cache pseudo_class_New");

	/* create classes representing primitive types */

	if (!link_primitivetype_table())
		return false;


	/* Correct vftbl-entries (retarded loading and linking of class
	   java/lang/String). */

	stringtable_update();

	return true;
}


/* link_primitivetype_table ****************************************************

   Create classes representing primitive types.

*******************************************************************************/

static bool link_primitivetype_table(void)
{  
	classinfo *c;
	utf       *u;
	s4         i;

	for (i = 0; i < PRIMITIVETYPE_COUNT; i++) {
		/* skip dummies */

		if (!primitivetype_table[i].name)
			continue;
		
		/* create primitive class */

		c = class_create_classinfo(utf_new_char(primitivetype_table[i].name));

		/* primitive classes don't have a super class */

		c->super.any = NULL;

		/* set flags and mark it as primitive class */

		c->flags = ACC_PUBLIC | ACC_FINAL | ACC_ABSTRACT | ACC_CLASS_PRIMITIVE;
		
		/* prevent loader from loading primitive class */

		c->state |= CLASS_LOADED;

		/* INFO: don't put primitive classes into the classcache */

		if (!link_class(c))
			return false;

		primitivetype_table[i].class_primitive = c;

		/* create class for wrapping the primitive type */

		u = utf_new_char(primitivetype_table[i].wrapname);
		c = load_class_bootstrap(u);

		if (c == NULL)
			return false;

		primitivetype_table[i].class_wrap = c;

		/* create the primitive array class */

		if (primitivetype_table[i].arrayname) {
			u = utf_new_char(primitivetype_table[i].arrayname);
			c = class_create_classinfo(u);
			c = load_newly_created_array(c, NULL);

			if (c == NULL)
				return false;

			primitivetype_table[i].arrayclass = c;

			assert(c->state & CLASS_LOADED);

			if (!(c->state & CLASS_LINKED))
				if (!link_class(c))
					return false;

			primitivetype_table[i].arrayvftbl = c->vftbl;
		}
	}

	return true;
}


/* link_class ******************************************************************

   Wrapper function for link_class_intern to ease monitor enter/exit
   and exception handling.

*******************************************************************************/

classinfo *link_class(classinfo *c)
{
	classinfo *r;
#if defined(ENABLE_RT_TIMING)
	struct timespec time_start, time_end;
#endif

	RT_TIMING_GET_TIME(time_start);

	if (c == NULL) {
		exceptions_throw_nullpointerexception();
		return NULL;
	}

	LOCK_MONITOR_ENTER(c);

	/* maybe the class is already linked */

	if (c->state & CLASS_LINKED) {
		LOCK_MONITOR_EXIT(c);

		return c;
	}

#if defined(ENABLE_STATISTICS)
	/* measure time */

	if (opt_getcompilingtime)
		compilingtime_stop();

	if (opt_getloadingtime)
		loadingtime_start();
#endif

	/* call the internal function */

	r = link_class_intern(c);

	/* if return value is NULL, we had a problem and the class is not linked */

	if (!r)
		c->state &= ~CLASS_LINKING;

#if defined(ENABLE_STATISTICS)
	/* measure time */

	if (opt_getloadingtime)
		loadingtime_stop();

	if (opt_getcompilingtime)
		compilingtime_start();
#endif

	LOCK_MONITOR_EXIT(c);

	RT_TIMING_GET_TIME(time_end);

	RT_TIMING_TIME_DIFF(time_start,time_end,RT_TIMING_LINK_TOTAL);

	return r;
}


/* linker_overwrite_method *****************************************************

   Overwrite a method with another one, update method flags and check
   assumptions.

   IN:
      mg................the general method being overwritten
	  ms................the overwriting (more specialized) method
	  wl................worklist where to add invalidated methods

   RETURN VALUE:
      true..............everything ok
	  false.............an exception has been thrown

*******************************************************************************/

static bool linker_overwrite_method(methodinfo *mg,
									methodinfo *ms,
									method_worklist **wl)
{
	classinfo *cg;
	classinfo *cs;

	cg = mg->class;
	cs = ms->class;

	/* overriding a final method is illegal */

	if (mg->flags & ACC_FINAL) {
		exceptions_throw_verifyerror(mg, "Overriding final method");
		return false;
	}

	/* method ms overwrites method mg */

#if defined(ENABLE_VERIFIER)
	/* Add loading constraints (for the more general types of method mg). */
	/* Not for <init>, as it is not invoked virtually.                    */

	if ((ms->name != utf_init)
			&& !classcache_add_constraints_for_params(
				cs->classloader, cg->classloader, mg))
	{
		return false;
	}
#endif

	/* inherit the vftbl index, and record the overwriting */

	ms->vftblindex = mg->vftblindex;
	ms->overwrites = mg;

	/* update flags and check assumptions */
	/* <init> methods are a special case, as they are never dispatched dynamically */

	if ((ms->flags & ACC_METHOD_IMPLEMENTED) && ms->name != utf_init) {
		do {
			if (mg->flags & ACC_METHOD_IMPLEMENTED) {
				/* this adds another implementation */

				mg->flags &= ~ACC_METHOD_MONOMORPHIC;

				INLINELOG( printf("becomes polymorphic: "); method_println(mg); );

				method_break_assumption_monomorphic(mg, wl);
			}
			else {
				/* this is the first implementation */

				mg->flags |= ACC_METHOD_IMPLEMENTED;

				INLINELOG( printf("becomes implemented: "); method_println(mg); );
			}

			ms = mg;
			mg = mg->overwrites;
		} while (mg != NULL);
	}

	return true;
}


/* link_class_intern ***********************************************************

   Tries to link a class. The function calculates the length in bytes
   that an instance of this class requires as well as the VTBL for
   methods and interface methods.
	
*******************************************************************************/

static classinfo *link_class_intern(classinfo *c)
{
	classinfo *super;             /* super class                              */
	classinfo *tc;                /* temporary class variable                 */
	s4 supervftbllength;          /* vftbllegnth of super class               */
	s4 vftbllength;               /* vftbllength of current class             */
	s4 interfacetablelength;      /* interface table length                   */
	vftbl_t *v;                   /* vftbl of current class                   */
	s4 i;                         /* interface/method/field counter           */
	arraydescriptor *arraydesc;   /* descriptor for array classes             */
	method_worklist *worklist;    /* worklist for recompilation               */
#if defined(ENABLE_RT_TIMING)
	struct timespec time_start, time_resolving, time_compute_vftbl,
					time_abstract, time_compute_iftbl, time_fill_vftbl,
					time_offsets, time_fill_iftbl, time_finalizer,
					time_subclasses;
#endif

	RT_TIMING_GET_TIME(time_start);

	/* the class is already linked */

	if (c->state & CLASS_LINKED)
		return c;

#if !defined(NDEBUG)
	if (linkverbose)
		log_message_class("Linking class: ", c);
#endif

	/* the class must be loaded */

	/* XXX should this be a specific exception? */
	assert(c->state & CLASS_LOADED);

	/* cache the self-reference of this class                          */
	/* we do this for cases where the defining loader of the class     */
	/* has not yet been recorded as an initiating loader for the class */
	/* this is needed so subsequent code can assume that self-refs     */
	/* will always resolve lazily                                      */
	/* No need to do it for the bootloader - it is always registered   */
	/* as initiating loader for the classes it loads.                  */
	if (c->classloader)
		classcache_store(c->classloader,c,false);

	/* this class is currently linking */

	c->state |= CLASS_LINKING;

	arraydesc = NULL;
	worklist = NULL;

	/* check interfaces */

	for (i = 0; i < c->interfacescount; i++) {
		/* resolve this super interface */

		if ((tc = resolve_classref_or_classinfo_eager(c->interfaces[i], true)) == NULL)
			return NULL;

		c->interfaces[i].cls = tc;
		
		/* detect circularity */

		if (tc == c) {
			exceptions_throw_classcircularityerror(c);
			return NULL;
		}

		assert(tc->state & CLASS_LOADED);

		if (!(tc->flags & ACC_INTERFACE)) {
			exceptions_throw_incompatibleclasschangeerror(tc,
														  "Implementing class");
			return NULL;
		}

		if (!(tc->state & CLASS_LINKED))
			if (!link_class(tc))
				return NULL;
	}
	
	/* check super class */

	super = NULL;

	if (c->super.any == NULL) {                     /* class java.lang.Object */
		c->index = 0;
		c->instancesize = sizeof(java_objectheader);
		
		vftbllength = supervftbllength = 0;

		c->finalizer = NULL;

	} else {
		/* resolve super class */

		if ((super = resolve_classref_or_classinfo_eager(c->super, true)) == NULL)
			return NULL;

		c->super.cls = super;
		
		/* detect circularity */

		if (super == c) {
			exceptions_throw_classcircularityerror(c);
			return NULL;
		}

		assert(super->state & CLASS_LOADED);

		if (super->flags & ACC_INTERFACE) {
			/* java.lang.IncompatibleClassChangeError: class a has interface java.lang.Cloneable as super class */
			log_text("Interface specified as super class");
			assert(0);
		}

		/* Don't allow extending final classes */

		if (super->flags & ACC_FINAL) {
			exceptions_throw_verifyerror(NULL,
										 "Cannot inherit from final class");
			return NULL;
		}

		/* link the superclass if necessary */
		
		if (!(super->state & CLASS_LINKED))
			if (!link_class(super))
				return NULL;

		/* OR the ACC_CLASS_HAS_POINTERS flag */

		c->flags |= (super->flags & ACC_CLASS_HAS_POINTERS);

		/* handle array classes */

		if (c->name->text[0] == '[')
			if (!(arraydesc = link_array(c)))
  				return NULL;

		if (c->flags & ACC_INTERFACE)
			c->index = interfaceindex++;
		else
			c->index = super->index + 1;
		
		c->instancesize = super->instancesize;

		vftbllength = supervftbllength = super->vftbl->vftbllength;
		
		c->finalizer = super->finalizer;
	}
	RT_TIMING_GET_TIME(time_resolving);


	/* compute vftbl length */

	for (i = 0; i < c->methodscount; i++) {
		methodinfo *m = &(c->methods[i]);

		if (!(m->flags & ACC_STATIC)) { /* is instance method */
			tc = super;

			while (tc) {
				s4 j;

				for (j = 0; j < tc->methodscount; j++) {
					if (method_canoverwrite(m, &(tc->methods[j]))) {
						if (tc->methods[j].flags & ACC_PRIVATE)
							goto notfoundvftblindex;

						/* package-private methods in other packages */
						/* must not be overridden                    */
						/* (see Java Language Specification 8.4.8.1) */
						if ( !(tc->methods[j].flags & (ACC_PUBLIC | ACC_PROTECTED)) 
							 && !SAME_PACKAGE(c,tc) ) 
						{
						    goto notfoundvftblindex;
						}

						if (!linker_overwrite_method(&(tc->methods[j]), m, &worklist))
							return NULL;

						goto foundvftblindex;
					}
				}

				tc = tc->super.cls;
			}

		notfoundvftblindex:
			m->vftblindex = (vftbllength++);
		foundvftblindex:
			;
		}
	}
	RT_TIMING_GET_TIME(time_compute_vftbl);


	/* Check all interfaces of an abstract class (maybe be an
	   interface too) for unimplemented methods.  Such methods are
	   called miranda-methods and are marked with the ACC_MIRANDA
	   flag.  VMClass.getDeclaredMethods does not return such
	   methods. */

	if (c->flags & ACC_ABSTRACT) {
		classinfo  *ic;
		methodinfo *im;
		s4 abstractmethodscount;
		s4 j;
		s4 k;

		abstractmethodscount = 0;

		/* check all interfaces of the abstract class */

		for (i = 0; i < c->interfacescount; i++) {
			ic = c->interfaces[i].cls;

			for (j = 0; j < ic->methodscount; j++) {
				im = &(ic->methods[j]);

				/* skip `<clinit>' and `<init>' */

				if ((im->name == utf_clinit) || (im->name == utf_init))
					continue;

				for (tc = c; tc != NULL; tc = tc->super.cls) {
					for (k = 0; k < tc->methodscount; k++) {
						if (method_canoverwrite(im, &(tc->methods[k])))
							goto noabstractmethod;
					}
				}

				abstractmethodscount++;

			noabstractmethod:
				;
			}
		}

		if (abstractmethodscount > 0) {
			methodinfo *am;

			/* reallocate methods memory */

			c->methods = MREALLOC(c->methods, methodinfo, c->methodscount,
								  c->methodscount + abstractmethodscount);

			for (i = 0; i < c->interfacescount; i++) {
				ic = c->interfaces[i].cls;

				for (j = 0; j < ic->methodscount; j++) {
					im = &(ic->methods[j]);

					/* skip `<clinit>' and `<init>' */

					if ((im->name == utf_clinit) || (im->name == utf_init))
						continue;

					for (tc = c; tc != NULL; tc = tc->super.cls) {
						for (k = 0; k < tc->methodscount; k++) {
							if (method_canoverwrite(im, &(tc->methods[k])))
								goto noabstractmethod2;
						}
					}

					/* Copy the method found into the new c->methods
					   array and tag it as miranda-method. */

					am = &(c->methods[c->methodscount]);
					c->methodscount++;

					MCOPY(am, im, methodinfo, 1);

					am->vftblindex  = (vftbllength++);
					am->class       = c;
					am->flags      |= ACC_MIRANDA;

				noabstractmethod2:
					;
				}
			}
		}
	}
	RT_TIMING_GET_TIME(time_abstract);


#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		count_vftbl_len +=
			sizeof(vftbl_t) + (sizeof(methodptr) * (vftbllength - 1));
#endif

	/* compute interfacetable length */

	interfacetablelength = 0;

	for (tc = c; tc != NULL; tc = tc->super.cls) {
		for (i = 0; i < tc->interfacescount; i++) {
			s4 h = class_highestinterface(tc->interfaces[i].cls) + 1;

			if (h > interfacetablelength)
				interfacetablelength = h;
		}
	}
	RT_TIMING_GET_TIME(time_compute_iftbl);

	/* allocate virtual function table */

	v = (vftbl_t *) mem_alloc(sizeof(vftbl_t) +
							  sizeof(methodptr) * (vftbllength - 1) +
							  sizeof(methodptr*) * (interfacetablelength - (interfacetablelength > 0)));
	v = (vftbl_t *) (((methodptr *) v) +
					 (interfacetablelength - 1) * (interfacetablelength > 1));

	c->vftbl                = v;
	v->class                = c;
	v->vftbllength          = vftbllength;
	v->interfacetablelength = interfacetablelength;
	v->arraydesc            = arraydesc;

	/* store interface index in vftbl */

	if (c->flags & ACC_INTERFACE)
		v->baseval = -(c->index);

	/* copy virtual function table of super class */

	for (i = 0; i < supervftbllength; i++) 
		v->table[i] = super->vftbl->table[i];

	/* Fill the remaining vftbl slots with the AbstractMethodError
	   stub (all after the super class slots, because they are already
	   initialized). */

	for (; i < vftbllength; i++) {
#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
		if (opt_intrp)
			v->table[i] = (methodptr) (ptrint) &intrp_asm_abstractmethoderror;
		else
# endif
			v->table[i] = (methodptr) (ptrint) &asm_abstractmethoderror;
#else
		v->table[i] = (methodptr) (ptrint) &intrp_asm_abstractmethoderror;
#endif
	}

	/* add method stubs into virtual function table */

	for (i = 0; i < c->methodscount; i++) {
		methodinfo *m = &(c->methods[i]);

		assert(m->stubroutine == NULL);

		/* Don't create a compiler stub for abstract methods as they
		   throw an AbstractMethodError with the default stub in the
		   vftbl.  This entry is simply copied by sub-classes. */

		if (m->flags & ACC_ABSTRACT)
			continue;

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
		if (opt_intrp)
			m->stubroutine = intrp_createcompilerstub(m);
		else
#endif
			m->stubroutine = codegen_generate_stub_compiler(m);
#else
		m->stubroutine = intrp_createcompilerstub(m);
#endif

		/* static methods are not in the vftbl */

		if (m->flags & ACC_STATIC)
			continue;

		/* insert the stubroutine into the vftbl */

		v->table[m->vftblindex] = (methodptr) (ptrint) m->stubroutine;
	}
	RT_TIMING_GET_TIME(time_fill_vftbl);

	/* compute instance size and offset of each field */
	
	for (i = 0; i < c->fieldscount; i++) {
		s4 dsize;
		fieldinfo *f = &(c->fields[i]);
		
		if (!(f->flags & ACC_STATIC)) {
			dsize = descriptor_typesize(f->parseddesc);

#if defined(__I386__) || defined(__ARM__)
			/* On i386 and ARM we align double and s8 fields to
			   4-bytes.  This matches what GCC does for struct
			   members. We must do the same as gcc here because the
			   offsets in native header structs like java_lang_Double
			   must match the offsets of the Java fields
			   (eg. java.lang.Double.value).  */

			c->instancesize = MEMORY_ALIGN(c->instancesize, 4);
#else
			c->instancesize = MEMORY_ALIGN(c->instancesize, dsize);
#endif

			f->offset = c->instancesize;
			c->instancesize += dsize;
		}
	}
	RT_TIMING_GET_TIME(time_offsets);

	/* initialize interfacetable and interfacevftbllength */

	v->interfacevftbllength = MNEW(s4, interfacetablelength);

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		count_vftbl_len += (4 + sizeof(s4)) * v->interfacetablelength;
#endif

	for (i = 0; i < interfacetablelength; i++) {
		v->interfacevftbllength[i] = 0;
		v->interfacetable[-i] = NULL;
	}

	/* add interfaces */

	for (tc = c; tc != NULL; tc = tc->super.cls)
		for (i = 0; i < tc->interfacescount; i++)
			if (!linker_addinterface(c, tc->interfaces[i].cls))
				return NULL;

	RT_TIMING_GET_TIME(time_fill_iftbl);

	/* add finalizer method (not for java.lang.Object) */

	if (super) {
		methodinfo *fi;

		fi = class_findmethod(c, utf_finalize, utf_void__void);

		if (fi)
			if (!(fi->flags & ACC_STATIC))
				c->finalizer = fi;
	}
	RT_TIMING_GET_TIME(time_finalizer);

	/* final tasks */

	linker_compute_subclasses(c);

	RT_TIMING_GET_TIME(time_subclasses);

	/* revert the linking state and class is linked */

	c->state = (c->state & ~CLASS_LINKING) | CLASS_LINKED;

	/* check worklist */

	/* XXX must this also be done in case of exception? */

	while (worklist != NULL) {
		method_worklist *wi = worklist;

		worklist = worklist->next;

		INLINELOG( printf("MUST BE RECOMPILED: "); method_println(wi->m); );
		jit_invalidate_code(wi->m);

		/* XXX put worklist into dump memory? */
		FREE(wi, method_worklist);
	}

#if !defined(NDEBUG)
	if (linkverbose)
		log_message_class("Linking done class: ", c);
#endif

	RT_TIMING_TIME_DIFF(time_start        ,time_resolving    ,RT_TIMING_LINK_RESOLVE);
	RT_TIMING_TIME_DIFF(time_resolving    ,time_compute_vftbl,RT_TIMING_LINK_C_VFTBL);
	RT_TIMING_TIME_DIFF(time_compute_vftbl,time_abstract     ,RT_TIMING_LINK_ABSTRACT);
	RT_TIMING_TIME_DIFF(time_abstract     ,time_compute_iftbl,RT_TIMING_LINK_C_IFTBL);
	RT_TIMING_TIME_DIFF(time_compute_iftbl,time_fill_vftbl   ,RT_TIMING_LINK_F_VFTBL);
	RT_TIMING_TIME_DIFF(time_fill_vftbl   ,time_offsets      ,RT_TIMING_LINK_OFFSETS);
	RT_TIMING_TIME_DIFF(time_offsets      ,time_fill_iftbl   ,RT_TIMING_LINK_F_IFTBL);
	RT_TIMING_TIME_DIFF(time_fill_iftbl   ,time_finalizer    ,RT_TIMING_LINK_FINALIZER);
	RT_TIMING_TIME_DIFF(time_finalizer    ,time_subclasses   ,RT_TIMING_LINK_SUBCLASS);

	/* just return c to show that we didn't had a problem */

	return c;
}


/* link_array ******************************************************************

   This function is called by link_class to create the arraydescriptor
   for an array class.

   This function returns NULL if the array cannot be linked because
   the component type has not been linked yet.

*******************************************************************************/

static arraydescriptor *link_array(classinfo *c)
{
	classinfo       *comp;
	s4               namelen;
	arraydescriptor *desc;
	vftbl_t         *compvftbl;
	utf             *u;

	comp = NULL;
	namelen = c->name->blength;

	/* Check the component type */

	switch (c->name->text[1]) {
	case '[':
		/* c is an array of arrays. */
		u = utf_new(c->name->text + 1, namelen - 1);
		if (!(comp = load_class_from_classloader(u, c->classloader)))
			return NULL;
		break;

	case 'L':
		/* c is an array of objects. */
		u = utf_new(c->name->text + 2, namelen - 3);
		if (!(comp = load_class_from_classloader(u, c->classloader)))
			return NULL;
		break;
	}

	/* If the component type has not been linked, link it now */

	assert(!comp || (comp->state & CLASS_LOADED));

	if (comp && !(comp->state & CLASS_LINKED))
		if (!link_class(comp))
			return NULL;

	/* Allocate the arraydescriptor */

	desc = NEW(arraydescriptor);

	if (comp) {
		/* c is an array of references */
		desc->arraytype = ARRAYTYPE_OBJECT;
		desc->componentsize = sizeof(void*);
		desc->dataoffset = OFFSET(java_objectarray, data);
		
		compvftbl = comp->vftbl;

		if (!compvftbl) {
			log_text("Component class has no vftbl");
			assert(0);
		}

		desc->componentvftbl = compvftbl;
		
		if (compvftbl->arraydesc) {
			desc->elementvftbl = compvftbl->arraydesc->elementvftbl;

			if (compvftbl->arraydesc->dimension >= 255) {
				log_text("Creating array of dimension >255");
				assert(0);
			}

			desc->dimension = compvftbl->arraydesc->dimension + 1;
			desc->elementtype = compvftbl->arraydesc->elementtype;

		} else {
			desc->elementvftbl = compvftbl;
			desc->dimension = 1;
			desc->elementtype = ARRAYTYPE_OBJECT;
		}

	} else {
		/* c is an array of a primitive type */
		switch (c->name->text[1]) {
		case 'Z':
			desc->arraytype = ARRAYTYPE_BOOLEAN;
			desc->dataoffset = OFFSET(java_booleanarray,data);
			desc->componentsize = sizeof(u1);
			break;

		case 'B':
			desc->arraytype = ARRAYTYPE_BYTE;
			desc->dataoffset = OFFSET(java_bytearray,data);
			desc->componentsize = sizeof(u1);
			break;

		case 'C':
			desc->arraytype = ARRAYTYPE_CHAR;
			desc->dataoffset = OFFSET(java_chararray,data);
			desc->componentsize = sizeof(u2);
			break;

		case 'D':
			desc->arraytype = ARRAYTYPE_DOUBLE;
			desc->dataoffset = OFFSET(java_doublearray,data);
			desc->componentsize = sizeof(double);
			break;

		case 'F':
			desc->arraytype = ARRAYTYPE_FLOAT;
			desc->dataoffset = OFFSET(java_floatarray,data);
			desc->componentsize = sizeof(float);
			break;

		case 'I':
			desc->arraytype = ARRAYTYPE_INT;
			desc->dataoffset = OFFSET(java_intarray,data);
			desc->componentsize = sizeof(s4);
			break;

		case 'J':
			desc->arraytype = ARRAYTYPE_LONG;
			desc->dataoffset = OFFSET(java_longarray,data);
			desc->componentsize = sizeof(s8);
			break;

		case 'S':
			desc->arraytype = ARRAYTYPE_SHORT;
			desc->dataoffset = OFFSET(java_shortarray,data);
			desc->componentsize = sizeof(s2);
			break;

		default:
			exceptions_throw_noclassdeffounderror(c->name);
			return NULL;
		}
		
		desc->componentvftbl = NULL;
		desc->elementvftbl = NULL;
		desc->dimension = 1;
		desc->elementtype = desc->arraytype;
	}

	return desc;
}


/* linker_compute_subclasses ***************************************************

   XXX

*******************************************************************************/

static void linker_compute_subclasses(classinfo *c)
{
#if defined(ENABLE_THREADS) && !defined(DISABLE_GC)
	threads_cast_stopworld();
#endif
	if (!(c->flags & ACC_INTERFACE)) {
		c->nextsub = NULL;
		c->sub     = NULL;
	}

	if (!(c->flags & ACC_INTERFACE) && (c->super.any != NULL)) {
		c->nextsub        = c->super.cls->sub;
		c->super.cls->sub = c;
	}

	classvalue = 0;

	/* compute class values */

	linker_compute_class_values(class_java_lang_Object);

#if defined(ENABLE_THREADS) && !defined(DISABLE_GC)
	threads_cast_startworld();
#endif
}


/* linker_compute_class_values *************************************************

   XXX

*******************************************************************************/

static void linker_compute_class_values(classinfo *c)
{
	classinfo *subs;

	c->vftbl->baseval = ++classvalue;

	subs = c->sub;

	while (subs) {
		linker_compute_class_values(subs);

		subs = subs->nextsub;
	}

	c->vftbl->diffval = classvalue - c->vftbl->baseval;
}


/* linker_addinterface *********************************************************

   Is needed by link_class for adding a VTBL to a class. All
   interfaces implemented by ic are added as well.

   RETURN VALUE:
      true.........everything ok
	  false........an exception has been thrown

*******************************************************************************/

static bool linker_addinterface(classinfo *c, classinfo *ic)
{
	s4          j, k;
	vftbl_t    *v;
	s4          i;
	classinfo  *sc;
	methodinfo *m;

	v = c->vftbl;
	i = ic->index;

	if (i >= v->interfacetablelength)
		vm_abort("Internal error: interfacetable overflow");

	/* if this interface has already been added, return immediately */

	if (v->interfacetable[-i] != NULL)
		return true;

	if (ic->methodscount == 0) {  /* fake entry needed for subtype test */
		v->interfacevftbllength[i] = 1;
		v->interfacetable[-i]      = MNEW(methodptr, 1);
		v->interfacetable[-i][0]   = NULL;
	}
	else {
		v->interfacevftbllength[i] = ic->methodscount;
		v->interfacetable[-i]      = MNEW(methodptr, ic->methodscount);

#if defined(ENABLE_STATISTICS)
		if (opt_stat)
			count_vftbl_len += sizeof(methodptr) *
				(ic->methodscount + (ic->methodscount == 0));
#endif

		for (j = 0; j < ic->methodscount; j++) {
			for (sc = c; sc != NULL; sc = sc->super.cls) {
				for (k = 0; k < sc->methodscount; k++) {
					m = &(sc->methods[k]);

					if (method_canoverwrite(m, &(ic->methods[j]))) {
						/* method m overwrites the (abstract) method */
#if defined(ENABLE_VERIFIER)
						/* Add loading constraints (for the more
						   general types of the method
						   ic->methods[j]).  */
						if (!classcache_add_constraints_for_params(
									c->classloader, ic->classloader,
									&(ic->methods[j])))
						{
							return false;
						}
#endif

						/* XXX taken from gcj */
						/* check for ACC_STATIC: IncompatibleClassChangeError */

						/* check for !ACC_PUBLIC: IllegalAccessError */

						/* check for ACC_ABSTRACT: AbstracMethodError,
						   not sure about that one */

						v->interfacetable[-i][j] = v->table[m->vftblindex];
						goto foundmethod;
					}
				}
			}

			/* If no method was found, insert the AbstractMethodError
			   stub. */

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
			if (opt_intrp)
				v->interfacetable[-i][j] =
					(methodptr) (ptrint) &intrp_asm_abstractmethoderror;
			else
# endif
				v->interfacetable[-i][j] =
					(methodptr) (ptrint) &asm_abstractmethoderror;
#else
			v->interfacetable[-i][j] =
				(methodptr) (ptrint) &intrp_asm_abstractmethoderror;
#endif

		foundmethod:
			;
		}
	}

	/* add superinterfaces of this interface */

	for (j = 0; j < ic->interfacescount; j++)
		if (!linker_addinterface(c, ic->interfaces[j].cls))
			return false;

	/* everything ok */

	return true;
}


/* class_highestinterface ******************************************************

   Used by the function link_class to determine the amount of memory
   needed for the interface table.

*******************************************************************************/

static s4 class_highestinterface(classinfo *c)
{
	s4 h;
	s4 h2;
	s4 i;
	
    /* check for ACC_INTERFACE bit already done in link_class_intern */

    h = c->index;

	for (i = 0; i < c->interfacescount; i++) {
		h2 = class_highestinterface(c->interfaces[i].cls);

		if (h2 > h)
			h = h2;
	}

	return h;
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

/* src/vmcore/class.c - class related functions

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

   $Id: class.c 7851 2007-05-02 19:57:07Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm/types.h"

#include "arch.h"

#include "mm/memory.h"

#include "threads/lock-common.h"

#include "toolbox/logging.h"

#include "vm/exceptions.h"
#include "vm/global.h"

#include "vm/jit/asmpart.h"

#include "vmcore/class.h"
#include "vmcore/classcache.h"
#include "vmcore/loader.h"
#include "vmcore/options.h"

#if defined(ENABLE_STATISTICS)
# include "vmcore/statistics.h"
#endif

#include "vmcore/suck.h"
#include "vmcore/utf8.h"


/* global variables ***********************************************************/

list_t unlinkedclasses;                 /* this is only used for eager class  */
                                        /* loading                            */


/* frequently used classes ****************************************************/

/* important system classes */

classinfo *class_java_lang_Object;
classinfo *class_java_lang_Class;
classinfo *class_java_lang_ClassLoader;
classinfo *class_java_lang_Cloneable;
classinfo *class_java_lang_SecurityManager;
classinfo *class_java_lang_String;
classinfo *class_java_lang_System;
classinfo *class_java_lang_Thread;
classinfo *class_java_lang_ThreadGroup;
classinfo *class_java_lang_VMSystem;
classinfo *class_java_lang_VMThread;
classinfo *class_java_io_Serializable;


/* system exception classes required in cacao */

classinfo *class_java_lang_Throwable;
classinfo *class_java_lang_Error;
classinfo *class_java_lang_LinkageError;
classinfo *class_java_lang_NoClassDefFoundError;
classinfo *class_java_lang_OutOfMemoryError;
classinfo *class_java_lang_VirtualMachineError;

#if defined(WITH_CLASSPATH_GNU)
classinfo *class_java_lang_VMThrowable;
#endif

classinfo *class_java_lang_Exception;
classinfo *class_java_lang_ClassCastException;
classinfo *class_java_lang_ClassNotFoundException;

#if defined(ENABLE_JAVASE)
classinfo *class_java_lang_Void;
#endif
classinfo *class_java_lang_Boolean;
classinfo *class_java_lang_Byte;
classinfo *class_java_lang_Character;
classinfo *class_java_lang_Short;
classinfo *class_java_lang_Integer;
classinfo *class_java_lang_Long;
classinfo *class_java_lang_Float;
classinfo *class_java_lang_Double;


/* some runtime exception */

classinfo *class_java_lang_NullPointerException;


/* some classes which may be used more often */

#if defined(ENABLE_JAVASE)
classinfo *class_java_lang_StackTraceElement;
classinfo *class_java_lang_reflect_Constructor;
classinfo *class_java_lang_reflect_Field;
classinfo *class_java_lang_reflect_Method;
classinfo *class_java_security_PrivilegedAction;
classinfo *class_java_util_Vector;

classinfo *arrayclass_java_lang_Object;
#endif


/* pseudo classes for the typechecker */

classinfo *pseudo_class_Arraystub;
classinfo *pseudo_class_Null;
classinfo *pseudo_class_New;


/* class_set_packagename *******************************************************

   Derive the package name from the class name and store it in the struct.

*******************************************************************************/

void class_set_packagename(classinfo *c)
{
	char *p = UTF_END(c->name) - 1;
	char *start = c->name->text;

	/* set the package name */
	/* classes in the unnamed package keep packagename == NULL */

	if (c->name->text[0] == '[') {
		/* set packagename of arrays to the element's package */

		for (; *start == '['; start++);

		/* skip the 'L' in arrays of references */
		if (*start == 'L')
			start++;

		for (; (p > start) && (*p != '/'); --p);

		c->packagename = utf_new(start, p - start);

	} else {
		for (; (p > start) && (*p != '/'); --p);

		c->packagename = utf_new(start, p - start);
	}
}


/* class_create_classinfo ******************************************************

   Create a new classinfo struct. The class name is set to the given utf *,
   most other fields are initialized to zero.

   Note: classname may be NULL. In this case a not-yet-named classinfo is
         created. The name must be filled in later and class_set_packagename
		 must be called after that.

*******************************************************************************/

classinfo *class_create_classinfo(utf *classname)
{
	classinfo *c;

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		size_classinfo += sizeof(classinfo);
#endif

	/* we use a safe name for temporarily unnamed classes */

	if (classname == NULL)
		classname = utf_not_named_yet;

#if !defined(NDEBUG)
	if (initverbose)
		log_message_utf("Creating class: ", classname);
#endif

	/* GCNEW_UNCOLLECTABLE clears the allocated memory */
	c = GCNEW_UNCOLLECTABLE(classinfo, 1);

	/*c=NEW(classinfo);*/
	c->name = classname;

	/* Set the header.vftbl of all loaded classes to the one of
       java.lang.Class, so Java code can use a class as object. */

	if (class_java_lang_Class != NULL)
		if (class_java_lang_Class->vftbl != NULL)
			c->object.header.vftbl = class_java_lang_Class->vftbl;

#if defined(ENABLE_JAVASE)
	/* check if the class is a reference class and flag it */

	if (classname == utf_java_lang_ref_SoftReference) {
		c->flags |= ACC_CLASS_SOFT_REFERENCE;
	}
	else if (classname == utf_java_lang_ref_WeakReference) {
		c->flags |= ACC_CLASS_WEAK_REFERENCE;
	}
	else if (classname == utf_java_lang_ref_PhantomReference) {
		c->flags |= ACC_CLASS_PHANTOM_REFERENCE;
	}
#endif

	if (classname != utf_not_named_yet)
		class_set_packagename(c);

	LOCK_INIT_OBJECT_LOCK(&c->object.header);

	return c;
}


/* class_postset_header_vftbl **************************************************

   Set the header.vftbl of all classes created before java.lang.Class
   was linked.  This is necessary that Java code can use a class as
   object.

*******************************************************************************/

void class_postset_header_vftbl(void)
{
	classinfo *c;
	u4 slot;
	classcache_name_entry *nmen;
	classcache_class_entry *clsen;

	assert(class_java_lang_Class);

	for (slot = 0; slot < hashtable_classcache.size; slot++) {
		nmen = (classcache_name_entry *) hashtable_classcache.ptr[slot];

		for (; nmen; nmen = nmen->hashlink) {
			/* iterate over all class entries */

			for (clsen = nmen->classes; clsen; clsen = clsen->next) {
				c = clsen->classobj;

				/* now set the the vftbl */

				if (c->object.header.vftbl == NULL)
					c->object.header.vftbl = class_java_lang_Class->vftbl;
			}
		}
	}
}


/* class_load_attribute_sourcefile *********************************************

   SourceFile_attribute {
       u2 attribute_name_index;
       u4 attribute_length;
	   u2 sourcefile_index;
   }

*******************************************************************************/

static bool class_load_attribute_sourcefile(classbuffer *cb)
{
	classinfo *c;
	u4         attribute_length;
	u2         sourcefile_index;
	utf       *sourcefile;

	/* get classinfo */

	c = cb->class;

	/* check buffer size */

	if (!suck_check_classbuffer_size(cb, 4 + 2))
		return false;

	/* check attribute length */

	attribute_length = suck_u4(cb);

	if (attribute_length != 2) {
		exceptions_throw_classformaterror(c, "Wrong size for VALUE attribute");
		return false;
	}

	/* there can be no more than one SourceFile attribute */

	if (c->sourcefile != NULL) {
		exceptions_throw_classformaterror(c, "Multiple SourceFile attributes");
		return false;
	}

	/* get sourcefile */

	sourcefile_index = suck_u2(cb);
	sourcefile = class_getconstant(c, sourcefile_index, CONSTANT_Utf8);

	if (sourcefile == NULL)
		return false;

	/* store sourcefile */

	c->sourcefile = sourcefile;

	return true;
}


/* class_load_attribute_enclosingmethod ****************************************

   EnclosingMethod_attribute {
       u2 attribute_name_index;
       u4 attribute_length;
	   u2 class_index;
	   u2 method_index;
   }

*******************************************************************************/

#if defined(ENABLE_JAVASE)
static bool class_load_attribute_enclosingmethod(classbuffer *cb)
{
	classinfo             *c;
	u4                     attribute_length;
	u2                     class_index;
	u2                     method_index;
	classref_or_classinfo  cr;
	constant_nameandtype  *cn;

	/* get classinfo */

	c = cb->class;

	/* check buffer size */

	if (!suck_check_classbuffer_size(cb, 4 + 2 + 2))
		return false;

	/* check attribute length */

	attribute_length = suck_u4(cb);

	if (attribute_length != 4) {
		exceptions_throw_classformaterror(c, "Wrong size for VALUE attribute");
		return false;
	}

	/* there can be no more than one EnclosingMethod attribute */

	if (c->enclosingmethod != NULL) {
		exceptions_throw_classformaterror(c, "Multiple EnclosingMethod attributes");
		return false;
	}

	/* get class index */

	class_index = suck_u2(cb);
	cr.ref = innerclass_getconstant(c, class_index, CONSTANT_Class);

	/* get method index */

	method_index = suck_u2(cb);
	cn = innerclass_getconstant(c, method_index, CONSTANT_NameAndType);

	/* store info in classinfo */

	c->enclosingclass.any = cr.any;
	c->enclosingmethod    = cn;

	return true;
}
#endif /* defined(ENABLE_JAVASE) */


/* class_load_attributes *******************************************************

   Read attributes from ClassFile.

   attribute_info {
       u2 attribute_name_index;
       u4 attribute_length;
       u1 info[attribute_length];
   }

   InnerClasses_attribute {
       u2 attribute_name_index;
       u4 attribute_length;
   }

*******************************************************************************/

bool class_load_attributes(classbuffer *cb)
{
	classinfo *c;
	u4         i, j;
	u2         attributes_count;
	u2         attribute_name_index;
	utf       *attribute_name;

	c = cb->class;

	/* get attributes count */

	if (!suck_check_classbuffer_size(cb, 2))
		return false;

	attributes_count = suck_u2(cb);

	for (i = 0; i < attributes_count; i++) {
		/* get attribute name */

		if (!suck_check_classbuffer_size(cb, 2))
			return false;

		attribute_name_index = suck_u2(cb);
		attribute_name =
			class_getconstant(c, attribute_name_index, CONSTANT_Utf8);

		if (attribute_name == NULL)
			return false;

		if (attribute_name == utf_InnerClasses) {
			/* InnerClasses */

			if (c->innerclass != NULL) {
				exceptions_throw_classformaterror(c, "Multiple InnerClasses attributes");
				return false;
			}
				
			if (!suck_check_classbuffer_size(cb, 4 + 2))
				return false;

			/* skip attribute length */
			suck_u4(cb);

			/* number of records */
			c->innerclasscount = suck_u2(cb);

			if (!suck_check_classbuffer_size(cb, (2 + 2 + 2 + 2) * c->innerclasscount))
				return false;

			/* allocate memory for innerclass structure */
			c->innerclass = MNEW(innerclassinfo, c->innerclasscount);

			for (j = 0; j < c->innerclasscount; j++) {
				/* The innerclass structure contains a class with an encoded
				   name, its defining scope, its simple name and a bitmask of
				   the access flags. If an inner class is not a member, its
				   outer_class is NULL, if a class is anonymous, its name is
				   NULL. */
   								
				innerclassinfo *info = c->innerclass + j;

				info->inner_class.ref =
					innerclass_getconstant(c, suck_u2(cb), CONSTANT_Class);
				info->outer_class.ref =
					innerclass_getconstant(c, suck_u2(cb), CONSTANT_Class);
				info->name =
					innerclass_getconstant(c, suck_u2(cb), CONSTANT_Utf8);
				info->flags = suck_u2(cb);
			}
		}
		else if (attribute_name == utf_SourceFile) {
			/* SourceFile */

			if (!class_load_attribute_sourcefile(cb))
				return false;
		}
#if defined(ENABLE_JAVASE)
		else if (attribute_name == utf_EnclosingMethod) {
			/* EnclosingMethod */

			if (!class_load_attribute_enclosingmethod(cb))
				return false;
		}
		else if (attribute_name == utf_Signature) {
			/* Signature */

			if (!loader_load_attribute_signature(cb, &(c->signature)))
				return false;
		}
#if 0
		/* XXX We can't do a release with that enabled */

		else if (attribute_name == utf_RuntimeVisibleAnnotations) {
			/* RuntimeVisibleAnnotations */

			if (!annotation_load_attribute_runtimevisibleannotations(cb))
				return false;
		}
#endif
#endif
		else {
			/* unknown attribute */

			if (!loader_skip_attribute_body(cb))
				return false;
		}
	}

	return true;
}


/* class_freepool **************************************************************

	Frees all resources used by this classes Constant Pool.

*******************************************************************************/

static void class_freecpool(classinfo *c)
{
	u4 idx;
	u4 tag;
	voidptr info;
	
	if (c->cptags && c->cpinfos) {
		for (idx = 0; idx < c->cpcount; idx++) {
			tag = c->cptags[idx];
			info = c->cpinfos[idx];
		
			if (info != NULL) {
				switch (tag) {
				case CONSTANT_Fieldref:
				case CONSTANT_Methodref:
				case CONSTANT_InterfaceMethodref:
					FREE(info, constant_FMIref);
					break;
				case CONSTANT_Integer:
					FREE(info, constant_integer);
					break;
				case CONSTANT_Float:
					FREE(info, constant_float);
					break;
				case CONSTANT_Long:
					FREE(info, constant_long);
					break;
				case CONSTANT_Double:
					FREE(info, constant_double);
					break;
				case CONSTANT_NameAndType:
					FREE(info, constant_nameandtype);
					break;
				}
			}
		}
	}

	if (c->cptags)
		MFREE(c->cptags, u1, c->cpcount);

	if (c->cpinfos)
		MFREE(c->cpinfos, voidptr, c->cpcount);
}


/* class_getconstant ***********************************************************

   Retrieves the value at position 'pos' of the constantpool of a
   class. If the type of the value is other than 'ctype', an error is
   thrown.

*******************************************************************************/

voidptr class_getconstant(classinfo *c, u4 pos, u4 ctype)
{
	/* check index and type of constantpool entry */
	/* (pos == 0 is caught by type comparison) */

	if ((pos >= c->cpcount) || (c->cptags[pos] != ctype)) {
		exceptions_throw_classformaterror(c, "Illegal constant pool index");
		return NULL;
	}

	return c->cpinfos[pos];
}


/* innerclass_getconstant ******************************************************

   Like class_getconstant, but if cptags is ZERO, null is returned.
	
*******************************************************************************/

voidptr innerclass_getconstant(classinfo *c, u4 pos, u4 ctype)
{
	/* invalid position in constantpool */

	if (pos >= c->cpcount) {
		exceptions_throw_classformaterror(c, "Illegal constant pool index");
		return NULL;
	}

	/* constantpool entry of type 0 */	

	if (c->cptags[pos] == 0)
		return NULL;

	/* check type of constantpool entry */

	if (c->cptags[pos] != ctype) {
		exceptions_throw_classformaterror(c, "Illegal constant pool index");
		return NULL;
	}
		
	return c->cpinfos[pos];
}


/* class_free ******************************************************************

   Frees all resources used by the class.

*******************************************************************************/

void class_free(classinfo *c)
{
	s4 i;
	vftbl_t *v;
		
	class_freecpool(c);

	if (c->interfaces)
		MFREE(c->interfaces, classinfo*, c->interfacescount);

	if (c->fields) {
		for (i = 0; i < c->fieldscount; i++)
			field_free(&(c->fields[i]));
#if defined(ENABLE_CACAO_GC)
		MFREE(c->fields, fieldinfo, c->fieldscount);
#endif
	}
	
	if (c->methods) {
		for (i = 0; i < c->methodscount; i++)
			method_free(&(c->methods[i]));
		MFREE(c->methods, methodinfo, c->methodscount);
	}

	if ((v = c->vftbl) != NULL) {
		if (v->arraydesc)
			mem_free(v->arraydesc,sizeof(arraydescriptor));
		
		for (i = 0; i < v->interfacetablelength; i++) {
			MFREE(v->interfacetable[-i], methodptr, v->interfacevftbllength[i]);
		}
		MFREE(v->interfacevftbllength, s4, v->interfacetablelength);

		i = sizeof(vftbl_t) + sizeof(methodptr) * (v->vftbllength - 1) +
		    sizeof(methodptr*) * (v->interfacetablelength -
		                         (v->interfacetablelength > 0));
		v = (vftbl_t*) (((methodptr*) v) -
						(v->interfacetablelength - 1) * (v->interfacetablelength > 1));
		mem_free(v, i);
	}

	if (c->innerclass)
		MFREE(c->innerclass, innerclassinfo, c->innerclasscount);

	/*	if (c->classvftbl)
		mem_free(c->header.vftbl, sizeof(vftbl) + sizeof(methodptr)*(c->vftbl->vftbllength-1)); */
	
/*  	GCFREE(c); */
}


/* get_array_class *************************************************************

   Returns the array class with the given name for the given
   classloader, or NULL if an exception occurred.

   Note: This function does eager loading. 

*******************************************************************************/

static classinfo *get_array_class(utf *name,java_objectheader *initloader,
											java_objectheader *defloader,bool link)
{
	classinfo *c;
	
	/* lookup this class in the classcache */
	c = classcache_lookup(initloader,name);
	if (!c)
		c = classcache_lookup_defined(defloader,name);

	if (!c) {
		/* we have to create it */
		c = class_create_classinfo(name);
		c = load_newly_created_array(c,initloader);
		if (c == NULL)
			return NULL;
	}

	assert(c);
	assert(c->state & CLASS_LOADED);
	assert(c->classloader == defloader);

	if (link && !(c->state & CLASS_LINKED))
		if (!link_class(c))
			return NULL;

	assert(!link || (c->state & CLASS_LINKED));

	return c;
}


/* class_array_of **************************************************************

   Returns an array class with the given component class. The array
   class is dynamically created if neccessary.

*******************************************************************************/

classinfo *class_array_of(classinfo *component, bool link)
{
    s4 namelen;
    char *namebuf;
	s4 dumpsize;
	classinfo *c;

	dumpsize = dump_size();

    /* Assemble the array class name */
    namelen = component->name->blength;
    
    if (component->name->text[0] == '[') {
        /* the component is itself an array */
        namebuf = DMNEW(char, namelen + 1);
        namebuf[0] = '[';
        MCOPY(namebuf + 1, component->name->text, char, namelen);
        namelen++;

    } else {
        /* the component is a non-array class */
        namebuf = DMNEW(char, namelen + 3);
        namebuf[0] = '[';
        namebuf[1] = 'L';
        MCOPY(namebuf + 2, component->name->text, char, namelen);
        namebuf[2 + namelen] = ';';
        namelen += 3;
    }

	c = get_array_class(utf_new(namebuf, namelen),
						component->classloader,
						component->classloader,
						link);

	dump_release(dumpsize);

	return c;
}


/* class_multiarray_of *********************************************************

   Returns an array class with the given dimension and element class.
   The array class is dynamically created if neccessary.

*******************************************************************************/

classinfo *class_multiarray_of(s4 dim, classinfo *element, bool link)
{
    s4 namelen;
    char *namebuf;
	s4 dumpsize;
	classinfo *c;

	dumpsize = dump_size();

	if (dim < 1) {
		log_text("Invalid array dimension requested");
		assert(0);
	}

    /* Assemble the array class name */
    namelen = element->name->blength;
    
    if (element->name->text[0] == '[') {
        /* the element is itself an array */
        namebuf = DMNEW(char, namelen + dim);
        memcpy(namebuf + dim, element->name->text, namelen);
        namelen += dim;
    }
    else {
        /* the element is a non-array class */
        namebuf = DMNEW(char, namelen + 2 + dim);
        namebuf[dim] = 'L';
        memcpy(namebuf + dim + 1, element->name->text, namelen);
        namelen += (2 + dim);
        namebuf[namelen - 1] = ';';
    }
	memset(namebuf, '[', dim);

	c = get_array_class(utf_new(namebuf, namelen),
						element->classloader,
						element->classloader,
						link);

	dump_release(dumpsize);

	return c;
}


/* class_lookup_classref *******************************************************

   Looks up the constant_classref for a given classname in the classref
   tables of a class.

   IN:
       cls..............the class containing the reference
	   name.............the name of the class refered to

    RETURN VALUE:
	   a pointer to a constant_classref, or 
	   NULL if the reference was not found
   
*******************************************************************************/

constant_classref *class_lookup_classref(classinfo *cls, utf *name)
{
	constant_classref *ref;
	extra_classref *xref;
	int count;

	assert(cls);
	assert(name);
	assert(!cls->classrefcount || cls->classrefs);
	
	/* first search the main classref table */
	count = cls->classrefcount;
	ref = cls->classrefs;
	for (; count; --count, ++ref)
		if (ref->name == name)
			return ref;

	/* next try the list of extra classrefs */
	for (xref = cls->extclassrefs; xref; xref = xref->next) {
		if (xref->classref.name == name)
			return &(xref->classref);
	}

	/* not found */
	return NULL;
}


/* class_get_classref **********************************************************

   Returns the constant_classref for a given classname.

   IN:
       cls..............the class containing the reference
	   name.............the name of the class refered to

   RETURN VALUE:
       a pointer to a constant_classref (never NULL)

   NOTE:
       The given name is not checked for validity!
   
*******************************************************************************/

constant_classref *class_get_classref(classinfo *cls, utf *name)
{
	constant_classref *ref;
	extra_classref *xref;

	assert(cls);
	assert(name);

	ref = class_lookup_classref(cls,name);
	if (ref)
		return ref;

	xref = NEW(extra_classref);
	CLASSREF_INIT(xref->classref,cls,name);

	xref->next = cls->extclassrefs;
	cls->extclassrefs = xref;

	return &(xref->classref);
}


/* class_get_self_classref *****************************************************

   Returns the constant_classref to the class itself.

   IN:
       cls..............the class containing the reference

   RETURN VALUE:
       a pointer to a constant_classref (never NULL)

*******************************************************************************/

constant_classref *class_get_self_classref(classinfo *cls)
{
	/* XXX this should be done in a faster way. Maybe always make */
	/* the classref of index 0 a self reference.                  */
	return class_get_classref(cls,cls->name);
}

/* class_get_classref_multiarray_of ********************************************

   Returns an array type reference with the given dimension and element class
   reference.

   IN:
       dim..............the requested dimension
	                    dim must be in [1;255]. This is NOT checked!
	   ref..............the component class reference

   RETURN VALUE:
       a pointer to the class reference for the array type

   NOTE:
       The referer of `ref` is used as the referer for the new classref.

*******************************************************************************/

constant_classref *class_get_classref_multiarray_of(s4 dim, constant_classref *ref)
{
    s4 namelen;
    char *namebuf;
	s4 dumpsize;
	constant_classref *cr;

	assert(ref);
	assert(dim >= 1 && dim <= 255);

	dumpsize = dump_size();

    /* Assemble the array class name */
    namelen = ref->name->blength;
    
    if (ref->name->text[0] == '[') {
        /* the element is itself an array */
        namebuf = DMNEW(char, namelen + dim);
        memcpy(namebuf + dim, ref->name->text, namelen);
        namelen += dim;
    }
    else {
        /* the element is a non-array class */
        namebuf = DMNEW(char, namelen + 2 + dim);
        namebuf[dim] = 'L';
        memcpy(namebuf + dim + 1, ref->name->text, namelen);
        namelen += (2 + dim);
        namebuf[namelen - 1] = ';';
    }
	memset(namebuf, '[', dim);

    cr = class_get_classref(ref->referer,utf_new(namebuf, namelen));

	dump_release(dumpsize);

	return cr;
}


/* class_get_classref_component_of *********************************************

   Returns the component classref of a given array type reference

   IN:
       ref..............the array type reference

   RETURN VALUE:
       a reference to the component class, or
	   NULL if `ref` is not an object array type reference

   NOTE:
       The referer of `ref` is used as the referer for the new classref.

*******************************************************************************/

constant_classref *class_get_classref_component_of(constant_classref *ref)
{
	s4 namelen;
	char *name;
	
	assert(ref);

	name = ref->name->text;
	if (*name++ != '[')
		return NULL;
	
	namelen = ref->name->blength - 1;
	if (*name == 'L') {
		name++;
		namelen -= 2;
	}
	else if (*name != '[') {
		return NULL;
	}

    return class_get_classref(ref->referer, utf_new(name, namelen));
}


/* class_findmethod ************************************************************
	
   Searches a 'classinfo' structure for a method having the given name
   and descriptor. If descriptor is NULL, it is ignored.

*******************************************************************************/

methodinfo *class_findmethod(classinfo *c, utf *name, utf *desc)
{
	methodinfo *m;
	s4          i;

	for (i = 0; i < c->methodscount; i++) {
		m = &(c->methods[i]);

		if ((m->name == name) && ((desc == NULL) || (m->descriptor == desc)))
			return m;
	}

	return NULL;
}


/* class_resolvemethod *********************************************************
	
   Searches a class and it's super classes for a method.

   Superinterfaces are *not* searched.

*******************************************************************************/

methodinfo *class_resolvemethod(classinfo *c, utf *name, utf *desc)
{
	methodinfo *m;

	while (c) {
		m = class_findmethod(c, name, desc);

		if (m)
			return m;

		/* JVM Specification bug: 

		   It is important NOT to resolve special <init> and <clinit>
		   methods to super classes or interfaces; yet, this is not
		   explicited in the specification.  Section 5.4.3.3 should be
		   updated appropriately.  */

		if (name == utf_init || name == utf_clinit)
			return NULL;

		c = c->super.cls;
	}

	return NULL;
}


/* class_resolveinterfacemethod_intern *****************************************

   Internally used helper function. Do not use this directly.

*******************************************************************************/

static methodinfo *class_resolveinterfacemethod_intern(classinfo *c,
													   utf *name, utf *desc)
{
	methodinfo *m;
	s4          i;

	/* try to find the method in the class */

	m = class_findmethod(c, name, desc);

	if (m != NULL)
		return m;

	/* no method found? try the superinterfaces */

	for (i = 0; i < c->interfacescount; i++) {
		m = class_resolveinterfacemethod_intern(c->interfaces[i].cls,
													name, desc);

		if (m != NULL)
			return m;
	}

	/* no method found */

	return NULL;
}


/* class_resolveclassmethod ****************************************************
	
   Resolves a reference from REFERER to a method with NAME and DESC in
   class C.

   If the method cannot be resolved the return value is NULL. If
   EXCEPT is true *exceptionptr is set, too.

*******************************************************************************/

methodinfo *class_resolveclassmethod(classinfo *c, utf *name, utf *desc,
									 classinfo *referer, bool throwexception)
{
	classinfo  *cls;
	methodinfo *m;
	s4          i;

/*  	if (c->flags & ACC_INTERFACE) { */
/*  		if (throwexception) */
/*  			*exceptionptr = */
/*  				new_exception(string_java_lang_IncompatibleClassChangeError); */
/*  		return NULL; */
/*  	} */

	/* try class c and its superclasses */

	cls = c;

	m = class_resolvemethod(cls, name, desc);

	if (m != NULL)
		goto found;

	/* try the superinterfaces */

	for (i = 0; i < c->interfacescount; i++) {
		m = class_resolveinterfacemethod_intern(c->interfaces[i].cls,
												name, desc);

		if (m != NULL)
			goto found;
	}
	
	if (throwexception)
		exceptions_throw_nosuchmethoderror(c, name, desc);

	return NULL;

 found:
	if ((m->flags & ACC_ABSTRACT) && !(c->flags & ACC_ABSTRACT)) {
		if (throwexception)
			exceptions_throw_abstractmethoderror();

		return NULL;
	}

	/* XXX check access rights */

	return m;
}


/* class_resolveinterfacemethod ************************************************

   Resolves a reference from REFERER to a method with NAME and DESC in
   interface C.

   If the method cannot be resolved the return value is NULL. If
   EXCEPT is true *exceptionptr is set, too.

*******************************************************************************/

methodinfo *class_resolveinterfacemethod(classinfo *c, utf *name, utf *desc,
										 classinfo *referer, bool throwexception)
{
	methodinfo *mi;

	if (!(c->flags & ACC_INTERFACE)) {
		if (throwexception)
			exceptions_throw_incompatibleclasschangeerror(c, "Not an interface");

		return NULL;
	}

	mi = class_resolveinterfacemethod_intern(c, name, desc);

	if (mi != NULL)
		return mi;

	/* try class java.lang.Object */

	mi = class_findmethod(class_java_lang_Object, name, desc);

	if (mi != NULL)
		return mi;

	if (throwexception)
		exceptions_throw_nosuchmethoderror(c, name, desc);

	return NULL;
}


/* class_findfield *************************************************************
	
   Searches for field with specified name and type in a classinfo
   structure. If no such field is found NULL is returned.

*******************************************************************************/

fieldinfo *class_findfield(classinfo *c, utf *name, utf *desc)
{
	s4 i;

	for (i = 0; i < c->fieldscount; i++)
		if ((c->fields[i].name == name) && (c->fields[i].descriptor == desc))
			return &(c->fields[i]);

	if (c->super.cls)
		return class_findfield(c->super.cls, name, desc);

	return NULL;
}


/* class_findfield_approx ******************************************************
	
   Searches in 'classinfo'-structure for a field with the specified
   name.

*******************************************************************************/
 
fieldinfo *class_findfield_by_name(classinfo *c, utf *name)
{
	s4 i;

	/* get field index */

	i = class_findfield_index_by_name(c, name);

	/* field was not found, return */

	if (i == -1)
		return NULL;

	/* return field address */

	return &(c->fields[i]);
}


s4 class_findfield_index_by_name(classinfo *c, utf *name)
{
	s4 i;

	for (i = 0; i < c->fieldscount; i++) {
		/* compare field names */

		if ((c->fields[i].name == name))
			return i;
	}

	/* field was not found, raise exception */	

	exceptions_throw_nosuchfielderror(c, name);

	return -1;
}


/****************** Function: class_resolvefield_int ***************************

    This is an internally used helper function. Do not use this directly.

	Tries to resolve a field having the given name and type.
    If the field cannot be resolved, NULL is returned.

*******************************************************************************/

static fieldinfo *class_resolvefield_int(classinfo *c, utf *name, utf *desc)
{
	fieldinfo *fi;
	s4         i;

	/* search for field in class c */

	for (i = 0; i < c->fieldscount; i++) { 
		if ((c->fields[i].name == name) && (c->fields[i].descriptor == desc)) {
			return &(c->fields[i]);
		}
    }

	/* try superinterfaces recursively */

	for (i = 0; i < c->interfacescount; i++) {
		fi = class_resolvefield_int(c->interfaces[i].cls, name, desc);
		if (fi)
			return fi;
	}

	/* try superclass */

	if (c->super.cls)
		return class_resolvefield_int(c->super.cls, name, desc);

	/* not found */

	return NULL;
}


/********************* Function: class_resolvefield ***************************
	
	Resolves a reference from REFERER to a field with NAME and DESC in class C.

    If the field cannot be resolved the return value is NULL. If EXCEPT is
    true *exceptionptr is set, too.

*******************************************************************************/

fieldinfo *class_resolvefield(classinfo *c, utf *name, utf *desc,
							  classinfo *referer, bool throwexception)
{
	fieldinfo *fi;

	fi = class_resolvefield_int(c, name, desc);

	if (!fi) {
		if (throwexception)
			exceptions_throw_nosuchfielderror(c, name);

		return NULL;
	}

	/* XXX check access rights */

	return fi;
}


/* class_issubclass ************************************************************

   Checks if sub is a descendant of super.
	
*******************************************************************************/

bool class_issubclass(classinfo *sub, classinfo *super)
{
	for (;;) {
		if (!sub)
			return false;

		if (sub == super)
			return true;

		sub = sub->super.cls;
	}
}


/* class_isanysubclass *********************************************************

   Checks a subclass relation between two classes. Implemented
   interfaces are interpreted as super classes.

   Return value: 1 ... sub is subclass of super
                 0 ... otherwise

*******************************************************************************/

bool class_isanysubclass(classinfo *sub, classinfo *super)
{
	castinfo classvalues;
	u4       diffval;
	bool     result;

	/* This is the trivial case. */

	if (sub == super)
		return true;

	/* Primitive classes are only subclasses of themselves. */

	if ((sub->flags & ACC_CLASS_PRIMITIVE) ||
		(super->flags & ACC_CLASS_PRIMITIVE))
		return false;

	/* Check for interfaces. */

	if (super->flags & ACC_INTERFACE) {
		result = (sub->vftbl->interfacetablelength > super->index) &&
			(sub->vftbl->interfacetable[-super->index] != NULL);
	}
	else {
		/* java.lang.Object is the only super class of any
		   interface. */

		if (sub->flags & ACC_INTERFACE)
			return (super == class_java_lang_Object);

		ASM_GETCLASSVALUES_ATOMIC(super->vftbl, sub->vftbl, &classvalues);

		diffval = classvalues.sub_baseval - classvalues.super_baseval;
		result  = diffval <= (u4) classvalues.super_diffval;
	}

	return result;
}


/* class_printflags ************************************************************

   Prints flags of a class.

*******************************************************************************/

#if !defined(NDEBUG)
void class_printflags(classinfo *c)
{
	if (c == NULL) {
		printf("NULL");
		return;
	}

	if (c->flags & ACC_PUBLIC)       printf(" PUBLIC");
	if (c->flags & ACC_PRIVATE)      printf(" PRIVATE");
	if (c->flags & ACC_PROTECTED)    printf(" PROTECTED");
	if (c->flags & ACC_STATIC)       printf(" STATIC");
	if (c->flags & ACC_FINAL)        printf(" FINAL");
	if (c->flags & ACC_SYNCHRONIZED) printf(" SYNCHRONIZED");
	if (c->flags & ACC_VOLATILE)     printf(" VOLATILE");
	if (c->flags & ACC_TRANSIENT)    printf(" TRANSIENT");
	if (c->flags & ACC_NATIVE)       printf(" NATIVE");
	if (c->flags & ACC_INTERFACE)    printf(" INTERFACE");
	if (c->flags & ACC_ABSTRACT)     printf(" ABSTRACT");
}
#endif


/* class_print *****************************************************************

   Prints classname plus flags.

*******************************************************************************/

#if !defined(NDEBUG)
void class_print(classinfo *c)
{
	if (c == NULL) {
		printf("NULL");
		return;
	}

	utf_display_printable_ascii(c->name);
	class_printflags(c);
}
#endif


/* class_classref_print ********************************************************

   Prints classname plus referer class.

*******************************************************************************/

#if !defined(NDEBUG)
void class_classref_print(constant_classref *cr)
{
	if (cr == NULL) {
		printf("NULL");
		return;
	}

	utf_display_printable_ascii(cr->name);
	printf("(ref.by ");
	if (cr->referer)
		class_print(cr->referer);
	else
		printf("NULL");
	printf(")");
}
#endif


/* class_println ***************************************************************

   Prints classname plus flags and new line.

*******************************************************************************/

#if !defined(NDEBUG)
void class_println(classinfo *c)
{
	class_print(c);
	printf("\n");
}
#endif


/* class_classref_println ******************************************************

   Prints classname plus referer class and new line.

*******************************************************************************/

#if !defined(NDEBUG)
void class_classref_println(constant_classref *cr)
{
	class_classref_print(cr);
	printf("\n");
}
#endif


/* class_classref_or_classinfo_print *******************************************

   Prints classname plus referer class.

*******************************************************************************/

#if !defined(NDEBUG)
void class_classref_or_classinfo_print(classref_or_classinfo c)
{
	if (c.any == NULL) {
		printf("(classref_or_classinfo) NULL");
		return;
	}
	if (IS_CLASSREF(c))
		class_classref_print(c.ref);
	else
		class_print(c.cls);
}
#endif


/* class_classref_or_classinfo_println *****************************************

   Prints classname plus referer class and a newline.

*******************************************************************************/

void class_classref_or_classinfo_println(classref_or_classinfo c)
{
	class_classref_or_classinfo_println(c);
	printf("\n");
}


/* class_showconstantpool ******************************************************

   Dump the constant pool of the given class to stdout.

*******************************************************************************/

#if !defined(NDEBUG)
void class_showconstantpool (classinfo *c) 
{
	u4 i;
	voidptr e;

	printf ("---- dump of constant pool ----\n");

	for (i=0; i<c->cpcount; i++) {
		printf ("#%d:  ", (int) i);
		
		e = c -> cpinfos [i];
		if (e) {
			
			switch (c -> cptags [i]) {
			case CONSTANT_Class:
				printf ("Classreference -> ");
				utf_display_printable_ascii ( ((constant_classref*)e) -> name );
				break;
			case CONSTANT_Fieldref:
				printf ("Fieldref -> ");
				field_fieldref_print((constant_FMIref *) e);
				break;
			case CONSTANT_Methodref:
				printf ("Methodref -> ");
				method_methodref_print((constant_FMIref *) e);
				break;
			case CONSTANT_InterfaceMethodref:
				printf ("InterfaceMethod -> ");
				method_methodref_print((constant_FMIref *) e);
				break;
			case CONSTANT_String:
				printf ("String -> ");
				utf_display_printable_ascii (e);
				break;
			case CONSTANT_Integer:
				printf ("Integer -> %d", (int) ( ((constant_integer*)e) -> value) );
				break;
			case CONSTANT_Float:
				printf ("Float -> %f", ((constant_float*)e) -> value);
				break;
			case CONSTANT_Double:
				printf ("Double -> %f", ((constant_double*)e) -> value);
				break;
			case CONSTANT_Long:
				{
					u8 v = ((constant_long*)e) -> value;
#if U8_AVAILABLE
					printf ("Long -> %ld", (long int) v);
#else
					printf ("Long -> HI: %ld, LO: %ld\n", 
							(long int) v.high, (long int) v.low);
#endif 
				}
				break;
			case CONSTANT_NameAndType:
				{
					constant_nameandtype *cnt = e;
					printf ("NameAndType: ");
					utf_display_printable_ascii (cnt->name);
					printf (" ");
					utf_display_printable_ascii (cnt->descriptor);
				}
				break;
			case CONSTANT_Utf8:
				printf ("Utf8 -> ");
				utf_display_printable_ascii (e);
				break;
			default: 
				log_text("Invalid type of ConstantPool-Entry");
				assert(0);
			}
		}

		printf ("\n");
	}
}
#endif /* !defined(NDEBUG) */


/* class_showmethods ***********************************************************

   Dump info about the fields and methods of the given class to stdout.

*******************************************************************************/

#if !defined(NDEBUG)
void class_showmethods (classinfo *c)
{
	s4 i;
	
	printf("--------- Fields and Methods ----------------\n");
	printf("Flags: ");
	class_printflags(c);
	printf("\n");

	printf("This: ");
	utf_display_printable_ascii(c->name);
	printf("\n");

	if (c->super.cls) {
		printf("Super: ");
		utf_display_printable_ascii(c->super.cls->name);
		printf ("\n");
	}

	printf("Index: %d\n", c->index);
	
	printf("Interfaces:\n");	
	for (i = 0; i < c->interfacescount; i++) {
		printf("   ");
		utf_display_printable_ascii(c->interfaces[i].cls->name);
		printf (" (%d)\n", c->interfaces[i].cls->index);
	}

	printf("Fields:\n");
	for (i = 0; i < c->fieldscount; i++)
		field_println(&(c->fields[i]));

	printf("Methods:\n");
	for (i = 0; i < c->methodscount; i++) {
		methodinfo *m = &(c->methods[i]);

		if (!(m->flags & ACC_STATIC))
			printf("vftblindex: %d   ", m->vftblindex);

		method_println(m);
	}

	printf ("Virtual function table:\n");
	for (i = 0; i < c->vftbl->vftbllength; i++)
		printf ("entry: %d,  %ld\n", i, (long int) (c->vftbl->table[i]));
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

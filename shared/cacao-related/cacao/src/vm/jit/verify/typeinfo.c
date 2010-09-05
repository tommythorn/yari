/* src/vm/jit/verify/typeinfo.c - type system used by the type checker

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

   $Id: typeinfo.c 7743 2007-04-17 20:53:41Z edwin $

*/


#include "config.h"

#include <assert.h>
#include <string.h>

#include "mm/memory.h"
#include "toolbox/logging.h"

#include "vm/exceptions.h"

#include "vm/jit/jit.h"
#include "vm/jit/verify/typeinfo.h"

#include "vmcore/class.h"
#include "vmcore/descriptor.h"
#include "vmcore/loader.h"
#include "vm/resolve.h"


/* check if a linked class is an array class. Only use for linked classes! */
#define CLASSINFO_IS_ARRAY(clsinfo)  ((clsinfo)->vftbl->arraydesc != NULL)

/* check if a linked class implements the interface with the given index */
#define CLASSINFO_IMPLEMENTS_INTERFACE(cls,index)                   \
    ( ((index) < (cls)->vftbl->interfacetablelength)            \
      && ( (cls)->vftbl->interfacetable[-(index)] != NULL ) )

/******************************************************************************/
/* DEBUG HELPERS                                                              */
/******************************************************************************/

#ifdef TYPEINFO_DEBUG
#define TYPEINFO_ASSERT(cond)  assert(cond)
#else
#define TYPEINFO_ASSERT(cond)
#endif

/**********************************************************************/
/* TYPEVECTOR FUNCTIONS                                               */
/**********************************************************************/

#if defined(ENABLE_VERIFIER)

/* typevector_copy *************************************************************
 
   Return a copy of the given typevector.
  
   IN:
	   src..............typevector set to copy, must be != NULL
	   size.............number of elements per typevector

   RETURN VALUE:
       a pointer to the new typevector set

*******************************************************************************/

varinfo *
typevector_copy(varinfo *src, int size)
{
	varinfo *dst;
	
	TYPEINFO_ASSERT(src);
	
	dst = DNEW_TYPEVECTOR(size);
	memcpy(dst,src,TYPEVECTOR_SIZE(size));

	return dst;
}

/* typevector_copy_inplace *****************************************************
 
   Copy a typevector to a given destination.

   IN:
	   src..............typevector to copy, must be != NULL
	   dst..............destination to write the copy to
	   size.............number of elements per typevector

*******************************************************************************/

void
typevector_copy_inplace(varinfo *src,varinfo *dst,int size)
{
	memcpy(dst,src,TYPEVECTOR_SIZE(size));
}

/* typevector_checktype ********************************************************
 
   Check if the typevector contains a given type at a given index.
  
   IN:
	   vec..............typevector set, must be != NULL
	   index............index of component to check
	   type.............TYPE_* constant to check against

   RETURN VALUE:
       true if the typevector contains TYPE at INDEX,
	   false otherwise

*******************************************************************************/

bool
typevector_checktype(varinfo *vec,int index,int type)
{
	TYPEINFO_ASSERT(vec);

	return vec[index].type == type;
}

/* typevector_checkreference ***************************************************
 
   Check if the typevector contains a reference at a given index.
  
   IN:
	   vec..............typevector, must be != NULL
	   index............index of component to check

   RETURN VALUE:
       true if the typevector contains a reference at INDEX,
	   false otherwise

*******************************************************************************/

bool
typevector_checkreference(varinfo *vec, int index)
{
	TYPEINFO_ASSERT(vec);
	return TYPEDESC_IS_REFERENCE(vec[index]);
}

/* typevectorset_checkretaddr **************************************************
 
   Check if the typevectors contains a returnAddress at a given index.
  
   IN:
	   vec..............typevector, must be != NULL
	   index............index of component to check

   RETURN VALUE:
       true if the typevector contains a returnAddress at INDEX,
	   false otherwise

*******************************************************************************/

bool
typevector_checkretaddr(varinfo *vec,int index)
{
	TYPEINFO_ASSERT(vec);
	return TYPEDESC_IS_RETURNADDRESS(vec[index]);
}

/* typevector_store ************************************************************
 
   Store a type at a given index in the typevector.
  
   IN:
	   vec..............typevector set, must be != NULL
	   index............index of component to set
	   type.............TYPE_* constant of type to set
	   info.............typeinfo of type to set, may be NULL, 
	                    if TYPE != TYPE_ADR

*******************************************************************************/

void
typevector_store(varinfo *vec,int index,int type,typeinfo *info)
{
	TYPEINFO_ASSERT(vec);

	vec[index].type = type;
	if (info)
		TYPEINFO_COPY(*info,vec[index].typeinfo);
}

/* typevector_store_retaddr ****************************************************
 
   Store a returnAddress type at a given index in the typevector.
  
   IN:
	   vec..............typevector set, must be != NULL
	   index............index of component to set
	   info.............typeinfo of the returnAddress.

*******************************************************************************/

void
typevector_store_retaddr(varinfo *vec,int index,typeinfo *info)
{
	TYPEINFO_ASSERT(vec);
	TYPEINFO_ASSERT(TYPEINFO_IS_PRIMITIVE(*info));
	
	vec[index].type = TYPE_ADR;
	TYPEINFO_INIT_RETURNADDRESS(vec[index].typeinfo,
			TYPEINFO_RETURNADDRESS(*info));
}

/* typevector_init_object ******************************************************
 
   Replace all uninitialized object types in the typevector set which were 
   created by the given instruction by initialized object types.
  
   IN:
	   set..............typevector set
	   ins..............instruction which created the uninitialized object type
	   initclass........class of the initialized object type to set
	   size.............number of elements per typevector

   RETURN VALUE:
       true.............success
	   false............an exception has been thrown

   XXX maybe we should do the lazy resolving before calling this function

*******************************************************************************/

bool
typevector_init_object(varinfo *set,void *ins,
					   classref_or_classinfo initclass,
					   int size)
{
	int i;

	for (i=0; i<size; ++i) {
		if (set[i].type == TYPE_ADR
			&& TYPEINFO_IS_NEWOBJECT(set[i].typeinfo)
			&& TYPEINFO_NEWOBJECT_INSTRUCTION(set[i].typeinfo) == ins)
		{
			if (!typeinfo_init_class(&(set[i].typeinfo),initclass))
				return false;
		}
	}
	return true;
}

/* typevector_merge ************************************************************
 
   Merge a typevector with another one.
   The given typevectors must have the same number of components.
  
   IN:
       m................method for exception messages
	   dst..............the first typevector
	   y................the second typevector
	   size.............number of elements per typevector

   OUT:
       *dst.............the resulting typevector

   RETURN VALUE:
       typecheck_TRUE...dst has been modified
	   typecheck_FALSE..dst has not been modified
	   typecheck_FAIL...an exception has been thrown

*******************************************************************************/

typecheck_result
typevector_merge(methodinfo *m,varinfo *dst,varinfo *y,int size)
{
	bool changed = false;
	typecheck_result r;
	
	varinfo *a = dst;
	varinfo *b = y;
	while (size--) {
		if (a->type != TYPE_VOID && a->type != b->type) {
			a->type = TYPE_VOID;
			changed = true;
		}
		else if (a->type == TYPE_ADR) {
			if (TYPEINFO_IS_PRIMITIVE(a->typeinfo)) {
				/* 'a' is a returnAddress */
				if (!TYPEINFO_IS_PRIMITIVE(b->typeinfo)
					|| (TYPEINFO_RETURNADDRESS(a->typeinfo)
						!= TYPEINFO_RETURNADDRESS(b->typeinfo)))
				{
					a->type = TYPE_VOID;
					changed = true;
				}
			}
			else {
				/* 'a' is a reference */
				if (TYPEINFO_IS_PRIMITIVE(b->typeinfo)) {
					a->type = TYPE_VOID;
					changed = true;
				}
				else {
					/* two reference types are merged. There cannot be */
					/* a merge error. In the worst case we get j.l.O.  */
					r = typeinfo_merge(m,&(a->typeinfo),&(b->typeinfo));
					if (r == typecheck_FAIL)
						return r;
					changed |= r;
				}
			}
		}
		a++;
		b++;
	}
	return changed;
}

/**********************************************************************/
/* READ-ONLY FUNCTIONS                                                */
/* The following functions don't change typeinfo data.                */
/**********************************************************************/

/* typeinfo_is_array ***********************************************************
 
   Check whether a typeinfo describes an array type.
   
   IN:
	   info.............the typeinfo, must be != NULL

   RETURN VALUE:
       true if INFO describes an array type.

*******************************************************************************/

bool
typeinfo_is_array(typeinfo *info)
{
	TYPEINFO_ASSERT(info);
    return TYPEINFO_IS_ARRAY(*info);
}

/* typeinfo_is_primitive_array *************************************************
 
   Check whether a typeinfo describes a primitive array type.
   
   IN:
	   info.............the typeinfo, must be != NULL

   RETURN VALUE:
       true if INFO describes an array of a primitive type.

*******************************************************************************/

bool
typeinfo_is_primitive_array(typeinfo *info,int arraytype)
{
	TYPEINFO_ASSERT(info);
    return TYPEINFO_IS_PRIMITIVE_ARRAY(*info,arraytype);
}

/* typeinfo_is_array_of_refs ***************************************************
 
   Check whether a typeinfo describes an array of references type.
   
   IN:
	   info.............the typeinfo, must be != NULL

   RETURN VALUE:
       true if INFO describes an array of a refrence type.

*******************************************************************************/

bool
typeinfo_is_array_of_refs(typeinfo *info)
{
	TYPEINFO_ASSERT(info);
    return TYPEINFO_IS_ARRAY_OF_REFS(*info);
}

/* interface_extends_interface *************************************************
 
   Check if a resolved interface extends a given resolved interface.
   
   IN:
	   cls..............the interface, must be linked
	   interf...........the interface to check against

   RETURN VALUE:
       true.............CLS extends INTERF
	   false............CLS does not extend INTERF

*******************************************************************************/

static bool
interface_extends_interface(classinfo *cls,classinfo *interf)
{
    int i;
    
	TYPEINFO_ASSERT(cls);
	TYPEINFO_ASSERT(interf);
	TYPEINFO_ASSERT((interf->flags & ACC_INTERFACE) != 0);
	TYPEINFO_ASSERT((cls->flags & ACC_INTERFACE) != 0);
	TYPEINFO_ASSERT(cls->state & CLASS_LINKED);

    /* first check direct superinterfaces */
    for (i=0; i<cls->interfacescount; ++i) {
        if (cls->interfaces[i].cls == interf)
            return true;
    }
    
    /* check indirect superinterfaces */
    for (i=0; i<cls->interfacescount; ++i) {
        if (interface_extends_interface(cls->interfaces[i].cls,interf))
            return true;
    }
    
    return false;
}

/* classinfo_implements_interface **********************************************
 
   Check if a resolved class implements a given resolved interface.
   
   IN:
	   cls..............the class
	   interf...........the interface

   RETURN VALUE:
       typecheck_TRUE...CLS implements INTERF
	   typecheck_FALSE..CLS does not implement INTERF
	   typecheck_FAIL...an exception has been thrown

*******************************************************************************/

static typecheck_result
classinfo_implements_interface(classinfo *cls,classinfo *interf)
{
	TYPEINFO_ASSERT(cls);
	TYPEINFO_ASSERT(interf);
	TYPEINFO_ASSERT((interf->flags & ACC_INTERFACE) != 0);

	if (!(cls->state & CLASS_LINKED))
		if (!link_class(cls))
			return typecheck_FAIL;

    if (cls->flags & ACC_INTERFACE) {
        /* cls is an interface */
        if (cls == interf)
            return typecheck_TRUE;

        /* check superinterfaces */
        return interface_extends_interface(cls,interf);
    }

	TYPEINFO_ASSERT(cls->state & CLASS_LINKED);
    return CLASSINFO_IMPLEMENTS_INTERFACE(cls,interf->index);
}

/* mergedlist_implements_interface *********************************************
 
   Check if all the classes in a given merged list implement a given resolved
   interface.
   
   IN:
	   merged...........the list of merged class types
	   interf...........the interface to check against

   RETURN VALUE:
       typecheck_TRUE...all classes implement INTERF
	   typecheck_FALSE..there is at least one class that does not implement
	                    INTERF
	   typecheck_MAYBE..check cannot be performed now because of unresolved
	                    classes
	   typecheck_FAIL...an exception has been thrown

*******************************************************************************/

static typecheck_result
mergedlist_implements_interface(typeinfo_mergedlist *merged,
                                classinfo *interf)
{
    int i;
    classref_or_classinfo *mlist;
	typecheck_result r;
    
	TYPEINFO_ASSERT(interf);
	TYPEINFO_ASSERT((interf->flags & ACC_INTERFACE) != 0);

    /* Check if there is an non-empty mergedlist. */
    if (!merged)
        return typecheck_FALSE;

    /* If all classinfos in the (non-empty) merged array implement the
     * interface return true, otherwise false.
     */
    mlist = merged->list;
    i = merged->count;
    while (i--) {
		if (IS_CLASSREF(*mlist)) {
			return typecheck_MAYBE;
		}
        r = classinfo_implements_interface((mlist++)->cls,interf);
        if (r != typecheck_TRUE)
			return r;
    }
    return typecheck_TRUE;
}

/* merged_implements_interface *************************************************
 
   Check if a possible merged type implements a given resolved interface
   interface.
   
   IN:
       typeclass........(common) class of the (merged) type
	   merged...........the list of merged class types
	   interf...........the interface to check against

   RETURN VALUE:
       typecheck_TRUE...the type implement INTERF
	   typecheck_FALSE..the type does not implement INTERF
	   typecheck_MAYBE..check cannot be performed now because of unresolved
	                    classes
	   typecheck_FAIL...an exception has been thrown

*******************************************************************************/

static typecheck_result
merged_implements_interface(classinfo *typeclass,typeinfo_mergedlist *merged,
                            classinfo *interf)
{
	typecheck_result r;
	
    /* primitive types don't support interfaces. */
    if (!typeclass)
        return typecheck_FALSE;

    /* the null type can be cast to any interface type. */
    if (typeclass == pseudo_class_Null)
        return typecheck_TRUE;

    /* check if typeclass implements the interface. */
    r = classinfo_implements_interface(typeclass,interf);
	if (r != typecheck_FALSE)
        return r;

    /* check the mergedlist */
	if (!merged)
		return typecheck_FALSE;
    return mergedlist_implements_interface(merged,interf);
}

/* merged_is_subclass **********************************************************
 
   Check if a possible merged type is a subclass of a given class.
   A merged type is a subclass of a class C if all types in the merged list
   are subclasses of C. A sufficient condition for this is that the
   common type of the merged type is a subclass of C.

   IN:
       typeclass........(common) class of the (merged) type
	                    MUST be a loaded and linked class
	   merged...........the list of merged class types
	   cls..............the class to theck against

   RETURN VALUE:
       typecheck_TRUE...the type is a subclass of CLS
	   typecheck_FALSE..the type is not a subclass of CLS
	   typecheck_MAYBE..check cannot be performed now because of unresolved
	                    classes
	   typecheck_FAIL...an exception has been thrown

*******************************************************************************/

static typecheck_result
merged_is_subclass(classinfo *typeclass,typeinfo_mergedlist *merged,
		classinfo *cls)
{
    int i;
    classref_or_classinfo *mlist;

	TYPEINFO_ASSERT(cls);
	
    /* primitive types aren't subclasses of anything. */
    if (!typeclass)
        return typecheck_FALSE;

    /* the null type can be cast to any reference type. */
    if (typeclass == pseudo_class_Null)
        return typecheck_TRUE;

	TYPEINFO_ASSERT(typeclass->state & CLASS_LOADED);
	TYPEINFO_ASSERT(typeclass->state & CLASS_LINKED);

    /* check if the common typeclass is a subclass of CLS. */
	if (class_issubclass(typeclass,cls))
		return typecheck_TRUE;
	
    /* check the mergedlist */
	if (!merged)
		return typecheck_FALSE;
    /* If all classinfos in the (non-empty) merged list are subclasses
	 * of CLS, return true, otherwise false.
	 * If there is at least one unresolved type in the list,
	 * return typecheck_MAYBE.
     */
    mlist = merged->list;
    i = merged->count;
    while (i--) {
		if (IS_CLASSREF(*mlist)) {
			return typecheck_MAYBE;
		}
		if (!(mlist->cls->state & CLASS_LINKED))
			if (!link_class(mlist->cls))
				return typecheck_FAIL;
		if (!class_issubclass(mlist->cls,cls))
			return typecheck_FALSE;
		mlist++;
    }
    return typecheck_TRUE;
}

/* typeinfo_is_assignable_to_class *********************************************
 
   Check if a type is assignable to a given class type.
   
   IN:
       value............the type of the value
	   dest.............the type of the destination

   RETURN VALUE:
       typecheck_TRUE...the type is assignable
	   typecheck_FALSE..the type is not assignable
	   typecheck_MAYBE..check cannot be performed now because of unresolved
	                    classes
	   typecheck_FAIL...an exception has been thrown

*******************************************************************************/

typecheck_result
typeinfo_is_assignable_to_class(typeinfo *value,classref_or_classinfo dest)
{
	classref_or_classinfo c;
    classinfo *cls;
	utf *classname;

	TYPEINFO_ASSERT(value);

    c = value->typeclass;

    /* assignments of primitive values are not checked here. */
    if (!c.any && !dest.any)
        return typecheck_TRUE;

    /* primitive and reference types are not assignment compatible. */
    if (!c.any || !dest.any)
        return typecheck_FALSE;

    /* the null type can be assigned to any type */
    if (TYPEINFO_IS_NULLTYPE(*value))
        return typecheck_TRUE;

    /* uninitialized objects are not assignable */
    if (TYPEINFO_IS_NEWOBJECT(*value))
        return typecheck_FALSE;

	if (IS_CLASSREF(c)) {
		/* The value type is an unresolved class reference. */
		classname = c.ref->name;
	}
	else {
		classname = c.cls->name;
	}

	if (IS_CLASSREF(dest)) {
		/* the destination type is an unresolved class reference */
		/* In this case we cannot tell a lot about assignability. */

		/* the common case of value and dest type having the same classname */
		if (dest.ref->name == classname && !value->merged)
			return typecheck_TRUE;

		/* we cannot tell if value is assignable to dest, so we */
		/* leave it up to the resolving code to check this      */
		return typecheck_MAYBE;
	}

	/* { we know that dest is a loaded class } */

	if (IS_CLASSREF(c)) {
		/* the value type is an unresolved class reference */
		
		/* the common case of value and dest type having the same classname */
		if (dest.cls->name == classname)
			return typecheck_TRUE;

		/* we cannot tell if value is assignable to dest, so we */
		/* leave it up to the resolving code to check this      */
		return typecheck_MAYBE;
	}

	/* { we know that both c and dest are loaded classes } */
	/* (c may still have a merged list containing unresolved classrefs!) */

	TYPEINFO_ASSERT(!IS_CLASSREF(c));
	TYPEINFO_ASSERT(!IS_CLASSREF(dest));

	cls = c.cls;
	
	TYPEINFO_ASSERT(cls->state & CLASS_LOADED);
	TYPEINFO_ASSERT(dest.cls->state & CLASS_LOADED);

	/* maybe we need to link the classes */
	if (!(cls->state & CLASS_LINKED))
		if (!link_class(cls))
			return typecheck_FAIL;
	if (!(dest.cls->state & CLASS_LINKED))
		if (!link_class(dest.cls))
			return typecheck_FAIL;

	/* { we know that both c and dest are linked classes } */
	TYPEINFO_ASSERT(cls->state & CLASS_LINKED);
	TYPEINFO_ASSERT(dest.cls->state & CLASS_LINKED);

    if (dest.cls->flags & ACC_INTERFACE) {
        /* We are assigning to an interface type. */
        return merged_implements_interface(cls,value->merged,dest.cls);
    }

    if (CLASSINFO_IS_ARRAY(dest.cls)) {
		arraydescriptor *arraydesc = dest.cls->vftbl->arraydesc;
		int dimension = arraydesc->dimension;
		classinfo *elementclass = (arraydesc->elementvftbl)
			? arraydesc->elementvftbl->class : NULL;
			
        /* We are assigning to an array type. */
        if (!TYPEINFO_IS_ARRAY(*value))
            return typecheck_FALSE;

        /* {Both value and dest.cls are array types.} */

        /* value must have at least the dimension of dest.cls. */
        if (value->dimension < dimension)
            return typecheck_FALSE;

        if (value->dimension > dimension) {
            /* value has higher dimension so we need to check
             * if its component array can be assigned to the
             * element type of dest.cls */

			if (!elementclass) return typecheck_FALSE;
            
            if (elementclass->flags & ACC_INTERFACE) {
                /* We are assigning to an interface type. */
                return classinfo_implements_interface(pseudo_class_Arraystub,
                                                      elementclass);
            }

            /* We are assigning to a class type. */
            return class_issubclass(pseudo_class_Arraystub,elementclass);
        }

        /* {value and dest.cls have the same dimension} */

        if (value->elementtype != arraydesc->elementtype)
            return typecheck_FALSE;

        if (value->elementclass.any) {
            /* We are assigning an array of objects so we have to
             * check if the elements are assignable.
             */

            if (elementclass->flags & ACC_INTERFACE) {
                /* We are assigning to an interface type. */

                return merged_implements_interface(value->elementclass.cls,
                                                   value->merged,
                                                   elementclass);
            }
            
            /* We are assigning to a class type. */
            return merged_is_subclass(value->elementclass.cls,value->merged,elementclass);
        }

        return typecheck_TRUE;
    }

    /* {dest.cls is not an array} */
    /* {dest.cls is a loaded class} */

	/* If there are any unresolved references in the merged list, we cannot */
	/* tell if the assignment will be ok.                                   */
	/* This can only happen when cls is java.lang.Object                    */
	if (cls == class_java_lang_Object && value->merged) {
		classref_or_classinfo *mlist = value->merged->list;
		int i = value->merged->count;
		while (i--)
			if (IS_CLASSREF(*mlist++))
				return typecheck_MAYBE;
	}
        
    /* We are assigning to a class type */
    if (cls->flags & ACC_INTERFACE)
        cls = class_java_lang_Object;
    
    return merged_is_subclass(cls,value->merged,dest.cls);
}

/* typeinfo_is_assignable ******************************************************
 
   Check if a type is assignable to a given type.
   
   IN:
       value............the type of the value
	   dest.............the type of the destination, must not be a merged type

   RETURN VALUE:
       typecheck_TRUE...the type is assignable
	   typecheck_FALSE..the type is not assignable
	   typecheck_MAYBE..check cannot be performed now because of unresolved
	                    classes
	   typecheck_FAIL...an exception has been thrown

*******************************************************************************/

typecheck_result
typeinfo_is_assignable(typeinfo *value,typeinfo *dest)
{
	TYPEINFO_ASSERT(value);
	TYPEINFO_ASSERT(dest);
	TYPEINFO_ASSERT(dest->merged == NULL);

	return typeinfo_is_assignable_to_class(value,dest->typeclass);
}

/**********************************************************************/
/* INITIALIZATION FUNCTIONS                                           */
/* The following functions fill in uninitialized typeinfo structures. */
/**********************************************************************/

/* typeinfo_init_classinfo *****************************************************
 
   Initialize a typeinfo to a resolved class.
   
   IN:
	   c................the class

   OUT:
       *info............is initialized

   RETURN VALUE:
       true.............success
	   false............an exception has been thrown

*******************************************************************************/

void
typeinfo_init_classinfo(typeinfo *info, classinfo *c)
{
	if ((info->typeclass.cls = c)->vftbl->arraydesc) {
		if (c->vftbl->arraydesc->elementvftbl)
			info->elementclass.cls = c->vftbl->arraydesc->elementvftbl->class;
		else
			info->elementclass.any = NULL;
		info->dimension = c->vftbl->arraydesc->dimension;
		info->elementtype = c->vftbl->arraydesc->elementtype;
	}
	else {
		info->elementclass.any = NULL;
		info->dimension = 0;
		info->elementtype = 0;
	}
	info->merged = NULL;
}

/* typeinfo_init_class *********************************************************
 
   Initialize a typeinfo to a possibly unresolved class type.
   
   IN:
	   c................the class type

   OUT:
       *info............is initialized

   RETURN VALUE:
       true.............success
	   false............an exception has been thrown

*******************************************************************************/

bool
typeinfo_init_class(typeinfo *info,classref_or_classinfo c)
{
	char *utf_ptr;
	int len;
	classinfo *cls;
		
	TYPEINFO_ASSERT(c.any);
	TYPEINFO_ASSERT(info);

	/* if necessary, try to resolve lazily */
	if (!resolve_classref_or_classinfo(NULL /* XXX should know method */,
				c,resolveLazy,false,true,&cls))
	{
		return false;
	}
	
	if (cls) {
		typeinfo_init_classinfo(info,cls);
		return true;
	}

	/* {the type could no be resolved lazily} */

	info->typeclass.ref = c.ref;
	info->elementclass.any = NULL;
	info->dimension = 0;
	info->merged = NULL;

	/* handle array type references */
	utf_ptr = c.ref->name->text;
	len = c.ref->name->blength;
	if (*utf_ptr == '[') {
		/* count dimensions */
		while (*utf_ptr == '[') {
			utf_ptr++;
			info->dimension++;
			len--;
		}
		if (*utf_ptr == 'L') {
			utf_ptr++;
			len -= 2;
			info->elementtype = ARRAYTYPE_OBJECT;
			info->elementclass.ref = class_get_classref(c.ref->referer,utf_new(utf_ptr,len));
		}
		else {
			/* an array with primitive element type */
			/* should have been resolved above */
			TYPEINFO_ASSERT(false);
		}
	}
	return true;
}

/* typeinfo_init_from_typedesc *************************************************
 
   Initialize a typeinfo from a typedesc.
   
   IN:
	   desc.............the typedesc

   OUT:
       *type............set to the TYPE_* constant of DESC (if type != NULL)
       *info............receives the typeinfo (if info != NULL)

   RETURN VALUE:
       true.............success
	   false............an exception has been thrown

*******************************************************************************/

bool
typeinfo_init_from_typedesc(typedesc *desc,u1 *type,typeinfo *info)
{
	TYPEINFO_ASSERT(desc);

#ifdef TYPEINFO_VERBOSE
	fprintf(stderr,"typeinfo_init_from_typedesc(");
	descriptor_debug_print_typedesc(stderr,desc);
	fprintf(stderr,")\n");
#endif

	if (type)
		*type = desc->type;

	if (info) {
		if (desc->type == TYPE_ADR) {
			TYPEINFO_ASSERT(desc->classref);
			if (!typeinfo_init_class(info,CLASSREF_OR_CLASSINFO(desc->classref)))
				return false;
		}
		else {
			TYPEINFO_INIT_PRIMITIVE(*info);
		}
	}
	return true;
}

/* typeinfos_init_from_methoddesc **********************************************
 
   Initialize an array of typeinfos and u1 TYPE_* values from a methoddesc.
   
   IN:
       desc.............the methoddesc
       buflen...........number of parameters the buffer can hold
       twoword..........if true, use two parameter slots for two-word types

   OUT:
       *typebuf.........receives a TYPE_* constant for each parameter
                        typebuf must be != NULL
       *infobuf.........receives a typeinfo for each parameter
                        infobuf must be != NULL
       *returntype......receives a TYPE_* constant for the return type
                        returntype may be NULL
       *returntypeinfo..receives a typeinfo for the return type
                        returntypeinfo may be NULL

   RETURN VALUE:
       true.............success
       false............an exception has been thrown

   NOTE:
       If (according to BUFLEN) the buffers are to small to hold the
	   parameter types, an internal error is thrown. This must be
	   avoided by checking the number of parameters and allocating enough
	   space before calling this function.

*******************************************************************************/

bool
typeinfos_init_from_methoddesc(methoddesc *desc,u1 *typebuf,typeinfo *infobuf,
                              int buflen,bool twoword,
                              u1 *returntype,typeinfo *returntypeinfo)
{
	int i;
    int args = 0;

	TYPEINFO_ASSERT(desc);
	TYPEINFO_ASSERT(typebuf);
	TYPEINFO_ASSERT(infobuf);

#ifdef TYPEINFO_VERBOSE
	fprintf(stderr,"typeinfos_init_from_methoddesc(");
	descriptor_debug_print_methoddesc(stderr,desc);
	fprintf(stderr,")\n");
#endif

    /* check arguments */
    for (i=0; i<desc->paramcount; ++i) {
		if (++args > buflen) {
			exceptions_throw_internalerror("Buffer too small for method arguments.");
			return false;
		}

		if (!typeinfo_init_from_typedesc(desc->paramtypes + i,typebuf++,infobuf++))
			return false;
		
		if (twoword && (typebuf[-1] == TYPE_LNG || typebuf[-1] == TYPE_DBL)) {
			if (++args > buflen) {
				exceptions_throw_internalerror("Buffer too small for method arguments.");
				return false;
			}

			*typebuf++ = TYPE_VOID;
			TYPEINFO_INIT_PRIMITIVE(*infobuf);
			infobuf++;
		}
    }

    /* check returntype */
    if (returntype) {
		if (!typeinfo_init_from_typedesc(&(desc->returntype),returntype,returntypeinfo))
			return false;
	}

	return true;
}

/* typedescriptor_init_from_typedesc *******************************************
 
   Initialize a typedescriptor from a typedesc.
   
   IN:
	   desc.............the typedesc

   OUT:
       *td..............receives the typedescriptor
	                    td must be != NULL

   RETURN VALUE:
       true.............success
	   false............an exception has been thrown

*******************************************************************************/

bool
typedescriptor_init_from_typedesc(typedescriptor *td,
								  typedesc *desc)
{
	TYPEINFO_ASSERT(td);
	TYPEINFO_ASSERT(desc);

	td->type = desc->type;
	if (td->type == TYPE_ADR) {
		if (!typeinfo_init_class(&(td->typeinfo),CLASSREF_OR_CLASSINFO(desc->classref)))
			return false;
	}
	else {
		TYPEINFO_INIT_PRIMITIVE(td->typeinfo);
	}
	return true;
}

/* typeinfo_init_varinfo_from_typedesc *****************************************
 
   Initialize a varinfo from a typedesc.
   
   IN:
	   desc.............the typedesc

   OUT:
       *var.............receives the type
	                    var must be != NULL

   RETURN VALUE:
       true.............success
	   false............an exception has been thrown

*******************************************************************************/

bool
typeinfo_init_varinfo_from_typedesc(varinfo *var,
								  typedesc *desc)
{
	TYPEINFO_ASSERT(var);
	TYPEINFO_ASSERT(desc);

	var->type = desc->type;
	if (var->type == TYPE_ADR) {
		if (!typeinfo_init_class(&(var->typeinfo),CLASSREF_OR_CLASSINFO(desc->classref)))
			return false;
	}
	else {
		TYPEINFO_INIT_PRIMITIVE(var->typeinfo);
	}
	return true;
}

/* typeinfo_init_varinfos_from_methoddesc **************************************
 
   Initialize an array of varinfos from a methoddesc.
   
   IN:
       desc.............the methoddesc
       buflen...........number of parameters the buffer can hold
	   startindex.......the zero-based index of the first parameter to
	                    write to the array. In other words the number of
						parameters to skip at the beginning of the methoddesc.
	   map..............map from parameter indices to varinfo indices
	                    (indexed like jitdata.local_map)

   OUT:
       *vars............array receiving the varinfos
	                    td[0] receives the type of the
						(startindex+1)th parameter of the method
       *returntype......receives the typedescriptor of the return type.
	                    returntype may be NULL

   RETURN VALUE:
       true.............everything ok
	   false............an exception has been thrown

   NOTE:
       If (according to BUFLEN) the buffer is to small to hold the
	   parameter types, an internal error is thrown. This must be
	   avoided by checking the number of parameters and allocating enough
	   space before calling this function.

*******************************************************************************/

bool
typeinfo_init_varinfos_from_methoddesc(varinfo *vars,
									 methoddesc *desc,
									 int buflen, int startindex,
									 s4 *map,
									 typedescriptor *returntype)
{
	s4 i;
    s4 varindex;
	s4 type;
	s4 slot = 0;

	/* skip arguments */
	for (i=0; i<startindex; ++i) {
		slot++;
		if (IS_2_WORD_TYPE(desc->paramtypes[i].type))
			slot++;
	}

    /* check arguments */
    for (i=startindex; i<desc->paramcount; ++i) {
		type = desc->paramtypes[i].type;
		varindex = map[5*slot + type];

		slot++;
		if (IS_2_WORD_TYPE(type))
			slot++;

		if (varindex == UNUSED)
			continue;

		if (varindex >= buflen) {
			exceptions_throw_internalerror("Buffer too small for method arguments.");
			return false;
		}

		if (!typeinfo_init_varinfo_from_typedesc(vars + varindex, desc->paramtypes + i))
			return false;
    }

    /* check returntype */
    if (returntype) {
		if (!typedescriptor_init_from_typedesc(returntype,&(desc->returntype)))
			return false;
	}

	return true;
}

/* typedescriptors_init_from_methoddesc ****************************************
 
   Initialize an array of typedescriptors from a methoddesc.
   
   IN:
       desc.............the methoddesc
       buflen...........number of parameters the buffer can hold
       twoword..........if true, use two parameter slots for two-word types
	   startindex.......the zero-based index of the first parameter to
	                    write to the array. In other words the number of
						parameters to skip at the beginning of the methoddesc.

   OUT:
       *td..............array receiving the typedescriptors.
	                    td[0] receives the typedescriptor of the
						(startindex+1)th parameter of the method
       *returntype......receives the typedescriptor of the return type.
	                    returntype may be NULL

   RETURN VALUE:
       >= 0.............number of typedescriptors filled in TD
	   -1...............an exception has been thrown

   NOTE:
       If (according to BUFLEN) the buffer is to small to hold the
	   parameter types, an internal error is thrown. This must be
	   avoided by checking the number of parameters and allocating enough
	   space before calling this function.

*******************************************************************************/

int
typedescriptors_init_from_methoddesc(typedescriptor *td,
									 methoddesc *desc,
									 int buflen,bool twoword,int startindex,
									 typedescriptor *returntype)
{
	int i;
    int args = 0;

    /* check arguments */
    for (i=startindex; i<desc->paramcount; ++i) {
		if (++args > buflen) {
			exceptions_throw_internalerror("Buffer too small for method arguments.");
			return -1;
		}

		if (!typedescriptor_init_from_typedesc(td,desc->paramtypes + i))
			return -1;
		td++;

		if (twoword && (td[-1].type == TYPE_LNG || td[-1].type == TYPE_DBL)) {
			if (++args > buflen) {
				exceptions_throw_internalerror("Buffer too small for method arguments.");
				return -1;
			}

			td->type = TYPE_VOID;
			TYPEINFO_INIT_PRIMITIVE(td->typeinfo);
			td++;
		}
    }

    /* check returntype */
    if (returntype) {
		if (!typedescriptor_init_from_typedesc(returntype,&(desc->returntype)))
			return -1;
	}

	return args;
}

/* typeinfo_init_component *****************************************************
 
   Initialize a typeinfo with the component type of a given array type.
   
   IN:
	   srcarray.........the typeinfo of the array type

   OUT:
       *dst.............receives the typeinfo of the component type

   RETURN VALUE:
       true.............success
	   false............an exception has been thrown

*******************************************************************************/

bool
typeinfo_init_component(typeinfo *srcarray,typeinfo *dst)
{
	typeinfo_mergedlist *merged;

	TYPEINFO_ASSERT(srcarray);
	TYPEINFO_ASSERT(dst);

    if (TYPEINFO_IS_NULLTYPE(*srcarray)) {
        TYPEINFO_INIT_NULLTYPE(*dst);
        return true;
    }
    
    if (!TYPEINFO_IS_ARRAY(*srcarray)) {
		/* XXX should we make that a verify error? */
		exceptions_throw_internalerror("Trying to access component of non-array");
		return false;
	}

	/* save the mergedlist (maybe dst == srcarray) */

	merged = srcarray->merged;

	if (IS_CLASSREF(srcarray->typeclass)) {
		constant_classref *comp;
		comp = class_get_classref_component_of(srcarray->typeclass.ref);

		if (comp) {
			if (!typeinfo_init_class(dst,CLASSREF_OR_CLASSINFO(comp)))
				return false;
		}
		else {
			TYPEINFO_INIT_PRIMITIVE(*dst);
		}
	}
	else {
		vftbl_t *comp;
		
		if (!(srcarray->typeclass.cls->state & CLASS_LINKED)) {
			if (!link_class(srcarray->typeclass.cls)) {
				return false;
			}
		}

		TYPEINFO_ASSERT(srcarray->typeclass.cls->vftbl);
		TYPEINFO_ASSERT(srcarray->typeclass.cls->vftbl->arraydesc);

		comp = srcarray->typeclass.cls->vftbl->arraydesc->componentvftbl;
		if (comp)
			typeinfo_init_classinfo(dst,comp->class);
		else
			TYPEINFO_INIT_PRIMITIVE(*dst);
	}
    
    dst->merged = merged; /* XXX should we do a deep copy? */
	return true;
}

/* typeinfo_clone **************************************************************
 
   Create a deep copy of a typeinfo struct.
   
   IN:
	   src..............the typeinfo to copy

   OUT:
       *dest............receives the copy

   NOTE:
       If src == dest this function is a nop.

*******************************************************************************/

void
typeinfo_clone(typeinfo *src,typeinfo *dest)
{
    int count;
    classref_or_classinfo *srclist,*destlist;

    if (src == dest)
        return;
    
    *dest = *src;

    if (src->merged) {
        count = src->merged->count;
        TYPEINFO_ALLOCMERGED(dest->merged,count);
        dest->merged->count = count;

        srclist = src->merged->list;
        destlist = dest->merged->list;
        while (count--)
            *destlist++ = *srclist++;
    }
}

/**********************************************************************/
/* MISCELLANEOUS FUNCTIONS                                            */
/**********************************************************************/

/* typeinfo_free ***************************************************************
 
   Free memory referenced by the given typeinfo. The typeinfo itself is not
   freed.
   
   IN:
       info.............the typeinfo

*******************************************************************************/

void
typeinfo_free(typeinfo *info)
{
    TYPEINFO_FREEMERGED_IF_ANY(info->merged);
    info->merged = NULL;
}

/**********************************************************************/
/* MERGING FUNCTIONS                                                  */
/* The following functions are used to merge the types represented by */
/* two typeinfo structures into one typeinfo structure.               */
/**********************************************************************/

static
void
typeinfo_merge_error(methodinfo *m,char *str,typeinfo *x,typeinfo *y) {
#ifdef TYPEINFO_VERBOSE
    fprintf(stderr,"Error in typeinfo_merge: %s\n",str);
    fprintf(stderr,"Typeinfo x:\n");
    typeinfo_print(stderr,x,1);
    fprintf(stderr,"Typeinfo y:\n");
    typeinfo_print(stderr,y,1);
    log_text(str);
#endif

	exceptions_throw_verifyerror(m, str);
}

/* Condition: clsx != clsy. */
/* Returns: true if dest was changed (currently always true). */
static
bool
typeinfo_merge_two(typeinfo *dest,classref_or_classinfo clsx,classref_or_classinfo clsy)
{
	TYPEINFO_ASSERT(dest);
    TYPEINFO_FREEMERGED_IF_ANY(dest->merged);
    TYPEINFO_ALLOCMERGED(dest->merged,2);
    dest->merged->count = 2;

	TYPEINFO_ASSERT(clsx.any != clsy.any);

    if (clsx.any < clsy.any) {
        dest->merged->list[0] = clsx;
        dest->merged->list[1] = clsy;
    }
    else {
        dest->merged->list[0] = clsy;
        dest->merged->list[1] = clsx;
    }

    return true;
}

/* Returns: true if dest was changed. */
static
bool
typeinfo_merge_add(typeinfo *dest,typeinfo_mergedlist *m,classref_or_classinfo cls)
{
    int count;
    typeinfo_mergedlist *newmerged;
    classref_or_classinfo *mlist,*newlist;

    count = m->count;
    mlist = m->list;

    /* Check if cls is already in the mergedlist m. */
    while (count--) {
        if ((mlist++)->any == cls.any) { /* XXX check equal classrefs? */
            /* cls is in the list, so m is the resulting mergedlist */
            if (dest->merged == m)
                return false;

            /* We have to copy the mergedlist */
            TYPEINFO_FREEMERGED_IF_ANY(dest->merged);
            count = m->count;
            TYPEINFO_ALLOCMERGED(dest->merged,count);
            dest->merged->count = count;
            newlist = dest->merged->list;
            mlist = m->list;
            while (count--) {
                *newlist++ = *mlist++;
            }
            return true;
        }
    }

    /* Add cls to the mergedlist. */
    count = m->count;
    TYPEINFO_ALLOCMERGED(newmerged,count+1);
    newmerged->count = count+1;
    newlist = newmerged->list;    
    mlist = m->list;
    while (count) {
        if (mlist->any > cls.any)
            break;
        *newlist++ = *mlist++;
        count--;
    }
    *newlist++ = cls;
    while (count--) {
        *newlist++ = *mlist++;
    }

    /* Put the new mergedlist into dest. */
    TYPEINFO_FREEMERGED_IF_ANY(dest->merged);
    dest->merged = newmerged;
    
    return true;
}

/* Returns: true if dest was changed. */
static
bool
typeinfo_merge_mergedlists(typeinfo *dest,typeinfo_mergedlist *x,
                           typeinfo_mergedlist *y)
{
    int count = 0;
    int countx,county;
    typeinfo_mergedlist *temp,*result;
    classref_or_classinfo *clsx,*clsy,*newlist;

    /* count the elements that will be in the resulting list */
    /* (Both lists are sorted, equal elements are counted only once.) */
    clsx = x->list;
    clsy = y->list;
    countx = x->count;
    county = y->count;
    while (countx && county) {
        if (clsx->any == clsy->any) {
            clsx++;
            clsy++;
            countx--;
            county--;
        }
        else if (clsx->any < clsy->any) {
            clsx++;
            countx--;
        }
        else {
            clsy++;
            county--;
        }
        count++;
    }
    count += countx + county;

    /* {The new mergedlist will have count entries.} */

    if ((x->count != count) && (y->count == count)) {
        temp = x; x = y; y = temp;
    }
    /* {If one of x,y is already the result it is x.} */
    if (x->count == count) {
        /* x->merged is equal to the result */
        if (x == dest->merged)
            return false;

        if (!dest->merged || dest->merged->count != count) {
            TYPEINFO_FREEMERGED_IF_ANY(dest->merged);
            TYPEINFO_ALLOCMERGED(dest->merged,count);
            dest->merged->count = count;
        }

        newlist = dest->merged->list;
        clsx = x->list;
        while (count--) {
            *newlist++ = *clsx++;
        }
        return true;
    }

    /* {We have to merge two lists.} */

    /* allocate the result list */
    TYPEINFO_ALLOCMERGED(result,count);
    result->count = count;
    newlist = result->list;

    /* merge the sorted lists */
    clsx = x->list;
    clsy = y->list;
    countx = x->count;
    county = y->count;
    while (countx && county) {
        if (clsx->any == clsy->any) {
            *newlist++ = *clsx++;
            clsy++;
            countx--;
            county--;
        }
        else if (clsx->any < clsy->any) {
            *newlist++ = *clsx++;
            countx--;
        }
        else {
            *newlist++ = *clsy++;
            county--;
        }
    }
    while (countx--)
            *newlist++ = *clsx++;
    while (county--)
            *newlist++ = *clsy++;

    /* replace the list in dest with the result list */
    TYPEINFO_FREEMERGED_IF_ANY(dest->merged);
    dest->merged = result;

    return true;
}

/* typeinfo_merge_nonarrays ****************************************************
 
   Merge two non-array types.
   
   IN:
       x................the first type
	   y................the second type
	   mergedx..........merged list of the first type, may be NULL
	   mergedy..........merged list of the descond type, may be NULL

   OUT:
       *dest............receives the resulting merged list
	   *result..........receives the resulting type

   RETURN VALUE:
       typecheck_TRUE...*dest has been modified
	   typecheck_FALSE..*dest has not been modified
	   typecheck_FAIL...an exception has been thrown

   NOTE:
       RESULT is an extra parameter so it can point to dest->typeclass or to
	   dest->elementclass.

*******************************************************************************/

static typecheck_result
typeinfo_merge_nonarrays(typeinfo *dest,
                         classref_or_classinfo *result,
                         classref_or_classinfo x,classref_or_classinfo y,
                         typeinfo_mergedlist *mergedx,
                         typeinfo_mergedlist *mergedy)
{
	classref_or_classinfo t;
    classinfo *tcls,*common;
    typeinfo_mergedlist *tmerged;
    bool changed;
	typecheck_result r;
	utf *xname;
	utf *yname;

	TYPEINFO_ASSERT(dest && result && x.any && y.any);
	TYPEINFO_ASSERT(x.cls != pseudo_class_Null);
	TYPEINFO_ASSERT(y.cls != pseudo_class_Null);
	TYPEINFO_ASSERT(x.cls != pseudo_class_New);
	TYPEINFO_ASSERT(y.cls != pseudo_class_New);

	/*--------------------------------------------------*/
	/* common cases                                     */
	/*--------------------------------------------------*/

    /* Common case 1: x and y are the same class or class reference */
    /* (This case is very simple unless *both* x and y really represent
     *  merges of subclasses of clsx==clsy.)
     */
    if ( (x.any == y.any) && (!mergedx || !mergedy) ) {
  return_simple_x:
        /* DEBUG */ /* log_text("return simple x"); */
        changed = (dest->merged != NULL);
        TYPEINFO_FREEMERGED_IF_ANY(dest->merged);
        dest->merged = NULL;
        *result = x;
        /* DEBUG */ /* log_text("returning"); */
        return changed;
    }

	xname = (IS_CLASSREF(x)) ? x.ref->name : x.cls->name;
	yname = (IS_CLASSREF(y)) ? y.ref->name : y.cls->name;

	/* Common case 2: xname == yname, at least one unresolved */
    if ((IS_CLASSREF(x) || IS_CLASSREF(y)) && (xname == yname))
	{
		/* use the loaded one if any */
		if (!IS_CLASSREF(y))
			x = y;
		goto return_simple_x;
    }

	/*--------------------------------------------------*/
	/* non-trivial cases                                */
	/*--------------------------------------------------*/

#ifdef TYPEINFO_VERBOSE
	{
		typeinfo dbgx,dbgy;
		fprintf(stderr,"merge_nonarrays:\n");
		fprintf(stderr,"    ");if(IS_CLASSREF(x))fprintf(stderr,"<ref>");utf_fprint_printable_ascii(stderr,xname);fprintf(stderr,"\n");
		fprintf(stderr,"    ");if(IS_CLASSREF(y))fprintf(stderr,"<ref>");utf_fprint_printable_ascii(stderr,yname);fprintf(stderr,"\n");
		fflush(stderr);
		typeinfo_init_class(&dbgx,x);
		dbgx.merged = mergedx;
		typeinfo_init_class(&dbgy,y);
		dbgy.merged = mergedy;
		typeinfo_print(stderr,&dbgx,4);
		fprintf(stderr,"  with:\n");
		typeinfo_print(stderr,&dbgy,4);
	}
#endif

	TYPEINFO_ASSERT(IS_CLASSREF(x) || (x.cls->state & CLASS_LOADED));
	TYPEINFO_ASSERT(IS_CLASSREF(y) || (y.cls->state & CLASS_LOADED));

    /* If y is unresolved or an interface, swap x and y. */
    if (IS_CLASSREF(y) || (!IS_CLASSREF(x) && y.cls->flags & ACC_INTERFACE))
	{
        t = x; x = y; y = t;
        tmerged = mergedx; mergedx = mergedy; mergedy = tmerged;
    }
	
    /* {We know: If only one of x,y is unresolved it is x,} */
    /* {         If both x,y are resolved and only one of x,y is an interface it is x.} */

	if (IS_CLASSREF(x)) {
		/* {We know: x and y have different class names} */
		
        /* Check if we are merging an unresolved type with java.lang.Object */
        if (y.cls == class_java_lang_Object && !mergedy) {
            x = y;
            goto return_simple_x;
        }
            
		common = class_java_lang_Object;
		goto merge_with_simple_x;
	}

	/* {We know: both x and y are resolved} */
    /* {We know: If only one of x,y is an interface it is x.} */

	TYPEINFO_ASSERT(!IS_CLASSREF(x) && !IS_CLASSREF(y));
	TYPEINFO_ASSERT(x.cls->state & CLASS_LOADED);
	TYPEINFO_ASSERT(y.cls->state & CLASS_LOADED);

    /* Handle merging of interfaces: */
    if (x.cls->flags & ACC_INTERFACE) {
        /* {x.cls is an interface and mergedx == NULL.} */
        
        if (y.cls->flags & ACC_INTERFACE) {
            /* We are merging two interfaces. */
            /* {mergedy == NULL} */

            /* {We know that x.cls!=y.cls (see common case at beginning.)} */
            result->cls = class_java_lang_Object;
            return typeinfo_merge_two(dest,x,y);
        }

        /* {We know: x is an interface, y is a class.} */

        /* Check if we are merging an interface with java.lang.Object */
        if (y.cls == class_java_lang_Object && !mergedy) {
            x = y;
            goto return_simple_x;
        }

        /* If the type y implements x then the result of the merge
         * is x regardless of mergedy.
         */

		/* we may have to link the classes */
		if (!(x.cls->state & CLASS_LINKED))
			if (!link_class(x.cls))
				return typecheck_FAIL;
		if (!(y.cls->state & CLASS_LINKED))
			if (!link_class(y.cls))
				return typecheck_FAIL;
        
		TYPEINFO_ASSERT(x.cls->state & CLASS_LINKED);
		TYPEINFO_ASSERT(y.cls->state & CLASS_LINKED);

        if (CLASSINFO_IMPLEMENTS_INTERFACE(y.cls,x.cls->index))
		{
            /* y implements x, so the result of the merge is x. */
            goto return_simple_x;
		}
		
        r = mergedlist_implements_interface(mergedy,x.cls);
		if (r == typecheck_FAIL)
			return r;
		if (r == typecheck_TRUE)
        {
            /* y implements x, so the result of the merge is x. */
            goto return_simple_x;
        }
        
        /* {We know: x is an interface, the type y a class or a merge
         * of subclasses and is not guaranteed to implement x.} */

        common = class_java_lang_Object;
        goto merge_with_simple_x;
    }

    /* {We know: x and y are classes (not interfaces).} */
    
	/* we may have to link the classes */
	if (!(x.cls->state & CLASS_LINKED))
		if (!link_class(x.cls))
			return typecheck_FAIL;
	if (!(y.cls->state & CLASS_LINKED))
		if (!link_class(y.cls))
			return typecheck_FAIL;
        
	TYPEINFO_ASSERT(x.cls->state & CLASS_LINKED);
	TYPEINFO_ASSERT(y.cls->state & CLASS_LINKED);

    /* If *x is deeper in the inheritance hierarchy swap x and y. */
    if (x.cls->index > y.cls->index) {
        t = x; x = y; y = t;
        tmerged = mergedx; mergedx = mergedy; mergedy = tmerged;
    }

    /* {We know: y is at least as deep in the hierarchy as x.} */

    /* Find nearest common anchestor for the classes. */
    common = x.cls;
    tcls = y.cls;
    while (tcls->index > common->index)
        tcls = tcls->super.cls;
    while (common != tcls) {
        common = common->super.cls;
        tcls = tcls->super.cls;
    }

    /* {common == nearest common anchestor of x and y.} */

    /* If x.cls==common and x is a whole class (not a merge of subclasses)
     * then the result of the merge is x.
     */
    if (x.cls == common && !mergedx) {
        goto return_simple_x;
    }
   
    if (mergedx) {
        result->cls = common;
        if (mergedy)
            return typeinfo_merge_mergedlists(dest,mergedx,mergedy);
        else
            return typeinfo_merge_add(dest,mergedx,y);
    }

merge_with_simple_x:
    result->cls = common;
    if (mergedy)
        return typeinfo_merge_add(dest,mergedy,x);
    else
        return typeinfo_merge_two(dest,x,y);
}

/* typeinfo_merge **************************************************************
 
   Merge two types.
   
   IN:
       m................method for exception messages
       dest.............the first type
       y................the second type

   OUT:
       *dest............receives the result of the merge

   RETURN VALUE:
       typecheck_TRUE...*dest has been modified
       typecheck_FALSE..*dest has not been modified
       typecheck_FAIL...an exception has been thrown

   PRE-CONDITIONS:
       1) *dest must be a valid initialized typeinfo
       2) dest != y

*******************************************************************************/

typecheck_result
typeinfo_merge(methodinfo *m,typeinfo *dest,typeinfo* y)
{
    typeinfo *x;
    typeinfo *tmp;
    classref_or_classinfo common;
    classref_or_classinfo elementclass;
    int dimension;
    int elementtype;
    bool changed;
	typecheck_result r;

	/*--------------------------------------------------*/
	/* fast checks                                      */
	/*--------------------------------------------------*/

    /* Merging something with itself is a nop */
    if (dest == y)
        return typecheck_FALSE;

    /* Merging two returnAddress types is ok. */
	/* Merging two different returnAddresses never happens, as the verifier */
	/* keeps them separate in order to check all the possible return paths  */
	/* from JSR subroutines.                                                */
    if (!dest->typeclass.any && !y->typeclass.any) {
		TYPEINFO_ASSERT(TYPEINFO_RETURNADDRESS(*dest) ==  TYPEINFO_RETURNADDRESS(*y));
        return typecheck_FALSE;
	}
    
    /* Primitive types cannot be merged with reference types */
	/* This must be checked before calls to typeinfo_merge.  */
    TYPEINFO_ASSERT(dest->typeclass.any && y->typeclass.any);

    /* handle uninitialized object types */
    if (TYPEINFO_IS_NEWOBJECT(*dest) || TYPEINFO_IS_NEWOBJECT(*y)) {
        if (!TYPEINFO_IS_NEWOBJECT(*dest) || !TYPEINFO_IS_NEWOBJECT(*y)) {
            typeinfo_merge_error(m,"Trying to merge uninitialized object type.",dest,y);
			return typecheck_FAIL;
		}
        if (TYPEINFO_NEWOBJECT_INSTRUCTION(*dest) != TYPEINFO_NEWOBJECT_INSTRUCTION(*y)) {
            typeinfo_merge_error(m,"Trying to merge different uninitialized objects.",dest,y);
			return typecheck_FAIL;
		}
		/* the same uninitialized object -- no change */
		return typecheck_FALSE;
    }
    
	/*--------------------------------------------------*/
	/* common cases                                     */
	/*--------------------------------------------------*/

    /* Common case: dest and y are the same class or class reference */
    /* (This case is very simple unless *both* dest and y really represent
     *  merges of subclasses of class dest==class y.)
     */
    if ((dest->typeclass.any == y->typeclass.any) && (!dest->merged || !y->merged)) {
return_simple:
        changed = (dest->merged != NULL);
        TYPEINFO_FREEMERGED_IF_ANY(dest->merged);
        dest->merged = NULL;
        return changed;
    }
    
    /* Handle null types: */
    if (TYPEINFO_IS_NULLTYPE(*y)) {
        return typecheck_FALSE;
    }
    if (TYPEINFO_IS_NULLTYPE(*dest)) {
        TYPEINFO_FREEMERGED_IF_ANY(dest->merged);
        TYPEINFO_CLONE(*y,*dest);
        return typecheck_TRUE;
    }

	/* Common case: two types with the same name, at least one unresolved */
	if (IS_CLASSREF(dest->typeclass)) {
		if (IS_CLASSREF(y->typeclass)) {
			if (dest->typeclass.ref->name == y->typeclass.ref->name)
				goto return_simple;
		}
		else {
			/* XXX should we take y instead of dest here? */
			if (dest->typeclass.ref->name == y->typeclass.cls->name)
				goto return_simple;
		}
	}
	else {
		if (IS_CLASSREF(y->typeclass) 
		    && (dest->typeclass.cls->name == y->typeclass.ref->name))
		{
			goto return_simple;
		}
	}

	/*--------------------------------------------------*/
	/* non-trivial cases                                */
	/*--------------------------------------------------*/

#ifdef TYPEINFO_VERBOSE
	fprintf(stderr,"merge:\n");
    typeinfo_print(stderr,dest,4);
    typeinfo_print(stderr,y,4);
#endif

    /* This function uses x internally, so x and y can be swapped
     * without changing dest. */
    x = dest;
    changed = false;
    
    /* Handle merging of arrays: */
    if (TYPEINFO_IS_ARRAY(*x) && TYPEINFO_IS_ARRAY(*y)) {
        
        /* Make x the one with lesser dimension */
        if (x->dimension > y->dimension) {
            tmp = x; x = y; y = tmp;
        }

        /* If one array (y) has higher dimension than the other,
         * interpret it as an array (same dim. as x) of Arraystubs. */
        if (x->dimension < y->dimension) {
            dimension = x->dimension;
            elementtype = ARRAYTYPE_OBJECT;
            elementclass.cls = pseudo_class_Arraystub;
        }
        else {
            dimension = y->dimension;
            elementtype = y->elementtype;
            elementclass = y->elementclass;
        }
        
        /* {The arrays are of the same dimension.} */
        
        if (x->elementtype != elementtype) {
            /* Different element types are merged, so the resulting array
             * type has one accessible dimension less. */
            if (--dimension == 0) {
                common.cls = pseudo_class_Arraystub;
                elementtype = 0;
                elementclass.any = NULL;
            }
            else {
                common.cls = class_multiarray_of(dimension,pseudo_class_Arraystub,true);
				if (!common.cls) {
					exceptions_throw_internalerror("XXX Coult not create array class");
					return typecheck_FAIL;
				}

                elementtype = ARRAYTYPE_OBJECT;
                elementclass.cls = pseudo_class_Arraystub;
            }
        }
        else {
            /* {The arrays have the same dimension and elementtype.} */

            if (elementtype == ARRAYTYPE_OBJECT) {
                /* The elements are references, so their respective
                 * types must be merged.
                 */
				r = typeinfo_merge_nonarrays(dest,
						&elementclass,
						x->elementclass,
						elementclass,
						x->merged,y->merged);
				TYPEINFO_ASSERT(r != typecheck_MAYBE);
				if (r == typecheck_FAIL)
					return r;
				changed |= r;

                /* DEBUG */ /* log_text("finding resulting array class: "); */
				if (IS_CLASSREF(elementclass))
					common.ref = class_get_classref_multiarray_of(dimension,elementclass.ref);
				else {
					common.cls = class_multiarray_of(dimension,elementclass.cls,true);
					if (!common.cls) {
						exceptions_throw_internalerror("XXX Coult not create array class");
						return typecheck_FAIL;
					}
				}
                /* DEBUG */ /* utf_display_printable_ascii(common->name); printf("\n"); */
            }
			else {
				common.any = y->typeclass.any;
			}
        }
    }
    else {
        /* {We know that at least one of x or y is no array, so the
         *  result cannot be an array.} */
        
		r = typeinfo_merge_nonarrays(dest,
				&common,
				x->typeclass,y->typeclass,
				x->merged,y->merged);
		TYPEINFO_ASSERT(r != typecheck_MAYBE);
		if (r == typecheck_FAIL)
			return r;
		changed |= r;

        dimension = 0;
        elementtype = 0;
        elementclass.any = NULL;
    }

    /* Put the new values into dest if neccessary. */

    if (dest->typeclass.any != common.any) {
        dest->typeclass.any = common.any;
        changed = true;
    }
    if (dest->dimension != dimension) {
        dest->dimension = dimension;
        changed = true;
    }
    if (dest->elementtype != elementtype) {
        dest->elementtype = elementtype;
        changed = true;
    }
    if (dest->elementclass.any != elementclass.any) {
        dest->elementclass.any = elementclass.any;
        changed = true;
    }

    return changed;
}
#endif /* ENABLE_VERIFER */


/**********************************************************************/
/* DEBUGGING HELPERS                                                  */
/**********************************************************************/

#ifdef TYPEINFO_DEBUG

#if 0
static int
typeinfo_test_compare(classref_or_classinfo *a,classref_or_classinfo *b)
{
    if (a->any == b->any) return 0;
    if (a->any < b->any) return -1;
    return +1;
}

static void
typeinfo_test_parse(typeinfo *info,char *str)
{
    int num;
    int i;
    typeinfo *infobuf;
    u1 *typebuf;
    int returntype;
    utf *desc = utf_new_char(str);
    
    num = typeinfo_count_method_args(desc,false);
    if (num) {
        typebuf = DMNEW(u1,num);
        infobuf = DMNEW(typeinfo,num);
        
        typeinfo_init_from_method_args(desc,typebuf,infobuf,num,false,
                                       &returntype,info);

        TYPEINFO_ALLOCMERGED(info->merged,num);
        info->merged->count = num;

        for (i=0; i<num; ++i) {
            if (typebuf[i] != TYPE_ADR) {
                log_text("non-reference type in mergedlist");
				assert(0);
			}

            info->merged->list[i].any = infobuf[i].typeclass.any;
        }
        qsort(info->merged->list,num,sizeof(classref_or_classinfo),
              (int(*)(const void *,const void *))&typeinfo_test_compare);
    }
    else {
        typeinfo_init_from_method_args(desc,NULL,NULL,0,false,
                                       &returntype,info);
    }
}
#endif

#define TYPEINFO_TEST_BUFLEN  4000

static bool
typeinfo_equal(typeinfo *x,typeinfo *y)
{
    int i;
    
    if (x->typeclass.any != y->typeclass.any) return false;
    if (x->dimension != y->dimension) return false;
    if (x->dimension) {
        if (x->elementclass.any != y->elementclass.any) return false;
        if (x->elementtype != y->elementtype) return false;
    }

    if (TYPEINFO_IS_NEWOBJECT(*x))
        if (TYPEINFO_NEWOBJECT_INSTRUCTION(*x)
            != TYPEINFO_NEWOBJECT_INSTRUCTION(*y))
            return false;

    if (x->merged || y->merged) {
        if (!(x->merged && y->merged)) return false;
        if (x->merged->count != y->merged->count) return false;
        for (i=0; i<x->merged->count; ++i)
            if (x->merged->list[i].any != y->merged->list[i].any)
                return false;
    }
    return true;
}

static void
typeinfo_testmerge(typeinfo *a,typeinfo *b,typeinfo *result,int *failed)
{
    typeinfo dest;
    bool changed,changed_should_be;
	typecheck_result r;

    TYPEINFO_CLONE(*a,dest);
    
    printf("\n          ");
    typeinfo_print_short(stdout,&dest);
    printf("\n          ");
    typeinfo_print_short(stdout,b);
    printf("\n");

	r = typeinfo_merge(NULL,&dest,b);
	if (r == typecheck_FAIL) {
		printf("EXCEPTION\n");
		return;
	}
    changed = (r) ? 1 : 0;
    changed_should_be = (!typeinfo_equal(&dest,a)) ? 1 : 0;

    printf("          %s\n",(changed) ? "changed" : "=");

    if (typeinfo_equal(&dest,result)) {
        printf("OK        ");
        typeinfo_print_short(stdout,&dest);
        printf("\n");
        if (changed != changed_should_be) {
            printf("WRONG RETURN VALUE!\n");
            (*failed)++;
        }
    }
    else {
        printf("RESULT    ");
        typeinfo_print_short(stdout,&dest);
        printf("\n");
        printf("SHOULD BE ");
        typeinfo_print_short(stdout,result);
        printf("\n");
        (*failed)++;
    }
}

#if 0
static void
typeinfo_inc_dimension(typeinfo *info)
{
    if (info->dimension++ == 0) {
        info->elementtype = ARRAYTYPE_OBJECT;
        info->elementclass = info->typeclass;
    }
    info->typeclass = class_array_of(info->typeclass,true);
}
#endif

#define TYPEINFO_TEST_MAXDIM  10

static void
typeinfo_testrun(char *filename)
{
    char buf[TYPEINFO_TEST_BUFLEN];
    char bufa[TYPEINFO_TEST_BUFLEN];
    char bufb[TYPEINFO_TEST_BUFLEN];
    char bufc[TYPEINFO_TEST_BUFLEN];
    typeinfo a,b,c;
    int maxdim;
    int failed = 0;
    FILE *file = fopen(filename,"rt");
	int res;
    
    if (!file) {
        log_text("could not open typeinfo test file");
		assert(0);
	}

    while (fgets(buf,TYPEINFO_TEST_BUFLEN,file)) {
        if (buf[0] == '#' || !strlen(buf))
            continue;
        
        res = sscanf(buf,"%s\t%s\t%s\n",bufa,bufb,bufc);
        if (res != 3 || !strlen(bufa) || !strlen(bufb) || !strlen(bufc)) {
            log_text("Invalid line in typeinfo test file (none of empty, comment or test)");
			assert(0);
		}

#if 0
        typeinfo_test_parse(&a,bufa);
        typeinfo_test_parse(&b,bufb);
        typeinfo_test_parse(&c,bufc);
#endif
#if 0
        do {
#endif
            typeinfo_testmerge(&a,&b,&c,&failed); /* check result */
            typeinfo_testmerge(&b,&a,&c,&failed); /* check commutativity */

            if (TYPEINFO_IS_NULLTYPE(a)) break;
            if (TYPEINFO_IS_NULLTYPE(b)) break;
            if (TYPEINFO_IS_NULLTYPE(c)) break;
            
            maxdim = a.dimension;
            if (b.dimension > maxdim) maxdim = b.dimension;
            if (c.dimension > maxdim) maxdim = c.dimension;

#if 0
            if (maxdim < TYPEINFO_TEST_MAXDIM) {
                typeinfo_inc_dimension(&a);
                typeinfo_inc_dimension(&b);
                typeinfo_inc_dimension(&c);
            }
        } while (maxdim < TYPEINFO_TEST_MAXDIM);
#endif
    }

    fclose(file);

    if (failed) {
        fprintf(stderr,"Failed typeinfo_merge tests: %d\n",failed);
        log_text("Failed test");
		assert(0);
    }
}

void
typeinfo_test()
{
    log_text("Running typeinfo test file...");
    typeinfo_testrun("typeinfo.tst");
    log_text("Finished typeinfo test file.");
}

#if 0
void
typeinfo_init_from_fielddescriptor(typeinfo *info,char *desc)
{
    typeinfo_init_from_descriptor(info,desc,desc+strlen(desc));
}
#endif

#define TYPEINFO_MAXINDENT  80

void
typeinfo_print_class(FILE *file,classref_or_classinfo c)
{
	/*fprintf(file,"<class %p>",c.any);*/

	if (!c.any) {
		fprintf(file,"<null>");
	}
	else {
		if (IS_CLASSREF(c)) {
			fprintf(file,"<ref>");
			utf_fprint_printable_ascii(file,c.ref->name);
		}
		else {
			utf_fprint_printable_ascii(file,c.cls->name);
		}
	}
}

void
typeinfo_print(FILE *file,typeinfo *info,int indent)
{
    int i;
    char ind[TYPEINFO_MAXINDENT + 1];
    instruction *ins;
	basicblock *bptr;

    if (indent > TYPEINFO_MAXINDENT) indent = TYPEINFO_MAXINDENT;
    
    for (i=0; i<indent; ++i)
        ind[i] = ' ';
    ind[i] = (char) 0;
    
    if (TYPEINFO_IS_PRIMITIVE(*info)) {
		bptr = (basicblock*) TYPEINFO_RETURNADDRESS(*info);
		if (bptr)
			fprintf(file,"%sreturnAddress (L%03d)\n",ind,bptr->nr);
		else
			fprintf(file,"%sprimitive\n",ind);
        return;
    }
    
    if (TYPEINFO_IS_NULLTYPE(*info)) {
        fprintf(file,"%snull\n",ind);
        return;
    }

    if (TYPEINFO_IS_NEWOBJECT(*info)) {
        ins = (instruction *) TYPEINFO_NEWOBJECT_INSTRUCTION(*info);
        if (ins) {
            fprintf(file,"%sNEW(%p):",ind,(void*)ins);
			typeinfo_print_class(file,ins[-1].sx.val.c);
            fprintf(file,"\n");
        }
        else {
            fprintf(file,"%sNEW(this)",ind);
        }
        return;
    }

    fprintf(file,"%sClass:      ",ind);
	typeinfo_print_class(file,info->typeclass);
    fprintf(file,"\n");

    if (TYPEINFO_IS_ARRAY(*info)) {
        fprintf(file,"%sDimension:    %d",ind,(int)info->dimension);
        fprintf(file,"\n%sElements:     ",ind);
        switch (info->elementtype) {
          case ARRAYTYPE_INT     : fprintf(file,"int\n"); break;
          case ARRAYTYPE_LONG    : fprintf(file,"long\n"); break;
          case ARRAYTYPE_FLOAT   : fprintf(file,"float\n"); break;
          case ARRAYTYPE_DOUBLE  : fprintf(file,"double\n"); break;
          case ARRAYTYPE_BYTE    : fprintf(file,"byte\n"); break;
          case ARRAYTYPE_CHAR    : fprintf(file,"char\n"); break;
          case ARRAYTYPE_SHORT   : fprintf(file,"short\n"); break;
          case ARRAYTYPE_BOOLEAN : fprintf(file,"boolean\n"); break;
              
          case ARRAYTYPE_OBJECT:
			  typeinfo_print_class(file,info->elementclass);
              fprintf(file,"\n");
              break;
              
          default:
              fprintf(file,"INVALID ARRAYTYPE!\n");
        }
    }

    if (info->merged) {
        fprintf(file,"%sMerged:     ",ind);
        for (i=0; i<info->merged->count; ++i) {
            if (i) fprintf(file,", ");
			typeinfo_print_class(file,info->merged->list[i]);
        }
        fprintf(file,"\n");
    }
}

void
typeinfo_print_short(FILE *file,typeinfo *info)
{
    int i;
    instruction *ins;
	basicblock *bptr;

	/*fprintf(file,"<typeinfo %p>",info);*/

	if (!info) {
		fprintf(file,"(typeinfo*)NULL");
		return;
	}

    if (TYPEINFO_IS_PRIMITIVE(*info)) {
		bptr = (basicblock*) TYPEINFO_RETURNADDRESS(*info);
		if (bptr)
			fprintf(file,"ret(L%03d)",bptr->nr);
		else
			fprintf(file,"primitive");
        return;
    }
    
    if (TYPEINFO_IS_NULLTYPE(*info)) {
        fprintf(file,"null");
        return;
    }
    
    if (TYPEINFO_IS_NEWOBJECT(*info)) {
        ins = (instruction *) TYPEINFO_NEWOBJECT_INSTRUCTION(*info);
        if (ins) {
			/*fprintf(file,"<ins %p>",ins);*/
            fprintf(file,"NEW(%p):",(void*)ins);
			typeinfo_print_class(file,ins[-1].sx.val.c);
        }
        else
            fprintf(file,"NEW(this)");
        return;
    }

    typeinfo_print_class(file,info->typeclass);

    if (info->merged) {
        fprintf(file,"{");
        for (i=0; i<info->merged->count; ++i) {
            if (i) fprintf(file,",");
			typeinfo_print_class(file,info->merged->list[i]);
        }
        fprintf(file,"}");
    }
}

void
typeinfo_print_type(FILE *file,int type,typeinfo *info)
{
    switch (type) {
      case TYPE_VOID: fprintf(file,"V"); break;
      case TYPE_INT:  fprintf(file,"I"); break;
      case TYPE_FLT:  fprintf(file,"F"); break;
      case TYPE_DBL:  fprintf(file,"D"); break;
      case TYPE_LNG:  fprintf(file,"J"); break;
	  case TYPE_RET:  fprintf(file,"R:"); /* FALLTHROUGH! */
      case TYPE_ADR:
		  typeinfo_print_short(file,info);
          break;
          
      default:
          fprintf(file,"!");
    }
}

void
typedescriptor_print(FILE *file,typedescriptor *td)
{
	typeinfo_print_type(file,td->type,&(td->typeinfo));
}

void
typevector_print(FILE *file,varinfo *vec,int size)
{
    int i;

    for (i=0; i<size; ++i) {
		fprintf(file," %d=",i);
        typeinfo_print_type(file, vec[i].type, &(vec[i].typeinfo));
    }
}

#endif /* TYPEINFO_DEBUG */


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

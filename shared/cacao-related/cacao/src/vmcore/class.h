/* src/vmcore/class.h - class related functions header

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

   $Id: class.h 7783 2007-04-20 13:28:27Z twisti $

*/


#ifndef _CLASS_H
#define _CLASS_H

/* forward typedefs ***********************************************************/

typedef struct classinfo      classinfo; 
typedef struct innerclassinfo innerclassinfo;
typedef struct extra_classref extra_classref;
typedef struct castinfo       castinfo;


#include "config.h"
#include "vm/types.h"

#include "toolbox/list.h"

#include "vm/global.h"

#if defined(ENABLE_JAVASE)
# include "vmcore/annotation.h"
#endif

#include "vmcore/field.h"
#include "vmcore/linker.h"
#include "vmcore/loader.h"
#include "vmcore/method.h"
#include "vmcore/references.h"
#include "vmcore/utf8.h"


/* class state defines ********************************************************/

#define CLASS_LOADING         0x0001
#define CLASS_LOADED          0x0002
#define CLASS_LINKING         0x0004
#define CLASS_LINKED          0x0008
#define CLASS_INITIALIZING    0x0010
#define CLASS_INITIALIZED     0x0020
#define CLASS_ERROR           0x0040


/* some macros ****************************************************************/

#define CLASS_IS_OR_ALMOST_INITIALIZED(c) \
    (((c)->state & CLASS_INITIALIZING) || ((c)->state & CLASS_INITIALIZED))


/* classinfo ******************************************************************/

/* We define this dummy structure of java_lang_Class so we can
   bootstrap cacaoh without needing a java_lang_Class.h file.  If the
   size is big enough, is checked during runtime in vm_create. */

typedef struct {
	java_objectheader header;
	ptrint            padding[4];
} dummy_java_lang_Class;

struct classinfo {                /* class structure                          */
	dummy_java_lang_Class object;

	s4          flags;            /* ACC flags                                */
	utf        *name;             /* class name                               */

	s4          cpcount;          /* number of entries in constant pool       */
	u1         *cptags;           /* constant pool tags                       */
	voidptr    *cpinfos;          /* pointer to constant pool info structures */

	s4          classrefcount;    /* number of symbolic class references      */
	constant_classref *classrefs; /* table of symbolic class references       */
	extra_classref *extclassrefs; /* additional classrefs                     */
	s4          parseddescsize;   /* size of the parsed descriptors block     */
	u1         *parseddescs;      /* parsed descriptors                       */

	classref_or_classinfo super;  /* super class                              */
	classinfo  *sub;              /* sub class pointer                        */
	classinfo  *nextsub;          /* pointer to next class in sub class list  */

	s4          interfacescount;  /* number of interfaces                     */
	classref_or_classinfo *interfaces; /* superinterfaces                     */

	s4          fieldscount;      /* number of fields                         */
	fieldinfo  *fields;           /* field table                              */

	s4          methodscount;     /* number of methods                        */
	methodinfo *methods;          /* method table                             */

	listnode_t  listnode;         /* linkage                                  */

	s4          state;            /* current class state                      */
	s4          index;            /* hierarchy depth (classes) or index       */
	                              /* (interfaces)                             */
	s4          instancesize;     /* size of an instance of this class        */

	vftbl_t    *vftbl;            /* pointer to virtual function table        */

	methodinfo *finalizer;        /* finalizer method                         */

	u2          innerclasscount;  /* number of inner classes                  */
	innerclassinfo *innerclass;

#if defined(ENABLE_JAVASE)
	classref_or_classinfo  enclosingclass;  /* enclosing class                */
	constant_nameandtype  *enclosingmethod; /* enclosing method               */
#endif

	utf        *packagename;      /* full name of the package                 */
	utf        *sourcefile;       /* SourceFile attribute                     */
#if defined(ENABLE_JAVASE)
	utf        *signature;        /* Signature attribute                      */
	s4            runtimevisibleannotationscount;
	annotation_t *runtimevisibleannotations;
#endif
	java_objectheader *classloader; /* NULL for bootstrap classloader         */
};


/* innerclassinfo *************************************************************/

struct innerclassinfo {
	classref_or_classinfo inner_class; /* inner class pointer                 */
	classref_or_classinfo outer_class; /* outer class pointer                 */
	utf                  *name;        /* innerclass name                     */
	s4                    flags;       /* ACC flags                           */
};


/* extra_classref **************************************************************

   for classrefs not occurring within descriptors

*******************************************************************************/

struct extra_classref {
	extra_classref    *next;
	constant_classref  classref;
};


/* castinfo *******************************************************************/

struct castinfo {
	s4 super_baseval;
	s4 super_diffval;
	s4 sub_baseval;
};


/* global variables ***********************************************************/

extern list_t unlinkedclasses; /* this is only used for eager class loading   */


/* frequently used classes ****************************************************/

/* important system classes */

extern classinfo *class_java_lang_Object;
extern classinfo *class_java_lang_Class;
extern classinfo *class_java_lang_ClassLoader;
extern classinfo *class_java_lang_Cloneable;
extern classinfo *class_java_lang_SecurityManager;
extern classinfo *class_java_lang_String;
extern classinfo *class_java_lang_System;
extern classinfo *class_java_lang_Thread;
extern classinfo *class_java_lang_ThreadGroup;
extern classinfo *class_java_lang_VMSystem;
extern classinfo *class_java_lang_VMThread;
extern classinfo *class_java_io_Serializable;


/* system exception classes required in cacao */

extern classinfo *class_java_lang_Throwable;
extern classinfo *class_java_lang_Error;
extern classinfo *class_java_lang_LinkageError;
extern classinfo *class_java_lang_NoClassDefFoundError;
extern classinfo *class_java_lang_OutOfMemoryError;
extern classinfo *class_java_lang_VirtualMachineError;

#if defined(WITH_CLASSPATH_GNU)
extern classinfo *class_java_lang_VMThrowable;
#endif

extern classinfo *class_java_lang_Exception;
extern classinfo *class_java_lang_ClassCastException;
extern classinfo *class_java_lang_ClassNotFoundException;

#if defined(ENABLE_JAVASE)
extern classinfo *class_java_lang_Void;
#endif

extern classinfo *class_java_lang_Boolean;
extern classinfo *class_java_lang_Byte;
extern classinfo *class_java_lang_Character;
extern classinfo *class_java_lang_Short;
extern classinfo *class_java_lang_Integer;
extern classinfo *class_java_lang_Long;
extern classinfo *class_java_lang_Float;
extern classinfo *class_java_lang_Double;


/* some runtime exception */

extern classinfo *class_java_lang_NullPointerException;


/* some classes which may be used more often */

#if defined(ENABLE_JAVASE)
extern classinfo *class_java_lang_StackTraceElement;
extern classinfo *class_java_lang_reflect_Constructor;
extern classinfo *class_java_lang_reflect_Field;
extern classinfo *class_java_lang_reflect_Method;
extern classinfo *class_java_security_PrivilegedAction;
extern classinfo *class_java_util_Vector;

extern classinfo *arrayclass_java_lang_Object;
#endif


/* pseudo classes for the type checker ****************************************/

/*
 * pseudo_class_Arraystub
 *     (extends Object implements Cloneable, java.io.Serializable)
 *
 *     If two arrays of incompatible component types are merged,
 *     the resulting reference has no accessible components.
 *     The result does, however, implement the interfaces Cloneable
 *     and java.io.Serializable. This pseudo class is used internally
 *     to represent such results. (They are *not* considered arrays!)
 *
 * pseudo_class_Null
 *
 *     This pseudo class is used internally to represent the
 *     null type.
 *
 * pseudo_class_New
 *
 *     This pseudo class is used internally to represent the
 *     the 'uninitialized object' type.
 */

extern classinfo *pseudo_class_Arraystub;
extern classinfo *pseudo_class_Null;
extern classinfo *pseudo_class_New;


/* function prototypes ********************************************************/

/* create a new classinfo struct */
classinfo *class_create_classinfo(utf *u);

/* postset's the header.vftbl */
void class_postset_header_vftbl(void);

/* set the package name after the name has been set */
void class_set_packagename(classinfo *c);

bool class_load_attributes(classbuffer *cb);

/* retrieve constantpool element */
voidptr class_getconstant(classinfo *class, u4 pos, u4 ctype);
voidptr innerclass_getconstant(classinfo *c, u4 pos, u4 ctype);

/* frees all resources used by the class */
void class_free(classinfo *);

/* return an array class with the given component class */
classinfo *class_array_of(classinfo *component,bool link);

/* return an array class with the given dimension and element class */
classinfo *class_multiarray_of(s4 dim, classinfo *element,bool link);

/* return a classref for the given class name */
/* (does a linear search!)                    */
constant_classref *class_lookup_classref(classinfo *cls,utf *name);

/* return a classref for the given class name */
/* (does a linear search!)                    */
constant_classref *class_get_classref(classinfo *cls,utf *name);

/* return a classref to the class itself */
/* (does a linear search!)                    */
constant_classref *class_get_self_classref(classinfo *cls);

/* return a classref for an array with the given dimension of with the */
/* given component type */
constant_classref *class_get_classref_multiarray_of(s4 dim,constant_classref *ref);

/* return a classref for the component type of the given array type */
constant_classref *class_get_classref_component_of(constant_classref *ref);

/* get a class' field by name and descriptor */
fieldinfo *class_findfield(classinfo *c, utf *name, utf *desc);

/* search 'classinfo'-structure for a field with the specified name */
fieldinfo *class_findfield_by_name(classinfo *c, utf *name);
s4 class_findfield_index_by_name(classinfo *c, utf *name);

/* search class for a field */
fieldinfo *class_resolvefield(classinfo *c, utf *name, utf *desc, classinfo *referer, bool throwexception);

/* search for a method with a specified name and descriptor */
methodinfo *class_findmethod(classinfo *c, utf *name, utf *desc);
methodinfo *class_resolvemethod(classinfo *c, utf *name, utf *dest);
methodinfo *class_resolveclassmethod(classinfo *c, utf *name, utf *dest, classinfo *referer, bool throwexception);
methodinfo *class_resolveinterfacemethod(classinfo *c, utf *name, utf *dest, classinfo *referer, bool throwexception);

bool class_issubclass(classinfo *sub, classinfo *super);
bool class_isanysubclass(classinfo *sub, classinfo *super);

/* some debugging functions */

#if !defined(NDEBUG)
void class_printflags(classinfo *c);
void class_print(classinfo *c);
void class_println(classinfo *c);
void class_classref_print(constant_classref *cr);
void class_classref_println(constant_classref *cr);
void class_classref_or_classinfo_print(classref_or_classinfo c);
void class_classref_or_classinfo_println(classref_or_classinfo c);
#endif

/* debug purposes */
void class_showmethods(classinfo *c);
void class_showconstantpool(classinfo *c);

#endif /* _CLASS_H */


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

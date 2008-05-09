/* src/native/vm/java_lang_Class.c - java/lang/Class

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

   $Id: java_lang_VMClass.c 6131 2006-12-06 22:15:57Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <string.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "native/jni.h"
#include "native/native.h"

#if defined(ENABLE_JAVAME_CLDC1_1)
# include "native/include/java_lang_String.h"/* required by java_lang_Class.h */
#endif

#include "native/include/java_lang_Class.h"
#include "native/include/java_lang_Object.h"

#if defined(ENABLE_JAVASE)
# include "native/include/java_lang_ClassLoader.h"
# include "native/include/java_lang_reflect_Constructor.h"
# include "native/include/java_lang_reflect_Field.h"
# include "native/include/java_lang_reflect_Method.h"
#endif

#include "native/vm/java_lang_Class.h"

#include "toolbox/logging.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/initialize.h"
#include "vm/stringlocal.h"

#include "vmcore/class.h"
#include "vmcore/loader.h"
#include "vm/resolve.h"


/*
 * Class:     java/lang/Class
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
java_lang_String *_Jv_java_lang_Class_getName(java_lang_Class *klass)
{
	classinfo        *c;
	java_lang_String *s;
	u4                i;

	c = (classinfo *) klass;

	/* create a java string */

	s = (java_lang_String *) javastring_new(c->name);

	if (s == NULL)
		return NULL;

	/* return string where '/' is replaced by '.' */

	for (i = 0; i < s->value->header.size; i++) {
		if (s->value->data[i] == '/')
			s->value->data[i] = '.';
	}

	return s;
}


/*
 * Class:     java/lang/Class
 * Method:    forName
 * Signature: (Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;
 */
#if defined(ENABLE_JAVASE)
java_lang_Class *_Jv_java_lang_Class_forName(java_lang_String *name, s4 initialize, java_lang_ClassLoader *loader)
#elif defined(ENABLE_JAVAME_CLDC1_1)
java_lang_Class *_Jv_java_lang_Class_forName(java_lang_String *name)
#endif
{
#if defined(ENABLE_JAVASE)
	java_objectheader *cl;
#endif
	utf               *ufile;
	utf               *uname;
	classinfo         *c;
	java_objectheader *xptr;
	classinfo         *xclass;
	u2                *pos;
	s4                 i;

#if defined(ENABLE_JAVASE)
	cl = (java_objectheader *) loader;
#endif

	/* illegal argument */

	if (name == NULL) {
		exceptions_throw_nullpointerexception();
		return NULL;
	}

	/* create utf string in which '.' is replaced by '/' */

	ufile = javastring_toutf((java_objectheader *) name, true);
	uname = javastring_toutf((java_objectheader *) name, false);

	/* name must not contain '/' (mauve test) */

	for (i = 0, pos = name->value->data + name->offset; i < name->count; i++, pos++) {
		if (*pos == '/') {
			exceptions_throw_classnotfoundexception(uname);
			return NULL;
		}
	}

	/* try to load, ... */

#if defined(ENABLE_JAVASE)
	c = load_class_from_classloader(ufile, cl);
#elif defined(ENABLE_JAVAME_CLDC1_1)
	c = load_class_bootstrap(ufile);
#endif

	if (c == NULL) {
		xptr = exceptions_get_exception();

		xclass = xptr->vftbl->class;

		/* if the exception is a NoClassDefFoundError, we replace it with a
		   ClassNotFoundException, otherwise return the exception */

		if (xclass == class_java_lang_NoClassDefFoundError) {
			/* clear exceptionptr, because builtin_new checks for 
			   ExceptionInInitializerError */
			exceptions_clear_exception();

			exceptions_throw_classnotfoundexception(uname);
		}

	    return NULL;
	}

	/* link, ... */

	if (!link_class(c))
		return NULL;
	
	/* ...and initialize it, if required */

#if defined(ENABLE_JAVASE)
	if (initialize)
#endif
		if (!initialize_class(c))
			return NULL;

	return (java_lang_Class *) c;
}


/*
 * Class:     java/lang/Class
 * Method:    isInstance
 * Signature: (Ljava/lang/Object;)Z
 */
s4 _Jv_java_lang_Class_isInstance(java_lang_Class *klass, java_lang_Object *o)
{
	classinfo         *c;
	java_objectheader *ob;

	c = (classinfo *) klass;
	ob = (java_objectheader *) o;

	if (!(c->state & CLASS_LINKED))
		if (!link_class(c))
			return 0;

	return builtin_instanceof(ob, c);
}


/*
 * Class:     java/lang/Class
 * Method:    isAssignableFrom
 * Signature: (Ljava/lang/Class;)Z
 */
s4 _Jv_java_lang_Class_isAssignableFrom(java_lang_Class *klass, java_lang_Class *c)
{
	classinfo *kc;
	classinfo *cc;

	kc = (classinfo *) klass;
	cc = (classinfo *) c;

	if (cc == NULL) {
		exceptions_throw_nullpointerexception();
		return 0;
	}

	if (!(kc->state & CLASS_LINKED))
		if (!link_class(kc))
			return 0;

	if (!(cc->state & CLASS_LINKED))
		if (!link_class(cc))
			return 0;

	return class_isanysubclass(cc, kc);
}


/*
 * Class:     java/lang/Class
 * Method:    isInterface
 * Signature: ()Z
 */
s4 _Jv_java_lang_Class_isInterface(java_lang_Class *klass)
{
	classinfo *c;

	c = (classinfo *) klass;

	if (c->flags & ACC_INTERFACE)
		return true;

	return false;
}


#if defined(ENABLE_JAVASE)

/*
 * Class:     java/lang/Class
 * Method:    isPrimitive
 * Signature: ()Z
 */
s4 _Jv_java_lang_Class_isPrimitive(java_lang_Class *klass)
{
	classinfo *c;
	s4         i;

	c = (classinfo *) klass;

	/* search table of primitive classes */

	for (i = 0; i < PRIMITIVETYPE_COUNT; i++)
		if (primitivetype_table[i].class_primitive == c)
			return true;

	return false;
}


/*
 * Class:     java/lang/Class
 * Method:    getSuperclass
 * Signature: ()Ljava/lang/Class;
 */
java_lang_Class *_Jv_java_lang_Class_getSuperclass(java_lang_Class *klass)
{
	classinfo *c;
	classinfo *sc;

	c = (classinfo *) klass;

	/* for java.lang.Object, primitive and Void classes we return NULL */

	if (!c->super.any)
		return NULL;

	/* for interfaces we also return NULL */

	if (c->flags & ACC_INTERFACE)
		return NULL;

	/* we may have to resolve the super class reference */

	if ((sc = resolve_classref_or_classinfo_eager(c->super, true)) == NULL)
		return NULL;

	/* store the resolution */

	c->super.cls = sc;

	return (java_lang_Class *) sc;
}


/*
 * Class:     java/lang/Class
 * Method:    getInterfaces
 * Signature: ()[Ljava/lang/Class;
 */
java_objectarray *_Jv_java_lang_Class_getInterfaces(java_lang_Class *klass)
{
	classinfo        *c;
	classinfo        *ic;
	java_objectarray *oa;
	u4                i;

	c = (classinfo *) klass;

	if (!(c->state & CLASS_LINKED))
		if (!link_class(c))
			return NULL;

	oa = builtin_anewarray(c->interfacescount, class_java_lang_Class);

	if (oa == NULL)
		return NULL;

	for (i = 0; i < c->interfacescount; i++) {
		ic = c->interfaces[i].cls;

		oa->data[i] = (java_objectheader *) ic;
	}

	return oa;
}


/*
 * Class:     java/lang/Class
 * Method:    getComponentType
 * Signature: ()Ljava/lang/Class;
 */
java_lang_Class *_Jv_java_lang_Class_getComponentType(java_lang_Class *klass)
{
	classinfo       *c;
	classinfo       *comp;
	arraydescriptor *desc;
	
	c = (classinfo *) klass;
	
	/* XXX maybe we could find a way to do this without linking. */
	/* This way should be safe and easy, however.                */

	if (!(c->state & CLASS_LINKED))
		if (!link_class(c))
			return NULL;

	desc = c->vftbl->arraydesc;
	
	if (desc == NULL)
		return NULL;
	
	if (desc->arraytype == ARRAYTYPE_OBJECT)
		comp = desc->componentvftbl->class;
	else
		comp = primitivetype_table[desc->arraytype].class_primitive;
		
	return (java_lang_Class *) comp;
}


/*
 * Class:     java/lang/Class
 * Method:    getModifiers
 * Signature: (Z)I
 */
s4 _Jv_java_lang_Class_getModifiers(java_lang_Class *klass, s4 ignoreInnerClassesAttrib)
{
	classinfo             *c;
	classref_or_classinfo  inner;
	classref_or_classinfo  outer;
	utf                   *innername;
	s4                     i;

	c = (classinfo *) klass;

	if (!ignoreInnerClassesAttrib && (c->innerclasscount != 0)) {
		/* search for passed class as inner class */

		for (i = 0; i < c->innerclasscount; i++) {
			inner = c->innerclass[i].inner_class;
			outer = c->innerclass[i].outer_class;

			/* Check if inner is a classref or a real class and get
               the name of the structure */

			innername = IS_CLASSREF(inner) ? inner.ref->name : inner.cls->name;

			/* innerclass is this class */

			if (innername == c->name) {
				/* has the class actually an outer class? */

				if (outer.any)
					/* return flags got from the outer class file */
					return c->innerclass[i].flags & ACC_CLASS_REFLECT_MASK;
				else
					return c->flags & ACC_CLASS_REFLECT_MASK;
			}
		}
	}

	/* passed class is no inner class or it was not requested */

	return c->flags & ACC_CLASS_REFLECT_MASK;
}


/*
 * Class:     java/lang/Class
 * Method:    getDeclaringClass
 * Signature: ()Ljava/lang/Class;
 */
java_lang_Class *_Jv_java_lang_Class_getDeclaringClass(java_lang_Class *klass)
{
	classinfo             *c;
	classref_or_classinfo  inner;
	utf                   *innername;
	classinfo             *outer;
	s4                     i;

	c = (classinfo *) klass;

	if (!_Jv_java_lang_Class_isPrimitive(klass) && (c->name->text[0] != '[')) {
		if (c->innerclasscount == 0)  /* no innerclasses exist */
			return NULL;
    
		for (i = 0; i < c->innerclasscount; i++) {
			inner = c->innerclass[i].inner_class;

			/* check if inner_class is a classref or a real class and
               get the class name from the structure */

			innername = IS_CLASSREF(inner) ? inner.ref->name : inner.cls->name;

			/* innerclass is this class */

			if (innername == c->name) {
				/* maybe the outer class is not loaded yet */

				if ((outer = resolve_classref_or_classinfo_eager(
								c->innerclass[i].outer_class,
								false)) == NULL)
					return NULL;

				if (!(outer->state & CLASS_LINKED))
					if (!link_class(outer))
						return NULL;

				return (java_lang_Class *) outer;
			}
		}
	}

	/* return NULL for arrayclasses and primitive classes */

	return NULL;
}


/*
 * Class:     java/lang/Class
 * Method:    getDeclaredClasses
 * Signature: (Z)[Ljava/lang/Class;
 */
java_objectarray *_Jv_java_lang_Class_getDeclaredClasses(java_lang_Class *klass, s4 publicOnly)
{
	classinfo             *c;
	classref_or_classinfo  outer;
	utf                   *outername;
	s4                     declaredclasscount;  /* number of declared classes */
	s4                     pos;                     /* current declared class */
	java_objectarray      *oa;                   /* array of declared classes */
	s4                     i;

	c = (classinfo *) klass;
	declaredclasscount = 0;

	if (!_Jv_java_lang_Class_isPrimitive(klass) && (c->name->text[0] != '[')) {
		/* determine number of declared classes */

		for (i = 0; i < c->innerclasscount; i++) {
			outer = c->innerclass[i].outer_class;

			/* check if outer_class is a classref or a real class and
               get the class name from the structure */

			outername = IS_CLASSREF(outer) ? outer.ref->name : outer.cls->name;

			/* outer class is this class */

			if ((outername == c->name) &&
				((publicOnly == 0) || (c->innerclass[i].flags & ACC_PUBLIC)))
				declaredclasscount++;
		}
	}

	/* allocate Class[] and check for OOM */

	oa = builtin_anewarray(declaredclasscount, class_java_lang_Class);

	if (oa == NULL)
		return NULL;

	for (i = 0, pos = 0; i < c->innerclasscount; i++) {
		outer = c->innerclass[i].outer_class;

		/* check if outer_class is a classref or a real class and
		   get the class name from the structure */

		outername = IS_CLASSREF(outer) ? outer.ref->name : outer.cls->name;

		/* outer class is this class */

		if ((outername == c->name) &&
			((publicOnly == 0) || (c->innerclass[i].flags & ACC_PUBLIC))) {
			classinfo *inner;

			if ((inner = resolve_classref_or_classinfo_eager(
											   c->innerclass[i].inner_class,
											   false)) == NULL)
				return NULL;

			if (!(inner->state & CLASS_LINKED))
				if (!link_class(inner))
					return NULL;

			oa->data[pos++] = (java_objectheader *) inner;
		}
	}

	return oa;
}


/*
 * Class:     java/lang/Class
 * Method:    getDeclaredFields
 * Signature: (Z)[Ljava/lang/reflect/Field;
 */
java_objectarray *_Jv_java_lang_Class_getDeclaredFields(java_lang_Class *klass, s4 publicOnly)
{
	classinfo               *c;
	java_objectarray        *oa;            /* result: array of field-objects */
	fieldinfo               *f;
	java_objectheader       *o;
	java_lang_reflect_Field *rf;
	s4 public_fields;                    /* number of elements in field-array */
	s4 pos;
	s4 i;

	c = (classinfo *) klass;

	/* determine number of fields */

	for (i = 0, public_fields = 0; i < c->fieldscount; i++)
		if ((c->fields[i].flags & ACC_PUBLIC) || (publicOnly == 0))
			public_fields++;

	/* create array of fields */

	oa = builtin_anewarray(public_fields, class_java_lang_reflect_Field);

	if (oa == NULL)
		return NULL;

	/* get the fields and store in the array */

	for (i = 0, pos = 0; i < c->fieldscount; i++) {
		f = &(c->fields[i]);

		if ((f->flags & ACC_PUBLIC) || (publicOnly == 0)) {
			/* create Field object */

			o = native_new_and_init(class_java_lang_reflect_Field);

			if (o == NULL)
				return NULL;

			/* initialize instance fields */

			rf = (java_lang_reflect_Field *) o;

			rf->declaringClass = (java_lang_Class *) c;
			rf->name           = (java_lang_String *) javastring_new(f->name);
			rf->slot           = i;

			/* store object into array */

			oa->data[pos++] = o;
		}
	}

	return oa;
}


/*
 * Class:     java/lang/Class
 * Method:    getDeclaredMethods
 * Signature: (Z)[Ljava/lang/reflect/Method;
 */
java_objectarray *_Jv_java_lang_Class_getDeclaredMethods(java_lang_Class *klass, s4 publicOnly)
{
	classinfo                *c;
	java_objectheader        *o;
	java_lang_reflect_Method *rm;
	java_objectarray         *oa;          /* result: array of Method-objects */
	methodinfo               *m;      /* the current method to be represented */
	s4 public_methods;               /* number of public methods of the class */
	s4 pos;
	s4 i;

	c = (classinfo *) klass;    
	public_methods = 0;

	/* JOWENN: array classes do not declare methods according to mauve
	   test.  It should be considered, if we should return to my old
	   clone method overriding instead of declaring it as a member
	   function. */

	if (_Jv_java_lang_Class_isArray(klass))
		return builtin_anewarray(0, class_java_lang_reflect_Method);

	/* determine number of methods */

	for (i = 0; i < c->methodscount; i++) {
		m = &c->methods[i];

		if (((m->flags & ACC_PUBLIC) || (publicOnly == false)) &&
			((m->name != utf_init) && (m->name != utf_clinit)) &&
			!(m->flags & ACC_MIRANDA))
			public_methods++;
	}

	oa = builtin_anewarray(public_methods, class_java_lang_reflect_Method);

	if (oa == NULL)
		return NULL;

	for (i = 0, pos = 0; i < c->methodscount; i++) {
		m = &c->methods[i];

		if (((m->flags & ACC_PUBLIC) || (publicOnly == false)) && 
			((m->name != utf_init) && (m->name != utf_clinit)) &&
			!(m->flags & ACC_MIRANDA)) {

			o = native_new_and_init(class_java_lang_reflect_Method);

			if (o == NULL)
				return NULL;

			/* initialize instance fields */

			rm = (java_lang_reflect_Method *) o;

			rm->declaringClass = (java_lang_Class *) m->class;
			rm->name           = (java_lang_String *) javastring_new(m->name);
			rm->slot           = i;

			/* store object into array */

			oa->data[pos++] = o;
		}
	}

	return oa;
}


/*
 * Class:     java/lang/Class
 * Method:    getDeclaredConstructors
 * Signature: (Z)[Ljava/lang/reflect/Constructor;
 */
java_objectarray *_Jv_java_lang_Class_getDeclaredConstructors(java_lang_Class *klass, s4 publicOnly)
{
	classinfo                     *c;
	methodinfo                    *m; /* the current method to be represented */
	java_objectarray              *oa;     /* result: array of Method-objects */
	java_objectheader             *o;
	java_lang_reflect_Constructor *rc;
	s4 public_methods;               /* number of public methods of the class */
	s4 pos;
	s4 i;

	c = (classinfo *) klass;

	/* determine number of constructors */

	for (i = 0, public_methods = 0; i < c->methodscount; i++) {
		m = &c->methods[i];

		if (((m->flags & ACC_PUBLIC) || (publicOnly == 0)) &&
			(m->name == utf_init))
			public_methods++;
	}

	oa = builtin_anewarray(public_methods, class_java_lang_reflect_Constructor);

	if (oa == NULL)
		return NULL;

	for (i = 0, pos = 0; i < c->methodscount; i++) {
		m = &c->methods[i];

		if (((m->flags & ACC_PUBLIC) || (publicOnly == 0)) &&
			(m->name == utf_init)) {

			o = native_new_and_init(class_java_lang_reflect_Constructor);

			if (o == NULL)
				return NULL;

			/* initialize instance fields */

			rc = (java_lang_reflect_Constructor *) o;

			rc->clazz = (java_lang_Class *) c;
			rc->slot  = i;

			/* store object into array */

			oa->data[pos++] = o;
		}
	}

	return oa;
}


/*
 * Class:     java/lang/Class
 * Method:    getClassLoader
 * Signature: ()Ljava/lang/ClassLoader;
 */
java_lang_ClassLoader *_Jv_java_lang_Class_getClassLoader(java_lang_Class *klass)
{
	classinfo *c;

	c = (classinfo *) klass;

	return (java_lang_ClassLoader *) c->classloader;
}

#endif /* defined(ENABLE_JAVASE) */


/*
 * Class:     java/lang/Class
 * Method:    isArray
 * Signature: ()Z
 */
s4 _Jv_java_lang_Class_isArray(java_lang_Class *klass)
{
	classinfo *c;

	c = (classinfo *) klass;

	if (!(c->state & CLASS_LINKED))
		if (!link_class(c))
			return 0;

	return (c->vftbl->arraydesc != NULL);
}


#if defined(ENABLE_JAVASE)

/*
 * Class:     java/lang/Class
 * Method:    throwException
 * Signature: (Ljava/lang/Throwable;)V
 */
void _Jv_java_lang_Class_throwException(java_lang_Throwable *t)
{
	java_objectheader *o;

	o = (java_objectheader *) t;

	exceptions_set_exception(o);
}


#if 0
/*
 * Class:     java/lang/Class
 * Method:    getDeclaredAnnotations
 * Signature: (Ljava/lang/Class;)[Ljava/lang/annotation/Annotation;
 */
java_objectarray *_Jv_java_lang_Class_getDeclaredAnnotations(java_lang_Class* klass)
{
}
#endif


/*
 * Class:     java/lang/Class
 * Method:    getEnclosingClass
 * Signature: (Ljava/lang/Class;)Ljava/lang/Class;
 */
java_lang_Class *_Jv_java_lang_Class_getEnclosingClass(java_lang_Class *klass)
{
	classinfo             *c;
	classref_or_classinfo  cr;
	classinfo             *ec;

	c = (classinfo *) klass;

	/* get enclosing class */

	cr = c->enclosingclass;

	if (cr.any == NULL)
		return NULL;

	/* resolve the class if necessary */

	if (IS_CLASSREF(cr)) {
		ec = resolve_classref_eager(cr.ref);

		if (ec == NULL)
			return NULL;
	}
	else
		ec = cr.cls;

	return (java_lang_Class *) ec;
}


/* _Jv_java_lang_Class_getEnclosingMethod_intern *******************************

   Helper function for _Jv_java_lang_Class_getEnclosingConstructor and
   _Jv_java_lang_Class_getEnclosingMethod.

*******************************************************************************/

static methodinfo *_Jv_java_lang_Class_getEnclosingMethod_intern(classinfo *c)
{
	classref_or_classinfo     cr;
	constant_nameandtype     *cn;
	classinfo                *ec;
	methodinfo               *m;

	/* get enclosing class and method */

	cr = c->enclosingclass;
	cn = c->enclosingmethod;

	/* check for enclosing class and method */

	if (cr.any == NULL)
		return NULL;

	if (cn == NULL)
		return NULL;

	/* resolve the class if necessary */

	if (IS_CLASSREF(cr)) {
		ec = resolve_classref_eager(cr.ref);

		if (ec == NULL)
			return NULL;
	}
	else
		ec = cr.cls;

	/* find method in enclosing class */

	m = class_findmethod(ec, cn->name, cn->descriptor);

	if (m == NULL) {
		exceptions_throw_internalerror("Enclosing method doesn't exist");
		return NULL;
	}

	return m;
}


/*
 * Class:     java/lang/Class
 * Method:    getEnclosingConstructor
 * Signature: (Ljava/lang/Class;)Ljava/lang/reflect/Constructor;
 */
java_lang_reflect_Constructor *_Jv_java_lang_Class_getEnclosingConstructor(java_lang_Class *klass)
{
	classinfo                     *c;
	methodinfo                    *m;
	java_objectheader             *o;
	java_lang_reflect_Constructor *rc;

	c = (classinfo *) klass;

	/* get enclosing method */

	m = _Jv_java_lang_Class_getEnclosingMethod_intern(c);

	if (m == NULL)
		return NULL;

	/* check for <init> */

	if (m->name != utf_init)
		return NULL;

	/* create java.lang.reflect.Constructor object */

	o = native_new_and_init(class_java_lang_reflect_Constructor);

	if (o == NULL)
		return NULL;

	/* initialize instance fields */

	rc = (java_lang_reflect_Constructor *) o;

	rc->clazz = (java_lang_Class *) m->class;
	rc->slot  = m - m->class->methods;               /* calculate method slot */

	return rc;
}


/*
 * Class:     java/lang/Class
 * Method:    getEnclosingMethod
 * Signature: (Ljava/lang/Class;)Ljava/lang/reflect/Method;
 */
java_lang_reflect_Method *_Jv_java_lang_Class_getEnclosingMethod(java_lang_Class *klass)
{
	classinfo                *c;
	methodinfo               *m;
	java_objectheader        *o;
	java_lang_reflect_Method *rm;

	c = (classinfo *) klass;

	/* get enclosing method */

	m = _Jv_java_lang_Class_getEnclosingMethod_intern(c);

	if (m == NULL)
		return NULL;

	/* check for <init> */

	if (m->name == utf_init)
		return NULL;

	/* create java.lang.reflect.Method object */

	o = native_new_and_init(class_java_lang_reflect_Method);

	if (o == NULL)
		return NULL;

	/* initialize instance fields */

	rm = (java_lang_reflect_Method *) o;

	rm->declaringClass = (java_lang_Class *) m->class;
	rm->name           = (java_lang_String *) javastring_new(m->name);
	rm->slot           = m - m->class->methods;      /* calculate method slot */

	return rm;
}


/*
 * Class:     java/lang/Class
 * Method:    getClassSignature
 * Signature: (Ljava/lang/Class;)Ljava/lang/String;
 */
java_lang_String *_Jv_java_lang_Class_getClassSignature(java_lang_Class* klass)
{
	classinfo         *c;
	java_objectheader *o;

	c = (classinfo *) klass;

	if (c->signature == NULL)
		return NULL;

	o = javastring_new(c->signature);

	/* in error case o is NULL */

	return (java_lang_String *) o;
}


#if 0
/*
 * Class:     java/lang/Class
 * Method:    isAnonymousClass
 * Signature: (Ljava/lang/Class;)Z
 */
s4 _Jv_java_lang_Class_isAnonymousClass(JNIEnv *env, jclass clazz, struct java_lang_Class* par1);


/*
 * Class:     java/lang/VMClass
 * Method:    isLocalClass
 * Signature: (Ljava/lang/Class;)Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isLocalClass(JNIEnv *env, jclass clazz, struct java_lang_Class* par1);


/*
 * Class:     java/lang/VMClass
 * Method:    isMemberClass
 * Signature: (Ljava/lang/Class;)Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isMemberClass(JNIEnv *env, jclass clazz, struct java_lang_Class* par1);
#endif

#endif /* ENABLE_JAVASE */


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

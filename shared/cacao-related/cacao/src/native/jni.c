/* src/native/jni.c - implementation of the Java Native Interface functions

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

   $Id: jni.c 7813 2007-04-25 19:20:13Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <string.h>

#include "vm/types.h"

#include "mm/gc-common.h"
#include "mm/memory.h"
#include "native/jni.h"
#include "native/native.h"

#if defined(ENABLE_JAVASE)
# if defined(WITH_CLASSPATH_GNU)
#  include "native/include/gnu_classpath_Pointer.h"

#  if SIZEOF_VOID_P == 8
#   include "native/include/gnu_classpath_Pointer64.h"
#  else
#   include "native/include/gnu_classpath_Pointer32.h"
#  endif
# endif
#endif

#include "native/include/java_lang_Object.h"
#include "native/include/java_lang_Byte.h"
#include "native/include/java_lang_Character.h"
#include "native/include/java_lang_Short.h"
#include "native/include/java_lang_Integer.h"
#include "native/include/java_lang_Boolean.h"
#include "native/include/java_lang_Long.h"
#include "native/include/java_lang_Float.h"
#include "native/include/java_lang_Double.h"
#include "native/include/java_lang_String.h"
#include "native/include/java_lang_Throwable.h"

#if defined(ENABLE_JAVASE)
# include "native/include/java_lang_ClassLoader.h"

# include "native/include/java_lang_reflect_Constructor.h"
# include "native/include/java_lang_reflect_Field.h"
# include "native/include/java_lang_reflect_Method.h"

# include "native/include/java_nio_Buffer.h"
# include "native/include/java_nio_DirectByteBufferImpl.h"
#endif

#if defined(ENABLE_JVMTI)
# include "native/jvmti/cacaodbg.h"
#endif

#include "native/vm/java_lang_Class.h"

#if defined(ENABLE_JAVASE)
# include "native/vm/java_lang_ClassLoader.h"
#endif

#include "threads/lock-common.h"
#include "threads/threads-common.h"

#include "toolbox/logging.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/initialize.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/jit.h"
#include "vm/jit/stacktrace.h"

#include "vmcore/loader.h"
#include "vmcore/options.h"
#include "vm/resolve.h"
#include "vmcore/statistics.h"


/* global variables ***********************************************************/

/* global reference table *****************************************************/

/* hashsize must be power of 2 */

#define HASHTABLE_GLOBAL_REF_SIZE    64 /* initial size of globalref-hash     */

static hashtable *hashtable_global_ref; /* hashtable for globalrefs           */


/* direct buffer stuff ********************************************************/

#if defined(ENABLE_JAVASE)
static classinfo *class_java_nio_Buffer;
static classinfo *class_java_nio_DirectByteBufferImpl;
static classinfo *class_java_nio_DirectByteBufferImpl_ReadWrite;

# if defined(WITH_CLASSPATH_GNU)
#  if SIZEOF_VOID_P == 8
static classinfo *class_gnu_classpath_Pointer64;
#  else
static classinfo *class_gnu_classpath_Pointer32;
#  endif
# endif

static methodinfo *dbbirw_init;
#endif


/* local reference table ******************************************************/

#if !defined(ENABLE_THREADS)
localref_table *_no_threads_localref_table;
#endif


/* accessing instance fields macros *******************************************/

#define SET_FIELD(o,type,f,value) \
    *((type *) ((ptrint) (o) + (ptrint) ((fieldinfo *) (f))->offset)) = (type) (value)

#define GET_FIELD(o,type,f) \
    *((type *) ((ptrint) (o) + (ptrint) ((fieldinfo *) (f))->offset))


/* some forward declarations **************************************************/

jobject _Jv_JNI_NewLocalRef(JNIEnv *env, jobject ref);
jint _Jv_JNI_EnsureLocalCapacity(JNIEnv* env, jint capacity);


/* jni_init ********************************************************************

   Initialize the JNI subsystem.

*******************************************************************************/

bool jni_init(void)
{
	/* create global ref hashtable */

	hashtable_global_ref = NEW(hashtable);

	hashtable_create(hashtable_global_ref, HASHTABLE_GLOBAL_REF_SIZE);


#if defined(ENABLE_JAVASE)
	/* direct buffer stuff */

	if (!(class_java_nio_Buffer =
		  load_class_bootstrap(utf_new_char("java/nio/Buffer"))) ||
		!link_class(class_java_nio_Buffer))
		return false;

	if (!(class_java_nio_DirectByteBufferImpl =
		  load_class_bootstrap(utf_new_char("java/nio/DirectByteBufferImpl"))) ||
		!link_class(class_java_nio_DirectByteBufferImpl))
		return false;

	if (!(class_java_nio_DirectByteBufferImpl_ReadWrite =
		  load_class_bootstrap(utf_new_char("java/nio/DirectByteBufferImpl$ReadWrite"))) ||
		!link_class(class_java_nio_DirectByteBufferImpl_ReadWrite))
		return false;

	if (!(dbbirw_init =
		class_resolvemethod(class_java_nio_DirectByteBufferImpl_ReadWrite,
							utf_init,
							utf_new_char("(Ljava/lang/Object;Lgnu/classpath/Pointer;III)V"))))
		return false;

# if defined(WITH_CLASSPATH_GNU)
#  if SIZEOF_VOID_P == 8
	if (!(class_gnu_classpath_Pointer64 =
		  load_class_bootstrap(utf_new_char("gnu/classpath/Pointer64"))) ||
		!link_class(class_gnu_classpath_Pointer64))
		return false;
#  else
	if (!(class_gnu_classpath_Pointer32 =
		  load_class_bootstrap(utf_new_char("gnu/classpath/Pointer32"))) ||
		!link_class(class_gnu_classpath_Pointer32))
		return false;
#  endif
# endif
#endif /* defined(ENABLE_JAVASE) */

	return true;
}


/* jni_init_localref_table *****************************************************

   Initializes the local references table of the current thread.

*******************************************************************************/

bool jni_init_localref_table(void)
{
	localref_table *lrt;

	lrt = GCNEW(localref_table);

	if (lrt == NULL)
		return false;

	lrt->capacity    = LOCALREFTABLE_CAPACITY;
	lrt->used        = 0;
	lrt->localframes = 1;
	lrt->prev        = LOCALREFTABLE;

	/* clear the references array (memset is faster then a for-loop) */

	MSET(lrt->refs, 0, java_objectheader*, LOCALREFTABLE_CAPACITY);

	LOCALREFTABLE = lrt;

	return true;
}


/* _Jv_jni_vmargs_from_objectarray *********************************************

   XXX

*******************************************************************************/

static bool _Jv_jni_vmargs_from_objectarray(java_objectheader *o,
											methoddesc *descr,
											vm_arg *vmargs,
											java_objectarray *params)
{
	java_objectheader *param;
	s4                 paramcount;
	typedesc          *paramtypes;
	classinfo         *c;
	s4                 i;
	s4                 j;
	s8                 value;

	paramcount = descr->paramcount;
	paramtypes = descr->paramtypes;

	/* if method is non-static fill first block and skip `this' pointer */

	i = 0;

	if (o != NULL) {
		/* this pointer */
		vmargs[0].type   = TYPE_ADR;
		vmargs[0].data.l = (u8) (ptrint) o;

		paramtypes++;
		paramcount--;
		i++;
	}

	for (j = 0; j < paramcount; i++, j++, paramtypes++) {
		switch (paramtypes->type) {
		/* primitive types */
		case TYPE_INT:
		case TYPE_LNG:
		case TYPE_FLT:
		case TYPE_DBL:
			param = params->data[j];

			if (param == NULL)
				goto illegal_arg;

			/* internally used data type */
			vmargs[i].type = paramtypes->type;

			/* convert the value according to its declared type */

			c = param->vftbl->class;

			switch (paramtypes->decltype) {
			case PRIMITIVETYPE_BOOLEAN:
				if (c == primitivetype_table[paramtypes->decltype].class_wrap)
					value = (s8) ((java_lang_Boolean *) param)->value;
				else
					goto illegal_arg;

				vmargs[i].data.l = value;
				break;

			case PRIMITIVETYPE_BYTE:
				if (c == primitivetype_table[paramtypes->decltype].class_wrap)
					value = (s8) ((java_lang_Byte *) param)->value;
				else
					goto illegal_arg;

				vmargs[i].data.l = value;
				break;

			case PRIMITIVETYPE_CHAR:
				if (c == primitivetype_table[paramtypes->decltype].class_wrap)
					value = (s8) ((java_lang_Character *) param)->value;
				else
					goto illegal_arg;

				vmargs[i].data.l = value;
				break;

			case PRIMITIVETYPE_SHORT:
				if (c == primitivetype_table[paramtypes->decltype].class_wrap)
					value = (s8) ((java_lang_Short *) param)->value;
				else if (c == primitivetype_table[PRIMITIVETYPE_BYTE].class_wrap)
					value = (s8) ((java_lang_Byte *) param)->value;
				else
					goto illegal_arg;

				vmargs[i].data.l = value;
				break;

			case PRIMITIVETYPE_INT:
				if (c == primitivetype_table[paramtypes->decltype].class_wrap)
					value = (s8) ((java_lang_Integer *) param)->value;
				else if (c == primitivetype_table[PRIMITIVETYPE_SHORT].class_wrap)
					value = (s8) ((java_lang_Short *) param)->value;
				else if (c == primitivetype_table[PRIMITIVETYPE_BYTE].class_wrap)
					value = (s8) ((java_lang_Byte *) param)->value;
				else
					goto illegal_arg;

				vmargs[i].data.l = value;
				break;

			case PRIMITIVETYPE_LONG:
				if (c == primitivetype_table[paramtypes->decltype].class_wrap)
					value = (s8) ((java_lang_Long *) param)->value;
				else if (c == primitivetype_table[PRIMITIVETYPE_INT].class_wrap)
					value = (s8) ((java_lang_Integer *) param)->value;
				else if (c == primitivetype_table[PRIMITIVETYPE_SHORT].class_wrap)
					value = (s8) ((java_lang_Short *) param)->value;
				else if (c == primitivetype_table[PRIMITIVETYPE_BYTE].class_wrap)
					value = (s8) ((java_lang_Byte *) param)->value;
				else
					goto illegal_arg;

				vmargs[i].data.l = value;
				break;

			case PRIMITIVETYPE_FLOAT:
				if (c == primitivetype_table[paramtypes->decltype].class_wrap)
					vmargs[i].data.f = (jfloat) ((java_lang_Float *) param)->value;
				else
					goto illegal_arg;
				break;

			case PRIMITIVETYPE_DOUBLE:
				if (c == primitivetype_table[paramtypes->decltype].class_wrap)
					vmargs[i].data.d = (jdouble) ((java_lang_Double *) param)->value;
				else if (c == primitivetype_table[PRIMITIVETYPE_FLOAT].class_wrap)
					vmargs[i].data.f = (jfloat) ((java_lang_Float *) param)->value;
				else
					goto illegal_arg;
				break;

			default:
				goto illegal_arg;
			}
			break;
		
			case TYPE_ADR:
				if (!resolve_class_from_typedesc(paramtypes, true, true, &c))
					return false;

				if (params->data[j] != 0) {
					if (paramtypes->arraydim > 0) {
						if (!builtin_arrayinstanceof(params->data[j], c))
							goto illegal_arg;

					} else {
						if (!builtin_instanceof(params->data[j], c))
							goto illegal_arg;
					}
				}

				vmargs[i].type   = TYPE_ADR;
				vmargs[i].data.l = (u8) (ptrint) params->data[j];
				break;

			default:
				goto illegal_arg;
		}
	}

/*  	if (rettype) */
/*  		*rettype = descr->returntype.decltype; */

	return true;

illegal_arg:
	exceptions_throw_illegalargumentexception();
	return false;
}


/* _Jv_jni_CallObjectMethod ****************************************************

   Internal function to call Java Object methods.

*******************************************************************************/

static java_objectheader *_Jv_jni_CallObjectMethod(java_objectheader *o,
												   vftbl_t *vftbl,
												   methodinfo *m, va_list ap)
{
	methodinfo        *resm;
	java_objectheader *ro;

	STATISTICS(jniinvokation());

	if (m == NULL) {
		exceptions_throw_nullpointerexception();
		return NULL;
	}

	/* Class initialization is done by the JIT compiler.  This is ok
	   since a static method always belongs to the declaring class. */

	if (m->flags & ACC_STATIC) {
		/* For static methods we reset the object. */

		if (o != NULL)
			o = NULL;

		/* for convenience */

		resm = m;

	} else {
		/* For instance methods we make a virtual function table lookup. */

		resm = method_vftbl_lookup(vftbl, m);
	}

	STATISTICS(jnicallXmethodnvokation());

	ro = vm_call_method_valist(resm, o, ap);

	return ro;
}


/* _Jv_jni_CallObjectMethodA ***************************************************

   Internal function to call Java Object methods.

*******************************************************************************/

static java_objectheader *_Jv_jni_CallObjectMethodA(java_objectheader *o,
													vftbl_t *vftbl,
													methodinfo *m, jvalue *args)
{
	methodinfo        *resm;
	java_objectheader *ro;

	STATISTICS(jniinvokation());

	if (m == NULL) {
		exceptions_throw_nullpointerexception();
		return NULL;
	}

	/* Class initialization is done by the JIT compiler.  This is ok
	   since a static method always belongs to the declaring class. */

	if (m->flags & ACC_STATIC) {
		/* For static methods we reset the object. */

		if (o != NULL)
			o = NULL;

		/* for convenience */

		resm = m;

	} else {
		/* For instance methods we make a virtual function table lookup. */

		resm = method_vftbl_lookup(vftbl, m);
	}

	STATISTICS(jnicallXmethodnvokation());

	ro = vm_call_method_jvalue(resm, o, args);

	return ro;
}


/* _Jv_jni_CallIntMethod *******************************************************

   Internal function to call Java integer class methods (boolean,
   byte, char, short, int).

*******************************************************************************/

static jint _Jv_jni_CallIntMethod(java_objectheader *o, vftbl_t *vftbl,
								  methodinfo *m, va_list ap)
{
	methodinfo *resm;
	jint        i;

	STATISTICS(jniinvokation());

	if (m == NULL) {
		exceptions_throw_nullpointerexception();
		return 0;
	}
        
	/* Class initialization is done by the JIT compiler.  This is ok
	   since a static method always belongs to the declaring class. */

	if (m->flags & ACC_STATIC) {
		/* For static methods we reset the object. */

		if (o != NULL)
			o = NULL;

		/* for convenience */

		resm = m;

	} else {
		/* For instance methods we make a virtual function table lookup. */

		resm = method_vftbl_lookup(vftbl, m);
	}

	STATISTICS(jnicallXmethodnvokation());

	i = vm_call_method_int_valist(resm, o, ap);

	return i;
}


/* _Jv_jni_CallIntMethodA ******************************************************

   Internal function to call Java integer class methods (boolean,
   byte, char, short, int).

*******************************************************************************/

static jint _Jv_jni_CallIntMethodA(java_objectheader *o, vftbl_t *vftbl,
								   methodinfo *m, jvalue *args)
{
	methodinfo *resm;
	jint        i;

	STATISTICS(jniinvokation());

	if (m == NULL) {
		exceptions_throw_nullpointerexception();
		return 0;
	}
        
	/* Class initialization is done by the JIT compiler.  This is ok
	   since a static method always belongs to the declaring class. */

	if (m->flags & ACC_STATIC) {
		/* For static methods we reset the object. */

		if (o != NULL)
			o = NULL;

		/* for convenience */

		resm = m;

	} else {
		/* For instance methods we make a virtual function table lookup. */

		resm = method_vftbl_lookup(vftbl, m);
	}

	STATISTICS(jnicallXmethodnvokation());

	i = vm_call_method_int_jvalue(resm, o, args);

	return i;
}


/* _Jv_jni_CallLongMethod ******************************************************

   Internal function to call Java long methods.

*******************************************************************************/

static jlong _Jv_jni_CallLongMethod(java_objectheader *o, vftbl_t *vftbl,
									methodinfo *m, va_list ap)
{
	methodinfo *resm;
	jlong       l;

	STATISTICS(jniinvokation());

	if (m == NULL) {
		exceptions_throw_nullpointerexception();
		return 0;
	}

	/* Class initialization is done by the JIT compiler.  This is ok
	   since a static method always belongs to the declaring class. */

	if (m->flags & ACC_STATIC) {
		/* For static methods we reset the object. */

		if (o != NULL)
			o = NULL;

		/* for convenience */

		resm = m;

	} else {
		/* For instance methods we make a virtual function table lookup. */

		resm = method_vftbl_lookup(vftbl, m);
	}

	STATISTICS(jnicallXmethodnvokation());

	l = vm_call_method_long_valist(resm, o, ap);

	return l;
}


/* _Jv_jni_CallFloatMethod *****************************************************

   Internal function to call Java float methods.

*******************************************************************************/

static jfloat _Jv_jni_CallFloatMethod(java_objectheader *o, vftbl_t *vftbl,
									  methodinfo *m, va_list ap)
{
	methodinfo *resm;
	jfloat      f;

	/* Class initialization is done by the JIT compiler.  This is ok
	   since a static method always belongs to the declaring class. */

	if (m->flags & ACC_STATIC) {
		/* For static methods we reset the object. */

		if (o != NULL)
			o = NULL;

		/* for convenience */

		resm = m;

	} else {
		/* For instance methods we make a virtual function table lookup. */

		resm = method_vftbl_lookup(vftbl, m);
	}

	STATISTICS(jnicallXmethodnvokation());

	f = vm_call_method_float_valist(resm, o, ap);

	return f;
}


/* _Jv_jni_CallDoubleMethod ****************************************************

   Internal function to call Java double methods.

*******************************************************************************/

static jdouble _Jv_jni_CallDoubleMethod(java_objectheader *o, vftbl_t *vftbl,
										methodinfo *m, va_list ap)
{
	methodinfo *resm;
	jdouble     d;

	/* Class initialization is done by the JIT compiler.  This is ok
	   since a static method always belongs to the declaring class. */

	if (m->flags & ACC_STATIC) {
		/* For static methods we reset the object. */

		if (o != NULL)
			o = NULL;

		/* for convenience */

		resm = m;

	} else {
		/* For instance methods we make a virtual function table lookup. */

		resm = method_vftbl_lookup(vftbl, m);
	}

	d = vm_call_method_double_valist(resm, o, ap);

	return d;
}


/* _Jv_jni_CallVoidMethod ******************************************************

   Internal function to call Java void methods.

*******************************************************************************/

static void _Jv_jni_CallVoidMethod(java_objectheader *o, vftbl_t *vftbl,
								   methodinfo *m, va_list ap)
{ 	
	methodinfo *resm;

	if (m == NULL) {
		exceptions_throw_nullpointerexception();
		return;
	}

	/* Class initialization is done by the JIT compiler.  This is ok
	   since a static method always belongs to the declaring class. */

	if (m->flags & ACC_STATIC) {
		/* For static methods we reset the object. */

		if (o != NULL)
			o = NULL;

		/* for convenience */

		resm = m;

	} else {
		/* For instance methods we make a virtual function table lookup. */

		resm = method_vftbl_lookup(vftbl, m);
	}

	STATISTICS(jnicallXmethodnvokation());

	(void) vm_call_method_valist(resm, o, ap);
}


/* _Jv_jni_CallVoidMethodA *****************************************************

   Internal function to call Java void methods.

*******************************************************************************/

static void _Jv_jni_CallVoidMethodA(java_objectheader *o, vftbl_t *vftbl,
									methodinfo *m, jvalue *args)
{ 	
	methodinfo *resm;

	if (m == NULL) {
		exceptions_throw_nullpointerexception();
		return;
	}

	/* Class initialization is done by the JIT compiler.  This is ok
	   since a static method always belongs to the declaring class. */

	if (m->flags & ACC_STATIC) {
		/* For static methods we reset the object. */

		if (o != NULL)
			o = NULL;

		/* for convenience */

		resm = m;

	} else {
		/* For instance methods we make a virtual function table lookup. */

		resm = method_vftbl_lookup(vftbl, m);
	}

	STATISTICS(jnicallXmethodnvokation());

	(void) vm_call_method_jvalue(resm, o, args);
}


/* _Jv_jni_invokeNative ********************************************************

   Invoke a method on the given object with the given arguments.

   For instance methods OBJ must be != NULL and the method is looked up
   in the vftbl of the object.

   For static methods, OBJ is ignored.

*******************************************************************************/

java_objectheader *_Jv_jni_invokeNative(methodinfo *m, java_objectheader *o,
										java_objectarray *params)
{
	methodinfo        *resm;
	vm_arg            *vmargs;
	java_objectheader *ro;
	s4                 argcount;
	s4                 paramcount;
	java_objectheader *xptr;

	if (m == NULL) {
		exceptions_throw_nullpointerexception();
		return NULL;
	}

	argcount = m->parseddesc->paramcount;
	paramcount = argcount;

	/* if method is non-static, remove the `this' pointer */

	if (!(m->flags & ACC_STATIC))
		paramcount--;

	/* For instance methods the object has to be an instance of the
	   class the method belongs to. For static methods the obj
	   parameter is ignored. */

	if (!(m->flags & ACC_STATIC) && o && (!builtin_instanceof(o, m->class))) {
		exceptions_throw_illegalargumentexception();
		return NULL;
	}

	/* check if we got the right number of arguments */

	if (((params == NULL) && (paramcount != 0)) ||
		(params && (params->header.size != paramcount))) 
	{
		exceptions_throw_illegalargumentexception();
		return NULL;
	}

	/* for instance methods we need an object */

	if (!(m->flags & ACC_STATIC) && (o == NULL)) {
		/* XXX not sure if that is the correct exception */
		exceptions_throw_nullpointerexception();
		return NULL;
	}

	/* for static methods, zero object to make subsequent code simpler */
	if (m->flags & ACC_STATIC)
		o = NULL;

	if (o != NULL) {
		/* for instance methods we must do a vftbl lookup */
		resm = method_vftbl_lookup(o->vftbl, m);
	}
	else {
		/* for static methods, just for convenience */
		resm = m;
	}

	vmargs = MNEW(vm_arg, argcount);

	if (!_Jv_jni_vmargs_from_objectarray(o, resm->parseddesc, vmargs, params)) {
		MFREE(vmargs, vm_arg, argcount);
		return NULL;
	}

	switch (resm->parseddesc->returntype.decltype) {
	case TYPE_VOID:
		(void) vm_call_method_vmarg(resm, argcount, vmargs);

		ro = NULL;
		break;

	case PRIMITIVETYPE_BOOLEAN: {
		s4 i;
		java_lang_Boolean *bo;

		i = vm_call_method_int_vmarg(resm, argcount, vmargs);

		ro = builtin_new(class_java_lang_Boolean);

		/* setting the value of the object direct */

		bo = (java_lang_Boolean *) ro;
		bo->value = i;
	}
	break;

	case PRIMITIVETYPE_BYTE: {
		s4 i;
		java_lang_Byte *bo;

		i = vm_call_method_int_vmarg(resm, argcount, vmargs);

		ro = builtin_new(class_java_lang_Byte);

		/* setting the value of the object direct */

		bo = (java_lang_Byte *) ro;
		bo->value = i;
	}
	break;

	case PRIMITIVETYPE_CHAR: {
		s4 i;
		java_lang_Character *co;

		i = vm_call_method_int_vmarg(resm, argcount, vmargs);

		ro = builtin_new(class_java_lang_Character);

		/* setting the value of the object direct */

		co = (java_lang_Character *) ro;
		co->value = i;
	}
	break;

	case PRIMITIVETYPE_SHORT: {
		s4 i;
		java_lang_Short *so;

		i = vm_call_method_int_vmarg(resm, argcount, vmargs);

		ro = builtin_new(class_java_lang_Short);

		/* setting the value of the object direct */

		so = (java_lang_Short *) ro;
		so->value = i;
	}
	break;

	case PRIMITIVETYPE_INT: {
		s4 i;
		java_lang_Integer *io;

		i = vm_call_method_int_vmarg(resm, argcount, vmargs);

		ro = builtin_new(class_java_lang_Integer);

		/* setting the value of the object direct */

		io = (java_lang_Integer *) ro;
		io->value = i;
	}
	break;

	case PRIMITIVETYPE_LONG: {
		s8 l;
		java_lang_Long *lo;

		l = vm_call_method_long_vmarg(resm, argcount, vmargs);

		ro = builtin_new(class_java_lang_Long);

		/* setting the value of the object direct */

		lo = (java_lang_Long *) ro;
		lo->value = l;
	}
	break;

	case PRIMITIVETYPE_FLOAT: {
		float f;
		java_lang_Float *fo;

		f = vm_call_method_float_vmarg(resm, argcount, vmargs);

		ro = builtin_new(class_java_lang_Float);

		/* setting the value of the object direct */

		fo = (java_lang_Float *) ro;
		fo->value = f;
	}
	break;

	case PRIMITIVETYPE_DOUBLE: {
		double d;
		java_lang_Double *_do;

		d = vm_call_method_double_vmarg(resm, argcount, vmargs);

		ro = builtin_new(class_java_lang_Double);

		/* setting the value of the object direct */

		_do = (java_lang_Double *) ro;
		_do->value = d;
	}
	break;

	case TYPE_ADR:
		ro = vm_call_method_vmarg(resm, argcount, vmargs);
		break;

	default:
		/* if this happens the exception has already been set by
		   fill_callblock_from_objectarray */

		MFREE(vmargs, vm_arg, argcount);

		return NULL;
	}

	MFREE(vmargs, vm_arg, argcount);

	xptr = exceptions_get_exception();

	if (xptr != NULL) {
		/* clear exception pointer, we are calling JIT code again */

		exceptions_clear_exception();

		exceptions_throw_invocationtargetexception(xptr);
	}

	return ro;
}


/* GetVersion ******************************************************************

   Returns the major version number in the higher 16 bits and the
   minor version number in the lower 16 bits.

*******************************************************************************/

jint _Jv_JNI_GetVersion(JNIEnv *env)
{
	STATISTICS(jniinvokation());

	/* we support JNI 1.4 */

	return JNI_VERSION_1_4;
}


/* Class Operations ***********************************************************/

/* DefineClass *****************************************************************

   Loads a class from a buffer of raw class data. The buffer
   containing the raw class data is not referenced by the VM after the
   DefineClass call returns, and it may be discarded if desired.

*******************************************************************************/

jclass _Jv_JNI_DefineClass(JNIEnv *env, const char *name, jobject loader,
						   const jbyte *buf, jsize bufLen)
{
#if defined(ENABLE_JAVASE)
	java_lang_ClassLoader *cl;
	java_lang_String      *s;
	java_bytearray        *ba;
	jclass                 c;

	STATISTICS(jniinvokation());

	cl = (java_lang_ClassLoader *) loader;
	s  = (java_lang_String *) javastring_new_from_utf_string(name);
	ba = (java_bytearray *) buf;

	c = (jclass) _Jv_java_lang_ClassLoader_defineClass(cl, s, ba, 0, bufLen,
													   NULL);

	return (jclass) _Jv_JNI_NewLocalRef(env, (jobject) c);
#else
	vm_abort("_Jv_JNI_DefineClass: not implemented in this configuration");

	/* keep compiler happy */

	return 0;
#endif
}


/* FindClass *******************************************************************

   This function loads a locally-defined class. It searches the
   directories and zip files specified by the CLASSPATH environment
   variable for the class with the specified name.

*******************************************************************************/

jclass _Jv_JNI_FindClass(JNIEnv *env, const char *name)
{
#if defined(ENABLE_JAVASE)
	utf       *u;
	classinfo *cc;
	classinfo *c;

	STATISTICS(jniinvokation());

	u = utf_new_char_classname((char *) name);

	/* Check stacktrace for classloader, if one found use it,
	   otherwise use the system classloader. */

	/* Quote from the JNI documentation:
	 
	   In the Java 2 Platform, FindClass locates the class loader
	   associated with the current native method.  If the native code
	   belongs to a system class, no class loader will be
	   involved. Otherwise, the proper class loader will be invoked to
	   load and link the named class. When FindClass is called through
	   the Invocation Interface, there is no current native method or
	   its associated class loader. In that case, the result of
	   ClassLoader.getBaseClassLoader is used." */

	cc = stacktrace_getCurrentClass();

	if (cc == NULL)
		c = load_class_from_sysloader(u);
	else
		c = load_class_from_classloader(u, cc->classloader);

	if (c == NULL)
		return NULL;

	if (!link_class(c))
		return NULL;

  	return (jclass) _Jv_JNI_NewLocalRef(env, (jobject) c);
#else
	vm_abort("_Jv_JNI_FindClass: not implemented in this configuration");

	/* keep compiler happy */

	return NULL;
#endif
}
  

/* GetSuperclass ***************************************************************

   If clazz represents any class other than the class Object, then
   this function returns the object that represents the superclass of
   the class specified by clazz.

*******************************************************************************/
 
jclass _Jv_JNI_GetSuperclass(JNIEnv *env, jclass sub)
{
	classinfo *c;

	STATISTICS(jniinvokation());

	c = ((classinfo *) sub)->super.cls;

	if (!c)
		return NULL;

	return (jclass) _Jv_JNI_NewLocalRef(env, (jobject) c);
}
  
 
/* IsAssignableFrom ************************************************************

   Determines whether an object of sub can be safely cast to sup.

*******************************************************************************/

jboolean _Jv_JNI_IsAssignableFrom(JNIEnv *env, jclass sub, jclass sup)
{
	java_lang_Class *csup;
	java_lang_Class *csub;

	csup = (java_lang_Class *) sup;
	csub = (java_lang_Class *) sub;

	STATISTICS(jniinvokation());

	return _Jv_java_lang_Class_isAssignableFrom(csup, csub);
}


/* Throw ***********************************************************************

   Causes a java.lang.Throwable object to be thrown.

*******************************************************************************/

jint _Jv_JNI_Throw(JNIEnv *env, jthrowable obj)
{
	java_objectheader *o;

	STATISTICS(jniinvokation());

	o = (java_objectheader *) obj;

	exceptions_set_exception(o);

	return JNI_OK;
}


/* ThrowNew ********************************************************************

   Constructs an exception object from the specified class with the
   message specified by message and causes that exception to be
   thrown.

*******************************************************************************/

jint _Jv_JNI_ThrowNew(JNIEnv* env, jclass clazz, const char *msg) 
{
	classinfo         *c;
	java_objectheader *o;
	java_objectheader *s;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	s = javastring_new_from_utf_string(msg);

  	/* instantiate exception object */

	o = native_new_and_init_string(c, s);

	if (o == NULL)
		return -1;

	exceptions_set_exception(o);

	return 0;
}


/* ExceptionOccurred ***********************************************************

   Determines if an exception is being thrown. The exception stays
   being thrown until either the native code calls ExceptionClear(),
   or the Java code handles the exception.

*******************************************************************************/

jthrowable _Jv_JNI_ExceptionOccurred(JNIEnv *env)
{
	java_objectheader *o;

	STATISTICS(jniinvokation());

	o = exceptions_get_exception();

	return _Jv_JNI_NewLocalRef(env, (jthrowable) o);
}


/* ExceptionDescribe ***********************************************************

   Prints an exception and a backtrace of the stack to a system
   error-reporting channel, such as stderr. This is a convenience
   routine provided for debugging.

*******************************************************************************/

void _Jv_JNI_ExceptionDescribe(JNIEnv *env)
{
	java_objectheader *o;
	methodinfo        *m;

	STATISTICS(jniinvokation());

	o = exceptions_get_exception();

	if (o == NULL) {
		/* clear exception, because we are calling jit code again */

		exceptions_clear_exception();

		/* get printStackTrace method from exception class */

		m = class_resolveclassmethod(o->vftbl->class,
									 utf_printStackTrace,
									 utf_void__void,
									 NULL,
									 true);

		if (m == NULL)
			/* XXX what should we do? */
			return;

		/* print the stacktrace */

		(void) vm_call_method(m, o);
	}
}


/* ExceptionClear **************************************************************

   Clears any exception that is currently being thrown. If no
   exception is currently being thrown, this routine has no effect.

*******************************************************************************/

void _Jv_JNI_ExceptionClear(JNIEnv *env)
{
	STATISTICS(jniinvokation());

	exceptions_clear_exception();
}


/* FatalError ******************************************************************

   Raises a fatal error and does not expect the VM to recover. This
   function does not return.

*******************************************************************************/

void _Jv_JNI_FatalError(JNIEnv *env, const char *msg)
{
	STATISTICS(jniinvokation());

	/* this seems to be the best way */

	vm_abort(msg);
}


/* PushLocalFrame **************************************************************

   Creates a new local reference frame, in which at least a given
   number of local references can be created.

*******************************************************************************/

jint _Jv_JNI_PushLocalFrame(JNIEnv* env, jint capacity)
{
	s4              additionalrefs;
	localref_table *lrt;
	localref_table *nlrt;

	STATISTICS(jniinvokation());

	if (capacity <= 0)
		return -1;

	/* Allocate new local reference table on Java heap.  Calculate the
	   additional memory we have to allocate. */

	if (capacity > LOCALREFTABLE_CAPACITY)
		additionalrefs = capacity - LOCALREFTABLE_CAPACITY;
	else
		additionalrefs = 0;

	nlrt = GCMNEW(u1, sizeof(localref_table) + additionalrefs * SIZEOF_VOID_P);

	if (nlrt == NULL)
		return -1;

	/* get current local reference table from thread */

	lrt = LOCALREFTABLE;

	/* Set up the new local reference table and add it to the local
	   frames chain. */

	nlrt->capacity    = capacity;
	nlrt->used        = 0;
	nlrt->localframes = lrt->localframes + 1;
	nlrt->prev        = lrt;

	/* store new local reference table in thread */

	LOCALREFTABLE = nlrt;

	return 0;
}


/* PopLocalFrame ***************************************************************

   Pops off the current local reference frame, frees all the local
   references, and returns a local reference in the previous local
   reference frame for the given result object.

*******************************************************************************/

jobject _Jv_JNI_PopLocalFrame(JNIEnv* env, jobject result)
{
	localref_table *lrt;
	localref_table *plrt;
	s4              localframes;

	STATISTICS(jniinvokation());

	/* get current local reference table from thread */

	lrt = LOCALREFTABLE;

	localframes = lrt->localframes;

	/* Don't delete the top local frame, as this one is allocated in
	   the native stub on the stack and is freed automagically on
	   return. */

	if (localframes == 1)
		return _Jv_JNI_NewLocalRef(env, result);

	/* release all current local frames */

	for (; localframes >= 1; localframes--) {
		/* get previous frame */

		plrt = lrt->prev;

		/* clear all reference entries */

		MSET(&lrt->refs[0], 0, java_objectheader*, lrt->capacity);

		lrt->prev = NULL;

		/* set new local references table */

		lrt = plrt;
	}

	/* store new local reference table in thread */

	LOCALREFTABLE = lrt;

	/* add local reference and return the value */

	return _Jv_JNI_NewLocalRef(env, result);
}


/* DeleteLocalRef **************************************************************

   Deletes the local reference pointed to by localRef.

*******************************************************************************/

void _Jv_JNI_DeleteLocalRef(JNIEnv *env, jobject localRef)
{
	java_objectheader *o;
	localref_table    *lrt;
	s4                 i;

	STATISTICS(jniinvokation());

	o = (java_objectheader *) localRef;

	/* get local reference table (thread specific) */

	lrt = LOCALREFTABLE;

	/* go through all local frames */

	for (; lrt != NULL; lrt = lrt->prev) {

		/* and try to remove the reference */

		for (i = 0; i < lrt->capacity; i++) {
			if (lrt->refs[i] == o) {
				lrt->refs[i] = NULL;
				lrt->used--;

				return;
			}
		}
	}

	/* this should not happen */

/*  	if (opt_checkjni) */
/*  	FatalError(env, "Bad global or local ref passed to JNI"); */
	log_text("JNI-DeleteLocalRef: Local ref passed to JNI not found");
}


/* IsSameObject ****************************************************************

   Tests whether two references refer to the same Java object.

*******************************************************************************/

jboolean _Jv_JNI_IsSameObject(JNIEnv *env, jobject ref1, jobject ref2)
{
	STATISTICS(jniinvokation());

	if (ref1 == ref2)
		return JNI_TRUE;
	else
		return JNI_FALSE;
}


/* NewLocalRef *****************************************************************

   Creates a new local reference that refers to the same object as ref.

*******************************************************************************/

jobject _Jv_JNI_NewLocalRef(JNIEnv *env, jobject ref)
{
	localref_table *lrt;
	s4              i;

	STATISTICS(jniinvokation());

	if (ref == NULL)
		return NULL;

	/* get local reference table (thread specific) */

	lrt = LOCALREFTABLE;

	/* Check if we have space for the requested reference?  No,
	   allocate a new frame.  This is actually not what the spec says,
	   but for compatibility reasons... */

	if (lrt->used == lrt->capacity) {
		if (_Jv_JNI_EnsureLocalCapacity(env, 16) != 0)
			return NULL;

		/* get the new local reference table */

		lrt = LOCALREFTABLE;
	}

	/* insert the reference */

	for (i = 0; i < lrt->capacity; i++) {
		if (lrt->refs[i] == NULL) {
			lrt->refs[i] = (java_objectheader *) ref;
			lrt->used++;

			return ref;
		}
	}

	/* should not happen, just to be sure */

	assert(0);

	/* keep compiler happy */

	return NULL;
}


/* EnsureLocalCapacity *********************************************************

   Ensures that at least a given number of local references can be
   created in the current thread

*******************************************************************************/

jint _Jv_JNI_EnsureLocalCapacity(JNIEnv* env, jint capacity)
{
	localref_table *lrt;

	STATISTICS(jniinvokation());

	/* get local reference table (thread specific) */

	lrt = LOCALREFTABLE;

	/* check if capacity elements are available in the local references table */

	if ((lrt->used + capacity) > lrt->capacity)
		return _Jv_JNI_PushLocalFrame(env, capacity);

	return 0;
}


/* AllocObject *****************************************************************

   Allocates a new Java object without invoking any of the
   constructors for the object. Returns a reference to the object.

*******************************************************************************/

jobject _Jv_JNI_AllocObject(JNIEnv *env, jclass clazz)
{
	classinfo         *c;
	java_objectheader *o;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;

	if ((c->flags & ACC_INTERFACE) || (c->flags & ACC_ABSTRACT)) {
		exceptions_throw_instantiationexception(c);
		return NULL;
	}
		
	o = builtin_new(c);

	return _Jv_JNI_NewLocalRef(env, o);
}


/* NewObject *******************************************************************

   Programmers place all arguments that are to be passed to the
   constructor immediately following the methodID
   argument. NewObject() accepts these arguments and passes them to
   the Java method that the programmer wishes to invoke.

*******************************************************************************/

jobject _Jv_JNI_NewObject(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
	java_objectheader *o;
	methodinfo        *m;
	va_list            ap;

	STATISTICS(jniinvokation());

	m = (methodinfo *) methodID;

	/* create object */

	o = builtin_new(clazz);
	
	if (o == NULL)
		return NULL;

	/* call constructor */

	va_start(ap, methodID);
	_Jv_jni_CallVoidMethod(o, o->vftbl, m, ap);
	va_end(ap);

	return _Jv_JNI_NewLocalRef(env, o);
}


/* NewObjectV ******************************************************************

   Programmers place all arguments that are to be passed to the
   constructor in an args argument of type va_list that immediately
   follows the methodID argument. NewObjectV() accepts these
   arguments, and, in turn, passes them to the Java method that the
   programmer wishes to invoke.

*******************************************************************************/

jobject _Jv_JNI_NewObjectV(JNIEnv* env, jclass clazz, jmethodID methodID,
						   va_list args)
{
	java_objectheader *o;
	methodinfo        *m;

	STATISTICS(jniinvokation());

	m = (methodinfo *) methodID;

	/* create object */

	o = builtin_new(clazz);
	
	if (o == NULL)
		return NULL;

	/* call constructor */

	_Jv_jni_CallVoidMethod(o, o->vftbl, m, args);

	return _Jv_JNI_NewLocalRef(env, o);
}


/* NewObjectA ***************************************************************** 

   Programmers place all arguments that are to be passed to the
   constructor in an args array of jvalues that immediately follows
   the methodID argument. NewObjectA() accepts the arguments in this
   array, and, in turn, passes them to the Java method that the
   programmer wishes to invoke.

*******************************************************************************/

jobject _Jv_JNI_NewObjectA(JNIEnv* env, jclass clazz, jmethodID methodID,
						   jvalue *args)
{
	java_objectheader *o;
	methodinfo        *m;

	STATISTICS(jniinvokation());

	m = (methodinfo *) methodID;

	/* create object */

	o = builtin_new(clazz);
	
	if (o == NULL)
		return NULL;

	/* call constructor */

	_Jv_jni_CallVoidMethodA(o, o->vftbl, m, args);

	return _Jv_JNI_NewLocalRef(env, o);
}


/* GetObjectClass **************************************************************

 Returns the class of an object.

*******************************************************************************/

jclass _Jv_JNI_GetObjectClass(JNIEnv *env, jobject obj)
{
	java_objectheader *o;
	classinfo         *c;

	STATISTICS(jniinvokation());

	o = (java_objectheader *) obj;

	if ((o == NULL) || (o->vftbl == NULL))
		return NULL;

 	c = o->vftbl->class;

	return (jclass) _Jv_JNI_NewLocalRef(env, (jobject) c);
}


/* IsInstanceOf ****************************************************************

   Tests whether an object is an instance of a class.

*******************************************************************************/

jboolean _Jv_JNI_IsInstanceOf(JNIEnv *env, jobject obj, jclass clazz)
{
	java_lang_Class  *c;
	java_lang_Object *o;

	STATISTICS(jniinvokation());

	c = (java_lang_Class *) clazz;
	o = (java_lang_Object *) obj;

	return _Jv_java_lang_Class_isInstance(c, o);
}


/* Reflection Support *********************************************************/

/* FromReflectedMethod *********************************************************

   Converts java.lang.reflect.Method or java.lang.reflect.Constructor
   object to a method ID.
  
*******************************************************************************/
  
jmethodID _Jv_JNI_FromReflectedMethod(JNIEnv *env, jobject method)
{
#if defined(ENABLE_JAVASE)
	methodinfo *mi;
	classinfo  *c;
	s4          slot;

	STATISTICS(jniinvokation());

	if (method == NULL)
		return NULL;
	
	if (builtin_instanceof(method, class_java_lang_reflect_Method)) {
		java_lang_reflect_Method *rm;

		rm = (java_lang_reflect_Method *) method;
		c = (classinfo *) (rm->declaringClass);
		slot = rm->slot;
	}
	else if (builtin_instanceof(method, class_java_lang_reflect_Constructor)) {
		java_lang_reflect_Constructor *rc;

		rc = (java_lang_reflect_Constructor *) method;
		c = (classinfo *) (rc->clazz);
		slot = rc->slot;
	}
	else
		return NULL;

	mi = &(c->methods[slot]);

	return (jmethodID) mi;
#else
	vm_abort("_Jv_JNI_FromReflectedMethod: not implemented in this configuration");

	/* keep compiler happy */

	return NULL;
#endif
}


/* FromReflectedField **********************************************************

   Converts a java.lang.reflect.Field to a field ID.

*******************************************************************************/
 
jfieldID _Jv_JNI_FromReflectedField(JNIEnv* env, jobject field)
{
#if defined(ENABLE_JAVASE)
	java_lang_reflect_Field *rf;
	classinfo               *c;
	fieldinfo               *f;

	STATISTICS(jniinvokation());

	rf = (java_lang_reflect_Field *) field;

	if (rf == NULL)
		return NULL;

	c = (classinfo *) rf->declaringClass;

	f = &(c->fields[rf->slot]);

	return (jfieldID) f;
#else
	vm_abort("_Jv_JNI_FromReflectedField: not implemented in this configuration");

	/* keep compiler happy */

	return NULL;
#endif
}


/* ToReflectedMethod ***********************************************************

   Converts a method ID derived from cls to an instance of the
   java.lang.reflect.Method class or to an instance of the
   java.lang.reflect.Constructor class.

*******************************************************************************/

jobject _Jv_JNI_ToReflectedMethod(JNIEnv* env, jclass cls, jmethodID methodID,
								  jboolean isStatic)
{
	STATISTICS(jniinvokation());

	log_text("JNI-Call: ToReflectedMethod: IMPLEMENT ME!");

	return NULL;
}


/* ToReflectedField ************************************************************

   Converts a field ID derived from cls to an instance of the
   java.lang.reflect.Field class.

*******************************************************************************/

jobject _Jv_JNI_ToReflectedField(JNIEnv* env, jclass cls, jfieldID fieldID,
								 jboolean isStatic)
{
	STATISTICS(jniinvokation());

	log_text("JNI-Call: ToReflectedField: IMPLEMENT ME!");

	return NULL;
}


/* Calling Instance Methods ***************************************************/

/* GetMethodID *****************************************************************

   Returns the method ID for an instance (nonstatic) method of a class
   or interface. The method may be defined in one of the clazz's
   superclasses and inherited by clazz. The method is determined by
   its name and signature.

   GetMethodID() causes an uninitialized class to be initialized.

*******************************************************************************/

jmethodID _Jv_JNI_GetMethodID(JNIEnv* env, jclass clazz, const char *name,
							  const char *sig)
{
	classinfo  *c;
	utf        *uname;
	utf        *udesc;
	methodinfo *m;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;

	if (!c)
		return NULL;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return NULL;

	/* try to get the method of the class or one of it's superclasses */

	uname = utf_new_char((char *) name);
	udesc = utf_new_char((char *) sig);

 	m = class_resolvemethod(clazz, uname, udesc);

	if ((m == NULL) || (m->flags & ACC_STATIC)) {
		exceptions_throw_nosuchmethoderror(c, uname, udesc);

		return NULL;
	}

	return (jmethodID) m;
}


/* JNI-functions for calling instance methods *********************************/

jobject _Jv_JNI_CallObjectMethod(JNIEnv *env, jobject obj, jmethodID methodID,
								 ...)
{
	java_objectheader *o;
	methodinfo        *m;
	java_objectheader *ret;
	va_list            ap;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	ret = _Jv_jni_CallObjectMethod(o, o->vftbl, m, ap);
	va_end(ap);

	return _Jv_JNI_NewLocalRef(env, ret);
}


jobject _Jv_JNI_CallObjectMethodV(JNIEnv *env, jobject obj, jmethodID methodID,
								  va_list args)
{
	java_objectheader *o;
	methodinfo        *m;
	java_objectheader *ret;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	ret = _Jv_jni_CallObjectMethod(o, o->vftbl, m, args);

	return _Jv_JNI_NewLocalRef(env, ret);
}


jobject _Jv_JNI_CallObjectMethodA(JNIEnv *env, jobject obj, jmethodID methodID,
								  jvalue *args)
{
	java_objectheader *o;
	methodinfo        *m;
	java_objectheader *ret;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	ret = _Jv_jni_CallObjectMethodA(o, o->vftbl, m, args);

	return _Jv_JNI_NewLocalRef(env, ret);
}


jboolean _Jv_JNI_CallBooleanMethod(JNIEnv *env, jobject obj, jmethodID methodID,
								   ...)
{
	java_objectheader *o;
	methodinfo        *m;
	va_list            ap;
	jboolean           b;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	b = _Jv_jni_CallIntMethod(o, o->vftbl, m, ap);
	va_end(ap);

	return b;
}


jboolean _Jv_JNI_CallBooleanMethodV(JNIEnv *env, jobject obj,
									jmethodID methodID, va_list args)
{
	java_objectheader *o;
	methodinfo        *m;
	jboolean           b;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	b = _Jv_jni_CallIntMethod(o, o->vftbl, m, args);

	return b;
}


jboolean _Jv_JNI_CallBooleanMethodA(JNIEnv *env, jobject obj,
									jmethodID methodID, jvalue *args)
{
	java_objectheader *o;
	methodinfo        *m;
	jboolean           b;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	b = _Jv_jni_CallIntMethodA(o, o->vftbl, m, args);

	return b;
}


jbyte _Jv_JNI_CallByteMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	java_objectheader *o;
	methodinfo        *m;
	va_list            ap;
	jbyte              b;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	b = _Jv_jni_CallIntMethod(o, o->vftbl, m, ap);
	va_end(ap);

	return b;

}


jbyte _Jv_JNI_CallByteMethodV(JNIEnv *env, jobject obj, jmethodID methodID,
							  va_list args)
{
	java_objectheader *o;
	methodinfo        *m;
	jbyte              b;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	b = _Jv_jni_CallIntMethod(o, o->vftbl, m, args);

	return b;
}


jbyte _Jv_JNI_CallByteMethodA(JNIEnv *env, jobject obj, jmethodID methodID,
							  jvalue *args)
{
	log_text("JNI-Call: CallByteMethodA: IMPLEMENT ME!");

	return 0;
}


jchar _Jv_JNI_CallCharMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	java_objectheader *o;
	methodinfo        *m;
	va_list            ap;
	jchar              c;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	c = _Jv_jni_CallIntMethod(o, o->vftbl, m, ap);
	va_end(ap);

	return c;
}


jchar _Jv_JNI_CallCharMethodV(JNIEnv *env, jobject obj, jmethodID methodID,
							  va_list args)
{
	java_objectheader *o;
	methodinfo        *m;
	jchar              c;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	c = _Jv_jni_CallIntMethod(o, o->vftbl, m, args);

	return c;
}


jchar _Jv_JNI_CallCharMethodA(JNIEnv *env, jobject obj, jmethodID methodID,
							  jvalue *args)
{
	log_text("JNI-Call: CallCharMethodA: IMPLEMENT ME!");

	return 0;
}


jshort _Jv_JNI_CallShortMethod(JNIEnv *env, jobject obj, jmethodID methodID,
							   ...)
{
	java_objectheader *o;
	methodinfo        *m;
	va_list            ap;
	jshort             s;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	s = _Jv_jni_CallIntMethod(o, o->vftbl, m, ap);
	va_end(ap);

	return s;
}


jshort _Jv_JNI_CallShortMethodV(JNIEnv *env, jobject obj, jmethodID methodID,
								va_list args)
{
	java_objectheader *o;
	methodinfo        *m;
	jshort             s;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	s = _Jv_jni_CallIntMethod(o, o->vftbl, m, args);

	return s;
}


jshort _Jv_JNI_CallShortMethodA(JNIEnv *env, jobject obj, jmethodID methodID,
								jvalue *args)
{
	log_text("JNI-Call: CallShortMethodA: IMPLEMENT ME!");

	return 0;
}



jint _Jv_JNI_CallIntMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	java_objectheader *o;
	methodinfo        *m;
	va_list            ap;
	jint               i;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	i = _Jv_jni_CallIntMethod(o, o->vftbl, m, ap);
	va_end(ap);

	return i;
}


jint _Jv_JNI_CallIntMethodV(JNIEnv *env, jobject obj, jmethodID methodID,
							va_list args)
{
	java_objectheader *o;
	methodinfo        *m;
	jint               i;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	i = _Jv_jni_CallIntMethod(o, o->vftbl, m, args);

	return i;
}


jint _Jv_JNI_CallIntMethodA(JNIEnv *env, jobject obj, jmethodID methodID,
							jvalue *args)
{
	log_text("JNI-Call: CallIntMethodA: IMPLEMENT ME!");

	return 0;
}



jlong _Jv_JNI_CallLongMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	java_objectheader *o;
	methodinfo        *m;
	va_list            ap;
	jlong              l;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	l = _Jv_jni_CallLongMethod(o, o->vftbl, m, ap);
	va_end(ap);

	return l;
}


jlong _Jv_JNI_CallLongMethodV(JNIEnv *env, jobject obj, jmethodID methodID,
							  va_list args)
{
	java_objectheader *o;
	methodinfo        *m;
	jlong              l;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	l = _Jv_jni_CallLongMethod(o, o->vftbl, m, args);

	return l;
}


jlong _Jv_JNI_CallLongMethodA(JNIEnv *env, jobject obj, jmethodID methodID,
							  jvalue *args)
{
	log_text("JNI-Call: CallLongMethodA: IMPLEMENT ME!");

	return 0;
}



jfloat _Jv_JNI_CallFloatMethod(JNIEnv *env, jobject obj, jmethodID methodID,
							   ...)
{
	java_objectheader *o;
	methodinfo        *m;
	va_list            ap;
	jfloat             f;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	f = _Jv_jni_CallFloatMethod(o, o->vftbl, m, ap);
	va_end(ap);

	return f;
}


jfloat _Jv_JNI_CallFloatMethodV(JNIEnv *env, jobject obj, jmethodID methodID,
								va_list args)
{
	java_objectheader *o;
	methodinfo        *m;
	jfloat             f;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	f = _Jv_jni_CallFloatMethod(o, o->vftbl, m, args);

	return f;
}


jfloat _Jv_JNI_CallFloatMethodA(JNIEnv *env, jobject obj, jmethodID methodID,
								jvalue *args)
{
	log_text("JNI-Call: CallFloatMethodA: IMPLEMENT ME!");

	return 0;
}



jdouble _Jv_JNI_CallDoubleMethod(JNIEnv *env, jobject obj, jmethodID methodID,
								 ...)
{
	java_objectheader *o;
	methodinfo        *m;
	va_list            ap;
	jdouble            d;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	d = _Jv_jni_CallDoubleMethod(o, o->vftbl, m, ap);
	va_end(ap);

	return d;
}


jdouble _Jv_JNI_CallDoubleMethodV(JNIEnv *env, jobject obj, jmethodID methodID,
								  va_list args)
{
	java_objectheader *o;
	methodinfo        *m;
	jdouble            d;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	d = _Jv_jni_CallDoubleMethod(o, o->vftbl, m, args);

	return d;
}


jdouble _Jv_JNI_CallDoubleMethodA(JNIEnv *env, jobject obj, jmethodID methodID,
								  jvalue *args)
{
	log_text("JNI-Call: CallDoubleMethodA: IMPLEMENT ME!");

	return 0;
}



void _Jv_JNI_CallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
	java_objectheader *o;
	methodinfo        *m;
	va_list            ap;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	_Jv_jni_CallVoidMethod(o, o->vftbl, m, ap);
	va_end(ap);
}


void _Jv_JNI_CallVoidMethodV(JNIEnv *env, jobject obj, jmethodID methodID,
							 va_list args)
{
	java_objectheader *o;
	methodinfo        *m;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	_Jv_jni_CallVoidMethod(o, o->vftbl, m, args);
}


void _Jv_JNI_CallVoidMethodA(JNIEnv *env, jobject obj, jmethodID methodID,
							 jvalue *args)
{
	java_objectheader *o;
	methodinfo        *m;

	o = (java_objectheader *) obj;
	m = (methodinfo *) methodID;

	_Jv_jni_CallVoidMethodA(o, o->vftbl, m, args);
}



jobject _Jv_JNI_CallNonvirtualObjectMethod(JNIEnv *env, jobject obj,
										   jclass clazz, jmethodID methodID,
										   ...)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	java_objectheader *r;
	va_list            ap;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	r = _Jv_jni_CallObjectMethod(o, c->vftbl, m, ap);
	va_end(ap);

	return _Jv_JNI_NewLocalRef(env, r);
}


jobject _Jv_JNI_CallNonvirtualObjectMethodV(JNIEnv *env, jobject obj,
											jclass clazz, jmethodID methodID,
											va_list args)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	java_objectheader *r;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	r = _Jv_jni_CallObjectMethod(o, c->vftbl, m, args);

	return _Jv_JNI_NewLocalRef(env, r);
}


jobject _Jv_JNI_CallNonvirtualObjectMethodA(JNIEnv *env, jobject obj,
											jclass clazz, jmethodID methodID,
											jvalue *args)
{
	log_text("JNI-Call: CallNonvirtualObjectMethodA: IMPLEMENT ME!");

	return _Jv_JNI_NewLocalRef(env, NULL);
}



jboolean _Jv_JNI_CallNonvirtualBooleanMethod(JNIEnv *env, jobject obj,
											 jclass clazz, jmethodID methodID,
											 ...)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	va_list            ap;
	jboolean           b;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	b = _Jv_jni_CallIntMethod(o, c->vftbl, m, ap);
	va_end(ap);

	return b;
}


jboolean _Jv_JNI_CallNonvirtualBooleanMethodV(JNIEnv *env, jobject obj,
											  jclass clazz, jmethodID methodID,
											  va_list args)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	jboolean           b;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	b = _Jv_jni_CallIntMethod(o, c->vftbl, m, args);

	return b;
}


jboolean _Jv_JNI_CallNonvirtualBooleanMethodA(JNIEnv *env, jobject obj,
											  jclass clazz, jmethodID methodID,
											  jvalue *args)
{
	log_text("JNI-Call: CallNonvirtualBooleanMethodA: IMPLEMENT ME!");

	return 0;
}


jbyte _Jv_JNI_CallNonvirtualByteMethod(JNIEnv *env, jobject obj, jclass clazz,
									   jmethodID methodID, ...)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	va_list            ap;
	jbyte              b;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	b = _Jv_jni_CallIntMethod(o, c->vftbl, m, ap);
	va_end(ap);

	return b;
}


jbyte _Jv_JNI_CallNonvirtualByteMethodV(JNIEnv *env, jobject obj, jclass clazz,
										jmethodID methodID, va_list args)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	jbyte              b;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	b = _Jv_jni_CallIntMethod(o, c->vftbl, m, args);

	return b;
}


jbyte _Jv_JNI_CallNonvirtualByteMethodA(JNIEnv *env, jobject obj, jclass clazz, 
										jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallNonvirtualByteMethodA: IMPLEMENT ME!");

	return 0;
}



jchar _Jv_JNI_CallNonvirtualCharMethod(JNIEnv *env, jobject obj, jclass clazz,
									   jmethodID methodID, ...)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	va_list            ap;
	jchar              ch;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	ch = _Jv_jni_CallIntMethod(o, c->vftbl, m, ap);
	va_end(ap);

	return ch;
}


jchar _Jv_JNI_CallNonvirtualCharMethodV(JNIEnv *env, jobject obj, jclass clazz,
										jmethodID methodID, va_list args)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	jchar              ch;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	ch = _Jv_jni_CallIntMethod(o, c->vftbl, m, args);

	return ch;
}


jchar _Jv_JNI_CallNonvirtualCharMethodA(JNIEnv *env, jobject obj, jclass clazz,
										jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallNonvirtualCharMethodA: IMPLEMENT ME!");

	return 0;
}



jshort _Jv_JNI_CallNonvirtualShortMethod(JNIEnv *env, jobject obj,
										 jclass clazz, jmethodID methodID, ...)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	va_list            ap;
	jshort             s;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	s = _Jv_jni_CallIntMethod(o, c->vftbl, m, ap);
	va_end(ap);

	return s;
}


jshort _Jv_JNI_CallNonvirtualShortMethodV(JNIEnv *env, jobject obj,
										  jclass clazz, jmethodID methodID,
										  va_list args)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	jshort             s;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	s = _Jv_jni_CallIntMethod(o, c->vftbl, m, args);

	return s;
}


jshort _Jv_JNI_CallNonvirtualShortMethodA(JNIEnv *env, jobject obj,
										  jclass clazz, jmethodID methodID,
										  jvalue *args)
{
	log_text("JNI-Call: CallNonvirtualShortMethodA: IMPLEMENT ME!");

	return 0;
}



jint _Jv_JNI_CallNonvirtualIntMethod(JNIEnv *env, jobject obj, jclass clazz,
									 jmethodID methodID, ...)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	va_list            ap;
	jint               i;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	i = _Jv_jni_CallIntMethod(o, c->vftbl, m, ap);
	va_end(ap);

	return i;
}


jint _Jv_JNI_CallNonvirtualIntMethodV(JNIEnv *env, jobject obj, jclass clazz,
									  jmethodID methodID, va_list args)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	jint               i;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	i = _Jv_jni_CallIntMethod(o, c->vftbl, m, args);

	return i;
}


jint _Jv_JNI_CallNonvirtualIntMethodA(JNIEnv *env, jobject obj, jclass clazz,
									  jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallNonvirtualIntMethodA: IMPLEMENT ME!");

	return 0;
}



jlong _Jv_JNI_CallNonvirtualLongMethod(JNIEnv *env, jobject obj, jclass clazz,
									   jmethodID methodID, ...)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	va_list            ap;
	jlong              l;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	l = _Jv_jni_CallLongMethod(o, c->vftbl, m, ap);
	va_end(ap);

	return l;
}


jlong _Jv_JNI_CallNonvirtualLongMethodV(JNIEnv *env, jobject obj, jclass clazz,
										jmethodID methodID, va_list args)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	jlong              l;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	l = _Jv_jni_CallLongMethod(o, c->vftbl, m, args);

	return l;
}


jlong _Jv_JNI_CallNonvirtualLongMethodA(JNIEnv *env, jobject obj, jclass clazz,
										jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallNonvirtualLongMethodA: IMPLEMENT ME!");

	return 0;
}



jfloat _Jv_JNI_CallNonvirtualFloatMethod(JNIEnv *env, jobject obj,
										 jclass clazz, jmethodID methodID, ...)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	va_list            ap;
	jfloat             f;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	f = _Jv_jni_CallFloatMethod(o, c->vftbl, m, ap);
	va_end(ap);

	return f;
}


jfloat _Jv_JNI_CallNonvirtualFloatMethodV(JNIEnv *env, jobject obj,
										  jclass clazz, jmethodID methodID,
										  va_list args)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	jfloat             f;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	f = _Jv_jni_CallFloatMethod(o, c->vftbl, m, args);

	return f;
}


jfloat _Jv_JNI_CallNonvirtualFloatMethodA(JNIEnv *env, jobject obj,
										  jclass clazz, jmethodID methodID,
										  jvalue *args)
{
	log_text("JNI-Call: CallNonvirtualFloatMethodA: IMPLEMENT ME!");

	return 0;
}



jdouble _Jv_JNI_CallNonvirtualDoubleMethod(JNIEnv *env, jobject obj,
										   jclass clazz, jmethodID methodID,
										   ...)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	va_list            ap;
	jdouble            d;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	d = _Jv_jni_CallDoubleMethod(o, c->vftbl, m, ap);
	va_end(ap);

	return d;
}


jdouble _Jv_JNI_CallNonvirtualDoubleMethodV(JNIEnv *env, jobject obj,
											jclass clazz, jmethodID methodID,
											va_list args)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	jdouble            d;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	d = _Jv_jni_CallDoubleMethod(o, c->vftbl, m, args);

	return d;
}


jdouble _Jv_JNI_CallNonvirtualDoubleMethodA(JNIEnv *env, jobject obj,
											jclass clazz, jmethodID methodID,
											jvalue *args)
{
	log_text("JNI-Call: CallNonvirtualDoubleMethodA: IMPLEMENT ME!");

	return 0;
}



void _Jv_JNI_CallNonvirtualVoidMethod(JNIEnv *env, jobject obj, jclass clazz,
									  jmethodID methodID, ...)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;
	va_list            ap;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	_Jv_jni_CallVoidMethod(o, c->vftbl, m, ap);
	va_end(ap);
}


void _Jv_JNI_CallNonvirtualVoidMethodV(JNIEnv *env, jobject obj, jclass clazz,
									   jmethodID methodID, va_list args)
{
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	_Jv_jni_CallVoidMethod(o, c->vftbl, m, args);
}


void _Jv_JNI_CallNonvirtualVoidMethodA(JNIEnv *env, jobject obj, jclass clazz,
									   jmethodID methodID, jvalue * args)
{	
	java_objectheader *o;
	classinfo         *c;
	methodinfo        *m;

	o = (java_objectheader *) obj;
	c = (classinfo *) clazz;
	m = (methodinfo *) methodID;

	_Jv_jni_CallVoidMethodA(o, c->vftbl, m, args);
}


/* Accessing Fields of Objects ************************************************/

/* GetFieldID ******************************************************************

   Returns the field ID for an instance (nonstatic) field of a
   class. The field is specified by its name and signature. The
   Get<type>Field and Set<type>Field families of accessor functions
   use field IDs to retrieve object fields.

*******************************************************************************/

jfieldID _Jv_JNI_GetFieldID(JNIEnv *env, jclass clazz, const char *name,
							const char *sig) 
{
	classinfo *c;
	fieldinfo *f;
	utf       *uname;
	utf       *udesc;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;

	uname = utf_new_char((char *) name);
	udesc = utf_new_char((char *) sig);

	f = class_findfield(clazz, uname, udesc); 
	
	if (f == NULL)
		exceptions_throw_nosuchfielderror(c, uname);  

	return (jfieldID) f;
}


/* Get<type>Field Routines *****************************************************

   This family of accessor routines returns the value of an instance
   (nonstatic) field of an object. The field to access is specified by
   a field ID obtained by calling GetFieldID().

*******************************************************************************/

jobject _Jv_JNI_GetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	java_objectheader *o;

	STATISTICS(jniinvokation());

	o = GET_FIELD(obj, java_objectheader*, fieldID);

	return _Jv_JNI_NewLocalRef(env, o);
}


jboolean _Jv_JNI_GetBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	s4 i;

	STATISTICS(jniinvokation());

	i = GET_FIELD(obj, s4, fieldID);

	return (jboolean) i;
}


jbyte _Jv_JNI_GetByteField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	s4 i;

	STATISTICS(jniinvokation());

	i = GET_FIELD(obj, s4, fieldID);

	return (jbyte) i;
}


jchar _Jv_JNI_GetCharField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	s4 i;

	STATISTICS(jniinvokation());

	i = GET_FIELD(obj, s4, fieldID);

	return (jchar) i;
}


jshort _Jv_JNI_GetShortField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	s4 i;

	STATISTICS(jniinvokation());

	i = GET_FIELD(obj, s4, fieldID);

	return (jshort) i;
}


jint _Jv_JNI_GetIntField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	java_objectheader *o;
	fieldinfo         *f;
	s4                 i;

	STATISTICS(jniinvokation());

	o = (java_objectheader *) obj;
	f = (fieldinfo *) fieldID;

	i = GET_FIELD(o, s4, f);

	return i;
}


jlong _Jv_JNI_GetLongField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	s8 l;

	STATISTICS(jniinvokation());

	l = GET_FIELD(obj, s8, fieldID);

	return l;
}


jfloat _Jv_JNI_GetFloatField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	float f;

	STATISTICS(jniinvokation());

	f = GET_FIELD(obj, float, fieldID);

	return f;
}


jdouble _Jv_JNI_GetDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	double d;

	STATISTICS(jniinvokation());

	d = GET_FIELD(obj, double, fieldID);

	return d;
}


/* Set<type>Field Routines *****************************************************

   This family of accessor routines sets the value of an instance
   (nonstatic) field of an object. The field to access is specified by
   a field ID obtained by calling GetFieldID().

*******************************************************************************/

void _Jv_JNI_SetObjectField(JNIEnv *env, jobject obj, jfieldID fieldID,
							jobject value)
{
	STATISTICS(jniinvokation());

	SET_FIELD(obj, java_objectheader*, fieldID, value);
}


void _Jv_JNI_SetBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID,
							 jboolean value)
{
	STATISTICS(jniinvokation());

	SET_FIELD(obj, s4, fieldID, value);
}


void _Jv_JNI_SetByteField(JNIEnv *env, jobject obj, jfieldID fieldID,
						  jbyte value)
{
	STATISTICS(jniinvokation());

	SET_FIELD(obj, s4, fieldID, value);
}


void _Jv_JNI_SetCharField(JNIEnv *env, jobject obj, jfieldID fieldID,
						  jchar value)
{
	STATISTICS(jniinvokation());

	SET_FIELD(obj, s4, fieldID, value);
}


void _Jv_JNI_SetShortField(JNIEnv *env, jobject obj, jfieldID fieldID,
						   jshort value)
{
	STATISTICS(jniinvokation());

	SET_FIELD(obj, s4, fieldID, value);
}


void _Jv_JNI_SetIntField(JNIEnv *env, jobject obj, jfieldID fieldID, jint value)
{
	STATISTICS(jniinvokation());

	SET_FIELD(obj, s4, fieldID, value);
}


void _Jv_JNI_SetLongField(JNIEnv *env, jobject obj, jfieldID fieldID,
						  jlong value)
{
	STATISTICS(jniinvokation());

	SET_FIELD(obj, s8, fieldID, value);
}


void _Jv_JNI_SetFloatField(JNIEnv *env, jobject obj, jfieldID fieldID,
						   jfloat value)
{
	STATISTICS(jniinvokation());

	SET_FIELD(obj, float, fieldID, value);
}


void _Jv_JNI_SetDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID,
							jdouble value)
{
	STATISTICS(jniinvokation());

	SET_FIELD(obj, double, fieldID, value);
}


/* Calling Static Methods *****************************************************/

/* GetStaticMethodID ***********************************************************

   Returns the method ID for a static method of a class. The method is
   specified by its name and signature.

   GetStaticMethodID() causes an uninitialized class to be
   initialized.

*******************************************************************************/

jmethodID _Jv_JNI_GetStaticMethodID(JNIEnv *env, jclass clazz, const char *name,
									const char *sig)
{
	classinfo  *c;
	utf        *uname;
	utf        *udesc;
	methodinfo *m;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;

	if (!c)
		return NULL;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return NULL;

	/* try to get the static method of the class */

	uname = utf_new_char((char *) name);
	udesc = utf_new_char((char *) sig);

 	m = class_resolvemethod(c, uname, udesc);

	if ((m == NULL) || !(m->flags & ACC_STATIC)) {
		exceptions_throw_nosuchmethoderror(c, uname, udesc);

		return NULL;
	}

	return (jmethodID) m;
}


jobject _Jv_JNI_CallStaticObjectMethod(JNIEnv *env, jclass clazz,
									   jmethodID methodID, ...)
{
	methodinfo        *m;
	java_objectheader *o;
	va_list            ap;

	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	o = _Jv_jni_CallObjectMethod(NULL, NULL, m, ap);
	va_end(ap);

	return _Jv_JNI_NewLocalRef(env, o);
}


jobject _Jv_JNI_CallStaticObjectMethodV(JNIEnv *env, jclass clazz,
										jmethodID methodID, va_list args)
{
	methodinfo        *m;
	java_objectheader *o;

	m = (methodinfo *) methodID;

	o = _Jv_jni_CallObjectMethod(NULL, NULL, m, args);

	return _Jv_JNI_NewLocalRef(env, o);
}


jobject _Jv_JNI_CallStaticObjectMethodA(JNIEnv *env, jclass clazz,
										jmethodID methodID, jvalue *args)
{
	methodinfo        *m;
	java_objectheader *o;

	m = (methodinfo *) methodID;

	o = _Jv_jni_CallObjectMethodA(NULL, NULL, m, args);

	return _Jv_JNI_NewLocalRef(env, o);
}


jboolean _Jv_JNI_CallStaticBooleanMethod(JNIEnv *env, jclass clazz,
										 jmethodID methodID, ...)
{
	methodinfo *m;
	va_list     ap;
	jboolean    b;

	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	b = _Jv_jni_CallIntMethod(NULL, NULL, m, ap);
	va_end(ap);

	return b;
}


jboolean _Jv_JNI_CallStaticBooleanMethodV(JNIEnv *env, jclass clazz,
										  jmethodID methodID, va_list args)
{
	methodinfo *m;
	jboolean    b;

	m = (methodinfo *) methodID;

	b = _Jv_jni_CallIntMethod(NULL, NULL, m, args);

	return b;
}


jboolean _Jv_JNI_CallStaticBooleanMethodA(JNIEnv *env, jclass clazz,
										  jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallStaticBooleanMethodA: IMPLEMENT ME!");

	return 0;
}


jbyte _Jv_JNI_CallStaticByteMethod(JNIEnv *env, jclass clazz,
								   jmethodID methodID, ...)
{
	methodinfo *m;
	va_list     ap;
	jbyte       b;

	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	b = _Jv_jni_CallIntMethod(NULL, NULL, m, ap);
	va_end(ap);

	return b;
}


jbyte _Jv_JNI_CallStaticByteMethodV(JNIEnv *env, jclass clazz,
									jmethodID methodID, va_list args)
{
	methodinfo *m;
	jbyte       b;

	m = (methodinfo *) methodID;

	b = _Jv_jni_CallIntMethod(NULL, NULL, m, args);

	return b;
}


jbyte _Jv_JNI_CallStaticByteMethodA(JNIEnv *env, jclass clazz,
									jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallStaticByteMethodA: IMPLEMENT ME!");

	return 0;
}


jchar _Jv_JNI_CallStaticCharMethod(JNIEnv *env, jclass clazz,
								   jmethodID methodID, ...)
{
	methodinfo *m;
	va_list     ap;
	jchar       c;

	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	c = _Jv_jni_CallIntMethod(NULL, NULL, m, ap);
	va_end(ap);

	return c;
}


jchar _Jv_JNI_CallStaticCharMethodV(JNIEnv *env, jclass clazz,
									jmethodID methodID, va_list args)
{
	methodinfo *m;
	jchar       c;

	m = (methodinfo *) methodID;

	c = _Jv_jni_CallIntMethod(NULL, NULL, m, args);

	return c;
}


jchar _Jv_JNI_CallStaticCharMethodA(JNIEnv *env, jclass clazz,
									jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallStaticCharMethodA: IMPLEMENT ME!");

	return 0;
}


jshort _Jv_JNI_CallStaticShortMethod(JNIEnv *env, jclass clazz,
									 jmethodID methodID, ...)
{
	methodinfo *m;
	va_list     ap;
	jshort      s;

	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	s = _Jv_jni_CallIntMethod(NULL, NULL, m, ap);
	va_end(ap);

	return s;
}


jshort _Jv_JNI_CallStaticShortMethodV(JNIEnv *env, jclass clazz,
									  jmethodID methodID, va_list args)
{
	methodinfo *m;
	jshort      s;

	m = (methodinfo *) methodID;

	s = _Jv_jni_CallIntMethod(NULL, NULL, m, args);

	return s;
}


jshort _Jv_JNI_CallStaticShortMethodA(JNIEnv *env, jclass clazz,
									  jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallStaticShortMethodA: IMPLEMENT ME!");

	return 0;
}


jint _Jv_JNI_CallStaticIntMethod(JNIEnv *env, jclass clazz, jmethodID methodID,
								 ...)
{
	methodinfo *m;
	va_list     ap;
	jint        i;

	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	i = _Jv_jni_CallIntMethod(NULL, NULL, m, ap);
	va_end(ap);

	return i;
}


jint _Jv_JNI_CallStaticIntMethodV(JNIEnv *env, jclass clazz,
								  jmethodID methodID, va_list args)
{
	methodinfo *m;
	jint        i;

	m = (methodinfo *) methodID;

	i = _Jv_jni_CallIntMethod(NULL, NULL, m, args);

	return i;
}


jint _Jv_JNI_CallStaticIntMethodA(JNIEnv *env, jclass clazz,
								  jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallStaticIntMethodA: IMPLEMENT ME!");

	return 0;
}


jlong _Jv_JNI_CallStaticLongMethod(JNIEnv *env, jclass clazz,
								   jmethodID methodID, ...)
{
	methodinfo *m;
	va_list     ap;
	jlong       l;

	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	l = _Jv_jni_CallLongMethod(NULL, NULL, m, ap);
	va_end(ap);

	return l;
}


jlong _Jv_JNI_CallStaticLongMethodV(JNIEnv *env, jclass clazz,
									jmethodID methodID, va_list args)
{
	methodinfo *m;
	jlong       l;
	
	m = (methodinfo *) methodID;

	l = _Jv_jni_CallLongMethod(NULL, NULL, m, args);

	return l;
}


jlong _Jv_JNI_CallStaticLongMethodA(JNIEnv *env, jclass clazz,
									jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallStaticLongMethodA: IMPLEMENT ME!");

	return 0;
}



jfloat _Jv_JNI_CallStaticFloatMethod(JNIEnv *env, jclass clazz,
									 jmethodID methodID, ...)
{
	methodinfo *m;
	va_list     ap;
	jfloat      f;

	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	f = _Jv_jni_CallFloatMethod(NULL, NULL, m, ap);
	va_end(ap);

	return f;
}


jfloat _Jv_JNI_CallStaticFloatMethodV(JNIEnv *env, jclass clazz,
									  jmethodID methodID, va_list args)
{
	methodinfo *m;
	jfloat      f;

	m = (methodinfo *) methodID;

	f = _Jv_jni_CallFloatMethod(NULL, NULL, m, args);

	return f;
}


jfloat _Jv_JNI_CallStaticFloatMethodA(JNIEnv *env, jclass clazz,
									  jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallStaticFloatMethodA: IMPLEMENT ME!");

	return 0;
}


jdouble _Jv_JNI_CallStaticDoubleMethod(JNIEnv *env, jclass clazz,
									   jmethodID methodID, ...)
{
	methodinfo *m;
	va_list     ap;
	jdouble     d;

	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	d = _Jv_jni_CallDoubleMethod(NULL, NULL, m, ap);
	va_end(ap);

	return d;
}


jdouble _Jv_JNI_CallStaticDoubleMethodV(JNIEnv *env, jclass clazz,
										jmethodID methodID, va_list args)
{
	methodinfo *m;
	jdouble     d;

	m = (methodinfo *) methodID;

	d = _Jv_jni_CallDoubleMethod(NULL, NULL, m, args);

	return d;
}


jdouble _Jv_JNI_CallStaticDoubleMethodA(JNIEnv *env, jclass clazz,
										jmethodID methodID, jvalue *args)
{
	log_text("JNI-Call: CallStaticDoubleMethodA: IMPLEMENT ME!");

	return 0;
}


void _Jv_JNI_CallStaticVoidMethod(JNIEnv *env, jclass clazz,
								  jmethodID methodID, ...)
{
	methodinfo *m;
	va_list     ap;

	m = (methodinfo *) methodID;

	va_start(ap, methodID);
	_Jv_jni_CallVoidMethod(NULL, NULL, m, ap);
	va_end(ap);
}


void _Jv_JNI_CallStaticVoidMethodV(JNIEnv *env, jclass clazz,
								   jmethodID methodID, va_list args)
{
	methodinfo *m;

	m = (methodinfo *) methodID;

	_Jv_jni_CallVoidMethod(NULL, NULL, m, args);
}


void _Jv_JNI_CallStaticVoidMethodA(JNIEnv *env, jclass clazz,
								   jmethodID methodID, jvalue * args)
{
	methodinfo *m;

	m = (methodinfo *) methodID;

	_Jv_jni_CallVoidMethodA(NULL, NULL, m, args);
}


/* Accessing Static Fields ****************************************************/

/* GetStaticFieldID ************************************************************

   Returns the field ID for a static field of a class. The field is
   specified by its name and signature. The GetStatic<type>Field and
   SetStatic<type>Field families of accessor functions use field IDs
   to retrieve static fields.

*******************************************************************************/

jfieldID _Jv_JNI_GetStaticFieldID(JNIEnv *env, jclass clazz, const char *name,
								  const char *sig)
{
	classinfo *c;
	fieldinfo *f;
	utf       *uname;
	utf       *usig;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;

	uname = utf_new_char((char *) name);
	usig  = utf_new_char((char *) sig);

	f = class_findfield(clazz, uname, usig);
	
	if (f == NULL)
		exceptions_throw_nosuchfielderror(c, uname);

	return (jfieldID) f;
}


/* GetStatic<type>Field ********************************************************

   This family of accessor routines returns the value of a static
   field of an object.

*******************************************************************************/

jobject _Jv_JNI_GetStaticObjectField(JNIEnv *env, jclass clazz,
									 jfieldID fieldID)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return NULL;

	return _Jv_JNI_NewLocalRef(env, f->value.a);
}


jboolean _Jv_JNI_GetStaticBooleanField(JNIEnv *env, jclass clazz,
									   jfieldID fieldID)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return false;

	return f->value.i;
}


jbyte _Jv_JNI_GetStaticByteField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return 0;

	return f->value.i;
}


jchar _Jv_JNI_GetStaticCharField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return 0;

	return f->value.i;
}


jshort _Jv_JNI_GetStaticShortField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return 0;

	return f->value.i;
}


jint _Jv_JNI_GetStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return 0;

	return f->value.i;
}


jlong _Jv_JNI_GetStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return 0;

	return f->value.l;
}


jfloat _Jv_JNI_GetStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return 0.0;

 	return f->value.f;
}


jdouble _Jv_JNI_GetStaticDoubleField(JNIEnv *env, jclass clazz,
									 jfieldID fieldID)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return 0.0;

	return f->value.d;
}


/*  SetStatic<type>Field *******************************************************

	This family of accessor routines sets the value of a static field
	of an object.

*******************************************************************************/

void _Jv_JNI_SetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID,
								  jobject value)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return;

	f->value.a = value;
}


void _Jv_JNI_SetStaticBooleanField(JNIEnv *env, jclass clazz, jfieldID fieldID,
								   jboolean value)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return;

	f->value.i = value;
}


void _Jv_JNI_SetStaticByteField(JNIEnv *env, jclass clazz, jfieldID fieldID,
								jbyte value)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return;

	f->value.i = value;
}


void _Jv_JNI_SetStaticCharField(JNIEnv *env, jclass clazz, jfieldID fieldID,
								jchar value)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return;

	f->value.i = value;
}


void _Jv_JNI_SetStaticShortField(JNIEnv *env, jclass clazz, jfieldID fieldID,
								 jshort value)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return;

	f->value.i = value;
}


void _Jv_JNI_SetStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID,
							   jint value)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return;

	f->value.i = value;
}


void _Jv_JNI_SetStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID,
								jlong value)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return;

	f->value.l = value;
}


void _Jv_JNI_SetStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID,
								 jfloat value)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return;

	f->value.f = value;
}


void _Jv_JNI_SetStaticDoubleField(JNIEnv *env, jclass clazz, jfieldID fieldID,
								  jdouble value)
{
	classinfo *c;
	fieldinfo *f;

	STATISTICS(jniinvokation());

	c = (classinfo *) clazz;
	f = (fieldinfo *) fieldID;

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return;

	f->value.d = value;
}


/* String Operations **********************************************************/

/* NewString *******************************************************************

   Create new java.lang.String object from an array of Unicode
   characters.

*******************************************************************************/

jstring _Jv_JNI_NewString(JNIEnv *env, const jchar *buf, jsize len)
{
	java_lang_String *s;
	java_chararray   *a;
	u4                i;

	STATISTICS(jniinvokation());
	
	s = (java_lang_String *) builtin_new(class_java_lang_String);
	a = builtin_newarray_char(len);

	/* javastring or characterarray could not be created */
	if ((a == NULL) || (s == NULL))
		return NULL;

	/* copy text */
	for (i = 0; i < len; i++)
		a->data[i] = buf[i];

	s->value  = a;
	s->offset = 0;
	s->count  = len;

	return (jstring) _Jv_JNI_NewLocalRef(env, (jobject) s);
}


static jchar emptyStringJ[]={0,0};

/* GetStringLength *************************************************************

   Returns the length (the count of Unicode characters) of a Java
   string.

*******************************************************************************/

jsize _Jv_JNI_GetStringLength(JNIEnv *env, jstring str)
{
	return ((java_lang_String *) str)->count;
}


/********************  convertes javastring to u2-array ****************************/
	
u2 *javastring_tou2(jstring so) 
{
	java_lang_String *s;
	java_chararray   *a;
	u2               *stringbuffer;
	u4                i;

	STATISTICS(jniinvokation());
	
	s = (java_lang_String *) so;

	if (!s)
		return NULL;

	a = s->value;

	if (!a)
		return NULL;

	/* allocate memory */

	stringbuffer = MNEW(u2, s->count + 1);

	/* copy text */

	for (i = 0; i < s->count; i++)
		stringbuffer[i] = a->data[s->offset + i];
	
	/* terminate string */

	stringbuffer[i] = '\0';

	return stringbuffer;
}


/* GetStringChars **************************************************************

   Returns a pointer to the array of Unicode characters of the
   string. This pointer is valid until ReleaseStringChars() is called.

*******************************************************************************/

const jchar *_Jv_JNI_GetStringChars(JNIEnv *env, jstring str, jboolean *isCopy)
{	
	jchar *jc;

	STATISTICS(jniinvokation());

	jc = javastring_tou2(str);

	if (jc)	{
		if (isCopy)
			*isCopy = JNI_TRUE;

		return jc;
	}

	if (isCopy)
		*isCopy = JNI_TRUE;

	return emptyStringJ;
}


/* ReleaseStringChars **********************************************************

   Informs the VM that the native code no longer needs access to
   chars. The chars argument is a pointer obtained from string using
   GetStringChars().

*******************************************************************************/

void _Jv_JNI_ReleaseStringChars(JNIEnv *env, jstring str, const jchar *chars)
{
	STATISTICS(jniinvokation());

	if (chars == emptyStringJ)
		return;

	MFREE(((jchar *) chars), jchar, ((java_lang_String *) str)->count + 1);
}


/* NewStringUTF ****************************************************************

   Constructs a new java.lang.String object from an array of UTF-8
   characters.

*******************************************************************************/

jstring _Jv_JNI_NewStringUTF(JNIEnv *env, const char *bytes)
{
	java_lang_String *s;

	STATISTICS(jniinvokation());

	s = (java_lang_String *) javastring_safe_new_from_utf8(bytes);

    return (jstring) _Jv_JNI_NewLocalRef(env, (jobject) s);
}


/****************** returns the utf8 length in bytes of a string *******************/

jsize _Jv_JNI_GetStringUTFLength (JNIEnv *env, jstring string)
{   
    java_lang_String *s = (java_lang_String*) string;

	STATISTICS(jniinvokation());

    return (jsize) u2_utflength(s->value->data, s->count); 
}


/* GetStringUTFChars ***********************************************************

   Returns a pointer to an array of UTF-8 characters of the
   string. This array is valid until it is released by
   ReleaseStringUTFChars().

*******************************************************************************/

const char *_Jv_JNI_GetStringUTFChars(JNIEnv *env, jstring string,
									  jboolean *isCopy)
{
	utf *u;

	STATISTICS(jniinvokation());

	if (string == NULL)
		return "";

	if (isCopy)
		*isCopy = JNI_TRUE;
	
	u = javastring_toutf((java_objectheader *) string, false);

	if (u != NULL)
		return u->text;

	return "";
}


/* ReleaseStringUTFChars *******************************************************

   Informs the VM that the native code no longer needs access to
   utf. The utf argument is a pointer derived from string using
   GetStringUTFChars().

*******************************************************************************/

void _Jv_JNI_ReleaseStringUTFChars(JNIEnv *env, jstring string, const char *utf)
{
	STATISTICS(jniinvokation());

    /* XXX we don't release utf chars right now, perhaps that should be done 
	   later. Since there is always one reference the garbage collector will
	   never get them */
}


/* Array Operations ***********************************************************/

/* GetArrayLength **************************************************************

   Returns the number of elements in the array.

*******************************************************************************/

jsize _Jv_JNI_GetArrayLength(JNIEnv *env, jarray array)
{
	java_arrayheader *a;

	STATISTICS(jniinvokation());

	a = (java_arrayheader *) array;

	return a->size;
}


/* NewObjectArray **************************************************************

   Constructs a new array holding objects in class elementClass. All
   elements are initially set to initialElement.

*******************************************************************************/

jobjectArray _Jv_JNI_NewObjectArray(JNIEnv *env, jsize length,
									jclass elementClass, jobject initialElement)
{
	java_objectarray *oa;
	s4                i;

	STATISTICS(jniinvokation());

	if (length < 0) {
		exceptions_throw_negativearraysizeexception();
		return NULL;
	}

    oa = builtin_anewarray(length, elementClass);

	if (oa == NULL)
		return NULL;

	/* set all elements to initialElement */

	for (i = 0; i < length; i++)
		oa->data[i] = initialElement;

	return (jobjectArray) _Jv_JNI_NewLocalRef(env, (jobject) oa);
}


jobject _Jv_JNI_GetObjectArrayElement(JNIEnv *env, jobjectArray array,
									  jsize index)
{
	java_objectarray *oa;
	jobject           o;

	STATISTICS(jniinvokation());

	oa = (java_objectarray *) array;

	if (index >= oa->header.size) {
		exceptions_throw_arrayindexoutofboundsexception();
		return NULL;
	}

	o = oa->data[index];

	return _Jv_JNI_NewLocalRef(env, o);
}


void _Jv_JNI_SetObjectArrayElement(JNIEnv *env, jobjectArray array,
								   jsize index, jobject val)
{
	java_objectarray  *oa;
	java_objectheader *o;

	STATISTICS(jniinvokation());

	oa = (java_objectarray *) array;
	o  = (java_objectheader *) val;

	if (index >= oa->header.size) {
		exceptions_throw_arrayindexoutofboundsexception();
		return;
	}

	/* check if the class of value is a subclass of the element class
	   of the array */

	if (!builtin_canstore(oa, o))
		return;

	oa->data[index] = val;
}


jbooleanArray _Jv_JNI_NewBooleanArray(JNIEnv *env, jsize len)
{
	java_booleanarray *ba;

	STATISTICS(jniinvokation());

	if (len < 0) {
		exceptions_throw_negativearraysizeexception();
		return NULL;
	}

	ba = builtin_newarray_boolean(len);

	return (jbooleanArray) _Jv_JNI_NewLocalRef(env, (jobject) ba);
}


jbyteArray _Jv_JNI_NewByteArray(JNIEnv *env, jsize len)
{
	java_bytearray *ba;

	STATISTICS(jniinvokation());

	if (len < 0) {
		exceptions_throw_negativearraysizeexception();
		return NULL;
	}

	ba = builtin_newarray_byte(len);

	return (jbyteArray) _Jv_JNI_NewLocalRef(env, (jobject) ba);
}


jcharArray _Jv_JNI_NewCharArray(JNIEnv *env, jsize len)
{
	java_chararray *ca;

	STATISTICS(jniinvokation());

	if (len < 0) {
		exceptions_throw_negativearraysizeexception();
		return NULL;
	}

	ca = builtin_newarray_char(len);

	return (jcharArray) _Jv_JNI_NewLocalRef(env, (jobject) ca);
}


jshortArray _Jv_JNI_NewShortArray(JNIEnv *env, jsize len)
{
	java_shortarray *sa;

	STATISTICS(jniinvokation());

	if (len < 0) {
		exceptions_throw_negativearraysizeexception();
		return NULL;
	}

	sa = builtin_newarray_short(len);

	return (jshortArray) _Jv_JNI_NewLocalRef(env, (jobject) sa);
}


jintArray _Jv_JNI_NewIntArray(JNIEnv *env, jsize len)
{
	java_intarray *ia;

	STATISTICS(jniinvokation());

	if (len < 0) {
		exceptions_throw_negativearraysizeexception();
		return NULL;
	}

	ia = builtin_newarray_int(len);

	return (jintArray) _Jv_JNI_NewLocalRef(env, (jobject) ia);
}


jlongArray _Jv_JNI_NewLongArray(JNIEnv *env, jsize len)
{
	java_longarray *la;

	STATISTICS(jniinvokation());

	if (len < 0) {
		exceptions_throw_negativearraysizeexception();
		return NULL;
	}

	la = builtin_newarray_long(len);

	return (jlongArray) _Jv_JNI_NewLocalRef(env, (jobject) la);
}


jfloatArray _Jv_JNI_NewFloatArray(JNIEnv *env, jsize len)
{
	java_floatarray *fa;

	STATISTICS(jniinvokation());

	if (len < 0) {
		exceptions_throw_negativearraysizeexception();
		return NULL;
	}

	fa = builtin_newarray_float(len);

	return (jfloatArray) _Jv_JNI_NewLocalRef(env, (jobject) fa);
}


jdoubleArray _Jv_JNI_NewDoubleArray(JNIEnv *env, jsize len)
{
	java_doublearray *da;

	STATISTICS(jniinvokation());

	if (len < 0) {
		exceptions_throw_negativearraysizeexception();
		return NULL;
	}

	da = builtin_newarray_double(len);

	return (jdoubleArray) _Jv_JNI_NewLocalRef(env, (jobject) da);
}


/* Get<PrimitiveType>ArrayElements *********************************************

   A family of functions that returns the body of the primitive array.

*******************************************************************************/

jboolean *_Jv_JNI_GetBooleanArrayElements(JNIEnv *env, jbooleanArray array,
										  jboolean *isCopy)
{
	java_booleanarray *ba;

	STATISTICS(jniinvokation());

	ba = (java_booleanarray *) array;

	if (isCopy)
		*isCopy = JNI_FALSE;

	return ba->data;
}


jbyte *_Jv_JNI_GetByteArrayElements(JNIEnv *env, jbyteArray array,
									jboolean *isCopy)
{
	java_bytearray *ba;

	STATISTICS(jniinvokation());

	ba = (java_bytearray *) array;

	if (isCopy)
		*isCopy = JNI_FALSE;

	return ba->data;
}


jchar *_Jv_JNI_GetCharArrayElements(JNIEnv *env, jcharArray array,
									jboolean *isCopy)
{
	java_chararray *ca;

	STATISTICS(jniinvokation());

	ca = (java_chararray *) array;

	if (isCopy)
		*isCopy = JNI_FALSE;

	return ca->data;
}


jshort *_Jv_JNI_GetShortArrayElements(JNIEnv *env, jshortArray array,
									  jboolean *isCopy)
{
	java_shortarray *sa;

	STATISTICS(jniinvokation());

	sa = (java_shortarray *) array;

	if (isCopy)
		*isCopy = JNI_FALSE;

	return sa->data;
}


jint *_Jv_JNI_GetIntArrayElements(JNIEnv *env, jintArray array,
								  jboolean *isCopy)
{
	java_intarray *ia;

	STATISTICS(jniinvokation());

	ia = (java_intarray *) array;

	if (isCopy)
		*isCopy = JNI_FALSE;

	return ia->data;
}


jlong *_Jv_JNI_GetLongArrayElements(JNIEnv *env, jlongArray array,
									jboolean *isCopy)
{
	java_longarray *la;

	STATISTICS(jniinvokation());

	la = (java_longarray *) array;

	if (isCopy)
		*isCopy = JNI_FALSE;

	/* We cast this one to prevent a compiler warning on 64-bit
	   systems since GNU Classpath typedef jlong to long long. */

	return (jlong *) la->data;
}


jfloat *_Jv_JNI_GetFloatArrayElements(JNIEnv *env, jfloatArray array,
									  jboolean *isCopy)
{
	java_floatarray *fa;

	STATISTICS(jniinvokation());

	fa = (java_floatarray *) array;

	if (isCopy)
		*isCopy = JNI_FALSE;

	return fa->data;
}


jdouble *_Jv_JNI_GetDoubleArrayElements(JNIEnv *env, jdoubleArray array,
										jboolean *isCopy)
{
	java_doublearray *da;

	STATISTICS(jniinvokation());

	da = (java_doublearray *) array;

	if (isCopy)
		*isCopy = JNI_FALSE;

	return da->data;
}


/* Release<PrimitiveType>ArrayElements *****************************************

   A family of functions that informs the VM that the native code no
   longer needs access to elems. The elems argument is a pointer
   derived from array using the corresponding
   Get<PrimitiveType>ArrayElements() function. If necessary, this
   function copies back all changes made to elems to the original
   array.

*******************************************************************************/

void _Jv_JNI_ReleaseBooleanArrayElements(JNIEnv *env, jbooleanArray array,
										 jboolean *elems, jint mode)
{
	java_booleanarray *ba;

	STATISTICS(jniinvokation());

	ba = (java_booleanarray *) array;

	if (elems != ba->data) {
		switch (mode) {
		case JNI_COMMIT:
			MCOPY(ba->data, elems, u1, ba->header.size);
			break;
		case 0:
			MCOPY(ba->data, elems, u1, ba->header.size);
			/* XXX TWISTI how should it be freed? */
			break;
		case JNI_ABORT:
			/* XXX TWISTI how should it be freed? */
			break;
		}
	}
}


void _Jv_JNI_ReleaseByteArrayElements(JNIEnv *env, jbyteArray array,
									  jbyte *elems, jint mode)
{
	java_bytearray *ba;

	STATISTICS(jniinvokation());

	ba = (java_bytearray *) array;

	if (elems != ba->data) {
		switch (mode) {
		case JNI_COMMIT:
			MCOPY(ba->data, elems, s1, ba->header.size);
			break;
		case 0:
			MCOPY(ba->data, elems, s1, ba->header.size);
			/* XXX TWISTI how should it be freed? */
			break;
		case JNI_ABORT:
			/* XXX TWISTI how should it be freed? */
			break;
		}
	}
}


void _Jv_JNI_ReleaseCharArrayElements(JNIEnv *env, jcharArray array,
									  jchar *elems, jint mode)
{
	java_chararray *ca;

	STATISTICS(jniinvokation());

	ca = (java_chararray *) array;

	if (elems != ca->data) {
		switch (mode) {
		case JNI_COMMIT:
			MCOPY(ca->data, elems, u2, ca->header.size);
			break;
		case 0:
			MCOPY(ca->data, elems, u2, ca->header.size);
			/* XXX TWISTI how should it be freed? */
			break;
		case JNI_ABORT:
			/* XXX TWISTI how should it be freed? */
			break;
		}
	}
}


void _Jv_JNI_ReleaseShortArrayElements(JNIEnv *env, jshortArray array,
									   jshort *elems, jint mode)
{
	java_shortarray *sa;

	STATISTICS(jniinvokation());

	sa = (java_shortarray *) array;

	if (elems != sa->data) {
		switch (mode) {
		case JNI_COMMIT:
			MCOPY(sa->data, elems, s2, sa->header.size);
			break;
		case 0:
			MCOPY(sa->data, elems, s2, sa->header.size);
			/* XXX TWISTI how should it be freed? */
			break;
		case JNI_ABORT:
			/* XXX TWISTI how should it be freed? */
			break;
		}
	}
}


void _Jv_JNI_ReleaseIntArrayElements(JNIEnv *env, jintArray array, jint *elems,
									 jint mode)
{
	java_intarray *ia;

	STATISTICS(jniinvokation());

	ia = (java_intarray *) array;

	if (elems != ia->data) {
		switch (mode) {
		case JNI_COMMIT:
			MCOPY(ia->data, elems, s4, ia->header.size);
			break;
		case 0:
			MCOPY(ia->data, elems, s4, ia->header.size);
			/* XXX TWISTI how should it be freed? */
			break;
		case JNI_ABORT:
			/* XXX TWISTI how should it be freed? */
			break;
		}
	}
}


void _Jv_JNI_ReleaseLongArrayElements(JNIEnv *env, jlongArray array,
									  jlong *elems, jint mode)
{
	java_longarray *la;

	STATISTICS(jniinvokation());

	la = (java_longarray *) array;

	/* We cast this one to prevent a compiler warning on 64-bit
	   systems since GNU Classpath typedef jlong to long long. */

	if ((s8 *) elems != la->data) {
		switch (mode) {
		case JNI_COMMIT:
			MCOPY(la->data, elems, s8, la->header.size);
			break;
		case 0:
			MCOPY(la->data, elems, s8, la->header.size);
			/* XXX TWISTI how should it be freed? */
			break;
		case JNI_ABORT:
			/* XXX TWISTI how should it be freed? */
			break;
		}
	}
}


void _Jv_JNI_ReleaseFloatArrayElements(JNIEnv *env, jfloatArray array,
									   jfloat *elems, jint mode)
{
	java_floatarray *fa;

	STATISTICS(jniinvokation());

	fa = (java_floatarray *) array;

	if (elems != fa->data) {
		switch (mode) {
		case JNI_COMMIT:
			MCOPY(fa->data, elems, float, fa->header.size);
			break;
		case 0:
			MCOPY(fa->data, elems, float, fa->header.size);
			/* XXX TWISTI how should it be freed? */
			break;
		case JNI_ABORT:
			/* XXX TWISTI how should it be freed? */
			break;
		}
	}
}


void _Jv_JNI_ReleaseDoubleArrayElements(JNIEnv *env, jdoubleArray array,
										jdouble *elems, jint mode)
{
	java_doublearray *da;

	STATISTICS(jniinvokation());

	da = (java_doublearray *) array;

	if (elems != da->data) {
		switch (mode) {
		case JNI_COMMIT:
			MCOPY(da->data, elems, double, da->header.size);
			break;
		case 0:
			MCOPY(da->data, elems, double, da->header.size);
			/* XXX TWISTI how should it be freed? */
			break;
		case JNI_ABORT:
			/* XXX TWISTI how should it be freed? */
			break;
		}
	}
}


/*  Get<PrimitiveType>ArrayRegion **********************************************

	A family of functions that copies a region of a primitive array
	into a buffer.

*******************************************************************************/

void _Jv_JNI_GetBooleanArrayRegion(JNIEnv *env, jbooleanArray array,
								   jsize start, jsize len, jboolean *buf)
{
	java_booleanarray *ba;

	STATISTICS(jniinvokation());

	ba = (java_booleanarray *) array;

    if ((start < 0) || (len < 0) || (start + len > ba->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
    else
		MCOPY(buf, &ba->data[start], u1, len);
}


void _Jv_JNI_GetByteArrayRegion(JNIEnv *env, jbyteArray array, jsize start,
								jsize len, jbyte *buf)
{
	java_bytearray *ba;

	STATISTICS(jniinvokation());

	ba = (java_bytearray *) array;

	if ((start < 0) || (len < 0) || (start + len > ba->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(buf, &ba->data[start], s1, len);
}


void _Jv_JNI_GetCharArrayRegion(JNIEnv *env, jcharArray array, jsize start,
								jsize len, jchar *buf)
{
	java_chararray *ca;

	STATISTICS(jniinvokation());

	ca = (java_chararray *) array;

	if ((start < 0) || (len < 0) || (start + len > ca->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(buf, &ca->data[start], u2, len);
}


void _Jv_JNI_GetShortArrayRegion(JNIEnv *env, jshortArray array, jsize start,
								 jsize len, jshort *buf)
{
	java_shortarray *sa;

	STATISTICS(jniinvokation());

	sa = (java_shortarray *) array;

	if ((start < 0) || (len < 0) || (start + len > sa->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else	
		MCOPY(buf, &sa->data[start], s2, len);
}


void _Jv_JNI_GetIntArrayRegion(JNIEnv *env, jintArray array, jsize start,
							   jsize len, jint *buf)
{
	java_intarray *ia;

	STATISTICS(jniinvokation());

	ia = (java_intarray *) array;

	if ((start < 0) || (len < 0) || (start + len > ia->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(buf, &ia->data[start], s4, len);
}


void _Jv_JNI_GetLongArrayRegion(JNIEnv *env, jlongArray array, jsize start,
								jsize len, jlong *buf)
{
	java_longarray *la;

	STATISTICS(jniinvokation());

	la = (java_longarray *) array;

	if ((start < 0) || (len < 0) || (start + len > la->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(buf, &la->data[start], s8, len);
}


void _Jv_JNI_GetFloatArrayRegion(JNIEnv *env, jfloatArray array, jsize start,
								 jsize len, jfloat *buf)
{
	java_floatarray *fa;

	STATISTICS(jniinvokation());

	fa = (java_floatarray *) array;

	if ((start < 0) || (len < 0) || (start + len > fa->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(buf, &fa->data[start], float, len);
}


void _Jv_JNI_GetDoubleArrayRegion(JNIEnv *env, jdoubleArray array, jsize start,
								  jsize len, jdouble *buf)
{
	java_doublearray *da;

	STATISTICS(jniinvokation());

	da = (java_doublearray *) array;

	if ((start < 0) || (len < 0) || (start + len > da->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(buf, &da->data[start], double, len);
}


/*  Set<PrimitiveType>ArrayRegion **********************************************

	A family of functions that copies back a region of a primitive
	array from a buffer.

*******************************************************************************/

void _Jv_JNI_SetBooleanArrayRegion(JNIEnv *env, jbooleanArray array,
								   jsize start, jsize len, jboolean *buf)
{
	java_booleanarray *ba;

	STATISTICS(jniinvokation());

	ba = (java_booleanarray *) array;

	if ((start < 0) || (len < 0) || (start + len > ba->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(&ba->data[start], buf, u1, len);
}


void _Jv_JNI_SetByteArrayRegion(JNIEnv *env, jbyteArray array, jsize start,
								jsize len, jbyte *buf)
{
	java_bytearray *ba;

	STATISTICS(jniinvokation());

	ba = (java_bytearray *) array;

	if ((start < 0) || (len < 0) || (start + len > ba->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(&ba->data[start], buf, s1, len);
}


void _Jv_JNI_SetCharArrayRegion(JNIEnv *env, jcharArray array, jsize start,
								jsize len, jchar *buf)
{
	java_chararray *ca;

	STATISTICS(jniinvokation());

	ca = (java_chararray *) array;

	if ((start < 0) || (len < 0) || (start + len > ca->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(&ca->data[start], buf, u2, len);
}


void _Jv_JNI_SetShortArrayRegion(JNIEnv *env, jshortArray array, jsize start,
								 jsize len, jshort *buf)
{
	java_shortarray *sa;

	STATISTICS(jniinvokation());

	sa = (java_shortarray *) array;

	if ((start < 0) || (len < 0) || (start + len > sa->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(&sa->data[start], buf, s2, len);
}


void _Jv_JNI_SetIntArrayRegion(JNIEnv *env, jintArray array, jsize start,
							   jsize len, jint *buf)
{
	java_intarray *ia;

	STATISTICS(jniinvokation());

	ia = (java_intarray *) array;

	if ((start < 0) || (len < 0) || (start + len > ia->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(&ia->data[start], buf, s4, len);
}


void _Jv_JNI_SetLongArrayRegion(JNIEnv* env, jlongArray array, jsize start,
								jsize len, jlong *buf)
{
	java_longarray *la;

	STATISTICS(jniinvokation());

	la = (java_longarray *) array;

	if ((start < 0) || (len < 0) || (start + len > la->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(&la->data[start], buf, s8, len);
}


void _Jv_JNI_SetFloatArrayRegion(JNIEnv *env, jfloatArray array, jsize start,
								 jsize len, jfloat *buf)
{
	java_floatarray *fa;

	STATISTICS(jniinvokation());

	fa = (java_floatarray *) array;

	if ((start < 0) || (len < 0) || (start + len > fa->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(&fa->data[start], buf, float, len);
}


void _Jv_JNI_SetDoubleArrayRegion(JNIEnv *env, jdoubleArray array, jsize start,
								  jsize len, jdouble *buf)
{
	java_doublearray *da;

	STATISTICS(jniinvokation());

	da = (java_doublearray *) array;

	if ((start < 0) || (len < 0) || (start + len > da->header.size))
		exceptions_throw_arrayindexoutofboundsexception();
	else
		MCOPY(&da->data[start], buf, double, len);
}


/* Registering Native Methods *************************************************/

/* RegisterNatives *************************************************************

   Registers native methods with the class specified by the clazz
   argument. The methods parameter specifies an array of
   JNINativeMethod structures that contain the names, signatures, and
   function pointers of the native methods. The nMethods parameter
   specifies the number of native methods in the array.

*******************************************************************************/

jint _Jv_JNI_RegisterNatives(JNIEnv *env, jclass clazz,
							 const JNINativeMethod *methods, jint nMethods)
{
	STATISTICS(jniinvokation());

    log_text("JNI-Call: RegisterNatives: IMPLEMENT ME!!!");
	/* XXX: if implemented this needs a call to jvmti_NativeMethodBind
	if (jvmti) jvmti_NativeMethodBind(method, address,  new_address_ptr);
	*/

    return 0;
}


/* UnregisterNatives ***********************************************************

   Unregisters native methods of a class. The class goes back to the
   state before it was linked or registered with its native method
   functions.

   This function should not be used in normal native code. Instead, it
   provides special programs a way to reload and relink native
   libraries.

*******************************************************************************/

jint _Jv_JNI_UnregisterNatives(JNIEnv *env, jclass clazz)
{
	STATISTICS(jniinvokation());

	/* XXX TWISTI hmm, maybe we should not support that (like kaffe) */

    log_text("JNI-Call: UnregisterNatives: IMPLEMENT ME!!!");

    return 0;
}


/* Monitor Operations *********************************************************/

/* MonitorEnter ****************************************************************

   Enters the monitor associated with the underlying Java object
   referred to by obj.

*******************************************************************************/

jint _Jv_JNI_MonitorEnter(JNIEnv *env, jobject obj)
{
	STATISTICS(jniinvokation());

	if (obj == NULL) {
		exceptions_throw_nullpointerexception();
		return JNI_ERR;
	}

	LOCK_MONITOR_ENTER(obj);

	return JNI_OK;
}


/* MonitorExit *****************************************************************

   The current thread must be the owner of the monitor associated with
   the underlying Java object referred to by obj. The thread
   decrements the counter indicating the number of times it has
   entered this monitor. If the value of the counter becomes zero, the
   current thread releases the monitor.

*******************************************************************************/

jint _Jv_JNI_MonitorExit(JNIEnv *env, jobject obj)
{
	STATISTICS(jniinvokation());

	if (obj == NULL) {
		exceptions_throw_nullpointerexception();
		return JNI_ERR;
	}

	LOCK_MONITOR_EXIT(obj);

	return JNI_OK;
}


/* JavaVM Interface ***********************************************************/

/* GetJavaVM *******************************************************************

   Returns the Java VM interface (used in the Invocation API)
   associated with the current thread. The result is placed at the
   location pointed to by the second argument, vm.

*******************************************************************************/

jint _Jv_JNI_GetJavaVM(JNIEnv *env, JavaVM **vm)
{
	STATISTICS(jniinvokation());

    *vm = (JavaVM *) _Jv_jvm;

	return 0;
}


/* GetStringRegion *************************************************************

   Copies len number of Unicode characters beginning at offset start
   to the given buffer buf.

   Throws StringIndexOutOfBoundsException on index overflow.

*******************************************************************************/

void _Jv_JNI_GetStringRegion(JNIEnv* env, jstring str, jsize start, jsize len,
							 jchar *buf)
{
	java_lang_String *s;
	java_chararray   *ca;

	STATISTICS(jniinvokation());

	s  = (java_lang_String *) str;
	ca = s->value;

	if ((start < 0) || (len < 0) || (start > s->count) ||
		(start + len > s->count)) {
		exceptions_throw_stringindexoutofboundsexception();
		return;
	}

	MCOPY(buf, &ca->data[start], u2, len);
}


void _Jv_JNI_GetStringUTFRegion(JNIEnv* env, jstring str, jsize start,
								jsize len, char *buf)
{
	STATISTICS(jniinvokation());

	log_text("JNI-Call: GetStringUTFRegion: IMPLEMENT ME!");
}


/* GetPrimitiveArrayCritical ***************************************************

   Obtain a direct pointer to array elements.

*******************************************************************************/

void *_Jv_JNI_GetPrimitiveArrayCritical(JNIEnv *env, jarray array,
										jboolean *isCopy)
{
	java_bytearray *ba;
	jbyte          *bp;

	ba = (java_bytearray *) array;

	/* do the same as Kaffe does */

	bp = _Jv_JNI_GetByteArrayElements(env, ba, isCopy);

	return (void *) bp;
}


/* ReleasePrimitiveArrayCritical ***********************************************

   No specific documentation.

*******************************************************************************/

void _Jv_JNI_ReleasePrimitiveArrayCritical(JNIEnv *env, jarray array,
										   void *carray, jint mode)
{
	STATISTICS(jniinvokation());

	/* do the same as Kaffe does */

	_Jv_JNI_ReleaseByteArrayElements(env, (jbyteArray) array, (jbyte *) carray,
									 mode);
}


/* GetStringCritical ***********************************************************

   The semantics of these two functions are similar to the existing
   Get/ReleaseStringChars functions.

*******************************************************************************/

const jchar *_Jv_JNI_GetStringCritical(JNIEnv *env, jstring string,
									   jboolean *isCopy)
{
	STATISTICS(jniinvokation());

	return _Jv_JNI_GetStringChars(env, string, isCopy);
}


void _Jv_JNI_ReleaseStringCritical(JNIEnv *env, jstring string,
								   const jchar *cstring)
{
	STATISTICS(jniinvokation());

	_Jv_JNI_ReleaseStringChars(env, string, cstring);
}


jweak _Jv_JNI_NewWeakGlobalRef(JNIEnv* env, jobject obj)
{
	STATISTICS(jniinvokation());

	log_text("JNI-Call: NewWeakGlobalRef: IMPLEMENT ME!");

	return obj;
}


void _Jv_JNI_DeleteWeakGlobalRef(JNIEnv* env, jweak ref)
{
	STATISTICS(jniinvokation());

	log_text("JNI-Call: DeleteWeakGlobalRef: IMPLEMENT ME");
}


/* NewGlobalRef ****************************************************************

   Creates a new global reference to the object referred to by the obj
   argument.

*******************************************************************************/
    
jobject _Jv_JNI_NewGlobalRef(JNIEnv* env, jobject obj)
{
	hashtable_global_ref_entry *gre;
	u4   key;                           /* hashkey                            */
	u4   slot;                          /* slot in hashtable                  */

	STATISTICS(jniinvokation());

	LOCK_MONITOR_ENTER(hashtable_global_ref->header);

	/* normally addresses are aligned to 4, 8 or 16 bytes */

	key  = ((u4) (ptrint) obj) >> 4;           /* align to 16-byte boundaries */
	slot = key & (hashtable_global_ref->size - 1);
	gre  = hashtable_global_ref->ptr[slot];
	
	/* search external hash chain for the entry */

	while (gre) {
		if (gre->o == obj) {
			/* global object found, increment the reference */

			gre->refs++;

			LOCK_MONITOR_EXIT(hashtable_global_ref->header);

			return obj;
		}

		gre = gre->hashlink;                /* next element in external chain */
	}

	/* global ref not found, create a new one */

	gre = NEW(hashtable_global_ref_entry);

	gre->o    = obj;
	gre->refs = 1;

	/* insert entry into hashtable */

	gre->hashlink = hashtable_global_ref->ptr[slot];

	hashtable_global_ref->ptr[slot] = gre;

	/* update number of hashtable-entries */

	hashtable_global_ref->entries++;

	LOCK_MONITOR_EXIT(hashtable_global_ref->header);

	return obj;
}


/* DeleteGlobalRef *************************************************************

   Deletes the global reference pointed to by globalRef.

*******************************************************************************/

void _Jv_JNI_DeleteGlobalRef(JNIEnv* env, jobject globalRef)
{
	hashtable_global_ref_entry *gre;
	hashtable_global_ref_entry *prevgre;
	u4   key;                           /* hashkey                            */
	u4   slot;                          /* slot in hashtable                  */

	STATISTICS(jniinvokation());

	LOCK_MONITOR_ENTER(hashtable_global_ref->header);

	/* normally addresses are aligned to 4, 8 or 16 bytes */

	key  = ((u4) (ptrint) globalRef) >> 4;     /* align to 16-byte boundaries */
	slot = key & (hashtable_global_ref->size - 1);
	gre  = hashtable_global_ref->ptr[slot];

	/* initialize prevgre */

	prevgre = NULL;

	/* search external hash chain for the entry */

	while (gre) {
		if (gre->o == globalRef) {
			/* global object found, decrement the reference count */

			gre->refs--;

			/* if reference count is 0, remove the entry */

			if (gre->refs == 0) {
				/* special handling if it's the first in the chain */

				if (prevgre == NULL)
					hashtable_global_ref->ptr[slot] = gre->hashlink;
				else
					prevgre->hashlink = gre->hashlink;

				FREE(gre, hashtable_global_ref_entry);
			}

			LOCK_MONITOR_EXIT(hashtable_global_ref->header);

			return;
		}

		prevgre = gre;                    /* save current pointer for removal */
		gre     = gre->hashlink;            /* next element in external chain */
	}

	log_println("JNI-DeleteGlobalRef: global reference not found");

	LOCK_MONITOR_EXIT(hashtable_global_ref->header);
}


/* ExceptionCheck **************************************************************

   Returns JNI_TRUE when there is a pending exception; otherwise,
   returns JNI_FALSE.

*******************************************************************************/

jboolean _Jv_JNI_ExceptionCheck(JNIEnv *env)
{
	java_objectheader *o;

	STATISTICS(jniinvokation());

	o = exceptions_get_exception();

	return (o != NULL) ? JNI_TRUE : JNI_FALSE;
}


/* New JNI 1.4 functions ******************************************************/

/* NewDirectByteBuffer *********************************************************

   Allocates and returns a direct java.nio.ByteBuffer referring to the
   block of memory starting at the memory address address and
   extending capacity bytes.

*******************************************************************************/

jobject _Jv_JNI_NewDirectByteBuffer(JNIEnv *env, void *address, jlong capacity)
{
#if defined(ENABLE_JAVASE)
	java_objectheader       *nbuf;
# if SIZEOF_VOID_P == 8
	gnu_classpath_Pointer64 *paddress;
# else
	gnu_classpath_Pointer32 *paddress;
# endif

	STATISTICS(jniinvokation());

	/* alocate a gnu.classpath.Pointer{32,64} object */

# if SIZEOF_VOID_P == 8
	if (!(paddress = (gnu_classpath_Pointer64 *)
		  builtin_new(class_gnu_classpath_Pointer64)))
# else
	if (!(paddress = (gnu_classpath_Pointer32 *)
		  builtin_new(class_gnu_classpath_Pointer32)))
# endif
		return NULL;

	/* fill gnu.classpath.Pointer{32,64} with address */

	paddress->data = (ptrint) address;

	/* create a java.nio.DirectByteBufferImpl$ReadWrite object */

	nbuf = (*env)->NewObject(env, class_java_nio_DirectByteBufferImpl_ReadWrite,
							 (jmethodID) dbbirw_init, NULL, paddress,
							 (jint) capacity, (jint) capacity, (jint) 0);

	/* add local reference and return the value */

	return _Jv_JNI_NewLocalRef(env, nbuf);
#else
	vm_abort("_Jv_JNI_NewDirectByteBuffer: not implemented in this configuration");

	/* keep compiler happy */

	return NULL;
#endif
}


/* GetDirectBufferAddress ******************************************************

   Fetches and returns the starting address of the memory region
   referenced by the given direct java.nio.Buffer.

*******************************************************************************/

void *_Jv_JNI_GetDirectBufferAddress(JNIEnv *env, jobject buf)
{
#if defined(ENABLE_JAVASE)
	java_nio_DirectByteBufferImpl *nbuf;
# if SIZEOF_VOID_P == 8
	gnu_classpath_Pointer64       *address;
# else
	gnu_classpath_Pointer32       *address;
# endif

	STATISTICS(jniinvokation());

	if (!builtin_instanceof(buf, class_java_nio_Buffer))
		return NULL;

	nbuf = (java_nio_DirectByteBufferImpl *) buf;

# if SIZEOF_VOID_P == 8
	address = (gnu_classpath_Pointer64 *) nbuf->address;
# else
	address = (gnu_classpath_Pointer32 *) nbuf->address;
# endif

	if (address == NULL)
		return NULL;

	return (void *) address->data;
#else
	vm_abort("_Jv_JNI_GetDirectBufferAddress: not implemented in this configuration");

	/* keep compiler happy */

	return NULL;
#endif
}


/* GetDirectBufferCapacity *****************************************************

   Fetches and returns the capacity in bytes of the memory region
   referenced by the given direct java.nio.Buffer.

*******************************************************************************/

jlong _Jv_JNI_GetDirectBufferCapacity(JNIEnv* env, jobject buf)
{
#if defined(ENABLE_JAVASE)
	java_nio_Buffer *nbuf;

	STATISTICS(jniinvokation());

	if (!builtin_instanceof(buf, class_java_nio_DirectByteBufferImpl))
		return -1;

	nbuf = (java_nio_Buffer *) buf;

	return (jlong) nbuf->cap;
#else
	vm_abort("_Jv_JNI_GetDirectBufferCapacity: not implemented in this configuration");

	/* keep compiler happy */

	return 0;
#endif
}


/* DestroyJavaVM ***************************************************************

   Unloads a Java VM and reclaims its resources. Only the main thread
   can unload the VM. The system waits until the main thread is only
   remaining user thread before it destroys the VM.

*******************************************************************************/

jint _Jv_JNI_DestroyJavaVM(JavaVM *vm)
{
	s4 status;

	STATISTICS(jniinvokation());

    status = vm_destroy(vm);

	return status;
}


/* AttachCurrentThread *********************************************************

   Attaches the current thread to a Java VM. Returns a JNI interface
   pointer in the JNIEnv argument.

   Trying to attach a thread that is already attached is a no-op.

   A native thread cannot be attached simultaneously to two Java VMs.

   When a thread is attached to the VM, the context class loader is
   the bootstrap loader.

*******************************************************************************/

static s4 jni_attach_current_thread(void **p_env, void *thr_args, bool isdaemon)
{
	JavaVMAttachArgs *vm_aargs;

#if defined(ENABLE_THREADS)
	if (threads_get_current_threadobject() == NULL) {
		vm_aargs = (JavaVMAttachArgs *) thr_args;

		if (vm_aargs != NULL) {
			if ((vm_aargs->version != JNI_VERSION_1_2) &&
				(vm_aargs->version != JNI_VERSION_1_4))
				return JNI_EVERSION;
		}

		if (!threads_attach_current_thread(vm_aargs, false))
			return JNI_ERR;

		if (!jni_init_localref_table())
			return JNI_ERR;
	}
#endif

	*p_env = _Jv_env;

	return JNI_OK;
}


jint _Jv_JNI_AttachCurrentThread(JavaVM *vm, void **p_env, void *thr_args)
{
	STATISTICS(jniinvokation());

	return jni_attach_current_thread(p_env, thr_args, false);
}


/* DetachCurrentThread *********************************************************

   Detaches the current thread from a Java VM. All Java monitors held
   by this thread are released. All Java threads waiting for this
   thread to die are notified.

   In JDK 1.1, the main thread cannot be detached from the VM. It must
   call DestroyJavaVM to unload the entire VM.

   In the JDK, the main thread can be detached from the VM.

   The main thread, which is the thread that created the Java VM,
   cannot be detached from the VM. Instead, the main thread must call
   JNI_DestroyJavaVM() to unload the entire VM.

*******************************************************************************/

jint _Jv_JNI_DetachCurrentThread(JavaVM *vm)
{
#if defined(ENABLE_THREADS)
	threadobject *thread;

	STATISTICS(jniinvokation());

	thread = threads_get_current_threadobject();

	if (thread == NULL)
		return JNI_ERR;

	if (!threads_detach_thread(thread))
		return JNI_ERR;
#endif

	return JNI_OK;
}


/* GetEnv **********************************************************************

   If the current thread is not attached to the VM, sets *env to NULL,
   and returns JNI_EDETACHED. If the specified version is not
   supported, sets *env to NULL, and returns JNI_EVERSION. Otherwise,
   sets *env to the appropriate interface, and returns JNI_OK.

*******************************************************************************/

jint _Jv_JNI_GetEnv(JavaVM *vm, void **env, jint version)
{
	STATISTICS(jniinvokation());

#if defined(ENABLE_THREADS)
	if (threads_get_current_threadobject() == NULL) {
		*env = NULL;

		return JNI_EDETACHED;
	}
#endif

	/* check the JNI version */

	switch (version) {
	case JNI_VERSION_1_1:
	case JNI_VERSION_1_2:
	case JNI_VERSION_1_4:
		*env = _Jv_env;
		return JNI_OK;

	default:
		;
	}

#if defined(ENABLE_JVMTI)
	if ((version & JVMTI_VERSION_MASK_INTERFACE_TYPE) 
		== JVMTI_VERSION_INTERFACE_JVMTI) {

		*env = (void *) jvmti_new_environment();

		if (env != NULL)
			return JNI_OK;
	}
#endif
	
	*env = NULL;

	return JNI_EVERSION;
}


/* AttachCurrentThreadAsDaemon *************************************************

   Same semantics as AttachCurrentThread, but the newly-created
   java.lang.Thread instance is a daemon.

   If the thread has already been attached via either
   AttachCurrentThread or AttachCurrentThreadAsDaemon, this routine
   simply sets the value pointed to by penv to the JNIEnv of the
   current thread. In this case neither AttachCurrentThread nor this
   routine have any effect on the daemon status of the thread.

*******************************************************************************/

jint _Jv_JNI_AttachCurrentThreadAsDaemon(JavaVM *vm, void **penv, void *args)
{
	STATISTICS(jniinvokation());

	return jni_attach_current_thread(penv, args, true);
}


/* JNI invocation table *******************************************************/

const struct JNIInvokeInterface _Jv_JNIInvokeInterface = {
	NULL,
	NULL,
	NULL,

	_Jv_JNI_DestroyJavaVM,
	_Jv_JNI_AttachCurrentThread,
	_Jv_JNI_DetachCurrentThread,
	_Jv_JNI_GetEnv,
	_Jv_JNI_AttachCurrentThreadAsDaemon
};


/* JNI function table *********************************************************/

struct JNINativeInterface _Jv_JNINativeInterface = {
	NULL,
	NULL,
	NULL,
	NULL,    
	_Jv_JNI_GetVersion,

	_Jv_JNI_DefineClass,
	_Jv_JNI_FindClass,
	_Jv_JNI_FromReflectedMethod,
	_Jv_JNI_FromReflectedField,
	_Jv_JNI_ToReflectedMethod,
	_Jv_JNI_GetSuperclass,
	_Jv_JNI_IsAssignableFrom,
	_Jv_JNI_ToReflectedField,

	_Jv_JNI_Throw,
	_Jv_JNI_ThrowNew,
	_Jv_JNI_ExceptionOccurred,
	_Jv_JNI_ExceptionDescribe,
	_Jv_JNI_ExceptionClear,
	_Jv_JNI_FatalError,
	_Jv_JNI_PushLocalFrame,
	_Jv_JNI_PopLocalFrame,

	_Jv_JNI_NewGlobalRef,
	_Jv_JNI_DeleteGlobalRef,
	_Jv_JNI_DeleteLocalRef,
	_Jv_JNI_IsSameObject,
	_Jv_JNI_NewLocalRef,
	_Jv_JNI_EnsureLocalCapacity,

	_Jv_JNI_AllocObject,
	_Jv_JNI_NewObject,
	_Jv_JNI_NewObjectV,
	_Jv_JNI_NewObjectA,

	_Jv_JNI_GetObjectClass,
	_Jv_JNI_IsInstanceOf,

	_Jv_JNI_GetMethodID,

	_Jv_JNI_CallObjectMethod,
	_Jv_JNI_CallObjectMethodV,
	_Jv_JNI_CallObjectMethodA,
	_Jv_JNI_CallBooleanMethod,
	_Jv_JNI_CallBooleanMethodV,
	_Jv_JNI_CallBooleanMethodA,
	_Jv_JNI_CallByteMethod,
	_Jv_JNI_CallByteMethodV,
	_Jv_JNI_CallByteMethodA,
	_Jv_JNI_CallCharMethod,
	_Jv_JNI_CallCharMethodV,
	_Jv_JNI_CallCharMethodA,
	_Jv_JNI_CallShortMethod,
	_Jv_JNI_CallShortMethodV,
	_Jv_JNI_CallShortMethodA,
	_Jv_JNI_CallIntMethod,
	_Jv_JNI_CallIntMethodV,
	_Jv_JNI_CallIntMethodA,
	_Jv_JNI_CallLongMethod,
	_Jv_JNI_CallLongMethodV,
	_Jv_JNI_CallLongMethodA,
	_Jv_JNI_CallFloatMethod,
	_Jv_JNI_CallFloatMethodV,
	_Jv_JNI_CallFloatMethodA,
	_Jv_JNI_CallDoubleMethod,
	_Jv_JNI_CallDoubleMethodV,
	_Jv_JNI_CallDoubleMethodA,
	_Jv_JNI_CallVoidMethod,
	_Jv_JNI_CallVoidMethodV,
	_Jv_JNI_CallVoidMethodA,

	_Jv_JNI_CallNonvirtualObjectMethod,
	_Jv_JNI_CallNonvirtualObjectMethodV,
	_Jv_JNI_CallNonvirtualObjectMethodA,
	_Jv_JNI_CallNonvirtualBooleanMethod,
	_Jv_JNI_CallNonvirtualBooleanMethodV,
	_Jv_JNI_CallNonvirtualBooleanMethodA,
	_Jv_JNI_CallNonvirtualByteMethod,
	_Jv_JNI_CallNonvirtualByteMethodV,
	_Jv_JNI_CallNonvirtualByteMethodA,
	_Jv_JNI_CallNonvirtualCharMethod,
	_Jv_JNI_CallNonvirtualCharMethodV,
	_Jv_JNI_CallNonvirtualCharMethodA,
	_Jv_JNI_CallNonvirtualShortMethod,
	_Jv_JNI_CallNonvirtualShortMethodV,
	_Jv_JNI_CallNonvirtualShortMethodA,
	_Jv_JNI_CallNonvirtualIntMethod,
	_Jv_JNI_CallNonvirtualIntMethodV,
	_Jv_JNI_CallNonvirtualIntMethodA,
	_Jv_JNI_CallNonvirtualLongMethod,
	_Jv_JNI_CallNonvirtualLongMethodV,
	_Jv_JNI_CallNonvirtualLongMethodA,
	_Jv_JNI_CallNonvirtualFloatMethod,
	_Jv_JNI_CallNonvirtualFloatMethodV,
	_Jv_JNI_CallNonvirtualFloatMethodA,
	_Jv_JNI_CallNonvirtualDoubleMethod,
	_Jv_JNI_CallNonvirtualDoubleMethodV,
	_Jv_JNI_CallNonvirtualDoubleMethodA,
	_Jv_JNI_CallNonvirtualVoidMethod,
	_Jv_JNI_CallNonvirtualVoidMethodV,
	_Jv_JNI_CallNonvirtualVoidMethodA,

	_Jv_JNI_GetFieldID,

	_Jv_JNI_GetObjectField,
	_Jv_JNI_GetBooleanField,
	_Jv_JNI_GetByteField,
	_Jv_JNI_GetCharField,
	_Jv_JNI_GetShortField,
	_Jv_JNI_GetIntField,
	_Jv_JNI_GetLongField,
	_Jv_JNI_GetFloatField,
	_Jv_JNI_GetDoubleField,
	_Jv_JNI_SetObjectField,
	_Jv_JNI_SetBooleanField,
	_Jv_JNI_SetByteField,
	_Jv_JNI_SetCharField,
	_Jv_JNI_SetShortField,
	_Jv_JNI_SetIntField,
	_Jv_JNI_SetLongField,
	_Jv_JNI_SetFloatField,
	_Jv_JNI_SetDoubleField,

	_Jv_JNI_GetStaticMethodID,

	_Jv_JNI_CallStaticObjectMethod,
	_Jv_JNI_CallStaticObjectMethodV,
	_Jv_JNI_CallStaticObjectMethodA,
	_Jv_JNI_CallStaticBooleanMethod,
	_Jv_JNI_CallStaticBooleanMethodV,
	_Jv_JNI_CallStaticBooleanMethodA,
	_Jv_JNI_CallStaticByteMethod,
	_Jv_JNI_CallStaticByteMethodV,
	_Jv_JNI_CallStaticByteMethodA,
	_Jv_JNI_CallStaticCharMethod,
	_Jv_JNI_CallStaticCharMethodV,
	_Jv_JNI_CallStaticCharMethodA,
	_Jv_JNI_CallStaticShortMethod,
	_Jv_JNI_CallStaticShortMethodV,
	_Jv_JNI_CallStaticShortMethodA,
	_Jv_JNI_CallStaticIntMethod,
	_Jv_JNI_CallStaticIntMethodV,
	_Jv_JNI_CallStaticIntMethodA,
	_Jv_JNI_CallStaticLongMethod,
	_Jv_JNI_CallStaticLongMethodV,
	_Jv_JNI_CallStaticLongMethodA,
	_Jv_JNI_CallStaticFloatMethod,
	_Jv_JNI_CallStaticFloatMethodV,
	_Jv_JNI_CallStaticFloatMethodA,
	_Jv_JNI_CallStaticDoubleMethod,
	_Jv_JNI_CallStaticDoubleMethodV,
	_Jv_JNI_CallStaticDoubleMethodA,
	_Jv_JNI_CallStaticVoidMethod,
	_Jv_JNI_CallStaticVoidMethodV,
	_Jv_JNI_CallStaticVoidMethodA,

	_Jv_JNI_GetStaticFieldID,

	_Jv_JNI_GetStaticObjectField,
	_Jv_JNI_GetStaticBooleanField,
	_Jv_JNI_GetStaticByteField,
	_Jv_JNI_GetStaticCharField,
	_Jv_JNI_GetStaticShortField,
	_Jv_JNI_GetStaticIntField,
	_Jv_JNI_GetStaticLongField,
	_Jv_JNI_GetStaticFloatField,
	_Jv_JNI_GetStaticDoubleField,
	_Jv_JNI_SetStaticObjectField,
	_Jv_JNI_SetStaticBooleanField,
	_Jv_JNI_SetStaticByteField,
	_Jv_JNI_SetStaticCharField,
	_Jv_JNI_SetStaticShortField,
	_Jv_JNI_SetStaticIntField,
	_Jv_JNI_SetStaticLongField,
	_Jv_JNI_SetStaticFloatField,
	_Jv_JNI_SetStaticDoubleField,

	_Jv_JNI_NewString,
	_Jv_JNI_GetStringLength,
	_Jv_JNI_GetStringChars,
	_Jv_JNI_ReleaseStringChars,

	_Jv_JNI_NewStringUTF,
	_Jv_JNI_GetStringUTFLength,
	_Jv_JNI_GetStringUTFChars,
	_Jv_JNI_ReleaseStringUTFChars,

	_Jv_JNI_GetArrayLength,

	_Jv_JNI_NewObjectArray,
	_Jv_JNI_GetObjectArrayElement,
	_Jv_JNI_SetObjectArrayElement,

	_Jv_JNI_NewBooleanArray,
	_Jv_JNI_NewByteArray,
	_Jv_JNI_NewCharArray,
	_Jv_JNI_NewShortArray,
	_Jv_JNI_NewIntArray,
	_Jv_JNI_NewLongArray,
	_Jv_JNI_NewFloatArray,
	_Jv_JNI_NewDoubleArray,

	_Jv_JNI_GetBooleanArrayElements,
	_Jv_JNI_GetByteArrayElements,
	_Jv_JNI_GetCharArrayElements,
	_Jv_JNI_GetShortArrayElements,
	_Jv_JNI_GetIntArrayElements,
	_Jv_JNI_GetLongArrayElements,
	_Jv_JNI_GetFloatArrayElements,
	_Jv_JNI_GetDoubleArrayElements,

	_Jv_JNI_ReleaseBooleanArrayElements,
	_Jv_JNI_ReleaseByteArrayElements,
	_Jv_JNI_ReleaseCharArrayElements,
	_Jv_JNI_ReleaseShortArrayElements,
	_Jv_JNI_ReleaseIntArrayElements,
	_Jv_JNI_ReleaseLongArrayElements,
	_Jv_JNI_ReleaseFloatArrayElements,
	_Jv_JNI_ReleaseDoubleArrayElements,

	_Jv_JNI_GetBooleanArrayRegion,
	_Jv_JNI_GetByteArrayRegion,
	_Jv_JNI_GetCharArrayRegion,
	_Jv_JNI_GetShortArrayRegion,
	_Jv_JNI_GetIntArrayRegion,
	_Jv_JNI_GetLongArrayRegion,
	_Jv_JNI_GetFloatArrayRegion,
	_Jv_JNI_GetDoubleArrayRegion,
	_Jv_JNI_SetBooleanArrayRegion,
	_Jv_JNI_SetByteArrayRegion,
	_Jv_JNI_SetCharArrayRegion,
	_Jv_JNI_SetShortArrayRegion,
	_Jv_JNI_SetIntArrayRegion,
	_Jv_JNI_SetLongArrayRegion,
	_Jv_JNI_SetFloatArrayRegion,
	_Jv_JNI_SetDoubleArrayRegion,

	_Jv_JNI_RegisterNatives,
	_Jv_JNI_UnregisterNatives,

	_Jv_JNI_MonitorEnter,
	_Jv_JNI_MonitorExit,

	_Jv_JNI_GetJavaVM,

	/* new JNI 1.2 functions */

	_Jv_JNI_GetStringRegion,
	_Jv_JNI_GetStringUTFRegion,

	_Jv_JNI_GetPrimitiveArrayCritical,
	_Jv_JNI_ReleasePrimitiveArrayCritical,

	_Jv_JNI_GetStringCritical,
	_Jv_JNI_ReleaseStringCritical,

	_Jv_JNI_NewWeakGlobalRef,
	_Jv_JNI_DeleteWeakGlobalRef,

	_Jv_JNI_ExceptionCheck,

	/* new JNI 1.4 functions */

	_Jv_JNI_NewDirectByteBuffer,
	_Jv_JNI_GetDirectBufferAddress,
	_Jv_JNI_GetDirectBufferCapacity
};


/* Invocation API Functions ***************************************************/

/* JNI_GetDefaultJavaVMInitArgs ************************************************

   Returns a default configuration for the Java VM.

*******************************************************************************/

jint JNI_GetDefaultJavaVMInitArgs(void *vm_args)
{
	JavaVMInitArgs *_vm_args;

	_vm_args = (JavaVMInitArgs *) vm_args;

	/* GNU classpath currently supports JNI 1.2 */

	switch (_vm_args->version) {
    case JNI_VERSION_1_1:
		_vm_args->version = JNI_VERSION_1_1;
		break;

    case JNI_VERSION_1_2:
    case JNI_VERSION_1_4:
		_vm_args->ignoreUnrecognized = JNI_FALSE;
		_vm_args->options = NULL;
		_vm_args->nOptions = 0;
		break;

    default:
		return -1;
	}
  
	return 0;
}


/* JNI_GetCreatedJavaVMs *******************************************************

   Returns all Java VMs that have been created. Pointers to VMs are written in
   the buffer vmBuf in the order they are created. At most bufLen number of
   entries will be written. The total number of created VMs is returned in
   *nVMs.

*******************************************************************************/

jint JNI_GetCreatedJavaVMs(JavaVM **vmBuf, jsize bufLen, jsize *nVMs)
{
	log_text("JNI_GetCreatedJavaVMs: IMPLEMENT ME!!!");

	return 0;
}


/* JNI_CreateJavaVM ************************************************************

   Loads and initializes a Java VM. The current thread becomes the main thread.
   Sets the env argument to the JNI interface pointer of the main thread.

*******************************************************************************/

jint JNI_CreateJavaVM(JavaVM **p_vm, void **p_env, void *vm_args)
{
	/* actually create the JVM */

	if (!vm_createjvm(p_vm, p_env, vm_args))
		return JNI_ERR;

	return JNI_OK;
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

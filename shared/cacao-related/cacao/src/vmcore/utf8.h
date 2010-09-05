/* src/vmcore/utf8.h - utf8 string functions

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

   $Id: utf8.h 7387 2007-02-21 23:26:24Z twisti $

*/


#ifndef _UTF_H
#define _UTF_H

/* forward typedefs ***********************************************************/

typedef struct utf utf;

#include "config.h"

#include <stdio.h>

#include "vm/types.h"

#include "vm/global.h"


/* data structure for utf8 symbols ********************************************/

struct utf {
	utf  *hashlink;                     /* link for external hash chain       */
	s4    blength;                      /* text length in bytes               */
	char *text;                         /* pointer to text                    */
};

/* to determine the end of utf strings */

#define UTF_END(u)    ((char *) u->text + u->blength)


/* utf-symbols for pointer comparison of frequently used strings **************/

extern utf *utf_java_lang_Object;

extern utf *utf_java_lang_Class;
extern utf *utf_java_lang_ClassLoader;
extern utf *utf_java_lang_Cloneable;
extern utf *utf_java_lang_SecurityManager;
extern utf *utf_java_lang_String;
extern utf *utf_java_lang_System;
extern utf *utf_java_lang_ThreadGroup;
extern utf *utf_java_lang_ref_SoftReference;
extern utf *utf_java_lang_ref_WeakReference;
extern utf *utf_java_lang_ref_PhantomReference;
extern utf *utf_java_io_Serializable;

extern utf *utf_java_lang_Throwable;
extern utf *utf_java_lang_Error;

extern utf *utf_java_lang_AbstractMethodError;
extern utf *utf_java_lang_ClassCircularityError;
extern utf *utf_java_lang_ClassFormatError;
extern utf *utf_java_lang_ExceptionInInitializerError;
extern utf *utf_java_lang_IncompatibleClassChangeError;
extern utf *utf_java_lang_InstantiationError;
extern utf *utf_java_lang_InternalError;
extern utf *utf_java_lang_LinkageError;
extern utf *utf_java_lang_NoClassDefFoundError;
extern utf *utf_java_lang_NoSuchFieldError;
extern utf *utf_java_lang_NoSuchMethodError;
extern utf *utf_java_lang_OutOfMemoryError;
extern utf *utf_java_lang_UnsatisfiedLinkError;
extern utf *utf_java_lang_UnsupportedClassVersionError;
extern utf *utf_java_lang_VerifyError;
extern utf *utf_java_lang_VirtualMachineError;

#if defined(WITH_CLASSPATH_GNU)
extern utf *utf_java_lang_VMThrowable;
#endif

extern utf *utf_java_lang_Exception;

extern utf *utf_java_lang_ArithmeticException;
extern utf *utf_java_lang_ArrayIndexOutOfBoundsException;
extern utf *utf_java_lang_ArrayStoreException;
extern utf *utf_java_lang_ClassCastException;
extern utf *utf_java_lang_ClassNotFoundException;
extern utf *utf_java_lang_CloneNotSupportedException;
extern utf *utf_java_lang_IllegalAccessException;
extern utf *utf_java_lang_IllegalArgumentException;
extern utf *utf_java_lang_IllegalMonitorStateException;
extern utf *utf_java_lang_InstantiationException;
extern utf *utf_java_lang_InterruptedException;
extern utf *utf_java_lang_NegativeArraySizeException;
extern utf *utf_java_lang_NullPointerException;
extern utf *utf_java_lang_StringIndexOutOfBoundsException;

extern utf *utf_java_lang_reflect_InvocationTargetException;

#if defined(ENABLE_JAVASE)
extern utf* utf_java_lang_Void;
#endif

extern utf* utf_java_lang_Boolean;
extern utf* utf_java_lang_Byte;
extern utf* utf_java_lang_Character;
extern utf* utf_java_lang_Short;
extern utf* utf_java_lang_Integer;
extern utf* utf_java_lang_Long;
extern utf* utf_java_lang_Float;
extern utf* utf_java_lang_Double;

#if defined(ENABLE_JAVASE)
extern utf *utf_java_lang_StackTraceElement;
extern utf *utf_java_lang_reflect_Constructor;
extern utf *utf_java_lang_reflect_Field;
extern utf *utf_java_lang_reflect_Method;
extern utf *utf_java_util_Vector;
#endif

extern utf *utf_InnerClasses;
extern utf *utf_ConstantValue;
extern utf *utf_Code;
extern utf *utf_Exceptions;
extern utf *utf_LineNumberTable;
extern utf *utf_SourceFile;

#if defined(ENABLE_JAVASE)
extern utf *utf_EnclosingMethod;
extern utf *utf_Signature;
extern utf *utf_RuntimeVisibleAnnotations;
extern utf *utf_StackMapTable;
#endif

extern utf *utf_init;
extern utf *utf_clinit;
extern utf *utf_clone;
extern utf *utf_finalize;
extern utf *utf_run;

extern utf *utf_add;
extern utf *utf_remove;
extern utf *utf_addThread;
extern utf *utf_removeThread;
extern utf *utf_put;
extern utf *utf_get;
extern utf *utf_value;

extern utf *utf_fillInStackTrace;
extern utf *utf_getSystemClassLoader;
extern utf *utf_loadClass;
extern utf *utf_printStackTrace;

extern utf *utf_Z;
extern utf *utf_B;
extern utf *utf_C;
extern utf *utf_S;
extern utf *utf_I;
extern utf *utf_J;
extern utf *utf_F;
extern utf *utf_D;

extern utf *utf_void__void;
extern utf *utf_boolean__void;
extern utf *utf_byte__void;
extern utf *utf_char__void;
extern utf *utf_short__void;
extern utf *utf_int__void;
extern utf *utf_long__void;
extern utf *utf_float__void;
extern utf *utf_double__void;

extern utf *utf_void__java_lang_ClassLoader;
extern utf *utf_void__java_lang_Object;
extern utf *utf_void__java_lang_Throwable;
extern utf *utf_java_lang_Object__java_lang_Object;
extern utf *utf_java_lang_String__void;
extern utf *utf_java_lang_String__java_lang_Class;
extern utf *utf_java_lang_Thread__V;
extern utf *utf_java_lang_Throwable__void;

extern utf *utf_not_named_yet;
extern utf *utf_null;
extern utf *array_packagename;


/* function prototypes ********************************************************/

/* initialize the utf8 subsystem */
bool utf8_init(void);

u4 utf_hashkey(const char *text, u4 length);
u4 utf_full_hashkey(const char *text, u4 length);

/* determine hashkey of a unicode-symbol */
u4 unicode_hashkey(u2 *text, u2 length);

/* create new utf-symbol */
utf *utf_new(const char *text, u2 length);

/* make utf symbol from u2 array */
utf *utf_new_u2(u2 *unicodedata, u4 unicodelength, bool isclassname);

utf *utf_new_char(const char *text);
utf *utf_new_char_classname(const char *text);

/* get number of bytes */
u4 utf_bytes(utf *u);

/* get next unicode character of a utf-string */
u2 utf_nextu2(char **utf);

/* get (number of) unicode characters of a utf string (safe) */
s4 utf8_safe_number_of_u2s(const char *text, s4 nbytes);
void utf8_safe_convert_to_u2s(const char *text, s4 nbytes, u2 *buffer);

/* get (number of) unicode characters of a utf string (UNSAFE!) */
u4 utf_get_number_of_u2s(utf *u);
u4 utf_get_number_of_u2s_for_buffer(const char *buffer, u4 blength);

/* determine utf length in bytes of a u2 array */
u4 u2_utflength(u2 *text, u4 u2_length);

void utf_copy(char *buffer, utf *u);
void utf_cat(char *buffer, utf *u);
void utf_copy_classname(char *buffer, utf *u);
void utf_cat_classname(char *buffer, utf *u);

/* write utf symbol to file/buffer */
void utf_display_printable_ascii(utf *u);
void utf_display_printable_ascii_classname(utf *u);

void utf_sprint_convert_to_latin1(char *buffer, utf *u);
void utf_sprint_convert_to_latin1_classname(char *buffer, utf *u);

void utf_strcat_convert_to_latin1(char *buffer, utf *u);
void utf_strcat_convert_to_latin1_classname(char *buffer, utf *u);

void utf_fprint_printable_ascii(FILE *file, utf *u);
void utf_fprint_printable_ascii_classname(FILE *file, utf *u);

/* check if a UTF-8 string is valid */
bool is_valid_utf(char *utf_ptr, char *end_pos);

/* check if a UTF-8 string may be used as a class/field/method name */
bool is_valid_name(char *utf_ptr, char *end_pos);
bool is_valid_name_utf(utf *u);

/* show utf-table */
void utf_show(void);

#endif /* _UTF_H */


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

/* src/vmcore/utf8.c - utf8 string functions

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

   $Id: utf8.c 7813 2007-04-25 19:20:13Z twisti $

*/


#include "config.h"

#include <string.h>
#include <assert.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "threads/lock-common.h"

#include "toolbox/hashtable.h"

#include "vm/exceptions.h"

#include "vmcore/options.h"

#if defined(ENABLE_STATISTICS)
# include "vmcore/statistics.h"
#endif

#include "vmcore/utf8.h"


/* global variables ***********************************************************/

/* hashsize must be power of 2 */

#define HASHTABLE_UTF_SIZE    16384     /* initial size of utf-hash           */

hashtable *hashtable_utf;               /* hashtable for utf8-symbols         */


/* utf-symbols for pointer comparison of frequently used strings **************/

utf *utf_java_lang_Object;

utf *utf_java_lang_Class;
utf *utf_java_lang_ClassLoader;
utf *utf_java_lang_Cloneable;
utf *utf_java_lang_SecurityManager;
utf *utf_java_lang_String;
utf *utf_java_lang_System;
utf *utf_java_lang_ThreadGroup;
utf *utf_java_lang_ref_SoftReference;
utf *utf_java_lang_ref_WeakReference;
utf *utf_java_lang_ref_PhantomReference;
utf *utf_java_io_Serializable;

utf *utf_java_lang_Throwable;
utf *utf_java_lang_Error;

utf *utf_java_lang_AbstractMethodError;
utf *utf_java_lang_ClassCircularityError;
utf *utf_java_lang_ClassFormatError;
utf *utf_java_lang_ExceptionInInitializerError;
utf *utf_java_lang_IncompatibleClassChangeError;
utf *utf_java_lang_InstantiationError;
utf *utf_java_lang_InternalError;
utf *utf_java_lang_LinkageError;
utf *utf_java_lang_NoClassDefFoundError;
utf *utf_java_lang_NoSuchFieldError;
utf *utf_java_lang_NoSuchMethodError;
utf *utf_java_lang_OutOfMemoryError;
utf *utf_java_lang_UnsatisfiedLinkError;
utf *utf_java_lang_UnsupportedClassVersionError;
utf *utf_java_lang_VerifyError;
utf *utf_java_lang_VirtualMachineError;

#if defined(WITH_CLASSPATH_GNU)
utf *utf_java_lang_VMThrowable;
#endif

utf *utf_java_lang_Exception;

utf *utf_java_lang_ArithmeticException;
utf *utf_java_lang_ArrayIndexOutOfBoundsException;
utf *utf_java_lang_ArrayStoreException;
utf *utf_java_lang_ClassCastException;
utf *utf_java_lang_ClassNotFoundException;
utf *utf_java_lang_CloneNotSupportedException;
utf *utf_java_lang_IllegalAccessException;
utf *utf_java_lang_IllegalArgumentException;
utf *utf_java_lang_IllegalMonitorStateException;
utf *utf_java_lang_InstantiationException;
utf *utf_java_lang_InterruptedException;
utf *utf_java_lang_NegativeArraySizeException;
utf *utf_java_lang_NullPointerException;
utf *utf_java_lang_StringIndexOutOfBoundsException;

utf *utf_java_lang_reflect_InvocationTargetException;

#if defined(ENABLE_JAVASE)
utf* utf_java_lang_Void;
#endif

utf* utf_java_lang_Boolean;
utf* utf_java_lang_Byte;
utf* utf_java_lang_Character;
utf* utf_java_lang_Short;
utf* utf_java_lang_Integer;
utf* utf_java_lang_Long;
utf* utf_java_lang_Float;
utf* utf_java_lang_Double;

#if defined(ENABLE_JAVASE)
utf *utf_java_lang_StackTraceElement;
utf *utf_java_lang_reflect_Constructor;
utf *utf_java_lang_reflect_Field;
utf *utf_java_lang_reflect_Method;
utf *utf_java_util_Vector;
#endif

utf *utf_InnerClasses;                  /* InnerClasses                       */
utf *utf_ConstantValue;                 /* ConstantValue                      */
utf *utf_Code;                          /* Code                               */
utf *utf_Exceptions;                    /* Exceptions                         */
utf *utf_LineNumberTable;               /* LineNumberTable                    */
utf *utf_SourceFile;                    /* SourceFile                         */

#if defined(ENABLE_JAVASE)
utf *utf_EnclosingMethod;
utf *utf_Signature;
utf *utf_RuntimeVisibleAnnotations;
utf *utf_StackMapTable;
#endif

utf *utf_init;                          /* <init>                             */
utf *utf_clinit;                        /* <clinit>                           */
utf *utf_clone;                         /* clone                              */
utf *utf_finalize;                      /* finalize                           */
utf *utf_run;                           /* run                                */

utf *utf_add;
utf *utf_remove;
utf *utf_addThread;
utf *utf_removeThread;
utf *utf_put;
utf *utf_get;
utf *utf_value;

utf *utf_fillInStackTrace;
utf *utf_getSystemClassLoader;
utf *utf_loadClass;
utf *utf_printStackTrace;

utf *utf_Z;                             /* Z                                  */
utf *utf_B;                             /* B                                  */
utf *utf_C;                             /* C                                  */
utf *utf_S;                             /* S                                  */
utf *utf_I;                             /* I                                  */
utf *utf_J;                             /* J                                  */
utf *utf_F;                             /* F                                  */
utf *utf_D;                             /* D                                  */

utf *utf_void__void;                    /* ()V                                */
utf *utf_boolean__void;                 /* (Z)V                               */
utf *utf_byte__void;                    /* (B)V                               */
utf *utf_char__void;                    /* (C)V                               */
utf *utf_short__void;                   /* (S)V                               */
utf *utf_int__void;                     /* (I)V                               */
utf *utf_long__void;                    /* (J)V                               */
utf *utf_float__void;                   /* (F)V                               */
utf *utf_double__void;                  /* (D)V                               */

utf *utf_void__java_lang_ClassLoader;   /* ()Ljava/lang/ClassLoader;          */
utf *utf_void__java_lang_Object;        /* ()Ljava/lang/Object;               */
utf *utf_void__java_lang_Throwable;     /* ()Ljava/lang/Throwable;            */
utf *utf_java_lang_Object__java_lang_Object;
utf *utf_java_lang_String__void;        /* (Ljava/lang/String;)V              */
utf *utf_java_lang_String__java_lang_Class;
utf *utf_java_lang_Thread__V;           /* (Ljava/lang/Thread;)V              */
utf *utf_java_lang_Throwable__void;     /* (Ljava/lang/Throwable;)V           */

utf *utf_not_named_yet;                 /* special name for unnamed classes   */
utf *utf_null;
utf *array_packagename;


/* utf_init ********************************************************************

   Initializes the utf8 subsystem.

*******************************************************************************/

bool utf8_init(void)
{
	/* create utf8 hashtable */

	hashtable_utf = NEW(hashtable);

	hashtable_create(hashtable_utf, HASHTABLE_UTF_SIZE);

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		count_utf_len += sizeof(utf*) * hashtable_utf->size;
#endif

	/* create utf-symbols for pointer comparison of frequently used strings */

	utf_java_lang_Object           = utf_new_char("java/lang/Object");

	utf_java_lang_Class            = utf_new_char("java/lang/Class");
	utf_java_lang_ClassLoader      = utf_new_char("java/lang/ClassLoader");
	utf_java_lang_Cloneable        = utf_new_char("java/lang/Cloneable");
	utf_java_lang_SecurityManager  = utf_new_char("java/lang/SecurityManager");
	utf_java_lang_String           = utf_new_char("java/lang/String");
	utf_java_lang_System           = utf_new_char("java/lang/System");
	utf_java_lang_ThreadGroup      = utf_new_char("java/lang/ThreadGroup");

	utf_java_lang_ref_SoftReference =
		utf_new_char("java/lang/ref/SoftReference");

	utf_java_lang_ref_WeakReference =
		utf_new_char("java/lang/ref/WeakReference");

	utf_java_lang_ref_PhantomReference =
		utf_new_char("java/lang/ref/PhantomReference");

	utf_java_io_Serializable       = utf_new_char("java/io/Serializable");

	utf_java_lang_Throwable        = utf_new_char("java/lang/Throwable");
	utf_java_lang_Error            = utf_new_char("java/lang/Error");

	utf_java_lang_ClassCircularityError =
		utf_new_char("java/lang/ClassCircularityError");

	utf_java_lang_ClassFormatError = utf_new_char("java/lang/ClassFormatError");

	utf_java_lang_ExceptionInInitializerError =
		utf_new_char("java/lang/ExceptionInInitializerError");

	utf_java_lang_IncompatibleClassChangeError =
		utf_new_char("java/lang/IncompatibleClassChangeError");

	utf_java_lang_InstantiationError =
		utf_new_char("java/lang/InstantiationError");

	utf_java_lang_InternalError    = utf_new_char("java/lang/InternalError");
	utf_java_lang_LinkageError     = utf_new_char("java/lang/LinkageError");

	utf_java_lang_NoClassDefFoundError =
		utf_new_char("java/lang/NoClassDefFoundError");

	utf_java_lang_OutOfMemoryError = utf_new_char("java/lang/OutOfMemoryError");

	utf_java_lang_UnsatisfiedLinkError =
		utf_new_char("java/lang/UnsatisfiedLinkError");

	utf_java_lang_UnsupportedClassVersionError =
		utf_new_char("java/lang/UnsupportedClassVersionError");

	utf_java_lang_VerifyError      = utf_new_char("java/lang/VerifyError");

	utf_java_lang_VirtualMachineError =
		utf_new_char("java/lang/VirtualMachineError");

#if defined(ENABLE_JAVASE)
	utf_java_lang_AbstractMethodError =
		utf_new_char("java/lang/AbstractMethodError");

	utf_java_lang_NoSuchFieldError =
		utf_new_char("java/lang/NoSuchFieldError");

	utf_java_lang_NoSuchMethodError =
		utf_new_char("java/lang/NoSuchMethodError");
#endif

#if defined(WITH_CLASSPATH_GNU)
	utf_java_lang_VMThrowable      = utf_new_char("java/lang/VMThrowable");
#endif

	utf_java_lang_Exception        = utf_new_char("java/lang/Exception");

	utf_java_lang_ArithmeticException =
		utf_new_char("java/lang/ArithmeticException");

	utf_java_lang_ArrayIndexOutOfBoundsException =
		utf_new_char("java/lang/ArrayIndexOutOfBoundsException");

	utf_java_lang_ArrayStoreException =
		utf_new_char("java/lang/ArrayStoreException");

	utf_java_lang_ClassCastException =
		utf_new_char("java/lang/ClassCastException");

	utf_java_lang_ClassNotFoundException =
		utf_new_char("java/lang/ClassNotFoundException");

	utf_java_lang_CloneNotSupportedException =
		utf_new_char("java/lang/CloneNotSupportedException");

	utf_java_lang_IllegalAccessException =
		utf_new_char("java/lang/IllegalAccessException");

	utf_java_lang_IllegalArgumentException =
		utf_new_char("java/lang/IllegalArgumentException");

	utf_java_lang_IllegalMonitorStateException =
		utf_new_char("java/lang/IllegalMonitorStateException");

	utf_java_lang_InstantiationException =
		utf_new_char("java/lang/InstantiationException");

	utf_java_lang_InterruptedException =
		utf_new_char("java/lang/InterruptedException");

	utf_java_lang_NegativeArraySizeException =
		utf_new_char("java/lang/NegativeArraySizeException");

	utf_java_lang_NullPointerException =
		utf_new_char("java/lang/NullPointerException");

	utf_java_lang_StringIndexOutOfBoundsException =
		utf_new_char("java/lang/StringIndexOutOfBoundsException");

	utf_java_lang_reflect_InvocationTargetException =
		utf_new_char("java/lang/reflect/InvocationTargetException");
 
#if defined(ENABLE_JAVASE)
	utf_java_lang_Void             = utf_new_char("java/lang/Void");
#endif

	utf_java_lang_Boolean          = utf_new_char("java/lang/Boolean");
	utf_java_lang_Byte             = utf_new_char("java/lang/Byte");
	utf_java_lang_Character        = utf_new_char("java/lang/Character");
	utf_java_lang_Short            = utf_new_char("java/lang/Short");
	utf_java_lang_Integer          = utf_new_char("java/lang/Integer");
	utf_java_lang_Long             = utf_new_char("java/lang/Long");
	utf_java_lang_Float            = utf_new_char("java/lang/Float");
	utf_java_lang_Double           = utf_new_char("java/lang/Double");

#if defined(ENABLE_JAVASE)
	utf_java_lang_StackTraceElement =
		utf_new_char("java/lang/StackTraceElement");

	utf_java_lang_reflect_Constructor =
		utf_new_char("java/lang/reflect/Constructor");

	utf_java_lang_reflect_Field    = utf_new_char("java/lang/reflect/Field");
	utf_java_lang_reflect_Method   = utf_new_char("java/lang/reflect/Method");
	utf_java_util_Vector           = utf_new_char("java/util/Vector");
#endif

	utf_InnerClasses               = utf_new_char("InnerClasses");
	utf_ConstantValue              = utf_new_char("ConstantValue");
	utf_Code                       = utf_new_char("Code");
	utf_Exceptions	               = utf_new_char("Exceptions");
	utf_LineNumberTable            = utf_new_char("LineNumberTable");
	utf_SourceFile                 = utf_new_char("SourceFile");

#if defined(ENABLE_JAVASE)
	utf_EnclosingMethod            = utf_new_char("EnclosingMethod");
	utf_Signature                  = utf_new_char("Signature");
	utf_RuntimeVisibleAnnotations  = utf_new_char("RuntimeVisibleAnnotations");
	utf_StackMapTable              = utf_new_char("StackMapTable");
#endif

	utf_init	                   = utf_new_char("<init>");
	utf_clinit	                   = utf_new_char("<clinit>");
	utf_clone                      = utf_new_char("clone");
	utf_finalize	               = utf_new_char("finalize");
	utf_run                        = utf_new_char("run");

	utf_add                        = utf_new_char("add");
	utf_remove                     = utf_new_char("remove");
	utf_addThread                  = utf_new_char("addThread");
	utf_removeThread               = utf_new_char("removeThread");
	utf_put                        = utf_new_char("put");
	utf_get                        = utf_new_char("get");
	utf_value                      = utf_new_char("value");

	utf_printStackTrace            = utf_new_char("printStackTrace");
	utf_fillInStackTrace           = utf_new_char("fillInStackTrace");
	utf_loadClass                  = utf_new_char("loadClass");
	utf_getSystemClassLoader       = utf_new_char("getSystemClassLoader");

	utf_Z                          = utf_new_char("Z");
	utf_B                          = utf_new_char("B");
	utf_C                          = utf_new_char("C");
	utf_S                          = utf_new_char("S");
	utf_I                          = utf_new_char("I");
	utf_J                          = utf_new_char("J");
	utf_F                          = utf_new_char("F");
	utf_D                          = utf_new_char("D");

	utf_void__void                 = utf_new_char("()V");
	utf_boolean__void              = utf_new_char("(Z)V");
	utf_byte__void                 = utf_new_char("(B)V");
	utf_char__void                 = utf_new_char("(C)V");
	utf_short__void                = utf_new_char("(S)V");
	utf_int__void                  = utf_new_char("(I)V");
	utf_long__void                 = utf_new_char("(J)V");
	utf_float__void                = utf_new_char("(F)V");
	utf_double__void               = utf_new_char("(D)V");
	utf_void__java_lang_Object     = utf_new_char("()Ljava/lang/Object;");
	utf_void__java_lang_Throwable  = utf_new_char("()Ljava/lang/Throwable;");

	utf_void__java_lang_ClassLoader =
		utf_new_char("()Ljava/lang/ClassLoader;");

	utf_java_lang_Object__java_lang_Object =
		utf_new_char("(Ljava/lang/Object;)Ljava/lang/Object;");

	utf_java_lang_String__void     = utf_new_char("(Ljava/lang/String;)V");

	utf_java_lang_String__java_lang_Class =
		utf_new_char("(Ljava/lang/String;)Ljava/lang/Class;");

	utf_java_lang_Thread__V        = utf_new_char("(Ljava/lang/Thread;)V");
	utf_java_lang_Throwable__void  = utf_new_char("(Ljava/lang/Throwable;)V");

	utf_null                       = utf_new_char("null");
	utf_not_named_yet              = utf_new_char("\t<not_named_yet>");
	array_packagename              = utf_new_char("\t<the array package>");

	/* everything's ok */

	return true;
}


/* utf_hashkey *****************************************************************

   The hashkey is computed from the utf-text by using up to 8
   characters.  For utf-symbols longer than 15 characters 3 characters
   are taken from the beginning and the end, 2 characters are taken
   from the middle.

*******************************************************************************/

#define nbs(val) ((u4) *(++text) << val) /* get next byte, left shift by val  */
#define fbs(val) ((u4) *(  text) << val) /* get first byte, left shift by val */

u4 utf_hashkey(const char *text, u4 length)
{
	const char *start_pos = text;       /* pointer to utf text                */
	u4 a;

	switch (length) {
	case 0: /* empty string */
		return 0;

	case 1: return fbs(0);
	case 2: return fbs(0) ^ nbs(3);
	case 3: return fbs(0) ^ nbs(3) ^ nbs(5);
	case 4: return fbs(0) ^ nbs(2) ^ nbs(4) ^ nbs(6);
	case 5: return fbs(0) ^ nbs(2) ^ nbs(3) ^ nbs(4) ^ nbs(6);
	case 6: return fbs(0) ^ nbs(1) ^ nbs(2) ^ nbs(3) ^ nbs(5) ^ nbs(6);
	case 7: return fbs(0) ^ nbs(1) ^ nbs(2) ^ nbs(3) ^ nbs(4) ^ nbs(5) ^ nbs(6);
	case 8: return fbs(0) ^ nbs(1) ^ nbs(2) ^ nbs(3) ^ nbs(4) ^ nbs(5) ^ nbs(6) ^ nbs(7);

	case 9:
		a = fbs(0);
		a ^= nbs(1);
		a ^= nbs(2);
		text++;
		return a ^ nbs(4) ^ nbs(5) ^ nbs(6) ^ nbs(7) ^ nbs(8);

	case 10:
		a = fbs(0);
		text++;
		a ^= nbs(2);
		a ^= nbs(3);
		a ^= nbs(4);
		text++;
		return a ^ nbs(6) ^ nbs(7) ^ nbs(8) ^ nbs(9);

	case 11:
		a = fbs(0);
		text++;
		a ^= nbs(2);
		a ^= nbs(3);
		a ^= nbs(4);
		text++;
		return a ^ nbs(6) ^ nbs(7) ^ nbs(8) ^ nbs(9) ^ nbs(10);

	case 12:
		a = fbs(0);
		text += 2;
		a ^= nbs(2);
		a ^= nbs(3);
		text++;
		a ^= nbs(5);
		a ^= nbs(6);
		a ^= nbs(7);
		text++;
		return a ^ nbs(9) ^ nbs(10);

	case 13:
		a = fbs(0);
		a ^= nbs(1);
		text++;
		a ^= nbs(3);
		a ^= nbs(4);
		text += 2;	
		a ^= nbs(7);
		a ^= nbs(8);
		text += 2;
		return a ^ nbs(9) ^ nbs(10);

	case 14:
		a = fbs(0);
		text += 2;	
		a ^= nbs(3);
		a ^= nbs(4);
		text += 2;	
		a ^= nbs(7);
		a ^= nbs(8);
		text += 2;
		return a ^ nbs(9) ^ nbs(10) ^ nbs(11);

	case 15:
		a = fbs(0);
		text += 2;	
		a ^= nbs(3);
		a ^= nbs(4);
		text += 2;	
		a ^= nbs(7);
		a ^= nbs(8);
		text += 2;
		return a ^ nbs(9) ^ nbs(10) ^ nbs(11);

	default:  /* 3 characters from beginning */
		a = fbs(0);
		text += 2;
		a ^= nbs(3);
		a ^= nbs(4);

		/* 2 characters from middle */
		text = start_pos + (length / 2);
		a ^= fbs(5);
		text += 2;
		a ^= nbs(6);	

		/* 3 characters from end */
		text = start_pos + length - 4;

		a ^= fbs(7);
		text++;

		return a ^ nbs(10) ^ nbs(11);
    }
}

/* utf_full_hashkey ************************************************************

   This function computes a hash value using all bytes in the string.

   The algorithm is the "One-at-a-time" algorithm as published
   by Bob Jenkins on http://burtleburtle.net/bob/hash/doobs.html.

*******************************************************************************/

u4 utf_full_hashkey(const char *text, u4 length)
{
	register const unsigned char *p = (const unsigned char *) text;
	register u4 hash;
	register u4 i;

	hash = 0;
	for (i=length; i--;)
	{
	    hash += *p++;
	    hash += (hash << 10);
	    hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

/* unicode_hashkey *************************************************************

   Compute the hashkey of a unicode string.

*******************************************************************************/

u4 unicode_hashkey(u2 *text, u2 len)
{
	return utf_hashkey((char *) text, len);
}


/* utf_new *********************************************************************

   Creates a new utf-symbol, the text of the symbol is passed as a
   u1-array. The function searches the utf-hashtable for a utf-symbol
   with this text. On success the element returned, otherwise a new
   hashtable element is created.

   If the number of entries in the hashtable exceeds twice the size of
   the hashtable slots a reorganization of the hashtable is done and
   the utf symbols are copied to a new hashtable with doubled size.

*******************************************************************************/

utf *utf_new(const char *text, u2 length)
{
	u4 key;                             /* hashkey computed from utf-text     */
	u4 slot;                            /* slot in hashtable                  */
	utf *u;                             /* hashtable element                  */
	u2 i;

	LOCK_MONITOR_ENTER(hashtable_utf->header);

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		count_utf_new++;
#endif

	key  = utf_hashkey(text, length);
	slot = key & (hashtable_utf->size - 1);
	u    = hashtable_utf->ptr[slot];

	/* search external hash chain for utf-symbol */

	while (u) {
		if (u->blength == length) {
			/* compare text of hashtable elements */

			for (i = 0; i < length; i++)
				if (text[i] != u->text[i])
					goto nomatch;
			
#if defined(ENABLE_STATISTICS)
			if (opt_stat)
				count_utf_new_found++;
#endif

			/* symbol found in hashtable */

			LOCK_MONITOR_EXIT(hashtable_utf->header);

			return u;
		}

	nomatch:
		u = u->hashlink; /* next element in external chain */
	}

	/* location in hashtable found, create new utf element */

	u = NEW(utf);

	u->blength  = length;               /* length in bytes of utfstring       */
	u->hashlink = hashtable_utf->ptr[slot]; /* link in external hashchain     */
	u->text     = mem_alloc(length + 1);/* allocate memory for utf-text       */

	memcpy(u->text, text, length);      /* copy utf-text                      */
	u->text[length] = '\0';

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		count_utf_len += sizeof(utf) + length + 1;
#endif

	hashtable_utf->ptr[slot] = u;       /* insert symbol into table           */
	hashtable_utf->entries++;           /* update number of entries           */

	if (hashtable_utf->entries > (hashtable_utf->size * 2)) {

        /* reorganization of hashtable, average length of the external
           chains is approx. 2 */

		hashtable *newhash;                              /* the new hashtable */
		u4         i;
		utf       *u;
		utf       *nextu;
		u4         slot;

		/* create new hashtable, double the size */

		newhash = hashtable_resize(hashtable_utf, hashtable_utf->size * 2);

#if defined(ENABLE_STATISTICS)
		if (opt_stat)
			count_utf_len += sizeof(utf*) * hashtable_utf->size;
#endif

		/* transfer elements to new hashtable */

		for (i = 0; i < hashtable_utf->size; i++) {
			u = hashtable_utf->ptr[i];

			while (u) {
				nextu = u->hashlink;
				slot  = utf_hashkey(u->text, u->blength) & (newhash->size - 1);
						
				u->hashlink = (utf *) newhash->ptr[slot];
				newhash->ptr[slot] = u;

				/* follow link in external hash chain */

				u = nextu;
			}
		}
	
		/* dispose old table */

		hashtable_free(hashtable_utf);

		hashtable_utf = newhash;
	}

	LOCK_MONITOR_EXIT(hashtable_utf->header);

	return u;
}


/* utf_new_u2 ******************************************************************

   Make utf symbol from u2 array, if isclassname is true '.' is
   replaced by '/'.

*******************************************************************************/

utf *utf_new_u2(u2 *unicode_pos, u4 unicode_length, bool isclassname)
{
	char *buffer;                   /* memory buffer for  unicode characters  */
	char *pos;                      /* pointer to current position in buffer  */
	u4 left;                        /* unicode characters left                */
	u4 buflength;                   /* utf length in bytes of the u2 array    */
	utf *result;                    /* resulting utf-string                   */
	int i;    	

	/* determine utf length in bytes and allocate memory */

	buflength = u2_utflength(unicode_pos, unicode_length); 
	buffer    = MNEW(char, buflength);
 
	left = buflength;
	pos  = buffer;

	for (i = 0; i++ < unicode_length; unicode_pos++) {
		/* next unicode character */
		u2 c = *unicode_pos;
		
		if ((c != 0) && (c < 0x80)) {
			/* 1 character */	
			left--;
	    	if ((int) left < 0) break;
			/* convert classname */
			if (isclassname && c == '.')
				*pos++ = '/';
			else
				*pos++ = (char) c;

		} else if (c < 0x800) { 	    
			/* 2 characters */				
	    	unsigned char high = c >> 6;
	    	unsigned char low  = c & 0x3F;
			left = left - 2;
	    	if ((int) left < 0) break;
	    	*pos++ = high | 0xC0; 
	    	*pos++ = low  | 0x80;	  

		} else {	 
	    	/* 3 characters */				
	    	char low  = c & 0x3f;
	    	char mid  = (c >> 6) & 0x3F;
	    	char high = c >> 12;
			left = left - 3;
	    	if ((int) left < 0) break;
	    	*pos++ = high | 0xE0; 
	    	*pos++ = mid  | 0x80;  
	    	*pos++ = low  | 0x80;   
		}
	}
	
	/* insert utf-string into symbol-table */
	result = utf_new(buffer,buflength);

	MFREE(buffer, char, buflength);

	return result;
}


/* utf_new_char ****************************************************************

   Creates a new utf symbol, the text for this symbol is passed as a
   c-string ( = char* ).

*******************************************************************************/

utf *utf_new_char(const char *text)
{
	return utf_new(text, strlen(text));
}


/* utf_new_char_classname ******************************************************

   Creates a new utf symbol, the text for this symbol is passed as a
   c-string ( = char* ) "." characters are going to be replaced by
   "/". Since the above function is used often, this is a separte
   function, instead of an if.

*******************************************************************************/

utf *utf_new_char_classname(const char *text)
{
	if (strchr(text, '.')) {
		char *txt = strdup(text);
		char *end = txt + strlen(txt);
		char *c;
		utf *tmpRes;

		for (c = txt; c < end; c++)
			if (*c == '.') *c = '/';

		tmpRes = utf_new(txt, strlen(txt));
		FREE(txt, 0);

		return tmpRes;

	} else
		return utf_new(text, strlen(text));
}


/* utf_nextu2 ******************************************************************

   Read the next unicode character from the utf string and increment
   the utf-string pointer accordingly.

   CAUTION: This function is unsafe for input that was not checked 
            by is_valid_utf!

*******************************************************************************/

u2 utf_nextu2(char **utf_ptr)
{
    /* uncompressed unicode character */
    u2 unicode_char = 0;
    /* current position in utf text */	
    unsigned char *utf = (unsigned char *) (*utf_ptr);
    /* bytes representing the unicode character */
    unsigned char ch1, ch2, ch3;
    /* number of bytes used to represent the unicode character */
    int len = 0;
	
    switch ((ch1 = utf[0]) >> 4) {
	default: /* 1 byte */
		(*utf_ptr)++;
		return (u2) ch1;
	case 0xC: 
	case 0xD: /* 2 bytes */
		if (((ch2 = utf[1]) & 0xC0) == 0x80) {
			unsigned char high = ch1 & 0x1F;
			unsigned char low  = ch2 & 0x3F;
			unicode_char = (high << 6) + low;
			len = 2;
		}
		break;

	case 0xE: /* 2 or 3 bytes */
		if (((ch2 = utf[1]) & 0xC0) == 0x80) {
			if (((ch3 = utf[2]) & 0xC0) == 0x80) {
				unsigned char low  = ch3 & 0x3f;
				unsigned char mid  = ch2 & 0x3f;
				unsigned char high = ch1 & 0x0f;
				unicode_char = (((high << 6) + mid) << 6) + low;
				len = 3;
			} else
				len = 2;					   
		}
		break;
    }

    /* update position in utf-text */
    *utf_ptr = (char *) (utf + len);

    return unicode_char;
}


/* utf_bytes *******************************************************************

   Determine number of bytes (aka. octets) in the utf string.

   IN:
      u............utf string

   OUT:
      The number of octets of this utf string.
	  There is _no_ terminating zero included in this count.

*******************************************************************************/

u4 utf_bytes(utf *u)
{
	return u->blength;
}


/* utf_get_number_of_u2s_for_buffer ********************************************

   Determine number of UTF-16 u2s in the given UTF-8 buffer

   CAUTION: This function is unsafe for input that was not checked 
            by is_valid_utf!

   CAUTION: Use this function *only* when you want to convert an UTF-8 buffer
   to an array of u2s (UTF-16) and want to know how many of them you will get.
   All other uses of this function are probably wrong.

   IN:
      buffer........points to first char in buffer
	  blength.......number of _bytes_ in the buffer

   OUT:
      the number of u2s needed to hold this string in UTF-16 encoding.
	  There is _no_ terminating zero included in this count.

   NOTE: Unlike utf_get_number_of_u2s, this function never throws an
   exception.

*******************************************************************************/

u4 utf_get_number_of_u2s_for_buffer(const char *buffer, u4 blength)
{
	const char *endpos;                 /* points behind utf string           */
	const char *utf_ptr;                /* current position in utf text       */
	u4 len = 0;                         /* number of unicode characters       */

	utf_ptr = buffer;
	endpos = utf_ptr + blength;

	while (utf_ptr < endpos) {
		len++;
		/* next unicode character */
		utf_nextu2((char **)&utf_ptr);
	}

	assert(utf_ptr == endpos);

	return len;
}


/* utf_get_number_of_u2s *******************************************************

   Determine number of UTF-16 u2s in the utf string.

   CAUTION: This function is unsafe for input that was not checked 
            by is_valid_utf!

   CAUTION: Use this function *only* when you want to convert a utf string
   to an array of u2s and want to know how many of them you will get.
   All other uses of this function are probably wrong.

   IN:
      u............utf string

   OUT:
      the number of u2s needed to hold this string in UTF-16 encoding.
	  There is _no_ terminating zero included in this count.
	  XXX 0 if a NullPointerException has been thrown (see below)

*******************************************************************************/

u4 utf_get_number_of_u2s(utf *u)
{
	char *endpos;                       /* points behind utf string           */
	char *utf_ptr;                      /* current position in utf text       */
	u4 len = 0;                         /* number of unicode characters       */

	/* XXX this is probably not checked by most callers! Review this after */
	/* the invalid uses of this function have been eliminated */
	if (u == NULL) {
		exceptions_throw_nullpointerexception();
		return 0;
	}

	endpos = UTF_END(u);
	utf_ptr = u->text;

	while (utf_ptr < endpos) {
		len++;
		/* next unicode character */
		utf_nextu2(&utf_ptr);
	}

	if (utf_ptr != endpos) {
		/* string ended abruptly */
		exceptions_throw_internalerror("Illegal utf8 string");
		return 0;
	}

	return len;
}


/* utf8_safe_number_of_u2s *****************************************************

   Determine number of UTF-16 u2s needed for decoding the given UTF-8 string.
   (For invalid UTF-8 the U+fffd replacement character will be counted.)

   This function is safe even for invalid UTF-8 strings.

   IN:
      text..........zero-terminated(!) UTF-8 string (may be invalid)
	                must NOT be NULL
	  nbytes........strlen(text). (This is needed to completely emulate
	                the RI).

   OUT:
      the number of u2s needed to hold this string in UTF-16 encoding.
	  There is _no_ terminating zero included in this count.

*******************************************************************************/

s4 utf8_safe_number_of_u2s(const char *text, s4 nbytes) {
	register const unsigned char *t;
	register s4 byte;
	register s4 len;
	register const unsigned char *tlimit;
	s4 byte1;
	s4 byte2;
	s4 byte3;
	s4 value;
	s4 skip;

	assert(text);
	assert(nbytes >= 0);

	len = 0;
	t = (const unsigned char *) text;
	tlimit = t + nbytes;

	/* CAUTION: Keep this code in sync with utf8_safe_convert_to_u2s! */

	while (1) {
		byte = *t++;

		if (byte & 0x80) {
			/* highest bit set, non-ASCII character */

			if ((byte & 0xe0) == 0xc0) {
				/* 2-byte: should be 110..... 10...... ? */

				if ((*t++ & 0xc0) == 0x80)
					; /* valid 2-byte */
				else
					t--; /* invalid */
			}
			else if ((byte & 0xf0) == 0xe0) {
				/* 3-byte: should be 1110.... 10...... 10...... */
				/*                            ^t                */

				if (t + 2 > tlimit)
					return len + 1; /* invalid, stop here */

				if ((*t++ & 0xc0) == 0x80) {
					if ((*t++ & 0xc0) == 0x80)
						; /* valid 3-byte */
					else
						t--; /* invalid */
				}
				else
					t--; /* invalid */
			}
			else if ((byte & 0xf8) == 0xf0) {
				/* 4-byte: should be 11110... 10...... 10...... 10...... */
				/*                            ^t                         */

				if (t + 3 > tlimit)
					return len + 1; /* invalid, stop here */

				if (((byte1 = *t++) & 0xc0) == 0x80) {
					if (((byte2 = *t++) & 0xc0) == 0x80) {
						if (((byte3 = *t++) & 0xc0) == 0x80) {
							/* valid 4-byte UTF-8? */
							value = ((byte  & 0x07) << 18)
								  | ((byte1 & 0x3f) << 12)
								  | ((byte2 & 0x3f) <<  6)
								  | ((byte3 & 0x3f)      );

							if (value > 0x10FFFF)
								; /* invalid */
							else if (value > 0xFFFF)
								len += 1; /* we need surrogates */
							else
								; /* 16bit suffice */
						}
						else
							t--; /* invalid */
					}
					else
						t--; /* invalid */
				}
				else
					t--; /* invalid */
			}
			else if ((byte & 0xfc) == 0xf8) {
				/* invalid 5-byte */
				if (t + 4 > tlimit)
					return len + 1; /* invalid, stop here */

				skip = 4;
				for (; skip && ((*t & 0xc0) == 0x80); --skip)
					t++;
			}
			else if ((byte & 0xfe) == 0xfc) {
				/* invalid 6-byte */
				if (t + 5 > tlimit)
					return len + 1; /* invalid, stop here */

				skip = 5;
				for (; skip && ((*t & 0xc0) == 0x80); --skip)
					t++;
			}
			else
				; /* invalid */
		}
		else {
			/* NUL */

			if (byte == 0)
				break;

			/* ASCII character, common case */
		}

		len++;
	}

	return len;
}


/* utf8_safe_convert_to_u2s ****************************************************

   Convert the given UTF-8 string to UTF-16 into a pre-allocated buffer.
   (Invalid UTF-8 will be replaced with the U+fffd replacement character.)
   Use utf8_safe_number_of_u2s to determine the number of u2s to allocate.

   This function is safe even for invalid UTF-8 strings.

   IN:
      text..........zero-terminated(!) UTF-8 string (may be invalid)
	                must NOT be NULL
	  nbytes........strlen(text). (This is needed to completely emulate
	  				the RI).
	  buffer........a preallocated array of u2s to receive the decoded
	                string. Use utf8_safe_number_of_u2s to get the
					required number of u2s for allocating this.

*******************************************************************************/

#define UNICODE_REPLACEMENT  0xfffd

void utf8_safe_convert_to_u2s(const char *text, s4 nbytes, u2 *buffer) {
	register const unsigned char *t;
	register s4 byte;
	register const unsigned char *tlimit;
	s4 byte1;
	s4 byte2;
	s4 byte3;
	s4 value;
	s4 skip;

	assert(text);
	assert(nbytes >= 0);

	t = (const unsigned char *) text;
	tlimit = t + nbytes;

	/* CAUTION: Keep this code in sync with utf8_safe_number_of_u2s! */

	while (1) {
		byte = *t++;

		if (byte & 0x80) {
			/* highest bit set, non-ASCII character */

			if ((byte & 0xe0) == 0xc0) {
				/* 2-byte: should be 110..... 10...... */

				if (((byte1 = *t++) & 0xc0) == 0x80) {
					/* valid 2-byte UTF-8 */
					*buffer++ = ((byte  & 0x1f) << 6)
							  | ((byte1 & 0x3f)     );
				}
				else {
					*buffer++ = UNICODE_REPLACEMENT;
					t--;
				}
			}
			else if ((byte & 0xf0) == 0xe0) {
				/* 3-byte: should be 1110.... 10...... 10...... */

				if (t + 2 > tlimit) {
					*buffer++ = UNICODE_REPLACEMENT;
					return;
				}

				if (((byte1 = *t++) & 0xc0) == 0x80) {
					if (((byte2 = *t++) & 0xc0) == 0x80) {
						/* valid 3-byte UTF-8 */
						*buffer++ = ((byte  & 0x0f) << 12)
								  | ((byte1 & 0x3f) <<  6)
								  | ((byte2 & 0x3f)      );
					}
					else {
						*buffer++ = UNICODE_REPLACEMENT;
						t--;
					}
				}
				else {
					*buffer++ = UNICODE_REPLACEMENT;
					t--;
				}
			}
			else if ((byte & 0xf8) == 0xf0) {
				/* 4-byte: should be 11110... 10...... 10...... 10...... */

				if (t + 3 > tlimit) {
					*buffer++ = UNICODE_REPLACEMENT;
					return;
				}

				if (((byte1 = *t++) & 0xc0) == 0x80) {
					if (((byte2 = *t++) & 0xc0) == 0x80) {
						if (((byte3 = *t++) & 0xc0) == 0x80) {
							/* valid 4-byte UTF-8? */
							value = ((byte  & 0x07) << 18)
								  | ((byte1 & 0x3f) << 12)
								  | ((byte2 & 0x3f) <<  6)
								  | ((byte3 & 0x3f)      );

							if (value > 0x10FFFF) {
								*buffer++ = UNICODE_REPLACEMENT;
							}
							else if (value > 0xFFFF) {
								/* we need surrogates */
								*buffer++ = 0xd800 | ((value >> 10) - 0x40);
								*buffer++ = 0xdc00 | (value & 0x03ff);
							}
							else
								*buffer++ = value; /* 16bit suffice */
						}
						else {
							*buffer++ = UNICODE_REPLACEMENT;
							t--;
						}
					}
					else {
						*buffer++ = UNICODE_REPLACEMENT;
						t--;
					}
				}
				else {
					*buffer++ = UNICODE_REPLACEMENT;
					t--;
				}
			}
			else if ((byte & 0xfc) == 0xf8) {
				if (t + 4 > tlimit) {
					*buffer++ = UNICODE_REPLACEMENT;
					return;
				}

				skip = 4;
				for (; skip && ((*t & 0xc0) == 0x80); --skip)
					t++;
				*buffer++ = UNICODE_REPLACEMENT;
			}
			else if ((byte & 0xfe) == 0xfc) {
				if (t + 5 > tlimit) {
					*buffer++ = UNICODE_REPLACEMENT;
					return;
				}

				skip = 5;
				for (; skip && ((*t & 0xc0) == 0x80); --skip)
					t++;
				*buffer++ = UNICODE_REPLACEMENT;
			}
			else
				*buffer++ = UNICODE_REPLACEMENT;
		}
		else {
			/* NUL */

			if (byte == 0)
				break;

			/* ASCII character, common case */

			*buffer++ = byte;
		}
	}
}


/* u2_utflength ****************************************************************

   Returns the utf length in bytes of a u2 array.

*******************************************************************************/

u4 u2_utflength(u2 *text, u4 u2_length)
{
	u4 result_len = 0;                  /* utf length in bytes                */
	u2 ch;                              /* current unicode character          */
	u4 len;
	
	for (len = 0; len < u2_length; len++) {
		/* next unicode character */
		ch = *text++;
	  
		/* determine bytes required to store unicode character as utf */
		if (ch && (ch < 0x80)) 
			result_len++;
		else if (ch < 0x800)
			result_len += 2;	
		else 
			result_len += 3;	
	}

    return result_len;
}


/* utf_copy ********************************************************************

   Copy the given utf string byte-for-byte to a buffer.

   IN:
      buffer.......the buffer
	  u............the utf string

*******************************************************************************/

void utf_copy(char *buffer, utf *u)
{
	/* our utf strings are zero-terminated (done by utf_new) */
	MCOPY(buffer, u->text, char, u->blength + 1);
}


/* utf_cat *********************************************************************

   Append the given utf string byte-for-byte to a buffer.

   IN:
      buffer.......the buffer
	  u............the utf string

*******************************************************************************/

void utf_cat(char *buffer, utf *u)
{
	/* our utf strings are zero-terminated (done by utf_new) */
	MCOPY(buffer + strlen(buffer), u->text, char, u->blength + 1);
}


/* utf_copy_classname **********************************************************

   Copy the given utf classname byte-for-byte to a buffer.
   '/' is replaced by '.'

   IN:
      buffer.......the buffer
	  u............the utf string

*******************************************************************************/

void utf_copy_classname(char *buffer, utf *u)
{
	char *bufptr;
	char *srcptr;
	char *endptr;
	char ch;

	bufptr = buffer;
	srcptr = u->text;
	endptr = UTF_END(u) + 1; /* utfs are zero-terminared by utf_new */

	while (srcptr != endptr) {
		ch = *srcptr++;
		if (ch == '/')
			ch = '.';
		*bufptr++ = ch;
	}
}


/* utf_cat *********************************************************************

   Append the given utf classname byte-for-byte to a buffer.
   '/' is replaced by '.'

   IN:
      buffer.......the buffer
	  u............the utf string

*******************************************************************************/

void utf_cat_classname(char *buffer, utf *u)
{
	utf_copy_classname(buffer + strlen(buffer), u);
}

/* utf_display_printable_ascii *************************************************

   Write utf symbol to stdout (for debugging purposes).
   Non-printable and non-ASCII characters are printed as '?'.

*******************************************************************************/

void utf_display_printable_ascii(utf *u)
{
	char *endpos;                       /* points behind utf string           */
	char *utf_ptr;                      /* current position in utf text       */

	if (u == NULL) {
		printf("NULL");
		fflush(stdout);
		return;
	}

	endpos = UTF_END(u);
	utf_ptr = u->text;

	while (utf_ptr < endpos) {
		/* read next unicode character */

		u2 c = utf_nextu2(&utf_ptr);

		if ((c >= 32) && (c <= 127))
			printf("%c", c);
		else
			printf("?");
	}

	fflush(stdout);
}


/* utf_display_printable_ascii_classname ***************************************

   Write utf symbol to stdout with `/' converted to `.' (for debugging
   purposes).
   Non-printable and non-ASCII characters are printed as '?'.

*******************************************************************************/

void utf_display_printable_ascii_classname(utf *u)
{
	char *endpos;                       /* points behind utf string           */
	char *utf_ptr;                      /* current position in utf text       */

	if (u == NULL) {
		printf("NULL");
		fflush(stdout);
		return;
	}

	endpos = UTF_END(u);
	utf_ptr = u->text;

	while (utf_ptr < endpos) {
		/* read next unicode character */

		u2 c = utf_nextu2(&utf_ptr);

		if (c == '/')
			c = '.';

		if ((c >= 32) && (c <= 127))
			printf("%c", c);
		else
			printf("?");
	}

	fflush(stdout);
}


/* utf_sprint_convert_to_latin1 ************************************************
	
   Write utf symbol into c-string (for debugging purposes).
   Characters are converted to 8-bit Latin-1, non-Latin-1 characters yield
   invalid results.

*******************************************************************************/

void utf_sprint_convert_to_latin1(char *buffer, utf *u)
{
	char *endpos;                       /* points behind utf string           */
	char *utf_ptr;                      /* current position in utf text       */
	u2 pos = 0;                         /* position in c-string               */

	if (!u) {
		strcpy(buffer, "NULL");
		return;
	}

	endpos = UTF_END(u);
	utf_ptr = u->text;

	while (utf_ptr < endpos) 
		/* copy next unicode character */       
		buffer[pos++] = utf_nextu2(&utf_ptr);

	/* terminate string */
	buffer[pos] = '\0';
}


/* utf_sprint_convert_to_latin1_classname **************************************
	
   Write utf symbol into c-string with `/' converted to `.' (for debugging
   purposes).
   Characters are converted to 8-bit Latin-1, non-Latin-1 characters yield
   invalid results.

*******************************************************************************/

void utf_sprint_convert_to_latin1_classname(char *buffer, utf *u)
{
	char *endpos;                       /* points behind utf string           */
	char *utf_ptr;                      /* current position in utf text       */
	u2 pos = 0;                         /* position in c-string               */

	if (!u) {
		strcpy(buffer, "NULL");
		return;
	}

	endpos = UTF_END(u);
	utf_ptr = u->text;

	while (utf_ptr < endpos) {
		/* copy next unicode character */       
		u2 c = utf_nextu2(&utf_ptr);
		if (c == '/') c = '.';
		buffer[pos++] = c;
	}

	/* terminate string */
	buffer[pos] = '\0';
}


/* utf_strcat_convert_to_latin1 ************************************************
	
   Like libc strcat, but uses an utf8 string.
   Characters are converted to 8-bit Latin-1, non-Latin-1 characters yield
   invalid results.

*******************************************************************************/

void utf_strcat_convert_to_latin1(char *buffer, utf *u)
{
	utf_sprint_convert_to_latin1(buffer + strlen(buffer), u);
}


/* utf_strcat_convert_to_latin1_classname **************************************
	
   Like libc strcat, but uses an utf8 string.
   Characters are converted to 8-bit Latin-1, non-Latin-1 characters yield
   invalid results.

*******************************************************************************/

void utf_strcat_convert_to_latin1_classname(char *buffer, utf *u)
{
	utf_sprint_convert_to_latin1_classname(buffer + strlen(buffer), u);
}


/* utf_fprint_printable_ascii **************************************************
	
   Write utf symbol into file.
   Non-printable and non-ASCII characters are printed as '?'.

*******************************************************************************/

void utf_fprint_printable_ascii(FILE *file, utf *u)
{
	char *endpos;                       /* points behind utf string           */
	char *utf_ptr;                      /* current position in utf text       */

	if (!u)
		return;

	endpos = UTF_END(u);
	utf_ptr = u->text;

	while (utf_ptr < endpos) { 
		/* read next unicode character */                
		u2 c = utf_nextu2(&utf_ptr);				

		if (c >= 32 && c <= 127) fprintf(file, "%c", c);
		else fprintf(file, "?");
	}
}


/* utf_fprint_printable_ascii_classname ****************************************
	
   Write utf symbol into file with `/' converted to `.'.
   Non-printable and non-ASCII characters are printed as '?'.

*******************************************************************************/

void utf_fprint_printable_ascii_classname(FILE *file, utf *u)
{
	char *endpos;                       /* points behind utf string           */
	char *utf_ptr;                      /* current position in utf text       */

    if (!u)
		return;

	endpos = UTF_END(u);
	utf_ptr = u->text;

	while (utf_ptr < endpos) { 
		/* read next unicode character */                
		u2 c = utf_nextu2(&utf_ptr);				
		if (c == '/') c = '.';

		if (c >= 32 && c <= 127) fprintf(file, "%c", c);
		else fprintf(file, "?");
	}
}


/* is_valid_utf ****************************************************************

   Return true if the given string is a valid UTF-8 string.

   utf_ptr...points to first character
   end_pos...points after last character

*******************************************************************************/

/*  static unsigned long min_codepoint[6] = {0,1L<<7,1L<<11,1L<<16,1L<<21,1L<<26}; */

bool is_valid_utf(char *utf_ptr, char *end_pos)
{
	int bytes;
	int len,i;
	char c;
	unsigned long v;

	if (end_pos < utf_ptr) return false;
	bytes = end_pos - utf_ptr;
	while (bytes--) {
		c = *utf_ptr++;

		if (!c) return false;                     /* 0x00 is not allowed */
		if ((c & 0x80) == 0) continue;            /* ASCII */

		if      ((c & 0xe0) == 0xc0) len = 1;     /* 110x xxxx */
		else if ((c & 0xf0) == 0xe0) len = 2;     /* 1110 xxxx */
		else if ((c & 0xf8) == 0xf0) len = 3;     /* 1111 0xxx */
		else if ((c & 0xfc) == 0xf8) len = 4;     /* 1111 10xx */
		else if ((c & 0xfe) == 0xfc) len = 5;     /* 1111 110x */
		else return false;                        /* invalid leading byte */

		if (len > 2) return false;                /* Java limitation */

		v = (unsigned long)c & (0x3f >> len);
		
		if ((bytes -= len) < 0) return false;     /* missing bytes */

		for (i = len; i--; ) {
			c = *utf_ptr++;
			if ((c & 0xc0) != 0x80)               /* 10xx xxxx */
				return false;
			v = (v << 6) | (c & 0x3f);
		}

		if (v == 0) {
			if (len != 1) return false;           /* Java special */

		} else {
			/* Sun Java seems to allow overlong UTF-8 encodings */
			
			/* if (v < min_codepoint[len]) */
				/* XXX throw exception? */
		}

		/* surrogates in UTF-8 seem to be allowed in Java classfiles */
		/* if (v >= 0xd800 && v <= 0xdfff) return false; */ /* surrogates */

		/* even these seem to be allowed */
		/* if (v == 0xfffe || v == 0xffff) return false; */ /* invalid codepoints */
	}

	return true;
}


/* is_valid_name ***************************************************************

   Return true if the given string may be used as a class/field/method
   name. (Currently this only disallows empty strings and control
   characters.)

   NOTE: The string is assumed to have passed is_valid_utf!

   utf_ptr...points to first character
   end_pos...points after last character

*******************************************************************************/

bool is_valid_name(char *utf_ptr, char *end_pos)
{
	if (end_pos <= utf_ptr) return false; /* disallow empty names */

	while (utf_ptr < end_pos) {
		unsigned char c = *utf_ptr++;

		if (c < 0x20) return false; /* disallow control characters */
		if (c == 0xc0 && (unsigned char) *utf_ptr == 0x80)  /* disallow zero */
			return false;
	}

	return true;
}

bool is_valid_name_utf(utf *u)
{
	return is_valid_name(u->text, UTF_END(u));
}


/* utf_show ********************************************************************

   Writes the utf symbols in the utfhash to stdout and displays the
   number of external hash chains grouped according to the chainlength
   (for debugging purposes).

*******************************************************************************/

#if !defined(NDEBUG)
void utf_show(void)
{

#define CHAIN_LIMIT 20               /* limit for seperated enumeration */

	u4 chain_count[CHAIN_LIMIT]; /* numbers of chains */
	u4 max_chainlength = 0;      /* maximum length of the chains */
	u4 sum_chainlength = 0;      /* sum of the chainlengths */
	u4 beyond_limit = 0;         /* number of utf-symbols in chains with length>=CHAIN_LIMIT-1 */
	u4 i;

	printf("UTF-HASH:\n");

	/* show element of utf-hashtable */

	for (i = 0; i < hashtable_utf->size; i++) {
		utf *u = hashtable_utf->ptr[i];

		if (u) {
			printf("SLOT %d: ", (int) i);

			while (u) {
				printf("'");
				utf_display_printable_ascii(u);
				printf("' ");
				u = u->hashlink;
			}	
			printf("\n");
		}
	}

	printf("UTF-HASH: %d slots for %d entries\n", 
		   (int) hashtable_utf->size, (int) hashtable_utf->entries );

	if (hashtable_utf->entries == 0)
		return;

	printf("chains:\n  chainlength    number of chains    %% of utfstrings\n");

	for (i=0;i<CHAIN_LIMIT;i++)
		chain_count[i]=0;

	/* count numbers of hashchains according to their length */
	for (i=0; i<hashtable_utf->size; i++) {
		  
		utf *u = (utf*) hashtable_utf->ptr[i];
		u4 chain_length = 0;

		/* determine chainlength */
		while (u) {
			u = u->hashlink;
			chain_length++;
		}

		/* update sum of all chainlengths */
		sum_chainlength+=chain_length;

		/* determine the maximum length of the chains */
		if (chain_length>max_chainlength)
			max_chainlength = chain_length;

		/* update number of utf-symbols in chains with length>=CHAIN_LIMIT-1 */
		if (chain_length>=CHAIN_LIMIT) {
			beyond_limit+=chain_length;
			chain_length=CHAIN_LIMIT-1;
		}

		/* update number of hashchains of current length */
		chain_count[chain_length]++;
	}

	/* display results */  
	for (i=1;i<CHAIN_LIMIT-1;i++) 
		printf("       %2d %17d %18.2f%%\n",i,chain_count[i],(((float) chain_count[i]*i*100)/hashtable_utf->entries));
	  
	printf("     >=%2d %17d %18.2f%%\n",CHAIN_LIMIT-1,chain_count[CHAIN_LIMIT-1],((float) beyond_limit*100)/hashtable_utf->entries);


	printf("max. chainlength:%5d\n",max_chainlength);

	/* avg. chainlength = sum of chainlengths / number of chains */
	printf("avg. chainlength:%5.2f\n",(float) sum_chainlength / (hashtable_utf->size-chain_count[0]));
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

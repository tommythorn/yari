/* src/vm/global.h - global definitions

   Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
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
            Andreas Krall
            Mark Probst
            Philipp Tomsich
            Edwin Steiner
            Joseph Wenninger
            Christian Thalinger

   $Id: global.h 7675 2007-04-05 14:23:04Z michi $

*/


#ifndef _GLOBAL_H
#define _GLOBAL_H

#include "config.h"
#include "vm/types.h"


/* additional data types ******************************************************/

typedef void *voidptr;                  /* generic pointer                    */
typedef void (*functionptr) (void);     /* generic function pointer           */
typedef u1* methodptr;

typedef unsigned int bool;              /* boolean data type                  */

#define true         1
#define false        0


#if defined(ENABLE_SSA)
/* immediate to get an addidional target Local Var Index */
/* for IINC in Combination with SSA */
struct imm {
	s4 i;
	s4 op1_t;
};
#endif

/* immediate data union */

typedef union {
	s4          i;
	s8          l;
	float       f;
	double      d;
	void       *a;
	functionptr fp;
	u1          b[8];
#if defined(ENABLE_SSA)
	struct imm  _i;
#endif
} imm_union;


/* forward typedefs ***********************************************************/

typedef struct java_objectheader java_objectheader; 
typedef struct java_objectarray java_objectarray;


#define MAX_ALIGN 8             /* most generic alignment for JavaVM values   */


/* basic data types ***********************************************************/

/* The JavaVM types must numbered in the same order as the ICMD_Ixxx
   to ICMD_Axxx instructions (LOAD and STORE).  All other types can be
   numbered arbitrarily. */

#define TYPE_INT     0
#define TYPE_LNG     1
#define TYPE_FLT     2
#define TYPE_DBL     3
#define TYPE_ADR     4

#define TYPE_RET     8   /* must not share bits with TYPE_FLT or TYPE_LNG */

#define TYPE_VOID    10


#define IS_INT_LNG_TYPE(a)      (!((a) & TYPE_FLT))
#define IS_FLT_DBL_TYPE(a)      ((a) & TYPE_FLT)
#define IS_2_WORD_TYPE(a)       ((a) & TYPE_LNG)

#define IS_INT_TYPE(a)          ((a) == TYPE_INT)
#define IS_LNG_TYPE(a)          ((a) == TYPE_LNG)
#define IS_FLT_TYPE(a)          ((a) == TYPE_FLT)
#define IS_DBL_TYPE(a)          ((a) == TYPE_DBL)
#define IS_ADR_TYPE(a)          ((a) == TYPE_ADR)

#define IS_VOID_TYPE(a)         ((a) == TYPE_VOID)


/* primitive data types *******************************************************/

/* These values are used in parsed descriptors and in some other
   places were the different types handled internally as TYPE_INT have
   to be distinguished. */

#define PRIMITIVETYPE_COUNT  11  /* number of primitive types (+ dummies)     */

/* CAUTION: Don't change the numerical values! These constants are
   used as indices into the primitive type table. */

#define PRIMITIVETYPE_INT     TYPE_INT
#define PRIMITIVETYPE_LONG    TYPE_LNG
#define PRIMITIVETYPE_FLOAT   TYPE_FLT
#define PRIMITIVETYPE_DOUBLE  TYPE_DBL
#define PRIMITIVETYPE_DUMMY1  TYPE_ADR     /* not used! */
#define PRIMITIVETYPE_BYTE    5
#define PRIMITIVETYPE_CHAR    6
#define PRIMITIVETYPE_SHORT   7
#define PRIMITIVETYPE_BOOLEAN 8
#define PRIMITIVETYPE_DUMMY2  9            /* not used! */
#define PRIMITIVETYPE_VOID    TYPE_VOID

/* some Java related defines **************************************************/

#define JAVA_VERSION    "1.5.0"         /* this version is supported by CACAO */
#define CLASS_VERSION   "50.0"


/* Java class file constants **************************************************/

#define MAGIC             0xCAFEBABE
#define MAJOR_VERSION     50
#define MINOR_VERSION     0


/* Constant pool tags *********************************************************/

#define CONSTANT_Class                 7
#define CONSTANT_Fieldref              9
#define CONSTANT_Methodref            10
#define CONSTANT_InterfaceMethodref   11
#define CONSTANT_String                8
#define CONSTANT_Integer               3
#define CONSTANT_Float                 4
#define CONSTANT_Long                  5
#define CONSTANT_Double                6
#define CONSTANT_NameAndType          12
#define CONSTANT_Utf8                  1

#define CONSTANT_UNUSED                0


/* Class/Field/Method access and property flags *******************************/

#define ACC_UNDEF               -1      /* used internally                    */
#define ACC_NONE                 0      /* used internally                    */

#define ACC_PUBLIC          0x0001
#define ACC_PRIVATE         0x0002
#define ACC_PROTECTED       0x0004
#define ACC_STATIC          0x0008
#define ACC_FINAL           0x0010
#define ACC_SUPER           0x0020
#define ACC_SYNCHRONIZED    0x0020
#define ACC_VOLATILE        0x0040
#define ACC_BRIDGE          0x0040
#define ACC_TRANSIENT       0x0080
#define ACC_VARARGS         0x0080
#define ACC_NATIVE          0x0100
#define ACC_INTERFACE       0x0200
#define ACC_ABSTRACT        0x0400
#define ACC_STRICT          0x0800
#define ACC_SYNTHETIC       0x1000
#define ACC_ANNOTATION      0x2000
#define ACC_ENUM            0x4000
#define ACC_MIRANDA         0x8000

/* special flags used in classinfo ********************************************/

#define ACC_CLASS_REFLECT_MASK      0x0000ffff/* flags reported by reflection */

#define ACC_CLASS_PRIMITIVE         0x00010000/* class is a primitive class   */
#define ACC_CLASS_HAS_POINTERS      0x00020000/* instance contains pointers   */
#define ACC_CLASS_SOFT_REFERENCE    0x00040000
#define ACC_CLASS_WEAK_REFERENCE    0x00080000
#define ACC_CLASS_PHANTOM_REFERENCE 0x00100000


/* special flags used in methodinfo *******************************************/

#define ACC_METHOD_IMPLEMENTED 0x00010000     /* there is an implementation   */
#define ACC_METHOD_MONOMORPHIC 0x00020000     /* currently monomorphic method */


/* data structures of the runtime system **************************************/

/* java_objectheader ***********************************************************

   All objects (and arrays) which resides on the heap need the
   following header at the beginning of the data structure.

   TODO: Include detailed description from the Wiki (ObjectHeader) here.

*******************************************************************************/

#define HDRFLAG_FLC 0x01

struct java_objectheader {             /* header for all objects              */
	struct _vftbl            *vftbl;   /* pointer to virtual function table   */
#if defined(ENABLE_THREADS)
	struct lock_record_t *monitorPtr;
#endif
#if defined(ENABLE_THREADS) || defined(ENABLE_GC_CACAO)
	ptrint                hdrflags;    /* word containing the FLC and GC bits */
#endif
};


/* arrays **********************************************************************

	All arrays are objects (they need the object header with a pointer
	to a vftbl (array class table). There is one class for each array
	type. The array type is described by an arraydescriptor struct
	which is referenced by the vftbl.
*/

/* CAUTION: Don't change the numerical values! These constants (with
 * the exception of ARRAYTYPE_OBJECT) are used as indices in the
 * primitive type table.
 */
#define ARRAYTYPE_INT      PRIMITIVETYPE_INT
#define ARRAYTYPE_LONG     PRIMITIVETYPE_LONG
#define ARRAYTYPE_FLOAT    PRIMITIVETYPE_FLOAT
#define ARRAYTYPE_DOUBLE   PRIMITIVETYPE_DOUBLE
#define ARRAYTYPE_BYTE     PRIMITIVETYPE_BYTE
#define ARRAYTYPE_CHAR     PRIMITIVETYPE_CHAR
#define ARRAYTYPE_SHORT    PRIMITIVETYPE_SHORT
#define ARRAYTYPE_BOOLEAN  PRIMITIVETYPE_BOOLEAN
#define ARRAYTYPE_OBJECT   PRIMITIVETYPE_VOID     /* don't use as index! */

typedef struct java_arrayheader {       /* header for all arrays              */
	java_objectheader objheader;        /* object header                      */
	s4 size;                            /* array size                         */
} java_arrayheader;



/* structs for all kinds of arrays ********************************************/

/*  booleanarray and bytearray need identical memory layout (access methods
    use the same machine code */

typedef struct java_booleanarray {
	java_arrayheader header;
	u1 data[1];
} java_booleanarray;

typedef struct java_bytearray {
	java_arrayheader header;
	s1 data[1];
} java_bytearray;

typedef struct java_chararray {
	java_arrayheader header;
	u2 data[1];
} java_chararray;

typedef struct java_shortarray {
	java_arrayheader header;
	s2 data[1];
} java_shortarray;

typedef struct java_intarray {
	java_arrayheader header;
	s4 data[1];
} java_intarray;

typedef struct java_longarray {
	java_arrayheader header;
	s8 data[1];
} java_longarray;

typedef struct java_floatarray {
	java_arrayheader header;
	float data[1];
} java_floatarray;

typedef struct java_doublearray {
	java_arrayheader header;
	double data[1];
} java_doublearray;

/*  objectarray and arrayarray need identical memory layout (access methods
    use the same machine code */

struct java_objectarray {
	java_arrayheader   header;
	java_objectheader *data[1];
};


/* global constants related to the verifier ***********************************/

/* The verifier needs additional variables in the variable array. Since these */
/* must be reserved and set by parse.c and stack.c, we define these numbers   */
/* here to avoid mysterious hard-coded constants.                             */
/* stack.c needs an extra variable if the verifier is disabled.               */

#if defined(ENABLE_VERIFIER)
#    define VERIFIER_EXTRA_LOCALS  1
#    define VERIFIER_EXTRA_VARS    1
#    define STACK_EXTRA_VARS       0
#else
#    define VERIFIER_EXTRA_LOCALS  0
#    define VERIFIER_EXTRA_VARS    0
#    define STACK_EXTRA_VARS       1
#endif

#endif /* _GLOBAL_H */


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

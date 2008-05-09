/* src/vmcore/descriptor.c - checking and parsing of field / method descriptors

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

   $Id: descriptor.c 7464 2007-03-06 00:26:31Z edwin $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "md-abi.h"

#include "mm/memory.h"

#include "vm/exceptions.h"

#include "vm/jit_interface.h"

#include "vmcore/descriptor.h"
#include "vmcore/options.h"


/* constants (private to descriptor.c) ****************************************/

/* initial number of entries for the classrefhash of a descriptor_pool */
/* (currently the hash is never grown!) */
#define CLASSREFHASH_INIT_SIZE  64

/* initial number of entries for the descriptorhash of a descriptor_pool */
/* (currently the hash is never grown!) */
#define DESCRIPTORHASH_INIT_SIZE  128

/* data structures (private to descriptor.c) **********************************/

typedef struct classref_hash_entry classref_hash_entry;
typedef struct descriptor_hash_entry descriptor_hash_entry;

/* entry struct for the classrefhash of descriptor_pool */
struct classref_hash_entry {
	classref_hash_entry *hashlink;  /* for hash chaining            */
	utf                 *name;      /* name of the class refered to */
	u2                   index;     /* index into classref table    */
};

/* entry struct for the descriptorhash of descriptor_pool */
struct descriptor_hash_entry {
	descriptor_hash_entry *hashlink;
	utf                   *desc;
	parseddesc             parseddesc;
	s2                     paramslots; /* number of params, LONG/DOUBLE counted as 2 */
};


/****************************************************************************/
/* MACROS FOR DESCRIPTOR PARSING (private to descriptor.c)                  */
/****************************************************************************/

/* SKIP_FIELDDESCRIPTOR:
 * utf_ptr must point to the first character of a field descriptor.
 * After the macro call utf_ptr points to the first character after
 * the field descriptor.
 *
 * CAUTION: This macro does not check for an unexpected end of the
 * descriptor. Better use SKIP_FIELDDESCRIPTOR_SAFE.
 */
#define SKIP_FIELDDESCRIPTOR(utf_ptr)							\
	do { while (*(utf_ptr)=='[') (utf_ptr)++;					\
		if (*(utf_ptr)++=='L')									\
			while(*(utf_ptr)++ != ';') /* skip */; } while(0)

/* SKIP_FIELDDESCRIPTOR_SAFE:
 * utf_ptr must point to the first character of a field descriptor.
 * After the macro call utf_ptr points to the first character after
 * the field descriptor.
 *
 * Input:
 *     utf_ptr....points to first char of descriptor
 *     end_ptr....points to first char after the end of the string
 *     errorflag..must be initialized (to false) by the caller!
 * Output:
 *     utf_ptr....points to first char after the descriptor
 *     errorflag..set to true if the string ended unexpectedly
 */
#define SKIP_FIELDDESCRIPTOR_SAFE(utf_ptr,end_ptr,errorflag)			\
	do { while ((utf_ptr) != (end_ptr) && *(utf_ptr)=='[') (utf_ptr)++;	\
		if ((utf_ptr) == (end_ptr))										\
			(errorflag) = true;											\
		else															\
			if (*(utf_ptr)++=='L') {									\
				while((utf_ptr) != (end_ptr) && *(utf_ptr)++ != ';')	\
					/* skip */;											\
				if ((utf_ptr)[-1] != ';')								\
					(errorflag) = true; }} while(0)


/****************************************************************************/
/* DEBUG HELPERS                                                            */
/****************************************************************************/

/*#define DESCRIPTOR_VERBOSE*/

/****************************************************************************/
/* FUNCTIONS                                                                */
/****************************************************************************/

/* descriptor_to_basic_type ****************************************************

   Return the basic type to use for a value with this descriptor.

   IN:
       utf..............descriptor utf string

   OUT:
       A TYPE_* constant.

   PRECONDITIONS:
       This function assumes that the descriptor has passed 
	   descriptor_pool_add checks and that it does not start with '('.

*******************************************************************************/

u2 descriptor_to_basic_type(utf *descriptor)
{
	assert(descriptor->blength >= 1);
	
	switch (descriptor->text[0]) {
		case 'B': 
		case 'C':
		case 'I':
		case 'S':  
		case 'Z':  return TYPE_INT;
		case 'D':  return TYPE_DBL;
		case 'F':  return TYPE_FLT;
		case 'J':  return TYPE_LNG;
		case 'L':
		case '[':  return TYPE_ADR;
	}
			
	assert(0);

	return 0; /* keep the compiler happy */
}

/* descriptor_typesize**** ****************************************************

   Return the size in bytes needed for the given type.

   IN:
       td..............typedesc describing the type

   OUT:
       The number of bytes

*******************************************************************************/

u2 descriptor_typesize(typedesc *td)
{
	assert(td);

	switch (td->type) {
		case TYPE_INT: return 4;
		case TYPE_LNG: return 8;
		case TYPE_FLT: return 4;
		case TYPE_DBL: return 8;
		case TYPE_ADR: return sizeof(voidptr);
	}

	assert(0);

	return 0; /* keep the compiler happy */
}

/* name_from_descriptor ********************************************************

   Return the class name indicated by the given descriptor
   (Internally used helper function)

   IN:
       c................class containing the descriptor
       utf_ptr..........first character of descriptor
       end_ptr..........first character after the end of the string
       mode.............a combination (binary or) of the following flags:

               (Flags marked with * are the default settings.)

               How to handle "V" descriptors:

			     * DESCRIPTOR_VOID.....handle it like other primitive types
                   DESCRIPTOR_NOVOID...treat it as an error

               How to deal with extra characters after the end of the
               descriptor:

			     * DESCRIPTOR_NOCHECKEND...ignore (useful for parameter lists)
                   DESCRIPTOR_CHECKEND.....treat them as an error

   OUT:
       *next............if non-NULL, *next is set to the first character after
                        the descriptor. (Undefined if an error occurs.)
       *name............set to the utf name of the class

   RETURN VALUE:
       true.............descriptor parsed successfully
	   false............an exception has been thrown

*******************************************************************************/

#define DESCRIPTOR_VOID          0      /* default */
#define DESCRIPTOR_NOVOID        0x0040
#define DESCRIPTOR_NOCHECKEND    0      /* default */
#define DESCRIPTOR_CHECKEND      0x1000

static bool 
name_from_descriptor(classinfo *c,
					 char *utf_ptr, char *end_ptr,
					 char **next, int mode, utf **name)
{
	char *start = utf_ptr;
	bool error = false;

	assert(c);
	assert(utf_ptr);
	assert(end_ptr);
	assert(name);
	
	*name = NULL;		
	SKIP_FIELDDESCRIPTOR_SAFE(utf_ptr, end_ptr, error);

	if (mode & DESCRIPTOR_CHECKEND)
		error |= (utf_ptr != end_ptr);
	
	if (!error) {
		if (next) *next = utf_ptr;
		
		switch (*start) {
		  case 'V':
			  if (mode & DESCRIPTOR_NOVOID)
				  break;
			  /* FALLTHROUGH! */
		  case 'I':
		  case 'J':
		  case 'F':
		  case 'D':
		  case 'B':
		  case 'C':
		  case 'S':
		  case 'Z':
			  return true;
			  
		  case 'L':
			  start++;
			  utf_ptr--;
			  /* FALLTHROUGH! */
		  case '[':
			  *name = utf_new(start, utf_ptr - start);
			  return true;
		}
	}

	exceptions_throw_classformaterror(c, "Invalid descriptor");
	return false;
}


/* descriptor_to_typedesc ******************************************************
 
   Parse the given type descriptor and fill a typedesc struct
   (Internally used helper function)

   IN:
       pool.............the descriptor pool
	   utf_ptr..........points to first character of type descriptor
	   end_pos..........points after last character of the whole descriptor

   OUT:
       *next............set to next character after type descriptor
	   *d...............filled with parsed information

   RETURN VALUE:
       true.............parsing succeeded  
	   false............an exception has been thrown

*******************************************************************************/

static bool
descriptor_to_typedesc(descriptor_pool *pool, char *utf_ptr, char *end_pos,
					   char **next, typedesc *td)
{
	utf *name;
	
	if (!name_from_descriptor(pool->referer, utf_ptr, end_pos, next, 0, &name))
		return false;

	if (name) {
		/* a reference type */
		td->type = TYPE_ADR;
		td->decltype = TYPE_ADR;
		td->arraydim = 0;
		for (utf_ptr = name->text; *utf_ptr == '['; ++utf_ptr)
			td->arraydim++;
		td->classref = descriptor_pool_lookup_classref(pool, name);

	} else {
		/* a primitive type */
		switch (*utf_ptr) {
		case 'B': 
			td->decltype = PRIMITIVETYPE_BYTE;
			td->type = TYPE_INT;
			break;
		case 'C':
			td->decltype = PRIMITIVETYPE_CHAR;
			td->type = TYPE_INT;
			break;
		case 'S':  
			td->decltype = PRIMITIVETYPE_SHORT;
			td->type = TYPE_INT;
			break;
		case 'Z':
			td->decltype = PRIMITIVETYPE_BOOLEAN;
			td->type = TYPE_INT;
			break;
		case 'I':
			td->decltype = PRIMITIVETYPE_INT;
			td->type = TYPE_INT;
			break;
		case 'D':
			td->decltype = PRIMITIVETYPE_DOUBLE;
			td->type = TYPE_DBL;
			break;
		case 'F':
			td->decltype = PRIMITIVETYPE_FLOAT;
			td->type = TYPE_FLT;
			break;
		case 'J':
			td->decltype = PRIMITIVETYPE_LONG;
			td->type = TYPE_LNG;
			break;
		case 'V':
			td->decltype = PRIMITIVETYPE_VOID;
			td->type = TYPE_VOID;
			break;
		default:
			assert(false);
		}

		td->arraydim = 0;
		td->classref = NULL;
	}

	return true;
}


/* descriptor_pool_new *********************************************************
 
   Allocate a new descriptor_pool

   IN:
       referer..........class for which to create the pool

   RETURN VALUE:
       a pointer to the new descriptor_pool

*******************************************************************************/

descriptor_pool * 
descriptor_pool_new(classinfo *referer)
{
	descriptor_pool *pool;
	u4 hashsize;
	u4 slot;

	pool = DNEW(descriptor_pool);
	assert(pool);

	pool->referer = referer;
	pool->fieldcount = 0;
	pool->methodcount = 0;
	pool->paramcount = 0;
	pool->descriptorsize = 0;
	pool->descriptors = NULL;
	pool->descriptors_next = NULL;
	pool->classrefs = NULL;
	pool->descriptor_kind = NULL;
	pool->descriptor_kind_next = NULL;

	hashsize = CLASSREFHASH_INIT_SIZE;
	pool->classrefhash.size = hashsize;
	pool->classrefhash.entries = 0;
	pool->classrefhash.ptr = DMNEW(voidptr,hashsize);
	for (slot=0; slot<hashsize; ++slot)
		pool->classrefhash.ptr[slot] = NULL;

	hashsize = DESCRIPTORHASH_INIT_SIZE;
	pool->descriptorhash.size = hashsize;
	pool->descriptorhash.entries = 0;
	pool->descriptorhash.ptr = DMNEW(voidptr,hashsize);
	for (slot=0; slot<hashsize; ++slot)
		pool->descriptorhash.ptr[slot] = NULL;

	return pool;
}


/* descriptor_pool_add_class ***************************************************
 
   Add the given class reference to the pool

   IN:
       pool.............the descriptor_pool
	   name.............the class reference to add

   RETURN VALUE:
       true.............reference has been added
	   false............an exception has been thrown

*******************************************************************************/

bool 
descriptor_pool_add_class(descriptor_pool *pool, utf *name)
{
	u4 key,slot;
	classref_hash_entry *c;
	
	assert(pool);
	assert(name);

#ifdef DESCRIPTOR_VERBOSE
	fprintf(stderr,"descriptor_pool_add_class(%p,",(void*)pool);
	utf_fprint_printable_ascii(stderr,name);fprintf(stderr,")\n");
#endif

	/* find a place in the hashtable */

	key = utf_hashkey(name->text, name->blength);
	slot = key & (pool->classrefhash.size - 1);
	c = (classref_hash_entry *) pool->classrefhash.ptr[slot];

	while (c) {
		if (c->name == name)
			return true; /* already stored */
		c = c->hashlink;
	}

	/* check if the name is a valid classname */

	if (!is_valid_name(name->text,UTF_END(name))) {
		exceptions_throw_classformaterror(pool->referer, "Invalid class name");
		return false; /* exception */
	}

	/* XXX check maximum array dimension */
	
	c = DNEW(classref_hash_entry);
	c->name = name;
	c->index = pool->classrefhash.entries++;
	c->hashlink = (classref_hash_entry *) pool->classrefhash.ptr[slot];
	pool->classrefhash.ptr[slot] = c;

	return true;
}


/* descriptor_pool_add *********************************************************
 
   Check the given descriptor and add it to the pool

   IN:
       pool.............the descriptor_pool
	   desc.............the descriptor to add. Maybe a field or method desc.

   OUT:
       *paramslots......if non-NULL, set to the number of parameters.
	                    LONG and DOUBLE are counted twice

   RETURN VALUE:
       true.............descriptor has been added
	   false............an exception has been thrown

*******************************************************************************/

bool 
descriptor_pool_add(descriptor_pool *pool, utf *desc, int *paramslots)
{
	u4 key,slot;
	descriptor_hash_entry *d;
	char *utf_ptr;
	char *end_pos;
	utf *name;
	s4 argcount = 0;
	
#ifdef DESCRIPTOR_VERBOSE
	fprintf(stderr,"descriptor_pool_add(%p,",(void*)pool);
	utf_fprint_printable_ascii(stderr,desc);fprintf(stderr,")\n");
#endif

	assert(pool);
	assert(desc);

	/* find a place in the hashtable */

	key = utf_hashkey(desc->text, desc->blength);
	slot = key & (pool->descriptorhash.size - 1);
	d = (descriptor_hash_entry *) pool->descriptorhash.ptr[slot];

	/* Save all method descriptors in the hashtable, since the parsed         */
	/* descriptor may vary between differenf methods (static vs. non-static). */

	utf_ptr = desc->text;

	if (*utf_ptr != '(') {
		while (d) {
			if (d->desc == desc) {
				if (paramslots)
					*paramslots = d->paramslots;
				return true; /* already stored */
			}
			d = d->hashlink;
		}
	}

	/* add the descriptor to the pool */

	d = DNEW(descriptor_hash_entry);
	d->desc = desc;
	d->parseddesc.any = NULL;
	d->hashlink = (descriptor_hash_entry *) pool->descriptorhash.ptr[slot];
	pool->descriptorhash.ptr[slot] = d;

	/* now check the descriptor */

	end_pos = UTF_END(desc);
	
	if (*utf_ptr == '(') {
		/* a method descriptor */

		pool->methodcount++;
		utf_ptr++;

		/* check arguments */

		while ((utf_ptr != end_pos) && (*utf_ptr != ')')) {
			pool->paramcount++;

			/* We cannot count the `this' argument here because
			 * we don't know if the method is static. */

			if (*utf_ptr == 'J' || *utf_ptr == 'D')
				argcount += 2;
			else
				argcount++;

			if (!name_from_descriptor(pool->referer, utf_ptr, end_pos, &utf_ptr,
								      DESCRIPTOR_NOVOID, &name))
				return false;

			if (name)
				if (!descriptor_pool_add_class(pool, name))
					return false;
		}

		if (utf_ptr == end_pos) {
			exceptions_throw_classformaterror(pool->referer,
											  "Missing ')' in method descriptor");
			return false;
		}

		utf_ptr++; /* skip ')' */

		if (!name_from_descriptor(pool->referer, utf_ptr, end_pos, NULL,
							  	  DESCRIPTOR_CHECKEND, &name))
			return false;

		if (name)
			if (!descriptor_pool_add_class(pool,name))
				return false;

		if (argcount > 255) {
			exceptions_throw_classformaterror(pool->referer,
											  "Too many arguments in signature");
			return false;
		}

	} else {
		/* a field descriptor */

		pool->fieldcount++;
		
	    if (!name_from_descriptor(pool->referer, utf_ptr, end_pos, NULL,
						    	  DESCRIPTOR_NOVOID | DESCRIPTOR_CHECKEND,
								  &name))
			return false;

		if (name)
			if (!descriptor_pool_add_class(pool,name))
				return false;
	}

	d->paramslots = argcount;

	if (paramslots)
		*paramslots = argcount;

	return true;
}


/* descriptor_pool_create_classrefs ********************************************
 
   Create a table containing all the classrefs which were added to the pool

   IN:
       pool.............the descriptor_pool

   OUT:
       *count...........if count is non-NULL, this is set to the number
	                    of classrefs in the table

   RETURN VALUE:
       a pointer to the constant_classref table

*******************************************************************************/

constant_classref * 
descriptor_pool_create_classrefs(descriptor_pool *pool, s4 *count)
{
	u4 nclasses;
	u4 slot;
	classref_hash_entry *c;
	constant_classref *ref;
	
	assert(pool);

	nclasses = pool->classrefhash.entries;
	pool->classrefs = MNEW(constant_classref,nclasses);

	/* fill the constant_classref structs */

	for (slot = 0; slot < pool->classrefhash.size; ++slot) {
		c = (classref_hash_entry *) pool->classrefhash.ptr[slot];
		while (c) {
			ref = pool->classrefs + c->index;
			CLASSREF_INIT(*ref, pool->referer, c->name);
			c = c->hashlink;
		}
	}

	if (count)
		*count = nclasses;

	return pool->classrefs;
}


/* descriptor_pool_lookup_classref *********************************************
 
   Return the constant_classref for the given class name

   IN:
       pool.............the descriptor_pool
	   classname........name of the class to look up

   RETURN VALUE:
       a pointer to the constant_classref, or
	   NULL if an exception has been thrown

*******************************************************************************/

constant_classref * 
descriptor_pool_lookup_classref(descriptor_pool *pool, utf *classname)
{
	u4 key,slot;
	classref_hash_entry *c;

	assert(pool);
	assert(pool->classrefs);
	assert(classname);

	key = utf_hashkey(classname->text, classname->blength);
	slot = key & (pool->classrefhash.size - 1);
	c = (classref_hash_entry *) pool->classrefhash.ptr[slot];

	while (c) {
		if (c->name == classname)
			return pool->classrefs + c->index;
		c = c->hashlink;
	}

	exceptions_throw_internalerror("Class reference not found in descriptor pool");
	return NULL;
}


/* descriptor_pool_alloc_parsed_descriptors ************************************
 
   Allocate space for the parsed descriptors

   IN:
       pool.............the descriptor_pool

   NOTE:
       This function must be called after all descriptors have been added
	   with descriptor_pool_add.

*******************************************************************************/

void 
descriptor_pool_alloc_parsed_descriptors(descriptor_pool *pool)
{
	u4 size;
	
	assert(pool);

	/* TWISTI: paramcount + 1: we don't know if the method is static or   */
	/* not, i have no better solution yet.                                */

	size =
		pool->fieldcount * sizeof(typedesc) +
		pool->methodcount * (sizeof(methoddesc) - sizeof(typedesc)) +
		pool->paramcount * sizeof(typedesc) +
		pool->methodcount * sizeof(typedesc);      /* possible `this' pointer */

	pool->descriptorsize = size;
	if (size) {
		pool->descriptors = MNEW(u1, size);
		pool->descriptors_next = pool->descriptors;
	}

	size = pool->fieldcount + pool->methodcount;
	if (size) {
		pool->descriptor_kind = DMNEW(u1, size);
		pool->descriptor_kind_next = pool->descriptor_kind;
	}
}


/* descriptor_pool_parse_field_descriptor **************************************
 
   Parse the given field descriptor

   IN:
       pool.............the descriptor_pool
	   desc.............the field descriptor

   RETURN VALUE:
       a pointer to the parsed field descriptor, or
	   NULL if an exception has been thrown

   NOTE:
       descriptor_pool_alloc_parsed_descriptors must be called (once)
       before this function is used.

*******************************************************************************/

typedesc * 
descriptor_pool_parse_field_descriptor(descriptor_pool *pool, utf *desc)
{
	u4 key,slot;
	descriptor_hash_entry *d;
	typedesc *td;

	assert(pool);
	assert(pool->descriptors);
	assert(pool->descriptors_next);

	/* lookup the descriptor in the hashtable */

	key = utf_hashkey(desc->text, desc->blength);
	slot = key & (pool->descriptorhash.size - 1);
	d = (descriptor_hash_entry *) pool->descriptorhash.ptr[slot];

	while (d) {
		if (d->desc == desc) {
			/* found */
			if (d->parseddesc.fd)
				return d->parseddesc.fd;
			break;
		}
		d = d->hashlink;
	}

	assert(d);
	
	if (desc->text[0] == '(') {
		exceptions_throw_classformaterror(pool->referer,
										  "Method descriptor used in field reference");
		return NULL;
	}

	td = (typedesc *) pool->descriptors_next;
	pool->descriptors_next += sizeof(typedesc);
	
	if (!descriptor_to_typedesc(pool, desc->text, UTF_END(desc), NULL, td))
		return NULL;

	*(pool->descriptor_kind_next++) = 'f';

	d->parseddesc.fd = td;

	return td;
}


/* descriptor_pool_parse_method_descriptor *************************************
 
   Parse the given method descriptor

   IN:
       pool.............the descriptor_pool
       desc.............the method descriptor
       mflags...........the method flags
	   thisclass........classref to the class containing the method.
	   					This is ignored if mflags contains ACC_STATIC.
						The classref is stored for inserting the 'this' argument.

   RETURN VALUE:
       a pointer to the parsed method descriptor, or
	   NULL if an exception has been thrown

   NOTE: 
       descriptor_pool_alloc_parsed_descriptors must be called
       (once) before this function is used.

*******************************************************************************/

methoddesc * 
descriptor_pool_parse_method_descriptor(descriptor_pool *pool, utf *desc,
										s4 mflags,constant_classref *thisclass)
{
	u4 key, slot;
	descriptor_hash_entry *d;
	methoddesc            *md;
	typedesc              *td;
	char *utf_ptr;
	char *end_pos;
	s2 paramcount = 0;
	s2 paramslots = 0;

#ifdef DESCRIPTOR_VERBOSE
	fprintf(stderr,"descriptor_pool_parse_method_descriptor(%p,%d,%p,",
			(void*)pool,(int)mflags,(void*)thisclass);
	utf_fprint_printable_ascii(stderr,desc); fprintf(stderr,")\n");
#endif

	assert(pool);
	assert(pool->descriptors);
	assert(pool->descriptors_next);

	/* check that it is a method descriptor */
	
	if (desc->text[0] != '(') {
		exceptions_throw_classformaterror(pool->referer,
										  "Field descriptor used in method reference");
		return NULL;
	}

	/* lookup the descriptor in the hashtable */

	key = utf_hashkey(desc->text, desc->blength);
	slot = key & (pool->descriptorhash.size - 1);
	d = (descriptor_hash_entry *) pool->descriptorhash.ptr[slot];

	/* find an un-parsed descriptor */

	while (d) {
		if (d->desc == desc)
			if (!d->parseddesc.md)
				break;
		d = d->hashlink;
	}

	assert(d);

	md = (methoddesc *) pool->descriptors_next;
	pool->descriptors_next += sizeof(methoddesc) - sizeof(typedesc);

	utf_ptr = desc->text + 1; /* skip '(' */
	end_pos = UTF_END(desc);

	td = md->paramtypes;

	/* count the `this' pointer */

	if ((mflags != ACC_UNDEF) && !(mflags & ACC_STATIC)) {
		td->type = TYPE_ADR;
		td->decltype = TYPE_ADR;
		td->arraydim = 0;
		td->classref = thisclass;

		td++;
		pool->descriptors_next += sizeof(typedesc);
		paramcount++;
		paramslots++;
	}

	while (*utf_ptr != ')') {
		/* parse a parameter type */

		if (!descriptor_to_typedesc(pool, utf_ptr, end_pos, &utf_ptr, td))
			return NULL;

		if (IS_2_WORD_TYPE(td->type))
			paramslots++;
		
		td++;
		pool->descriptors_next += sizeof(typedesc);
		paramcount++;
		paramslots++;
	}
	utf_ptr++; /* skip ')' */

	/* Skip possible `this' pointer in paramtypes array to allow a possible   */
	/* memory move later in parse.                                            */
	/* We store the thisclass reference, so we can later correctly fill in    */
	/* the parameter slot of the 'this' argument.                             */

	if (mflags == ACC_UNDEF) {
		td->classref = thisclass;
		td++;
		pool->descriptors_next += sizeof(typedesc);
	}

	/* parse return type */

	if (!descriptor_to_typedesc(pool, utf_ptr, end_pos, NULL,
								&(md->returntype)))
		return NULL;

	md->paramcount = paramcount;
	md->paramslots = paramslots;

	/* If m != ACC_UNDEF we parse a real loaded method, so do param prealloc. */
	/* Otherwise we do this in stack analysis.                                */

	if (mflags != ACC_UNDEF) {
		if (md->paramcount > 0) {
			/* allocate memory for params */

			md->params = MNEW(paramdesc, md->paramcount);
		}
		else {
			md->params = METHODDESC_NOPARAMS;
		}

		/* fill the paramdesc */
		/* md_param_alloc has to be called if md->paramcount == 0,
		   too, so it can make the reservation for the Linkage Area,
		   Return Register... */

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
		if (!opt_intrp)
# endif
			md_param_alloc(md);
#endif

	} else {
		/* params will be allocated later by
		   descriptor_params_from_paramtypes if necessary */

		md->params = NULL;
	}

	*(pool->descriptor_kind_next++) = 'm';

	d->parseddesc.md = md;

	return md;
}

/* descriptor_params_from_paramtypes *******************************************
 
   Create the paramdescs for a method descriptor. This function is called
   when we know whether the method is static or not. This function may only
   be called once for each methoddesc, and only if md->params == NULL.

   IN:
       md...............the parsed method descriptor
	                    md->params MUST be NULL.
	   mflags...........the ACC_* access flags of the method. Only the
	                    ACC_STATIC bit is checked.
						The value ACC_UNDEF is NOT allowed.

   RETURN VALUE:
       true.............the paramdescs were created successfully
	   false............an exception has been thrown

   POSTCONDITION:
       md->parms != NULL

*******************************************************************************/

bool descriptor_params_from_paramtypes(methoddesc *md, s4 mflags)
{
	typedesc *td;

	assert(md);
	assert(md->params == NULL);
	assert(mflags != ACC_UNDEF);

	td = md->paramtypes;

	/* check for `this' pointer */

	if (!(mflags & ACC_STATIC)) {
		constant_classref *thisclass;

		/* fetch class reference from reserved param slot */
		thisclass = td[md->paramcount].classref;
		assert(thisclass);

		if (md->paramcount > 0) {
			/* shift param types by 1 argument */
			MMOVE(td + 1, td, typedesc, md->paramcount);
		}

		/* fill in first argument `this' */

		td->type = TYPE_ADR;
		td->decltype = TYPE_ADR;
		td->arraydim = 0;
		td->classref = thisclass;

		md->paramcount++;
		md->paramslots++;
	}

	/* if the method has params, process them */

	if (md->paramcount > 0) {
		/* allocate memory for params */

		md->params = MNEW(paramdesc, md->paramcount);

	} else {
		md->params = METHODDESC_NOPARAMS;
	}

	/* fill the paramdesc */
	/* md_param_alloc has to be called if md->paramcount == 0, too, so
	   it can make the reservation for the Linkage Area, Return
	   Register.. */

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
	if (!opt_intrp)
# endif
		md_param_alloc(md);
#endif

	return true;
}


/* descriptor_pool_get_parsed_descriptors **************************************
 
   Return a pointer to the block of parsed descriptors

   IN:
       pool.............the descriptor_pool

   OUT:
   	   *size............if size is non-NULL, this is set to the size of the
	                    parsed descriptor block (in u1)

   RETURN VALUE:
       a pointer to the block of parsed descriptors

   NOTE:
       descriptor_pool_alloc_parsed_descriptors must be called (once)
       before this function is used.

*******************************************************************************/

void * 
descriptor_pool_get_parsed_descriptors(descriptor_pool *pool, s4 *size)
{
	assert(pool);
	assert((!pool->fieldcount && !pool->methodcount) || pool->descriptors);
	
	if (size)
		*size = pool->descriptorsize;

	return pool->descriptors;
}


/* descriptor_pool_get_sizes ***************************************************
 
   Get the sizes of the class reference table and the parsed descriptors

   IN:
       pool.............the descriptor_pool

   OUT:
       *classrefsize....set to size of the class reference table
	   *descsize........set to size of the parsed descriptors

   NOTE:
       This function may only be called after both
	       descriptor_pool_create_classrefs, and
		   descriptor_pool_alloc_parsed_descriptors
	   have been called.

*******************************************************************************/

void 
descriptor_pool_get_sizes(descriptor_pool *pool, u4 *classrefsize, u4 *descsize)
{
	assert(pool);
	assert((!pool->fieldcount && !pool->methodcount) || pool->descriptors);
	assert(pool->classrefs);
	assert(classrefsize);
	assert(descsize);

	*classrefsize = pool->classrefhash.entries * sizeof(constant_classref);
	*descsize = pool->descriptorsize;
}


/****************************************************************************/
/* DEBUG HELPERS                                                            */
/****************************************************************************/

#ifndef NDEBUG
/* descriptor_debug_print_typedesc *********************************************
 
   Print the given typedesc to the given stream

   IN:
	   file.............stream to print to
	   d................the parsed descriptor

*******************************************************************************/

void 
descriptor_debug_print_typedesc(FILE *file,typedesc *d)
{
	int ch;

	if (!d) {
		fprintf(file,"(typedesc *)NULL");
		return;
	}
	
	if (d->type == TYPE_ADR) {
		if (d->classref)
			utf_fprint_printable_ascii(file,d->classref->name);
		else
			fprintf(file,"<class=NULL>");
	}
	else {
		switch (d->decltype) {
			case PRIMITIVETYPE_INT    : ch='I'; break;
			case PRIMITIVETYPE_CHAR   : ch='C'; break;
			case PRIMITIVETYPE_BYTE   : ch='B'; break;
			case PRIMITIVETYPE_SHORT  : ch='S'; break;
			case PRIMITIVETYPE_BOOLEAN: ch='Z'; break;
			case PRIMITIVETYPE_LONG   : ch='J'; break;
			case PRIMITIVETYPE_FLOAT  : ch='F'; break;
			case PRIMITIVETYPE_DOUBLE : ch='D'; break;
			case PRIMITIVETYPE_VOID   : ch='V'; break;
			default                   : ch='!';
		}
		fputc(ch,file);
	}
	if (d->arraydim)
		fprintf(file,"[%d]",d->arraydim);
}

/* descriptor_debug_print_paramdesc ********************************************
 
   Print the given paramdesc to the given stream

   IN:
	   file.............stream to print to
	   d................the parameter descriptor

*******************************************************************************/

void
descriptor_debug_print_paramdesc(FILE *file,paramdesc *d)
{
	if (!d) {
		fprintf(file,"(paramdesc *)NULL");
		return;
	}
	
	if (d->inmemory) {
		fprintf(file,"<m%d>",d->regoff);
	}
	else {
		fprintf(file,"<r%d>",d->regoff);
	}
}

/* descriptor_debug_print_methoddesc *******************************************
 
   Print the given methoddesc to the given stream

   IN:
	   file.............stream to print to
	   d................the parsed descriptor

*******************************************************************************/

void 
descriptor_debug_print_methoddesc(FILE *file,methoddesc *d)
{
	int i;
	
	if (!d) {
		fprintf(file,"(methoddesc *)NULL");
		return;
	}
	
	fputc('(',file);
	for (i=0; i<d->paramcount; ++i) {
		if (i)
			fputc(',',file);
		descriptor_debug_print_typedesc(file,d->paramtypes + i);
		if (d->params) {
			descriptor_debug_print_paramdesc(file,d->params + i);
		}
	}
	if (d->params == METHODDESC_NOPARAMS)
		fputs("<NOPARAMS>",file);
	fputc(')',file);
	descriptor_debug_print_typedesc(file,&(d->returntype));
}

/* descriptor_pool_debug_dump **************************************************
 
   Print the state of the descriptor_pool to the given stream

   IN:
       pool.............the descriptor_pool
	   file.............stream to print to

*******************************************************************************/

void 
descriptor_pool_debug_dump(descriptor_pool *pool,FILE *file)
{
	u4 slot;
	u1 *pos;
	u1 *kind;
	u4 size;
	
	fprintf(file,"======[descriptor_pool for ");
	utf_fprint_printable_ascii(file,pool->referer->name);
	fprintf(file,"]======\n");

	fprintf(file,"fieldcount:     %d\n",pool->fieldcount);
	fprintf(file,"methodcount:    %d\n",pool->methodcount);
	fprintf(file,"paramcount:     %d\n",pool->paramcount);
	fprintf(file,"classrefcount:  %d\n",pool->classrefhash.entries);
	fprintf(file,"descriptorsize: %d bytes\n",pool->descriptorsize);
	fprintf(file,"classrefsize:   %d bytes\n",
			(int)(pool->classrefhash.entries * sizeof(constant_classref)));

	fprintf(file,"class references:\n");
	for (slot=0; slot<pool->classrefhash.size; ++slot) {
		classref_hash_entry *c = (classref_hash_entry *) pool->classrefhash.ptr[slot];
		while (c) {
			fprintf(file,"    %4d: ",c->index);
			utf_fprint_printable_ascii(file,c->name);
			fprintf(file,"\n");
			c = c->hashlink;
		}
	}

	fprintf(file,"hashed descriptors:\n");
	for (slot=0; slot<pool->descriptorhash.size; ++slot) {
		descriptor_hash_entry *c = (descriptor_hash_entry *) pool->descriptorhash.ptr[slot];
		while (c) {
			fprintf(file,"    %p: ",c->parseddesc.any);
			utf_fprint_printable_ascii(file,c->desc);
			fprintf(file,"\n");
			c = c->hashlink;
		}
	}

	fprintf(file,"descriptors:\n");
	if (pool->descriptors) {
		pos = pool->descriptors;
		size = pool->descriptors_next - pool->descriptors;
		fprintf(file,"    size: %d bytes\n",size);
		
		if (pool->descriptor_kind) {
			kind = pool->descriptor_kind;

			while (pos < (pool->descriptors + size)) {
				fprintf(file,"    %p: ",pos);
				switch (*kind++) {
					case 'f':
						descriptor_debug_print_typedesc(file,(typedesc*)pos);
						pos += sizeof(typedesc);
						break;
					case 'm':
						descriptor_debug_print_methoddesc(file,(methoddesc*)pos);
						pos += ((methoddesc*)pos)->paramcount * sizeof(typedesc);
						pos += sizeof(methoddesc) - sizeof(typedesc);
						break;
					default:
						fprintf(file,"INVALID KIND");
				}
				fputc('\n',file);
			}
		}
		else {
			while (size >= sizeof(voidptr)) {
				fprintf(file,"    %p\n",*((voidptr*)pos));
				pos += sizeof(voidptr);
				size -= sizeof(voidptr);
			}
		}
	}

	fprintf(file,"==========================================================\n");
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


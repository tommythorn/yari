/* src/vm/access.c - checking access rights

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

   $Id: access.c 7563 2007-03-23 21:33:53Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "vm/access.h"
#include "vm/builtin.h"
#include "vm/exceptions.h"

#include "vm/jit/stacktrace.h"

#include "vmcore/class.h"


/****************************************************************************/
/* ACCESS CHECKS                                                            */
/****************************************************************************/

/* access_is_accessible_class **************************************************
 
   Check if a class is accessible from another class
  
   IN:
       referer..........the class containing the reference
       cls..............the result of resolving the reference
  
   RETURN VALUE:
       true.............access permitted
       false............access denied
   
   NOTE:
       This function performs the checks listed in section 5.4.4.
	   "Access Control" of "The Java(TM) Virtual Machine Specification,
	   Second Edition".

*******************************************************************************/

bool access_is_accessible_class(classinfo *referer, classinfo *cls)
{
	assert(referer);
	assert(cls);

	/* public classes are always accessible */

	if (cls->flags & ACC_PUBLIC)
		return true;

	/* a class in the same package is always accessible */

	if (SAME_PACKAGE(referer, cls))
		return true;

	/* a non-public class in another package is not accessible */

	return false;
}


/* access_is_accessible_member *************************************************
 
   Check if a field or method is accessible from a given class
  
   IN:
       referer..........the class containing the reference
       declarer.........the class declaring the member
       memberflags......the access flags of the member
  
   RETURN VALUE:
       true.............access permitted
       false............access denied

   NOTE:
       This function only performs the checks listed in section 5.4.4.
	   "Access Control" of "The Java(TM) Virtual Machine Specification,
	   Second Edition".

	   In particular a special condition for protected access with is
	   part of the verification process according to the spec is not
	   checked in this function.
   
*******************************************************************************/

bool access_is_accessible_member(classinfo *referer, classinfo *declarer,
								 s4 memberflags)
{
	assert(referer);
	assert(declarer);
	
	/* public members are accessible */

	if (memberflags & ACC_PUBLIC)
		return true;

	/* {declarer is not an interface} */

	/* private members are only accessible by the class itself */

	if (memberflags & ACC_PRIVATE)
		return (referer == declarer);

	/* {the member is protected or package private} */

	/* protected and package private members are accessible in the
	   same package */

	if (SAME_PACKAGE(referer, declarer))
		return true;

	/* package private members are not accessible outside the package */

	if (!(memberflags & ACC_PROTECTED))
		return false;

	/* {the member is protected and declarer is in another package} */

	/* a necessary condition for access is that referer is a subclass
	   of declarer */

	assert((referer->state & CLASS_LINKED) && (declarer->state & CLASS_LINKED));

	if (class_isanysubclass(referer, declarer))
		return true;

	return false;
}


/* access_check_member *********************************************************
 
   Check if the (indirect) caller has access rights to a member.
  
   IN:
       declarer.........the class declaring the member
       memberflags......the access flags of the member
	   calldepth........number of callers to ignore
	                    For example if the stacktrace looks like this:

					   java.lang.reflect.Method.invokeNative (Native Method)
				   [0] java.lang.reflect.Method.invoke (Method.java:329)
				   [1] <caller>

				        you must specify 1 so the access rights of <caller> 
						are checked.
  
   RETURN VALUE:
       true.............access permitted
       false............access denied, an exception has been thrown
   
*******************************************************************************/

bool access_check_member(classinfo *declarer, s4 memberflags, s4 calldepth)
{
	java_objectarray *oa;
	classinfo        *callerclass;

	/* if everything is public, there is nothing to check */

	if ((declarer->flags & ACC_PUBLIC) && (memberflags & ACC_PUBLIC))
		return true;

	/* get the caller's class */

	oa = stacktrace_getClassContext();

	if (oa == NULL)
		return false;

	assert(calldepth >= 0 && calldepth < oa->header.size);

	callerclass = (classinfo *) oa->data[calldepth];

	/* check access rights */

	if (!access_is_accessible_member(callerclass, declarer, memberflags)) {
		exceptions_throw_illegalaccessexception(callerclass);
		return false;
	}

	/* access granted */

	return true;
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


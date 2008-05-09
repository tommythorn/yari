/* src/native/vm/VMjdwp.c - jvmti->jdwp interface

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

   Author: Martin Platter

   Changes:             


   $Id: VMjdwp.c 4661 2006-03-21 00:04:59Z motse $

*/

#ifndef _VMJDWP_H
#define _VMJDWP_H

#include "native/jvmti/jvmti.h"

jvmtiEnv* jvmtienv;
extern jvmtiEventCallbacks jvmti_jdwp_EventCallbacks;
char* jdwpoptions;
bool suspend;               /* should the virtual machine suspend on startup?  */
jthread jdwpthread;

void printjvmtierror(char *desc, jvmtiError err);
#endif

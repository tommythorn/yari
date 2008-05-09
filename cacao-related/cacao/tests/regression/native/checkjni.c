/* src/tests/native/checkjni.c - for testing JNI stuff

   Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
   C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
   E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
   TU Wien

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

   Authors: Christian Thalinger

   Changes:

   $Id: checkjni.c 4695 2006-03-28 14:21:14Z twisti $

*/


#include <stdio.h>

#include "config.h"
#include "native/jni.h"


JNIEXPORT jboolean JNICALL Java_checkjni_IsAssignableFrom(JNIEnv *env, jclass clazz, jclass sub, jclass sup)
{
  return (*env)->IsAssignableFrom(env, sub, sup);
}

JNIEXPORT jboolean JNICALL Java_checkjni_IsInstanceOf(JNIEnv *env, jclass clazz, jobject obj, jclass c)
{
  return (*env)->IsInstanceOf(env, obj, c);
}

JNIEXPORT jint     JNICALL Java_checkjni_PushLocalFrame(JNIEnv *env, jclass clazz, jint capacity)
{
  return (*env)->PushLocalFrame(env, capacity);
}

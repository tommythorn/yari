/* src/native/vm/VMFrame.c - jdwp->jvmti interface

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

Authors: Martin Platter

Changes: 


$Id: gnu_classpath_jdwp_VMFrame.c 6213 2006-12-18 17:36:06Z twisti $

*/

#include "toolbox/logging.h"
#include "native/jni.h"
#include "native/include/gnu_classpath_jdwp_VMFrame.h"


/*
 * Class:     gnu/classpath/jdwp/VMFrame
 * Method:    getValue
 * Signature: (I)Ljava/lang/Object;
 */
JNIEXPORT struct java_lang_Object* JNICALL Java_gnu_classpath_jdwp_VMFrame_getValue(JNIEnv *env, struct gnu_classpath_jdwp_VMFrame* this, s4 par1) {
    log_text ("JVMTI-Call: IMPLEMENT ME!!!");
    return 0;
}


/*
 * Class:     gnu/classpath/jdwp/VMFrame
 * Method:    setValue
 * Signature: (ILjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_gnu_classpath_jdwp_VMFrame_setValue(JNIEnv *env, struct gnu_classpath_jdwp_VMFrame* this, s4 par1, struct java_lang_Object* par2) {
    log_text ("JVMTI-Call: IMPLEMENT ME!!!");
	return 0;
}

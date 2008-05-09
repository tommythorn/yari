/* src/toolbox/logging.c - contains logging functions

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

   $Id: logging.c 7685 2007-04-11 08:21:06Z twisti $

*/


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm/types.h"

#include "mm/memory.h"
#include "toolbox/logging.h"
#include "toolbox/util.h"
#include "vm/global.h"

#if defined(ENABLE_STATISTICS)
# include "vmcore/statistics.h"
#endif


/***************************************************************************
                        LOG FILE HANDLING 
***************************************************************************/

FILE *logfile = NULL;


void log_init(const char *fname)
{
	if (fname) {
		if (fname[0]) {
			logfile = fopen(fname, "w");
		}
	}
}


/* log_start *******************************************************************

   Writes the preleading LOG: text to the protocol file (if opened) or
   to stdout.

*******************************************************************************/

/* ATTENTION: Don't include threads-common.h, because we can't
   bootstrap this file in that case (missing java_lang_Thread.h).
   Instead we declare threads_get_current_threadobject differently: */

/* #if defined(ENABLE_THREADS) */
/* # include "threads/threads-common.h" */
/* #endif */

extern ptrint threads_get_current_tid(void);


void log_start(void)
{
#if defined(ENABLE_THREADS)
	ptrint tid;

	tid = threads_get_current_tid();
#endif

	if (logfile) {
#if defined(ENABLE_THREADS)
# if SIZEOF_VOID_P == 8
		fprintf(logfile, "[0x%016lx] ", tid );
# else
		fprintf(logfile, "[0x%08x] ", tid);
# endif
#endif
	}
	else {
#if defined(ENABLE_THREADS)
# if SIZEOF_VOID_P == 8
		fprintf(stdout, "LOG: [0x%016lx] ", tid);
# else
		fprintf(stdout, "LOG: [0x%08x] ", tid);
# endif
#else
		fputs("LOG: ", stdout);
#endif
	}
}


/* log_vprint ******************************************************************

   Writes logtext to the protocol file (if opened) or to stdout.

*******************************************************************************/

void log_vprint(const char *text, va_list ap)
{
	if (logfile)
		vfprintf(logfile, text, ap);
	else
		vfprintf(stdout, text, ap);
}


/* log_print *******************************************************************

   Writes logtext to the protocol file (if opened) or to stdout.

*******************************************************************************/

void log_print(const char *text, ...)
{
	va_list ap;

	va_start(ap, text);
	log_vprint(text, ap);
	va_end(ap);
}


/* log_println *****************************************************************

   Writes logtext to the protocol file (if opened) or to stdout with a
   trailing newline.

*******************************************************************************/

void log_println(const char *text, ...)
{
	va_list ap;

	log_start();

	va_start(ap, text);
	log_vprint(text, ap);
	va_end(ap);

	log_finish();
}


/* log_finish ******************************************************************

   Finishes a logtext line with trailing newline and a fflush.

*******************************************************************************/

void log_finish(void)
{
	if (logfile) {
		fputs("\n", logfile);
		fflush(logfile);
	}
	else {
		fputs("\n", stdout);
		fflush(stdout);
	}
}


/* log_message_utf *************************************************************

   Outputs log text like this:

   LOG: Creating class: java/lang/Object

*******************************************************************************/

void log_message_utf(const char *msg, utf *u)
{
	char *buf;
	s4    len;

	len = strlen(msg) + utf_bytes(u) + strlen("0");

	buf = MNEW(char, len);

	strcpy(buf, msg);
	utf_cat(buf, u);

	log_text(buf);

	MFREE(buf, char, len);
}


/* log_message_class ***********************************************************

   Outputs log text like this:

   LOG: Loading class: java/lang/Object

*******************************************************************************/

void log_message_class(const char *msg, classinfo *c)
{
	log_message_utf(msg, c->name);
}


/* log_message_class_message_class *********************************************

   Outputs log text like this:

   LOG: Initialize super class java/lang/Object from java/lang/VMThread

*******************************************************************************/

void log_message_class_message_class(const char *msg1, classinfo *c1,
									 const char *msg2, classinfo *c2)
{
	char *buf;
	s4    len;

	len =
		strlen(msg1) + utf_bytes(c1->name) +
		strlen(msg2) + utf_bytes(c2->name) + strlen("0");

	buf = MNEW(char, len);

	strcpy(buf, msg1);
	utf_cat_classname(buf, c1->name);
	strcat(buf, msg2);
	utf_cat_classname(buf, c2->name);

	log_text(buf);

	MFREE(buf, char, len);
}


/* log_message_method **********************************************************

   Outputs log text like this:

   LOG: Compiling: java.lang.Object.clone()Ljava/lang/Object;

*******************************************************************************/

void log_message_method(const char *msg, methodinfo *m)
{
	char *buf;
	s4    len;

	len = strlen(msg) + utf_bytes(m->class->name) + strlen(".") +
		utf_bytes(m->name) + utf_bytes(m->descriptor) + strlen("0");

	buf = MNEW(char, len);

	strcpy(buf, msg);
	utf_cat_classname(buf, m->class->name);
	strcat(buf, ".");
	utf_cat(buf, m->name);
	utf_cat(buf, m->descriptor);

	log_text(buf);

	MFREE(buf, char, len);
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

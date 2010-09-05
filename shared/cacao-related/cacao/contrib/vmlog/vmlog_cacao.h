/* vmlog - high-speed logging for free VMs                  */
/* Copyright (C) 2006 Edwin Steiner <edwin.steiner@gmx.net> */

/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _VMLOG_CACAO_H_
#define _VMLOG_CACAO_H_

#include <threads/native/threads.h>

void vmlog_cacao_init(JavaVMInitArgs *vmargs);

void vmlog_cacao_init_lock(void);

void vmlog_cacao_enter_method(methodinfo *m);
void vmlog_cacao_leave_method(methodinfo *m);
void vmlog_cacao_unwnd_method(methodinfo *m);
void vmlog_cacao_unrol_method(methodinfo *m);
void vmlog_cacao_rerol_method(methodinfo *m);

void vmlog_cacao_throw(java_objectheader *xptr);
void vmlog_cacao_catch(java_objectheader *xptr);
void vmlog_cacao_signl(const char *name);

#endif

/* vim: noet ts=8 sw=8
 */


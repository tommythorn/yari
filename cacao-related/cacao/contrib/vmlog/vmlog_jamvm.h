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

#ifndef _VMLOG_JAMVM_H_
#define _VMLOG_JAMVM_H_

void vmlog_jamvm_init(int *pargc,char **argv);

void vmlog_jamvm_enter_method(Object *thread,MethodBlock *mb);
void vmlog_jamvm_leave_method(Object *thread,MethodBlock *mb);
void vmlog_jamvm_unwnd_method(Object *thread,MethodBlock *mb);

void vmlog_jamvm_throw(Object *thread,Object *exp);
void vmlog_jamvm_catch(Object *thread,Object *exp);

#endif

/* vim: noet ts=8 sw=8
 */


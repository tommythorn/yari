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

/* vmlog_jamvm.c - code to be #included in jamvm */

#include <vmlog_jamvm.h>
#include <vmlog.h>
#include <assert.h>

/*** global variables ************************************************/

static vmlog_log *vmlog_global_log = NULL;
static VMLock vmlog_global_lock;

/*** locking *********************************************************/

#define VMLOG_LOCK(vml)    lockVMLock(vmlog_global_lock,threadSelf())
#define VMLOG_UNLOCK(vml)  unlockVMLock(vmlog_global_lock,threadSelf())

/*** include the vmlog code ******************************************/

#include "vmlog.c"

/*** internal functions **********************************************/

void vmlog_jamvm_init(int *pargc,char **argv)
{
	vmlog_options *opts;

	opts = vmlog_opt_parse_cmd_line(pargc,argv);
	
	if (!opts->prefix)
		return;

	vmlog_global_log = vmlog_log_new(opts->prefix,1);

	if (opts->ignoreprefix) {
		vmlog_log_load_ignorelist(vmlog_global_log,
				opts->ignoreprefix);
	}

	if (opts->stringprefix) {
		vmlog_load_stringhash(vmlog_global_log,
				opts->stringprefix);
	}

	vmlog_opt_free(opts);
}

static void vmlog_jamvm_do_log(vmlog_log_function fun,
			       Object *thread,MethodBlock *mb)
{
	char *name;
	int namelen;
	ClassBlock *cb;
	
	assert(mb);

	if (!vmlog_global_log)
		return;

	cb = CLASS_CB(mb->class);
		
	name = vmlog_concat4len(
		cb->name,strlen(cb->name),
		".",1,
		mb->name,strlen(mb->name),
		mb->type,strlen(mb->type),
		&namelen);

	fun(vmlog_global_log,thread,name,namelen);

	VMLOG_FREE_ARRAY(char,namelen+1,name);
}

/*** functions callable from jamvm ***********************************/

void vmlog_jamvm_enter_method(Object *thread,MethodBlock *mb)
{
	vmlog_jamvm_do_log(vmlog_log_enter,thread,mb);
}

void vmlog_jamvm_leave_method(Object *thread,MethodBlock *mb)
{
	vmlog_jamvm_do_log(vmlog_log_leave,thread,mb);
}

void vmlog_jamvm_unwnd_method(Object *thread,MethodBlock *mb)
{
	vmlog_jamvm_do_log(vmlog_log_unwnd,thread,mb);
}

void vmlog_jamvm_throw(Object *thread,Object *exp)
{
	ClassBlock *cb;
	
	assert(exp);

	if (!vmlog_global_log)
		return;

	cb = CLASS_CB(exp->class);
	assert(cb);

	vmlog_log_throw(vmlog_global_log,thread,cb->name,strlen(cb->name));
}

void vmlog_jamvm_catch(Object *thread,Object *exp)
{
	ClassBlock *cb;
	
	assert(exp);

	if (!vmlog_global_log)
		return;

	cb = CLASS_CB(exp->class);
	assert(cb);

	vmlog_log_catch(vmlog_global_log,thread,cb->name,strlen(cb->name));
}

/* vim: noet ts=8 sw=8
 */


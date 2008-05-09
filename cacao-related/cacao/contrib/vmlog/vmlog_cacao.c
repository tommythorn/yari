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

/* vmlog_cacao.c - code to be #included in cacao */

#include <vmlog_cacao.h>
#define VMLOG_HAVE_PTRINT
#include <vmlog.h>
#include <assert.h>

/*** global variables ************************************************/

static vmlog_log *vmlog_global_log = NULL;
static java_objectheader vmlog_global_lock;

/*** locking *********************************************************/

#define VMLOG_LOCK(vml)    lock_monitor_enter(&vmlog_global_lock)
#define VMLOG_UNLOCK(vml)  lock_monitor_exit(&vmlog_global_lock)

/*** include the vmlog code ******************************************/

#include <vmlog.c>

/*** internal functions **********************************************/

void vmlog_cacao_init(JavaVMInitArgs *vmargs)
{
	vmlog_options *opts;

	opts = vmlog_opt_parse_vmargs(vmargs);
	
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

void vmlog_cacao_init_lock(void)
{
	lock_init_object_lock(&vmlog_global_lock);
}

static void vmlog_cacao_do_log(vmlog_log_function fun,
			       methodinfo *m)
{
	char *name;
	int namelen;

	assert(m);

	if (!vmlog_global_log)
		return;

	name = vmlog_concat4len(m->class->name->text,m->class->name->blength,
				".",1,
				m->name->text,m->name->blength,
				m->descriptor->text,m->descriptor->blength,
				&namelen);

	fun(vmlog_global_log,(void*) THREADOBJECT,name,namelen);

	VMLOG_FREE_ARRAY(char,namelen+1,name);
}

/*** functions callable from cacao ***********************************/

void vmlog_cacao_enter_method(methodinfo *m)
{
	vmlog_cacao_do_log(vmlog_log_enter,m);
}

void vmlog_cacao_leave_method(methodinfo *m)
{
	vmlog_cacao_do_log(vmlog_log_leave,m);
}

void vmlog_cacao_unrol_method(methodinfo *m)
{
	vmlog_cacao_do_log(vmlog_log_unrol,m);
}

void vmlog_cacao_rerol_method(methodinfo *m)
{
	vmlog_cacao_do_log(vmlog_log_rerol,m);
}

void vmlog_cacao_unwnd_method(methodinfo *m)
{
	vmlog_cacao_do_log(vmlog_log_unwnd,m);
}

void vmlog_cacao_throw(java_objectheader *xptr)
{
	classinfo *c;
	
	if (!vmlog_global_log)
		return;

	if (xptr) {
		c = xptr->vftbl->class;
		vmlog_log_throw(vmlog_global_log,(void*) THREADOBJECT,
				c->name->text,c->name->blength);
	}
	else {
		vmlog_log_throw(vmlog_global_log,(void*) THREADOBJECT,
				"unknown Throwable",strlen("unknown Throwable"));
	}
}

void vmlog_cacao_catch(java_objectheader *xptr)
{
	classinfo *c;
	
	if (!vmlog_global_log)
		return;

	if (xptr) {
		c = xptr->vftbl->class;
		vmlog_log_catch(vmlog_global_log,(void*) THREADOBJECT,
				c->name->text,c->name->blength);
	}
	else {
		vmlog_log_catch(vmlog_global_log,(void*) THREADOBJECT,
				"unknown Throwable",strlen("unknown Throwable"));
	}
}

void vmlog_cacao_signl(const char *name)
{
	if (!vmlog_global_log)
		return;

	vmlog_log_signl(vmlog_global_log,(void*) THREADOBJECT,
			name, strlen(name));
}

/* vim: noet ts=8 sw=8
 */


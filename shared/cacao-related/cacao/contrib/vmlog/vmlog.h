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

#ifndef _VMLOG_H_
#define _VMLOG_H_

#define VMLOG_PAD_TO_8

#include <limits.h>

/*** constants *******************************************************/

/* CAUTION: these are indices into the vmlog_tag_definitions table! */
#define VMLOG_TAG_ENTER  0
#define VMLOG_TAG_LEAVE  1
#define VMLOG_TAG_THROW  2
#define VMLOG_TAG_CATCH  3
#define VMLOG_TAG_UNWND  4
#define VMLOG_TAG_SIGNL  5
#define VMLOG_TAG_UNROL  6
#define VMLOG_TAG_REROL  7

/*** memory management macros ****************************************/

#ifndef VMLOG_NEW
#define VMLOG_NEW(type)                (type*)malloc(sizeof(type))
#endif

#ifndef VMLOG_FREE
#define VMLOG_FREE(type,ptr)           free(ptr)
#endif

#ifndef VMLOG_NEW_ARRAY
#define VMLOG_NEW_ARRAY(type,n)        (type*)malloc(sizeof(type)*(n))
#endif

#ifndef VMLOG_FREE_ARRAY
#define VMLOG_FREE_ARRAY(type,n,ptr)   free(ptr)
#endif

/*** struct typedefs *************************************************/

typedef struct vmlog_tag_definition vmlog_tag_definition;
typedef struct vmlog_hash_entry vmlog_hash_entry;
typedef struct vmlog_hash_table vmlog_hash_table;
typedef struct vmlog_string_entry vmlog_string_entry;
typedef struct vmlog_log_entry vmlog_log_entry;
typedef struct vmlog_thread_log vmlog_thread_log;
typedef struct vmlog_file vmlog_file;
typedef struct vmlog_frame vmlog_frame;
typedef struct vmlog_ringbuf vmlog_ringbuf;
typedef struct vmlog_log vmlog_log;
typedef struct vmlog_options vmlog_options;

/*** integer types ***************************************************/

typedef long long vmlog_fofs_t;
typedef long long vmlog_seq_t;
#define VMLOG_SEQ_FMT   "%lld"
#define VMLOG_SEQ_FMT_W "%10lld"
#ifdef LLONG_MAX
#define VMLOG_SEQ_MAX LLONG_MAX
#else
#define VMLOG_SEQ_MAX 9223372036854775807LL
#endif

#ifndef VMLOG_HAVE_PTRINT
typedef int ptrint; /* XXX */
#endif

/*** enums ***********************************************************/

typedef enum {
	vmlogRead,
	vmlogAppend,
	vmlogTruncateAppend
} vmlog_fmode;

/*** function types **************************************************/

typedef void (*vmlog_hash_entry_destructor)(vmlog_hash_entry *entry);
typedef void (*vmlog_log_function)(vmlog_log *vml,void *threadid,
				   const char *name,int namelen);

/*** structs *********************************************************/

struct vmlog_tag_definition {
	char             *name;
	char             *fixname;
	int               depth;
};

struct vmlog_hash_entry {
	unsigned int      hash;
	int               len;
	int               index;
	void             *data;
	vmlog_hash_entry *hashlink;
};

struct vmlog_hash_table {
	int               size;
	int               nentries;
	vmlog_hash_entry *table;
};

struct vmlog_string_entry {
	vmlog_fofs_t ofs;
	int          len;
#if defined(VMLOG_PAD_TO_8)
	int          dummy;
#endif
};

struct vmlog_log_entry {
	int          tag:8;
	int          index:24;
};

struct vmlog_file {
	int          fd;
	char        *fname;
	int          fnamelen;
	vmlog_fofs_t ofs;
	vmlog_fofs_t size;
};

struct vmlog_frame {
	int                index;
	vmlog_seq_t        seq;
};

struct vmlog_thread_log {
	void              *threadid;
	int                threadidx;
	int                depth;
	int                ignoredepth;
	vmlog_file         logfile;
	vmlog_log_entry   *logbuf;
	vmlog_log_entry   *logbufend;
	vmlog_log_entry   *logbufptr;
	int                logbufcap;
	vmlog_frame       *frames;
	int                framescap;
	vmlog_seq_t        seq;
};

struct vmlog_log {
	vmlog_hash_table   threadhash;
	vmlog_hash_table   stringhash;
	vmlog_file         idxfile;
	vmlog_file         strfile;
	char              *prefix;
	int                prefixlen;
	int                ignorelistlen;
};

struct vmlog_ringbuf {
	vmlog_file         file;
	vmlog_log_entry   *buf;
	vmlog_log_entry   *bufend;
	vmlog_log_entry   *start;
	vmlog_log_entry   *end;
	vmlog_log_entry   *cur;
	int                bufsize;
	vmlog_seq_t        seq;

	int                debug_availbefore;
	int                debug_availafter;
};

struct vmlog_options {
	char              *progname;
	char              *prefix;
	char              *stringprefix;
	char              *ignoreprefix;
};

/*** variables *******************************************************/

extern vmlog_tag_definition vmlog_tag_definitions[];

/*** public functions ************************************************/

/* constructor / destructor */
vmlog_log * vmlog_log_new(const char *prefix,int truncate);
void vmlog_log_free(vmlog_log *vml);

/* configuration */
void vmlog_log_load_ignorelist(vmlog_log *vml,const char *prefix);

/* logging */
void vmlog_log_enter(vmlog_log *vml,void *threadid,const char *name,int namelen);
void vmlog_log_leave(vmlog_log *vml,void *threadid,const char *name,int namelen);
void vmlog_log_throw(vmlog_log *vml,void *threadid,const char *name,int namelen);
void vmlog_log_catch(vmlog_log *vml,void *threadid,const char *name,int namelen);
void vmlog_log_unwnd(vmlog_log *vml,void *threadid,const char *name,int namelen);
void vmlog_log_signl(vmlog_log *vml,void *threadid,const char *name,int namelen);
void vmlog_log_unrol(vmlog_log *vml,void *threadid,const char *name,int namelen);
void vmlog_log_rerol(vmlog_log *vml,void *threadid,const char *name,int namelen);

/* tag definitions */
int vmlog_tag_from_name(const char *name,int namelen);

/* thread logging */
vmlog_thread_log *vmlog_thread_log_new(vmlog_log *vml,void *threadid,int index);
void vmlog_thread_log_free(vmlog_thread_log *tlog);
vmlog_frame * vmlog_thread_log_enter(vmlog_thread_log *tlog,int index,vmlog_seq_t seq);
vmlog_frame * vmlog_thread_log_leave(vmlog_thread_log *tlog,int index,vmlog_seq_t seq);

/* string/index handling */
int vmlog_get_string_index(vmlog_log *vml,const char *data,int len);
void vmlog_load_stringhash(vmlog_log *vml,const char *prefix);

/* string helpers */
char *vmlog_concat3(const char *a,const char *b,const char *c,int *plen);
char *vmlog_concat4len(const char *a,int alen,const char *b,int blen,
		       const char *c,int clen,const char *d,int dlen,int *plen);

/* error reporting */
void vmlog_set_progname(const char *progname);
void vmlog_die(const char *fmt,...);
void vmlog_warn(const char *fmt,...);
void vmlog_die_usage(const char *usage,int error);

/* file handling */
void vmlog_file_open(vmlog_file *file,const char *fname,vmlog_fmode fmode);
void vmlog_file_close(vmlog_file *file);
void vmlog_file_append(vmlog_file *file,const void *data,int len);
void * vmlog_file_mmap(const char *fname,int *plen);
void vmlog_file_munmap(void *m,int len);
void vmlog_file_stat(vmlog_file *file);
void vmlog_file_seek(vmlog_file *file,vmlog_fofs_t ofs);

/* log entry ring buffer */
vmlog_ringbuf * vmlog_ringbuf_new(const char *fname,int bufsize);
void vmlog_ringbuf_free(vmlog_ringbuf *ring);
void vmlog_ringbuf_seek(vmlog_ringbuf *ring,vmlog_seq_t seq);
int vmlog_ringbuf_fill(vmlog_ringbuf *ring,int len);
vmlog_log_entry * vmlog_ringbuf_next(vmlog_ringbuf *ring,int prefetch);
vmlog_log_entry * vmlog_ringbuf_prev(vmlog_ringbuf *ring,int prefetch);

/* option parsing */
int vmlog_opt_parse_seq(const char *arg,int len,vmlog_seq_t *seq);
int vmlog_opt_parse_range(const char *arg,vmlog_seq_t *start,vmlog_seq_t *end);
vmlog_options *vmlog_opt_parse_cmd_line(int *pargc,char **argv);
void vmlog_opt_free(vmlog_options *opts);

/*** memory allocation helpers ***************************************/

/* allocate, check, and zero memory */
#define VMLOG_XZNEW(var,type) \
	do { var = VMLOG_NEW(type); \
	     if (!(var)) vmlog_die("out of memory"); \
	     memset((var),0,sizeof(type)); \
	} while (0)

/* allocate, check, and zero memory for array */
#define VMLOG_XZNEW_ARRAY(var,type,n) \
	do { var = VMLOG_NEW_ARRAY(type,n); \
	     if (!(var)) vmlog_die("out of memory"); \
	     memset((var),0,(n)*sizeof(type)); \
	} while (0)

#endif

/* vim: noet ts=8 sw=8
 */

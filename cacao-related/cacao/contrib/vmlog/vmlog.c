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

#include "vmlog.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <jni.h>

/*** default macros **************************************************/

#ifndef VMLOG_LOCK
#define VMLOG_LOCK(vml)
#define VMLOG_UNLOCK(vml)
#endif

/* #define VMLOG_ENDIAN_CONVERT_WRITE */
#define VMLOG_HOST_LITTLE_ENDIAN

/*** constants *******************************************************/

/* currently vmlog does no rehashing, so these should be quite big */
#define VMLOG_INITIAL_STRING_HASH_SIZE       50000 /* XXX debug */
#define VMLOG_INITIAL_THREAD_HASH_SIZE       8 /* XXX debug */

/* initial size of the frame buffer - this is doubled each time    */
/* the frame buffer has to grow                                    */
#define VMLOG_INITIAL_FRAMES_CAPACITY        1 /* XXX debug */

/*** types ***********************************************************/

/* we declare this here because defining _LARGEFILE64_SOURCE works */
/* only if we are the first ones to include the headers, which may */
/* not be the case if vmlog.c is used as an include file.          */

#ifndef _LARGEFILE64_SOURCE
typedef long long off64_t;
off64_t lseek64(int fd, off64_t offset, int whence);
#endif

/*** tag definitions *************************************************/

/* CAUTION: these must are indexed by the VMLOG_TAG_... constants! */
vmlog_tag_definition vmlog_tag_definitions[] = {
	{ "enter", "enter", +1 },
	{ "leave", "leave", -1 },
	{ "throw", "throw",  0 },
	{ "catch", "catch",  0 },
	{ "unwnd", "unwnd", -1 },
	{ "signl", "signl",  0 },
	{ "unrol", "unrol", -1 },
	{ "rerol", "rerol", +1 },
	{ NULL   , NULL   ,  0 }
};

/*** global variables ************************************************/

static char *vmlog_progname = "vmlog";

/*** prototypes ******************************************************/

static void *vmlog_memdup(const void *m,int len);

/*** error reporting *************************************************/

void vmlog_set_progname(const char *progname)
{
	if (!progname) {
		progname = "vmlog (progname == NULL)";
	}

	vmlog_progname = vmlog_memdup(progname,strlen(progname)+1);
}

void vmlog_die(const char *fmt,...)
{
	va_list ap;

	fputs(vmlog_progname,stderr);
	fputs(": error: ",stderr);
	va_start(ap,fmt);
	vfprintf(stderr,fmt,ap);
	va_end(ap);
	fputc('\n',stderr);
	exit(1);
}

void vmlog_warn(const char *fmt,...)
{
	va_list ap;

	fputs(vmlog_progname,stderr);
	fputs(": warning: ",stderr);
	va_start(ap,fmt);
	vfprintf(stderr,fmt,ap);
	va_end(ap);
	fputc('\n',stderr);
}

void vmlog_die_usage(const char *usage,int error)
{
	assert(usage);
	
	fputs(usage,(error) ? stderr : stdout);
	exit((error) ? 1 : 0);
}

/*** utility functions ***********************************************/

static void *vmlog_memdup(const void *data,int len)
{
	char *p;

	p = VMLOG_NEW_ARRAY(char,len);
	assert(p);
	memcpy(p,data,len);

	return p;
}

static void *vmlog_strdup(const void *data,int len)
{
	char *p;

	p = VMLOG_NEW_ARRAY(char,len+1);
	assert(p);
	memcpy(p,data,len);
	p[len] = 0;

	return p;
}

char *vmlog_concat4len(const char *a,int alen,const char *b,int blen,
		       const char *c,int clen,const char *d,int dlen,
		       int *plen)
{
	int len;
	char *p;
	char *pp;

	assert(a);
	assert(b);
	assert(c);
	assert(d);

	len = alen + blen + clen + dlen;
	if (plen)
		*plen = len;

	p = VMLOG_NEW_ARRAY(char,len+1);
	pp = p;
	memcpy(pp,a,alen); pp += alen;
	memcpy(pp,b,blen); pp += blen;
	memcpy(pp,c,clen); pp += clen;
	memcpy(pp,d,dlen); pp += dlen;
	*pp = 0;

	return p;
}

char *vmlog_concat3(const char *a,const char *b,const char *c,int *plen)
{
	int len,lena,lenb,lenc;
	char *p;
	char *pp;

	assert(a);
	assert(b);
	assert(c);

	lena = strlen(a);
	lenb = strlen(b);
	lenc = strlen(c);

	len = lena + lenb + lenc;
	if (plen)
		*plen = len;

	p = VMLOG_NEW_ARRAY(char,len+1);
	pp = p;
	memcpy(pp,a,lena); pp += lena;
	memcpy(pp,b,lenb); pp += lenb;
	memcpy(pp,c,lenc); pp += lenc;
	*pp = 0;

	return p;
}

/*** file ops ********************************************************/

void vmlog_file_open(vmlog_file *file,const char *fname,vmlog_fmode fmode)
{
	int r;
	struct stat st;
	int flags = 0;
	
	assert(file);

	switch (fmode) {
		case vmlogRead:
			flags = O_RDONLY;
			break;

		case vmlogAppend:
			flags = O_WRONLY | O_APPEND | O_CREAT;
			break;

		case vmlogTruncateAppend:
			flags = O_WRONLY | O_APPEND | O_CREAT | O_TRUNC;
			break;

		default:
			vmlog_die("unknown fmode for opening file: %s: %d",
				fname,fmode);
	}

	r = open(fname,flags,0644);
	if (r == -1) {
		vmlog_die("could not open file: %s: %s",fname,strerror(errno));
	}
	file->fd = r;
	file->fnamelen = strlen(fname);
	file->fname = vmlog_memdup(fname,file->fnamelen+1);

	r = fstat(file->fd,&st);
	if (r == -1) {
		vmlog_die("could not stat file: %s: %s",fname,strerror(errno));
	}
	file->ofs = (fmode == vmlogRead) ? 0 : st.st_size;
}

void vmlog_file_close(vmlog_file *file)
{
	assert(file);

	if (file->fd == -1)
		return;

	close(file->fd);
	file->fd = -1;
	VMLOG_FREE_ARRAY(char,file->fnamelen+1,file->fname);
	file->fname = NULL;
	file->fnamelen = 0;
}

void vmlog_file_append(vmlog_file *file,const void *data,int len)
{
	int r;

	assert(len >= 0);
	if (!len)
		return;
	assert(data);
	
	do {
		r = write(file->fd,data,len);
	} while (r == -1 && errno == EINTR);

	if (r == -1) {
		vmlog_die("could not write to file: %s: %s",file->fname,strerror(errno));
	}

	if (r != len) {
		vmlog_die("could not write all data to file: %s",file->fname);
	}

	file->ofs += len;
}

void vmlog_file_stat(vmlog_file *file)
{
	int r;
	struct stat st;
	
	r = fstat(file->fd,&st);
	if (r == -1)
		vmlog_die("could not stat file: %s: %s",file->fname,strerror(errno));

	file->size = st.st_size;
}

void * vmlog_file_mmap(const char *fname,int *plen)
{
	int fd;
	int r;
	struct stat st;
	void *m;

	fd = open(fname,O_RDONLY);
	if (fd == -1)
		vmlog_die("could not open file: %s: %s",fname,strerror(errno));
	
	r = fstat(fd,&st);
	if (r == -1)
		vmlog_die("could not stat file: %s: %s",fname,strerror(errno));

	if (plen)
		*plen = st.st_size;
	
	if (st.st_size) {
		m = mmap(NULL,st.st_size,PROT_READ,MAP_PRIVATE,fd,0);
		if (m == MAP_FAILED) {
			vmlog_die("could not mmap file: %s: %s",fname,strerror(errno));
		}
	}
	else {
		/* fake a pointer */
		m = VMLOG_NEW(char);
	}

	close(fd);

	return m;
}

void vmlog_file_munmap(void *m,int len)
{
	int r;
	
	if (len) {
		r = munmap(m,len);
		if (r != 0)
			vmlog_warn("could not munmap file: %s",strerror(errno));
	}
	else {
		VMLOG_FREE(char,m);
	}
}

void vmlog_file_seek(vmlog_file *file,vmlog_fofs_t ofs)
{
	off64_t r;

	r = lseek64(file->fd,ofs,SEEK_SET);
	if (r == (off64_t)-1)
		vmlog_die("could not seek position in file: %s: %s",
			file->fname,strerror(errno));
	file->ofs = ofs;
}

/*** string storage **************************************************/

static void vmlog_add_string(vmlog_log *vml,const char *data,int len)
{
	vmlog_string_entry strent;
#if defined(VMLOG_ENDIAN_CONVERT_WRITE)
	vmlog_fofs_t tmp;
#endif
	
	assert(vml);

	if (vml->strfile.fd == -1)
		return;
	if (vml->idxfile.fd == -1)
		return;

	strent.ofs = vml->strfile.ofs;
	strent.len = len;

#if defined(VMLOG_ENDIAN_CONVERT_WRITE)
#if defined(VMLOG_HOST_LITTLE_ENDIAN)
	tmp = ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[0]) << 56)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[1]) << 48)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[2]) << 40)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[3]) << 32)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[4]) << 24)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[5]) << 16)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[6]) <<  8)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[7]) <<  0);
	strent.ofs = tmp;
	tmp = ((vmlog_fofs_t)(((unsigned char*)&strent.len)[0]) << 24)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.len)[1]) << 16)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.len)[2]) <<  8)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.len)[3]) <<  0);
	strent.len = tmp;
#else
	tmp = ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[7]) << 56)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[6]) << 48)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[5]) << 40)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[4]) << 32)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[3]) << 24)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[2]) << 16)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[1]) <<  8)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.ofs)[0]) <<  0);
	strent.ofs = tmp;
	tmp = ((vmlog_fofs_t)(((unsigned char*)&strent.len)[3]) << 24)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.len)[2]) << 16)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.len)[1]) <<  8)
	    | ((vmlog_fofs_t)(((unsigned char*)&strent.len)[0]) <<  0);
	strent.len = tmp;
#endif
#endif /* defined(VMLOG_ENDIAN_CONVERT_WRITE) */
	
	vmlog_file_append(&(vml->strfile),data,len);
	vmlog_file_append(&(vml->idxfile),&strent,sizeof(vmlog_string_entry));
}

/*** index functions *************************************************/

static int vmlog_is_ignored(vmlog_log *vml,int index)
{
	return (index < vml->ignorelistlen);
}

/*** thread log functions ********************************************/

static void vmlog_thread_log_alloc_logbuf(vmlog_thread_log *tlog,int cap)
{
	assert(tlog);
	assert(cap >= 0);

	if (cap) {
		VMLOG_XZNEW_ARRAY(tlog->logbuf,vmlog_log_entry,cap);
	}
	else {
		tlog->logbuf = NULL;
	}
	tlog->logbufptr = tlog->logbuf;
	tlog->logbufend = tlog->logbuf + cap;
	tlog->logbufcap = cap;
}

static void vmlog_thread_log_flush(vmlog_thread_log *tlog)
{
	assert(tlog);
	assert(tlog->logbuf);

	vmlog_file_append(&(tlog->logfile),tlog->logbuf,
			(tlog->logbufptr - tlog->logbuf) * sizeof(vmlog_log_entry));

	tlog->logbufptr = tlog->logbuf;
}

static void vmlog_thread_log_realloc_frames(vmlog_thread_log *tlog,int cap)
{
	vmlog_frame *oldframes;

	assert(tlog);
	assert(cap >= tlog->depth);

	oldframes = tlog->frames;

	if (cap) {
		VMLOG_XZNEW_ARRAY(tlog->frames,vmlog_frame,cap);
	}
	else {
		tlog->frames = NULL;
	}
	
	if (oldframes) {
		if (tlog->frames) {
			memcpy(tlog->frames,oldframes,sizeof(vmlog_frame) * tlog->depth);
		}
		VMLOG_FREE_ARRAY(vmlog_frame,tlog->framescap,oldframes);
	}
	tlog->framescap = cap;
}

static void vmlog_thread_log_append(vmlog_thread_log *tlog,vmlog_log_entry *logent)
{
#if defined(VMLOG_ENDIAN_CONVERT_WRITE)
	unsigned int tmp;

#if defined(VMLOG_HOST_LITTLE_ENDIAN)
	tmp = ((unsigned int)(((unsigned char*)logent)[3]) <<  0)
	    | ((unsigned int)(((unsigned char*)logent)[2]) <<  8)
	    | ((unsigned int)(((unsigned char*)logent)[1]) << 16);
#else
	tmp = ((unsigned int)(((unsigned char*)logent)[1]) <<  0)
	    | ((unsigned int)(((unsigned char*)logent)[2]) <<  8)
	    | ((unsigned int)(((unsigned char*)logent)[3]) << 16);
#endif
	logent->index = tmp;
#endif /* defined(VMLOG_ENDIAN_CONVERT_WRITE) */
	if (tlog->logbufptr) {
		if (tlog->logbufptr == tlog->logbufend) {
			vmlog_thread_log_flush(tlog);
		}
		*tlog->logbufptr++ = *logent;
	}
	else {
		vmlog_file_append(&(tlog->logfile),logent,sizeof(vmlog_log_entry));
	}
}

#define VMLOG_INT2STR_BUFFER 20

vmlog_thread_log *vmlog_thread_log_new(vmlog_log *vml,void *threadid,int index)
{
	vmlog_thread_log *tlog;
	char buf[VMLOG_INT2STR_BUFFER];
	int r;
	char *name;
	int namelen;

	VMLOG_XZNEW(tlog,vmlog_thread_log);

	tlog->threadid = threadid;
	tlog->threadidx = index;
	tlog->logfile.fd = -1;

	vmlog_thread_log_realloc_frames(tlog,VMLOG_INITIAL_FRAMES_CAPACITY);

	if (vml && vml->prefix) {
		r = snprintf(buf,VMLOG_INT2STR_BUFFER,"%d",index);
		assert(r < VMLOG_INT2STR_BUFFER);
		buf[VMLOG_INT2STR_BUFFER-1] = 0;
		name = vmlog_concat4len(vml->prefix,vml->prefixlen,
				        ".",1,
				        buf,strlen(buf),
				        ".log",4,
				        &namelen);
		vmlog_file_open(&(tlog->logfile),name,vmlogTruncateAppend);
		VMLOG_FREE_ARRAY(char,namelen+1,name);
	}

	return tlog;
}

void vmlog_thread_log_free(vmlog_thread_log *tlog)
{
	if (!tlog)
		return;

	if (tlog->logbuf)
		vmlog_thread_log_flush(tlog);
	
	vmlog_file_close(&(tlog->logfile));
	
	if (tlog->frames) {
		VMLOG_FREE_ARRAY(vmlog_frame,tlog->framescap,tlog->frames);
	}
	VMLOG_FREE(vmlog_thread_log,tlog);
}

vmlog_frame * vmlog_thread_log_enter(vmlog_thread_log *tlog,int index,vmlog_seq_t seq)
{
	vmlog_frame *frame;

	if (tlog->depth < 0) {
		vmlog_warn("negative call frame depth %d at seq " VMLOG_SEQ_FMT,
				tlog->depth,seq);
		return NULL;
	}
	
	if (tlog->depth >= tlog->framescap)
		vmlog_thread_log_realloc_frames(tlog,tlog->framescap * 2);

	frame = tlog->frames + tlog->depth;

	frame->index = index;
	frame->seq = seq;

	tlog->depth++;

	return frame;
}

vmlog_frame * vmlog_thread_log_leave(vmlog_thread_log *tlog,int index,vmlog_seq_t seq)
{
	vmlog_frame *frame;

	if (--tlog->depth < 0) {
		vmlog_warn("negative call frame depth %d at seq " VMLOG_SEQ_FMT,
				tlog->depth,seq);
		return NULL;
	}
	
	frame = tlog->frames + tlog->depth;

	if (index != frame->index)
		vmlog_warn("mismatched leave at seq " VMLOG_SEQ_FMT 
				": entered index %d, left index %d",
			seq,frame->index,index);

	return frame;
}

/*** tag definitions *************************************************/

/* RETURNS                                                           */
/*     the tag number, or -1 if the tag name is invalid              */

int vmlog_tag_from_name(const char *name,int namelen)
{
	vmlog_tag_definition *td;
	int i;
	
	if (!name || namelen < 1)
		return -1;

	td = vmlog_tag_definitions;
	i = 0;
	while (td->name) {
		if (namelen == strlen(td->name)
		    && strncmp(td->name,name,namelen) == 0) 
		{
			return i;
		}
		td++;
		i++;
	}

	return -1;
}

/*** hash functions **************************************************/

static unsigned int vmlog_thread_hash(void *threadid) 
{
	/* XXX use a better hash function? */
	return (unsigned int)(ptrint)threadid;
}

static unsigned int vmlog_string_hash(const char *data,int len) 
{
	register const unsigned char *p = (const unsigned char *) data;
	register unsigned int hash;
	register int i;

	/* The algorithm is the "One-at-a-time" algorithm as published    */
	/* by Bob Jenkins on http://burtleburtle.net/bob/hash/doobs.html. */

	hash = 0;
	for (i=len; i--;)
	{
	    hash += *p++;
	    hash += (hash << 10);
	    hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

/*** hash tables *****************************************************/

static vmlog_thread_log *vmlog_get_thread_log(vmlog_log *vml,void *threadid)
{
	unsigned int h;
	vmlog_hash_entry *preventry = NULL;
	vmlog_hash_entry *entry;
	vmlog_thread_log *tlog;
	
	assert(vml);

	h = vmlog_thread_hash(threadid);
	entry = vml->threadhash.table + (h % vml->threadhash.size);
	do {
		tlog = (vmlog_thread_log *)entry->data;
		if (tlog && tlog->threadid == threadid)
			return tlog;
		preventry = entry;
		entry = entry->hashlink;
	} while (entry);

	/* this is a new threadid */
	tlog = vmlog_thread_log_new(vml,threadid,vml->threadhash.nentries++);
	
	assert(preventry);
	if (preventry->data) {
		VMLOG_XZNEW(entry,vmlog_hash_entry);

		preventry->hashlink = entry;
	}
	else {
		entry = preventry;
	}

	entry->data = tlog;

	/* XXX maybe rehash */

	return tlog;
}

int vmlog_get_string_index(vmlog_log *vml,const char *data,int len)
{
	unsigned int hash;
	vmlog_hash_entry *entry;
	vmlog_hash_entry *preventry = NULL;
	
	assert(vml);
	assert(data);
	assert(len >= 0);

	hash = vmlog_string_hash(data,len);
	entry = vml->stringhash.table + (hash % vml->stringhash.size);
	do {
		if (entry->len == len && entry->data && memcmp(data,entry->data,len) == 0)
			return entry->index;
		preventry = entry;
		entry = entry->hashlink;
	} while (entry);

	/* this is a new string */
	assert(preventry);
	if (preventry->data) {
		VMLOG_XZNEW(entry,vmlog_hash_entry);

		preventry->hashlink = entry;
	}
	else {
		entry = preventry;
	}

	entry->data = vmlog_memdup(data,len);
	entry->len = len;
	entry->index = vml->stringhash.nentries++;
	vmlog_add_string(vml,data,len);

	return entry->index;
}

static void vmlog_hashtable_init(vmlog_hash_table *ht,int size)
{
	assert(ht);
	assert(size > 0);
	
	ht->size = size;
	VMLOG_XZNEW_ARRAY(ht->table,vmlog_hash_entry,size);
	ht->nentries = 0;
}

static void vmlog_hashtable_free(vmlog_hash_table *ht,vmlog_hash_entry_destructor destr)
{
	int i;
	vmlog_hash_entry *entry,*next;
	
	assert(ht);

	for (i=0; i<ht->size; ++i) {
		entry = ht->table + i;
		if (destr)
			destr(entry);

		next = entry->hashlink;
		while (next) {
			entry = next;
			if (destr)
				destr(entry);
			next = entry->hashlink;
			VMLOG_FREE(vmlog_hash_entry,entry);
		}
	}

	VMLOG_FREE_ARRAY(vmlog_hash_entry,ht->size,ht->table);
	memset(ht,0,sizeof(vmlog_hash_table));
}

static void vmlog_thread_log_destructor(vmlog_hash_entry *entry)
{
	vmlog_thread_log *tlog;
	
	assert(entry);

	tlog = (vmlog_thread_log *)entry->data;
	vmlog_thread_log_free(tlog);
}

static void vmlog_string_destructor(vmlog_hash_entry *entry)
{
	char *str;

	assert(entry);

	str = (char *)entry->data;
	if (str) {
		VMLOG_FREE_ARRAY(char,entry->len,str);
	}
}

static void vmlog_open_string_files(vmlog_log *vml,int truncate)
{
	char *name;
	int namelen;
	int fmode;

	if (!vml->prefix)
		return;

	fmode = (truncate) ? vmlogTruncateAppend : vmlogAppend;

	name = vmlog_concat3(vml->prefix,"",".idx",&namelen);
	vmlog_file_open(&(vml->idxfile),name,fmode);
	VMLOG_FREE_ARRAY(char,namelen+1,name);

	name = vmlog_concat3(vml->prefix,"",".str",&namelen);
	vmlog_file_open(&(vml->strfile),name,fmode);
	VMLOG_FREE_ARRAY(char,namelen+1,name);
}

void vmlog_load_stringhash(vmlog_log *vml,const char *prefix)
{
	int n;
	vmlog_string_entry *idxmap;
	vmlog_string_entry *strent;
	char *strmap;
	int idxlen;
	int strlen;
	char *idxfname;
	char *strfname;
	int idxnamelen;
	int strnamelen;
	
	assert(vml);
	assert(prefix);

	idxfname = vmlog_concat3(prefix,".","idx",&idxnamelen);
	strfname = vmlog_concat3(prefix,".","str",&strnamelen);
	
	vmlog_hashtable_free(&(vml->stringhash),vmlog_string_destructor);
	vmlog_hashtable_init(&(vml->stringhash),VMLOG_INITIAL_STRING_HASH_SIZE);

	vmlog_file_close(&(vml->idxfile));
	vmlog_file_close(&(vml->strfile));
	vmlog_open_string_files(vml,1);

	idxmap = vmlog_file_mmap(idxfname,&idxlen);
	strmap = vmlog_file_mmap(strfname,&strlen);

	n = idxlen / sizeof(vmlog_string_entry);
	strent = idxmap;
	while (n--) {
		vmlog_get_string_index(vml,strmap + strent->ofs,strent->len);
		strent++;
	}

	vmlog_file_munmap(idxmap,idxlen);
	vmlog_file_munmap(strmap,strlen);
	
	VMLOG_FREE_ARRAY(char,idxnamelen+1,idxfname);
	VMLOG_FREE_ARRAY(char,strnamelen+1,strfname);
}

/*** public functions ************************************************/

vmlog_log * vmlog_log_new(const char *prefix,int truncate)
{
	vmlog_log *vml;

	VMLOG_XZNEW(vml,vmlog_log);

	vml->idxfile.fd = -1;
	vml->strfile.fd = -1;
	vmlog_hashtable_init(&(vml->stringhash),VMLOG_INITIAL_STRING_HASH_SIZE);
	vmlog_hashtable_init(&(vml->threadhash),VMLOG_INITIAL_THREAD_HASH_SIZE);
	
	if (prefix) {
		vml->prefixlen = strlen(prefix);
		vml->prefix = vmlog_memdup(prefix,vml->prefixlen+1);

		vmlog_open_string_files(vml,truncate);
	}

	return vml;
}

void vmlog_log_free(vmlog_log *vml)
{
	if (!vml)
		return;

	VMLOG_FREE_ARRAY(char,vml->prefixlen+1,vml->prefix);
	vml->prefix = NULL;
	vml->prefixlen = 0;	

	vmlog_hashtable_free(&(vml->threadhash),vmlog_thread_log_destructor);
	vmlog_hashtable_free(&(vml->stringhash),vmlog_string_destructor);

	vmlog_file_close(&(vml->idxfile));
	vmlog_file_close(&(vml->strfile));

	VMLOG_FREE(vmlog_log,vml);
}

static void vmlog_log_enter_tag(vmlog_log *vml,void *threadid,int tag,const char *name,int namelen)
{
	vmlog_thread_log *tlog;
	int index;
	vmlog_log_entry logent;
	
	assert(vml);
	assert(name);
	assert(namelen >= 0);

	VMLOG_LOCK();
	tlog = vmlog_get_thread_log(vml,threadid);
	index = vmlog_get_string_index(vml,name,namelen);
	VMLOG_UNLOCK();

	if (tlog->ignoredepth) {
		tlog->ignoredepth++;
		return;
	}

	if (vmlog_is_ignored(vml,index)) {
		tlog->ignoredepth++;
		return;
	}
	
	logent.tag = tag;
	logent.index = index;
	vmlog_thread_log_append(tlog,&logent);

	tlog->seq++;
}

static void vmlog_log_leave_tag(vmlog_log *vml,void *threadid,int tag,const char *name,int namelen)
{
	vmlog_thread_log *tlog;
	int index;
	vmlog_log_entry logent;
	
	assert(vml);
	assert(name);
	assert(namelen >= 0);
	
	VMLOG_LOCK();
	tlog = vmlog_get_thread_log(vml,threadid);
	index = vmlog_get_string_index(vml,name,namelen);
	VMLOG_UNLOCK();

	if (tlog->ignoredepth) {
		tlog->ignoredepth--;
		return;
	}
	
	logent.tag = tag;
	logent.index = index;
	vmlog_thread_log_append(tlog,&logent);

	tlog->seq++;
}

void vmlog_log_enter(vmlog_log *vml,void *threadid,const char *name,int namelen)
{
	vmlog_log_enter_tag(vml,threadid,VMLOG_TAG_ENTER,name,namelen);
}

void vmlog_log_rerol(vmlog_log *vml,void *threadid,const char *name,int namelen)
{
	vmlog_log_enter_tag(vml,threadid,VMLOG_TAG_REROL,name,namelen);
}

void vmlog_log_leave(vmlog_log *vml,void *threadid,const char *name,int namelen)
{
	vmlog_log_leave_tag(vml,threadid,VMLOG_TAG_LEAVE,name,namelen);
}

void vmlog_log_unrol(vmlog_log *vml,void *threadid,const char *name,int namelen)
{
	vmlog_log_leave_tag(vml,threadid,VMLOG_TAG_UNROL,name,namelen);
}

void vmlog_log_throw(vmlog_log *vml,void *threadid,const char *name,int namelen)
{
	vmlog_thread_log *tlog;
	int index;
	vmlog_log_entry logent;
	
	assert(vml);
	assert(name);
	assert(namelen >= 0);
	
	VMLOG_LOCK();
	tlog = vmlog_get_thread_log(vml,threadid);
	index = vmlog_get_string_index(vml,name,namelen);
	VMLOG_UNLOCK();

	if (tlog->ignoredepth)
		return;
	
	logent.tag = VMLOG_TAG_THROW;
	logent.index = index;
	vmlog_thread_log_append(tlog,&logent);

	tlog->seq++;
}

void vmlog_log_catch(vmlog_log *vml,void *threadid,const char *name,int namelen)
{
	vmlog_thread_log *tlog;
	int index;
	vmlog_log_entry logent;
	
	assert(vml);
	assert(name);
	assert(namelen >= 0);
	
	VMLOG_LOCK();
	tlog = vmlog_get_thread_log(vml,threadid);
	index = vmlog_get_string_index(vml,name,namelen);
	VMLOG_UNLOCK();

	if (tlog->ignoredepth)
		return;
	
	logent.tag = VMLOG_TAG_CATCH;
	logent.index = index;
	vmlog_thread_log_append(tlog,&logent);

	tlog->seq++;
}

void vmlog_log_unwnd(vmlog_log *vml,void *threadid,const char *name,int namelen)
{
	vmlog_thread_log *tlog;
	int index;
	vmlog_log_entry logent;
	
	assert(vml);
	assert(name);
	assert(namelen >= 0);
	
	VMLOG_LOCK();
	tlog = vmlog_get_thread_log(vml,threadid);
	index = vmlog_get_string_index(vml,name,namelen);
	VMLOG_UNLOCK();

	if (tlog->ignoredepth) {
		tlog->ignoredepth--;
		return;
	}
	
	logent.tag = VMLOG_TAG_UNWND;
	logent.index = index;
	vmlog_thread_log_append(tlog,&logent);

	tlog->seq++;
}

void vmlog_log_signl(vmlog_log *vml,void *threadid,const char *name,int namelen)
{
	vmlog_thread_log *tlog;
	int index;
	vmlog_log_entry logent;
	
	assert(vml);
	assert(name);
	assert(namelen >= 0);
	
	VMLOG_LOCK();
	tlog = vmlog_get_thread_log(vml,threadid);
	index = vmlog_get_string_index(vml,name,namelen);
	VMLOG_UNLOCK();

	logent.tag = VMLOG_TAG_SIGNL;
	logent.index = index;
	vmlog_thread_log_append(tlog,&logent);

	tlog->seq++;
}

void vmlog_log_load_ignorelist(vmlog_log *vml,const char *prefix)
{
	assert(vml);
	assert(prefix);

	vmlog_load_stringhash(vml,prefix);
	vml->ignorelistlen = vml->stringhash.nentries;
}

/*** ring buffer functions *******************************************/

static void vmlog_ringbuf_visualize(vmlog_ringbuf *ring)
{
	int i;

	fprintf(stdout,"vmlog_ringbuf %p: bufsize=%d availbefore=%d availafter=%d\n",
			(void*)ring,ring->bufsize,
			ring->debug_availbefore,ring->debug_availafter);

	for (i=0; i<=ring->bufsize; ++i) {
		if (i == ring->bufsize) {
			fprintf(stdout,"%3d: xxxxxxxxxxxxx",i);
		}
		else {
			fprintf(stdout,"%3d: %2d %10d",i,ring->buf[i].tag,ring->buf[i].index);
		}
		if (ring->start - ring->buf == i) fputs(" start",stdout);
		if (ring->cur   - ring->buf == i) fputs(" cur",stdout);
		if (ring->end   - ring->buf == i) fputs(" end",stdout);
		if (ring->cur   - ring->buf == i) fprintf(stdout," (" VMLOG_SEQ_FMT ")",ring->seq);
		fputc('\n',stdout);
	}	
}

static void vmlog_ringbuf_check_invariants(vmlog_ringbuf *ring)
{
	/* vmlog_ringbuf_visualize(ring); */
	
	assert(ring);

	assert(ring->bufsize > 0);
	assert(ring->bufend == ring->buf + ring->bufsize);

	assert(ring->start >= ring->buf && ring->start < ring->bufend);
	assert((ring->end > ring->buf && ring->end <= ring->bufend)
			||
	       (ring->end == ring->start));

	assert(ring->debug_availbefore >= 0);
	assert(ring->debug_availafter >= 0);
	assert(ring->debug_availbefore + ring->debug_availafter <= ring->bufsize);

	/* ring->cur can point to any present  */
	/* element (#) or be equal to ring->end*/

	if (ring->end >= ring->start) {
		/* case A: ring->end >= ring->start    */
		/*                                     */
		/* -------#############-----------     */
		/* ^      ^            ^          ^    */
		/* buf    start        end     bufend  */

		assert(ring->cur >= ring->start && ring->cur <= ring->end);

		assert(ring->cur - ring->start == ring->debug_availbefore);
		assert(ring->end - ring->cur   == ring->debug_availafter);
	}
	else {
		/* case B: ring->end < ring->start     */
		/*                                     */
		/* #######------------############     */
		/* ^      ^           ^           ^    */
		/* buf    end        start     bufend  */

		assert((ring->cur >= ring->start && ring->cur < ring->bufend)
				||
		       (ring->cur >= ring->buf && ring->cur <= ring->end));

		if (ring->cur >= ring->start) {
			assert(ring->cur - ring->start == ring->debug_availbefore);
			assert((ring->bufend - ring->cur) + (ring->end - ring->buf) 
					== ring->debug_availafter);
		}
		else {
			assert((ring->bufend - ring->start) + (ring->cur - ring->buf) 
					== ring->debug_availbefore);
			assert(ring->end - ring->cur == ring->debug_availafter);
		}
	}
}

vmlog_ringbuf * vmlog_ringbuf_new(const char *fname,int bufsize)
{
	vmlog_ringbuf *ring;

	assert(bufsize > 0);

	VMLOG_XZNEW(ring,vmlog_ringbuf);
	VMLOG_XZNEW_ARRAY(ring->buf,vmlog_log_entry,bufsize);

	ring->bufsize = bufsize;
	ring->bufend = ring->buf + bufsize;
	ring->start = ring->buf;
	ring->end = ring->buf;
	ring->cur = ring->buf;

	vmlog_file_open(&(ring->file),fname,vmlogRead);
	vmlog_file_stat(&(ring->file));

	vmlog_ringbuf_check_invariants(ring);

	return ring;
}

void vmlog_ringbuf_free(vmlog_ringbuf *ring)
{
	if (!ring)
		return;

	vmlog_ringbuf_check_invariants(ring);

	vmlog_file_close(&(ring->file));

	VMLOG_FREE_ARRAY(vmlog_log_entry,ring->bufsize,ring->buf);
	VMLOG_FREE(vmlog_ringbuf,ring);
}

static int vmlog_ringbuf_read(vmlog_ringbuf *ring,vmlog_log_entry *buf,
		vmlog_seq_t seq,int n)
{
	int r;
	vmlog_fofs_t ofs;

	ofs = seq * sizeof(vmlog_log_entry);
	if (ofs != ring->file.ofs)
		vmlog_file_seek(&(ring->file),ofs);

	do {
		/* fprintf(stdout,"vmlog_ringbuf_read(%p,%d,%d)\n",
				(void*)ring,buf-ring->buf,n); */
		
		r = read(ring->file.fd,buf,n * sizeof(vmlog_log_entry));
	} while (r == -1 && errno == EINTR);

	if (r == -1)
		vmlog_die("reading from file: %s: %s",ring->file.fname,strerror(errno));

	ring->file.ofs += r;

	if (r % sizeof(vmlog_log_entry) != 0) {
		/* XXX */
		vmlog_warn("partial log entry read from file: %s",ring->file.fname);
	}

	return r / sizeof(vmlog_log_entry);
}

static int vmlog_ringbuf_fill_forward(vmlog_ringbuf *ring,vmlog_log_entry *fillstart,
				      vmlog_log_entry *fillend,vmlog_seq_t seq,int len)
{
	int space;
	int n;
	int read;
	vmlog_log_entry *oldend;

#if 0
	fprintf(stdout,"vmlog_ringbuf_fill_forward(%p,%d,%d," VMLOG_SEQ_FMT ",%d)\n",
			(void*)ring,fillstart-ring->buf,fillend-ring->buf,seq,len);
#endif

	vmlog_ringbuf_check_invariants(ring);

	space = fillend - fillstart;
	n = (len <= space) ? len : space;

	if (n <= 0)
		return 0;

	read = vmlog_ringbuf_read(ring,fillstart,seq,n);
	if (!read)
		return 0;

	oldend = ring->end;
	ring->end = fillstart + read;
	ring->debug_availafter += read;

	if (ring->cur == ring->bufend)
		ring->cur = ring->buf;

	if (ring->start >= fillstart && ring->start != oldend) {
		/* check if old entries have been overwritten */
		if (ring->start <= ring->end) {
			ring->debug_availbefore -=
				ring->end - ring->start + 1;
			ring->start = ring->end + 1;
			if (ring->start >= ring->bufend) {
				ring->start = ring->buf;
				ring->debug_availbefore = ring->cur - ring->start;
			}
		}
	}

	vmlog_ringbuf_check_invariants(ring);

	return read;
}
	
static int vmlog_ringbuf_fill_backward(vmlog_ringbuf *ring,vmlog_log_entry *fillstart,
				       vmlog_log_entry *fillend,vmlog_seq_t seq,int len)
{
	int space;
	int n;
	int read;
	vmlog_log_entry *oldstart;
	vmlog_log_entry *oldend;

#if 0
	fprintf(stdout,"vmlog_ringbuf_fill_backward(%p,%d,%d," VMLOG_SEQ_FMT ",%d)\n",
			(void*)ring,fillstart-ring->buf,fillend-ring->buf,seq,len);
#endif

	vmlog_ringbuf_check_invariants(ring);

	space = fillend - fillstart;
	n = (len <= space) ? len : space;

	if (n <= 0)
		return 0;

	seq += space - n;
	fillstart += space - n;

	read = vmlog_ringbuf_read(ring,fillstart,seq,n);
	if (read != n)
		vmlog_die("could not read backward in file: %s: %s",
			ring->file.fname,strerror(errno));

	oldstart = ring->start;
	ring->start = fillstart;
	ring->debug_availbefore += read;

	oldend = ring->end;
	if (ring->end <= fillend && ring->end != oldstart) {
		/* check if old entries have been overwritten */
		if (ring->start <= ring->end) {
			ring->debug_availafter -=
				ring->end - ring->start + 1;
			ring->end = ring->start - 1;

			if (ring->end <= ring->buf) {
				ring->end = ring->bufend;
				if (ring->cur == ring->buf && ring->end == ring->bufend)
					ring->cur = ring->bufend;
				ring->debug_availafter = ring->end - ring->cur;
			}
		}
	}

	if (ring->end == ring->buf) {
		assert(oldstart == oldend);
		ring->end = ring->bufend;
	}

	if (ring->cur == ring->buf && ring->end == ring->bufend)
		ring->cur = ring->bufend;

	vmlog_ringbuf_check_invariants(ring);

	return read;
}
	
int vmlog_ringbuf_fill(vmlog_ringbuf *ring,int len)
{
	int count;
	int read;
	vmlog_log_entry *fillend;
	vmlog_seq_t seq;
	
	assert(ring);

	if (!len)
		return 0 /*XXX*/;

	count = 0;

	vmlog_ringbuf_check_invariants(ring);
	
	if (len > 0) {
		if (ring->end >= ring->cur) {
			/* case A'1: ring->end >= ring->start  */
			/*                                     */
			/*                     vvvvvvvvvvv     */
			/* ------OOOOO#########-----------     */
			/* ^     ^    ^        ^          ^    */
			/* buf  start cur      end     bufend  */

			/* case B'1: ring->end < ring->start   */
			/*                                     */
			/*                     vvvvvvvvvvv     */
			/* OOOOOOOOOOO#########----OOOOOOO     */
			/* ^          ^        ^   ^      ^    */
			/* buf        cur      end st. bufend  */

			/* fill space at end of buf */
			seq = ring->seq + (ring->end - ring->cur);
			read = vmlog_ringbuf_fill_forward(ring,ring->end,ring->bufend,
					seq,len);
			count += read;
			len -= read;

			if (ring->end != ring->bufend)
				goto no_more_entries;

			/* case A'1: ring->end >= ring->start  */
			/*                                     */
			/* vvvvvvvvvv                          */
			/* ------OOOOO####################     */
			/* ^     ^    ^                   ^    */
			/* buf  start cur         end==bufend  */

			/* case B'1: ring->end < ring->start   */
			/*                                     */
			/* vvvvvvvvvv                          */
			/* OOOOOOOOOOO####################     */
			/* ^          ^                   ^    */
			/* buf==start cur         end==bufend  */

			/* fill space at beg of buf */
			seq = ring->seq + (ring->end - ring->cur);
			fillend = (ring->cur == ring->bufend) ? ring->bufend : (ring->cur - 1);
			read = vmlog_ringbuf_fill_forward(ring,ring->buf,fillend,
					seq,len);
			count += read;
			len -= read;
		}
		else {
			/* ring->end < ring->cur */

			/* no case A'2 */
			assert(ring->end < ring->start);

			/* case B'2: ring->end < ring->start   */
			/*                                     */
			/*      vvvvvvvvvvv                    */
			/* #####------OOOOOO##############     */
			/* ^    ^     ^     ^             ^    */
			/* buf  end   start cur        bufend  */

			/* fill space in middle of buf */
			seq = ring->seq + (ring->bufend - ring->cur) + (ring->end - ring->buf);
			read = vmlog_ringbuf_fill_forward(ring,ring->end,ring->cur - 1,
					seq,len);
			count += read;
			len -= read;
		}
	}
	else {
		len = -len;

		if (len > ring->seq)
			len = ring->seq;

		if (ring->start <= ring->cur) {
			/* case A'1: ring->end >= ring->start  */
			/*                                     */
			/* vvvvvv                              */
			/* ------#####OOOOOOOOO-----------     */
			/* ^     ^    ^        ^          ^    */
			/* buf  start cur      end     bufend  */

			/* case B'2: ring->end < ring->start   */
			/*                                     */
			/* vvvvvvvvvvv                         */
			/* OOOOO------######OOOOOOOOOOOOOO     */
			/* ^    ^     ^     ^             ^    */
			/* buf  end   start cur        bufend  */

			/* fill space at beg of buf */
			seq = ring->seq - (ring->cur - ring->buf);
			read = vmlog_ringbuf_fill_backward(ring,ring->buf,ring->start,
					seq,len);
			count += read;
			len -= read;

			if (ring->start != ring->buf)
				goto no_more_entries;

			/* case A'1: ring->end >= ring->start  */
			/*                                     */
			/*             vvvvvvvvvvvvvvvvvvv     */
			/* ###########OOOOOOOOO-----------     */
			/* ^          ^        ^          ^    */
			/* buf=start  cur      end     bufend  */

			/* case B'2: ring->end < ring->start   */
			/*                                     */
			/*                   vvvvvvvvvvvvv     */
			/* #################OOOOOOOOOOOOOO     */
			/* ^                ^             ^    */
			/* buf=start        cur    end=bufend  */

			/* fill space at end of buf */
			seq -= (ring->bufend - ring->cur - 1);
			read = vmlog_ringbuf_fill_backward(ring,ring->cur+1,ring->bufend,
					seq,len);
			count += read;
			len -= read;
		}
		else {
			/* ring->start > ring->cur */

			/* case B'1: ring->end < ring->start   */
			/*                                     */
			/*             vvvvvvvvvvvv            */
			/* ###########OOOOOOOOO----#######     */
			/* ^          ^        ^   ^      ^    */
			/* buf        cur      end st. bufend  */

			/* no case A'2 */
			assert(ring->end < ring->start);

			/* fill space in middle of buf */
			seq = ring->seq - (ring->cur - ring->buf) - (ring->bufend - ring->cur - 1);
			read = vmlog_ringbuf_fill_backward(ring,ring->cur + 1,ring->start,
					seq,len);
			count += read;
			len -= read;
		}
	}

no_more_entries:
	vmlog_ringbuf_check_invariants(ring);
	
	return count;
}

static void vmlog_ringbuf_reset(vmlog_ringbuf *ring)
{
	ring->start = ring->buf;
	ring->cur = ring->buf;
	ring->end = ring->buf;

	ring->debug_availbefore = 0;
	ring->debug_availafter = 0;

	vmlog_ringbuf_check_invariants(ring);
}

static int vmlog_ringbuf_advance(vmlog_ringbuf *ring,int diff)
{
	int space;
	
	if (diff > 0) {
		if (ring->end >= ring->start) {
			/* case A */
advance_cur_to_end:
			space = ring->end - ring->cur;

			if (space <= 0)
				return 0;

			if (space < diff)
				diff = space;

simple_advance:
			ring->cur += diff;
			ring->seq += diff;
			ring->debug_availbefore += diff;
			ring->debug_availafter -= diff;
			return diff;
		}
		else {
			/* case B */
			if (ring->end >= ring->cur)
				goto advance_cur_to_end;

			space = ring->bufend - ring->cur;
			if (space > diff)
				goto simple_advance;

			ring->cur = ring->buf - space;
			goto advance_cur_to_end;
		}
	}
	else if (diff < 0) {
		if (ring->end >= ring->start) {
			/* case A */
advance_cur_to_start:
			space = ring->cur - ring->start;

			if (space <= 0)
				return 0;

			if (-space > diff)
				diff = -space;
			goto simple_advance;
		}
		else {
			/* case B */
			if (ring->cur >= ring->start)
				goto advance_cur_to_start;

			space = ring->cur - ring->buf;
			if (space >= -diff)
				goto simple_advance;

			ring->cur = ring->bufend + space;
			goto advance_cur_to_start;
		}
	}
	else {
		return 0;
	}
}

void vmlog_ringbuf_seek(vmlog_ringbuf *ring,vmlog_seq_t seq)
{
	vmlog_seq_t diff;
	
	vmlog_ringbuf_check_invariants(ring);

	diff = seq - ring->seq;
	if (abs(diff) < ring->bufsize)
		diff -= vmlog_ringbuf_advance(ring,(int)diff);

	if (!diff)
		return;

	vmlog_ringbuf_reset(ring);
	vmlog_file_seek(&(ring->file),seq * sizeof(vmlog_log_entry));
	ring->seq = seq;

	vmlog_ringbuf_check_invariants(ring);
}

vmlog_log_entry * vmlog_ringbuf_next(vmlog_ringbuf *ring,int prefetch)
{
	vmlog_ringbuf_check_invariants(ring);

	while (1) {
		if (ring->end >= ring->start) {
			/* case A */
			if (ring->cur < ring->end) {
				ring->debug_availafter--;
				ring->debug_availbefore++;
				ring->seq++;
				return ring->cur++;
			}
		}
		else {
			/* case B */
			if (ring->end >= ring->cur) {
				if (ring->cur < ring->end) {
					ring->debug_availafter--;
					ring->debug_availbefore++;
					ring->seq++;
					return ring->cur++;
				}
			}
			else {
				if (ring->cur < ring->bufend) {
					vmlog_log_entry *r;

					r = ring->cur;
					ring->seq++;
					if (++ring->cur == ring->bufend)
						ring->cur = ring->buf;
					ring->debug_availafter--;
					ring->debug_availbefore++;
					return r;
				}
			}
		}	

		if (!vmlog_ringbuf_fill(ring,prefetch))
			return NULL;
	}

	assert(0); /* NOT REACHED */
	return NULL;
}

vmlog_log_entry * vmlog_ringbuf_prev(vmlog_ringbuf *ring,int prefetch)
{
	vmlog_ringbuf_check_invariants(ring);

	while (1) {
		if (ring->end >= ring->start) {
			/* case A */
			if (ring->cur > ring->start) {
				ring->debug_availafter++;
				ring->debug_availbefore--;
				ring->seq--;
				return --ring->cur;
			}
		}
		else {
			/* case B */
			if (ring->cur >= ring->start) {
				if (ring->cur > ring->start) {
					ring->debug_availafter++;
					ring->debug_availbefore--;
					ring->seq--;
					return --ring->cur;
				}
			}
			else {
				if (--ring->cur < ring->buf)
					ring->cur = ring->bufend - 1;
				ring->seq--;
				ring->debug_availafter++;
				ring->debug_availbefore--;
				return ring->cur;
			}
		}	

		if (!vmlog_ringbuf_fill(ring,-prefetch))
			return NULL;
	}

	assert(0); /* NOT REACHED */
	return NULL;
}

/*** option parsing **************************************************/

int vmlog_opt_parse_seq(const char *arg,int len,vmlog_seq_t *seq)
{
	char *buf;
	char *endptr;
	int r;
	
	assert(arg);

	if (len < 1)
		return 0;

	buf = vmlog_strdup(arg,len);
	*seq = strtoll(buf,&endptr,10);

	r = (endptr[0] == 0);

	VMLOG_FREE_ARRAY(char,len+1,buf);
	return r;
}

int vmlog_opt_parse_range(const char *arg,vmlog_seq_t *start,vmlog_seq_t *end)
{
	const char *sep;
	int len;

	sep = strchr(arg,':');
	if (!sep) {
		len = strlen(arg);
		if (!vmlog_opt_parse_seq(arg,len,start))
			return 0;
		*end = *start;
		return 1;
	}

	len = sep - arg;
	if (!len) {
		*start = 0;
	}
	else {
		if (!vmlog_opt_parse_seq(arg,len,start))
			return 0;
	}

	len = strlen(arg) - len - 1;
	if (!len) {
		*end = VMLOG_SEQ_MAX;
	}
	else {
		if (!vmlog_opt_parse_seq(sep+1,len,end))
			return 0;
	}
	return 1;
}

static int vmlog_opt_parse_one_option(vmlog_options *opts, const char *arg, const char *nextarg)
{
	int eat;

	if (strncmp(arg,"-vmlog:",7) != 0) {
		return 0;
	}

	/* a vmlog option */

	eat = 1;
	if (strcmp(arg,"-vmlog:prefix") == 0) {
		if (!nextarg)
			vmlog_die("expected a prefix after -vmlog:prefix");
		opts->prefix = vmlog_strdup(nextarg,strlen(nextarg));
		eat++;
	}
	else if (strcmp(arg,"-vmlog:strings") == 0) {
		if (!nextarg)
			vmlog_die("expected a prefix after -vmlog:strings");
		opts->stringprefix = vmlog_strdup(nextarg,strlen(nextarg));
		eat++;
	}
	else if (strcmp(arg,"-vmlog:ignore") == 0) {
		if (!nextarg)
			vmlog_die("expected a prefix after -vmlog:ignore");
		opts->ignoreprefix = vmlog_strdup(nextarg,strlen(nextarg));
		eat++;
	}
	else {
		vmlog_die("unknown -vmlog:... option: %s",arg);
	}

	return eat;
}

vmlog_options *vmlog_opt_parse_cmd_line(int *pargc,char **argv)
{
	int i;
	const char *arg;
	vmlog_options *opts;
	int eat;
	int left;

	assert(pargc);

	VMLOG_XZNEW(opts,vmlog_options);

	if (*pargc && argv[0])
		opts->progname = vmlog_strdup(argv[0],strlen(argv[0]));

	i = 1;
	while (i < *pargc) {
		arg = argv[i];
		
		left = *pargc - i - 1;

		eat = vmlog_opt_parse_one_option(opts,arg,
				(left) ? argv[i+1] : NULL);

		if (eat == 0) {
			i++;
			continue;
		}

		/* remove the option from the command line */
		
		memmove(argv + i,argv + i + eat,sizeof(char*) * (*pargc - (i+eat)));
		*pargc -= eat;
	}

	return opts;
}

vmlog_options *vmlog_opt_parse_vmargs(JavaVMInitArgs *vmargs)
{
	int i;
	const char *arg;
	vmlog_options *opts;
	int eat;

	assert(vmargs);

	VMLOG_XZNEW(opts,vmlog_options);

	i = 0;
	while (i < vmargs->nOptions) {
		arg = vmargs->options[i].optionString;

		eat = vmlog_opt_parse_one_option(opts,arg,
				(i+1 < vmargs->nOptions) ? vmargs->options[i+1].optionString : NULL);

		if (eat == 0) {
			i++;
			continue;
		}
		
		/* remove the option from the command line */
		
		memmove(vmargs->options + i,vmargs->options + i + eat,
				sizeof(JavaVMOption) * (vmargs->nOptions - (i+eat)));
		vmargs->nOptions -= eat;
	}

	return opts;
}

void vmlog_opt_free(vmlog_options *opts)
{
	if (!opts)
		return;

	if (opts->prefix)
		VMLOG_FREE_ARRAY(char,strlen(opts->prefix)+1,opts->prefix);
	if (opts->stringprefix)
		VMLOG_FREE_ARRAY(char,strlen(opts->stringprefix)+1,opts->stringprefix);
	if (opts->ignoreprefix)
		VMLOG_FREE_ARRAY(char,strlen(opts->ignoreprefix)+1,opts->ignoreprefix);

	VMLOG_FREE(vmlog_options,opts);
}

/* vim: noet ts=8 sw=8
 */

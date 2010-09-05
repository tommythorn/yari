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
#include <getopt.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define VMLOGDUMP_RINGBUF_SIZE 1024*16
#define VMLOGDUMP_PREFETCH 1024

/*** options *********************************************************/

char *opt_prefix;
int opt_threadidx = 0;
int opt_dumpstrings = 0;
int opt_countcalls = 0;
int opt_show_zero_counts = 0;
int opt_open_frames = 0;
int opt_prefix_open_frames = 0;
int opt_show_depth = 1;
int opt_show_seqns = 0;
int opt_indent = 4;
int opt_reldepth = 0;
int opt_strip_desc = 0;
int opt_maxdepth = INT_MAX;

char *opt_tag = NULL;
char *opt_string = NULL;
vmlog_log_entry opt_needle;

vmlog_seq_t opt_range_start = 0;
vmlog_seq_t opt_range_end = VMLOG_SEQ_MAX;

/*** global variables ************************************************/

char *g_idxfname;
char *g_strfname;
char *g_logfname;
int g_logfd;
int g_idxlen;
int g_strlen;
int g_nstrings;
char *g_indentation;

vmlog_string_entry *g_idxmap;
char *g_strmap;

vmlog_ringbuf *g_ringbuf = NULL;

int *g_counters = NULL;

/*** usage ***********************************************************/

const char *usage =
"Usage: vmlogdump [options] prefix\n"
"\n"
"    Options:\n"
"        -c      count call frequency of methods\n"
"        -D      don't show depth numbers\n"
"           -z   show methods with zero call count\n"
"        -gT:S   select entries with given Tag and String\n"
"        -h      display this help message\n"
"        -iNUM   indentation width\n"
"        -lNUM   limit max depth to NUM\n"
"        -n      show sequence numbers / string indices\n"
"        -o      show open frames\n"
"        -O      show open frames before dump\n"
"        -P      strip method parameter and return types\n"
"        -rN:M   limit output to a range of sequence numbers\n"
"        -R      show depth relative to minimum depth in range\n"
"        -s      dump strings\n"
"        -tNUM   use log of thread NUM (default=0)\n"
"\n";

/*********************************************************************/

static void parse_tag_and_string(const char *arg)
{
	const char *sep;
	int len;

	sep = strchr(arg,':');
	if (!sep)
		vmlog_die_usage(usage,1);

	len = sep - arg;
	VMLOG_XZNEW_ARRAY(opt_tag,char,len+1);
	memcpy(opt_tag,arg,len);

	opt_needle.tag = vmlog_tag_from_name(opt_tag,len);
	if (opt_needle.tag == -1) {
		vmlog_die("unknown tag identifier: %s",opt_tag);
	}
	
	len = strlen(arg) - len - 1;
	VMLOG_XZNEW_ARRAY(opt_string,char,len+1);
	memcpy(opt_string,sep+1,len);
}

void parse_command_line(int argc,char **argv)
{
	int r;

	while (1) {
		r = getopt(argc,argv,"cDg:hi:l:noOPr:Rst:z");
		if (r == -1)
			break;
		switch (r) {
			case 'c':
				opt_countcalls = 1;
				break;

			case 'D':
				opt_show_depth = 0;
				break;

			case 'g':
				parse_tag_and_string(optarg);
				break;

			case 'h':
				vmlog_die_usage(usage,0);

			case 'i':
				opt_indent = atoi(optarg);
				if (opt_indent < 0)
					vmlog_die_usage(usage,1);
				break;

			case 'l':
				opt_maxdepth = atoi(optarg);
				if (opt_maxdepth < 0)
					vmlog_die_usage(usage,1);
				break;

			case 'n':
				opt_show_seqns = 1;
				break;

			case 'o':
				opt_open_frames = 1;
				break;

			case 'O':
				opt_prefix_open_frames = 1;
				break;

			case 'P':
				opt_strip_desc = 1;
				break;

			case 'r':
				if (!vmlog_opt_parse_range(optarg,&opt_range_start,&opt_range_end))
					vmlog_die("invalid range: %s",optarg);
				break;

			case 'R':
				opt_reldepth = 1;
				break;
				
			case 's':
				opt_dumpstrings = 1;
				break;
				
			case 't':
				opt_threadidx = atoi(optarg);
				break;

			case 'z':
				opt_show_zero_counts = 1;
				break;
				
			case '?': vmlog_die_usage(usage,1);
		}
	}

	if (argc - optind < 1)
		vmlog_die_usage(usage,1);
	
	opt_prefix = argv[optind++];

	g_indentation = VMLOG_NEW_ARRAY(char,opt_indent+1);
	memset(g_indentation,' ',opt_indent);
	g_indentation[opt_indent] = 0;
}

static void fprint_string(FILE *file,int index)
{
	char *str;
	vmlog_string_entry *strent;
	char *buf;
	int len;

	strent = g_idxmap + index;
	str = g_strmap + strent->ofs;
	len = strent->len;

	if (opt_strip_desc) {
		char *desc;

		desc = (char*) memchr(str,'(',len);
		if (desc)
			len = desc - str;
	}

	buf = VMLOG_NEW_ARRAY(char,len+1);

	memcpy(buf,str,len);
	buf[len] = 0;

	fputs(buf,file);
	
	VMLOG_FREE_ARRAY(char,len+1,buf);
}

static void dump_log_entry(vmlog_log_entry *logent,int depth,vmlog_seq_t seq)
{
	int i;

	if (opt_show_depth) {
		if (opt_reldepth)
			fputc('R',stdout);
		fprintf(stdout,"%d",depth);
		fputc(':',stdout);
		if (depth < 100) {
			fputc(' ',stdout);
			if (depth < 10)
				fputc(' ',stdout);
		}
	}
	if (opt_show_seqns) {
		fprintf(stdout,"(" VMLOG_SEQ_FMT_W ")",seq);
	}
	for (i=0; i<depth; ++i)
		fputs(g_indentation,stdout);
	switch(logent->tag) {
		case VMLOG_TAG_ENTER: fputs("enter ",stdout); break;
		case VMLOG_TAG_LEAVE: fputs("leave ",stdout); break;
		case VMLOG_TAG_THROW: fputs("throw ",stdout); break;
		case VMLOG_TAG_CATCH: fputs("catch ",stdout); break;
		case VMLOG_TAG_UNWND: fputs("unwnd ",stdout); break;
		case VMLOG_TAG_SIGNL: fputs("signl ",stdout); break;
		case VMLOG_TAG_UNROL: fputs("unrol ",stdout); break;
		case VMLOG_TAG_REROL: fputs("rerol ",stdout); break;
		default: fputs("<UNKNOWN TAG> ",stdout);
	}
	fprint_string(stdout,logent->index);
	fputs("\n",stdout);
}

static void dump_log_entries(void)
{
	vmlog_log_entry *logent;
	int depth = 0;
	int mindepth = INT_MAX;
	vmlog_seq_t seq;

	if (opt_reldepth) {
		vmlog_ringbuf_seek(g_ringbuf,opt_range_start);
		while ((g_ringbuf->seq <= opt_range_end)
			&& (logent = vmlog_ringbuf_next(g_ringbuf,VMLOGDUMP_PREFETCH)))
		{
			int depthchange = vmlog_tag_definitions[logent->tag].depth;

			depth += depthchange;
			if (depthchange < 0) {
				if (depth < mindepth)
					mindepth = depth;
			}
		}

		vmlog_ringbuf_seek(g_ringbuf,opt_range_start);
		if (mindepth != INT_MAX)
			depth = -mindepth;
		else
			depth = 0;
	}

	while ((g_ringbuf->seq <= opt_range_end)
		&& (logent = vmlog_ringbuf_next(g_ringbuf,VMLOGDUMP_PREFETCH)))
	{
		int depthchange = vmlog_tag_definitions[logent->tag].depth;

		if (depthchange < 0) {
			depth += depthchange;
			if (depth < mindepth)
				mindepth = depth;
		}

		seq = g_ringbuf->seq - 1;

		if ((!opt_string || (logent->tag == opt_needle.tag && logent->index == opt_needle.index))
			&& (seq >= opt_range_start)
			&& (depth <= opt_maxdepth))
		{
			dump_log_entry(logent,depth,seq);
		}

		if (depthchange > 0)
			depth += depthchange;
	}
}

static void dump_strings(void)
{
	int i;

	for (i=0; i<g_nstrings; ++i) {
		if (opt_show_seqns) {
			fprintf(stdout,"%8d: ",i);
		}
		fprint_string(stdout,i);
		fputc('\n',stdout);
	}
}

static int compare_by_counter(const void *a,const void *b)
{
	return g_counters[*(int*)b] - g_counters[*(int*)a];
}

static void count_calls(void)
{
	int *indices;
	int i;
	int index;
	vmlog_log_entry *logent;

	g_counters = VMLOG_NEW_ARRAY(int,g_nstrings);
	assert(g_counters);
	memset(g_counters,0,sizeof(int)*g_nstrings);

	while (1) {
		logent = vmlog_ringbuf_next(g_ringbuf,VMLOGDUMP_PREFETCH);
		if (!logent)
			break;

		if (logent->tag == VMLOG_TAG_ENTER) {
			g_counters[logent->index]++;
		}
	}

	indices = VMLOG_NEW_ARRAY(int,g_nstrings);
	assert(indices);
	for (i=0; i<g_nstrings; ++i)
		indices[i] = i;

	qsort(indices,g_nstrings,sizeof(int),compare_by_counter);

	for (i=0; i<g_nstrings; ++i) {
		index = indices[i];
		if (!g_counters[index] && !opt_show_zero_counts)
			break;
		fprintf(stdout,"%12d ",g_counters[index]);
		fprint_string(stdout,index);
		fputc('\n',stdout);
	}
	
	VMLOG_FREE_ARRAY(int,g_nstrings,indices);
	VMLOG_FREE_ARRAY(int,g_nstrings,g_counters);
}

static void dump_open_frames(void)
{
	vmlog_thread_log *tlog;
	vmlog_log_entry *logent;
	vmlog_frame *frame;
	vmlog_seq_t start;
	vmlog_seq_t end;
	int i;
	int maxdepth = 0;

	tlog = vmlog_thread_log_new(NULL,NULL,0);

	if (opt_prefix_open_frames) {
		start = 0;
		end = opt_range_start - 1;
	}
	else {
		start = opt_range_start;
		end = opt_range_end;
	}

	if (start > 0)
		vmlog_ringbuf_seek(g_ringbuf,start);

	while (g_ringbuf->seq <= end 
		&& (logent = vmlog_ringbuf_next(g_ringbuf,VMLOGDUMP_PREFETCH)))
	{
		int depthchange = vmlog_tag_definitions[logent->tag].depth;
		if (depthchange < 0) {
			if (!vmlog_thread_log_leave(tlog,logent->index,g_ringbuf->seq - 1))
				vmlog_warn("unmatched leave");
		}
		else if (depthchange > 0) {
			vmlog_thread_log_enter(tlog,logent->index,g_ringbuf->seq - 1);
			if (tlog->depth > maxdepth)
				maxdepth = tlog->depth;
		}
	}

	if (tlog->depth < 0) {
		vmlog_warn("thread ends with negative call frame depth: %i",tlog->depth);
		vmlog_warn("frame buffer will be dumped, expect bogus results:");
		tlog->depth = tlog->framescap;
		if (tlog->depth > maxdepth)
			tlog->depth = maxdepth;
	}

	for (i=0; i<tlog->depth; ++i) {
		frame = tlog->frames + i;
		fprintf(stdout,"%3d: (" VMLOG_SEQ_FMT_W ") ",i,frame->seq);
		fprint_string(stdout,frame->index);
		fputc('\n',stdout);
	}

	vmlog_thread_log_free(tlog);
}

int main(int argc,char **argv)
{
	char buf[20];
	int need_logfile = 1;
	int need_vml = 0;
	vmlog_log *vml;
	
	if (argc)
		vmlog_set_progname(argv[0]);
	parse_command_line(argc,argv);

	sprintf(buf,"%d",opt_threadidx);
	g_idxfname = vmlog_concat3(opt_prefix,"",".idx",NULL);
	g_strfname = vmlog_concat3(opt_prefix,"",".str",NULL);
	g_logfname = vmlog_concat4len(opt_prefix,strlen(opt_prefix),
			".",1,buf,strlen(buf),".log",4,NULL);

	if (opt_dumpstrings)
		need_logfile = 0;

	if (opt_string)
		need_vml = 1;

	if (need_logfile) {
		g_ringbuf = vmlog_ringbuf_new(g_logfname,VMLOGDUMP_RINGBUF_SIZE);
	}

	g_idxmap = (vmlog_string_entry *) vmlog_file_mmap(g_idxfname,&g_idxlen);
	g_nstrings = g_idxlen / sizeof(vmlog_string_entry);
	g_strmap = (char *) vmlog_file_mmap(g_strfname,&g_strlen);

	if (need_vml) {
		vml = vmlog_log_new(NULL,0);
		vmlog_load_stringhash(vml,opt_prefix);
		if (opt_string) {
			opt_needle.index = vmlog_get_string_index(vml,opt_string,strlen(opt_string));
			fprintf(stdout,"needle '%s:%s' tag=%d index=%d\n",
					opt_tag,opt_string,opt_needle.tag,opt_needle.index);
		}
	}
	
	if (opt_countcalls) {
		count_calls();
	}
	else if (opt_dumpstrings) {
		dump_strings();
	}
	else if (opt_open_frames) {
		dump_open_frames();
	}
	else {
		if (opt_prefix_open_frames) {
			dump_open_frames();
			fputs("---\n",stdout);
		}
		dump_log_entries();
	}

	vmlog_ringbuf_free(g_ringbuf);
	g_ringbuf = NULL;
	munmap(g_strmap,g_strlen);
	munmap(g_idxmap,g_idxlen);

	return 0;
}

/* vim: noet ts=8 sw=8
 */


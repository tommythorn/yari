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

#define VMLOGDIFF_RINGBUF_SIZE 1024*1024
#define VMLOGDIFF_PREFETCH 256


#if 0
#define LOG(args) printf args
#else
#define LOG(args)
#endif

typedef struct vmlogdiff_removed vmlogdiff_removed;

struct vmlogdiff_removed {
	vmlog_seq_t start;
	vmlog_seq_t len;
};

char *opt_prefix;
char *opt_fname[2];
int opt_verbose = 0;
int opt_context = 10;

char *g_idxfname;
char *g_strfname;
char *g_cleanedfname[2] = { "vmlogdiff.A.clean", "vmlogdiff.B.clean" };
char *g_removedfname[2] = { "vmlogdiff.A.removed", "vmlogdiff.B.removed" };
int g_idxlen;
int g_strlen;
int g_nstrings;
char *g_indentation = "    ";
vmlog_seq_t g_context_available = 0;
vmlog_seq_t g_context_requested = 0;

vmlog_string_entry *g_idxmap;
char *g_strmap;
vmlogdiff_removed *g_removedmap[2];
vmlogdiff_removed *g_removedptr[2];
vmlogdiff_removed *g_removedend[2];
int g_removedlen[2];
int g_nremoved[2];
vmlog_seq_t g_showseq[2] = { 0 };

int g_level = 0;

vmlog_ringbuf *g_ringbuf[2] = { NULL };
vmlog_ringbuf *g_ringbuf_back[2] = { NULL };
vmlog_ringbuf *g_ringbuf_show[2] = { NULL };
vmlog_ringbuf *g_ringbuf_orig[2] = { NULL };

const char *usage = 
"Usage: vmlogdiff [options] prefix fileA fileB\n"
"\n"
"    Options:\n"
"        -h      display this help message\n"
"        -v      verbose messages to stderr\n"
"\n";

/* global variables for diff algorithm */

vmlog_seq_t g_n[2];
vmlog_seq_t *g_Vforw_buf = NULL;
vmlog_seq_t *g_Vback_buf = NULL;
vmlog_seq_t *g_Vforw = NULL;
vmlog_seq_t *g_Vback = NULL;
int g_maxD;
int g_Vsize = 0;

char *g_index_used = NULL;

static void compare(vmlog_seq_t xstart,vmlog_seq_t ystart,vmlog_seq_t xend,vmlog_seq_t yend);
	
static void diff_alloc(int maxD)
{
	assert(maxD >= 0);

	if (g_Vsize) {
		VMLOG_FREE_ARRAY(vmlog_seq_t,g_Vsize,g_Vforw_buf);
		VMLOG_FREE_ARRAY(vmlog_seq_t,g_Vsize,g_Vback_buf);
	}

	g_maxD = maxD;
	g_Vsize = 2 * g_maxD + 1;

	g_Vforw_buf = VMLOG_NEW_ARRAY(vmlog_seq_t,g_Vsize);
	g_Vback_buf = VMLOG_NEW_ARRAY(vmlog_seq_t,g_Vsize);

	g_Vforw = g_Vforw_buf + g_maxD;
	g_Vback = g_Vback_buf + g_maxD;
}

void parse_command_line(int argc,char **argv)
{
	int r;

	while (1) {
		r = getopt(argc,argv,"hv");
		if (r == -1)
			break;
		switch (r) {
			case 'h':
				vmlog_die_usage(usage,0);

			case 'v':
				opt_verbose++;
				break;

			case '?':
				vmlog_die_usage(usage,1);
		}
	}

	if (argc - optind < 3)
		vmlog_die_usage(usage,1);
	
	opt_prefix = argv[optind++];
	opt_fname[0] = argv[optind++];
	opt_fname[1] = argv[optind++];
}

static void fprint_string(FILE *file,int index)
{
	char *str;
	vmlog_string_entry *strent;
	char *buf;

	strent = g_idxmap + index;
	str = g_strmap + strent->ofs;

	buf = VMLOG_NEW_ARRAY(char,strent->len+1);

	memcpy(buf,str,strent->len);
	buf[strent->len] = 0;

	fputs(buf,file);
	
	VMLOG_FREE_ARRAY(char,strent->len+1,buf);
}

static void dump_log_entry(vmlog_log_entry *logent,int depth)
{
#if 0
	fprintf(stdout,"%d",depth);
	fputc(':',stdout);
	if (depth < 100) {
		fputc(' ',stdout);
		if (depth < 10)
			fputc(' ',stdout);
	}
	for (i=0; i<depth; ++i)
		fputs(g_indentation,stdout);
#endif
	switch(logent->tag) {
		case VMLOG_TAG_ENTER: fputs("enter ",stdout); break;
		case VMLOG_TAG_LEAVE: fputs("leave ",stdout); break;
		case VMLOG_TAG_THROW: fputs("throw ",stdout); break;
		case VMLOG_TAG_CATCH: fputs("catch ",stdout); break;
		case VMLOG_TAG_UNWND: fputs("unwnd ",stdout); break;
	}
	fprint_string(stdout,logent->index);
	fputs("\n",stdout);
}

static void show_context(void)
{
	int len;
	vmlog_log_entry *logent;
	vmlog_seq_t skipped;

	if (g_context_available > 2*opt_context) {
		skipped = g_context_available - 2*opt_context;
		printf("@@ " VMLOG_SEQ_FMT " common entr%s skipped @@\n",skipped,(skipped > 1) ? "ies" : "y");
		g_context_available = opt_context;
	}
	
	len = opt_context;
	if (len > g_context_available) {
		len = g_context_available;
	}

	if (len <= 0)
		return;

	vmlog_ringbuf_seek(g_ringbuf_show[0],g_ringbuf_show[0]->seq - len);
	while (len-- && (logent = vmlog_ringbuf_next(g_ringbuf_show[0],VMLOGDIFF_PREFETCH))) {
		fputc(' ',stdout);
		dump_log_entry(logent,0);
		g_context_available--;
	}
}

static void handle_unmatchable(int which,vmlogdiff_removed *rm)
{
	int i;
	vmlog_log_entry *logent;
	
	LOG(("unmatchable, only in %d: ofs=%lld len=%lld\n",which,rm->start,rm->len));

	assert(rm->len);

	show_context();

	vmlog_ringbuf_seek(g_ringbuf_orig[which],rm->start);
	for (i=0; i<rm->len; ++i) {
		logent = vmlog_ringbuf_next(g_ringbuf_orig[which],1 /* XXX */);
		fputc((which) ? '+' : '-',stdout);
		dump_log_entry(logent,0);
	}

	g_context_requested = opt_context;
	g_context_available = 0;
}

static void handle_common(vmlog_seq_t xstart,vmlog_seq_t ystart,vmlog_seq_t len)
{
	int i;
	int j;
	vmlog_log_entry *logent;
	
	LOG(("common: x=%lld y=%lld len=%lld\n",xstart,ystart,len));

	vmlog_ringbuf_seek(g_ringbuf_show[0],xstart);
	for (i=0; i<len; ++i) {
		for (j=0; j<2; ++j) {
			if (g_removedptr[j] < g_removedend[j] && g_showseq[j] == g_removedptr[j]->start) {
				handle_unmatchable(j,g_removedptr[j]);
				g_showseq[j] += g_removedptr[j]->len;
				g_removedptr[j]++;
			}
		}
		
		logent = vmlog_ringbuf_next(g_ringbuf_show[0],VMLOGDIFF_PREFETCH);
		if (g_context_requested) {
			fputc(' ',stdout);
			dump_log_entry(logent,0);
			g_context_requested--;
			assert(!g_context_available);
		}
		else {
			g_context_available++;
		}

		g_showseq[0]++;
		g_showseq[1]++;
	}
}

static void handle_only_in(int which,vmlog_seq_t start,vmlog_seq_t len)
{
	int i;
	vmlog_log_entry *logent;
	
	LOG(("only in %d: ofs=%lld len=%lld\n",which,start,len));

	assert(len);

	show_context();

	vmlog_ringbuf_seek(g_ringbuf_show[which],start);
	for (i=0; i<len; ++i) {
		if (g_removedptr[which] < g_removedend[which] && g_showseq[which] == g_removedptr[which]->start) {
			handle_unmatchable(which,g_removedptr[which]);
			g_showseq[which] += g_removedptr[which]->len;
			g_removedptr[which]++;
		}
		logent = vmlog_ringbuf_next(g_ringbuf_show[which],1 /* XXX */);
		fputc((which) ? '+' : '-',stdout);
		dump_log_entry(logent,0);

		g_showseq[which]++;
	}

	g_context_requested = opt_context;
	g_context_available = 0;
}

static vmlog_seq_t match_forward(vmlog_seq_t x,vmlog_seq_t y,vmlog_seq_t xend,vmlog_seq_t yend)
{
	vmlog_log_entry *enta;
	vmlog_log_entry *entb;
	vmlog_seq_t xorigin;

	if (x >= xend || y >= yend)
		return 0;

	xorigin = x;
	vmlog_ringbuf_seek(g_ringbuf[0],x);
	vmlog_ringbuf_seek(g_ringbuf[1],y);

	do {
		enta = vmlog_ringbuf_next(g_ringbuf[0],VMLOGDIFF_PREFETCH);
		entb = vmlog_ringbuf_next(g_ringbuf[1],VMLOGDIFF_PREFETCH);
		assert(enta && entb);

		if (enta->index != entb->index || enta->tag != entb->tag)
			break;

		/* a diagonal edge in the edit graph */
		x++;
		y++;
	} while (x < xend && y < yend);

	return x - xorigin;
}

static vmlog_seq_t match_backward(vmlog_seq_t xstart,vmlog_seq_t ystart,vmlog_seq_t x,vmlog_seq_t y)
{
	vmlog_log_entry *enta;
	vmlog_log_entry *entb;
	vmlog_seq_t xorigin;

	if (x <= xstart || y <= ystart)
		return 0;

	xorigin = x;
	vmlog_ringbuf_seek(g_ringbuf_back[0],x);
	vmlog_ringbuf_seek(g_ringbuf_back[1],y);

	do {
		enta = vmlog_ringbuf_prev(g_ringbuf_back[0],VMLOGDIFF_PREFETCH);
		entb = vmlog_ringbuf_prev(g_ringbuf_back[1],VMLOGDIFF_PREFETCH);
		assert(enta && entb);

		if (enta->index != entb->index || enta->tag != entb->tag)
			break;

		/* a diagonal edge in the edit graph */
		x--;
		y--;
	} while (x > xstart && y > ystart);

	return xorigin - x;
}

/* pre-condition: X and Y differ at both ends */

static void find_middle_snake(vmlog_seq_t xstart,vmlog_seq_t ystart,vmlog_seq_t xend,vmlog_seq_t yend)
{
	int D;
	int k;
	int resultD;
	vmlog_seq_t diagofs;
	vmlog_seq_t rdiagofs;
	vmlog_seq_t Delta;
	vmlog_seq_t x,y;
	vmlog_seq_t snakex,snakey;
	vmlog_seq_t snakestartx = 0;
	vmlog_seq_t snakestarty = 0;
	vmlog_seq_t snakelen = 0;
	vmlog_seq_t match;
	vmlog_seq_t best_forward;
	vmlog_seq_t best_backward;

	LOG(("find_middle_snake(%lld,%lld,%lld,%lld)\n",
			xstart,ystart,xend,yend));

	diagofs = ystart - xstart;
	rdiagofs = yend - xend;

	Delta = diagofs - rdiagofs;
	
	/* fake vertical edge (0,-1) -> (0,0) ending on 0-diagonal */
	g_Vforw[1] = xstart;

	/* fake vertical edge (N,M+1) -> (N,M) ending on 0-reverse-diagonal */
	g_Vback[-1] = xend;

	best_forward = 0;
	best_backward = xend;
	
	for (D=0; D <= g_maxD; ++D) {
		LOG(("D = %d\n",D));

		if (opt_verbose && D && (D % 1000 == 0))
			fprintf(stderr,"%d. (" VMLOG_SEQ_FMT_W ":" VMLOG_SEQ_FMT_W " entries, " VMLOG_SEQ_FMT_W " left) D = %d\n",
					g_level,xend-xstart,yend-ystart,best_backward - best_forward,D);

		for (k = -D; k <= D; k += 2) {
			LOG(("    k = %d, forward\n",k));

			/* we take a (D-1)-path that ends on a          */
			/* neighbouring diagonal and extend it with a   */
			/* vertical or horizontal edge (whichever takes */
			/* us further)                                  */

			if (k == -D || (k != +D && g_Vforw[k-1] < g_Vforw[k+1])) {
				/* vertical edge from diagonal k+1 down to k */
				x = g_Vforw[k+1];
			}
			else {
				/* horizontal edge from diagonal k-1 right to k */
				x = g_Vforw[k-1] + 1;
			}

			/* calculate y using the k-diagonal equation */
			
			y = x - k + diagofs;

			/* append the trailing snake to the D-path */

			snakex = x;
			snakey = y;

			match = match_forward(x,y,xend,yend);
			x += match;
			y += match;

			if (match) {
				LOG(("match from (%lld,%lld) to (%lld,%lld)\n",
						snakex,snakey,x-1,y-1));
			}

			if (D > 4000) {
				resultD = 2*D - 1;
				snakestartx = (xstart + xend) / 2;;
				snakestarty = snakestartx;
				if (snakestarty < ystart)
					snakestarty = ystart;
				else if (snakestarty > yend)
					snakestarty = yend;
				snakelen = 0;
				goto found_middle_snake;
			}

			/* this is the furthest reaching D-path on the k-diagonal */

			g_Vforw[k] = x;
			if (x > best_forward)
				best_forward = x;

			LOG(("\tlongest %d-path on %d-diagonal ends at x=%lld\n",
					D,k,x));

			/* check overlap with furthest reaching reverse D-1 path */

			if ((Delta & 1) && k - Delta >= -(D-1) && k - Delta <= +(D-1)) {
				if (x >= g_Vback[k - Delta]) {
					LOG(("length of SES is %d\n",2*D - 1));

					resultD = 2*D - 1;
					snakestartx = snakex;
					snakestarty = snakey;
					snakelen = x - snakex;
					goto found_middle_snake;
				}
			}
		}

		for (k = -D; k <= +D; k += 2) {

			LOG(("    k = %d, backward\n",k));

			/* we take a reverse (D-1)-path that ends on a  */
			/* neighbouring diagonal and extend it with a   */
			/* vertical or horizontal edge (whichever takes */
			/* us further)                                  */

			if (k == +D || (k != -D && g_Vback[k-1] < g_Vback[k+1])) {
				/* vertical edge from reverse diagonal k-1 up to k */
				x = g_Vback[k-1];
			}
			else {
				/* horizontal edge from reverse diagonal k+1 left to k */
				x = g_Vback[k+1] - 1;
			}

			/* calculate y using the k-reverse-diagonal equation */
			
			y = x - k + rdiagofs;

			/* append the trailing snake to the D-path */

			snakex = x;
			snakey = y;
			match = match_backward(xstart,ystart,x,y);
			x -= match;
			y -= match;

			if (match)
				LOG(("match from (%lld,%lld) to (%lld,%lld)\n",
						x,y,snakex-1,snakey-1));

			/* this is the furthest reaching reverse D-path on the k-diagonal */

			g_Vback[k] = x;
			if (x < best_backward)
				best_backward = x;

			LOG(("\tlongest reverse %d-path on %d-diagonal ends at x=%lld\n",
					D,k,x));

			/* check overlap with forward reaching D path */

			if (!(Delta & 1) && k + Delta >= -D && k + Delta <= +D) {
				if (x <= g_Vforw[k + Delta]) {
					LOG(("length of SES is %d\n",2*D));

					resultD = 2*D;
					snakestartx = x;
					snakestarty = y;
					snakelen = snakex - x;
					goto found_middle_snake;
				}
			}
		}
	}

	vmlog_die("length of shortest editing script is > %d\n",g_maxD);

found_middle_snake:
	if (opt_verbose && g_level == 0) {
		fprintf(stderr,"shortest editing script D = %d\n",resultD);
	}
	
	if (resultD > 1) {
		g_level++;
		compare(xstart,ystart,snakestartx,snakestarty);
	
		LOG(("middle snake: x=%lld y=%lld len=%lld\n",
				snakestartx,snakestarty,snakelen));

		if (snakelen) {
			handle_common(snakestartx,snakestarty,snakelen);
		}

		compare(snakestartx + snakelen,snakestarty + snakelen,xend,yend);
		g_level--;
	}
	else {
		vmlog_seq_t lendiff = (yend - ystart) - (xend - xstart);
		
		LOG(("snakestartx=%lld snakestarty=%lld snakelen=%lld\n",
				snakestartx,snakestarty,snakelen));
		assert(snakelen == 0);
		assert(lendiff);

		if (lendiff > 0) {
			handle_only_in(1,ystart,lendiff);
		}
		else {
			handle_only_in(0,xstart,-lendiff);
		}
	}
}

static void compare(vmlog_seq_t xstart,vmlog_seq_t ystart,vmlog_seq_t xend,vmlog_seq_t yend)
{
	vmlog_seq_t prefix;
	vmlog_seq_t suffix;
	vmlog_seq_t xdiffend;
	vmlog_seq_t ydiffend;

	LOG(("compare(%lld,%lld,%lld,%lld)\n",
			xstart,ystart,xend,yend));

	/* process and strip common prefix */

	prefix = match_forward(xstart,ystart,xend,yend);
	if (opt_verbose && g_level == 0) {
		fprintf(stderr,"common prefix = " VMLOG_SEQ_FMT "\n",prefix);
	}
	if (prefix) {
		handle_common(xstart,ystart,prefix);
		xstart += prefix;
		ystart += prefix;
	}

	/* strip common suffix */

	suffix = match_backward(xstart,ystart,xend,yend);
	if (opt_verbose && g_level == 0) {
		fprintf(stderr,"common suffix = " VMLOG_SEQ_FMT "\n",suffix);
	}
	xdiffend = xend - suffix;
	ydiffend = yend - suffix;
	

	/* handle differences */

	if (xstart < xdiffend || ystart < ydiffend) {
		if (xstart == xdiffend) {
			handle_only_in(1,ystart,ydiffend - ystart);
		}
		else if (ystart == ydiffend) {
			handle_only_in(0,xstart,xdiffend - xstart);
		}
		else {
			find_middle_snake(xstart,ystart,xdiffend,ydiffend);
		}
	}

	/* process common suffix */

	if (suffix) {
		handle_common(xdiffend,ydiffend,suffix);
	}
}

static void unmatchable_entries(void)
{
	vmlog_log_entry *logent;
	int i;
	int mark;
	vmlog_file cleaned;
	vmlog_file seqs;
	vmlog_seq_t count;
	vmlog_seq_t seq;
	vmlogdiff_removed removed;
	
	VMLOG_XZNEW_ARRAY(g_index_used,char,g_nstrings);

	for (i=0; i<2; ++i) {
		mark = 1<<i;
		
		vmlog_ringbuf_seek(g_ringbuf[i],0);
		while ((logent = vmlog_ringbuf_next(g_ringbuf[i],VMLOGDIFF_PREFETCH))) {
			g_index_used[logent->index] |= mark;
		}
	}

	for (i=0; i<2; ++i) {
		vmlog_file_open(&cleaned,g_cleanedfname[i],vmlogTruncateAppend);
		vmlog_file_open(&seqs,g_removedfname[i],vmlogTruncateAppend);
		
		vmlog_ringbuf_seek(g_ringbuf[i],0);
		count = 0;
		seq = 0;
		removed.start = 0;
		removed.len = 0;
		while ((logent = vmlog_ringbuf_next(g_ringbuf[i],VMLOGDIFF_PREFETCH))) {
			if (g_index_used[logent->index] == 3) {
				vmlog_file_append(&cleaned,logent,sizeof(vmlog_log_entry));
				if (removed.len)
					vmlog_file_append(&seqs,&removed,sizeof(vmlogdiff_removed));
				removed.len = 0;
				removed.start = seq + 1;
			}
			else {
				count++;
				removed.len++;
			}
			seq++;
		}
		if (removed.len)
			vmlog_file_append(&seqs,&removed,sizeof(vmlogdiff_removed));

		vmlog_file_close(&seqs);
		vmlog_file_close(&cleaned);

		if (opt_verbose)
			fprintf(stderr,"removed " VMLOG_SEQ_FMT " unmatchable entries from %s\n",
					count,g_ringbuf[i]->file.fname);

		g_removedmap[i] = vmlog_file_mmap(g_removedfname[i],&(g_removedlen[i]));
		g_nremoved[i] = g_removedlen[i] / sizeof(vmlogdiff_removed);
		g_removedptr[i] = g_removedmap[i];
		g_removedend[i] = g_removedmap[i] + g_nremoved[i];
	}
}

static void diff_logs(void)
{
	int i;

	for (i=0; i<2; ++i) {
		g_ringbuf[i] = vmlog_ringbuf_new(opt_fname[i],VMLOGDIFF_RINGBUF_SIZE);
	}

	unmatchable_entries();
	diff_alloc(200000);

	for (i=0; i<2; ++i) {
		g_ringbuf_orig[i] = g_ringbuf[i];
		g_ringbuf[i] = vmlog_ringbuf_new(g_cleanedfname[i],VMLOGDIFF_RINGBUF_SIZE);
		g_ringbuf_back[i] = vmlog_ringbuf_new(g_cleanedfname[i],VMLOGDIFF_RINGBUF_SIZE);
		g_ringbuf_show[i] = vmlog_ringbuf_new(g_cleanedfname[i],VMLOGDIFF_RINGBUF_SIZE);
	}

	g_n[0] = g_ringbuf[0]->file.size / sizeof(vmlog_log_entry);
	g_n[1] = g_ringbuf[1]->file.size / sizeof(vmlog_log_entry);

	compare(0,0,g_n[0],g_n[1]);
}

int main(int argc,char **argv)
{
	if (argc)
		vmlog_set_progname(argv[0]);
	parse_command_line(argc,argv);

	g_idxfname = vmlog_concat3(opt_prefix,"",".idx",NULL);
	g_strfname = vmlog_concat3(opt_prefix,"",".str",NULL);

	g_idxmap = (vmlog_string_entry *) vmlog_file_mmap(g_idxfname,&g_idxlen);
	g_nstrings = g_idxlen / sizeof(vmlog_string_entry);
	g_strmap = (char *) vmlog_file_mmap(g_strfname,&g_strlen);

	diff_logs();
	
	vmlog_ringbuf_free(g_ringbuf[0]);
	vmlog_ringbuf_free(g_ringbuf[1]);
	g_ringbuf[0] = NULL;
	g_ringbuf[1] = NULL;
	vmlog_file_munmap(g_strmap,g_strlen);
	vmlog_file_munmap(g_idxmap,g_idxlen);
	vmlog_file_munmap(g_removedmap[0],g_removedlen[0]);
	vmlog_file_munmap(g_removedmap[1],g_removedlen[1]);

	return 0;
}

/* vim: noet ts=8 sw=8
 */


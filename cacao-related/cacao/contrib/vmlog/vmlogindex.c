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
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>

char *opt_listfname;
char *opt_prefix;

char *g_idxfname;
char *g_strfname;

const char *usage =
"Usage: vmlogindex prefix listfile\n";

#define VMLOG_MAXLINELENGTH  1024*16

void parse_command_line(int argc,char **argv)
{
	int r;

	while (1) {
		r = getopt(argc,argv,"");
		if (r == -1)
			break;
		switch (r) {
			case '?': vmlog_die("invalid option");
		}
	}

	if (argc - optind < 2)
		vmlog_die_usage(usage,1);

	opt_prefix = argv[optind++];
	opt_listfname = argv[optind++];
}

int main(int argc,char **argv)
{
	int i;
	int len;
	vmlog_log *vml;
	char *buf;
	FILE *listfile;
	char *p;
	int warnws;
	
	if (argc)
		vmlog_set_progname(argv[0]);
	parse_command_line(argc,argv);

	listfile = fopen(opt_listfname,"r");
	if (!listfile)
		vmlog_die("could not open file: %s: %s",opt_listfname,strerror(errno));
	
	vml = vmlog_log_new(opt_prefix,1);

	buf = VMLOG_NEW_ARRAY(char,VMLOG_MAXLINELENGTH+1);
	while (!feof(listfile)) {
		p = fgets(buf,VMLOG_MAXLINELENGTH,listfile);
		if (!p) {
			if (ferror(listfile))
				vmlog_die("reading from file: %s: %s",opt_listfname,strerror(errno));
			assert(feof(listfile));
			break;
		}

		len = strlen(p);
		if (p[len-1] != '\n') {
			vmlog_warn("line without newline or overlong line read");
		}
		else {
			/* chop of newline */
			p[--len] = 0;
		}
		warnws = 0;
		while (isspace(p[len-1])) {
			p[--len] = 0;
			warnws = 1;
		}
		if (warnws) {
			vmlog_warn("removed trailing whitespace from line");
		}
		i = vmlog_get_string_index(vml,buf,len);
	}

	vmlog_log_free(vml);
	fclose(listfile);

	return 0;
}

/* vim: noet ts=8 sw=8
 */


#include "t/prolog.h"

int main(int argc,char **argv) 
{
	char *p;
	int len;
	int r;
	vmlog_seq_t seq,end;
	vmlog_options *opts;
	char **myargv;
	int myargc;

	seq = 7777;
	r = vmlog_opt_parse_seq("123",0,&seq);
	IS(r,0);
	IS(seq,7777);
	
	seq = 7777;
	r = vmlog_opt_parse_seq("123",1,&seq);
	IS(r,1);
	IS(seq,1);
	
	seq = 7777;
	r = vmlog_opt_parse_seq("123",2,&seq);
	IS(r,1);
	IS(seq,12);
	
	seq = 7777;
	r = vmlog_opt_parse_seq("123",3,&seq);
	IS(r,1);
	IS(seq,123);
	
	seq = 7777;
	r = vmlog_opt_parse_seq("123",4,&seq);
	IS(r,1);
	IS(seq,123);
	
	seq = 7777;
	r = vmlog_opt_parse_seq("123x",4,&seq);
	IS(r,0);
	IS(seq,123);

	seq = 7777; end = 8888;
	r = vmlog_opt_parse_range("345",&seq,&end);
	IS(r,1);
	IS(seq,345);
	IS(end,345);

	seq = 7777; end = 8888;
	r = vmlog_opt_parse_range("345:",&seq,&end);
	IS(r,1);
	IS(seq,345);
	IS(end,LLONG_MAX);

	seq = 7777; end = 8888;
	r = vmlog_opt_parse_range(":345",&seq,&end);
	IS(r,1);
	IS(seq,0);
	IS(end,345);

	seq = 7777; end = 8888;
	r = vmlog_opt_parse_range("234:345",&seq,&end);
	IS(r,1);
	IS(seq,234);
	IS(end,345);

	seq = 7777; end = 8888;
	r = vmlog_opt_parse_range("234x:345",&seq,&end);
	IS(r,0);

	VMLOG_XZNEW_ARRAY(myargv,char*,10);

	myargv[0] = "progname";
	myargv[1] = "xyz";
	myargv[2] = "-vmlog:ignore";
	myargv[3] = "testIGN";
	myargv[4] = "bar";
	myargv[5] = "-vmlog:prefix";
	myargv[6] = "theprefix";
	myargv[7] = "last";
	myargc = 8;

	opts = vmlog_opt_parse_cmd_line(&myargc,myargv);

	NOTNULL(opts);
	TRUE(strcmp(opts->progname,"progname") == 0);
	TRUE(strcmp(opts->prefix,"theprefix") == 0);
	TRUE(strcmp(opts->ignoreprefix,"testIGN") == 0);
	TRUE(opts->stringprefix == NULL);

	IS(myargc,4);
	TRUE(strcmp(myargv[0],"progname") == 0);
	TRUE(strcmp(myargv[1],"xyz") == 0);
	TRUE(strcmp(myargv[2],"bar") == 0);
	TRUE(strcmp(myargv[3],"last") == 0);

	finished();
}

/* vim: noet ts=8 sw=8
 */




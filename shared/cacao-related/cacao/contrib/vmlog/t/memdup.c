#include "t/prolog.h"

int main(int argc,char **argv) 
{
	char *str = "this is a test";
	char *p;

	p = (char*) vmlog_memdup(str,strlen(str));
	NOTNULL(p);
	IS(memcmp(p,str,strlen(str)),0);

	p = (char*) vmlog_memdup("",0);
	NOTNULL(p);

	return 0;
}

/* vim: noet ts=8 sw=8
 */


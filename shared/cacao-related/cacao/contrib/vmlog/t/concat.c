#include "t/prolog.h"

int main(int argc,char **argv) 
{
	char *p;
	int len;

	p = vmlog_concat3("foo","","bar",&len);
	NOTNULL(p);
	IS(len,6);
	TRUE(strcmp(p,"foobar") == 0);

	p = vmlog_concat3("","","",&len);
	NOTNULL(p);
	IS(len,0);
	TRUE(strcmp(p,"") == 0);

	p = vmlog_concat3("This is a"," stupid ","test.",&len);
	NOTNULL(p);
	IS(len,strlen("This is a stupid test."));
	TRUE(strcmp(p,"This is a stupid test.") == 0);

	p = vmlog_concat4len("eins",2,"zwei",4,"drei",0,"vier",4,&len);
	NOTNULL(p);
	IS(len,10);
	TRUE(strcmp(p,"eizweivier") == 0);
	
	return 0;
}

/* vim: noet ts=8 sw=8
 */



#include "vmlog.h"
#include "vmlog.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define IS(a,b)  do{ if ((a) == (b)) { printf("ok\n"); } else { printf("FAILED: " #a " == " #b "\n"); } }while(0)
#define TRUE(a)  do{ if ((a)) { printf("ok\n"); } else { printf("FAILED: " #a "\n"); } }while(0)
#define NOTNULL(a)  do{ if ((a) != NULL) { printf("ok\n"); } else { printf("FAILED: " #a " != NULL\n"); } }while(0)

void finished()
{
	printf("finished\n");
	exit(0);
}

/* vim: noet ts=8 sw=8
 */


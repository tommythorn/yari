#include "t/prolog.h"

int main(int argc,char **argv) 
{
	vmlog_log *vml;
	int i;
	int r;
	int fd;
	struct stat st;
	int cumlen;
	void *t1 = (void*)123;
	char *name;
	vmlog_thread_log *tlog;
	vmlog_log_entry logent;

	vml = vmlog_log_new(NULL,1);
	NOTNULL(vml);

	vmlog_file_open(&(vml->idxfile),"TESTIDX",vmlogTruncateAppend);
	vmlog_file_open(&(vml->strfile),"TESTSTR",vmlogTruncateAppend);

	tlog = vmlog_get_thread_log(vml,t1);
	vmlog_file_open(&(tlog->logfile),"TESTLOG1",vmlogTruncateAppend);

	vmlog_log_enter(vml,t1,"foo(II)V",8);
	name = "bar(Ljava/lang/Object;)Z"; vmlog_log_enter(vml,t1,name,strlen(name));
	name = "bar(Ljava/lang/Object;)Z"; vmlog_log_leave(vml,t1,name,strlen(name));
	vmlog_log_leave(vml,t1,"foo(II)V",8);

	vmlog_log_free(vml);
	vml = NULL;

	fd = open("TESTLOG1",O_RDONLY);
	TRUE(fd != -1);
	r = fstat(fd,&st);
	TRUE(r != -1);
	IS(st.st_size,4*sizeof(vmlog_log_entry));

	r = read(fd,&logent,sizeof(vmlog_log_entry));
	TRUE(r == sizeof(vmlog_log_entry));
	IS(logent.tag,VMLOG_TAG_ENTER);
	
	r = read(fd,&logent,sizeof(vmlog_log_entry));
	TRUE(r == sizeof(vmlog_log_entry));
	IS(logent.tag,VMLOG_TAG_ENTER);
	
	r = read(fd,&logent,sizeof(vmlog_log_entry));
	TRUE(r == sizeof(vmlog_log_entry));
	IS(logent.tag,VMLOG_TAG_LEAVE);
	
	r = read(fd,&logent,sizeof(vmlog_log_entry));
	TRUE(r == sizeof(vmlog_log_entry));
	IS(logent.tag,VMLOG_TAG_LEAVE);
	
	close(fd);

	return 0;
}

/* vim: noet ts=8 sw=8
 */




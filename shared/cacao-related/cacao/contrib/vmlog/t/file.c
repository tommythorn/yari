#include "t/prolog.h"

int main(int argc,char **argv) 
{
	vmlog_file file;
	int x;
	int fd;
	struct stat st;
	int r;
	int len;
	char buf[100];

	vmlog_file_open(&file,"TESTFILE",vmlogTruncateAppend);
	IS(file.fnamelen,8);
	TRUE(strcmp(file.fname,"TESTFILE") == 0);

	len = 0;
	IS(file.ofs,0);
	vmlog_file_append(&file,"foo",3); len += 3;
	IS(file.ofs,len);
	vmlog_file_append(&file,"",1); len += 1;
	IS(file.ofs,len);
	vmlog_file_append(&file,"",0);
	IS(file.ofs,len);
	x = 0;
	vmlog_file_append(&file,&x,sizeof(int)); len += sizeof(int);
	IS(file.ofs,len);
	vmlog_file_append(&file,"ENDE",4); len += 4;
	IS(file.ofs,len);
	
	vmlog_file_close(&file);
	IS(file.fd,-1);
	IS(file.fname,NULL);
	IS(file.fnamelen,0);

	fd = open("TESTFILE",O_RDONLY);
	TRUE(fd != -1);
	r = fstat(fd,&st);
	TRUE(r != -1);
	IS(st.st_size,len);

	r = read(fd,buf,len);
	IS(r,len);

	TRUE(memcmp(buf,"foo",3) == 0);
	IS(buf[3],0);
	TRUE(*(int*)(buf+4) == 0);
	TRUE(memcmp(buf+len-4,"ENDE",4) == 0);
	
	close(fd);

	return 0;
}


/* vim: noet ts=8 sw=8
 */


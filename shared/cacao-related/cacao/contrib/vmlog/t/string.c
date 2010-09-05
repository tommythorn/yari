#include "t/prolog.h"

static int destruct_counter = 0;
static int destruct_len = 0;

static void test_destuctor(vmlog_hash_entry *entry) {
	if (entry->data) {
		destruct_counter++;
		destruct_len += entry->len;
	}
	else {
		IS(entry->len,0);
		IS(entry->index,0);
	}
}

int main(int argc,char **argv) 
{
	vmlog_log *vml;
	int i;
	int r;
	int fd;
	struct stat st;
	int cumlen;

	vml = vmlog_log_new(NULL,1);
	NOTNULL(vml);

	vmlog_file_open(&(vml->idxfile),"TESTIDX",vmlogTruncateAppend);
	vmlog_file_open(&(vml->strfile),"TESTSTR",vmlogTruncateAppend);

	cumlen = 0;
	i = vmlog_get_string_index(vml,"foo",3);
	IS(i,0);
	IS(vml->stringhash.nentries,1);
	cumlen += 3;

	i = vmlog_get_string_index(vml,"foo",3);
	IS(i,0);
	IS(vml->stringhash.nentries,1);

	i = vmlog_get_string_index(vml,"bar",3);
	IS(i,1);
	IS(vml->stringhash.nentries,2);
	cumlen += 3;

	i = vmlog_get_string_index(vml,"foo",3);
	IS(i,0);
	IS(vml->stringhash.nentries,2);

	i = vmlog_get_string_index(vml,"",0);
	IS(i,2);
	IS(vml->stringhash.nentries,3);

	i = vmlog_get_string_index(vml,"",0);
	IS(i,2);
	IS(vml->stringhash.nentries,3);

	vmlog_hashtable_free(&(vml->stringhash),test_destuctor);
	IS(destruct_counter,3);

	vmlog_file_close(&(vml->idxfile));
	vmlog_file_close(&(vml->strfile));

	fd = open("TESTIDX",O_RDONLY);
	TRUE(fd != -1);
	r = fstat(fd,&st);
	IS(st.st_size,3*sizeof(vmlog_string_entry));
	close(fd);

	fd = open("TESTSTR",O_RDONLY);
	TRUE(fd != -1);
	r = fstat(fd,&st);
	IS(st.st_size,cumlen);
	close(fd);

	return 0;
}

/* vim: noet ts=8 sw=8
 */



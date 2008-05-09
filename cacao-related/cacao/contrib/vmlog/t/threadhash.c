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
	vmlog_thread_log *tlog;
	vmlog_thread_log *tlog1;
	int i;
	vmlog_thread_log *tlogs[100];

	vml = vmlog_log_new(NULL,1);
	NOTNULL(vml);

	tlog = vmlog_get_thread_log(vml,(void*)123);
	tlog1 = tlog;
	NOTNULL(tlog);
	IS(vml->threadhash.nentries,1);
	IS(tlog->threadid,(void*)123);

	tlog = vmlog_get_thread_log(vml,(void*)0);
	NOTNULL(tlog);
	IS(vml->threadhash.nentries,2);
	IS(tlog->threadid,(void*)0);

	tlog = vmlog_get_thread_log(vml,(void*)123);
	NOTNULL(tlog);
	IS(vml->threadhash.nentries,2);
	IS(tlog,tlog1);
	IS(tlog->threadid,(void*)123);

	vmlog_hashtable_free(&(vml->threadhash),test_destuctor);
	IS(destruct_counter,2);

	vmlog_hashtable_init(&(vml->threadhash),5);
	for (i=0; i<10; ++i) {
		tlog = vmlog_get_thread_log(vml,(void*)(1000+i));
		NOTNULL(tlog);
		tlogs[i] = tlog;
	}
	for (i=0; i<10; ++i) {
		tlog = vmlog_get_thread_log(vml,(void*)(1000+i));
		IS(tlog,tlogs[i]);
	}
	destruct_counter = 0;
	vmlog_hashtable_free(&(vml->threadhash),test_destuctor);
	IS(destruct_counter,10);

	return 0;
}

/* vim: noet ts=8 sw=8
 */



#include "t/prolog.h"

static int destruct_counter = 0;

static void test_destuctor(vmlog_hash_entry *entry) {
	printf("# destroying entry %p\n",(void *)entry);
	destruct_counter++;
}

int main(int argc,char **argv) 
{
	vmlog_hash_table ht;

	vmlog_hashtable_init(&ht,10);

	IS(ht.size,10);
	IS(ht.nentries,0);

	vmlog_hashtable_free(&ht,test_destuctor);

	IS(destruct_counter,10);
	IS(ht.size,0);
	IS(ht.nentries,0);
	IS(ht.table,0);

	return 0;
}


/* vim: noet ts=8 sw=8
 */

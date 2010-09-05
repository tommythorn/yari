#include "t/prolog.h"

const char *testfname = "TESTFILE";
const int testdelta = 1000000;
const int testsize = 100;
const int testbufsize = 11;

vmlog_ringbuf *ring;
	
static void prepare_test_file(void)
{
	vmlog_log_entry logent;
	vmlog_file file;
	int i;

	vmlog_file_open(&file,testfname,vmlogTruncateAppend);
	for (i=0; i<testsize; ++i) {
		logent.tag = i % 5;
		logent.index = i + testdelta;
		vmlog_file_append(&file,&logent,sizeof(vmlog_log_entry));
	}

	vmlog_file_close(&file);
}

static void check_logent(vmlog_log_entry *logent,int seq)
{
	TRUE(logent->tag == seq % 5 && logent->index == seq + testdelta);
	if (!(logent->tag == seq % 5 && logent->index == seq + testdelta)) {
		fprintf(stdout,"THE FAILED ONE:\n");
		vmlog_ringbuf_visualize(ring);
	}
}

static void check_ringbuf(void)
{
	int i;
	int n;
	vmlog_log_entry *logent;

	vmlog_ringbuf_check_invariants(ring);

	n = ring->debug_availbefore;
	logent = ring->cur;
	for (i=0; i<n; ++i) {
		if (--logent < ring->buf)
			logent = ring->bufend - 1;

		check_logent(logent,ring->seq - i - 1);
	}
	
	n = ring->debug_availafter;
	logent = ring->cur;
	for (i=0; i<n; ++i) {
		check_logent(logent,ring->seq + i);
		
		if (++logent >= ring->bufend)
			logent = ring->buf;
	}
}

static void test_forward_iteration(int base,int n,int expect)
{
	vmlog_log_entry *logent;
	int count;

	count = 0;
	check_ringbuf();
	while ((count < n) && (logent = vmlog_ringbuf_next(ring,1 + rand()%7))) {
		check_logent(logent,base + count);
		check_ringbuf();

		count++;
	}
	check_ringbuf();

	IS(count,expect);
}

static void test_backward_iteration(int base,int n,int expect)
{
	vmlog_log_entry *logent;
	int count;

	count = 0;
	check_ringbuf();
	while ((count < n) && (logent = vmlog_ringbuf_prev(ring,1 + rand()%7))) {
		check_logent(logent,base - count - 1);
		check_ringbuf();

		count++;
	}
	check_ringbuf();

	IS(count,expect);
}

#define MIN(a,b)  (((a) <= (b)) ? (a) : (b))

static void test_random_walk(void)
{
	int i;
	int steps;
	int base;
	
	base = testsize/2;
	vmlog_ringbuf_seek(ring,base);

	for (i=0; i<1000; ++i) {
		steps = rand()%20 - 10;
		
		if (steps > 0 || (steps == 0 && rand()%2)) {
			test_forward_iteration(base,steps,MIN(steps,testsize - base));
			base += MIN(steps,testsize - base);
		}
		else {
			test_backward_iteration(base,-steps,MIN(-steps,base));
			base -= MIN(-steps,base);
		}
	}
}

int main(int argc,char **argv) 
{
	prepare_test_file();

	ring = vmlog_ringbuf_new(testfname,testbufsize);

	test_forward_iteration(0,testsize+7,testsize);
	test_backward_iteration(testsize,testsize+7,testsize);
	test_random_walk();	

	finished();
}

/* vim: noet ts=8 sw=8
 */

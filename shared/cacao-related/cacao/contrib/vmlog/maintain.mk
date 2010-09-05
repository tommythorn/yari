VERSION=0.0.5
RELEASENAME=vmlog-$(VERSION)
TARBALL=releases/$(RELEASENAME).tar.gz

CFLAGS=-g -ansi -std=c99 -pedantic -Wall -Wno-long-long -Wno-unused-function -I/home/edwin/local/classpath/include
#CFLAGS_TOOL=-O2 -DNDEBUG -ansi -std=c99 -march=pentium4m -pedantic -Wall -Wno-long-long
CFLAGS_TOOL=-g3 -pedantic -ansi -std=c99 -Wall -Wno-long-long -Wno-unused-function

all: vmlog.o vmlogdump vmlogindex vmlogdiff

runtests: t/hash.run t/memdup.run t/string.run t/threadhash.run t/file.run t/log.run t/concat.run t/ring.run t/opt.run t/tags.run

test: cleantests runtests
	cat TESTRESULTS

cleantests:
	@rm -f TESTIDX TESTSTR TESTLOG? TESTFILE TESTRESULTS
	
%.run: %.c t/prolog.h vmlog.c vmlog.h
	$(CC) $(CFLAGS) -I. -o $* $<
	echo $* >>TESTRESULTS
	{ $* || echo "FAILED: exitcode == $$?" ; } | uniq -c >>TESTRESULTS

vmlogdump: vmlogdump.c vmlog.c vmlog.h
	$(CC) $(CFLAGS_TOOL) -o $@ vmlogdump.c vmlog.c
vmlogindex: vmlogindex.c vmlog.c vmlog.h
	$(CC) $(CFLAGS_TOOL) -o $@ vmlogindex.c vmlog.c
vmlogdiff: vmlogdiff.c vmlog.c vmlog.h
	$(CC) $(CFLAGS_TOOL) -o $@ vmlogdiff.c vmlog.c

cscope:
	cscope -b *.[ch] t/*.[ch]

clean: cleantests
	rm -f *.o
	rm -f vmlogdump vmlogindex vmlogdiff
	rm -f cscope.out
	rm -f t/hash t/memdup t/string t/threadhash t/file t/log t/concat t/ring t/opt t/tags

checkedin:
	if (cg-status | grep ^[AMD]) ; then echo "difference to repository!"; exit 1; else /bin/true ; fi

distprep: clean all test clean
	mkdir $(RELEASENAME)
	cp -a --parents `git-ls-files` $(RELEASENAME)
	tar cfz $(TARBALL) $(RELEASENAME)/*
	rm -rf $(RELEASENAME)

dist: checkedin distprep
	cg-tag $(VERSION)

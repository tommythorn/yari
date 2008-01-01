CFLAGS=-g -Wall -MD -O2
TESTPROG=../gdbstub/gdbstub.mips
FLAGS=

all: runmips

run: runmips
	./runmips $(FLAGS) $(TESTPROG)

tests: tests.c
	mips-elf-gcc -O -msoft-float -Tmymips.ld $< -o $@

tests.c:
	../testcases/testgen > tests.c

all_: runmips output
	./runmips --simple output

output: output.c Makefile mymips.ld
	mips-elf-gcc -msoft-float -Tmymips.ld output.c -o output

runmips: runmips.o support.o run_simple.o

clean:
	-rm *.o *.d runmips

realclean: clean
	-rm *~

-include *.d

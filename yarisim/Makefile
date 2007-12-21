CFLAGS=-g -Wall -MD -O2
TESTPROG=please-set-TESTPROG
VERB=

all: sim

run: sim
	./sim $(VERB) $(TESTPROG)

tests: tests.c
	mips-elf-gcc -O -msoft-float -Tmymips.ld $< -o $@

tests.c:
	../testcases/testgen > tests.c

all_: sim output
	./sim --simple output

output: output.c Makefile mymips.ld
	mips-elf-gcc -msoft-float -Tmymips.ld output.c -o output

sim: sim.o support.o run_simple.o

clean:
	-rm *.o *.d sim

realclean: clean
	-rm *~

-include *.d

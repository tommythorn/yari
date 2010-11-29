include ../config.mk

CFLAGS=-g -Wall -Werror -MD -O2 $(shell sdl-config --cflags)
LDFLAGS=$(shell sdl-config --libs)
TESTPROG=please-set-TESTPROG
FLAGS=

all: yarisim

install: yarisim
	cp yarisim $(PREFIX)/bin

run: yarisim
	yarisim $(FLAGS) $(TESTPROG) $(FIRMWARE)

tests: tests.c
	mips-elf-gcc -O -msoft-float -Tmymips.ld $< -o $@

tests.c:
	../testcases/testgen > tests.c

all_: yarisim output
	./yarisim --simple output

output: output.c Makefile mymips.ld
	mips-elf-gcc -msoft-float -Tmymips.ld output.c -o output

yarisim: sim.o support.o run_simple.o
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	-rm *.o *.d yarisim

realclean: clean
	-rm *~

-include *.d

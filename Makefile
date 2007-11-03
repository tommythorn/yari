# XXX Needs a proper multi-level build system

# No suffix or path!!
TESTPROG=testall
#TESTPROG=testicache
#TESTPROG=testeret

VERB=--verb

usage:
	@echo Usage:
	@echo '   make isasim -- run a software simulation of the test program'
	@echo '   make rtlsim -- run a simulated the RTL on the test program'
	@echo Make variables:
	@echo "TESTPROG -- the program to run. Default is $(TESTPROG)"
	@echo "VERB     -- Options for the simulator.  Default is $(VERB)."

isasim:
	make -C testcases $(TESTPROG)-prom.mips
	make -C yarisim VERB=$(VERB) TESTPROG=../testcases/$(TESTPROG)-prom.mips run

rtlsim:
	make -C testcases PROG=$(TESTPROG) promote
	make -C rtl/target/Icarus simulate

cosim:
	make -C testcases PROG=$(TESTPROG) $(TESTPROG)-prom.mips promote
	make -C rtl/target/Icarus simulate | \
	make -C yarisim VERB="--cosim $(VERB)" TESTPROG=../testcases/$(TESTPROG)-prom.mips run


clean:
	-make -C yarisim clean
	-make -C testcases clean
	-make -C gdbstub clean
	-make -C firmware clean
	-make -C rtl/target/Icarus clean

realclean:
	-find . -name \*~|xargs rm
	-make -C yarisim realclean
	-make -C testcases realclean
	-make -C gdbstub realclean
	-make -C firmware realclean
	-make -C rtl/target/Icarus realclean

tabcheck:
	@grep --perl '\t' `find . -name \*.v` || echo All ok

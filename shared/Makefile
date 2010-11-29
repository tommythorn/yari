# XXX Needs a proper multi-level build system

include config.mk

# export these variables for the BUILD-xxx scripts
export YARI_XTOOLS_TARGET
export PREFIX
export YARI_XTOOLS_SUDO
export YARI_XTOOLS_BUILDDIR
export PATH
export MAKE_OPTION


# No suffix or path!!
TESTPROG=testall
#TESTPROG=testicache
#TESTPROG=testeret

VERB=--verb

usage:
	@echo Usage:
	@echo '   make install-tools -- Install compiler, libraries, and simulator'
	@echo '   make isasim -- run a software simulation of the test program'
	@echo '   make rtlsim -- run a simulated the RTL on the test program'
	@echo Make variables:
	@echo "TESTPROG -- the program to run. Default is $(TESTPROG)"
	@echo "VERB     -- Options for the simulator.  Default is $(VERB)."

install-gdb:
	cd tools; ./BUILD-gdb.sh

install-sdl:
	$(MAKE) -C SDL-1.2 install

install-binutils: $(PREFIX)/bin/$(YARI_XTOOLS_TARGET)-as
	cd tools; ./BUILD-binutils.sh

install-gcc: $(PREFIX)/bin/$(YARI_XTOOLS_TARGET)-gcc
	cd tools; ./BUILD-gcc.sh

install-newlib: $(PREFIX)/$(YARI_XTOOLS_TARGET)/lib/yari.ld
	cd tools; ./BUILD-newlib.sh

install-yarisim:
	$(MAKE) -C yarisim install

install-tools: install-yarisim install-binutils install-gcc install-newlib install-gdb install-sdl

isasim:
	$(MAKE) -C testcases $(TESTPROG)-prom.mips
	$(MAKE) -C yarisim FLAGS=$(VERB) TESTPROG=../testcases/$(TESTPROG)-prom.mips run

rtlsim:
	$(MAKE) -C testcases PROG=$(TESTPROG) promote
	$(MAKE) -C rtl/target/Icarus simulate

cosim:
	$(MAKE) -C testcases PROG=$(TESTPROG) $(TESTPROG)-prom.mips promote
	$(MAKE) -C rtl/target/Icarus simulate | \
	$(MAKE) -C yarisim FLAGS="--cosim $(VERB)" TESTPROG=../testcases/$(TESTPROG)-prom.mips run


clean:
	-$(MAKE) -C yarisim clean
	-$(MAKE) -C testcases clean
	-$(MAKE) -C gdbstub clean
	-$(MAKE) -C firmware clean
	-$(MAKE) -C rtl/target/Icarus clean

realclean:
	-find . -name \*~|xargs rm
	-$(MAKE) -C yarisim realclean
	-$(MAKE) -C testcases realclean
	-$(MAKE) -C gdbstub realclean
	-$(MAKE) -C firmware realclean
	-$(MAKE) -C rtl/target/Icarus realclean

tabcheck:
	@grep --perl '\t' `find . -name \*.v` || echo All ok

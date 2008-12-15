# Please change to match your preferences
export YARI_XTOOLS_INSTALL_PREFIX=/opt/$(YARI_XTOOLS_TARGET)
export YARI_XTOOLS_SUDO= #sudo
export YARI_XTOOLS_BUILDDIR=/tmp/build-xtools/build
export YARI_XTOOLS_TARGET=mips-elf

export PATH:=$(YARI_XTOOL_INSTALL_PREFIX)/bin:$(PATH)
export MAKE_OPTION=-j4
# enable this if you install into a directory that you don't own

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

install-gdb:
	cd xtools; ./BUILD-gdb.sh

install-sdl:
	$(MAKE) -C SDL-1.2 install

install-binutils:
	cd xtools; ./BUILD-binutils.sh

install-gcc:
	cd xtools; ./BUILD-gcc.sh

install-newlib:
	cd xtools; ./BUILD-newlib.sh

install-yarisim:
	$(MAKE) -C yarisim install

install-xtools: install-binutils install-gcc install-newlib install-gdb install-sdl install-yarisim

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

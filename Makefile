# XXX Needs a proper multi-level build system

# No suffix or path!!
TESTPROG=testall-prom
#TESTPROG=testicache-prom
#TESTPROG=testeret-prom

ISASIMFLAGS=

usage:
	@echo Usage:
	@echo '   make isasim  -- run a software simulation of the test program'
	@echo '   make rtlsim  -- run a simulated the RTL on the test program'
	@echo Make variables:
	@echo "TESTPROG    -- the program to run. Default is $(TESTPROG)"
	@echo "ISASIMFLAGS -- Options for the MIPS simulator.  Default is none."

isasim:
	cd testcases; make $(TESTPROG).mips
	cd swsim; make FLAGS=$(ISASIMFLAGS) TESTPROG=../testcases/$(TESTPROG).mips run

rtlsim:
	cd swsim;      make runmips
	cd testcases;  make $(TESTPROG).data $(TESTPROG).mif
	cp testcases/$(TESTPROG).mif rtl/onnama/ep1c20/initmem.mif
	cd rtl/Icarus; make TESTPROG=../../testcases/$(TESTPROG).data simulate

clean:
	-(cd swsim;make clean)
	-(cd testcases;make clean)
	-(cd gdbstub;make clean)
	-(cd firmware;make clean)
	-(cd rtl/Icarus;make clean)

realclean:
	-find . -name \*~|xargs rm
	-(cd swsim;make realclean)
	-(cd testcases;make realclean)
	-(cd gdbstub;make realclean)
	-(cd firmware;make realclean)
	-(cd rtl/Icarus;make realclean)

tabcheck:
	@grep --perl '\t' `find . -name \*.v` || echo All ok

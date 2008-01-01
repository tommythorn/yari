# -----------------------------------------------------------------------
#
#   Copyright 2004 Tommy Thorn - All Rights Reserved
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
#   Bostom MA 02111-1307, USA; either version 2 of the License, or
#   (at your option) any later version; incorporated herein by reference.
#
# -----------------------------------------------------------------------

CORESRC:=mipscore.v stage_I.v stage_D.v stage_X.v stage_M.v

SYSTEMSRC:=pipeconnect.v main.v \
    bus_ctrl.v sram_ctrl.v \
    hexled.v \
    vga.v blockram.v \
    peri_ctrl.v rs232.v rs232out.v rs232in.v

#ep1c20/fbfifo.v \

# Target logic that we model
TARGETSRC:=\
    pll.v altsyncram.v \
    arithshiftbidir.v logshiftright.v idt71v416s10.v

SRC:=$(patsubst %,../onnama/mipscore/%,$(CORESRC)) \
     $(patsubst %,../onnama/%,$(SYSTEMSRC)) \
     $(TARGETSRC)

TESTPROG=../../testcases/testall.data # XXX shouldn't be defined here
IVERILOGOPTS=-Wall -I../onnama -I../onnama/mipscore

all: simulate

simulate: $(SRC) Makefile
	cp $(TESTPROG) program.data
	iverilog -o main.test $(IVERILOGOPTS) -DSIMULATE_MAIN -DTRACE_SERIAL $(SRC)
	./main.test

program.data: $(TESTPROG)
	cp $(TESTPROG) program.data

#all: # phony #stage_D.test
#	cd ..;make

mult:
	iverilog mult.v -o mult.test
	./mult.test

testpipe: ../pipeconnect.v
	iverilog -I .. ../pipeconnect.v testpipe.v -o testpipe.test
	./testpipe.test

testpipe2: ../pipeconnect.v
	iverilog -I .. ../pipeconnect.v testpipe2.v -o testpipe2.test
	./testpipe2.test

testpipe3: ../pipeconnect.v
	iverilog -I .. ../pipeconnect.v testpipe3.v -o testpipe3.test
	./testpipe3.test


stage_I.test: ../stage_I.v coderom.v
	iverilog -DSIMULATE ../stage_I.v coderom.v -o stage_I.test
	./stage_I.test

stage_D.test: stage_D.v coderom.v
	iverilog -DSIMULATE stage_D.v coderom.v -o stage_D.test
	./stage_D.test

test1: pipeline1
	cp ../testcases/tests.data program.data
	./pipeline1

rs232out: rs232out.v
	iverilog -DSIMULATE rs232out.v -o rs232out
	./rs232out

pipeline1: pipeline1.v
	iverilog pipeline1.v -o pipeline1

clean:
	-rm main.test

realclean: clean
	-rm *~ *.data *.txt a.out

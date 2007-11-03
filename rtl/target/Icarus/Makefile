# -----------------------------------------------------------------------
#
#   Copyright 2004,2007 Tommy Thorn - All Rights Reserved
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
#   Bostom MA 02111-1307, USA; either version 2 of the License, or
#   (at your option) any later version; incorporated herein by reference.
#
# -----------------------------------------------------------------------

IC_LINE_INDEX_BITS=7    # Each set has 128 lines
IC_WORD_INDEX_BITS=2    # Each line has 4 32-bit words (128 bits)
DC_LINE_INDEX_BITS=8    # Each set has 256 lines
DC_WORD_INDEX_BITS=2    # Each line has 4 32-bit words (128 bits)

YARISRC:=yari.v stage_I.v stage_D.v stage_X.v stage_M.v

SYSTEMSRC:=pipeconnect.v  \
    bus_ctrl.v sram_ctrl.v \
    hexled.v \
    vga.v blockram.v \
    peri_ctrl.v rs232.v rs232out.v rs232in.v

PRIMSRC:=simpledpram.v dpram.v

# Target logic that we model
TARGETSRC:=\
    ../niosdevkit-1c20/main.v \
    pll.v altsyncram.v \
    arithshiftbidir.v logshiftright.v idt71v416s10.v

SRC:=$(patsubst %,../../yari-core/%,$(YARISRC)) \
     $(patsubst %,../../soclib/%,$(SYSTEMSRC)) \
     $(PRIMSRC) \
     $(TARGETSRC)

IVERILOGOPTS=-Wall -I../../soclib -I../../yari-core

all: simulate

config.h: Makefile ../../makeconfig.sh
	../../makeconfig.sh \
		$(IC_LINE_INDEX_BITS) $(IC_WORD_INDEX_BITS) \
		$(DC_LINE_INDEX_BITS) $(DC_WORD_INDEX_BITS)

simulate: $(SRC) config.h Makefile
	iverilog -o main.test $(IVERILOGOPTS) -DSIMULATE_MAIN -DTRACE_SERIAL $(SRC)
	./main.test

mult:
	iverilog mult.v -o mult.test
	./mult.test

testpipe: ../../soclib/pipeconnect.v
	iverilog -I .. ../../soclib/pipeconnect.v testpipe.v -o testpipe.test
	./testpipe.test

testpipe2: ../../soclib/pipeconnect.v
	iverilog -I .. ../../soclib/pipeconnect.v testpipe2.v -o testpipe2.test
	./testpipe2.test

testpipe3: ../../soclib/pipeconnect.v
	iverilog -I .. ../../soclib/pipeconnect.v testpipe3.v -o testpipe3.test
	./testpipe3.test


stage_I.test: ../../yari-core/stage_I.v coderom.v
	iverilog -DSIMULATE ../../yari-core/stage_I.v coderom.v -o stage_I.test
	./stage_I.test

stage_D.test: stage_D.v coderom.v
	iverilog -DSIMULATE stage_D.v coderom.v -o stage_D.test
	./stage_D.test

test1: pipeline1
	cp ../../../testcases/tests.data initmem.data
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

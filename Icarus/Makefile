# -----------------------------------------------------------------------
#
#   Copyright 2004,2007,2010 Tommy Thorn - All Rights Reserved
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
#   Bostom MA 02111-1307, USA; either version 2 of the License, or
#   (at your option) any later version; incorporated herein by reference.
#
# -----------------------------------------------------------------------

TOPDIR=..
include default.conf
include ../shared/shared.mk

YARISRC:=yari.v stage_I.v stage_D.v stage_X.v stage_M.v

SYSTEMSRC:=pipeconnect.v  \
    bus_ctrl.v sram_ctrl.v \
    hexled.v \
    vga.v blockram.v \
    peri_ctrl.v rs232.v rs232out.v rs232in.v

PRIMSRC:=simpledpram.v dpram.v

# Target logic that we model
TARGETSRC:=\
    toplevel.v \
    pll.v altsyncram.v \
    arithshiftbidir.v logshiftright.v

# Change IDT_SRAM_MODEL to point to the IDT SRAM model if one is available
IDT_SRAM_MODEL=

SRC:=$(patsubst %,../../shared/rtl/yari-core/%,$(YARISRC)) \
     $(patsubst %,../../shared/rtl/soclib/%,$(SYSTEMSRC)) \
     $(PRIMSRC) \
     $(TARGETSRC) \
     $(IDT_SRAM_MODEL)

IVERILOGOPTS=-Wall -g2005 -I../../shared/rtl/soclib -I../../shared/rtl/yari-core

all: simulate

simulate: $(patsubst %,rtl/%,$(SRC)) rtl/config.h Makefile rtl/icache_ram0.data
	cd rtl; iverilog -o main.test \
		$(IVERILOGOPTS) \
		-DSIMULATE_MAIN -DTRACE_SERIAL -DSRAM_INIT=$(SRAM_INIT) $(SRC)
	cd rtl; ./main.test

rtl/icache_ram0.data: tinymon.mips $(YARISIM)
	cd rtl; ../$(YARISIM)                         \
		--data                                \
		--icache-way=$(IC_LINE_INDEX_BITS)    \
		--icache-words=$(IC_WORD_INDEX_BITS)  \
		--dcache-way=$(DC_LINE_INDEX_BITS)    \
		--dcache-words=$(DC_WORD_INDEX_BITS)  \
		../tinymon.mips

mult:
	iverilog mult.v -o mult.test
	./mult.test

testpipe: ../shared/rtl/soclib/pipeconnect.v
	iverilog -I .. ../shared/rtl/soclib/pipeconnect.v testpipe.v -o testpipe.test
	./testpipe.test

testpipe2: ../shared/rtl/soclib/pipeconnect.v
	iverilog -I .. ../shared/rtl/soclib/pipeconnect.v testpipe2.v -o testpipe2.test
	./testpipe2.test

testpipe3: ../shared/rtl/soclib/pipeconnect.v
	iverilog -I .. ../shared/rtl/soclib/pipeconnect.v testpipe3.v -o testpipe3.test
	./testpipe3.test


stage_I.test: ../shared/rtl/yari-core/stage_I.v coderom.v
	iverilog -DSIMULATE ../shared/rtl/yari-core/stage_I.v coderom.v -o stage_I.test
	./stage_I.test

stage_D.test: stage_D.v coderom.v
	iverilog -DSIMULATE stage_D.v coderom.v -o stage_D.test
	./stage_D.test

test1: pipeline1
	cp ../shared/rtl/../testcases/tests.data initmem.data
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

tinymon.mips-o: tinymon.c
	mips-elf-gcc -D_mips_ -msoft-float -c -Os $< -o $@

tinymon.mips: tinymon.mips-o
	mips-elf-ld -Tprom.ld $< -o $@

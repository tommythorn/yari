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

SRC=main.v mipscore.v stage_I.v stage_D.v stage_X.v stage_M.v stage_W.v \
    bus_ctrl.v sram_ctrl.v \
    hexled.v mega/dcache.v mega/fbfifo.v \
    vga.v blockram.v \
    peri_ctrl.v rs232.v rs232out.v rs232in.v  \
    Icarus/altsyncram.v Icarus/pll.v \
    Icarus/arithshiftbidir.v Icarus/logshiftright.v Icarus/idt71v416s10.v \
    pipeconnect.v

TESTPROG=../testcases/sieve.data
IVERILOGOPTS=-Wall

all: serial

Icarus/program.data: $(TESTPROG)
	cp $(TESTPROG) Icarus/program.data

external: $(SRC) Icarus/program.data
	iverilog $(IVERILOGOPTS) -DEXTERNAL_SIMULATION $(SRC) -o main.test
	./main.test

detailed: $(SRC) Icarus/program.data
	iverilog $(IVERILOGOPTS) -DSIMULATE_MAIN -DDETAILED_SIMUL $(SRC) -o main.test
	./main.test

serial: $(SRC) Icarus/program.data
	iverilog $(IVERILOGOPTS) -DSIMULATE_MAIN -DTRACE_SERIAL $(SRC) -o main.test
	./main.test

regsonly: $(SRC) Icarus/program.data
	iverilog $(IVERILOGOPTS) -DSIMULATE_MAIN -DREGTRACE $(SRC) -o main.test
	./main.test

sram_ctrl:
	iverilog $(IVERILOGOPTS) -DSIMULATE_SRAM_CTRL sram_ctrl.v Icarus/idt71v416s10.v \
		-o sram_ctrl.test
	./sram_ctrl.test

vga:
	iverilog $(IVERILOGOPTS) -DSIMULATE_VGA vga.v mega/fbfifo.v Icarus/altsyncram.v \
		-o vga.test
	./vga.test

rs232in:
	iverilog $(IVERILOGOPTS) -DSIMULATE_RS232IN rs232out.v rs232in.v -o rs232in.test
	./rs232in.test

listing.ps: $(SRC) Icarus/program.data Makefile
	a2ps -l90 -o listing.ps \
		stage_I.v stage_D.v stage_X.v stage_M.v stage_W.v \
		sram_ctrl.v vga.v rs232in.v rs232out.v main.v \
		/tmp/bus_ctrl.v

preview: listing.ps
	gv listing.ps

realclean:
	-rm *.done *.fsf *.mif *.rpt *.eqn *.test *.pof *.ini *.pin *.ps

#all:
#	cd Icarus; make $(MAKECMDGOALS)

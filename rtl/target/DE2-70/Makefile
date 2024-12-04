# -----------------------------------------------------------------------
#
#   Copyright 2007,2008 Tommy Thorn - All Rights Reserved
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
DC_LINE_INDEX_BITS=7    # Each set has 256 lines
DC_WORD_INDEX_BITS=2    # Each line has 4 32-bit words (128 bits)

all: program

# Funny, the 13 year old quartus_pgm failed, but modern quartus_pgm
# doesn't have a problem programming a higher-end part
program: main.sof
	time quartus_pgm main.cdf

main.sof: main.v
	time quartus9.1sp2_map main
	time quartus9.1sp2_fit main
	time quartus9.1sp2_sta main
	grep MHz main.sta.rpt
	time quartus9.1sp2_asm main

# This didn't pan out as openFPGALoader doesn't know how to program
# this, but keeping here for future reference
# https://trabucayre.github.io/openFPGALoader/vendors/intel.html
%.rbf: %.sof
	quartus9.1sp2_cpf --option=bitstream_compression=off -c $< $@

%.svf: %.sof
	quartus9.1sp2_cpf -c -q 12.0MHz -g 3.3 -n p  $< $@

config.h: Makefile ../../makeconfig.sh
	../../makeconfig.sh \
		$(IC_LINE_INDEX_BITS) $(IC_WORD_INDEX_BITS) \
		$(DC_LINE_INDEX_BITS) $(DC_WORD_INDEX_BITS)

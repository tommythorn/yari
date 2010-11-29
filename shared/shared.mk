# -----------------------------------------------------------------------
#
#   Copyright 2010 Tommy Thorn - All Rights Reserved
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
#   Bostom MA 02111-1307, USA; either version 2 of the License, or
#   (at your option) any later version; incorporated herein by reference.
#
# -----------------------------------------------------------------------

MAKECONFIG=$(TOPDIR)/shared/tools/makeconfig.sh
YARISIM ?= $(TOPDIR)/shared/yarisim/yarisim
FIRMWARE ?= tinymon.mips

all: rtl/config.h rtl/icache_ram0.mif

rtl/config.h: $(MAKECONFIG) default.conf
	cd rtl; ../$(MAKECONFIG) ../default.conf

rtl/icache_ram0.mif: tinymon.mips $(YARISIM)
	cd rtl; ../$(YARISIM)                         \
		--mif                                 \
		--icache-way=$(IC_LINE_INDEX_BITS)    \
		--icache-words=$(IC_WORD_INDEX_BITS)  \
		--dcache-way=$(DC_LINE_INDEX_BITS)    \
		--dcache-words=$(DC_WORD_INDEX_BITS)  \
		../$(FIRMWARE)

tinymon.mips-o: $(TOPDIR)/shared/firmware/tinymon.c
	mips-elf-gcc -D_mips_ -msoft-float -c -Os $< -o $@

tinymon.mips: tinymon.mips-o
	mips-elf-ld -T$(TOPDIR)/shared/firmware/prom.ld $< -o $@

yarisim:
	make -C $(TOPDIR)/shared/yarisim

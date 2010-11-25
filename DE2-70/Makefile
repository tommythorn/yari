# -----------------------------------------------------------------------
#
#   Copyright 2007,2010 Tommy Thorn - All Rights Reserved
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
QFILTER=|grep -v '^    '

all: rtl/yarisoc.sof

program: rtl/yarisoc.sof
	cd rtl; quartus_pgm yarisoc.cdf

rtl/yarisoc.sof: rtl/config.h rtl/icache_ram0.mif rtl/toplevel.v
	cd rtl; quartus_map yarisoc.qsf $(QFILTER)
	cd rtl; quartus_fit yarisoc.qsf $(QFILTER)
	cd rtl; quartus_asm yarisoc.qsf $(QFILTER)

include ../shared/shared.mk

tinymon.mips-o: tinymon.c
	mips-elf-gcc -D_mips_ -msoft-float -c -Os $< -o $@

tinymon.mips: tinymon.mips-o
	mips-elf-ld -Tprom.ld $< -o $@

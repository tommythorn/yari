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
QFILTER=|grep -v '^    '|egrep -i 'warning|error'
YARISRC=../shared/rtl/yari-core/yari.v \
	../shared/rtl/yari-core/stage_I.v \
	../shared/rtl/yari-core/stage_D.v \
	../shared/rtl/yari-core/stage_X.v \
	../shared/rtl/yari-core/stage_M.v \
	../shared/rtl/yari-core/asm.v \
	../shared/rtl/yari-core/perfcounters.v

all: rtl/yari.sof

program: rtl/yari.sof
	cd rtl; quartus_pgm yari.cdf

rtl/yari.sof: rtl/config.h rtl/yari.qsf rtl/icache_ram0.mif rtl/toplevel.v $(YARISRC)
	cd rtl; quartus_map yari.qsf $(QFILTER)
	cd rtl; quartus_fit yari.qsf $(QFILTER)
	cd rtl; quartus_asm yari.qsf $(QFILTER)

include ../shared/shared.mk

tinymon.mips-o: tinymon.c
	mips-elf-gcc -D_mips_ -msoft-float -c -Os $< -o $@

tinymon.mips: tinymon.mips-o
	mips-elf-ld -Tprom.ld $< -o $@

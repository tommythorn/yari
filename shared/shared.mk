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

all: rtl/config.h rtl/icache_ram0.mif

rtl/config.h: $(MAKECONFIG) default.conf
	cd rtl; ../$(MAKECONFIG) ../default.conf

# XXX we'll make this MUCH better in future
rtl/icache_ram0.mif: $(TOPDIR)/shared/rtl/initmem.mif
	cp $< $@

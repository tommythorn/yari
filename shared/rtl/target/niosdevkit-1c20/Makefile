# -----------------------------------------------------------------------
#
#   Copyright 2007 Tommy Thorn - All Rights Reserved
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

config.h: Makefile ../../makeconfig.sh
	../../makeconfig.sh \
		$(IC_LINE_INDEX_BITS) $(IC_WORD_INDEX_BITS) \
		$(DC_LINE_INDEX_BITS) $(DC_WORD_INDEX_BITS)

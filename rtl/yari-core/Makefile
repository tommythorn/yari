# -----------------------------------------------------------------------
#
#   Copyright 2008 Tommy Thorn - All Rights Reserved
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
#   Bostom MA 02111-1307, USA; either version 2 of the License, or
#   (at your option) any later version; incorporated herein by reference.
#
# -----------------------------------------------------------------------

PIPELINE=stage_D.v stage_I.v stage_M.v stage_X.v

perfcounters.v: $(PIPELINE) Makefile
	grep perf_ $(PIPELINE) | \
	sed -e 's/^.*perf_/perf_/' -e 's/[^a-z_].*$$//' | \
	sort | uniq | tr "a-z" "A-Z" | \
	(t=0; while read x;do echo '`define' $$x $$t;t=$$((t + 1));done; \
         echo '`define' PERF_COUNTERS $$t) > perfcounters.v

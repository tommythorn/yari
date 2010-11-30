// -----------------------------------------------------------------------
//
//   Copyright 2010 Tommy Thorn - All Rights Reserved
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
//   Bostom MA 02111-1307, USA; either version 2 of the License, or
//   (at your option) any later version; incorporated herein by reference.
//
// -----------------------------------------------------------------------

`timescale 1ns/10ps
`include "../../shared/rtl/soclib/pipeconnect.h"
module toplevel(input              clk50, // 50 MHz
                input              cpu_rst_n,
                output reg [ 7:0]  f_led,

                output      [13:0] ram_a,
                output      [ 1:0] ram_ba,
                output             ram_cas_n,
                output             ram_ck_n,
                output             ram_ck_p,
                output             ram_cke,
                output             ram_cs_n,

                inout      [15:0]  ram_d,

                output             ram_ldm,
                output             ram_ldqs,
                output             ram_ras_n,
                output             ram_udm,
                output             ram_udqs,
                output             ram_ws_n
                );
endmodule

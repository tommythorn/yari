// -----------------------------------------------------------------------
//   
//   Copyright 2004 Tommy Thorn - All Rights Reserved
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
//   Bostom MA 02111-1307, USA; either version 2 of the License, or
//   (at your option) any later version; incorporated herein by reference.
//
// -----------------------------------------------------------------------

/*
 * Simulate a specific subset of the Altera Shift register
 * (lpm_clshift).
 *
 * Not very ambitious, just the bare minimum.
 */

`timescale 1ns/10ps
module logshiftright(distance,
                     data,
                     result);

   parameter             lpm_type = "LPM_CLSHIFT";
   parameter             lpm_width = 32;
   parameter             lpm_widthdist = 5;
   
   input  wire [lpm_widthdist-1:0] distance;
   input  wire [lpm_width-1    :0] data;
   output wire [lpm_width-1    :0] result;

   assign result = data >> distance;
endmodule

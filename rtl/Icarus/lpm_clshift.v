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

module arithshiftbidir(distance,
                       data,
                       direction,
                       result);

   parameter             lpm_type = "LPM_CLSHIFT";
   parameter             lpm_shifttype = "ARITHMETIC";
   parameter             lpm_width = 32;
   parameter             lpm_widthdist = 5;
   
   input  wire [lpm_widthdist-1:0] distance;
   input  wire [lpm_width-1    :0] data;
   input  wire                     direction;
   output wire [lpm_width-1    :0] result;

   assign result = direction ? data >>> distance : data << distance;
endmodule

module test_arithshiftbidir();
   reg  [31:0] data;
   reg  [ 4:0] dist;
   reg         dir;
   wire [31:0] resulta, resultl;
   
   arithshiftbidir a(dist, data, dir, resulta);
   defparam    a.lpm_shifttype = "ARITHMETIC";
   
   initial begin
      #0 data = 48; dir = 0; dist = 0;
      $monitor("dir %d dist %2d A %8x", dir, dist, resulta);
      repeat (2) begin
         repeat (32)
           #1 dist = dist + 1;
         dir = ~dir;
      end      
   end   
endmodule
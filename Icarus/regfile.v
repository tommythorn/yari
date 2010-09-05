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

`timescale 1ns/10ps

module regfile(input  wire        clock,
               input  wire        enable,

               input  wire [ 4:0] rdaddress_a,
               input  wire [ 4:0] rdaddress_b,
               // Write port
               input  wire        wren,
               input  wire [ 4:0] wraddress,
               input  wire [31:0] data,

               // Read ports
               output reg  [31:0] qa, // One clock cycle delayed
               output reg  [31:0] qb  // One clock cycle delayed
               );

   reg [31:0] regs [31:0];

   always @(posedge clock)
     if (enable) begin
        if (wren)
          regs[wraddress] <= data;
        qa <= regs[rdaddress_a];
        qb <= regs[rdaddress_b];
     end
      
   reg [7:0] i;
   initial begin
      i = 0;
      repeat (32) begin
         regs[i] = 0 /*{4{i}}*/;
         i = i + 1;
      end
   end
endmodule

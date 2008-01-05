/* -----------------------------------------------------------------------
 *
 *   Copyright 2004,2007 Tommy Thorn - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * -----------------------------------------------------------------------
 *
 *
 * Block ram controller
 */

`timescale 1ns/10ps
`include "pipeconnect.h"
module blockram(
        input  wire        clock,
        input  wire        rst,
        input  wire `REQ   sram_ctrl_req,
        output wire `RES   sram_ctrl_res);

   parameter size  = 18; // 4 * 2^18 = 1 MiB
   parameter INIT_FILE = "";

   wire [31:0] a  = sram_ctrl_req`A;
   wire [31:0] q;
   wire        sel = a[31:28] == 'h4;

   reg         sel_  = 0; always @(posedge clock) sel_  <= sel;
   reg         read_ = 0; always @(posedge clock) read_ <= sram_ctrl_req`R;
   reg         ready = 0; always @(posedge clock) ready <= sram_ctrl_res`HOLD;

   assign      sram_ctrl_res`HOLD = !ready & (sram_ctrl_req`R | sram_ctrl_req`W);
   assign      sram_ctrl_res`RD   = read_ & sel_ ? q : 0;

   dpram memory(.clock(clock),
                .address_a(a[size+1:2]),
                .byteena_a(sram_ctrl_req`WBE),
                .wrdata_a(sram_ctrl_req`WD),
                .wren_a(sram_ctrl_req`W & sel),
                .rddata_a(q),

                .address_b(0),
                .byteena_b(0),
                .wrdata_b(0),
                .wren_b(0),
                .rddata_b());

   defparam
           memory.DATA_WIDTH = 32,
           memory.ADDR_WIDTH = size,
           memory.INIT_FILE  = INIT_FILE;

`ifdef DEBUG_BLOCKRAM
   reg [31:0] a_  = 0;

   always @(posedge clock) begin
      a_ <= a;

      if (sram_ctrl_req`W & sel)
         $display("%05d  blockram[%x] <- %8x/%x", $time, a[size+1:2], sram_ctrl_req`WD, sram_ctrl_req`WBE);
      if (read_ & sel_)
         $display("%05d  blockram[%x] -> %8x", $time, a_[size+1:2], sram_ctrl_res`RD);
   end
`endif
endmodule

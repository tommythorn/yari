/* -----------------------------------------------------------------------
 *
 *   Copyright 2008 Tommy Thorn - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * -----------------------------------------------------------------------
 */

`timescale 1ns/10ps
module sram_ctrl
   (input  wire        clock
   ,input  wire        rst

   ,output             mem_waitrequest
   ,input        [1:0] mem_id
   ,input       [29:0] mem_address
   ,input              mem_read
   ,input              mem_write
   ,input       [31:0] mem_writedata
   ,input        [3:0] mem_writedatamask
   ,output reg  [31:0] mem_readdata
   ,output reg   [1:0] mem_readdataid = 0

   ,output reg  [29:0] sram_a
   ,inout  wire [31:0] sram_d
   ,output reg         sram_cs_n = 1
   ,output reg   [3:0] sram_be_n
   ,output reg         sram_oe_n = 1
   ,output reg         sram_we_n = 1
   );

   parameter burst_bits = 2;
   parameter need_wait  = 0;

   parameter burst_length = 1 << burst_bits;

   parameter S_IDLE     = 0;
   parameter S_READ     = 1;
   parameter S_READWAIT = 2;
   parameter S_WRITE1   = 3;
   parameter S_WRITE2   = 4;
   reg [3:0] state      = S_IDLE;

   wire      sel        = mem_address[29:26] == 'h4;
   reg [burst_bits:0] cnt = ~0;
   reg        int_we_n     = 1;
   reg [1:0]  pendingid;
   reg [31:0] sram_dout;

   assign    mem_waitrequest   = state != S_IDLE;
   // XXX This coupling between oe# and the tri-state driver is perhaps a bit unsound
   // clearly there will be fighting for a short period
   assign    sram_d            = sram_oe_n ? sram_dout : 'hZ;

   always @(negedge clock)
      sram_we_n <= int_we_n;

   always @(posedge clock) begin
      mem_readdataid <= 0;
      case (state)
      S_IDLE:
         if (mem_read) begin
            pendingid <= mem_id;
            sram_a    <= mem_address;
            sram_cs_n <= 0;
            sram_oe_n <= 0;
            sram_be_n <= 0;
            int_we_n  <= 1;
            cnt       <= burst_length - 1;
            state     <= need_wait ? S_READWAIT : S_READ;
         end else if (mem_write) begin
            sram_a    <= mem_address;
            sram_dout <= mem_writedata;
            sram_be_n <= ~mem_writedatamask;
            sram_cs_n <= 0;
            sram_oe_n <= 1;
            int_we_n  <= 0;
            state     <= S_WRITE1;
         end else begin
            sram_cs_n <= 0;
            sram_oe_n <= 1;
            int_we_n  <= 1;
         end

      S_READWAIT:
         state <= S_READ;

      S_READ:
         if (!cnt[burst_bits]) begin
            cnt            <= cnt - 1;
            sram_a         <= sram_a + 1;
            mem_readdata   <= sram_d;
            mem_readdataid <= pendingid;
            state          <= need_wait ? S_READWAIT : S_READ;
         end else
            // XXX save a cycle and pick up new reads and writes from here. That however requires
            // adjusting waitrequest al
            state          <= S_IDLE;

      S_WRITE1: begin
         if (need_wait)
            state    <= S_WRITE2;
         else begin
            int_we_n <= 1;
            state    <= S_IDLE;
         end
      end

      S_WRITE2: begin
         int_we_n <= 1;
         state    <= S_IDLE;
      end
      endcase
   end

//`define DEBUG_SRAM 1
`ifdef DEBUG_SRAM
   always @(negedge clock)
         $display("%05d  SRAM cs# %x a %x d %x we# %x oe# %x", $time,
                  sram_cs_n, sram_a, sram_d, sram_we_n, sram_oe_n);

   always @(posedge clock) begin
      if (!mem_waitrequest & sel & mem_read)
         $display("%05d  sram[%x] -> ? for %d", $time,
                  {mem_address,2'd0}, mem_id);

      if (!mem_waitrequest & sel & mem_write)
         $display("%05d  sram[%x] <- %8x/%x", $time,
                  {mem_address,2'd0}, mem_writedata, mem_writedatamask);

      if (mem_readdataid)
         $display("%05d  sram[%x] -> %8x for %d", $time,
                  32'h3fff_fffc + (sram_a << 2), mem_readdata, mem_readdataid);

      $display("%05d  SRAM cs# %x a %x d %x we# %x oe# %x", $time,
               sram_cs_n, sram_a, sram_d, sram_we_n, sram_oe_n);
   end
`endif
endmodule

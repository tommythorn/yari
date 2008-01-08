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
module blockram
   (input  wire        clock
   ,input  wire        rst

   ,output             mem_waitrequest
   ,input        [1:0] mem_id
   ,input       [29:0] mem_address
   ,input              mem_read
   ,input              mem_write
   ,input       [31:0] mem_writedata
   ,input        [3:0] mem_writedatamask
   ,output      [31:0] mem_readdata
   ,output reg   [1:0] mem_readdataid = 0
   );

   parameter burst_bits = 2;
   parameter size  = 18; // 4 * 2^18 = 1 MiB
   parameter INIT_FILE = "";

   parameter burst_length = 1 << burst_bits;

   wire        sel             = mem_address[29:26] == 'h4;
   reg [burst_bits:0] cnt      = ~0;
   reg [size-1:0] read_address = 0;

   assign    mem_waitrequest   = !cnt[burst_bits];

   dpram memory(.clock(clock),
                .address_a(mem_waitrequest
                           ? read_address
                           : mem_address[size-1:0]),
                .byteena_a(mem_writedatamask),
                .wrdata_a(mem_writedata),
                .wren_a(!mem_waitrequest & sel & mem_write),
                .rddata_a(mem_readdata),

                .address_b(0),
                .byteena_b(0),
                .wrdata_b(0),
                .wren_b(0),
                .rddata_b());
   defparam
           memory.DATA_WIDTH = 32,
           memory.ADDR_WIDTH = size,
           memory.INIT_FILE  = INIT_FILE;

   always @(posedge clock)
      if (mem_waitrequest) begin
         cnt <= cnt - 1;
         read_address <= read_address + 1;
      end else begin
         mem_readdataid <= 0;
         if (sel & mem_read) begin
            read_address <= mem_address[size-1:0] + 1;
            mem_readdataid <= mem_id;
            cnt <= burst_length - 2;
         end
      end

`define DEBUG_BLOCKRAM 1
`ifdef DEBUG_BLOCKRAM
   always @(posedge clock) begin
      if (!mem_waitrequest & sel & mem_read)
         $display("%05d  blockram[%x] -> ? for %d", $time,
                  {mem_address,2'd0}, mem_id);

      if (!mem_waitrequest & sel & mem_write)
         $display("%05d  blockram[%x] <- %8x/%x", $time,
                  {mem_address,2'd0}, mem_writedata, mem_writedatamask);

      if (mem_readdataid)
         $display("%05d  blockram[%x] -> %8x for %d", $time,
                  32'h3fff_fffc + (read_address << 2), mem_readdata, mem_readdataid);
   end
`endif
endmodule

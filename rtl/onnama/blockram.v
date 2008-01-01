/* -----------------------------------------------------------------------
 *
 *   Copyright 2004 Tommy Thorn - All Rights Reserved
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
        input  wire        clk,
        input  wire        rst,

        // One read/write port (Latency 1, if !busy_writing)
        input  wire `REQ   sram_ctrl_req,
        output wire `RES   sram_ctrl_res);

   parameter debug = 1;
   parameter size  = 10;  // 1024B = 32768B 1/9.325 of vga
   parameter delaybits = 2;

   /* The memory */
   reg [31:0] memory[0 : (1 << size) - 1];

   // Grrr
   wire [31:0] a  = sram_ctrl_req`A;
   wire [31:0] wd = sram_ctrl_req`WD;
   wire        sel       = a[size+5:size+1] == 0;

   reg [size - 1:0]    addr  = 0;
   reg                 read  = 0;
   reg [delaybits-1:0] delay = 0;

   wire                ready = delay == 0;

   assign      sram_ctrl_res`RD   = read ? memory[addr] : 'h0;
   assign      sram_ctrl_res`HOLD = sel & (sram_ctrl_req`R | sram_ctrl_req`W) & ~ready;

   pipechecker check("blockram", clk, sram_ctrl_req, sram_ctrl_res);

   always @(posedge clk)
     if (rst) begin
        delay <= ~0;

        // XXX SIMULATION ONLY
`ifdef SIMULATE_MAIN
        memory[0] <= 'h87654321;
        memory[1] <= 'h12345678;
        memory[2] <= 'hdeadbeef;
        memory[3] <= 'h1729AAAA;
        memory[4] <= 'h00000001;
`endif
     end else begin
        // $display("%5d A %x R %d W %d", $time, sram_ctrl_req`A, sram_ctrl_req`R, sram_ctrl_req`W);
        addr <= a[size + 1 : 2];
        read <= sel & sram_ctrl_req`R & ready;
        delay <= (sel & (sram_ctrl_req`R | sram_ctrl_req`W)) ? (delay - 1) : ~0;
        delay <= 0; // for an undelayed version

        if (sram_ctrl_res`WAIT)
          $display("%5d BLOCKRAM delaying %d", $time, delay);

        if (sel & sram_ctrl_req`W & ~sram_ctrl_res`HOLD) begin
           memory[a[size + 1 : 2]] = wd;
           $display("%5d BLOCKRAM write %x -> [%x]", $time, wd, addr);
        end

        if (sel & read & ~sram_ctrl_res`HOLD) begin
           $display("%5d BLOCKRAM read [%x] -> %x", $time, addr, sram_ctrl_res`RD);
        end
     end
endmodule

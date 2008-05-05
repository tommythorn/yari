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
module ssram_ctrl
   (input  wire        clock
   ,input  wire        reset

   ,output             mem_waitrequest
   ,input        [1:0] mem_id
   ,input       [29:0] mem_address
   ,input              mem_read
   ,input              mem_write
   ,input       [31:0] mem_writedata
   ,input        [3:0] mem_writedatamask
   ,output reg  [31:0] mem_readdata
   ,output reg   [1:0] mem_readdataid = 0

   ,output wire        sram_clk
   ,output reg         sram_adsc_n = 1 // controller address status
   ,output reg         sram_adv_n  = 1 // burst address advance
   ,output reg  [18:0] sram_a      = 0
   ,output reg  [ 3:0] sram_be_n   = 1 // byte write enable
   ,output reg         sram_oe_n   = 1 // output enable
   ,output reg         sram_we_n   = 1 // write enable
   ,inout  wire [ 3:0] sram_dpa        // "parity" bits, just extra bits
   ,inout  wire [31:0] sram_dq

   ,output wire        sram_adsp_n     // processor address status
   ,output wire        sram_ce1_n      // chip enable
   ,output wire        sram_ce2        // chip enable
   ,output wire        sram_ce3_n      // chip enable
   ,output wire        sram_gw_n       // global write, overrides WE# and BE#
   );

   parameter  burst_bits        = 2;
   parameter  need_wait         = 0;
   parameter  burst_length      = 1 << burst_bits;

   /* Fixed SSRAM outputs */

   assign     sram_clk          = clock; // XXX, should be phase shifted (tested and it works but requires a rework of the logic that I don't have time for)
   assign     sram_adsp_n       = 1; // we use adsc#
   assign     sram_gw_n         = 1; // don't use global write
   assign     sram_ce1_n        = 0; // chip enable
   assign     sram_ce2          = 1; // chip enable
   assign     sram_ce3_n        = 0; // chip enable

   reg [31:0] sram_dout;
   reg        sram_dout_en      = 0;
// assign     sram_dpa          = sram_dout_en ? sram_dpa_out : 'hZ;
   assign     sram_dq           = sram_dout_en ? sram_dout    : 'hZ;


   parameter  S_IDLE            = 0;
   parameter  S_READWAIT        = 1;
   parameter  S_READ_BURST      = 4;
   parameter  S_WRITE1          = 2;
   parameter  S_WRITE2          = 3;
   reg [ 5:0] state             = S_IDLE;

   reg [burst_bits:0] cnt       = ~0;
   reg [ 1:0]  pendingid        = 0;
   reg [ 1:0]  pendingid2       = 0;
   reg [ 1:0]  readdataid       = 0;

   assign      mem_waitrequest  = state != S_IDLE ||
                                  mem_write && pendingid2;

   /*
    * NOTE: It is important that sram_dout_en be (redundantly)
    * assigned in every state to insure that the enable is packed into
    * the IO buffer. Credit to Martin Schoeberl for finding this
    * obscure fact.
    */

   /*
    Assuming ADSP_N = 1, CE_N = 0, etc and OE_N = 0 for reads

        ADSC_N  ADV_N   WRITE_N
        0       X       0       begin burst WRITE
        0       X       1       begin burst READ

        1       0       0       continue burst WRITE
        1       0       1       continue burst READ

    6. For a WRITE operation following a READ operation, OE must be
       HIGH before the input data setup time and held HIGH during the
       input data hold time.

    XXX For proper operation, we need a PLL to phase shift the
    external SRAM clock, but for 50 MHz we might be able to get away
    with a less ideal solution.
    */


   /* Read data follows two cycles behind the read command */
   always @(posedge clock) if (reset) begin
      mem_readdataid <= 0;
   end else begin
      mem_readdata   <= sram_dq; //readdataid ? sram_dq : ~0; // Really mostly for debugging
      mem_readdataid <= readdataid;
   end

   always @(posedge clock) begin
      // Set up default values so that writes below are one-shot
      sram_adsc_n <= 1;
      sram_be_n   <= ~0;
      sram_we_n   <= 1;
      sram_adv_n  <= 1;

      if (reset) begin
        readdataid  <= 0;
        pendingid2  <= 0;
      end else begin
        readdataid  <= pendingid2;
        pendingid2  <= pendingid;

        case (state)
      S_IDLE:
         if (mem_read) begin
            sram_a         <= mem_address;
            sram_adsc_n    <= 0;
            sram_dout_en   <= 0;
            sram_oe_n      <= 0;

            pendingid      <= mem_id;
            cnt            <= burst_length - 3; // 1
            state          <= S_READ_BURST;
         end else if (mem_write && !pendingid2) begin
            // sram_oe_n == 1
            sram_a         <= mem_address;
            sram_adsc_n    <= 0;
            sram_dout_en   <= 1;
            sram_dout      <= mem_writedata;
            sram_be_n      <= ~mem_writedatamask;
            sram_we_n      <= 0;
            sram_oe_n     <= 1  /*pendingid2 == 0*/;

            pendingid      <= 0;
         end else begin
            pendingid     <= 0;
            sram_dout_en  <= 0;
         end

      S_READ_BURST: begin
         // Burst reading: assert ADV# for three cycles
         // cnt = 1, 0, -1
         sram_adv_n     <= 0;
         sram_dout_en   <= 0;
         cnt            <= cnt - 1;
         if (cnt[burst_bits])
            state <= S_IDLE;
      end

      S_READWAIT: begin
         state             <= S_IDLE;
         sram_dout_en     <= 0;
      end

      S_WRITE1: begin
         sram_dout_en     <= 1;
         sram_we_n        <= 1;
         state             <= S_IDLE;
      end

      S_WRITE2: begin
         sram_dout_en     <= 1;
         sram_we_n        <= 1;
         state             <= S_IDLE;
      end
      endcase
   end
   end
endmodule

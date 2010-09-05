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
   ,input       [29:0] mem_address     // *word* address
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

   ,inout       [14:0] FLASH_DQ        // FLASH Data bus 15 Bits (0 to 14)
   ,inout              FLASH_DQ15_AM1  // FLASH Data bus Bit 15 or Address A-1
   ,output      [21:0] oFLASH_A        // FLASH Address bus 22 Bits
   ,output             oFLASH_WE_N     // FLASH Write Enable
   ,output             oFLASH_RST_N    // FLASH Reset
   ,output             oFLASH_WP_N     // FLASH Write Protect /Programming Acceleration
   ,input              iFLASH_RY_N     // FLASH Ready/Busy output
   ,output             oFLASH_BYTE_N   // FLASH Byte/Word Mode Configuration
   ,output             oFLASH_OE_N     // FLASH Output Enable
   ,output             oFLASH_CE_N     // FLASH Chip Enable
   );

   parameter  burst_bits        = 2;
   parameter  need_wait         = 0;
   parameter  burst_length      = 3'd1 << burst_bits;

   /* Flash timing parameters */
   parameter  tACC              = 7'd90;  // ns
   parameter  tCYCLE            = 7'd20;  // ns XXX pass in from outside!!

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


   /* DE2-70 Flash RAM, 8 MiB = #80_0000
    * Accessible at #B000_0000 - #B07F_FFFF
    * Mirrored at   #B080_0000 - #B07F_FFFF
    *               ...
    *               #BF80_0000 - #BF7F_FFFF
    * Tinymon starts at #BFC0_0000, thus offset by
    * #40_0000 bytes, that is, #20_0000 wydes.
    */
   reg  [ 1:0] flash_id         = 2'd0;
   reg  [21:0] flash_address    = 22'd0;
   reg [31:0]  flash_data       = 0;
   wire [15:0] flash_data_in;
   wire        flash_data_out   = 1'd0;
   reg         flash_data_out_en= 1'd0;

   parameter   FLASH_DELAY_MSB  = 7;  // [-128; 127] more than enough
   reg  [FLASH_DELAY_MSB:0]
               flash_delay      = 1'd0;

   assign      {FLASH_DQ15_AM1,FLASH_DQ}
                                = flash_data_out_en ? flash_data_out : 16'hZZ;
   assign      flash_data_in    = {FLASH_DQ15_AM1,FLASH_DQ};
   assign      oFLASH_A         = flash_address;
   assign      oFLASH_CE_N      = 0;
   assign      oFLASH_OE_N      = 0;
   assign      oFLASH_WE_N      = 1;
   assign      oFLASH_RST_N     = 1;
   assign      oFLASH_WP_N      = 0; // doesn't matter
   assign      oFLASH_BYTE_N    = 1;

   parameter  S_IDLE            = 0;
   parameter  S_READWAIT        = 1;
   parameter  S_READ_BURST      = 2;
   parameter  S_WRITE1          = 3;
   parameter  S_WRITE2          = 4;
   parameter  S_READ_FLASH      = 5;
   reg [ 5:0] state             = S_IDLE;

   reg [burst_bits:0] cnt       = 0;
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
   always @(posedge clock) begin
      if (reset) begin
         mem_readdataid    <= 0;
         readdataid        <= 0;
         pendingid2        <= 0;
         state             <= S_IDLE;
      end else begin
         mem_readdata      <= sram_dq;
         mem_readdataid    <= readdataid;
         readdataid        <= pendingid2;
         pendingid2        <= pendingid;
      end

      /* Set up default values so that writes below are one-shot */
      sram_adsc_n          <= 1;
      sram_be_n            <= 15;
      sram_we_n            <= 1;
      sram_adv_n           <= 1;

      case (state)
      S_IDLE:
         if (mem_read && mem_address[29:26] == 4'hB) begin
            /* Reading from Flash */
            // byte address 16-byte aligned == {a[:4],4'd0}
            // word address 16-byte aligned == {a[:2],4'd0}
            // as wyde address == {a[:2],3'd0}
            flash_address  <= {mem_address[20:2],3'd0};
            flash_delay    <= (tACC + tCYCLE - 1'd1) / tCYCLE - 1'd1;
            flash_id       <= mem_id;
            pendingid      <= 0; // Too be safe
            state          <= S_READ_FLASH;
         end else if (mem_read) begin
            sram_a         <= mem_address[18:0];
            sram_adsc_n    <= 0;
            sram_dout_en   <= 0;
            sram_oe_n      <= 0;

            pendingid      <= mem_id;
            cnt            <= burst_length - 2'd3; // 1
            state          <= S_READ_BURST;
         end else if (mem_write && !pendingid2) begin
            // sram_oe_n == 1
            sram_a         <= mem_address[18:0];
            sram_adsc_n    <= 0;
            sram_dout_en   <= 1;
            sram_dout      <= mem_writedata;
            sram_be_n      <= ~mem_writedatamask;
            sram_we_n      <= 0;
            sram_oe_n      <= 1  /*pendingid2 == 0*/;

            pendingid      <= 0;
         end else begin
            pendingid      <= 0;
            sram_dout_en   <= 0;
         end

      S_READ_BURST: begin
         // Burst reading: assert ADV# for three cycles
         // cnt = 1, 0, -1
         sram_adv_n        <= 0;
         sram_dout_en      <= 0;
         cnt               <= cnt - 1'd1;
         if (cnt[burst_bits])
            state          <= S_IDLE;
      end

      S_READWAIT: begin
         state             <= S_IDLE;
         sram_dout_en      <= 0;
      end

      S_WRITE1: begin
         sram_dout_en      <= 1;
         sram_we_n         <= 1;
         state             <= S_IDLE;
      end

      S_WRITE2: begin
         sram_dout_en      <= 1;
         sram_we_n         <= 1;
         state             <= S_IDLE;
      end

      S_READ_FLASH: begin
         if (!flash_delay[FLASH_DELAY_MSB])
            flash_delay    <= flash_delay - 1'd1;
         else begin
            /* Readout the data. Notice that Terasic's control panel
               stores the wydes in little endian, but YARI is
               currently bigendian, thus a 76543210 word would be
               stored in two wydes like 5476 1032.

               We accumulate the read data in flash_data and copy it to mem_readdata.
             */
            flash_data     <= {flash_data[15:0], flash_data_in[7:0], flash_data_in[15:8]};
            mem_readdata   <= {flash_data[15:0], flash_data_in[7:0], flash_data_in[15:8]};
            flash_address  <= flash_address + 1'd1;

            /* Every other read is a full word */
            mem_readdataid <= flash_address[0] ? flash_id : 2'd0;

            /* Every 4th read takes a while (XXX actually faster reads not implemented yet) */
            flash_delay    <= (tACC + tCYCLE - 1'd1) / tCYCLE - 1'd1;

            if (flash_address[2:0] == 7)
               state       <= S_IDLE;
         end
       end
      endcase
   end
endmodule

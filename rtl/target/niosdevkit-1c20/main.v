// -----------------------------------------------------------------------
//
//   Copyright 2004,2007,2008 Tommy Thorn - All Rights Reserved
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
//   Bostom MA 02111-1307, USA; either version 2 of the License, or
//   (at your option) any later version; incorporated herein by reference.
//
// -----------------------------------------------------------------------
//
// Main module


/*
  4000_0000 - 400F_FFFF Extern SRAM (1 MiB)
  BFC0_0000 - BFC0_3FFF Boot ROM (16 KiB) (Preloaded I$ cache)
  FF00_0000 - FF00_1FFF Peripherals
                        Read             Write
                      0 rs232out busy    rs232out data
                      1 rs232in  data
                      2 rs232in  count
                      3 TSC
 */

`timescale 1ns/10ps
`include "../../soclib/pipeconnect.h"

`ifdef SIMULATE_MAIN
`define BLOCKRAM 1
`endif

module main(// Clock and reset
             input  wire        clkin        // K5  PLL input clock (50 MHz)
            ,output wire        pld_clkout   // L8  Clock to zero-skew buffer Lancelot board
            ,input  wire        pld_clkfb    // L14 Feedback from pld_clkout to PLL2
            ,input  wire        reset_n      // C4  CPU Reset button

            // Lancelot VGA interface
            ,output wire  [7:0] vga_r        // VGA red
            ,output wire  [7:0] vga_g        // VGA green
            ,output wire  [7:0] vga_b        // VGA blue
            ,output wire        vga_hs       // VGA horz sync
            ,output wire        vga_vs       // VGA vert sync
            ,output wire        vga_blank_n  // VGA DAC force blank
            ,output wire        vga_sync_n   // VGA sync enable
            ,output wire        vga_sync_t   // VGA sync on R/G/B
            ,output wire        vga_m1       // VGA color space config
            ,output wire        vga_m2       // VGA color space config

`ifdef PS2_ENABLED
            // PS/2 keyboard/mouse
            ,output             ps2_sel      // PS/2 port input/output select
            ,inout              ps2_kclk     // PS/2 keyboard clock
            ,inout              ps2_kdata    // PS/2 keyboard data
            ,inout              ps2_mclk     // PS/2 mouse clock
            ,inout              ps2_mdata    // PS/2 mouse data
`endif

            // Push buttons LEDs 7-segments
            ,input  wire  [3:0] sw           // Pushbutton switches
            ,output       [7:0] led          // Debugging LEDs
            ,output wire  [7:0] s7_0         // Debugging 7-segment LEDs
            ,output wire  [7:0] s7_1         // --

`ifndef SIMULATE_MAIN
            // Flash-SRAM-Ethernet bus
            ,output wire [22:0] fse_a        // Mainboard common bus address
            ,inout  wire [31:0] fse_d        // Mainboard common bus data
            ,output wire        flash_cs_n   // Flash ROM CS#
            ,output wire        enet_aen     // Ethernet Access Enable
            ,output wire        sram_cs_n    // SRAM CS#
            ,output wire  [3:0] sram_be_n    // SRAM byte enables
            ,output wire        sram_oe_n    // SRAM OE#
            ,output wire        sram_we_n    // SRAM WE#
`endif

`ifdef SERIAL_ENABLED
            // Serial IO
            ,output             ttya_dcd     // Console DCD#
            ,output             ttya_txd     // Console TxD
            ,input              ttya_rxd     // Console RxD
            ,input              ttya_dtr     // Console DTR#
            ,output             ttya_dsr     // Console DSR#
            ,input              ttya_rts     // Console RTS#
            ,output             ttya_cts     // Console CTS#
            ,output             ttya_ri      // Console RI#
`endif
            ,output             ttyb_txd     // Debug TxD
            ,input              ttyb_rxd     // Debug RxD

`ifdef CF_ENABLED
            // CompactFlash slot
            ,output [10:0]      cf_a         // CompactFlash address bus
            ,inout [15:0]       cf_d         // CompactFlash data bus
            ,input              cf_rdy       // CompactFlash RDY
            ,input              cf_wait_n    // CompactFlash WAIT#
            ,output             cf_ce1_n     // CompactFlash CE1#
            ,output             cf_ce2_n     // CompactFlash CE2#
            ,output             cf_oe_n      // CompactFlash OE#
            ,output             cf_we_n      // CompactFlash WE#
            ,output             cf_reg_n     // CompactFlash REG#
            ,input              cf_cd1_n     // CompactFlash card detect
`endif
            // SDRAM
            // Audio
            // Ethernet
            // Flash memory
            );
   // ------------------------------------------------------------------------
   //  Reset
   // (there is option for extending the reset internally here, if necessary)
   // ------------------------------------------------------------------------
//    wire          rst = ~reset_n;

   // ------------------------------------------------------------------------
   //  PLLs and clock distribution
   // ------------------------------------------------------------------------

   // XXX Note 45 MHz appears to be the limit for the sram with just 1
   // wait state (I happen to know the memory is good for much more
   // with a trick I can't explain why works.)
   parameter FREQ = 45_454_545; // match clock frequency
   parameter BPS  =    115_200; // Serial speed

   wire [ 7:0]   rs232out_d;
   wire          rs232out_w;
   wire          rs232out_busy;

   wire [ 7:0]   rs232in_data;
   wire          rs232in_attention;

   wire          mem_waitrequest;
   wire    [1:0] mem_id;
   wire   [29:0] mem_address;
   wire          mem_read;
   wire          mem_write;
   wire   [31:0] mem_writedata;
   wire    [3:0] mem_writedatamask;
   wire   [31:0] mem_readdata;
   wire    [1:0] mem_readdataid;

   wire `REQ     rs232_req;
   wire `RES     rs232_res;

`ifdef SIMULATE_MAIN
   // Flash-SRAM-Ethernet bus
   wire [22:0]   fse_a;        // Mainboard common bus address
   wire [31:0]   fse_d;        // Mainboard common bus data
   wire          flash_cs_n;   // Flash ROM CS#
   wire          enet_aen;     // Ethernet Access Enable
   wire          sram_cs_n;    // SRAM CS#
   wire [3:0]    sram_be_n;    // SRAM byte enables
   wire          sram_oe_n;    // SRAM OE#
   wire          sram_we_n;    // SRAM WE#

`ifndef BLOCKRAM
   idt71v416s10 u35(fse_d[15: 0], fse_a[19:2], sram_we_n, sram_oe_n, sram_cs_n,
                    sram_be_n[0], sram_be_n[1]); // Yep, strange order...
   idt71v416s10 u36(fse_d[31:16], fse_a[19:2], sram_we_n, sram_oe_n, sram_cs_n,
                    sram_be_n[2], sram_be_n[3]);
`endif
`endif

   assign        flash_cs_n = 1;  // Disable flash ROM
   assign        enet_aen   = 1;  // Disable Ethernet

`ifdef SIMULATE_MAIN
   reg           clk = 0;
`else
   wire          clk;
   wire          pll_locked;

   pll pll(
        .inclk0(clkin),         // 50 MHz input clock
        .c0(clk),
        .locked(pll_locked)
        );

   assign  led = {rled[0],rled[1],rled[2],rled[3],rled[4],rled[5],rled[6],rled[7]};
   reg  [7:0] rled = 0;
`endif // SIMULATE_MAIN

   reg  [ 3:0] reset_count_up = 0;
`ifdef SIMULATE_MAIN
   wire        rst = 0;
`else
   wire        rst = ~reset_n /*reset_count_up != 15*/;
`endif

   always @(posedge clk)
      if (~reset_n)
        reset_count_up <= 0;
      else if (rst) begin
         if (reset_count_up == 14)
           $display("Reset count %d", reset_count_up);
         reset_count_up <= reset_count_up + 1'd1;
      end

   yari yari_inst(
         .clock(clk)
        ,.rst(rst)

        ,.mem_waitrequest(mem_waitrequest)
        ,.mem_id(mem_id)
        ,.mem_address(mem_address)
        ,.mem_read(mem_read)
        ,.mem_write(mem_write)
        ,.mem_writedata(mem_writedata)
        ,.mem_writedatamask(mem_writedatamask)
        ,.mem_readdata(mem_readdata)
        ,.mem_readdataid(mem_readdataid)

        ,.peripherals_req(rs232_req)
        ,.peripherals_res(rs232_res)
        );

`ifdef SIMULATE_MAIN
   blockram blockram_inst
      (.clock(clk)
      ,.rst(rst)
      ,.mem_waitrequest(mem_waitrequest)
      ,.mem_id(mem_id)
      ,.mem_address(mem_address)
      ,.mem_read(mem_read)
      ,.mem_write(mem_write)
      ,.mem_writedata(mem_writedata)
      ,.mem_writedatamask(mem_writedatamask)
      ,.mem_readdata(mem_readdata)
      ,.mem_readdataid(mem_readdataid)
      );

   defparam blockram_inst.INIT_FILE="`SRAM_INIT";
`else
   sram_ctrl sram_ctrl_inst
      (.clock(clk)
      ,.rst(rst)
      ,.mem_waitrequest(mem_waitrequest)
      ,.mem_id(mem_id)
      ,.mem_address(mem_address)
      ,.mem_read(mem_read)
      ,.mem_write(mem_write)
      ,.mem_writedata(mem_writedata)
      ,.mem_writedatamask(mem_writedatamask)
      ,.mem_readdata(mem_readdata)
      ,.mem_readdataid(mem_readdataid)

      ,.sram_a(fse_a[19:2])
      ,.sram_d(fse_d)
      ,.sram_cs_n(sram_cs_n)
      ,.sram_be_n(sram_be_n)
      ,.sram_oe_n(sram_oe_n)
      ,.sram_we_n(sram_we_n)
      );
   defparam sram_ctrl_inst.need_wait = 1;
`endif

   rs232out rs232out_inst
      (.clock(clk),
       .serial_out(ttyb_txd),
       .transmit_data(rs232out_d),
       .we(rs232out_w),
       .busy(rs232out_busy));

   defparam rs232out_inst.frequency = FREQ,
            rs232out_inst.bps       = BPS;


   rs232in rs232in_inst
      (.clock(clk),
       .serial_in(ttyb_rxd),
       .received_data(rs232in_data),
       .attention(rs232in_attention));

   defparam rs232in_inst.frequency = FREQ,
            rs232in_inst.bps       = BPS;


   rs232 rs232_inst(.clk(clk),
               .rst(rst),

               .rs232_req(rs232_req),
               .rs232_res(rs232_res),

               .rs232in_attention(rs232in_attention),
               .rs232in_data(rs232in_data),

               .rs232out_busy(rs232out_busy),
               .rs232out_w(rs232out_w),
               .rs232out_d(rs232out_d));

`ifdef SIMULATE_MAIN
   always #50 clk = ~clk;
   initial #50 clk = 0;
`endif
endmodule

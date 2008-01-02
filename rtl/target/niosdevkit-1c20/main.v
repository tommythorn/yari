// -----------------------------------------------------------------------
//
//   Copyright 2004,2007 Tommy Thorn - All Rights Reserved
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
             input  wire        clkin        // K5  PLL1 input clock (50 MHz)
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

   wire          clk100MHz;     //  100 MHz
   wire          clk25MHz;
   wire          video_clk;     //   25 MHz = screen pixel rate

   wire [ 7:0]   rs232out_d;
   wire          rs232out_w;
   wire          rs232out_busy;

   wire [ 7:0]   rs232in_data;
   wire          rs232in_attention;

   wire `REQ     vga_req, dmem_req, imem_req, master2_req, dc_ctrl_req,
                 sram_req, peripheral_req, rs232_req;
   wire `RES     vga_res, dmem_res, imem_res, master2_res, dc_ctrl_res,
                 sram_res, peripheral_res, rs232_res;

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
   wire          pll1_locked;

   pll1 pll1(
        .inclk0(clkin),         // 50 MHz input clock
        .c0(clk100MHz),         // x2/1 = 100 MHz output clock
        .c1(clk25MHz),          // x1/2 =  25 MHz output clock
        .locked(pll1_locked),
        .e0(pld_clkout)         // External only output x1/2 = 25 MHz
        );

   assign  led = {rled[0],rled[1],rled[2],rled[3],rled[4],rled[5],rled[6],rled[7]};
   reg  [7:0] rled = 0;


   reg [25:0] divider = 0;
   wire       clk = clk25MHz;


   always @(posedge clk25MHz) begin
     divider <= divider - 1'd1;
   end

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
        ,.imem_req(imem_req)
        ,.imem_res(imem_res)
        ,.dmem_req(dmem_req)
        ,.dmem_res(dmem_res)
        ,.peripherals_req(rs232_req)
        ,.peripherals_res(rs232_res)
        );

   vga vga(clk, rst,
           // Lancelot VGA interface
           vga_r, vga_g, vga_b, vga_m1, vga_m2,
           vga_sync_n, vga_sync_t,
           vga_blank_n, vga_hs, vga_vs,

           'h400E_6A00, // FB start addr (may move)

           // Video memory port
           vga_req, vga_res);


   bus_ctrl bus_ctrl(.clk(clk),
                     .rst(rst),

                     // Master ports
                     .master1_req(vga_req),  // VGA highest priority!
                     .master1_res(vga_res),

                     .master2_req(imem_req),
                     .master2_res(imem_res),

                     .master3_req(dmem_req),
                     .master3_res(dmem_res),

                     // Target ports
                     .target1_req(sram_req),
                     .target1_res(sram_res),

                     .target2_req(peripheral_req),
                     .target2_res(0)
                     );

`ifdef SIMULATE_MAIN
   blockram blockram_inst(.clock(clk),
                          .rst(rst),

                          .sram_ctrl_req(sram_req),
                          .sram_ctrl_res(sram_res));

   defparam blockram_inst.INIT_FILE="`SRAM_INIT";
`else
   sram_ctrl sram_ctrl(.clk(clk),

                       .sram_req(sram_req),
                       .sram_res(sram_res),

                       .sram_a(fse_a),
                       .sram_d(fse_d),
                       .sram_cs_n(sram_cs_n),
                       .sram_be_n(sram_be_n),
                       .sram_oe_n(sram_oe_n),
                       .sram_we_n(sram_we_n));
`endif


   rs232out #(115200,25000000)
      rs232out_inst(
               .clk25MHz(clk),
               .reset(rst),

               .serial_out(ttyb_txd),
               .transmit_data(rs232out_d),
               .we(rs232out_w),
               .busy(rs232out_busy));

   rs232in #(115200,25000000)
      rs232in_inst(.clk25MHz(clk),
                   .reset(rst),

                   .serial_in(ttyb_rxd),
                   .received_data(rs232in_data),
                   .attention(rs232in_attention));

   rs232 rs232(.clk(clk),
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

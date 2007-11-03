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
module main(

	input wire clk,
//
//	serial interface
//
	output wire ser_txd,
	input wire ser_rxd,
	input wire ser_ncts,
	output wire ser_nrts,

//
//	watchdog
//
	output wire wd,

//
//	two ram banks
//
	output wire [17:0] rama_a,
	inout wire [15:0] rama_d,
	output wire rama_ncs,
	output wire rama_noe,
	output wire rama_nlb,
	output wire rama_nub,
	output wire rama_nwe,
	output wire [17:0] ramb_a,
	inout wire [15:0] ramb_d,
	output wire ramb_ncs,
	output wire ramb_noe,
	output wire ramb_nlb,
	output wire ramb_nub,
	output wire ramb_nwe);

   wire clock = clk;

   assign wd = rst_counter[22];

   reg [26:0] rst_counter = 0;
   always @(posedge clock)
//      if (~USER_PB[0])
//         rst_counter <= 'd48_000_000;
//      else if (~rst_counter[26])
      if (~rst_counter[26])
         rst_counter <= rst_counter - 1;

   wire rst = ~rst_counter[26];

   assign       ramb_a = rama_a;
   assign       ramb_ncs = rama_ncs;
   assign       ramb_noe = rama_noe;
   assign       ramb_nwe = rama_nwe;




   parameter FREQ = 20_000_000; // 20 MHz, match clock frequency
   parameter BPS  =    115_200; // Serial speed

   wire [ 7:0]   rs232out_d;
   wire          rs232out_w;
   wire          rs232out_busy;

   wire [ 7:0]   rs232in_data;
   wire          rs232in_attention;

   wire [17:0]   port1_addr;
   wire          port1_rd_strobe;
   wire [31:0]   port1_rd_data;
   wire          port1_wait;

   wire `REQ     vga_req, dmem_req, imem_req, master2_req, dc_ctrl_req,
                 sram_req, peripheral_req, rs232_req;
   wire `RES     vga_res, dmem_res, imem_res, master2_res, dc_ctrl_res,
                 sram_res, peripheral_res, rs232_res;

   wire [31:0]   debug_info;
   wire [ 7:0]   debug_byte;

//   assign        USER_LED = debug_byte[3:0];

   yari yari_inst(
         .clock(clock)
        ,.rst(rst)
        ,.imem_req(imem_req)
        ,.imem_res(imem_res)
        ,.dmem_req(dmem_req)
        ,.dmem_res(dmem_res)
        ,.debug_info(debug_info)
        ,.debug_byte(debug_byte)
        );

   bus_ctrl bus_ctrl(.clk(clock),
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
                     .target2_res(peripheral_res)
                     );

   peri_ctrl peri_ctrl(.clk(clock),
                       .rst(rst),

                       .peripheral_req(peripheral_req),
                       .peripheral_res(peripheral_res),

                       .rs232_req(rs232_req),
                       .rs232_res(rs232_res));

   wire [1:0]    dummy; // Need to generalize sram_ctr a bit

   sram_ctrl sram_ctrl(.clk(clock),

                       .sram_req(sram_req),
                       .sram_res(sram_res),

                       .sram_a({rama_a,dummy}),
                       .sram_d({rama_d,ramb_d}),
                       .sram_cs_n(rama_ncs),
                       .sram_be_n({rama_nub,rama_nlb,ramb_nub,ramb_nlb}),
                       .sram_oe_n(rama_noe),
                       .sram_we_n(rama_nwe));

   rs232out #(BPS,FREQ)
      rs232out_inst(.clk25MHz(clock),
               .reset(0),

               .serial_out(ser_txd),
               .transmit_data(rs232out_d),
               .we(rs232out_w),
               .busy(rs232out_busy));

   rs232in #(BPS,FREQ)
      rs232in_inst(.clk25MHz(clock),
                   .reset(0),

                   .serial_in(ser_rxd),
                   .received_data(rs232in_data),
                   .attention(rs232in_attention));

   rs232 rs232_inst(.clk(clock),
               .rst(rst),

               .rs232_req(rs232_req),
               .rs232_res(rs232_res),

               .rs232in_attention(rs232in_attention),
               .rs232in_data(rs232in_data),

               .rs232out_busy(rs232out_busy),
               .rs232out_w(rs232out_w),
               .rs232out_d(rs232out_d));
endmodule

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

   wire clock;

   pll pll(.inclk0(clk),    // 20 MHz on Cycore
           .c0(clock));     // xx MHz output

   assign wd = rst_counter[22];

   reg [26:0] rst_counter = 0;
   always @(posedge clock)
//      if (~USER_PB[0])
//         rst_counter <= 'd48_000_000;
//      else if (~rst_counter[26])
      if (~rst_counter[26])
         rst_counter <= rst_counter - 1;

   wire         rst      = ~rst_counter[26];

   assign       ramb_a   = rama_a;
   assign       ramb_ncs = rama_ncs;
   assign       ramb_noe = rama_noe;
   assign       ramb_nwe = rama_nwe;




   parameter FREQ = 80_000_000; // match clock frequency
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

   yari yari_inst(
         .clock(clock)
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

   sram_ctrl sram_ctrl
      (.clock(clock)
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

      ,.sram_a(rama_a)
      ,.sram_d({rama_d,ramb_d})
      ,.sram_cs_n(rama_ncs)
      ,.sram_be_n({rama_nub,rama_nlb,ramb_nub,ramb_nlb})
      ,.sram_oe_n(rama_noe)
      ,.sram_we_n(rama_nwe)
      );

   defparam sram_ctrl.need_wait = 1;

   rs232out rs232out_inst
      (.clock(clock),
       .serial_out(ser_txd),
       .transmit_data(rs232out_d),
       .we(rs232out_w),
       .busy(rs232out_busy));

   defparam rs232out_inst.frequency = FREQ,
            rs232out_inst.bps       = BPS;


   rs232in rs232in_inst
      (.clock(clock),
       .serial_in(ser_rxd),
       .received_data(rs232in_data),
       .attention(rs232in_attention));

   defparam rs232in_inst.frequency = FREQ,
            rs232in_inst.bps       = BPS;


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

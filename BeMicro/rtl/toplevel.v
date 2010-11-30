// -----------------------------------------------------------------------
//
//   Copyright 2010 Tommy Thorn - All Rights Reserved
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
//   Bostom MA 02111-1307, USA; either version 2 of the License, or
//   (at your option) any later version; incorporated herein by reference.
//
// -----------------------------------------------------------------------

`timescale 1ns/10ps
`include "../../shared/rtl/soclib/pipeconnect.h"
module toplevel(input              clk, // 16 MHz
                input              exp_rst_n,
                output reg [ 7:0]  led,
                output             ram_lb_n,
                output             ram_ub_n,
                inout      [15:0]  ram_data,
                output             ram_oe_n,
                output             ram_ce1_n,
                output             ram_we_n,
                output             ram_ce2,
                output      [17:0] ram_addr,
                input              exp_pres,
                input              rxd,
                output             txd,

                inout       [3:32] X202
                );

   parameter                       FREQ = 27'd50000000; // 27-bit is enough for 268 MHz
   parameter                       BPS = 230400;

   wire                            reset_button = 0 /*~exp_rst_n*/;
   reg [26:0]                      rst_counter = FREQ;
   wire                            reset = ~rst_counter[26];

   wire                            clock;
   wire                            clock_locked;

   wire [ 7:0]                     rs232out_d;
   wire                            rs232out_w;
   wire                            rs232out_busy;

   wire [ 7:0]                     rs232in_data;
   wire                            rs232in_attention;

   wire                            mem_waitrequest;
   wire [ 1:0]                     mem_id;
   wire [29:0]                     mem_address;
   wire                            mem_read;
   wire                            mem_write;
   wire [31:0]                     mem_writedata;
   wire [ 3:0]                     mem_writedatamask;
   wire [31:0]                     mem_readdata;
   wire [ 1:0]                     mem_readdataid;

   wire `REQ                       rs232_req;
   wire `RES                       rs232_res;

   always @(posedge clock)
      if (rs232out_w)
         led <= ~rs232out_d;
      else if (rs232in_attention)
         led <= ~rs232in_data;

   always @(posedge clock)
      if (reset_button | ~clock_locked)
         rst_counter <= FREQ; // 1 sec delay
      else if (~rst_counter[26])
         rst_counter <= rst_counter - 1'd1;

   // Actually, just a 1-1 clock filter at this point
   pll	pll_inst (
	.inclk0 ( clk ),
	.c0 ( clock ),
	.locked ( clock_locked )
	);

   yari yari_inst
      (.clock(clock)
       ,.rst(reset)

       // Inputs
       ,.mem_waitrequest  (mem_waitrequest)
       ,.mem_readdata     (mem_readdata)
       ,.mem_readdataid   (mem_readdataid)

       // Outputs
       ,.mem_id           (mem_id)
       ,.mem_address      (mem_address)
       ,.mem_read         (mem_read)
       ,.mem_write        (mem_write)
       ,.mem_writedata    (mem_writedata)
       ,.mem_writedatamask(mem_writedatamask)

       ,.peripherals_req(rs232_req)
       ,.peripherals_res(rs232_res)
       );
   defparam yari_inst.FREQ = FREQ;

   assign ram_ce2 = 1;
   sram16_ctrl sram16_ctrl_inst
      (.clock(clock)
       ,.rst(reset)
       ,.mem_waitrequest(mem_waitrequest)
       ,.mem_id(mem_id)
       ,.mem_address(mem_address)
       ,.mem_read(mem_read)
       ,.mem_write(mem_write)
       ,.mem_writedata(mem_writedata)
       ,.mem_writedatamask(mem_writedatamask)
       ,.mem_readdata(mem_readdata)
       ,.mem_readdataid(mem_readdataid)

       ,.sram_a(ram_addr)
       ,.sram_d(ram_data)
       ,.sram_cs_n(ram_ce1_n)
       ,.sram_be_n({ram_ub_n,ram_lb_n})
       ,.sram_oe_n(ram_oe_n)
       ,.sram_we_n(ram_we_n)
       );
   defparam sram16_ctrl_inst.FREQ = FREQ;

      rs232out rs232out_inst
      (.clock(clock),
       .serial_out(txd),
       .transmit_data(rs232out_d),
       .we(rs232out_w),
       .busy(rs232out_busy));

   defparam rs232out_inst.frequency = FREQ,
            rs232out_inst.bps       = BPS;

   rs232in rs232in_inst
      (.clock(clock),
       .serial_in(rxd),
       .received_data(rs232in_data),
       .attention(rs232in_attention));

   defparam rs232in_inst.frequency = FREQ,
            rs232in_inst.bps       = BPS;

   rs232 rs232_inst(.clk(clock),
               .rst(reset),

               .rs232_req(rs232_req),
               .rs232_res(rs232_res),

               .rs232in_attention(rs232in_attention),
               .rs232in_data(rs232in_data),

               .rs232out_busy(rs232out_busy),
               .rs232out_w(rs232out_w),
               .rs232out_d(rs232out_d));


   // Extension board X202
   reg [3:32] x202_counter = 0;

   always @(posedge clock)
      x202_counter <= x202_counter + 1;

   assign X202 = x202_counter;
endmodule

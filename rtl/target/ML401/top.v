// -----------------------------------------------------------------------
//
//   Copyright 2004,2007-2009 Tommy Thorn - All Rights Reserved
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
//   Bostom MA 02111-1307, USA; either version 2 of the License, or
//   (at your option) any later version; incorporated herein by reference.
//
// -----------------------------------------------------------------------

`timescale 1ns/10ps
`include "../../soclib/pipeconnect.h"
module main
   (input         iSYS_CLK_100
   ,input         iSYS_RST

   ,input         iUART_RXD
   ,output        oUART_TXD

   );

   parameter FREQ = 100_000_000; // match clock frequency
   parameter BPS  =       9_600; // Serial speed

   // Copied from yari.v
   parameter   ID_DC              = 2'd1;
   parameter   ID_IC              = 2'd2;
   parameter   ID_FB              = 2'd3;

   wire        clock; // The master clock

   /*
   wire        video_clock;
   wire        clock_locked;

   // Actually, just a 1-1 clock filter for c0
   // and a 65 MHz for video_clock
   pll  pll_inst(.inclk0(iCLK_50)
                ,.c0(clock)
                ,.c2(video_clock)
                ,.locked(clock_locked));
   reg           iSW17_, iSW17, manual_reset;
   */
   assign      clock = iSYS_CLK_100;
   wire        reset = iSYS_RST;

   wire [ 7:0]   rs232out_transmit_data;
   wire          rs232out_write_enable;
   wire          rs232out_busy;

   wire [ 7:0]   rs232in_received_data;
   wire          rs232in_received_data_valid;

   wire          mem_waitrequest;
   wire    [1:0] mem_id;
   wire   [29:0] mem_address;
   wire          mem_read;
   wire          mem_write;
   wire   [31:0] mem_writedata;
   wire    [3:0] mem_writedatamask;
   wire   [31:0] mem_readdata;
   wire    [1:0] mem_readdataid;

   wire          yari_mem_waitrequest;
   wire    [1:0] yari_mem_id;
   wire   [29:0] yari_mem_address;
   wire          yari_mem_read;
   wire          yari_mem_write;
   wire   [31:0] yari_mem_writedata;
   wire    [3:0] yari_mem_writedatamask;

   wire `REQ     rs232_req;
   wire `RES     rs232_res;

   yari yari_inst(
         .clock(clock)
        ,.rst(reset)

        // Inputs
        ,.mem_waitrequest  (yari_mem_waitrequest)
        ,.mem_readdata     (mem_readdata)
        ,.mem_readdataid   (mem_readdataid)

        // Outputs
        ,.mem_id           (yari_mem_id)
        ,.mem_address      (yari_mem_address)
        ,.mem_read         (yari_mem_read)
        ,.mem_write        (yari_mem_write)
        ,.mem_writedata    (yari_mem_writedata)
        ,.mem_writedatamask(yari_mem_writedatamask)

        ,.peripherals_req(rs232_req)
        ,.peripherals_res(rs232_res)
        );

   rs232out rs232out_inst
      (.clock        (clock),
       .serial_out   (oUART_TXD),
       .transmit_data(rs232out_transmit_data),
       .we           (rs232out_write_enable),
       .busy         (rs232out_busy));

   defparam rs232out_inst.frequency = FREQ,
            rs232out_inst.bps       = BPS;


   rs232in rs232in_inst
      (.clock        (clock),
       .serial_in    (iUART_RXD),
       .received_data(rs232in_received_data),
       .attention    (rs232in_received_data_valid));

   defparam rs232in_inst.frequency = FREQ,
            rs232in_inst.bps       = BPS;

   wire [31:0] vsynccnt;

   rs232 rs232_inst(.clk(clock),
               .rst(reset),

               .iKEY(0),
               .vsynccnt(vsynccnt),

               .rs232_req(rs232_req),
               .rs232_res(rs232_res),

               .rs232in_attention(rs232in_received_data_valid),
               .rs232in_data     (rs232in_received_data),

               .rs232out_busy(rs232out_busy),
               .rs232out_w   (rs232out_write_enable),
               .rs232out_d   (rs232out_transmit_data));


   assign      mem_id             = yari_mem_id;
   assign      mem_address        = yari_mem_address;
   assign      mem_read           = yari_mem_read;
   assign      mem_write          = yari_mem_write;
   assign      mem_writedata      = yari_mem_writedata;
   assign      mem_writedatamask  = yari_mem_writedatamask;
   assign      yari_mem_waitrequest = mem_waitrequest;
endmodule

// -----------------------------------------------------------------------
//
//   Copyright 2004,2007-2008 Tommy Thorn - All Rights Reserved
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
   (input         iCLK_50

   ,input         iUART_RXD
   ,output        oUART_TXD

   ,output [18:0] oSRAM_A
   ,output        oSRAM_ADSC_N		// Burst control
   ,output        oSRAM_ADSP_N		// --
   ,output        oSRAM_ADV_N		// --
   ,output [ 3:0] oSRAM_BE_N
   ,output        oSRAM_CE1_N
   ,output        oSRAM_CE2
   ,output        oSRAM_CE3_N
   ,output        oSRAM_CLK
   ,inout  [ 3:0] SRAM_DPA		// ssram parity data bus
   ,inout  [31:0] SRAM_DQ
   ,output        oSRAM_GW_N		// global write (overrides byte enable)
   ,output        oSRAM_OE_N
   ,output        oSRAM_WE_N
   ,output [ 7:0] oLEDG
   ,output [17:0] oLEDR
   );

   parameter FREQ = 50_000_000; // match clock frequency
   parameter BPS  =    115_200; // Serial speed

   wire      clock; // The master clock
   wire      clock_locked;

   // Actually, just a 1-1 clock filter at this point
   pll  pll_inst(.inclk0(iCLK_50)
                ,.c0(clock)
                ,.locked(clock_locked));
   wire          reset = !clock_locked;

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

   wire `REQ     rs232_req;
   wire `RES     rs232_res;

//`define TEST_SSRAM_CTRL 1
`ifndef TEST_SSRAM_CTRL
   yari yari_inst(
         .clock(clock)
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

        ,.peripherals_req(rs232_req)
        ,.peripherals_res(rs232_res)
        );
`else
   wire   [31:0] errors;
   assign {oLEDR,oLEDG} = errors;

   dummy_master dummy_master(
         .clock(clock)
        ,.reset(reset)

        ,.mem_waitrequest(mem_waitrequest)
        ,.mem_id(mem_id)
        ,.mem_address(mem_address)
        ,.mem_read(mem_read)
        ,.mem_write(mem_write)
        ,.mem_writedata(mem_writedata)
        ,.mem_writedatamask(mem_writedatamask)
        ,.mem_readdata(mem_readdata)
        ,.mem_readdataid(mem_readdataid)

        ,.errors(errors)
   );
`endif

   // Note SRAM_ZZ is pulled low and is not connected to FPGA (AFAICT)

   ssram_ctrl the_ssram_ctrl
      (.clock(clock)
      ,.reset(reset)
      ,.mem_waitrequest(mem_waitrequest)
      ,.mem_id(mem_id)
      ,.mem_address(mem_address)
      ,.mem_read(mem_read)
      ,.mem_write(mem_write)
      ,.mem_writedata(mem_writedata)
      ,.mem_writedatamask(mem_writedatamask)
      ,.mem_readdata(mem_readdata)
      ,.mem_readdataid(mem_readdataid)

      ,.sram_clk   (oSRAM_CLK)
      ,.sram_adsc_n(oSRAM_ADSC_N)
      ,.sram_adv_n (oSRAM_ADV_N)
      ,.sram_a     (oSRAM_A    )
      ,.sram_be_n  (oSRAM_BE_N )
      ,.sram_oe_n  (oSRAM_OE_N )
      ,.sram_we_n  (oSRAM_WE_N )
      ,.sram_dpa   (SRAM_DPA  )
      ,.sram_dq    (SRAM_DQ)
      ,.sram_adsp_n(oSRAM_ADSP_N )
      ,.sram_ce1_n (oSRAM_CE1_N  )
      ,.sram_ce2   (oSRAM_CE2    )
      ,.sram_ce3_n (oSRAM_CE3_N  )
      ,.sram_gw_n  (oSRAM_GW_N   )
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

   rs232 rs232_inst(.clk(clock),
               .rst(reset),

               .rs232_req(rs232_req),
               .rs232_res(rs232_res),

               .rs232in_attention(rs232in_received_data_valid),
               .rs232in_data     (rs232in_received_data),

               .rs232out_busy(rs232out_busy),
               .rs232out_w   (rs232out_write_enable),
               .rs232out_d   (rs232out_transmit_data));
endmodule

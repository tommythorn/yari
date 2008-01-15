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
module main(input  wire        CLK_48_MHZ,

            // Table 1
            //output wire        BURST_MODE,
            input  wire        CHRG_N,
            input  wire        LBO_N,

            // Table 2
            output wire        ENABLE_12V,

            // Table 3
            //output wire        ADC_SPI_DIN,
            input  wire        ADC_SPI_DOUT,
            //output wire        ADC_SPI_CLK,
            //output wire        ADC_SPI_CS_N,

            // Table 4
            //output wire        MISO, // XXX IS THE DIRECTION CORRECT?
            input  wire        MOSI,
            input  wire        SPI_CLK,

            // Table 5
            //output wire        AUDIO_SPI_DATA,
            //output wire        AUDIO_SPI_EN,
            //output wire        AUDIO_SPI_CLK,

            //output wire        AUDIO_I2C_DATA,
            //output wire        AUDIO_I2C_WS,
            //output wire        AUDIO_I2C_CLK,
            //output wire        AUDIO_CLK,

            // Table 6
            //output wire [16:0] FLASH_A,
            //inout  wire [15:0] FLASH_D,
            input  wire        FLASH_WAIT,
            //output wire        FLASH_ADV_N,
            //output wire        FLASH_CLK,
            //output wire        FLASH_RST_N,
            //output wire        FLASH_OE_N,
            //output wire        FLASH_CE_N,
            //output wire        FLASH_WP_N,
            //output wire        FLASH_WE_N, // XXX ?

            // Table 7
            input  wire        SD_CARD_SPI_MISO, // SD_D[0]
            //output wire        SD_CARD_SPI_MOSI,
            ////output wire      ADC_SPI_CLK,
            //output wire        SD_SCARD_SPI_CS_N,

            // Table 8
            input  wire        SD_PROTECT_N,
            input  wire        SD_DETECT_N,
            input  wire        SD_D2,
            input  wire        SD_D1,

            // Table 9
            //output wire        HSEN,

            // Table 10
            inout  wire [ 7:0] DISPLAY_D,
            //output wire        DISPLAY_ERD_N,
            //output wire        DISPLAY_RW_N,
            //output wire        DISPLAY_DC_N,
            //output wire        DISPLAY_BSI,
            //output wire        DISPLAY_RES_N,
            //output wire        DISPLAY_CS_N,

            // Table 11
            input  wire [ 5:0] USER_PB, // {ENTER,ESC,DOWN,UP,RIGHT,LEFT}
            input  wire        BUTTON_INT_N,

            // Table 12
	    output wire [ 3:0] USER_LED,

            // Table 13
            inout  wire [12:1] USER_IO);

   parameter FREQ = 48_000_000; // match clock frequency
   parameter BPS  =    115_200; // Serial speed

   wire clock = CLK_48_MHZ;

   assign USER_LED = rst_counter[25:22];

   reg [26:0] rst_counter = 0;
   always @(posedge clock)
      if (~USER_PB[0])
         rst_counter <= 'd48_000_000;
      else if (~rst_counter[26])
         rst_counter <= rst_counter - 1;

   wire rst = ~rst_counter[26];

   wire [ 7:0]   rs232out_d;
   wire          rs232out_w;
   wire          rs232out_busy;

   wire [ 7:0]   rs232in_data;
   wire          rs232in_attention;

   wire          ttyb_txd;

   assign          USER_IO[11] = 1'bz; // Input
   assign          USER_IO[12] = ttyb_txd;
   wire            ttyb_rxd    = USER_IO[11];

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

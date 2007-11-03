// -----------------------------------------------------------------------
//
//   Copyright 2004 Tommy Thorn - All Rights Reserved
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


/* 1000 words:

   YARI

   _________
  |         |
  | Stage_I |<=============\
  |_________|              \\
       |                    \\
   ____v____                 \\
  |         |                 \\
  | Stage D |                  \\
  |_________|                   \\
       |                         \\
   ____v____                      \\           Targets
  |         |                      \\
  | Stage X |                       \|
  |_________|                       ||master
       |                            ||
   ____v____                     __\||/___      _________
  |         |           master  |         |/   |         |
  | Stage M |<=================>| Bus Ctrl|<==>| Periph. |
  |_________|                   |___  ____|\   |_________|
       |                           /||\    \\   _________
   ____v____                  master||      \\ |         |
  |         |                    ___||____   \>|SRAM ctrl|
  | Stage W |                   |         |    |_________|
  |_________|                   | VGA FB  |
                                |_________|


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

   blockram blockram(.clock(clock),
                     .rst(rst),

                     .sram_ctrl_req(sram_req),
                     .sram_ctrl_res(sram_res));

   rs232out #(115200,48000000)
      rs232out_inst(.clk25MHz(clock),
               .reset(0),

               .serial_out(ttyb_txd),
               .transmit_data(rs232out_d),
               .we(rs232out_w),
               .busy(rs232out_busy));

   rs232in #(115200,48000000)
      rs232in_inst(.clk25MHz(clock),
                   .reset(0),

                   .serial_in(ttyb_rxd),
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

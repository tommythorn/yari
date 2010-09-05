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
   ,input         iCLK_28 // Actually 28.63636 MHz

   ,input         iUART_RXD
   ,output        oUART_TXD

   ,input  [ 3:0] iKEY
   ,input  [17:0] iSW
   ,output [ 7:0] oLEDG
   ,output [17:0] oLEDR

   ,output [18:0] oSRAM_A
   ,output        oSRAM_ADSC_N          // Burst control
   ,output        oSRAM_ADSP_N          // --
   ,output        oSRAM_ADV_N           // --
   ,output [ 3:0] oSRAM_BE_N
   ,output        oSRAM_CE1_N
   ,output        oSRAM_CE2
   ,output        oSRAM_CE3_N
   ,output        oSRAM_CLK
   ,inout  [ 3:0] SRAM_DPA              // ssram parity data bus
   ,inout  [31:0] SRAM_DQ
   ,output        oSRAM_GW_N            // global write (overrides byte enable)
   ,output        oSRAM_OE_N
   ,output        oSRAM_WE_N

   ,output             oVGA_CLOCK
   ,output wire [ 9:0] oVGA_R
   ,output wire [ 9:0] oVGA_B
   ,output wire [ 9:0] oVGA_G
   ,output wire        oVGA_BLANK_N
   ,output wire        oVGA_HS
   ,output wire        oVGA_VS
   ,output             oVGA_SYNC_N

   ,inout       [14:0] FLASH_DQ       // FLASH Data bus 15 Bits (0 to 14)
   ,inout              FLASH_DQ15_AM1 // FLASH Data bus Bit 15 or Address A-1
   ,output      [21:0] oFLASH_A       // FLASH Address bus 22 Bits
   ,output             oFLASH_WE_N    // FLASH Write Enable
   ,output             oFLASH_RST_N   // FLASH Reset
   ,output             oFLASH_WP_N    // FLASH Write Protect /Programming Acceleration
   ,input              iFLASH_RY_N    // FLASH Ready/Busy output
   ,output             oFLASH_BYTE_N  // FLASH Byte/Word Mode Configuration
   ,output             oFLASH_OE_N    // FLASH Output Enable
   ,output             oFLASH_CE_N    // FLASH Chip Enable
   );

   parameter FREQ = 50_000_000; // match clock frequency
   parameter BPS  =    115_200; // Serial speed

   // Copied from yari.v
   parameter   ID_DC              = 2'd1;
   parameter   ID_IC              = 2'd2;
   parameter   ID_FB              = 2'd3;

   wire        clock; // The master clock
   wire        video_clock;
   wire        clock_locked;

   // Actually, just a 1-1 clock filter for c0
   // and a 65 MHz for video_clock
   pll  pll_inst(.inclk0(iCLK_50)
                ,.c0(clock)
                ,.c2(video_clock)
                ,.locked(clock_locked));
   reg           iSW17_, iSW17, manual_reset;
   wire          reset = !clock_locked | manual_reset;
   always @(posedge clock) begin
      iSW17_ <= iSW[17];
      iSW17 <= iSW17_;
      manual_reset <= iSW17;
   end

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

   wire   [29:0] fb_address;
   wire          fb_read;

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

   ssram_ctrl the_ssram_ctrl
      (.clock(clock)
      ,.reset(reset)

      // Outputs
      ,.mem_waitrequest  (mem_waitrequest)
      ,.mem_readdata     (mem_readdata)
      ,.mem_readdataid   (mem_readdataid)

      // Inputs
      ,.mem_id           (mem_id)
      ,.mem_address      (mem_address)
      ,.mem_read         (mem_read)
      ,.mem_write        (mem_write)
      ,.mem_writedata    (mem_writedata)
      ,.mem_writedatamask(mem_writedatamask)

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

      ,.FLASH_DQ(FLASH_DQ)
      ,.FLASH_DQ15_AM1(FLASH_DQ15_AM1)
      ,.oFLASH_A(oFLASH_A)
      ,.oFLASH_WE_N(oFLASH_WE_N)
      ,.oFLASH_RST_N(oFLASH_RST_N)
      ,.oFLASH_WP_N(oFLASH_WP_N)
      ,.iFLASH_RY_N(iFLASH_RY_N)
      ,.oFLASH_BYTE_N(oFLASH_BYTE_N)
      ,.oFLASH_OE_N(oFLASH_OE_N)
      ,.oFLASH_CE_N(oFLASH_CE_N)
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

               .iKEY({iKEY[0],iKEY[1],iKEY[2],iKEY[3]}),
               .vsynccnt(vsynccnt),

               .rs232_req(rs232_req),
               .rs232_res(rs232_res),

               .rs232in_attention(rs232in_received_data_valid),
               .rs232in_data     (rs232in_received_data),

               .rs232out_busy(rs232out_busy),
               .rs232out_w   (rs232out_write_enable),
               .rs232out_d   (rs232out_transmit_data));


   wire        fb_access = fb_read /* & !yari_mem_read & !yari_mem_write */;

   assign      mem_id             = fb_access ? ID_FB      : yari_mem_id;
   assign      mem_address        = fb_access ? fb_address : yari_mem_address;
   assign      mem_read           = fb_access ? fb_read    : yari_mem_read;
   assign      mem_write          = fb_access ? 1'd0       : yari_mem_write;
   assign      mem_writedata      =                          yari_mem_writedata;
   assign      mem_writedatamask  =                          yari_mem_writedatamask;

   wire        fb_waitrequest       = mem_waitrequest;
   assign      yari_mem_waitrequest = mem_waitrequest | fb_access;

/*
 Sadly, iCLK_50 can at most feed one PLL
   video_pll video_pll_inst
      (.inclk0(iCLK_50)
       ,.c0(video_clock));
 */

   video video_inst
      (.memory_clock    (clock)
      ,.fb_waitrequest  (fb_waitrequest)
      ,.fb_readdata     (mem_readdata)
      ,.fb_readdatavalid(mem_readdataid == ID_FB)
      ,.fb_address      (fb_address)
      ,.fb_read         (fb_read)
      ,.vsynccnt        (vsynccnt)

      ,.video_clock(video_clock)
      ,.oVGA_CLOCK(oVGA_CLOCK)
      ,.oVGA_R(oVGA_R)
      ,.oVGA_B(oVGA_B)
      ,.oVGA_G(oVGA_G)
      ,.oVGA_BLANK_N(oVGA_BLANK_N)
      ,.oVGA_HS(oVGA_HS)
      ,.oVGA_VS(oVGA_VS)
      ,.oVGA_SYNC_N(oVGA_SYNC_N)
      );

   /* Attractive sub 1 MiB options

    VESA
    ModeLine "1024x768_60.00"  65.0    1024 1048 1184 1344  768  771  777  806 -hsync -vsync

    GTF
    ModeLine "1152x864_75.00" 108.0    1152 1216 1344 1600  864  865  868  900 +hsync +vsync
    ModeLine "1280x768_60.00"  80.14   1280 1344 1480 1680  768  769  772  795 -hsync +vsync

    GTF, but may not work with LCD?
    Modeline "1280x819_60.00"  85.48   1280 1344 1480 1680  819  820  823  848 -HSync +Vsync
    Modeline "1368x766_60.00"  85.64   1368 1440 1584 1800  766  767  770  793 -HSync +Vsync
   */
   defparam    video_inst.FB_BEGIN = 1024*1024 / 4,
               video_inst.FB_SIZE  = 1024*768 / 4,
               video_inst.FB_MASK  = ~0,
               video_inst.M1 = 12'd1024,
               video_inst.M2 = 12'd1048,
               video_inst.M3 = 12'd1184,
               video_inst.M4 = 12'd1344,

               video_inst.M5 = 12'd768,
               video_inst.M6 = 12'd771,
               video_inst.M7 = 12'd777,
               video_inst.M8 = 12'd806,
               video_inst.HS_NEG = 1'd0,
               video_inst.VS_NEG = 1'd0;
endmodule

//
// videotest.v
//
// Simple VGA graphics generator
//
// We use the standard VGA "text" monitor timings mode,
// htime = 31.77 us (31.47 kHz), vtime = 14.27 ms (70 Hz)
// The standard VGA uses a pixel clock of 25.175 MHz, we use 25 MHz,
// and round to multiples of 8.  The error is about 0.8%, which
// is far, far less than the margin of error in real systems.

// TODO
// - I really need to replace these timings with a more standard
//   VGA timings and get the framebuffer working. [DONE]
// - switch to 32-bit framebuffer [DONE]
// - more "interesting" initialization for the framebuffer. [DONE]
// - switch to 1 ported RAM, multiplexed [DONE]
// - switch to SRAM (+ 8-bit color) [DONE]

//
// This gives us the following timings:
// Horizontal:  96 pixels (12 char) sync
//              48 pixels ( 6 char) back porch/border
//             640 pixels (80 char) graphics
//              16 pixels ( 2 char) front porch
// Vertical:     2 lines sync
//              41 lines back porch/border
//             384 lines graphics (24 rows @ 16 pixels)
//              22 lines front porch
//
// The DAC used introduced in the Lancelot card introduces
// 8 cycles of delay.
//
// Framebuffer
//   640 * 384 = 240KiB
// 18 bits of address.  However, we can at most get 16, so we the width
// make four times larger.
//
// Unfortunately, we only have 294,912 bits to play with, thus one bit deep

/*
   VGA 640x480 core
   Tommy Thorn

   This module assumes a 25Mhz clock and implements VESA VGA output
   in the 640 x 480 resolution, based on the XFree86 modeline

       "640x480" 25.175  640 664 760 800  480 491 493 525

My original timing values

*/

`timescale 1ns/10ps
`include "pipeconnect.h"

`define ONE_BPP 1

module vga(// Clock
           input  wire        clk25MHz,        // PLL input clock
           input  wire        rst,

           // Lancelot VGA interface
           output wire  [7:0] vga_r,
           output wire  [7:0] vga_g,
           output wire  [7:0] vga_b,
           output wire        vga_m1,
           output wire        vga_m2,
           output wire        vga_sync_n,
           output wire        vga_sync_t,
           output wire        vga_blank_n,
           output reg         vga_hs = 0,
           output reg         vga_vs = 0,

           input  wire [31:0] fb_addr0,   // Top of FB

           // Memory port
           output reg  `REQ   fb_req = 0,
           input  wire `RES   fb_res);

   parameter      debug = 0;

   parameter      M1 = 640;
   parameter      M2 = 664;
   parameter      M3 = 760;
   parameter      M4 = 800;

   parameter      M5 = 480;
   parameter      M6 = 491;
   parameter      M7 = 493;
   parameter      M8 = 525;

   parameter      BPP   =  24;
   parameter      FBWL2 =  5;
   parameter      FBW   = (1 << FBWL2);

   parameter      BUFL2 = 2; // must be at least 1

   // VGA interface
   assign         vga_sync_t  = 0;         // No sync-on-RGB
   assign         vga_sync_n  = 1;
   assign         vga_m1      = 0;         // Color space configuration: GBR
   assign         vga_m2      = 0;
   /*
    * The blanking facility didn't work right for me and it seems I
    * don't really need it.
    */
   assign         vga_blank_n = 1;
   wire           hsync_neg   = 1;         // Negative hsync
   wire           vsync_neg   = 1;         // Positive vsync

   wire [9:0]     x_blank = M1;        // 640
   wire [9:0]     x_sync  = M2;        // 664
   wire [9:0]     x_back  = M3;        // 760
   wire [9:0]     x_max   = M4;        // 800

   wire [9:0]     y_blank = M5;        // 480
   wire [9:0]     y_sync  = M6;        // 491
   wire [9:0]     y_back  = M7;        // 493
`ifdef SIMULATE_MAIN
   wire [9:0]     y_max   = M8;        // 525
`else
   wire [9:0]     y_max   = M8;        // 525
`endif

   reg [9:0]      x = M4-5;  // Diverging from FPGA here to hit issues earlier.
   reg [9:0]      y = M8-1;  // Diverging from FPGA here to hit issues earlier.
   reg [9:0]      frame_ctr = 0;

   /* PIXEL */
   reg [31:0]     pixels32 = 0;

   reg [23:0]     vga_pixel = 0;
   assign         vga_r = vga_pixel[23:16];
   assign         vga_g = vga_pixel[15: 8];
   assign         vga_b = vga_pixel[ 7: 0];


   // red   3-bit
   // green 2-bit
   // blue  2-bit
   // ============
   //       8-bit


   /* FIFO */
   reg  [     31:0] pixel_buffer[0:(1 << BUFL2) - 1]; // Initialised at the end.
   reg  [     31:0] pixel_buffer_addr = 'h400E6A00; // Diverging from FPGA here to hit issues earlier.
   reg  [BUFL2-1:0] pixel_buffer_rp = 0, pixel_buffer_wp = 0;
   wire [BUFL2-1:0] pixel_buffer_rp_plus1 = pixel_buffer_rp + 1;
   wire [BUFL2-1:0] pixel_buffer_wp_plus1 = pixel_buffer_wp + 1;
   wire [BUFL2-1:0] pixel_buffer_wp_plus2 = pixel_buffer_wp + 2;
   wire [BUFL2-1:0] pixel_buffer_wp_plus3 = pixel_buffer_wp + 3;
   reg  [BUFL2-1:0] free = (1 << BUFL2) - 3;  // Diverging from FPGA here to hit issues earlier.
   reg              rd_valid = 0;


   /*
    clk   __/~~\__/~~\__/~~\__/~~\__/~~\__/~~\__/~~\__/~~\
    addr  <## AD #######:##>--:---------------------------
    read  /~~~~~~~~~~~~~:~~\______________________________
    wait  _/~~~~~~~~~\__:_____:___________________________
    data  _________________<#D0#>_________________________
                        :     :
Continuous reading
            1     2     3     4     5     6     7     8
    clk   __/~~\__/~~\__/~~\__/~~\__/~~\__/~~\__/~~\__/~~\
    addr  <###AD0#######:##><AD1#><#####AD2##>------------
    read  /~~~~~~~~~~~~~~~~\/~~~~\/~~~~~~~~~~\____________
    wait  _/~~~~~~~~~~\____________/~~~~\_________________
    data  _________________<#D0#><#D1#>______<#D2#>_______

Master output are valid on the rising edge, Slave output are sampled
at rising edge.


    FIFO workings.  Each clock cycle these events can occur (simultaneously):

    1. If the last pixel in a word was displayed, one datum was
       consumed, advancing the pixel_buffer_rp (no check for empty fifo).
    2. If a read is ready, one datum can be produced advancing the
       pixel_buffer_wp
    3. If the fifo isn't full, a read will be scheduled

    Because of the latencies involved, the controller will generally
    issue a burst of three reads before the first data tickles in,
    thus we keep a buffer of three free slots in the fifo.
 */

   always @(posedge clk25MHz) begin
      if (rst)  fb_req`R   <= 0;
        fb_req`W   <= 0;
        fb_req`WD  <= 0;
        fb_req`WBE <= 0;

        vga_hs     <= (x_sync <= x && x < x_back) ^ vsync_neg;
        vga_vs     <= (y_sync <= y && y < y_back) ^ hsync_neg;

        // $display("%05d VGA: rp %d wp %d free %d",
        //          $time, pixel_buffer_rp, pixel_buffer_wp, free);

        /* Pipeconnect read data always have one cycle latency. */
        rd_valid <= fb_req`R & ~fb_res`WAIT;
        if (rd_valid) begin
           if (debug)
             $display("%05d VGA: FIFO got %x at pos %d", $time, fb_res`RD, pixel_buffer_wp);
           pixel_buffer[pixel_buffer_wp] <= fb_res`RD;
           pixel_buffer_wp               <= pixel_buffer_wp_plus1;
        end

        if (~fb_req`R & fb_res`WAIT)
          $display("%05d VGA: IMPOSSIBLE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", $time);

        if (fb_res`WAIT && debug)
          $display("%05d VGA: MEMORY BUSY", $time);

        if (~fb_res`WAIT) begin
           /*
            * Only issue more requests if it won't overflow the FIFO.
            *
            * Writing out the explanation below convinced me that it was
            * much simpler and better to simple introduce a count of
            * free slots in the FIFO.
            *
            * OLD AND STALE DESCRIPTION
            * If pixel_buffer_wp == pixel_buffer_rp then we risk
            * overwriting the next element of the FIFO so we reserve
            * this to mean FIFO empty. That means we should stop
            * requesting data when pixel_buffer_wp+1 == pixel_buffer_rp.
            * However, due to the one-outstanding-read nature of pipeconnect,
            * we must be one ahead.
            *
            * There is a race condition when we receive a word
            * in the same cycle, so we must also guard against
            * pixel_buffer_wp+3 == pixel_buffer_rp (it's better to
            * loose a position in the FIFO than the pay to price for the
            * acurate check).
            *
            * There's a corresponding race with pixel_buffer_rp but that
            * one we can ignore as at worst it will delay the next fetch
            * request by one cycle.
            *
            * Notice also, that we can't just write
            *  pixel_buffer_wp+2 != pixel_buffer_rp
            * as the synthesizer (and simulator) will extend the width
            * of expressions to guard against overflow whereas we want
            * "wraparound" (modulo) semantics.
            *
            */

           /*    OLD
            if (pixel_buffer_rp != pixel_buffer_wp_plus1 &&
                pixel_buffer_rp != pixel_buffer_wp_plus2 &&
                pixel_buffer_rp != pixel_buffer_wp_plus3) begin
            */

           if (free != 0) begin
              if (debug)
                $display("%05d VGA: SCHEDULE READ from %x (fifo %d free)", 
                         $time, pixel_buffer_addr, free);

              fb_req`A          <= pixel_buffer_addr;
              fb_req`R          <= 1;
              pixel_buffer_addr <= pixel_buffer_addr + 4;
              free              <= free - 1;
           end else begin
              // $display("%5d VGA: Ok, FIFO FULL, stop reading", $time);
              fb_req`R <= 0;
           end
        end

        /* Get the pixel */
        if (x < x_blank && y < y_blank ||
            x == x_max-1 && y == y_max-1) // GROSS HACK
          begin
           /*
            * Grab one bit from the tiny pixel buffer and expand it to
            * 24 for black or white.
            */
           vga_pixel <= pixels32[31] ? 24'hFFFFFF :
                        (x ==   0)   ? 24'h0000FF :
                        (y ==   0)   ? 24'h00FF00 :
                        (x == 639)   ? 24'hFF0000 :
                        (y == 479)   ? 24'h00FFFF :
                                       24'h000000 ;
                         // {24{pixels32[31]}};

           if (x[4:0] == 31) begin
              /*
               * We just consumed the last pixel in the tiny pixel
               * buffer, so refill it from the pixel_buffer FIFO.
               *
               * Notice there's no underflow check as this can't
               * happen (underflow would be catastrophic and point to
               * either lack of memory bandwidth or too small a FIFO).
               */
              pixels32 <= pixel_buffer[pixel_buffer_rp];
              pixel_buffer_rp <= pixel_buffer_rp_plus1;
              if (debug)
                $display("%05d VGA: just read in %x", $time, pixel_buffer[pixel_buffer_rp]);

              if (~fb_res`WAIT && free != 0) begin
                 /* We issued another read, so free remains unchanged. */
                 if (debug)
                   $display("%05d VGA: Simultaneous loving babe!", $time);
                 free <= free;
              end else begin
                 free <= free+1;
              end
           end else begin
              pixels32 <= {pixels32[30:0],1'h1};
           end
        end else begin
           vga_pixel <= 24'h0;

           if (y == y_max-2) begin
              /*
               * We just displayed last visible pixel in this frame.
               * Resynchronize, clear the fifo, and start fetching from fb_addr0.
               */
              // frame_ctr       <= frame_ctr + 1;
              // if (~fb_res`WAIT) begin
                 fb_req`A        <= 0;
                 fb_req`R        <= 0;
                 pixel_buffer_addr <= fb_addr0;
                 pixel_buffer_wp <= 0;
                 pixel_buffer_rp <= 0;
                 free            <= ~0;
              // end
           end
        end

        /* Advance the (x,y) pointer. */
        if (x == x_max-1)
          y <= (y == y_max-1) ? 0 : y+1;
        x <= (x == x_max-1) ? 0 : x+1;
     end

     reg [31:0] i;
     initial for (i = 0; i < (1 << BUFL2) - 1; i = i + 1) pixel_buffer[i] = 0;
endmodule

`ifdef SIMULATE_VGA
module tester();
   reg         clk25MHz, rst;

   // Lancelot VGA interface
   wire  [7:0] vga_r;
   wire  [7:0] vga_g;
   wire  [7:0] vga_b;
   wire        vga_m1;
   wire        vga_m2;
   wire        vga_sync_n;
   wire        vga_sync_t;
   wire        vga_blank_n;
   wire        vga_hs;
   wire        vga_vs;

   reg  [31:0] fb_addr0;   // Top of FB

   // Memory port
   wire `REQ   fb_req;
   wire `RES   fb_res;

   reg  holdit;

   reg [31:0] addr;

   assign fb_res`WAIT = holdit & (fb_req`R | fb_req`W);
   assign fb_res`RD   = addr;

   always @(posedge clk25MHz) addr <= fb_req`A;

   vga vga(clk25MHz, rst, vga_r, vga_g, vga_b,
           vga_m1, vga_m2, vga_sync_n, vga_sync_t, vga_blank_n, vga_hs, vga_vs,
           'h9000_0000,
           fb_req, fb_res);

   always #20 clk25MHz = ~clk25MHz;
   initial begin
      #0 clk25MHz = 0; rst = 1; holdit = 0;
      #40 rst = 0;

      $monitor(clk25MHz, rst, vga_r,vga_g,vga_b);
      #4000 holdit = 1; $display("%05d VGA: HOLDIT", $time);
      #110000 holdit = 0; $display("%05d VGA: ~HOLDIT", $time);
   end
endmodule
`endif //  `ifdef SIMULATE_VGA

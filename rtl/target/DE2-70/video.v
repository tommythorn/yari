/*
 Video core

 We operate in two clock domains, the video "dot clock, and the normal
 memory clock. In the dot clock domain we simply scan through a
 rectangle defined my M4 x M8 which correspond to a full frame. The
 inner rectangle defined by M1 x M5 is the visible portion. Colomn
 [M2,M3] corresponds to the horizontal sync signal, and the rows
 [M6,M7] corresponds to the vertical sync signal. While in the visible
 portion we display the pixels get pull out of the (async) pixel FIFO,
 which is filled by a simple DMA engine in the memory domain. The
 vertical sync signal is used to restart the pixel fetching from
 begining and clear the FIFO.

 The parameters M1, .., M8 comes straight from the X11
 modelines. Because we use down counters, the initial values is
 slightly offset.

 The dot clock and the modeline parameters are provide by the client
 of this module.


 Example mode lines.
 # 640x480 @ ? Hz
 Modeline "640x480" 25.175  640 664 760 800  480 491 493 525
 # 1024x768 @ 60Hz (VESA) hsync: 48.4kHz
 ModeLine "1024x768"   65.0   1024 1048 1184 1344  768  771  777  806 -hsync -vsync
 # 1152x864 @ 60Hz hsync: 53.7kHz
 Modeline "1152x864"  81.642  1152 1216 1336 1520  864  865  868  895 +hsync +vsync
 # 1280x1024 @ 60Hz (VESA) hsync: 64.0kHz
 ModeLine "1280x1024" 108.0   1280 1328 1440 1688 1024 1025 1028 1066 +hsync +vsync

*/

module video
   // memory clock domain
   (input              memory_clock
   ,input              fb_waitrequest
   ,input [31:0]       fb_readdata
   ,input              fb_readdatavalid
   ,output reg [29:0]  fb_address
   ,output reg         fb_read = 0
   ,output reg [31:0]  vsynccnt = 0

   // video clock domain
   ,input              video_clock
   ,output             oVGA_CLOCK
   ,output reg  [ 9:0] oVGA_R = 0
   ,output reg  [ 9:0] oVGA_B = 0
   ,output reg  [ 9:0] oVGA_G = 0
   ,output reg         oVGA_BLANK_N = 0
   ,output reg         oVGA_HS = 0
   ,output reg         oVGA_VS = 0
   ,output             oVGA_SYNC_N
   );

   parameter      FB_BEGIN = 1024*1024/4;
   parameter      FB_MASK  = ~0;

   parameter      M1 = 12'd1280;
   parameter      M2 = 12'd1328;
   parameter      M3 = 12'd1440;
   parameter      M4 = 12'd1688;

   parameter      M5 = 12'd1024;
   parameter      M6 = 12'd1025;
   parameter      M7 = 12'd1028;
   parameter      M8 = 12'd1066;
   parameter      HS_NEG = 1'd0;
   parameter      VS_NEG = 1'd0;


   // Make the counters big enough for any conceivable situation
   // 13:0 -> [-2**13; 2**13-1] == [-8192;8191]
   parameter      MSB = 13;

   wire [MSB:0]   video_x0_init = M1-10'd1;
   wire [MSB:0]   video_x1_init = M2-10'd1;
   wire [MSB:0]   video_x2_init = M3-10'd1;
   wire [MSB:0]   video_x3_init = M4-10'd2; // Yes, -2

   wire [MSB:0]   video_y0_init = M5-10'd1;
   wire [MSB:0]   video_y1_init = M6-10'd1;
   wire [MSB:0]   video_y2_init = M7-10'd1;
   wire [MSB:0]   video_y3_init = M8-10'd2; // Yes, -2

   reg [MSB:0]    video_x0, video_x1, video_x2, video_x3;
   reg [MSB:0]    video_y0, video_y1, video_y2, video_y3;
   reg            video_vsync;

   assign         oVGA_CLOCK = video_clock;
   assign         oVGA_SYNC_N = 1;

   reg            video_fifo_read = 0;
   wire [7:0]     video_fifo_read_data, video_fifo_read_data_really;
   wire           video_fifo_empty;
   reg            vsync_ = 1'd0;

   wire           fifo_write      = fb_readdatavalid;
   wire    [31:0] fifo_write_data = fb_readdata;
   wire           fifo_full;
   wire     [6:0] fifo_used;
   reg            vsync = 1'd0, vsync_next = 1'd0;

   always @(posedge memory_clock) vsync_next <= video_vsync; // Clock domain crossing!
   always @(posedge memory_clock) vsync      <= vsync_next;
   always @(posedge memory_clock) vsync_     <= vsync;

   video_fifo video_fifo_inst
      // Memory domain
      (.wrclk   (memory_clock)
      ,.wrreq  (fifo_write)
      // Argh, Quartus FIFO assumes little endian
      ,.data   ({fifo_write_data[7:0],fifo_write_data[15:8],fifo_write_data[23:16],fifo_write_data[31:24]})
      ,.aclr   (vsync)
      ,.wrfull (fifo_full)
      ,.wrusedw(fifo_used)

      // Video domain
      ,.rdclk  (video_clock)
      ,.rdreq  (video_fifo_read)
      ,.q      (video_fifo_read_data)
      ,.rdempty(video_fifo_empty)
      );


   // Pixel DMA - memory domain

   // Pixel pump (really: very simply master that issues reads to the frame buffer)
   reg [29:0] next = 1'd0;
   always @(posedge memory_clock)
      if (!fb_waitrequest) begin
         fb_read <= 0;
         if (vsync & !vsync_) begin
            vsynccnt <= vsynccnt + 1;
            next <= FB_BEGIN;
         end else if (!vsync & !fifo_full & !fifo_used[5]) begin
            fb_read    <= 1;
            fb_address <= next;
            next       <= (next + 4) & FB_MASK; // useful for looping around
         end
      end


   // Video generation - dot clock domain

   always @(posedge video_clock) begin
      // Sadly we can only (barely) afford 8 bits pr pixel
      // Pack as RGB332 / RRRGGGBB (the eye is least sensive to blue apparently)
      // XXX spend the 7680 bytes and add a palette
      // v * (1023 / 7)
      case (video_fifo_read_data[7:5])
      0: oVGA_R <= 10'd0;
      1: oVGA_R <= 10'd146;
      2: oVGA_R <= 10'd292;
      3: oVGA_R <= 10'd438;
      4: oVGA_R <= 10'd585;
      5: oVGA_R <= 10'd732;
      6: oVGA_R <= 10'd877;
      7: oVGA_R <= 10'd1023;
      endcase
      case (video_fifo_read_data[4:2])
      0: oVGA_G <= 10'd0;
      1: oVGA_G <= 10'd146;
      2: oVGA_G <= 10'd292;
      3: oVGA_G <= 10'd438;
      4: oVGA_G <= 10'd585;
      5: oVGA_G <= 10'd732;
      6: oVGA_G <= 10'd877;
      7: oVGA_G <= 10'd1023;
      endcase
      // v * (1023 / 3)
      case (video_fifo_read_data[1:0])
      0: oVGA_B <= 10'd0;
      1: oVGA_B <= 10'd341;
      2: oVGA_B <= 10'd682;
      3: oVGA_B <= 10'd1023;
      endcase

      // Hacks below to provide reference point
`ifdef HACK
      if (video_x0 == video_x0_init || video_x0 == 0 || video_y0 == video_y0_init || video_y0 == 0)
         {oVGA_R[9:2], oVGA_G[9:2], oVGA_B[9:2]} <= 24'hFFFFFF; // White frame
      else if (video_x0 == video_y0)
         {oVGA_R[9:2], oVGA_G[9:2], oVGA_B[9:2]} <= 24'h0000FF; // Blue lineppp
`endif

      if (video_fifo_empty)
         {oVGA_R,oVGA_G,oVGA_B} <= {10'h3FF,10'd0,10'd0}; // RED!

      oVGA_BLANK_N    <= ~(video_x0[MSB] | video_y0[MSB]);
      oVGA_HS         <= HS_NEG ^ video_x1[MSB] ^ video_x2[MSB];
      oVGA_VS         <= VS_NEG ^ video_y1[MSB] ^ video_y2[MSB];
      video_vsync     <= video_y1[MSB] ^ video_y2[MSB];
      video_fifo_read <= ~(video_x0[MSB] | video_y0[MSB]);

      if (!video_x3[MSB]) begin
         video_x0 <= video_x0 - 1'd1;
         video_x1 <= video_x1 - 1'd1;
         video_x2 <= video_x2 - 1'd1;
         video_x3 <= video_x3 - 1'd1;
      end else begin
         video_x0 <= video_x0_init;
         video_x1 <= video_x1_init;
         video_x2 <= video_x2_init;
         video_x3 <= video_x3_init;
         if (!video_y3[MSB]) begin
            video_y0 <= video_y0 - 1'd1;
            video_y1 <= video_y1 - 1'd1;
            video_y2 <= video_y2 - 1'd1;
            video_y3 <= video_y3 - 1'd1;
         end else begin
            video_y0 <= video_y0_init;
            video_y1 <= video_y1_init;
            video_y2 <= video_y2_init;
            video_y3 <= video_y3_init;
         end
      end
   end
endmodule

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

`timescale 1ns/10ps
module stage_I(input  wire        clk
              ,input  wire        stall
              ,input  wire        flush // != jump??

               // SRAM ports
              ,output wire `REQ   imem_req
              ,input  wire `RES   imem_res

               // Async inputs
              ,input  wire        jump             // Target is next PC.
              ,input  wire [31:0] jump_target

               // Outputs
              ,output reg         i_valid = 0      // 0 => ignore i_instr.
              ,output reg  [31:0] i_instr = 0      // The current instruction.
              ,output reg  [31:0] i_pc = 0         // The address of the instr.
              ,output reg  [31:0] i_npc            // The address of the instr + 4.
              ,output wire        i_hazzard
              );

   parameter debug = 0;

   /*
    * The I$ is divided into n sets of k lines of m bytes (m/4 32-bit words).
    * 16 KiB = 4 KiW.  Each set is 1024 W = 32 lines
    *
    * We split a physical address into
    *
    *   | uncachable | check | cache line index | byte index |
    *
    * for example
    *
    *   |      0     |   20  |        5         |     7      |
    *
    * The "uncachable" bits represent the fact that there's no point in wasting
    * precious tag bits to verify address that can never (resonable) occur.  If
    * for example we never expect to address more than 64 MiB (2^26) then we
    * can save 32-26=6 tag bits (and more importantly get a faster tag
    * compare).
    *
    * The cache line index bits + byte index = 12 < log2(cache size) = 14
    * reflects the fact the more than one cache line can map to the same
    * physical address (in this example 14-12=4 way set associative).
    *
    */
   parameter SET_INDEX_BITS  =  2;  /* Caches has four sets. */
   parameter LINE_INDEX_BITS =  5;  /* Each set has 32 lines. */
   parameter WORD_INDEX_BITS =  5;  /* Each line has 32 32-bit words (128 B). */

   /* Derived meassures. */

   /* Size in log2 bytes of a line. */
   parameter LINE_BITS       = WORD_INDEX_BITS + 2;             //  7
   /* Size in log2 bytes of a set. */
   parameter SET_BITS        = LINE_INDEX_BITS + LINE_BITS;     // 12
   /* Size in log2 bytes of the cache. */
   parameter CACHE_BITS      = SET_INDEX_BITS + SET_BITS;       // 14

   /*
    * Caching less than the full range turns out to be a problem due
    * to how initialization depends on it. (2007-08-23: I forgot, but
    * think the issue is that standard MIPS reset address would have
    * been outside the range. Probably could have been fixed with some
    * deliberate aliasing in the memory space.)
    */
   parameter CACHEABLE_BITS  = 32;                              // 4 GiB
   parameter TAG_BITS        = CACHEABLE_BITS - SET_BITS;       // 20

   (* ram_init_file = "../onnama/ep1c20/initmem.mif" *)
   reg [31:0]                 icache[(1 << (CACHE_BITS - 2)) - 1:0];

   (* ram_init_file = "../onnama/ep1c20/tags0.mif" *)
   reg [TAG_BITS-1:0]         tags0[(1 << LINE_INDEX_BITS)-1:0];

   (* ram_init_file = "../onnama/ep1c20/tags1.mif" *)
   reg [TAG_BITS-1:0]         tags1[(1 << LINE_INDEX_BITS)-1:0];

   (* ram_init_file = "../onnama/ep1c20/tags2.mif" *)
   reg [TAG_BITS-1:0]         tags2[(1 << LINE_INDEX_BITS)-1:0];

   (* ram_init_file = "../onnama/ep1c20/tags3.mif" *)
   reg [TAG_BITS-1:0]         tags3[(1 << LINE_INDEX_BITS)-1:0];

   // Stage 1 - generate address.
   reg  [31:0]                pc_1 = 0;

   // Stage 2 - look up tags.
   reg                        valid_2 = 0;
   reg  [31:0]                pc_2    = 0;
   reg  [TAG_BITS-1:0]        tag0    = 0, tag1 = 0, tag2 = 0, tag3 = 0;
   wire [LINE_INDEX_BITS-1:0] csi_1   = pc_1[SET_BITS-1:LINE_BITS];
   reg  [LINE_INDEX_BITS-1:0] csi_2   = 0;

   // Stage 3 - Look up instruction and check tags.
   wire [TAG_BITS-1:0]        chk_2   = pc_2[CACHEABLE_BITS-1 : SET_BITS]; // pc_2[31:12]
   wire [WORD_INDEX_BITS-1:0] wi_2    = pc_2[WORD_INDEX_BITS+1:2];  // pc_2[6:2] Word Index (5 bits, 32 words)
   wire [ 3:0]                hits_2  = {tag3 == chk_2, tag2 == chk_2, tag1 == chk_2, tag0 == chk_2};
   reg  [ 1:0]                set_2   = 0;
   always @* casex (hits_2)
               'b1000: set_2 = 3;
               'b0100: set_2 = 2;
               'b0010: set_2 = 1;
               'b0001: set_2 = 0;
               default: set_2 = 2'bxx;
             endcase

   assign i_hazzard = ~|hits_2 & chk_2 != 0;
   wire     stalled = stall | i_hazzard;

   // Cache filling stage machinery.
   parameter RUNNING   = 0;
   parameter FIRST_REQ = 1;
   parameter FILLING   = 2;
   parameter RESTART   = 3;

   reg [WORD_INDEX_BITS-1:0]  fill_wi      = 0;
   reg [LINE_INDEX_BITS-1:0]  fill_csi     = 0;
   reg [SET_INDEX_BITS-1 :0]  fill_set     = 0;
   reg [3:0]                  state        = 0;

   reg [31:0]                 fill_address = 0;
   reg                        fill_stb     = 0;
   reg                        fill_valid   = 0;

   assign imem_req`A   = fill_address;
   assign imem_req`R   = fill_stb;
   assign imem_req`W   = 0;
   assign imem_req`WD  = 0;
   assign imem_req`WBE = 0;
   wire        fill_wait = imem_res`HOLD;
   wire [31:0] fill_data = imem_res`RD;

   always @(posedge clk) begin
        if (debug)
          $display(
"%05da S%1d V%d PC %x %x %x HITS %x READ %d ADDR %x WAIT %x RD %x  jump %d:%x",
                   $time,
                   state,
                   i_valid,
                   pc_1, pc_2, i_pc, hits_2,
                   fill_stb, fill_address,
                   fill_wait, fill_data,
                   jump, jump_target);

      // Stage 1 -- generate the address.  Q: Can I cut this stage?
      if (jump | ~stalled)
        pc_1 <= jump ? jump_target : pc_1 + 4;

      // Stage 2 -- look up tags.
      if (jump) begin // Q: what about flush?
         valid_2 <= 0; // Kill the pipe
         pc_2    <= 0;
         csi_2   <= 0;
      end else if (~stalled) begin
         valid_2 <= 1;
         pc_2    <= pc_1;
         csi_2   <= csi_1;
         tag0    <= tags0[csi_1];
         tag1    <= tags1[csi_1];
         tag2    <= tags2[csi_1];
         tag3    <= tags3[csi_1];
      end

      // Stage 3 -- match tags and access I$
      if (flush) begin
         i_valid <= 0;
         i_pc    <= 0;
         i_npc   <= 0;
      end else if (~stalled) begin
         i_valid <= valid_2;
         /*
          * Note the careful use of x <= icache[..].  Using
          * icache[...] in a non-trivial expression usually limits
          * the synthesizers ability to inferring memory.
          */
         i_instr <= icache[{set_2,csi_2,wi_2}];
         i_pc    <= pc_2;
         i_npc   <= pc_1;
      end

      fill_valid <= fill_stb & ~fill_wait;

      case (state)
        RUNNING:
          if (i_hazzard) begin
`ifdef SIMULATE_MAIN
             if (debug)
               $display("%05da   MISSED the I$ at %x, (hits %x tags %x %x %x %x chk_2 %x) starting filling",
                        $time, pc_2, hits_2, tag3, tag2, tag1, tag0, chk_2);
`endif
             state        <= FILLING;
             fill_set     <= 2'b11 ^ fill_set[0]; // XXX: Need a better replacement algorithm.
                                                  // XXX: Hack to protect the preloaded cache.
             fill_wi      <= 0;
             fill_csi     <= pc_2[SET_BITS-1:LINE_BITS];
             fill_address <= pc_2[CACHEABLE_BITS-1:LINE_BITS] << LINE_BITS;
             fill_stb     <= 1;
          end
        FILLING: begin
           if (fill_valid) begin
`ifdef SIMULATE_MAIN
              if (debug) $display("%05da   GOT %x for I$[{%x,%x,%x}]",
                                  $time, fill_data,
                                  fill_set, fill_csi, fill_wi);
`endif
              icache[{fill_set,fill_csi,fill_wi}] <= fill_data;
              fill_wi <= fill_wi + 1'd1;

              if (&fill_wi) begin
`ifdef SIMULATE_MAIN
                 if (debug) $display("%05da   DONE with filling chk = %x", $time, chk_2);
`endif
                 state <= RESTART;
                 fill_stb <= 0;
              end
           end

           if (~fill_wait)
              fill_address <= fill_address + 4;
        end
        // It seems to speed up by introducing another step.
        RESTART: begin
                 state <= RUNNING;
                 case (fill_set)
                   0: tags0[fill_csi] <= chk_2;
                   1: tags1[fill_csi] <= chk_2;
                   2: tags2[fill_csi] <= chk_2;
                   3: tags3[fill_csi] <= chk_2;
                 endcase

                 // Q: Intuitively it would be better to restart the
                 // icache at stage 2 instead of forcing tagX, but
                 // so far my attempts have hurt rather than help
                 // performance.
                 case (fill_set)
                   0: tag0 <= chk_2;
                   1: tag1 <= chk_2;
                   2: tag2 <= chk_2;
                   3: tag3 <= chk_2;
                 endcase
        end
      endcase
   end

`ifdef SIMULATE_MAIN
   reg [31:0] i;

   initial begin
      #0
      $readmemh("program.data", icache);
      /* This preloads the cache to cover 'hBFC0_0000 ... 'hBFC0_3FFF. */
      for (i = 0; i < (1 << LINE_INDEX_BITS); i = i + 1) begin
         tags0[i] = ('hBFC0_0000 >> SET_BITS);
         tags1[i] = ('hBFC0_1000 >> SET_BITS);
         tags2[i] = ('hBFC0_2000 >> SET_BITS);
         tags3[i] = ('hBFC0_3000 >> SET_BITS);
      end
   end
`endif
endmodule

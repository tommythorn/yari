// -----------------------------------------------------------------------
//
//   Copyright 2004,2007,2008 Tommy Thorn - All Rights Reserved
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
//   Bostom MA 02111-1307, USA; either version 2 of the License, or
//   (at your option) any later version; incorporated herein by reference.
//
// -----------------------------------------------------------------------

// TODO: For extra credits, make the stage start fetching from the
// missed instruction and emit instructions as soon as they arrive,
// rather than waiting for the whole line to be filled. Watch out for
// issues as the filling line wraps around and watch out for misses
// that happens while already filling. The value of this goes down for
// shorter cache lines and longer memory latencies.


/*
 * The instruction fetch/cache stage generates a sequential stream of
 * instructions until kill is asserted, leaving the pipe without valid
 * instructions until the next restart. Restart implies kill.
 *
 * When a pc misses in the cache, the stage emits invalid instructions
 * until the missed line is filled. While this happens, both kill and
 * restart are respected (causing the missed instruction not to be
 * emitted).
 */

`timescale 1ns/10ps

module stage_I(input  wire        clock
              ,input  wire        kill             // Empty the pipeline
                                                   // until next restart
              ,input  wire        restart          // Target is next PC.
              ,input  wire [31:0] restart_pc

              ,input  wire        synci
              ,input  wire [31:0] synci_a

               // Memory access
              ,input              imem_waitrequest
              ,output reg  [29:0] imem_address
              ,output reg         imem_read = 0
              ,input       [31:0] imem_readdata
              ,input              imem_readdatavalid

               // Outputs
              ,output wire        i1_valid
              ,output wire [31:0] i1_pc
              ,output wire        i2_valid
              ,output wire [31:0] i2_pc

              ,output reg         i_valid = 0      // 0 => ignore i_instr.
              ,output wire [31:0] i_instr
              ,output reg  [31:0] i_pc = 0         // The address of the instr.
              ,output wire [31:0] i_npc            // The next instruction
              ,output reg  [31:0] perf_icache_misses = 0
              );

   parameter debug = 0;

`include "config.h"

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
   /* Derived meassures. */

   /* Size in log2 bytes of a line. */
   parameter LINE_BITS       = IC_WORD_INDEX_BITS + 2;             //  7
   /* Size in log2 bytes of a set. */
   parameter SET_BITS        = IC_LINE_INDEX_BITS + LINE_BITS;     // 12
   /* Size in log2 bytes of the cache. */
   parameter CACHE_BITS      = IC_SET_INDEX_BITS + SET_BITS;       // 14

   /*
    * Restricting the physical address space would enable us to cache
    * less than the full range and thus could save tag memory. However
    * that would require much care as the standard MIPS reset address
    * must be covered as well. It probably could be done with some
    * deliberate aliasing in the memory space, but it is deemed not
    * worth the effort and complication.
    */
   parameter TAG_BITS        = CACHEABLE_BITS - SET_BITS;       // 20

   // Divide instruction addresses into segments
   `define CHK [CACHEABLE_BITS-1 :SET_BITS]
   `define CSI [SET_BITS-1       :LINE_BITS]
   `define WDX [IC_WORD_INDEX_BITS+1:2]

   // Stage 1 - generate address.
   reg                        valid_1 = 0;
   reg  [31:0]                pc_1    = 0;

   // Stage 2 - look up tags.
   reg                        valid_2 = 0;
   reg  [31:0]                pc_2    = 0;
   wire [TAG_BITS-1:0]        tag0, tag1, tag2, tag3;
   wire [(1 << IC_SET_INDEX_BITS)-1:0]
                              hits_2 = {tag3 == pc_2`CHK, tag2 == pc_2`CHK,
                                        tag1 == pc_2`CHK, tag0 == pc_2`CHK};

   // Cache filling stage machinery.
   parameter S_RUNNING     = 0;
   parameter S_FILLING     = 1;
   parameter S_PRE_RUNNING = 2;
   parameter S_LOOKUP      = 3;
   parameter S_INVALIDATE  = 4;

   reg [IC_SET_INDEX_BITS-1:0]  fill_set       = 2; // !! XXX
   reg [31:0]                   state          = S_RUNNING;
   reg [IC_WORD_INDEX_BITS-1:0] fill_wi;

   assign i_npc   = pc_2;

   // set_2 is constructed such that it will be fill_set during
   // filling, and the matching tag when there is a hit (which implies
   // that tag update must be done no sooner than the last word
   // written to the cache line)
   reg  [IC_SET_INDEX_BITS-1:0]  set_2;
   always @* casex (hits_2)
             'b0001: set_2 = 0;
             'b0010: set_2 = 1;
             'b0100: set_2 = 2;
             'b1000: set_2 = 3;
             default:set_2 = 2'bxx;
             endcase

   reg [IC_LINE_INDEX_BITS-1:0] tag_wraddress;
   reg [TAG_BITS-1:0]        tag_write_data;
   reg [3:0]                 tag_write_ena;


   simpledpram #(32,CACHE_BITS - 2,"../../initmem")
      icache_ram(.clock(clock),
                 .rdaddress({set_2,pc_2`CSI,pc_2`WDX}), .rddata(i_instr),
                 .wraddress({fill_set | 2'd2,pc_2`CSI,fill_wi}),
                 .wrdata(imem_readdata),
                 .wren(state == S_FILLING && imem_readdatavalid));

   simpledpram #(TAG_BITS,IC_LINE_INDEX_BITS,"tag0")
      tag0_ram(.clock(clock), .rdaddress(pc_1`CSI), .rddata(tag0),
               .wraddress(tag_wraddress), .wrdata(tag_write_data),
               .wren(tag_write_ena[0]));

   simpledpram #(TAG_BITS,IC_LINE_INDEX_BITS,"tag1")
      tag1_ram(.clock(clock), .rdaddress(pc_1`CSI), .rddata(tag1),
               .wraddress(tag_wraddress), .wrdata(tag_write_data),
               .wren(tag_write_ena[1]));

   simpledpram #(TAG_BITS,IC_LINE_INDEX_BITS,"tag2")
      tag2_ram(.clock(clock), .rdaddress(pc_1`CSI), .rddata(tag2),
               .wraddress(tag_wraddress), .wrdata(tag_write_data),
               .wren(tag_write_ena[2]));

   simpledpram #(TAG_BITS,IC_LINE_INDEX_BITS,"tag3")
      tag3_ram(.clock(clock), .rdaddress(pc_1`CSI), .rddata(tag3),
               .wraddress(tag_wraddress), .wrdata(tag_write_data),
               .wren(tag_write_ena[3]));

   assign       i1_valid = valid_1;
   assign       i1_pc    = pc_1;
   assign       i2_valid = valid_2;
   assign       i2_pc    = pc_2;

   reg          pending_synci      = 0;
   reg [31:0]   pending_synci_a    = 0;
   reg [31:0]   pending_synci_pc   = 0;

   always @(posedge clock) begin
      tag_write_ena <= 0;
      if (synci) begin
         pending_synci      <= 1;
         pending_synci_a    <= synci_a;
         pending_synci_pc   <= restart_pc; // restart is coincident with synci
      end

      if (~imem_waitrequest & imem_read) begin
         $display("%05d  I$ done issueing", $time);
         imem_read <= 0;
      end

      case (state)
      S_RUNNING:
         if (synci | pending_synci) begin
            $display("%05d  I$ flushing line @ %x (index %d)", $time,
                     synci ? synci_a : pending_synci_a,
                     synci ? synci_a`CSI : pending_synci_a`CSI
                     );
            pc_1         <= synci ? synci_a : pending_synci_a;
            i_valid      <= 0;
            valid_1      <= 0;
            valid_2      <= 0;

            state        <= S_LOOKUP;
         end else if (|hits_2 | ~valid_2 | restart) begin
            /*
             * This is the normal flow (we don't care about invalid misses)
             * Advance the pc; look up tags, word index, find hitting
             * set; look up cache word.
             */
            pc_1      <= pc_1 + 4;
            valid_2   <= valid_1;
            pc_2      <= pc_1;
            i_valid   <= valid_2;
            i_pc      <= pc_2;

// XXX just for debugging
//$display(
//"%05d  I$ access %x hit set %d (hits: %x), index %d tags %x %x %x %x",
//$time, pc_2, set_2, hits_2, pc_2`CSI, tag0, tag1, tag2, tag3);
         end else begin
            // We missed in the cache, start the filling machine
            $display("%05d  I$ %8x missed, starting to fill", $time, pc_2);
            perf_icache_misses <= perf_icache_misses + 1;
            pc_1         <= pc_2;
            i_valid      <= 0;
            valid_2      <= 0;

            fill_wi      <= 0;
            imem_address <= {pc_2[CACHEABLE_BITS-1:LINE_BITS],
                             {(LINE_BITS - 2){1'd0}}};
            imem_read    <= 1;
            $display("%05d  I$ begin fetching from %8x", $time,
                     {pc_2[CACHEABLE_BITS-1:LINE_BITS],{(LINE_BITS){1'd0}}});

            state        <= S_FILLING;
         end

      S_LOOKUP: begin
         pc_2 <= pc_1;
         state <= S_INVALIDATE;
      end

      S_INVALIDATE: begin
         if (|hits_2) begin
            $display("%05d  I$ flushing %x (= %x TAG) found a stale line from set %d (hits %x), index %d tags %x %x %x %x",
                     $time,
                     pc_1, pc_1`CHK, set_2, hits_2, pc_1`CSI,
                     tag0, tag1, tag2, tag3);
         end else
            $display("%05d  I$ flushing %x (= %x TAG) found nothing (hits %x), index %d tags %x %x %x %x",
                     $time,
                     pc_1, pc_1`CHK, hits_2, pc_1`CSI,
                     tag0, tag1, tag2, tag3);

         tag_wraddress  <= pending_synci_a`CSI;
         tag_write_data <= ~0;
         tag_write_ena  <= hits_2;
         // XXX We must wait for SB to drain!  It happens to work as
         // is right now as the SB gets priority but that's actually a
         // bug.
         pc_1 <= pending_synci_pc;
         valid_1 <= 1;
         pending_synci <= 0;
         state <= S_PRE_RUNNING; // To give a cycle for the tags to be written
      end

      S_FILLING:
         if (imem_readdatavalid) begin
            $display("%05d  I$ {%1d,%1d,%1d} <- %8x", $time,
                     fill_set, pc_2`CSI, fill_wi, imem_readdata);

            fill_wi <= fill_wi + 1'd1;

            if (&fill_wi) begin
               $display("%05d  IF tag%d[%d] <- %x", $time,
                        fill_set, pc_2`CSI, pc_2`CHK);
               $display("%05d  IF cache filled, back to running", $time);

               tag_wraddress  <= pc_2`CSI;
               tag_write_data <= pc_2`CHK;
               tag_write_ena  <= 1 << (fill_set | 2'd2);

               // Restrict writes to set 2 and 3 to protect the
               // preloaded image. XXX This should be under sw control
               // so we can switch it off ROM.
               // XXX: Need a better replacement algorithm.
               fill_set <= {1'd1, !fill_set[0]};
               state <= S_PRE_RUNNING;
            end
         end

      S_PRE_RUNNING:
         // This lame pause is to keep the tags interface simple (for now)
         state <= S_RUNNING;
      endcase

      // Keep the restart & kill handling down here to take priority
      if (!synci & !pending_synci)
         if (restart) begin
            valid_1 <= 1;
            valid_2 <= 0;
            i_valid <= 0;
            pc_1    <= restart_pc;
            $display("%05d  IF restarted at %8x", $time, restart_pc);
         end else if (kill) begin
            valid_1 <= 0;
            valid_2 <= 0;
            i_valid <= 0;
         end

      // Keep all debugging output down here to keep the logic readable
      if (debug) begin
         if (state == S_RUNNING)
            $display(
"%05d  $I running: PC %x (valid %d) <%x;%x;%x> -- PC %x (valid %d) HITS %x -- PC %x INST %x VALID %d",
                  $time,
                  pc_1, valid_1, set_2, pc_1`CSI, pc_2`WDX,
                  pc_2, valid_2, hits_2,
                  i_pc, i_instr, i_valid);
      end
   end
endmodule

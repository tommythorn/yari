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

/*
 IF
    Calculates the fetch address
    Looks up tags and instructions and does a late select, leaving
    the result in {i_valid,i_instr}
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
              ,output reg         i1_valid = 0     // For debugging only
              ,output wire [31:0] i1_pc            // == i_npc

              ,output reg         i2_valid = 0
              ,output wire [31:0] i2_pc            // == i_pc

              ,output reg         i_valid = 0      // 0 => ignore i_instr.
              ,output reg  [31:0] i_pc = 0         // The address of the instr.
              ,output reg  [31:0] i_npc = 0        // The next instruction
              ,output reg  [31:0] i_instr

              ,output reg  [31:0] perf_icache_misses = 0
              );

   parameter debug = 1;

`include "config.h"

   /*
    * The I$ is divided into n sets of k lines of m bytes (m/4 32-bit words).
    * 16 KiB = 4 KiW.  Each set is 1024 W = 32 lines
    *
    * We split a physical address into
    *
    *   | check | cache line index | byte index |
    *
    * for example
    *
    *   |   20  |        5         |     7      |
    *
    * The cache line index bits + byte index = 12 < log2(cache size) = 14
    * reflects the fact the more than one cache line can map to the same
    * physical address (in this example 14-12=4 way set associative).
    *
    */
   /* Derived meassures. */

   /* Size in log2 bytes of a line. */
   parameter LINE_BITS       = IC_WORD_INDEX_BITS + 2;             //  4
   /* Size in log2 bytes of a set. */
   parameter SET_BITS        = IC_LINE_INDEX_BITS + LINE_BITS;     // 11
   /* Size in log2 bytes of the cache. */
   parameter CACHE_BITS      = IC_SET_INDEX_BITS + SET_BITS;       // 14
   parameter TAG_BITS        = CACHEABLE_BITS - SET_BITS;          // 20

   // Divide instruction addresses into segments
   `define CHK [CACHEABLE_BITS-1 :SET_BITS]
   `define CSI [SET_BITS-1       :LINE_BITS]
   `define WDX [IC_WORD_INDEX_BITS+1:2]

   // Stage 1 - generate address.
   wire [31:0]                fetchaddress = restart ? restart_pc : i_npc;
   assign                     i1_pc  = fetchaddress;

   // Stage 1 - look up tags and instructions.
   assign                     i2_pc  = i_pc;
   wire [TAG_BITS-1:0]        tag0, tag1, tag2, tag3;
   wire [31:0]                ic_q0, ic_q1, ic_q2, ic_q3;
   wire [(1 << IC_SET_INDEX_BITS)-1:0]
                              hits_2 = {tag3 == i_pc`CHK, tag2 == i_pc`CHK,
                                        tag1 == i_pc`CHK, tag0 == i_pc`CHK};

   // Cache filling stage machinery.
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

   // XXX Is it better to directly use hits_2?
   always @* case (set_2)
               0: i_instr = ic_q0;
               1: i_instr = ic_q1;
               2: i_instr = ic_q2;
               3: i_instr = ic_q3;
               endcase

   always @* casex (hits_2)
             'b0001: i_valid = i2_valid & ~restart;
             'b0010: i_valid = i2_valid & ~restart;
             'b0100: i_valid = i2_valid & ~restart;
             'b1000: i_valid = i2_valid & ~restart;
             default:i_valid = 0;
             endcase

   parameter S_RUNNING     = 0;
   parameter S_FILLING     = 1;
   parameter S_PRE_RUNNING = 2;
   parameter S_LOOKUP      = 3;
   parameter S_INVALIDATE  = 4;

   reg [IC_SET_INDEX_BITS-1:0]  fill_set       = 0;
   reg [31:0]                   state          = S_RUNNING;
   reg [IC_WORD_INDEX_BITS-1:0] fill_wi;


   reg [IC_LINE_INDEX_BITS-1:0] tag_wraddress;
   reg [TAG_BITS-1:0]        tag_write_data;
   reg [3:0]                 tag_write_ena = 0;


   simpledpram #(TAG_BITS,IC_LINE_INDEX_BITS,"tag0")
      tag0_ram(.clock(clock), .rdaddress(fetchaddress`CSI), .rddata(tag0),
               .wraddress(tag_wraddress), .wrdata(tag_write_data),
               .wren(tag_write_ena[0]));

   simpledpram #(TAG_BITS,IC_LINE_INDEX_BITS,"tag1")
      tag1_ram(.clock(clock), .rdaddress(fetchaddress`CSI), .rddata(tag1),
               .wraddress(tag_wraddress), .wrdata(tag_write_data),
               .wren(tag_write_ena[1]));

   simpledpram #(TAG_BITS,IC_LINE_INDEX_BITS,"tag2")
      tag2_ram(.clock(clock), .rdaddress(fetchaddress`CSI), .rddata(tag2),
               .wraddress(tag_wraddress), .wrdata(tag_write_data),
               .wren(tag_write_ena[2]));

   simpledpram #(TAG_BITS,IC_LINE_INDEX_BITS,"tag3")
      tag3_ram(.clock(clock), .rdaddress(fetchaddress`CSI), .rddata(tag3),
               .wraddress(tag_wraddress), .wrdata(tag_write_data),
               .wren(tag_write_ena[3]));

   simpledpram #(32,CACHE_BITS - 4,"../../initmem")
      icache_ram0(.clock(clock),
                  .rdaddress({fetchaddress`CSI,fetchaddress`WDX}), .rddata(ic_q0),
                  .wraddress({i_pc`CSI,fill_wi}),
                  .wrdata(imem_readdata),
                  .wren(fill_set == 0 && state == S_FILLING && imem_readdatavalid));

   simpledpram #(32,CACHE_BITS - 4,"../../initmem1")
      icache_ram1(.clock(clock),
                  .rdaddress({fetchaddress`CSI,fetchaddress`WDX}), .rddata(ic_q1),
                  .wraddress({i_pc`CSI,fill_wi}),
                  .wrdata(imem_readdata),
                  .wren(fill_set == 1 && state == S_FILLING && imem_readdatavalid));

   simpledpram #(32,CACHE_BITS - 4,"../../initmem2")
      icache_ram2(.clock(clock),
                  .rdaddress({fetchaddress`CSI,fetchaddress`WDX}), .rddata(ic_q2),
                  .wraddress({i_pc`CSI,fill_wi}),
                  .wrdata(imem_readdata),
                  .wren(fill_set == 2 && state == S_FILLING && imem_readdatavalid));

   simpledpram #(32,CACHE_BITS - 4,"../../initmem3")
      icache_ram3(.clock(clock),
                  .rdaddress({fetchaddress`CSI,fetchaddress`WDX}), .rddata(ic_q3),
                  .wraddress({i_pc`CSI,fill_wi}),
                  .wrdata(imem_readdata),
                  .wren(fill_set == 3 && state == S_FILLING && imem_readdatavalid));

   reg [32:0]  lfsr = 0;

   reg          pending_synci      = 0;
   reg [31:0]   pending_synci_a    = 0;
   reg [31:0]   pending_synci_pc   = 0;

   always @(posedge clock) begin
      lfsr <= {lfsr[31:0], ~lfsr[32] ^ lfsr[19]};
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
                     synci ? synci_a     : pending_synci_a,
                     synci ? synci_a`CSI : pending_synci_a`CSI);
            i_npc        <= synci ? synci_a : pending_synci_a;
            i1_valid     <= 0;
            i2_valid     <= 0;

            state        <= S_LOOKUP;
         end else if (|hits_2 | ~i2_valid | restart) begin
            if (debug)
               $display("%05d  I$ business as usual i_npc = %x", $time, i_npc);

            /*
             * This is the normal flow (we don't care about invalid misses)
             * Advance the pc; look up tags, word index, find hitting
             * set; look up cache word.
             */
            i_pc         <= fetchaddress;
            i_npc        <= fetchaddress + 4;
            i2_valid     <= i1_valid | restart;
            if (restart)
               i1_valid  <= 1;

            if (debug & restart)
               $display("%05d  I$ DEBUG1 restart from %x", $time, restart_pc);
         end else begin
            // We missed in the cache, start the filling machine
            $display("%05d  I$ %8x missed, starting to fill", $time, i_pc);
            perf_icache_misses <= perf_icache_misses + 1;
            i_npc        <= restart ? restart_pc : i_pc;
            i2_valid     <= 0;

            fill_wi      <= 0;
            imem_address <= {i_pc[CACHEABLE_BITS-1:LINE_BITS],
                             {(LINE_BITS - 2){1'd0}}};
            imem_read    <= 1;
            $display("%05d  I$ begin fetching from %8x", $time,
                     {i_pc[CACHEABLE_BITS-1:LINE_BITS],{(LINE_BITS){1'd0}}});

            if (debug & restart)
               $display("%05d  I$ DEBUG2 restart from %x", $time, restart_pc);
            state        <= S_FILLING;
         end

      S_LOOKUP: begin
         i_pc <= i_npc;
         state <= S_INVALIDATE;
      end

      S_INVALIDATE: begin
         if (|hits_2) begin
            $display("%05d  I$ flushing %x (= %x TAG) found a stale line from set %d (hits %x), index %d tags %x %x %x %x",
                     $time,
                     fetchaddress, fetchaddress`CHK, set_2, hits_2, fetchaddress`CSI,
                     tag0, tag1, tag2, tag3);
         end else
            $display("%05d  I$ flushing %x (= %x TAG) found nothing (hits %x), index %d tags %x %x %x %x",
                     $time,
                     fetchaddress, fetchaddress`CHK, hits_2, fetchaddress`CSI,
                     tag0, tag1, tag2, tag3);

         tag_wraddress  <= pending_synci_a`CSI;
         tag_write_data <= ~0;
         tag_write_ena  <= hits_2;
         // XXX We must wait for SB to drain!  It happens to work as
         // is right now as the SB gets priority but that's actually a
         // bug.
         i_npc          <= pending_synci_pc;
         i1_valid       <= 1;
         pending_synci  <= 0;
         state          <= S_PRE_RUNNING; // To give a cycle for the tags to be written
      end

      S_FILLING: begin
         if (restart) begin
            if (debug)
               $display("%05d  I$ DEBUG3 restart from %x", $time, restart_pc);
            i_npc <= restart_pc;
         end

         if (imem_readdatavalid) begin
            $display("%05d  I$ {%1d,%1d,%1d} <- %8x", $time,
                     fill_set, i_pc`CSI, fill_wi, imem_readdata);

            fill_wi <= fill_wi + 1'd1;

            if (&fill_wi) begin
               $display("%05d  IF tag%d[%d] <- %x", $time,
                        fill_set, i_pc`CSI, i_pc`CHK);
               $display("%05d  IF cache filled, back to running", $time);

               tag_wraddress  <= i_pc`CSI;
               tag_write_data <= i_pc`CHK;
               tag_write_ena  <= 4'd1 << fill_set;
               fill_set       <= lfsr[1:0];

               state          <= S_PRE_RUNNING;
            end
         end else
            if (debug)
               $display("%05d  I$ waiting for memory", $time);
      end

      S_PRE_RUNNING: begin
         // This lame pause is to keep the tags interface simple (for now)
         if (restart) begin
            if (debug)
               $display("%05d  I$ DEBUG3 restart from %x", $time, restart_pc);
            i_npc <= restart_pc;
         end

         state <= S_RUNNING;
      end
      endcase

      // Keep the kill handling down here to take priority
      if (!synci & !pending_synci & kill & ~restart) begin
         if (debug)
               $display("%05d  I$ killed", $time);
         i1_valid <= 0;
         i2_valid <= 0;
      end

      // Keep all debugging output down here to keep the logic readable
      if (debug) begin
         if (state == S_RUNNING)
            $display(
"%05d  I$ running: PC %x (valid %d) <%x;%x;%x> -- PC %x (valid %d) HITS %x -- PC %x INST %x VALID %d",
                  $time,
                  fetchaddress, i1_valid, set_2, fetchaddress`CSI, i_pc`WDX,
                  i_pc, i_valid, hits_2,
                  i_pc, i_instr, i_valid);
      end
   end
endmodule

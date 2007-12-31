// -----------------------------------------------------------------------
//
//   Copyright 2004,2007 Tommy Thorn - All Rights Reserved
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
//   Bostom MA 02111-1307, USA; either version 2 of the License, or
//   (at your option) any later version; incorporated herein by reference.
//
// -----------------------------------------------------------------------
//
// Memory stage -
//
//   This stage traditionally sits after EX (which computes the address),
//   but adders in FPGA are cheap and thus address computation
//   trivial.
//
//   Instead we attach right after DE and performs the tag lookup in
//   parallel with EX and the actual cache access in ME.
//

`timescale 1ns/10ps
`include "../soclib/pipeconnect.h"

module stage_M(input  wire        clock

              ,input  wire [31:0] d_op1_val
              ,input  wire [31:0] d_simm

              ,input  wire        x_valid
              ,input  wire [31:0] x_instr // XXX for debugging only
              ,input  wire        x_is_delay_slot
              ,input  wire [31:0] x_pc
              ,input  wire [ 5:0] x_opcode
              ,input  wire [31:0] x_op1_val
              ,input  wire [31:0] x_rt_val // for stores only
              ,input  wire [ 5:0] x_wbr
              ,input  wire [31:0] x_res

              // Connection to main memory
              ,output reg  `REQ   dmem_req = 0
              ,input  wire `RES   dmem_res

              // Connection to peripherals
              ,output reg  `REQ   peripherals_req = 0
              ,input  wire `RES   peripherals_res

              // Stage output
              ,output reg         m_valid = 0
              ,output reg  [31:0] m_instr = 0
              ,output reg  [31:0] m_pc    = 0 // XXX for debugging only
              ,output reg  [ 5:0] m_wbr   = 0
              ,output reg  [31:0] m_res   = 0

              ,output reg         m_kill  = 0 // flush the pipeline up to and including ME
              ,output reg         m_restart = 0
              ,output reg  [31:0] m_restart_pc
              );
   parameter debug = 1;
`ifdef SIMULATE_MAIN
   pipechecker check("stage_M", clock, dmem_req, dmem_res);
   pipechecker check2("stage_M", clock, peripherals_req, peripherals_res);
`endif

`include "config.h"

   /*
    * The cache is divided into n sets of k lines of m bytes (m/4
    * 32-bit words).
    *
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
    * The "uncachable" bits represent the fact that there's no point in
    * wasting precious tag bits to verify address that can never
    * (resonable) occur.  If for example we never expect to address more
    * than 64 MiB (2^26) then we can save 32-26=6 tag bits (and more
    * importantly get a faster tag compare). However, doing this obviously
    * creates aliases so if we use uncachable bits we must only detect
    * hits on on one of the aliases. Another way to say this, is that
    * uncachable corresponds to a constant part of the tags.
    *
    * The cache line index bits + byte index = 12 < log2(cache size) = 14
    * reflects the fact the more than one cache line can map to the same
    * physical address (in this example 14-12=4 way set associative).
    *
    * We do *not* have a present/non-present bit, as 0xFF...... as
    * uncachable so we use tags in that range to represent invalid.
    * XXX Unfortunately, uncachable loads aren't implemented yet.
    */

   /* Derived meassures. */

   /* Size in log2 bytes of a line. */
   parameter LINE_BITS       = DC_WORD_INDEX_BITS + 2;
   /* Size in log2 bytes of a set. */
   parameter SET_BITS        = DC_LINE_INDEX_BITS + LINE_BITS;
   /* Size in log2 bytes of the cache. */
   parameter CACHE_BITS      = DC_SET_INDEX_BITS + SET_BITS;

   // Caching less than the full range would require a bit more work
   parameter TAG_BITS        = CACHEABLE_BITS - SET_BITS;


   /*
    * Store buffer - we just need it to be big enough to subsume a
    * store burst. Any load miss will block until the store buffer
    * has drained (we don't attempt any fancy forwarding from the
    * store buffer).
    */
   parameter STORE_BUFFER_BITS = 3; // XXX Move to makeconfig.sh

   // Divide instruction addresses into segments
   `define CHK [CACHEABLE_BITS-1:SET_BITS]
   `define CSI [SET_BITS-1:LINE_BITS]
   `define WDX [DC_WORD_INDEX_BITS+1:2]

   // Inputs to stage EX - split the address into Cache Set Index, Word Index and Check
   wire [31:0]                   d_address = d_op1_val + d_simm;
   wire [TAG_BITS-1:0]           d_chk; // Tag check
   wire [DC_LINE_INDEX_BITS-1:0] d_csi; // Cache Set Index (which cache in the set)
   wire [DC_WORD_INDEX_BITS-1:0] d_wi; // Word Index (which word in the cache line)
   assign {d_chk,d_csi,d_wi} = d_address[31:2];

   // Inputs to stage ME - check tags and access cache (this is the critical path)
   reg  [31:0]                   x_address = 0;
   reg  [TAG_BITS-1:0]           x_chk     = 0; // Tag check
   reg  [DC_LINE_INDEX_BITS-1:0] x_csi     = 0; // Cache Set Index (which cache in the set)
   reg  [DC_WORD_INDEX_BITS-1:0] x_wi      = 0; // Word Index (which word in the cache line)

   always @(posedge clock) begin
      x_address <= d_address;
      {x_chk,x_csi,x_wi} <= d_address[31:2];
   end

   wire [TAG_BITS-1:0]        x_tag0, x_tag1, x_tag2, x_tag3;
   wire [(1 << DC_SET_INDEX_BITS)-1:0]
                              x_hits = {x_tag3 == x_chk,
                                        x_tag2 == x_chk,
                                        x_tag1 == x_chk,
                                        x_tag0 == x_chk};

   wire                       x_miss = x_hits == 0;
   reg  [DC_SET_INDEX_BITS-1:0]  x_set;
   always @* casex (x_hits)
               'b1000: x_set = 3;
               'b0100: x_set = 2;
               'b0010: x_set = 1;
               'b0001: x_set = 0;
               default:x_set = 'hX;
             endcase

   // Shift and replicate the stored data
   reg [31:0] x_store_data;
   always @*
      case (x_opcode[1:0])
      0:       x_store_data = {x_rt_val[7:0], x_rt_val[7:0], x_rt_val[7:0], x_rt_val[7:0]};
      1:       x_store_data = {x_rt_val[15:0], x_rt_val[15:0]};
      default: x_store_data = x_rt_val;
      endcase
   // Generated the byte enables (Big Endian!)
   // XXX Trap on unaligned access!!
   reg [3:0] x_byteena;
   always @*
      case (x_opcode[1:0])
      0:       x_byteena = 4'h8 >> x_address[1:0];
      1:       x_byteena = {~x_address[1],~x_address[1],x_address[1],x_address[1]};
      default: x_byteena = 4'hF;
      endcase
   wire x_load  = x_valid & x_opcode[5:3] == 4;
   wire x_store = x_valid & x_opcode[5:3] == 5;


   // Inputs to stage WB - access cache
   reg  [31:0]                m_address    = 0;
   reg                        m_load       = 0;
   reg                        m_store      = 0;
   reg  [DC_SET_INDEX_BITS-1:0] m_set      = 0;
   reg  [TAG_BITS-1:0]        m_chk        = 0; // Tag check
   reg  [DC_LINE_INDEX_BITS-1:0] m_csi     = 0; // Cache Set Index (which cache in the set)
   reg  [DC_WORD_INDEX_BITS-1:0] m_wi      = 0; // Word Index (which word in the cache line)
   reg  [31:0]                m_store_data = 0;
   reg  [ 3:0]                m_byteena    = 0;
   reg  [ 5:0]                m_opcode     = 0;
   wire [31:0]                dc_q;
   wire [31:0]                q            = m_load ? dc_q : m_res_alu;
   reg  [31:0]                m_res_alu    = 0;


   // Store buffer
   reg  [31:0]                 store_buffer_data[0:(1 << STORE_BUFFER_BITS) - 1];
   reg  [31:0]                 store_buffer_addr[0:(1 << STORE_BUFFER_BITS) - 1];
   reg  [ 3:0]                 store_buffer_be[0:(1 << STORE_BUFFER_BITS) - 1];
   reg  [STORE_BUFFER_BITS-1:0] store_buffer_wp = 0;
   reg  [STORE_BUFFER_BITS-1:0] store_buffer_rp = 0;
   wire [STORE_BUFFER_BITS-1:0] store_buffer_wp_1 = store_buffer_wp + 1;
   wire [STORE_BUFFER_BITS-1:0] store_buffer_rp_1 = store_buffer_rp + 1;


   reg [31:0]                   fill_address;
   reg                          fill_cache = 0;
   reg [DC_WORD_INDEX_BITS-1:0] fill_wi;
   reg [DC_LINE_INDEX_BITS-1:0] fill_csi;
   reg [TAG_BITS-1:0]           fill_chk;
   reg [DC_SET_INDEX_BITS-1:0]  fill_set = 0;

   // ------------------------------------------------------------

   reg  [15:0] q_half_shifted = 0;
   reg  [ 7:0] q_byte_shifted = 0;


   // Shifting and sign-extending the loaded result (if any).
   always @* begin
      case (m_address[1]) // BE
        1: q_half_shifted = q[15: 0];
        0: q_half_shifted = q[31:16];
      endcase
      case (m_address[0]) // BE
        1: q_byte_shifted = q_half_shifted[ 7: 0];
        0: q_byte_shifted = q_half_shifted[15: 8];
      endcase
      case (m_opcode[5:3])
         4: case (m_opcode[1:0])
              0: m_res = {{24{q_byte_shifted[ 7] & ~m_opcode[2]}}, q_byte_shifted};
              1: m_res = {{16{q_half_shifted[15] & ~m_opcode[2]}}, q_half_shifted};
              default: m_res = q;
            endcase
         default: m_res = q;
      endcase
   end

   /*
    * Memory interface
    */
   reg         dmem_readdatavalid = 0;
   reg         peripherals_readdatavalid = 0;

   simpledpram #(TAG_BITS,DC_LINE_INDEX_BITS,"dtag0")
      tag0_ram(.clock(clock), .rdaddress(d_csi), .rddata(x_tag0),
               .wraddress(fill_csi), .wrdata(fill_chk),
               .wren(dmem_readdatavalid && &fill_wi && fill_set == 0));

   simpledpram #(TAG_BITS,DC_LINE_INDEX_BITS,"dtag1")
      tag1_ram(.clock(clock), .rdaddress(d_csi), .rddata(x_tag1),
               .wraddress(fill_csi), .wrdata(fill_chk),
               .wren(dmem_readdatavalid && &fill_wi && fill_set == 1));

   simpledpram #(TAG_BITS,DC_LINE_INDEX_BITS,"dtag2")
      tag2_ram(.clock(clock), .rdaddress(d_csi), .rddata(x_tag2),
               .wraddress(fill_csi), .wrdata(fill_chk),
               .wren(dmem_readdatavalid && &fill_wi && fill_set == 2));

   simpledpram #(TAG_BITS,DC_LINE_INDEX_BITS,"dtag3")
      tag3_ram(.clock(clock), .rdaddress(d_csi), .rddata(x_tag3),
               .wraddress(fill_csi), .wrdata(fill_chk),
               .wren(dmem_readdatavalid && &fill_wi && fill_set == 3));

   dpram #(32, CACHE_BITS -2, "dcache")
      dcache_ram(.clock(clock),
      // Write-through caches only write to caches on hits, but go to
      // memory in all cases
                 .address_a({x_set,x_csi,x_wi}),
                 .byteena_a(x_byteena),
                 .wrdata_a(x_store_data),
                 .wren_a(x_store && x_hits),
                 .rddata_a(dc_q),

                 .address_b({fill_set,fill_csi,fill_wi}),
                 .byteena_b(4'hF), .wrdata_b(dmem_res`RD),
                 .wren_b(dmem_readdatavalid),
                 .rddata_b());
   //defparam    dcache_ram.debug = 1;


   reg         outstanding_cache_miss = 0;
   reg         got_uncached_data = 0;
   reg         uncached_load_pending = 0;
   reg [31:0]  uncached_data = 0;

   always @(posedge clock) begin
      if (~peripherals_res`HOLD) begin
         // Only issue one
         peripherals_req`R <= 0;
         peripherals_req`W <= 0;
         if (peripherals_req`R | peripherals_req`W)
            $display("%05d  ME uncached request [%x] taken", $time, peripherals_req`A);
      end

      m_valid <= x_valid;

      // Stalling for uncached_loads
      if (x_valid) begin
         // We thread x_res through the byte/halfword extraction network
         m_pc      <= x_pc;
         m_instr   <= x_instr;
         m_address <= x_load ? x_address : 0;
         m_store   <= x_store;
         m_load    <= x_load;
         m_opcode  <= x_opcode;
         m_wbr     <= x_wbr;
         m_res_alu <= x_res;
      end

      m_chk     <= x_chk;
      m_set     <= x_set;
      m_csi     <= x_csi;
      m_wi      <= x_wi;

      // ****** Store ******

      // Uncache stores
      if (x_store && x_address[31:24] == 8'hFF) begin
         $display("%05d   uncached store %8x <- %8x/%1d", $time,
                  x_address, x_store_data, x_byteena);

         if (~peripherals_res`HOLD) begin
            peripherals_req`A   <= x_address;
            peripherals_req`W   <= 1;
            peripherals_req`WD  <= x_store_data;
            peripherals_req`WBE <= x_byteena;
         end else begin
            // Another peripheral transaction is pending, restart the store
            m_restart    <= 1;
            m_valid      <= 0;
            m_restart_pc <= x_pc - 4 * x_is_delay_slot;
            $display("%05d     peripherals busy, restarting %x", $time,
                     x_pc - 4 * x_is_delay_slot);
         end
      end

      // Write-through caches only write to caches on hits, but go to
      // memory in all cases
      if (x_store && x_address[31:24] != 8'hFF) begin
         // Write to store buffer, and stall/restart if buffer is full
         if (store_buffer_wp_1 == store_buffer_rp) begin
            $display("%05d  ME store buffer full, restarting pipe", $time);
            m_restart    <= 1;
            m_valid      <= 0;
            m_restart_pc <= x_pc - 4 * x_is_delay_slot;
         end else begin
            store_buffer_addr[store_buffer_wp] <= x_address;
            store_buffer_data[store_buffer_wp] <= x_store_data;
            store_buffer_be[store_buffer_wp] <= x_byteena;
            store_buffer_wp <= store_buffer_wp_1;
            $display("%05d  SB[%1d] <- %8x:%8x/%1d", $time,
                     store_buffer_wp, x_address, x_store_data, x_byteena);
         end
      end


      // ****** Load ******


      // Uncached loads
      if (x_load && x_address[31:24] == 8'hFF) begin
         if (got_uncached_data) begin
            got_uncached_data     <= 0;
            uncached_load_pending <= 0;
            m_res_alu             <= uncached_data;
            m_load                <= 0; // Guide the mux
            $display("%05d  ME uncached load [%x] final value %x", $time,
                     x_address, uncached_data);
         end else begin
            peripherals_req`A <= x_address;

            // Have to be careful here as it could just have been
            // cleared above
            if (!uncached_load_pending) begin
               $display("%05d  ME new uncached load [%x]", $time, x_address);
               peripherals_req`R <= 1;
            end else
               $display("%05d  ME repeat uncached load [%x], still no uncached data",
                        $time, x_address);
            uncached_load_pending <= 1;

            // Keep the pipe killed until the load until fulfilled
            m_restart    <= 1;
            m_valid      <= 0;
            m_restart_pc <= x_pc - 4 * x_is_delay_slot;
         end
      end


      // Cache loads
      if (x_load && x_address[31:24] != 8'hFF) begin
         $display("%05d  ME %8x:load %8x hits %x in cache (set %d, csi %d, wi %d ; tags %x %x %x %x)", $time,
                  x_pc, x_address, x_hits, x_set, x_csi, x_wi,
                  x_tag0, x_tag1, x_tag2, x_tag3);

         if (x_miss) begin
            // Keep restarting the load until fulfilled
            m_restart    <= 1;
            m_valid      <= 0;
            m_restart_pc <= x_pc - 4 * x_is_delay_slot;
            outstanding_cache_miss <= 1;

            if (!outstanding_cache_miss) begin
               fill_cache   <= 1;
               fill_address <= {x_address[CACHEABLE_BITS-1:LINE_BITS],{(LINE_BITS){1'd0}}};
               fill_wi      <= 0;
               fill_csi     <= x_csi;
               fill_chk     <= x_chk;
               fill_set     <= fill_set + 1'd1; // XXX: Need a better replacement algorithm.
            end

            $display("%05d  ME load miss the cache, restarting %8x", $time,
                     x_pc - 4 * x_is_delay_slot);
         end
      end

      if (m_load & m_valid)
         $display("%05d  ME load %8x -> %8x (%8x, %1d, %1d, %1d) [cache_q %8x]", $time,
                  m_address, m_res,
                  q, m_set, m_csi, m_wi,
                  dc_q);

      dmem_readdatavalid <= dmem_req`R & ~dmem_res`HOLD;
      if (dmem_readdatavalid) begin
         $display("%05d  D$ got [{%1d,%1d,%1d] <- %8x", $time,
                  fill_set, fill_csi, fill_wi, dmem_res`RD);
         fill_wi <= fill_wi + 1'd1;
         if (&fill_wi) begin
            outstanding_cache_miss <= 0;
            m_restart <= 0;
            // Last one, so update tags
            $display("%05d  ME fill done, restarting %8x", $time,
                     m_restart_pc);
         end
      end

      peripherals_readdatavalid <= peripherals_req`R & ~peripherals_res`HOLD;
      if (peripherals_readdatavalid) begin
         $display("%05d  ME uncached load [%x] served %x", $time,
                  peripherals_req`A, peripherals_res`RD);
         got_uncached_data <= 1;
         uncached_data <= peripherals_res`RD;
         m_restart <= 0;
      end

      if (~dmem_res`HOLD) begin
         dmem_req`R <= 0;
         dmem_req`W <= 0;

         if (store_buffer_rp != store_buffer_wp) begin
            dmem_req`A   <= store_buffer_addr[store_buffer_rp];
            dmem_req`W   <= 1;
            dmem_req`WD  <= store_buffer_data[store_buffer_rp];
            dmem_req`WBE <= store_buffer_be[store_buffer_rp];
            store_buffer_rp <= store_buffer_rp_1;
            $display("%05d  D$ issue store %8x <- %8x/%x", $time,
                     store_buffer_addr[store_buffer_rp],
                     store_buffer_data[store_buffer_rp],
                     store_buffer_be[store_buffer_rp]);
         end else if (fill_cache) begin
            dmem_req`R <= 1'd1;
            dmem_req`A <= fill_address;
            fill_address <= fill_address + 4;
            $display("%05d  D$ issue load %8x", $time, fill_address);
            if (&fill_address[DC_WORD_INDEX_BITS+1:2]) begin
               fill_cache <= 0;
               $display("%05d  D$ done issueing", $time);
            end
         end
      end
   end
endmodule

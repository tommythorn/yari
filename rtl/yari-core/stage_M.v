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
              ,output reg  [31:0] x_lw_res // lw has latecy 1, other loads 2

              // Connection to main memory
              ,input              dmem_waitrequest
              ,output reg  [29:0] dmem_address
              ,output reg         dmem_read = 0
              ,output reg         dmem_write = 0
              ,output reg  [31:0] dmem_writedata
              ,output reg   [3:0] dmem_writedatamask
              ,input       [31:0] dmem_readdata
              ,input              dmem_readdatavalid

              // Connection to peripherals
              ,output reg  `REQ   peripherals_req = 0
              ,input  wire `RES   peripherals_res

              // Stage output
              ,output reg         m_valid = 0
              ,output reg  [31:0] m_instr = 0
              ,output reg  [31:0] m_pc    = 0 // XXX for debugging only
              ,output reg  [ 5:0] m_wbr   = 0
              ,output reg  [31:0] m_res   = 0

              ,output reg         m_restart = 0
              ,output reg  [31:0] m_restart_pc

              ,output reg  [31:0] perf_dcache_misses = 0
              ,output reg  [31:0] perf_load_hit_store_hazard = 0
              ,output reg  [31:0] perf_sb_full = 0
              ,output reg  [31:0] perf_io_store_busy = 0
              ,output reg  [31:0] perf_io_load_busy = 0
              );
   parameter debug = 1;

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

   // Inputs to stage EX - split the address into Cache Set Index, Word Index and Check
   wire [31:0]                   d_address = d_op1_val + d_simm;
   wire [TAG_BITS-1:0]           d_chk; // Tag check
   wire [DC_LINE_INDEX_BITS-1:0] d_csi; // Cache Set Index (which cache in the set)
   wire [DC_WORD_INDEX_BITS-1:0] d_wi; // Word Index (which word in the cache line)
   assign {d_chk,d_csi,d_wi} = d_address[31:2];

   // Inputs to stage ME - check tags and access cache (this is the critical path)
   reg  [31:0]                   x_address = 0;

   // The parallel look up in tags and data array means that writes
   // are out of phase with reads so we need to worry about a rare
   // hazard: immediately loading a stored word (note, this could be
   // fx. a sb followed by a lw).
   reg  [29:0]                   x_last_store_address = ~0;
   reg  [TAG_BITS-1:0]           x_chk     = 0; // Tag check
   reg  [DC_LINE_INDEX_BITS-1:0] x_csi     = 0; // Cache Set Index (which cache in the set)
   reg  [DC_WORD_INDEX_BITS-1:0] x_wi      = 0; // Word Index (which word in the cache line)

   always @(posedge clock) begin
      x_address <= d_address;
      {x_chk,x_csi,x_wi} <= {d_chk,d_csi,d_wi};
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
               default:x_set = 2'bxx;
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
   reg  [DC_SET_INDEX_BITS-1:0] m_set      = 0;
   reg  [TAG_BITS-1:0]        m_chk        = 0; // Tag check
   reg  [DC_LINE_INDEX_BITS-1:0] m_csi     = 0; // Cache Set Index (which cache in the set)
   reg  [DC_WORD_INDEX_BITS-1:0] m_wi      = 0; // Word Index (which word in the cache line)
   reg  [31:0]                m_store_data = 0;
   reg  [ 3:0]                m_byteena    = 0;
   reg  [ 5:0]                m_opcode     = 0;
   wire [31:0]                dc_q0, dc_q1, dc_q2, dc_q3;
   always @(*) case (x_set)
               0: x_lw_res = dc_q0;
               1: x_lw_res = dc_q1;
               2: x_lw_res = dc_q2;
               3: x_lw_res = dc_q3;
               endcase

   reg  [31:0]                m_res_alu    = 0;
   reg  [31:0]                m_lw_res     = 0;
   always @(posedge clock)    m_lw_res    <= x_lw_res;

   wire [31:0]                q            = m_load ? m_lw_res : m_res_alu;


   // Store buffer
   reg  [31:0]                 store_buffer_data[0:(1 << STORE_BUFFER_BITS) - 1];
   reg  [29:0]                 store_buffer_addr[0:(1 << STORE_BUFFER_BITS) - 1];
   reg  [ 3:0]                 store_buffer_be[0:(1 << STORE_BUFFER_BITS) - 1];
   reg  [STORE_BUFFER_BITS-1:0] store_buffer_wp = 0;
   reg  [STORE_BUFFER_BITS-1:0] store_buffer_rp = 0;
   wire [STORE_BUFFER_BITS-1:0] store_buffer_wp_1 = store_buffer_wp + 1'd1;
   wire [STORE_BUFFER_BITS-1:0] store_buffer_rp_1 = store_buffer_rp + 1'd1;


   reg [29:0]                   fill_address;
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

   /*
    * Each way get its own memory block as we look up in all set in parallel
    * and use a late select.
    */

   dpram #(32, CACHE_BITS -4, "dcache0")
      dcache0_ram(.clock(clock),
                 // Write-through caches only write to caches on hits, but go to
                 // memory in all cases
                 .address_a({d_csi,d_wi}),
                 .rddata_a(dc_q0),
                 .byteena_a(0),
                 .wrdata_a(0),
                 .wren_a(0),

                 // Unfortunately, we have to use different port for
                 // reading and writing now, as we can't write until
                 // we know our way (pun intended). This means the
                 // fill machinery has to use the stage X pipeline
                 // registers.
                 .address_b(outstanding_cache_miss ? {fill_csi,fill_wi} : {x_csi,x_wi}),
                 .byteena_b(outstanding_cache_miss ? 4'hF               : x_byteena),
                 .wrdata_b (outstanding_cache_miss ? dmem_readdata      : x_store_data),
                 .wren_b(dmem_readdatavalid && fill_set == 0 ||
                         x_store && x_hits && x_set == 0),
                 .rddata_b());
   //defparam    dcache_ram.debug = 1;

   dpram #(32, CACHE_BITS -4, "dcache1")
      dcache1_ram(.clock(clock),
                 // Write-through caches only write to caches on hits, but go to
                 // memory in all cases
                 .address_a({d_csi,d_wi}),
                 .rddata_a(dc_q1),
                 .byteena_a(0),
                 .wrdata_a(0),
                 .wren_a(0),

                 // Unfortunately, we have to use different port for
                 // reading and writing now, as we can't write until
                 // we know our way (pun intended). This means the
                 // fill machinery has to use the stage X pipeline
                 // registers.
                 .address_b(outstanding_cache_miss ? {fill_csi,fill_wi} : {x_csi,x_wi}),
                 .byteena_b(outstanding_cache_miss ? 4'hF               : x_byteena),
                 .wrdata_b (outstanding_cache_miss ? dmem_readdata      : x_store_data),
                 .wren_b(dmem_readdatavalid && fill_set == 1 ||
                         x_store && x_hits && x_set == 1),
                 .rddata_b());
   //defparam    dcache_ram.debug = 1;

   dpram #(32, CACHE_BITS -4, "dcache2")
      dcache2_ram(.clock(clock),
                 // Write-through caches only write to caches on hits, but go to
                 // memory in all cases
                 .address_a({d_csi,d_wi}),
                 .rddata_a(dc_q2),
                 .byteena_a(0),
                 .wrdata_a(0),
                 .wren_a(0),

                 // Unfortunately, we have to use different port for
                 // reading and writing now, as we can't write until
                 // we know our way (pun intended). This means the
                 // fill machinery has to use the stage X pipeline
                 // registers.
                 .address_b(outstanding_cache_miss ? {fill_csi,fill_wi} : {x_csi,x_wi}),
                 .byteena_b(outstanding_cache_miss ? 4'hF               : x_byteena),
                 .wrdata_b (outstanding_cache_miss ? dmem_readdata      : x_store_data),
                 .wren_b(dmem_readdatavalid && fill_set == 2 ||
                         x_store && x_hits && x_set == 2),
                 .rddata_b());
   //defparam    dcache_ram.debug = 1;

   dpram #(32, CACHE_BITS -4, "dcache3")
      dcache3_ram(.clock(clock),
                 // Write-through caches only write to caches on hits, but go to
                 // memory in all cases
                 .address_a({d_csi,d_wi}),
                 .rddata_a(dc_q3),
                 .byteena_a(0),
                 .wrdata_a(0),
                 .wren_a(0),

                 // Unfortunately, we have to use different port for
                 // reading and writing now, as we can't write until
                 // we know our way (pun intended). This means the
                 // fill machinery has to use the stage X pipeline
                 // registers.
                 .address_b(outstanding_cache_miss ? {fill_csi,fill_wi} : {x_csi,x_wi}),
                 .byteena_b(outstanding_cache_miss ? 4'hF               : x_byteena),
                 .wrdata_b (outstanding_cache_miss ? dmem_readdata      : x_store_data),
                 .wren_b(dmem_readdatavalid && fill_set == 3 ||
                         x_store && x_hits && x_set == 3),
                 .rddata_b());
   //defparam    dcache_ram.debug = 1;



   reg         outstanding_cache_miss = 0;
   reg         got_uncached_data = 0;
   reg         uncached_load_pending = 0;
   reg [31:0]  uncached_data = 0;
   reg         one_shot_restart = 0;

   reg [32:0]  lfsr = 0;

   always @(posedge clock) begin
      x_last_store_address <= ~0;

      lfsr <= {lfsr[31:0], ~lfsr[32] ^ lfsr[19]};

      if (~peripherals_res`HOLD) begin
         // Only issue one
         peripherals_req`R <= 0;
         peripherals_req`W <= 0;
         if (peripherals_req`R | peripherals_req`W)
            $display("%05d  ME uncached request [%x] taken", $time, peripherals_req`A);
      end

      m_valid <= x_valid;

      if (one_shot_restart)
         m_restart <= 0;
      one_shot_restart <= 0;

      // Stalling for uncached_loads
      if (x_valid) begin
         // We thread x_res through the byte/halfword extraction network
         m_pc      <= x_pc;
         m_instr   <= x_instr;
         m_address <= x_load ? x_address : 0;
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
            perf_io_store_busy <= perf_io_store_busy + 1;
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
            $display("%05d  ME store buffer full, restarting %x", $time,
                     x_pc - 4 * x_is_delay_slot);
            m_restart    <= 1;
            m_valid      <= 0;
            m_restart_pc <= x_pc - 4 * x_is_delay_slot;
            one_shot_restart <= 1;
            perf_sb_full <= perf_sb_full + 1;
         end else begin
            x_last_store_address               <= x_address[31:2];
            store_buffer_addr[store_buffer_wp] <= x_address[31:2];
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
            perf_io_load_busy <= perf_io_load_busy + 1;
         end
      end


      // Cache loads
      if (x_load && x_address[31:24] != 8'hFF) begin
         $display("%05d  ME %8x:load %8x x_hits %x in cache (set %d, csi %d, wi %d ; tags %x %x %x %x)", $time,
                  x_pc, x_address, x_hits, x_set, x_csi, x_wi,
                  x_tag0, x_tag1, x_tag2, x_tag3, x_lw_res);
         $display("         q %x %x %x %x -> %x", dc_q0, dc_q1, dc_q2, dc_q3, x_lw_res);

         if (x_address[31:2] == x_last_store_address) begin
            /*
             * Previous instruction was a store to the location we're
             * loading, but the store has finish yet as it happens one
             * cycle out of phase of the load. We could forward the
             * result, but that's tricky and expensive. Given how rare
             * this is (expected to be), we simply restart.
             */
            m_restart    <= 1;
            m_valid      <= 0;
            m_restart_pc <= x_pc - 4 * x_is_delay_slot;
            one_shot_restart <= 1;
            perf_load_hit_store_hazard <= perf_load_hit_store_hazard + 1;

            $display("%05d  ME load hit store hazard, restarting %8x", $time,
                     x_pc - 4 * x_is_delay_slot);
         end

         /*
          * We don't need an else here as the two conditions cannot
          * happen simultaneously.
          */

         if (x_miss) begin

            /*
             * There are several things going on here so we have to
             * be more careful and that translates into higher latency.
             * First, as designed, loads can come in and miss while
             * we are filling a line. In such cases we must ignore and
             * restart the miss.
             * Second, there may be pending stores in the store buffer
             * so we cannot simply issue the read here, but signal it
             * with fill_cache which is then picked up below (we _could_
             * and probably _should_ start it here if the store buffer is
             * empty, but it's just one cycle out of many, not a big
             * deal.).
             */

            perf_dcache_misses <= perf_dcache_misses + 1;

            m_restart    <= 1;
            m_valid      <= 0;
            m_restart_pc <= x_pc - 4 * x_is_delay_slot;
            outstanding_cache_miss <= 1;

            if (!outstanding_cache_miss) begin
               fill_cache   <= 1;
               // XXX we get a warning here as fill_address is always
               // starting at the beginning of a line, thus the bottom
               // bits are zero. This comment is to remind me to
               // eventually play with critical-word-first and
               // background filling.
               fill_address <= {x_address[CACHEABLE_BITS-1:LINE_BITS],{(LINE_BITS-2){1'd0}}};
               fill_wi      <= 0;
               fill_csi     <= x_csi;
               fill_chk     <= x_chk;
               fill_set     <= lfsr[1:0];
            end

            $display("%05d  ME load miss the cache, restarting %8x", $time,
                     x_pc - 4 * x_is_delay_slot);
         end
      end

      if (m_load & m_valid)
         $display("%05d  ME load %8x -> %8x (%8x, %1d, %1d, %1d) [x_lw_res %8x]", $time,
                  m_address, m_res,
                  q, m_set, m_csi, m_wi,
                  x_lw_res);

      if (dmem_readdatavalid) begin
         $display("%05d  D$ got [{%1d,%1d,%1d] <- %8x", $time,
                  fill_set, fill_csi, fill_wi, dmem_readdata);
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

      if (~dmem_waitrequest) begin

         dmem_read <= 0;
         dmem_write <= 0;

         if (store_buffer_rp != store_buffer_wp) begin

            dmem_address       <= store_buffer_addr[store_buffer_rp];
            dmem_write         <= 1;
            dmem_writedata     <= store_buffer_data[store_buffer_rp];
            dmem_writedatamask <= store_buffer_be[store_buffer_rp];
            store_buffer_rp    <= store_buffer_rp_1;

            $display("%05d  D$ issue store %8x <- %8x/%x", $time,
                     {store_buffer_addr[store_buffer_rp],2'd0},
                     store_buffer_data[store_buffer_rp],
                     store_buffer_be[store_buffer_rp]);

         end else if (fill_cache) begin

            dmem_address <= fill_address;
            dmem_read    <= 1;
            fill_cache   <= 0;

            $display("%05d  D$ issue load %8x", $time, fill_address);
         end
      end
   end
endmodule

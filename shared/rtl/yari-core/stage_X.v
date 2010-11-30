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

`timescale 1ns/10ps
`include "asm.v"
`include "perfcounters.v"

module stage_X(input  wire        clock

              ,input  wire        restart // for synci
              ,input  wire [31:0] restart_pc // for synci

              ,input  wire        d_valid
              ,input  wire [31:0] d_instr
              ,input  wire [31:0] d_pc
              ,input  wire [31:0] d_npc
              ,input  wire [ 5:0] d_opcode
              ,input  wire [ 5:0] d_fn
              ,input  wire [ 4:0] d_rd
              ,input  wire [ 5:0] d_rs
              ,input  wire [ 5:0] d_rt
              ,input  wire [ 4:0] d_sa
              ,input  wire [31:0] d_target
              ,input  wire [ 5:0] d_wbr
              ,input  wire        d_has_delay_slot

              ,input  wire [31:0] d_op1_val
              ,input  wire [31:0] d_op2_val
              ,input  wire [31:0] d_rt_val
              ,input  wire [31:0] d_simm


              ,input  wire        d_restart
              ,input  wire [31:0] d_restart_pc

              ,input  wire        d_load_use_hazard

              ,input  wire        m_valid
              ,input  wire [ 5:0] m_wbr

              ,output reg         x_valid         = 0
              ,output reg  [31:0] x_instr         = 0 // XXX for debugging only
              ,output reg         x_is_delay_slot = 0
              ,output reg  [31:0] x_pc            = 0
              ,output reg  [ 5:0] x_opcode        = 0
              ,output reg  [31:0] x_op1_val       = 0 // XXX
              ,output reg  [ 5:0] x_rt            = 0
              ,output reg  [31:0] x_rt_val        = 0 // for stores only
              ,output reg  [ 5:0] x_wbr           = 0
              ,output reg  [31:0] x_res

              ,output reg         x_synci         = 0
              ,output reg  [31:0] x_synci_a       = 0

              ,output reg         x_restart       = 0
              ,output reg  [31:0] x_restart_pc    = 0
              ,output reg         x_flush_D       = 0

              ,output reg  [31:0] perf_branch_hazard = 0
              ,input  wire [31:0] perf_dcache_misses
              ,input  wire [31:0] perf_delay_slot_bubble
              ,output reg  [31:0] perf_div_hazard = 0
              ,input  wire [31:0] perf_icache_misses
              ,input  wire [31:0] perf_io_load_busy
              ,input  wire [31:0] perf_io_store_busy
              ,input  wire [31:0] perf_load_hit_store_hazard
              ,output reg  [31:0] perf_load_use_hazard = 0
              ,output reg  [31:0] perf_mult_hazard = 0
              ,input  wire [47:0] perf_retired_inst
              ,input  wire [31:0] perf_sb_full
              );

   parameter FREQ = 0;
   parameter debug = 0;

`include "config.h"

   reg [31:0] x_op2_val = 0;
   reg [ 5:0] x_fn      = 0;
   reg [ 4:0] x_sa      = 0;

   wire [31:0]        perf_frequency   = FREQ / 1000; // Given in Hz, reported in kHz

   wire               d_ops_eq         = d_op1_val == d_op2_val;
   reg                x_negate_op2     = 0;

   always @(posedge clock)
      x_negate_op2 <= d_opcode == `SLTI  ||
                      d_opcode == `SLTIU ||
                      d_opcode == `REG && (d_fn == `SLT  ||
                                           d_fn == `SLTU ||
                                           d_fn == `SUB  ||
                                           d_fn == `SUBU);
   wire [31:0]        x_sum;
   wire               x_carry_flag;
   wire [31:0]        x_op2_neg          = {32{x_negate_op2}} ^ x_op2_val;
   assign             {x_carry_flag,x_sum} = x_op1_val + x_op2_neg + x_negate_op2;
   wire               x_sign_flag        = x_sum[31];
   wire               x_overflow_flag    = x_op1_val[31] == x_op2_neg[31] &&
                                           x_op1_val[31] != x_sum[31];
   wire [4:0]         x_shift_dist       = x_fn[2] ? x_op1_val[4:0] : x_sa;

   // XXX BUG These architectural registers must live in ME or later
   // as ME can flush the pipe rendering an update of state in EX
   // premature. Of course this leads to headaches with forwarding and
   // hazards on instruction depending on these... Sigh.
   reg                mult_busy = 0;
   reg [63:0]         mult_a = 0;
`ifdef MULT_RADIX_4
   reg [63:0]         mult_3a = 0;
`endif
   reg [31:0]         mult_b = 0;
   reg                mult_neg = 0;
   reg [31:0]         mult_lo = 0;
   reg [31:0]         mult_hi = 0;

   reg                div_busy = 0, div_neg_res, div_neg_rem;
   reg [31:0]         divisor = 0, div_hi = 0, div_lo = 0;

   wire [64:0]        div_shifted = {div_hi, div_lo, 1'd0};
   wire [32:0]        div_diff    = div_shifted[64:32] - divisor;

   reg [ 6:0]         div_n = 0;

`ifdef LATER
   reg [31:0]         cp0_status = 0,     // XXX -- " --
                      cp0_epc = 0,
                      cp0_errorepc = 0,
                      cp0_cause = 0;
`endif

   reg x_has_delay_slot = 0;

   reg [35:0] tsc = 0; // Free running counter

   reg branch_event = 0;

   reg [31:0] x_special = 0; // A value that can be precomputed
   always @(posedge clock)
      case (d_opcode)
      `REG:    x_special <= d_npc + 4;
      `REGIMM: x_special <= d_npc + 4;
      `JAL:    x_special <= d_npc + 4;
      `RDHWR:
         case (d_rd)
         0:  x_special <= 0; // # of processors-1
         1:  x_special <= 4 << IC_WORD_INDEX_BITS;
         2:  x_special <= tsc[35:4]; // @40 MHz 28 min before rollover
         3:  x_special <= 1 << 4;    // TSC scaling factor
         4:  x_special <= tsc[31:0]; // Unscaled, but truncated TSC (local hack)
         endcase

      `LUI: x_special <= {d_simm[15: 0], 16'd0};
      `CP2:
         case (d_rd)
         `PERF_BRANCH_HAZARD:     x_special <= perf_branch_hazard;
         `PERF_DCACHE_MISSES:     x_special <= perf_dcache_misses;
         `PERF_DELAY_SLOT_BUBBLE: x_special <= perf_delay_slot_bubble;
         `PERF_DIV_HAZARD:        x_special <= perf_div_hazard;
         `PERF_FREQUENCY:         x_special <= perf_frequency;
         `PERF_ICACHE_MISSES:     x_special <= perf_icache_misses;
         `PERF_IO_LOAD_BUSY:      x_special <= perf_io_load_busy;
         `PERF_IO_STORE_BUSY:     x_special <= perf_io_store_busy;
         `PERF_LOAD_HIT_STORE_HAZARD: x_special <= perf_load_hit_store_hazard;
         `PERF_LOAD_USE_HAZARD:   x_special <= perf_load_use_hazard;
         `PERF_MULT_HAZARD:       x_special <= perf_mult_hazard;
         // Count 16 retired instructions. @40 MHz 1 CPI, it takes 28 min to roll over
         `PERF_RETIRED_INST:      x_special <= perf_retired_inst[35:4];
         `PERF_SB_FULL:           x_special <= perf_sb_full;
         endcase
      endcase

   /*
    * The ALU
    */

   always @* begin
      x_res = 32'hXXXXXXXX;
      case (x_opcode)
      `REG:
         case (x_fn)
         `SLL :   x_res = x_op2_val          <<  x_shift_dist;
         `SRL :   x_res = x_op2_val          >>  x_shift_dist;
         `SRA :   x_res = $signed(x_op2_val) >>> x_shift_dist;
         `SLLV:   x_res = x_op2_val          <<  x_shift_dist;
         `SRLV:   x_res = x_op2_val          >>  x_shift_dist;
         `SRAV:   x_res = $signed(x_op2_val) >>> x_shift_dist;

         `JALR:   x_res = x_special;
         // XXX BUG See the comment above with mult_lo and mult_hi
         `MFHI:   x_res = mult_hi;
         `MFLO:   x_res = mult_lo;
         // XXX BUG Trap on overflow for ADD, ADDI and SUB
         `ADD:    x_res = x_sum;
         `ADDU:   x_res = x_sum;
         `SUB:    x_res = x_sum;
         `SUBU:   x_res = x_sum;
         `AND:    x_res = x_op1_val & x_op2_val;
         `OR:     x_res = x_op1_val | x_op2_val;
         `XOR:    x_res = x_op1_val ^ x_op2_val;
         `NOR:    x_res = ~(x_op1_val | x_op2_val);
         `SLT:    x_res = {{31{1'b0}}, x_sign_flag ^ x_overflow_flag};
         `SLTU:   x_res = {{31{1'b0}}, ~x_carry_flag};
         default: x_res = 32'hXXXXXXXX;
         endcase
      `REGIMM:    x_res = x_special;// BLTZ, BGEZ, BLTZAL, BGEZAL
      `JAL:       x_res = x_special;
      `ADDI:      x_res = x_sum;
      `ADDIU:     x_res = x_sum;
      `SLTI:      x_res = {{31{1'b0}}, x_sign_flag ^ x_overflow_flag};
      `SLTIU:     x_res = {{31{1'b0}}, ~x_carry_flag};
      `ANDI:      x_res = {16'b0,            x_op1_val[15:0] & x_op2_val[15:0]};
      `ORI:       x_res = {x_op1_val[31:16], x_op1_val[15:0] | x_op2_val[15:0]};
      `XORI:      x_res = {x_op1_val[31:16], x_op1_val[15:0] ^ x_op2_val[15:0]};
      `LUI:       x_res = x_special;
      //`CP1:
      `RDHWR:     x_res = x_special;
      `CP2:       x_res = x_special;
      default: x_res = 32'hXXXXXXXX;
      endcase
   end

   always @(posedge clock) begin
      tsc                <= tsc + 1;
      x_valid            <= d_valid;
      x_instr            <= d_instr;
      x_pc               <= d_pc;
      x_opcode           <= d_opcode;
      x_fn               <= d_fn;
      x_sa               <= d_sa;
      x_op1_val          <= d_op1_val;
      x_op2_val          <= d_op2_val;
      x_rt               <= d_rt;
      x_rt_val           <= d_rt_val;
      x_wbr              <= d_wbr;
      x_has_delay_slot   <= d_has_delay_slot & d_valid;
      x_is_delay_slot    <= x_has_delay_slot & x_valid;

      x_restart          <= 0;
      x_restart_pc       <= d_target;
      x_flush_D          <= 0;
      x_synci            <= 0;


      /* Stat counts aren't critical, so I delay them to keep them out
         of the critical path */
      if (branch_event)
         perf_branch_hazard <= perf_branch_hazard + 1;
      branch_event <= 0;

//`define MULT_RADIX_4 1
`ifdef MULT_RADIX_4
      // Radix-2 Multiplication Machine (this is not the best way to do this)
      if (mult_busy) begin
         $display("MULT[U] %x * %x + %x", mult_a, mult_b, {mult_hi,mult_lo});

         case (mult_b[1:0])
         1: {mult_hi,mult_lo} <= {mult_hi,mult_lo} + mult_a;
         2: {mult_hi,mult_lo} <= {mult_hi,mult_lo} + (mult_a << 1);
         3: {mult_hi,mult_lo} <= {mult_hi,mult_lo} + mult_3a;
         endcase
         mult_a <= mult_a << 2;
         mult_3a <= mult_3a << 2;
         mult_b <= mult_b >> 2;
         if (mult_b == 0) begin
            if (mult_neg) begin
               {mult_hi,mult_lo} <= 64'd0 - {mult_hi,mult_lo};
               mult_neg <= 0;
            end else
               mult_busy <= 0;
            $display("MULT[U] = %x", mult_a + {mult_hi,mult_lo});
         end
      end
`else
      // Radix-2 Multiplication Machine (this is not the best way to do this)
      if (mult_busy) begin
         $display("MULT[U] %x * %x + %x", mult_a, mult_b, {mult_hi,mult_lo});

         if (mult_b[0])
            {mult_hi,mult_lo} <= {mult_hi,mult_lo} + mult_a;
         mult_a <= mult_a << 1;
         mult_b <= mult_b >> 1;
         if (mult_b == 0) begin
            if (mult_neg) begin
               {mult_hi,mult_lo} <= 64'd0 - {mult_hi,mult_lo};
               mult_neg <= 0;
            end else
               mult_busy <= 0;
            $display("MULT[U] = %x", mult_a + {mult_hi,mult_lo});
         end
      end
`endif

      /*
       * Division uses a simple algorithm:
       * for 1 .. 32:
       *    divident = divident << 1
       *    if (divident >= (divisor << 32)):
       *       divident = divident - (divisor << 32) + 1
       * result = divisor & 0xFFFF_FFFF
       */
      if (!div_n[6]) begin
         {div_hi,div_lo} <= div_shifted[63:0];
         if (!div_diff[32]) begin
            div_hi    <= div_diff[31:0];
            div_lo[0] <= 1'd1;
         end
         div_n <= div_n - 1'd1;
      end else if (div_busy) begin
         div_busy <= 0;
         mult_lo <= div_neg_res ? -div_lo : div_lo; // result
         mult_hi <= div_neg_rem ? -div_hi : div_hi; // remainder
         $display("DIV = hi %d lo %d",
                  div_neg_rem ? -div_hi : div_hi,
                  div_neg_res ? -div_lo : div_lo);
      end

      case (d_opcode)
      `REG:
         case (d_fn)
         `JALR:
            if (d_valid) begin
               $display("JAL: d_npc = %x", d_npc);
               x_restart    <= 1;
               x_restart_pc <= d_op1_val;
               branch_event <= 1;
            end
         `JR:
            if (d_valid) begin
               x_restart    <= 1;
               x_restart_pc <= d_op1_val;
               branch_event <= 1;
            end

         // XXX BUG See the comment above with mult_lo and mult_hi
         `MFHI:
            if ((mult_busy | div_busy) && d_valid) begin
               x_flush_D    <= 1;
               x_valid      <= 0;
               x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
               x_restart    <= 1;
               if (mult_busy)
                  perf_mult_hazard <= perf_mult_hazard + 1;
               else
                  perf_div_hazard <= perf_div_hazard + 1;
            end
         `MFLO:
            if ((mult_busy | div_busy) && d_valid) begin
               x_flush_D    <= 1;
               x_valid      <= 0;
               x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
               x_restart    <= 1;
               if (mult_busy)
                  perf_mult_hazard <= perf_mult_hazard + 1;
               else
                  perf_div_hazard <= perf_div_hazard + 1;
            end
         `MTHI:
            if (d_valid) begin
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
                  if (mult_busy)
                     perf_mult_hazard <= perf_mult_hazard + 1;
                  else
                     perf_div_hazard <= perf_div_hazard + 1;
               end else
                  mult_hi      <= d_op1_val;
            end
         `MTLO:
            if (d_valid) begin
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
                  if (mult_busy)
                     perf_mult_hazard <= perf_mult_hazard + 1;
                  else
                     perf_div_hazard <= perf_div_hazard + 1;
               end else
                  mult_lo      <= d_op1_val;
            end

         `DIV:
            if (d_valid)
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
                  if (mult_busy)
                     perf_mult_hazard <= perf_mult_hazard + 1;
                  else
                     perf_div_hazard <= perf_div_hazard + 1;
               end else begin
                  div_busy    <= 1;
                  div_hi      <= 0;
                  div_lo      <= d_op1_val[31] ? -d_op1_val : d_op1_val;
                  divisor     <= d_op2_val[31] ? -d_op2_val : d_op2_val;
                  div_neg_res <= d_op1_val[31] ^ d_op2_val[31];

                  // res = a/b, rem = a - b*(a/b)
                  // thus the rem sign follows a only

                  div_neg_rem <= d_op1_val[31];
                  div_n       <= 31;
                  $display("%05dc EX: %d / %d", $time, d_op1_val, d_op2_val);
               end

         `DIVU:
            if (d_valid)
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
                  if (mult_busy)
                     perf_mult_hazard <= perf_mult_hazard + 1;
                  else
                     perf_div_hazard <= perf_div_hazard + 1;
               end else begin
                  div_busy    <= 1;
                  div_hi      <= 0;
                  div_lo      <= d_op1_val;
                  divisor     <= d_op2_val;
                  div_neg_res <= 0;
                  div_neg_rem <= 0;
                  div_n       <= 31;
                  $display("%05dc EX: %d /U %d", $time, d_op1_val, d_op2_val);
               end

         `MULTU:
            if (d_valid)
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
                  if (mult_busy)
                     perf_mult_hazard <= perf_mult_hazard + 1;
                  else
                     perf_div_hazard <= perf_div_hazard + 1;
               end else begin
                  $display("MULTU %x * %x", d_op1_val, d_op2_val);
                  mult_busy <= 1;
                  mult_hi <= 0;
                  mult_lo <= 0;
                  mult_a <= d_op1_val;
                  mult_b <= d_op2_val;
`ifdef MULT_RADIX_4
                  mult_3a <= 3 * d_op1_val;
`endif
                  mult_neg <= 0;

                  $display("%05dc EX: %dU * %dU", $time, d_op1_val, d_op2_val);
               end

         `MULT:
            if (d_valid)
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
                  if (mult_busy)
                     perf_mult_hazard <= perf_mult_hazard + 1;
                  else
                     perf_div_hazard <= perf_div_hazard + 1;
               end else begin
                  $display("MULT %x * %x", d_op1_val, d_op2_val);
                  mult_busy <= 1;
                  mult_hi <= 0;
                  mult_lo <= 0;
                  mult_neg <= d_op1_val[31] ^ d_op2_val[31];
                  mult_a <= d_op1_val[31] ? {32'd0,32'd0 - d_op1_val} : d_op1_val;
`ifdef MULT_RADIX_4
                  mult_3a <= d_op1_val[31] ? 3 * {32'd0,32'd0-d_op1_val} : 3 * d_op1_val;
`endif
                  mult_b <= d_op2_val[31] ? 32'd0 - d_op2_val  : d_op2_val;
                  $display("%05dc EX: %d * %d", $time, d_op1_val, d_op2_val);
               end

            `BREAK:
               if (d_valid) begin
                  x_restart    <= 1;
                  x_restart_pc <= 'hBFC00380;
                  x_flush_D    <= 1;
`ifdef LATER
                  cp0_status[`CP0_STATUS_EXL] <= 1;
                  //cp0_cause.exc_code = EXC_BP;
                  cp0_cause <= 9 << 2;
                  // cp0_cause.bd = branch_delay_slot; // XXX DELAY SLOT HANDLING!
                  cp0_epc <= d_pc; // XXX DELAY SLOT HANDLING!
`endif
               end
            endcase
      `REGIMM: // BLTZ, BGEZ, BLTZAL, BGEZAL
         if (d_valid)
            if (d_rt[4:0] == `SYNCI) begin
               x_restart    <= 1;
               x_restart_pc <= x_restart ? restart_pc : d_npc;
               x_flush_D    <= 1;
               $display("synci restart at %x (d_restart = %d, d_restart_pc = %x, d_npc = %x)",
                        d_restart ? d_restart_pc : d_npc,
                        d_restart, d_restart_pc, d_npc);
               x_synci      <= 1;
               x_synci_a    <= d_op1_val + d_simm;
            end else begin
               x_restart <= d_rt[0] ^ d_op1_val[31];
               branch_event <= 1;
            end
      `JAL:
         if (d_valid) begin
            x_restart <= 1;
            branch_event <= 1;
         end
      `J: if (d_valid) x_restart <= 1;
      `BEQ:
         if (d_valid) begin
            x_restart <= d_ops_eq;
            branch_event <= d_ops_eq;
            $display("%05d BEQ %8x == %8x (%1d)", $time,
                     d_op1_val, d_op2_val, d_ops_eq);
         end
      `BNE:
         if (d_valid) begin
            x_restart <= ~d_ops_eq;
            branch_event <= ~d_ops_eq;
            $display("%05d BNE %8x != %8x (%1d) target %8x", $time,
                     d_op1_val, d_op2_val, !d_ops_eq, d_target);
         end

      `BLEZ:
         if (d_valid) begin
            x_restart <= d_op1_val[31] || d_op1_val == 0;
            branch_event <= (d_op1_val[31] || d_op1_val == 0);
         end

      `BGTZ:
         // XXX Share logic
         if (d_valid) begin
            x_restart <= !d_op1_val[31] && d_op1_val != 0;
            branch_event <= (!d_op1_val[31] && d_op1_val != 0);
         end

         `CP2: begin
`ifdef SIMULATE_MAIN
            if (d_valid && !d_rs[4] && 0) begin
               if (mult_lo == 32'h87654321)
                  $display("TEST SUCCEEDED!");
               else
                  $display("%05d TEST FAILED WITH %x  (%1d:%8x:%8x)", $time, mult_lo,
                           d_valid, d_pc, d_instr);
               $finish; // XXX do something more interesting for real hw.
            end else
`endif
               if (~d_rs[4])
                  if (d_rs[2])
                     $display("MTCP2 r%d <- %x (ignored)", d_rd, d_op2_val);
                  else
                     $display("MFCP2 r%d", d_rd);
         end

         /*
          * XXX Comment out the CP0 handling for now. I want to handle
          * that in a way that doesn't affect the performance of the
          * regular instructions
          */
`ifdef LATER
      `CP0: if (d_valid) begin
         /* Two possible formats */
         if (d_rs[4]) begin
            if (d_fn == `C0_ERET) begin
               /* Exception Return */
               x_restart <= 1;
               x_flush_D <= 1; // XXX BUG? Check that ERET doesn't have a delay slot!
               if (cp0_status[`CP0_STATUS_ERL]) begin
                  x_restart_pc <= cp0_errorepc;
                  cp0_status[`CP0_STATUS_ERL] <= 0;
 `ifdef SIMULATE_MAIN
                  $display("ERET ERROREPC %x", cp0_errorepc);
 `endif
               end else begin
                  x_restart_pc <= cp0_epc;
                  cp0_status[`CP0_STATUS_EXL] <= 0;
 `ifdef SIMULATE_MAIN
                  $display("ERET EPC %x", cp0_epc);
 `endif
               end
            end
 `ifdef SIMULATE_MAIN
            else
               /* C1 format */
               $display("Unhandled CP0 command %s\n",
                        d_fn == `C0_TLBR  ? "tlbr" :
                        d_fn == `C0_TLBWI ? "tlbwi" :
                        d_fn == `C0_TLBWR ? "tlbwr" :
                        d_fn == `C0_TLBP  ? "tlbp" :
                        d_fn == `C0_ERET  ? "eret" :
                        d_fn == `C0_DERET ? "deret" :
                        d_fn == `C0_WAIT  ? "wait" :
                        "???");
 `endif
         end else begin
 `ifdef SIMULATE_MAIN
            if (d_rs[2])
               $display("MTCP0 r%d <- %x", d_rd, d_op2_val);
            else
               $display("MFCP0 r%d", d_rd);

            if (d_fn != 0) $display("d_fn == %x", d_fn);
 `endif
            if (d_rs[2]) begin
               x_wbr <= 0; // XXX BUG?
               // cp0regs[i.r.rd] = t;
               case (d_rd)
               `CP0_STATUS:
                  begin
                     cp0_status   <= d_op2_val;
                     $display("STATUS <= %x", d_op2_val);
                  end
               `CP0_CAUSE:
                  begin
                     cp0_cause <= d_op2_val;
                     $display("CAUSE <= %x", d_op2_val);
                  end
               `CP0_EPC:
                  begin
                     cp0_epc      <= d_op2_val;
                     $display("EPC <= %x", d_op2_val);
                  end
               `CP0_ERROREPC:
                  begin
                     cp0_errorepc <= d_op2_val;
                     $display("ERROREPC <= %x", d_op2_val);
                  end
               /*
                cp0_status.raw = t;
                cp0_status.res1 = cp0_status.res2 = 0;
                printf("Operating mode %s\n",
                cp0_status.ksu == 0 ? "kernel" :
                cp0_status.ksu == 1 ? "supervisor" :
                cp0_status.ksu == 2 ? "user" : "??");
                printf("Exception level %d\n", cp0_status.exl);
                printf("Error level %d\n", cp0_status.erl);
                printf("Interrupts %sabled\n", cp0_status.ie ? "en" : "dis");
                break;
                */
               default:
                  $display("Setting an unknown CP0 register %d", d_rd);
               //case CP0_CAUSE:
               endcase
            end
         end
      end
`endif
      endcase

      if (d_load_use_hazard)
         perf_load_use_hazard <= perf_load_use_hazard + 1;
   end
endmodule

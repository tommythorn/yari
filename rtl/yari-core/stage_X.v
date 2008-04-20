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

`timescale 1ns/10ps
`include "asm.v"

module stage_X(input  wire        clock

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


              ,input  wire        d_restart
              ,input  wire [31:0] d_restart_pc

              ,input  wire        m_valid
              ,input  wire [ 5:0] m_wbr

              ,output reg         x_valid         = 0
              ,output reg  [31:0] x_instr         = 0 // XXX for debugging only
              ,output reg         x_is_delay_slot = 0
              ,output reg  [31:0] x_pc            = 0
              ,output reg  [ 5:0] x_opcode        = 0
              ,output reg  [31:0] x_op1_val       = 0 // XXX
              ,output reg  [31:0] x_rt_val        = 0 // for stores only
              ,output reg  [ 5:0] x_wbr           = 0
              ,output reg  [31:0] x_res           = 0

              ,output reg         x_restart       = 0
              ,output reg  [31:0] x_restart_pc    = 0
              ,output reg         x_flush_D       = 0
              );

   parameter debug = 0;

`include "config.h"

   wire               ops_eq = d_op1_val == d_op2_val;
   wire [32:0]        subtracted = {d_op1_val[31],d_op1_val} - {d_op2_val[31],d_op2_val};

   wire [4:0]         shift_dist = d_fn[2] ? d_op1_val[4:0] : d_sa;
   wire [31:0]        ashift_out;
   wire [31:0]        lshift_out;

   // XXX BUG These architectural registers must live in ME or later
   // as ME can flush the pipe rendering an update of state in EX
   // premature. Of course this leads to headaches with forwarding and
   // hazards on instruction depending on these... Sigh.
   reg                mult_busy = 0;
   reg [63:0]         mult_a = 0, mult_3a = 0;
   reg [31:0]         mult_b = 0;
   reg                mult_neg = 0;
   reg [31:0]         mult_lo = 0;
   reg [31:0]         mult_hi = 0;

   reg                div_busy = 0, div_neg_res, div_neg_rem;
   reg [31:0]         divisor = 0, div_hi = 0, div_lo = 0;
   reg [32:0]         diff = 0;
   reg [ 6:0]         div_n = 0;

   reg [31:0]         cp0_status = 0,     // XXX -- " --
                      cp0_epc = 0,
                      cp0_errorepc = 0,
                      cp0_cause = 0;

   arithshiftbidir arithshiftbidir(
        .distance(shift_dist),
        .data(d_op2_val),
        .direction(d_fn[1]),
        .result(ashift_out));

   logshiftright logshiftright(
        .distance(shift_dist),
        .data(d_op2_val),
        .result(lshift_out));

   reg x_has_delay_slot = 0;

   reg [31:0] tsc = 0; // Free running counter

   always @(posedge clock) begin
      tsc                <= tsc + 1;
      x_instr            <= d_instr;
      x_pc               <= d_pc;
      x_valid            <= d_valid;
      x_opcode           <= d_opcode;
      x_op1_val          <= d_op1_val;
      x_rt_val           <= d_rt_val;
      x_wbr              <= d_wbr;
      x_has_delay_slot   <= d_has_delay_slot & d_valid;
      x_is_delay_slot    <= x_has_delay_slot & x_valid;

      x_restart          <= d_restart;
      x_restart_pc       <= d_valid ? d_target : d_restart_pc;
      x_res              <= 'hDEADBEEF;
      x_flush_D          <= 0;

`define MULT_RADIX_4 1
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

      // XXX the use of non-blocking assignments here is intentional
      // (easier to read), but it has the unfortunate consequence of
      // making the final negation more expensive than it should have
      // been. Rework this.
      if (!div_n[6]) begin
         {div_hi,div_lo} = {div_hi,div_lo} << 1;
         diff = div_hi - divisor;
         if (!diff[32]) begin
            div_hi = diff;
            div_lo[0] = 1;
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
         `SLL: x_res <= ashift_out;
         `SRL: x_res <= lshift_out;
         `SRA: x_res <= ashift_out;

         `SLLV: x_res <= ashift_out;
         `SRLV: x_res <= lshift_out;
         `SRAV: x_res <= ashift_out;

         `JALR:
            begin
               x_res <= d_npc + 4;
               if (d_valid) begin
                  x_restart    <= 1;
                  x_restart_pc <= d_op1_val;
               end
            end
         `JR:
            if (d_valid) begin
               x_restart    <= 1;
               x_restart_pc <= d_op1_val;
            end



         // XXX BUG See the comment above with mult_lo and mult_hi
         `MFHI:
            if ((mult_busy | div_busy) && d_valid) begin
               x_flush_D    <= 1;
               x_valid      <= 0;
               x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
               x_restart    <= 1;
            end else
               x_res        <= mult_hi;
         `MFLO:
            if ((mult_busy | div_busy) && d_valid) begin
               x_flush_D    <= 1;
               x_valid      <= 0;
               x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
               x_restart    <= 1;
            end else
               x_res        <= mult_lo;
         `MTHI:
            if (d_valid) begin
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
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
               end else
                  mult_lo      <= d_op1_val;
            end

         `DIV:
            if (d_valid) begin
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
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
            end

         `DIVU:
            if (d_valid) begin
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
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
            end

         `MULTU:
            if (d_valid) begin
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
               end else begin
                  $display("MULTU %x * %x", d_op1_val, d_op2_val);
                  mult_busy <= 1;
                  mult_hi <= 0;
                  mult_lo <= 0;
                  mult_a <= d_op1_val;
                  mult_b <= d_op2_val;
                  mult_3a <= 3 * d_op1_val;
                  mult_neg <= 0;

                  $display("%05dc EX: %dU * %dU", $time, d_op1_val, d_op2_val);
                end
            end

         `MULT:
            if (d_valid) begin
               if (mult_busy | div_busy) begin
                  x_flush_D    <= 1;
                  x_valid      <= 0;
                  x_restart_pc <= d_pc - {x_has_delay_slot,2'd0};
                  x_restart    <= 1;
               end else begin
                  $display("MULT %x * %x", d_op1_val, d_op2_val);
                  mult_busy <= 1;
                  mult_hi <= 0;
                  mult_lo <= 0;
                  mult_neg <= d_op1_val[31] ^ d_op2_val[31];
                  mult_a <= d_op1_val[31] ? {32'd0,32'd0 - d_op1_val} : d_op1_val;
                  mult_3a <= d_op1_val[31] ? 3 * {32'd0,32'd0-d_op1_val} : 3 * d_op1_val;
                  mult_b <= d_op2_val[31] ? 32'd0 - d_op2_val  : d_op2_val;
                  $display("%05dc EX: %d * %d", $time, d_op1_val, d_op2_val);
                end
            end

         // XXX BUG Trap on overflow for ADD, ADDI and SUB
         `ADD:    x_res <= d_op1_val + d_op2_val;
         `ADDU:   x_res <= d_op1_val + d_op2_val;
         `SUB:    x_res <= d_op1_val - d_op2_val;
         `SUBU:   x_res <= d_op1_val - d_op2_val;
         `AND:    x_res <= d_op1_val & d_op2_val;
         `OR:     x_res <= d_op1_val | d_op2_val;
         `XOR:    x_res <= d_op1_val ^ d_op2_val;
         `NOR:    x_res <= d_op1_val | ~d_op2_val;

         `SLT:    x_res <= {{31{1'b0}}, subtracted[32]};
         `SLTU: if (d_op1_val < d_op2_val) x_res <= 1; else x_res <= 0;
         `BREAK:
            if (d_valid) begin
               x_restart    <= 1;
               x_restart_pc <= 'hBFC00380;
               x_flush_D    <= 1;
               cp0_status[`CP0_STATUS_EXL] <= 1;
               //cp0_cause.exc_code = EXC_BP;
               cp0_cause <= 9 << 2;
               // cp0_cause.bd = branch_delay_slot; // XXX DELAY SLOT HANDLING!
               cp0_epc <= d_pc; // XXX DELAY SLOT HANDLING!
            end

`ifdef SIMULATE_MAIN
         default:
            $display("%05dc EX: %8x:%8x unhandled REG function %x", $time,
                     d_pc, d_instr, d_fn);
`endif
         endcase
      `REGIMM: // BLTZ, BGEZ, BLTZAL, BGEZAL
         if (d_valid) begin
            x_restart <= d_rt[0] ^ d_op1_val[31];
            x_res  <= d_npc + 4;
         end
      `JAL:
         if (d_valid) begin
            x_restart <= 1;
            x_res  <= d_npc + 4;
         end
      `J: if (d_valid) x_restart <= 1;
      `BEQ:
         if (d_valid) begin
            x_restart <=  ops_eq;
            $display("%05d BEQ %8x == %8x (%1d)", $time,
                     d_op1_val, d_op2_val, ops_eq);
         end
      `BNE:
         if (d_valid) begin
            x_restart <= ~ops_eq;
            $display("%05d BNE %8x == %8x (%1d)", $time,
                     d_op1_val, d_op2_val, ops_eq);
         end

      `BLEZ:
         if (d_valid)
            x_restart <= d_op1_val[31] || d_op1_val == 0;
      `BGTZ:
         // XXX Share logic
         if (d_valid)
            x_restart <= !d_op1_val[31] && d_op1_val != 0;

      `ADDI: x_res <= d_op1_val + d_op2_val;
      `ADDIU:x_res <= d_op1_val + d_op2_val;
      `SLTI: x_res <= $signed(d_op1_val) < $signed(d_op2_val);
      `SLTIU:x_res <= d_op1_val < d_op2_val;
      `ANDI: x_res <= {16'b0, d_op1_val[15:0] & d_op2_val[15:0]};
      `ORI:  x_res <= {d_op1_val[31:16], d_op1_val[15:0] | d_op2_val[15:0]};
      `XORI: x_res <= {d_op1_val[31:16], d_op1_val[15:0] ^ d_op2_val[15:0]};
      `LUI:  x_res <= {d_op2_val[15:0], 16'd0};

      //`CP1:
`ifdef SIMULATE_MAIN
      `CP2:
         if (d_valid) begin
            if (mult_lo == 32'h87654321)
               $display("TEST SUCCEEDED!");
            else
               $display("%05d TEST FAILED WITH %x  (%1d:%8x:%8x)", $time, mult_lo,
                        d_valid, d_pc, d_instr);
            $finish; // XXX do something more interesting for real hw.
         end
`endif
      `RDHWR:
         if (d_fn == 59)
            case (d_rd)
            0: x_res <= 0;
            1: x_res <= 4 << IC_WORD_INDEX_BITS;
            2: x_res <= tsc;
            3: x_res <= 1;
            endcase

      //`BBQL:

/*
  These are handled in Stage_ME

      `LB:   x_res <= d_op1_val + d_op2_val;
      `LBU:  x_res <= d_op1_val + d_op2_val;
      `LH:   x_res <= d_op1_val + d_op2_val;
      `LHU:  x_res <= d_op1_val + d_op2_val;
      `LW:   x_res <= d_op1_val + d_op2_val;
      `SB:   x_res <= d_op1_val + d_op2_val;
      `SH:   x_res <= d_op1_val + d_op2_val;
      `SW:   x_res <= d_op1_val + d_op2_val;
*/

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
            end else
               case (d_rd)
               `CP0_STATUS:   x_res <= cp0_status;   // 12
               `CP0_CAUSE:    x_res <= cp0_cause;    // 13
               `CP0_EPC:      x_res <= cp0_epc;      // 14
               `CP0_ERROREPC: x_res <= cp0_errorepc; // 30
               default:
                  $display("Accessing an unknown CP0 register %d", d_rd);
               endcase
         end
      end
`endif
      endcase

      if (d_valid) begin
         if (x_valid
             && (d_rs == x_wbr || d_rt == x_wbr)
             && x_opcode[5:3] == 4)
            begin
               x_restart <= 1;
               x_flush_D <= 1;
               x_valid   <= 0;
               x_restart_pc <= d_pc; // Notice, we know that EX had a
               // load, thus DE isn't a delay slot
               $display("%05d  *** load-use hazard, restarting %8x", $time,
                        d_pc);
            end
      end
   end
endmodule

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
              ,output reg  [31:0] x_instr         = 0   // XXX for debugging only
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

              ,output wire [ 7:0] debug_byte
              );

   parameter debug = 0;

   wire               ops_eq = d_op1_val == d_op2_val;
   wire [32:0]        subtracted = {d_op1_val[31],d_op1_val} - {d_op2_val[31],d_op2_val};

   wire [4:0]         shift_dist = d_fn[2] ? d_op1_val[4:0] : d_sa;
   wire [31:0]        ashift_out;
   wire [31:0]        lshift_out;

   // XXX BUG These architectural registers must live in ME or later
   // as ME can flush the pipe rendering an update of state in EX
   // premature. Of course this leads to headaches with forwarding and
   // hazards on instruction depending on these... Sigh.
   reg [31:0]         x_lo = 0, x_hi = 0;

   reg [31:0]         cp0_status = 0,     // XXX -- " --
                      cp0_epc = 0,
                      cp0_errorepc = 0,
                      cp0_cause = 0;

   assign             debug_byte = x_lo[7:0];

   arithshiftbidir arithshiftbidir(
        .distance(shift_dist),
        .data(d_op2_val),
        .direction(d_fn[1]),
        .result(ashift_out));

   logshiftright logshiftright(
        .distance(shift_dist),
        .data(d_op2_val),
        .result(lshift_out));

   /*
   wire               d_mult_strobe =
                      d_opcode == `REG && (d_fn == `MULT || d_fn == `MULTF);

   mult mult(.clock(clock),

             .op1 (d_op1_val),
             .op2 (d_op2_val),
             .go  (d_mult_strobe),

             .hold(x_mult_hold),
             .res (x_mult_res));
    */

   // Simulate FU busy conditions
   reg [32:0] lfsr = 0;
   always @(posedge clock)
     lfsr <= {lfsr[31:0], ~lfsr[32] ^ lfsr[19]};
 //  assign x_hazard = 0 & ~lfsr[31];

   reg x_has_delay_slot = 0;

   always @(posedge clock) begin
      x_instr            <= d_instr;
      x_pc               <= d_pc;
      x_valid            <= d_valid;
      x_opcode           <= d_opcode;
      x_op1_val          <= d_op1_val;
      x_rt_val           <= d_rt_val;
      x_wbr              <= d_wbr;
      x_has_delay_slot   <= d_has_delay_slot;
      x_is_delay_slot    <= x_has_delay_slot;

      x_restart          <= d_restart;
      x_restart_pc       <= d_valid ? d_target : d_restart_pc;
      x_res              <= 'hDEADBEEF;
      x_flush_D          <= 0;


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

         // XXX BUG See the comment above with x_lo and x_hi
         `MFHI: begin x_res <= x_hi; end
         `MFLO: begin x_res <= x_lo; end
         `MTHI: begin x_hi <= d_op1_val; end
         `MTLO: begin x_lo <= d_op1_val; end

         //`MULTU: reg_mul.u64 = (u_int64_t)s * (u_int64_t)t;
         //`DIV:   reg_mul.u32[1] = (int)s % (int)t;
         //        reg_mul.u32[0] = (int)s / (int)t; break;
         //`DIVU:  reg_mul.u32[1] = s % t;
         //        reg_mul.u32[0] = s / t; break;

/*
         `MULT: begin
                  {x_hi,x_lo} <= d_op1_val * d_op2_val;
                  $display("%05dc EX: %d * %d", $time, d_op1_val, d_op2_val);
                end
*/
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
            if (x_lo == 32'h87654321)
               $display("TEST SUCCEEDED!");
            else
               $display("%05d TEST FAILED WITH %x  (%1d:%8x:%8x)", $time, x_lo,
                        d_valid, d_pc, d_instr);
            $finish; // XXX do something more interesting for real hw.
         end
`endif
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

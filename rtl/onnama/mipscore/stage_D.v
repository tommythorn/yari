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

`include "mips_asm.v"

/*

 The decoding stage is relatively simple.  It takes an instruction
 and summerizes the different aspects.  The most important task in
 this stage is the fetch the value of the operands from the register
 file and to note if they need to be bypassed (forwarded from later
 stages).

 XXX Bypassing: I could
  1. track the destination register of the following stages
     and just export steering wires
  2. import the destination registers
  3. ignore bypassing here and let the next stage deal
 So far I go with 3. as this stage is already big enough as is.

        0       1       2       3       4       5       6       7
0       REG     REGIMM  J       JAL     BEQ     BNE     BLEZ    BGTZ
1       ADDI    ADDIU   SLTI    SLTIU   ANDI*   ORI*    XORI*   LUI*
2       CP0     CP1     CP2             BBQL                    BGTZL
3
4       LB      LH      LWL     LW      LBU     LHU     LWR
5       SB      SH      SWL     SW                      SWR
6
7

*/

`timescale 1ns/10ps
module stage_D(input  wire        clk
              ,input  wire        stall
              ,input  wire        flush

              ,input  wire        i_valid       // 0 => ignore i_instr.
              ,input  wire [31:0] i_instr       // Current instr.
              ,input  wire [31:0] i_pc          // Addr of current instr
              ,input  wire [31:0] i_npc         // Addr of next instr

               // Hazard detection/forwarding
              ,input  wire [ 5:0] x_wbr
              ,input  wire [31:0] x_res
              ,input  wire [ 5:0] m_wbr
              ,input  wire [31:0] m_res

              ,output reg         d_valid         = 0
              ,output reg         d_illegal_instr = 0 // XXX Must trap on this
              ,output reg  [31:0] d_pc            = 0
              ,output reg  [31:0] d_instr         = 0
              ,output reg  [31:0] d_npc           = 0
              ,output reg  [ 5:0] d_opcode        = 0
              ,output reg  [ 5:0] d_fn            = 0
              ,output reg  [ 4:0] d_rd            = 0
              ,output reg  [ 5:0] d_rs            = 0
              ,output reg  [ 5:0] d_rt            = 0
              ,output reg  [ 4:0] d_sa            = 0
              ,output reg  [31:0] d_op1_val       = 0   // aka d_rs_val
              ,output reg  [31:0] d_op2_val       = 0
              ,output reg  [31:0] d_rt_val        = 0   // For stores
              ,output reg  [ 5:0] d_wbr           = 0
              ,output reg  [31:0] d_target        = 0

              ,output wire        d_hazzard   // ASYNC
              );
   parameter   debug = 0;

   reg  [31:0] regs_A [31:0]; // Initialization is handled below
   reg  [31:0] regs_B [31:0]; // Initialization is handled below

   // Name various instruction fields
   wire [ 5:0] i_opcode;
   wire [ 5:0] i_rs, i_rt;
   wire [ 4:0] i__rs, i__rt, i_rd;
   wire [ 4:0] i_sa;
   wire [ 5:0] i_fn;
   assign {i_opcode,i__rs,i__rt,i_rd,i_sa,i_fn} = i_valid ? i_instr : 'h0;
   assign i_rs = {1'b1,i__rs}; // Bit 5 means valid.
   assign i_rt = {1'b1,i__rt};

   wire [25:0] i_offset = i_valid ? i_instr[25:0] : 'h0;

   // Sign-extend immediate field
   wire [31:0] i_simm = i_valid ? {{16{i_instr[15]}}, i_instr[15:0]} : 'h0;

   /*
    Write-After-Read hazard detection: detect if the register value fetched
    and made available in the next cycle is valid.

    Fx. if d_wbr is the same as i_rs that means that the instruction
    preceeding us is writing a register we're using, a write that won't be
    commited until the writeback stage (stage W). Bypassing/forwarding is not
    yet implemented, but if it and and we knew that the preceeding instruction
    is purly computational (eg. non-load), then we could proceed and instruct
    stage X to take the operand from it's own result register (x_res).

    If x_wbr == i_rs then we could certainly forward from m_res in the next
    cycle.

    If m_wbr == i_rs then we need not forward if the register file is
    flow-through (that is when writing a cell and reading the same cell in the
    same cycle results in the new value written and not the old value).

    XXX Should hazzard detection be moved outside stage D?
    XXX the w_wbr isn't needed if the register file is the flow-through kind
    (which Altera RAM is and probably also Xilinx).
    XXX No bypassing implemented intentionally; I want to have all the
    hazzard detection implemented and well-tested first.
    */
   wire  [3:0] i_WAR1 = {i_rs == d_wbr, i_rs == x_wbr, i_rs == m_wbr};
   wire  [3:0] i_WAR2 = {i_rt == d_wbr, i_rt == x_wbr, i_rt == m_wbr};
   assign      d_hazzard = |i_WAR1 | |i_WAR2;

   wire [31:0] i_branch_target = i_npc + {i_simm[29:0], 2'h0};
   wire [31:0] i_jump_target   = {i_npc[31:28],i_offset,2'h0};


   // XXX Just for debugging
   reg  [ 3:0] d_WAR1 = 0, d_WAR2 = 0;

   wire [31:0] i_rt_val = m_wbr == i_rt ? m_res : regs_B[i_rt[4:0]];
   wire [31:0] i_op2_val = (i_opcode[5:3] == 1 || /* Immediate instructions */
                            i_opcode[5:3] == 4 || /* Loads */
                            i_opcode[5:3] == 5    /* Stores */
                            ? i_simm : i_rt_val);

   always @(posedge clk) begin
      /*
       * Writeback. (Not flow-through?)  Notice, this is really the WB stage
       * and as such is not affected stalling of this stage.  XXX Actually it
       * should have it own stalling.  I think we're safe to ignore this for now.
       */
      if (m_wbr[5]) begin
        regs_A[m_wbr[4:0]] <= m_res;
        regs_B[m_wbr[4:0]] <= m_res;
      end

`ifdef SIMULATE_MAIN
      if (m_wbr == 32) $display("%5db DE: Trying to write $0!!", $time);

      if (debug) begin
         $display("%5db DE: instr %8x stall %d valid %d (m_wbr:%2x) (i_npc %8x i_offset*4 %8x target %8x)",
                  $time, d_instr, stall, d_valid,   m_wbr,
                  i_npc, i_offset << 2, i_jump_target);

         if (0)
         $display("%5db DE:   %x %x %x %x %x %x %x %x %x %x %x %x ",
                  $time,
                  regs[0], regs[1], regs[2], regs[3],
                  regs[4], regs[5], regs[6], regs[7],
                  regs[8], regs[9], regs[10], regs[11]);
         if (1)
         $display("%5db DE:   %x %x %x",
                  $time,
                  d_target, i_branch_target, i_jump_target);


         if (d_hazzard)
           $display("%5db DE: HAZZARD rs:%x, rt:%x d_WAR1 %d d_WAR2 %d (%x %x %x)",
                    $time, i_rs, i_rt, i_WAR1, i_WAR2,
                    d_wbr, x_wbr, m_wbr);
      end
`endif

      if (flush) begin
         d_valid <= 0;
         d_wbr <= 0;
         d_pc <= 0; // XXX Expensive and not really needed?
         d_npc <= 0; // XXX Expensive and not really needed?
         d_instr <= 0; // XXX Expensive and not really needed?
         d_opcode <= 0;// XXX Expensive and not really needed?
         d_fn <= 0; // XXX Expensive and not really needed?
      end else if (~stall) begin
         d_valid   <= ~d_hazzard;
         d_npc     <= i_npc;
         d_pc      <= i_pc;

         // Name various instruction fields
         {d_opcode,d_rs,d_rt,d_rd,d_sa,d_fn} <= {i_opcode,i_rs,i_rt,i_rd,i_sa,i_fn};

         // Register file access
         // Simulate a flow-through regfile
         d_op1_val <= m_wbr == i_rs ? m_res : regs_A[i_rs[4:0]];
         d_op2_val <= i_op2_val;
         d_rt_val  <= i_rt_val;

         // Determine write-back register.  We set this to 32 for unrecognized
         // instructions so avoid unintended effects.
         case ({d_hazzard,i_opcode[5:3]})
           0: case (i_opcode[2:0])
                `REG:    d_wbr <= {|i_rd[4:0],i_rd[4:0]};
                // XXX I assume nothing is written if the condition is not met
                `REGIMM: d_wbr <= {6{i_rt[4]}};// d_rt == `BLTZAL || d_rt == `BGEZAL ? 31 : 0;
                `JAL:    d_wbr <= 32+31;
                default: d_wbr <= 0; // no writeback
              endcase
           1:       d_wbr <= {|i_rt[4:0],i_rt[4:0]}; // Immediate instructions
           2:       if (i_opcode == `CP0 && ~i_rs[4] && ~i_rs[2])
                      d_wbr <= {|i_rt[4:0],i_rt[4:0]};  // MTCP0
                    else
                      d_wbr <= 0;
           4:       d_wbr <= {|i_rt[4:0],i_rt[4:0]}; // Loads
           default: d_wbr <= 0;
         endcase

         // Calculate branch targets
         case (i_opcode)
           `REGIMM: d_target <= i_branch_target;
           `BEQ:    d_target <= i_branch_target;
           `BNE:    d_target <= i_branch_target;
           `BLEZ:   d_target <= i_branch_target;
           `BGTZ:   d_target <= i_branch_target;
           `JAL:    d_target <= i_jump_target;
           `J:      d_target <= i_jump_target;
           default: d_target <= debug ? i_pc : 32'hxxxxxxxx;
         endcase

         // We use d_illegal_instr to mark the instructions that we don't support
         case (i_opcode)
           `REG: case (i_fn)
                   `SLL:    d_illegal_instr <= 0;
                   `SRL:    d_illegal_instr <= 0;
                   `SRA:    d_illegal_instr <= 0;
                   `SLLV:   d_illegal_instr <= 0;
                   `SRLV:   d_illegal_instr <= 0;
                   `SRAV:   d_illegal_instr <= 0;
                   `JALR:   d_illegal_instr <= 0;
                   `JR:     d_illegal_instr <= 0;
                   `MFHI:   d_illegal_instr <= 1;
                   `MTHI:   d_illegal_instr <= 1;
                   `MFLO:   d_illegal_instr <= 1;
                   `MTLO:   d_illegal_instr <= 1;
                   `MULT:   d_illegal_instr <= 1;
                   `MULTU:  d_illegal_instr <= 1;
                   `DIV:    d_illegal_instr <= 1;
                   `DIVU:   d_illegal_instr <= 1;
                   `ADD:    d_illegal_instr <= 1;
                   `ADDU:   d_illegal_instr <= 0;
                   `SUB:    d_illegal_instr <= 1;
                   `SUBU:   d_illegal_instr <= 0;
                   `AND:    d_illegal_instr <= 0;
                   `OR:     d_illegal_instr <= 0;
                   `XOR:    d_illegal_instr <= 0;
                   `NOR:    d_illegal_instr <= 0;
                   `SLT:    d_illegal_instr <= 0;
                   `SLTU:   d_illegal_instr <= 0;
                   default: d_illegal_instr <= 1;
                 endcase
           // XXX I assume nothing is written if the condition is not met
           `REGIMM: d_illegal_instr <= 0;
           `JAL:    d_illegal_instr <= 0;
           `J:      d_illegal_instr <= 0;
           `BEQ:    d_illegal_instr <= 0;
           `BNE:    d_illegal_instr <= 0;
           `BLEZ:   d_illegal_instr <= 0;
           `BGTZ:   d_illegal_instr <= 0;
           `ADDI:   d_illegal_instr <= 1;
           `ADDIU:  d_illegal_instr <= 0;
           `SLTI:   d_illegal_instr <= 0;
           `SLTIU:  d_illegal_instr <= 0;
           `ANDI:   d_illegal_instr <= 0;
           `ORI:    d_illegal_instr <= 0;
           `XORI:   d_illegal_instr <= 0;
           `LUI:    d_illegal_instr <= 0;
           `CP0:    d_illegal_instr <= 0; // Supported == ignored
           // `CP1:
           `CP2:    d_illegal_instr <= 0; // Supported == ignored
           // `BBQL:
           `LB:     d_illegal_instr <= 0;
           `LBU:    d_illegal_instr <= 0;
           `LH:     d_illegal_instr <= 0;
           `LHU:    d_illegal_instr <= 0;
           `LW:     d_illegal_instr <= 0;
           `SB:     d_illegal_instr <= 0;
           `SH:     d_illegal_instr <= 0;
           `SW:     d_illegal_instr <= 0;
           default: d_illegal_instr <= 1;
         endcase // case (i_opcode)

         // XXX Just for debugging
         d_instr   <= i_valid ? i_instr : 'h0;
         {d_WAR1,d_WAR2} <= {i_WAR1,i_WAR2};
      end
   end

`ifdef SIMULATE_MAIN
   wire [31:0] debug_regs_rs = regs_A[i_rs[4:0]];
   wire [31:0] debug_regs_rt = regs_B[i_rt[4:0]];

   reg [31:0]  i;
   initial begin
      for (i = 0; i < 32; i = i + 1) begin
         regs_A[i] = 0;
         regs_B[i] = 0;
      end
      if (debug && 0)
         $monitor("%05db DE [MON]: m_wbr %x m_res %x i_instr %x i_rs %x i_rt %x regs[i_rs[4:0]] %x regs[i_rt[4:0]] %x op2 %x target %x",
                  $time,
                  m_wbr, m_res,
                  i_valid ? i_instr : 'h0, i__rs, i__rt, debug_regs_rs, debug_regs_rt,
                  d_op2_val, d_target);
   end
`endif
endmodule // stage_D

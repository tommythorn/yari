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

/*
 The decoding stage is relatively simple.  It takes an instruction
 and summerizes the different aspects.  The most important task in
 this stage is the fetch the value of the operands from the register
 file and to forwarded from later stages if needed.
*/

module stage_D(input  wire        clock

              ,input  wire        i_valid       // 0 => ignore i_instr.
              ,input  wire [31:0] i_instr       // Current instr.
              ,input  wire [31:0] i_pc          // Addr of current instr
              ,input  wire [31:0] i_npc         // Addr of next instr

               // Forwarding
              ,input  wire        x_valid
              ,input  wire [ 5:0] x_wbr
              ,input  wire [31:0] x_res

              ,input  wire [31:0] m_pc
              ,input  wire        m_valid
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
              ,output reg  [31:0] d_target        = 0
              ,output reg  [ 5:0] d_wbr           = 0
              ,output reg         d_has_delay_slot= 0

              ,output wire [31:0] d_op1_val             // aka d_rs_val
              ,output wire [31:0] d_op2_val
              ,output wire [31:0] d_rt_val              // For stores
              ,output reg  [31:0] d_simm          = 0

              ,output reg         d_restart       = 0
              ,output reg  [31:0] d_restart_pc    = 0
              ,output reg         d_flush_X       = 0

              ,output reg         d_load_use_hazard = 0

              ,input  wire        flush_D
              ,output reg  [31:0] perf_delay_slot_bubble = 0
              ,output reg  [47:0] perf_retired_inst = 0

              // For debugging only
              ,output wire        i_valid_muxed
              ,output wire [31:0] i_pc_muxed
              ,output wire [31:0] i_instr_muxed
              );

   parameter debug = 0;

   // Name various instruction fields
   wire [ 5:0] i_opcode;
   wire [ 5:0] i_rs, i_rt;
   wire [ 4:0] i__rs, i__rt, i_rd;
   wire [ 4:0] i_sa;
   wire [ 5:0] i_fn;

   /*
    * When we restart instructions from DE due to load-use hazards,
    * reuse the already seen instruction rather than wait for it to
    * tickle down from IF.
    */
   assign i_valid_muxed = i_valid | d_load_use_hazard;
   assign i_pc_muxed    = d_load_use_hazard ? d_pc    : i_pc;
   assign i_instr_muxed = d_load_use_hazard ? d_instr : i_instr;
   wire [31:0] i_npc_muxed = d_load_use_hazard ? d_npc   : i_npc;

   assign {i_opcode,i__rs,i__rt,i_rd,i_sa,i_fn} = i_instr_muxed;
   assign i_rs = {1'b1,i__rs}; // Bit 5 means valid.
   assign i_rt = {1'b1,i__rt};

   wire [25:0] i_offset = i_instr_muxed[25:0];

   // Sign-extend immediate field
   wire [31:0] i_simm = {{16{i_instr_muxed[15]}}, i_instr_muxed[15:0]};

   wire [31:0] i_branch_target = i_npc_muxed + {i_simm[29:0], 2'h0};
   wire [31:0] i_jump_target   = {i_npc_muxed[31:28],i_offset,2'h0};

   always @(posedge clock) d_simm <= i_simm;

   reg d_op2_is_imm = 0;
   always @(posedge clock)
      d_op2_is_imm <= (i_opcode[5:3] == 1 || /* Immediate instructions */
                       i_opcode[5:3] == 4 || /* Loads */
                       i_opcode[5:3] == 5);  /* Stores */

   // Register file
   reg  [31:0] regs_A [31:0]; // Initialization is handled below
   reg  [31:0] regs_B [31:0]; // Initialization is handled below

   reg  [31:0] rs_reg_val, rt_reg_val;

   always @(posedge clock) begin
      if (m_valid & m_wbr[5])
        regs_A[m_wbr[4:0]] <= m_res;
      rs_reg_val <= regs_A[i_rs[4:0]];
   end

   always @(posedge clock) begin
      if (m_valid & m_wbr[5])
        regs_B[m_wbr[4:0]] <= m_res;
      rt_reg_val <= regs_B[i_rt[4:0]];
   end

   // Stage WB is only present here for bypass
   reg         w_valid = 0;
   reg  [31:0] w_res = 0;
   reg [ 5:0]  w_wbr = 0;
   always @(posedge clock) begin
      w_valid <= m_valid;
      w_wbr   <= m_wbr;
      w_res   <= m_res;
   end

   wire        d_forward_x_to_s = x_valid && d_rs == x_wbr;
   wire        d_forward_x_to_t = x_valid && d_rt == x_wbr;
   wire        d_forward_m_to_s = m_valid && d_rs == m_wbr;
   wire        d_forward_m_to_t = m_valid && d_rt == m_wbr;
   wire        d_forward_w_to_s = w_valid && d_rs == w_wbr;
   wire        d_forward_w_to_t = w_valid && d_rt == w_wbr;

   assign d_op1_val = (d_forward_x_to_s ? x_res :
                       d_forward_m_to_s ? m_res :
                       d_forward_w_to_s ? w_res :
                       rs_reg_val);
   assign d_rt_val  = (d_forward_x_to_t ? x_res :
                       d_forward_m_to_t ? m_res :
                       d_forward_w_to_t ? w_res :
                       rt_reg_val);

   assign d_op2_val = d_op2_is_imm ? d_simm : d_rt_val;

   reg delay_slot_bubble = 0;
   always @(posedge clock) begin
      d_valid   <= i_valid_muxed;
      d_pc      <= i_pc_muxed;
      d_npc     <= i_npc_muxed;
      d_instr   <= i_instr_muxed;
      {d_opcode,d_rs,d_rt,d_rd,d_sa,d_fn} <= {i_opcode,i_rs,i_rt,i_rd,i_sa,i_fn};
      d_restart <= 0;
      d_flush_X <= 0;

      // Determine write-back register.  We set this to 0 for
      // unrecognized instructions so avoid unintended effects. (Valid
      // registers are remapped to 32 - 63 to avoid having to make a
      // special case for r0 in the bypass network.
      case (i_opcode[5:3])
      0: case (i_opcode[2:0])
         `REG:    d_wbr <= {|i_rd[4:0],i_rd[4:0]};
         `REGIMM:
            if (i__rt == `SYNCI)
               d_wbr <= 0;
            else
               d_wbr <= {6{i_rt[4]}};// d_rt == `BLTZAL || d_rt == `BGEZAL ? 31 : 0;
         `JAL:    d_wbr <= 6'd32+6'd31;
         default: d_wbr <= 0; // no writeback
         endcase
      1: d_wbr <= {|i_rt[4:0],i_rt[4:0]}; // Immediate instructions
      2: if ((i_opcode == `CP0 || i_opcode == `CP2) && ~i_rs[4] && ~i_rs[2])
            d_wbr <= {|i_rt[4:0],i_rt[4:0]};  // MTCP0
         else
            d_wbr <= 0;
      3: if (i_opcode == `RDHWR)
             d_wbr <= {|i_rt[4:0],i_rt[4:0]}; // RDHWR
      4: d_wbr <= {|i_rt[4:0],i_rt[4:0]}; // Loads
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
      default: d_target <= debug ? i_pc_muxed : 32'hxxxxxxxx;
      endcase

      // Detect control transfers
      d_has_delay_slot <= 0;
      case (i_opcode)
      `REG: case (i_fn)
            `JALR:   d_has_delay_slot <= 1;
            `JR:     d_has_delay_slot <= 1;
            endcase
      `REGIMM: if (i_rt[4:0] != `SYNCI) d_has_delay_slot <= 1;
      `BEQ:    d_has_delay_slot <= 1;
      `BNE:    d_has_delay_slot <= 1;
      `BLEZ:   d_has_delay_slot <= 1;
      `BGTZ:   d_has_delay_slot <= 1;
      `JAL:    d_has_delay_slot <= 1;
      `J:      d_has_delay_slot <= 1;
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
      endcase

      /*
       * Detect and restart upon delay-slot bubbles. The cost of this
       * is completely hidden by the I$ fill overhead.  The guarantee
       * that delay slots always follow immediately after their
       * preceeding instruction make correct handling of delayed
       * branches/jumps much simpler.
       *
       * Note, we are making an assumption on the I$ behaviour here,
       * that is, we're assuming it will (eventually) always be
       * possible to hit the two consecutive lines without a miss.
       * All normal caches behaves this way (even directly mapped),
       * but it may be possible to construct an odd cache that
       * doesn't have this property.
       */

      delay_slot_bubble <= 0;
      if (d_valid & ~flush_D & d_has_delay_slot & ~i_valid_muxed) begin
         $display("%05d  DE: *** Taken-branch w/bubble delay slot, restarting branch at %8x",
                  $time, d_pc);
         d_valid      <= 0;
         d_restart    <= 1;
         d_restart_pc <= d_pc;
         d_flush_X    <= 1;
         delay_slot_bubble <= 1;
      end

      // Delay the count one cycle to improve cycle time
      if (delay_slot_bubble)
         perf_delay_slot_bubble <= perf_delay_slot_bubble + 1;

      if (m_valid)
         perf_retired_inst <= perf_retired_inst + 1;

      d_load_use_hazard <= 0;
      if (i_valid_muxed &&
          d_valid &&
          d_opcode[5:3] == 4 &&                    // Load in stage DE
          (i_rs == d_wbr ||
           (i_rt == d_wbr && i_opcode[5:4] != 2))) // load/store forward t
         begin
            d_valid      <= 0;
            d_restart    <= 1;
            d_restart_pc <= i_npc_muxed; // Notice, we know that DE had a
            // load, thus IF isn't a delay slot
            d_load_use_hazard <= 1;
            $display("%05d  DE: *** load-use hazard, restarting %8x", $time,
                     i_pc_muxed);
      end
   end




`ifdef SIMULATE_MAIN
   always @(posedge clock) begin
      if (0)
         $display("%05d  d_op1_val (r%1d) %8x  d_op2_is_imm %1d ? d_simm %1d : d_rt_val (r%1d) %8x    (non fwd %8x %8x)    (d_forward_x_to_t %1d %1d %1d %1d)", $time,
                  d_rs[4:0], d_op1_val,
                  d_op2_is_imm, d_simm, d_rt[4:0], d_rt_val,
                  rs_reg_val, rt_reg_val,

                  d_forward_x_to_t,
                  x_valid, i_rt, x_wbr
                  );

      if (0) begin
         if (d_forward_x_to_s)
            $display("%05d  DE %8x: rs (r%1d) <- EX (%8x)", $time,
                     d_pc, x_wbr - 32, x_res);
         else if (d_forward_m_to_s)
            $display("%05d  DE %8x: rs (r%1d) <- ME (%8x)", $time,
                     d_pc, m_wbr - 32, m_res);

         if (d_forward_x_to_t)
            $display("%05d  DE %8x: rt (r%1d) <- EX (%8x)", $time,
                     d_pc, x_wbr - 32, x_res);
         else if (d_forward_m_to_t)
            $display("%05d  DE %8x: rt (r%1d) <- ME (%8x)", $time,
                     d_pc, m_wbr - 32, m_res);
      end

      // !!CAREFUL!! This line is being matched by the cosimulation,
      // so if anything is changed, then run_simple.c:get_rtl_commit()
      // must be adjusted accordingly.
      if (m_valid & m_wbr[5])
         $display("%05d  COMMIT                                             %8x:r%02d <- %8x",
                  $time, m_pc, m_wbr[4:0], m_res);

      if (debug) begin
         $display("%5db DE: instr %8x valid %d (m_wbr:%2x) (i_npc %8x i_offset*4 %8x target %8x)",
                  $time, i_instr_muxed, i_valid_muxed,   m_wbr,
                  i_npc_muxed, i_offset << 2, i_jump_target);

         if (0)
            $display("%5db DE:   %x %x %x %x %x %x %x %x %x %x %x %x ", $time,
                     regs[0], regs[1], regs[2], regs[3],
                     regs[4], regs[5], regs[6], regs[7],
                     regs[8], regs[9], regs[10], regs[11]);
      end
   end

   wire [31:0] debug_regs_rs = regs_A[i_rs[4:0]];
   wire [31:0] debug_regs_rt = regs_B[i_rt[4:0]];
   reg  [31:0] i;
   initial
      for (i = 0; i < 32; i = i + 1) begin
         regs_A[i] = 0;
         regs_B[i] = 0;
      end
`endif
endmodule

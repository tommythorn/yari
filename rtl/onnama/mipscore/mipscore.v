`timescale 1ns/10ps
`include "mips_asm.v"
`include "pipeconnect.h"

`ifdef SIMULATE_MAIN
/* Conditional compilation removes a lot of annoying warnings during synthesis. */
module stallcheck(clk, stall, a);
   parameter which = "?";
   parameter id = -1;
   parameter w = 1;

   input     clk;
   input     stall;
   input [w-1:0] a;

   reg           stall_;
   reg [w-1:0]   a_;

   always @(posedge clk) begin
      {a_,stall_} <= {a,stall};
      if (stall_ && a != a_)
        $display("** %05d STALLCHECKER violation %d: was %x != now %x (%d)",
                 $time, id, a_, a, stall_);
   end
endmodule
`endif

module mipscore(input  wire        clk          // K5  PLL1 input clock (50 MHz)
               ,input  wire        rst

               ,output wire `REQ   imem_req
               ,input  wire `RES   imem_res

               ,output wire `REQ   dmem_req
               ,input  wire `RES   dmem_res

               ,output wire [31:0] debug_info // Can be used for anything.
               ,output wire [ 7:0] debug_byte);
   parameter debug = 1;

   wire          i_hazzard;
   wire          war_hazzard;
   wire          x_hazzard;
   wire          m_hazzard;

   wire          d_valid;
   wire          x_valid;
   wire          m_valid;

   wire          i_delayed;
   wire [31:0]   i_pc;
   wire [31:0]   i_npc;

   wire          i_valid;
   wire [31:0]   i_instr;

   wire [31:0]   d_instr;
   wire [31:0]   d_pc;
   wire [31:0]   d_npc;

   wire [ 5:0]   d_opcode;
   wire [ 5:0]   d_fn;
   wire [ 4:0]   d_rd;
   wire [ 5:0]   d_rs;
   wire [ 5:0]   d_rt;
   wire [ 4:0]   d_sa;
   wire [31:0]   d_op1_val;
   wire [31:0]   d_op2_val;
   wire [31:0]   d_rt_val;
   wire [ 5:0]   d_wbr;
   wire [31:0]   d_target;


   wire [31:0]   x_pc;
   wire [32:0]   x_instr;
   wire [ 6:0]   x_opcode;
   wire [31:0]   x_op1_val;
   wire [31:0]   x_rt_val;
   wire [ 5:0]   x_wbr;
   wire [31:0]   x_res;
   wire          x_jump;
   wire          x_annul_delay_slot;
   wire [31:0]   x_target;

   wire [32:0]   m_instr;
   wire [31:0]   m_pc;
   wire [ 5:0]   m_wbr;
   wire [31:0]   m_res;

   /* EVERYTHING BELOW HASN'T BEEN UPDATED.  PLEASE IGNORE THE
    RAMBLING.  THE TABLE SHOULD BE CORRECT HOWEVER!

    Pipeline hazzards and their stalling/flushing behaviour.

    There are five hazzards that can disrupt the flow of our pipeline:

    - BRANCH_haz: Taken branch control hazzard, detected at the exit
      from EX / *before* ME.

    - WAR_haz: WAR data hazzard, detected *before* DE.

    - EX_haz: Functional unit busy structural hazzard, detected
      *before* EX.

    - IF_haz & ME_haz: Memory busy structural hazzard, detected
      *before* IF and ME respectively.

    Although at first this looks like a 2^5 state space (not including
    dependencies on the past), we can bring this number down a lot by
    ordering the hazzards.  By chosing (*) to unconditionally freeze
    preceeding pipeline stages on ME and EX structural hazzards, and
    freezing DE and IF on IF hazzards, we reduce the cases to consider
    to just: ME_haz, EX_haz, IF_haz, WAR_haz, branch_haz, and WAR_haz
    & BRANCH_haz.  The freezing behaviour imposes a natural order

      ME_haz > EX_haz > IF_haz > {WAR_haz, BRANCH_haz}

    that is, we only ever need to consider the highest priority
    hazzard.

    (*) A more complicated option is to only freeze if the preceeding
    has a real instruction (ie. non-bubble and non-NOP).

    The final two WAR_haz and BRANCH_haz are less obvious and requires
    us to consider the handling of these three cases:

    - Just BRANCH_haz: the instruction fetched follows the branch
      delay slot and must be annulated (flush IF) *unless* the
      previous cycle flushed DE (and thus inserted a bubble between
      the branching instruction and its delay slot).

    - Just WAR_haz: DE must wait for this hazzard to clear before being able
      to pass on the looked up register operands, thus it passes a bubble to
      EX (flushes DE) while freezing IF.

    - Both WAR_haz and BRANCH_haz: There are two cases: 1) The
      previous cycle stalled I, thus the WAR_haz is from a valid delay
      slot instruction, and 2) otherwise the WAR_haz can be ignored.

    As it turns out, this case behaves exactly like a BRANCH_haz
    alone, thus we can safely order BRANCH_haz > WAR_haz.

    The preceeding discussion is summarized in table below (again notice that
    this is conservative - we stall as much as we can get away with!):

      Stages       | WB | ME | EX | DE | IF  |
      -------------+----+----+----+----+-----+
      ME_haz       | S  | S* | S  | S  | S   | Freeze
      -------------+----+----+----+----+-----+
      EX_haz       | S  | S  | S* | S  | S   | Freeze
      -------------+----+----+----+----+-----+
      IF_haz       | -  | -  | F  | S  | S*  | Insert bubble in EX
      -------------+----+----+----+----+-----+
      BRANCH_haz   | -  | -  | -  | F  | F   | Flush
      -------------+----+----+----+----+-----+
      BRANCH_haz'  | -  | -  | -  | F  | S   | Insert bubble in DE (stalled)
      -------------+----+----+----+----+-----+
      WAR_haz      | -  | -  | -  | F  | S   | Insert bubble in DE
      -------------+----+----+----+----+-----+


      Legend: F - Flush (insert a bubble)
              S - Stall (freeze stage)
              S* - Implicitly stall (The stage does it itself already and
                   forcing it causes problems)
   */

   reg flush_I = 0;
   reg flush_D = 0;
   reg flush_X = 0;
   reg flush_M = 0; // Not used
   reg stall_I = 0;
   reg stall_D = 0;
   reg stall_X = 0;
   reg stall_M = 0;
   reg stall_W = 0;
   reg stall_I_= 0; // Previous value of stall_I

   always @(posedge clk) stall_I_ <= stall_I;

   wire pure_branch = branch_hazzard & ~stall_I_;
   wire war         = branch_hazzard &  stall_I_ | war_hazzard;

   always @* casex ({m_hazzard,x_hazzard,i_hazzard,pure_branch,war})
   5'b1XXXX: {stall_W,stall_M,stall_X,stall_D,stall_I} = 5'b10111; // 17
   5'b01XXX: {stall_W,stall_M,stall_X,stall_D,stall_I} = 5'b11011; // 1B
   5'b001XX: {stall_W,stall_M,stall_X,stall_D,stall_I} = 5'b00010; // 02
   5'b0001X: {stall_W,stall_M,stall_X,stall_D,stall_I} = 5'b00000; // 00  BRANCH
   5'b00001: {stall_W,stall_M,stall_X,stall_D,stall_I} = 5'b00001; // 01  WAR
   5'b00000: {stall_W,stall_M,stall_X,stall_D,stall_I} = 5'b00000; // 00
   endcase
   always @* casex ({m_hazzard,x_hazzard,i_hazzard,pure_branch,war})
   5'b1XXXX: {flush_M,flush_X,flush_D,flush_I} = 0;
   5'b01XXX: {flush_M,flush_X,flush_D,flush_I} = 0;
   5'b001XX: {flush_M,flush_X,flush_D,flush_I} = 4'b0100;
   5'b0001X: {flush_M,flush_X,flush_D,flush_I} = 4'b0011; // BRANCH
   5'b00001: {flush_M,flush_X,flush_D,flush_I} = 4'b0010; // WAR
   5'b0000X: {flush_M,flush_X,flush_D,flush_I} = 0;
   endcase

   assign debug_info = rst ? 32'h5555 : i_pc;

   reg [9:0] initialized = 0;
   always @(posedge clk) initialized <= {initialized[8:0],~rst};

   wire branch_hazzard = x_jump;

   stage_I stI(.clk(clk)
              ,.stall(stall_I)
              ,.flush(flush_I | x_annul_delay_slot)

              ,.imem_req(imem_req)
              ,.imem_res(imem_res)

              ,.jump(initialized[9] ? x_jump : 1)
              ,.jump_target(initialized[9] ? x_target : 'hBFC00000)

              // Outputs
              ,.i_valid(i_valid)
              ,.i_instr(i_instr)
              ,.i_pc(i_pc)
              ,.i_npc(i_npc)
              ,.i_hazzard(i_hazzard));

`ifdef SIMULATE_MAIN
   // Check stage_I respects stalling
   stallcheck #("i_npc",     2, 32) check2(clk, stall_I, i_npc);
   stallcheck #("i_instr",   3, 32) check3(clk, stall_I, i_valid ? i_instr : 'h0);
`endif

   stage_D stD(.clk(clk),
               .stall(stall_D),
               .flush(flush_D | x_annul_delay_slot),

               .i_valid(i_valid),
               .i_instr(i_instr),
               .i_pc(i_pc),
               .i_npc(i_npc),

               .x_wbr(x_wbr),
               .x_res(x_res),
               .m_wbr(m_wbr), // XXX Depends on M stalling the whole pipeline
               .m_res(m_res),

               // Outputs
               .d_valid(d_valid),
               .d_instr(d_instr),
               .d_pc(d_pc),
               .d_npc(d_npc),
               .d_opcode(d_opcode),
               .d_fn(d_fn),
               .d_rd(d_rd),
               .d_rs(d_rs),
               .d_rt(d_rt),
               .d_sa(d_sa),
               .d_op1_val(d_op1_val),
               .d_op2_val(d_op2_val),
               .d_rt_val(d_rt_val),
               .d_wbr(d_wbr),
               .d_target(d_target),
               .d_hazzard(war_hazzard)); // XXX update stage D

`ifdef SIMULATE_MAIN
   // Check stage_D respects stalling
   stallcheck #("d_valid",   4,  1) check05(clk, stall_D, d_valid);
   stallcheck #("d_npc",     5, 32) check06(clk, stall_D, d_npc);
   stallcheck #("d_opcode",  6,  6) check07(clk, stall_D, d_opcode);
   stallcheck #("d_fn",      7,  6) check08(clk, stall_D, d_fn);
   stallcheck #("d_rs",      8,  6) check09(clk, stall_D, d_rs);
   stallcheck #("d_rt",      9,  6) check10(clk, stall_D, d_rt);
   stallcheck #("d_sa",     10,  5) check11(clk, stall_D, d_sa);
   stallcheck #("d_op1_val",11, 32) check12(clk, stall_D, d_op1_val);
   stallcheck #("d_op2_val",12, 32) check13(clk, stall_D, d_op2_val);
   stallcheck #("d_rt_val", 13, 32) check14(clk, stall_D, d_rt_val);
   stallcheck #("d_wbr",    14,  6) check15(clk, stall_D, d_wbr);
   stallcheck #("d_target", 15, 32) check16(clk, stall_D, d_target);
`endif

   stage_X stX(.clk(clk),
               .stall(stall_X),
               .flush(flush_X | x_annul_delay_slot),

               .d_valid(d_valid),
               .d_instr(d_instr),
               .d_pc(d_pc),
               .d_npc(d_npc),
               .d_opcode(d_opcode),
               .d_fn(d_fn),
               .d_rd(d_rd),
               .d_rs(d_rs),
               .d_rt(d_rt),
               .d_sa(d_sa),
               .d_op1_val(d_op1_val),
               .d_op2_val(d_op2_val),
               .d_rt_val(d_rt_val),
               .d_wbr(d_wbr),
               .d_target(d_target),

               .x_valid(x_valid),
               .x_pc(x_pc),
               .x_instr(x_instr), // XXX for debugging only
               .x_opcode(x_opcode),
               .x_op1_val(x_op1_val),
               .x_rt_val(x_rt_val),
               .x_wbr(x_wbr),
               .x_res(x_res),
               .x_jump(x_jump),
               .x_annul_delay_slot(x_annul_delay_slot),
               .x_target(x_target),
               .x_hazzard(x_hazzard),

               .debug_byte(debug_byte)
               );

`ifdef SIMULATE_MAIN
   // Check stage_D respects stalling
   stallcheck #("x_valid",   17,  1) checkD01(clk, stall_X, x_valid);
   stallcheck #("x_opcode",  18,  7) checkD02(clk, stall_X, x_opcode);
   stallcheck #("x_op1_val", 19, 32) checkD03(clk, stall_X, x_op1_val);
   stallcheck #("x_rt_val",  20, 32) checkD04(clk, stall_X, x_rt_val);
   stallcheck #("x_wbr",     21,  6) checkD05(clk, stall_X, x_wbr);
   stallcheck #("x_res",     22, 32) checkD06(clk, stall_X, x_res);
   stallcheck #("x_jump",    23,  1) checkD07(clk, stall_X, x_jump);
   stallcheck #("x_target",  24, 32) checkD08(clk, stall_X, x_target);
   stallcheck #("x_hazzard", 25,  1) checkD09(clk, stall_X, x_hazzard);
   stallcheck #("x_annul_delay_slot", 125,  1) checkDA9(clk, stall_X, x_annul_delay_slot);
`endif

   stage_M stM(.clk(clk),
               .stall(stall_M),

               .x_valid(x_valid),
               .x_pc(x_pc),
               .x_instr(x_instr), // XXX for debugging only
               .x_opcode(x_opcode),
               .x_op1_val(x_op1_val),
               .x_rt_val(x_rt_val),
               .x_wbr(x_wbr),
               .x_res(x_res),

               .dmem_req(dmem_req),
               .dmem_res(dmem_res),

               .m_pc(m_pc),
               .m_instr(m_instr), // XXX for debugging only
               .m_valid(m_valid),// XXX for debugging only
               .m_wbr(m_wbr),
               .m_res(m_res),
               .m_hazzard(m_hazzard));

`ifdef SIMULATE_MAIN
   always @(posedge clk) if (debug) begin
   // $display("%x %x %x %x %x", m_hazzard,x_hazzard,i_hazzard,war_hazzard,branch_hazzard);
      $display(
"%05dz %x %x/%x D %8x:%8x X %8x:%8x:%8x>%2x M %8x:%8x>%2x W %8x:%8x>%2x",

               $time,
               {stall_I_ & branch_hazzard, // We only care in case of a branch
                m_hazzard,
                x_hazzard,
                i_hazzard,
                branch_hazzard,
                war_hazzard},

               {flush_M,flush_X,flush_D},
               {stall_M,stall_X,stall_D,stall_I},

               // DE
               i_pc, i_valid ? i_instr : 'h0,

               // EX
               d_pc, d_op1_val, d_op2_val, d_wbr,

               // ME
               x_pc, x_res, x_wbr,

               // WB
               m_pc, m_res, m_wbr);
      //$display("%05d", $time);
   end
`endif
endmodule

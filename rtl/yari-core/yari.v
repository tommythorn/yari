`timescale 1ns/10ps
`include "asm.v"
`include "../soclib/pipeconnect.h"

`ifdef SIMULATE_MAIN
/* Conditional compilation removes a lot of annoying warnings during synthesis. */
module stallcheck(clock, stall, a);
   parameter which = "?";
   parameter id = -1;
   parameter w = 1;

   input     clock;
   input     stall;
   input [w-1:0] a;

   reg           stall_;
   reg [w-1:0]   a_;

   always @(posedge clock) begin
      {a_,stall_} <= {a,stall};
      if (stall_ && a != a_)
        $display("** %05d STALLCHECKER violation %d: was %x != now %x (%d)",
                 $time, id, a_, a, stall_);
   end
endmodule
`endif

module yari(input  wire        clock          // K5  PLL1 input clock (50 MHz)
           ,input  wire        rst

           ,output wire `REQ   imem_req
           ,input  wire `RES   imem_res

           ,output wire `REQ   dmem_req
           ,input  wire `RES   dmem_res

           ,output wire `REQ   peripherals_req
           ,input  wire `RES   peripherals_res
           );

   parameter debug = 1;

   wire          i1_valid, i2_valid;
   wire [31:0]   i1_pc, i2_pc;

   wire          i_valid;
   wire [31:0]   i_instr;
   wire [31:0]   i_pc;
   wire [31:0]   i_npc;

   wire          d_valid;
   wire [31:0]   d_instr;
   wire [31:0]   d_pc;
   wire [31:0]   d_npc;
   wire [ 5:0]   d_opcode;
   wire [ 5:0]   d_fn;
   wire [ 4:0]   d_rd;
   wire [ 5:0]   d_rs;
   wire [ 5:0]   d_rt;
   wire [ 4:0]   d_sa;
   wire [31:0]   d_target;
   wire [ 5:0]   d_wbr;
   wire          d_has_delay_slot;
   wire [31:0]   d_op1_val;
   wire [31:0]   d_op2_val;
   wire [31:0]   d_rt_val;
   wire [31:0]   d_simm;

   wire          d_restart;
   wire [31:0]   d_restart_pc;
   wire          d_flush_X;

   wire          x_valid;
   wire [31:0]   x_instr;
   wire          x_is_delay_slot;
   wire [31:0]   x_pc;
   wire [ 5:0]   x_opcode;
   wire [31:0]   x_op1_val;
   wire [31:0]   x_rt_val;
   wire [ 5:0]   x_wbr;
   wire [31:0]   x_res;

   wire          x_restart;
   wire [31:0]   x_restart_pc;
   wire          x_flush_D;

   wire          m_valid;
   wire [31:0]   m_instr;
   wire [31:0]   m_pc;
   wire [ 5:0]   m_wbr;
   wire [31:0]   m_res;

   wire          m_kill;
   wire          m_restart;
   wire [31:0]   m_restart_pc;

   reg [9:0] initialized = 0;
   always @(posedge clock) initialized <= {initialized[8:0],~rst};

   // XXX It would be nice to make this a bit more general and merge
   // it with the interrupt mechanism (still to come)
   wire        boot = initialized[7] & ~initialized[8];

   wire        restart = ~m_kill & (x_restart | m_restart | boot);
   wire [31:0] restart_pc = (boot ? 'hBFC00000 :
                             m_restart ? m_restart_pc :
                             /*********/ x_restart_pc);
   wire        flush_I = m_kill | restart;
   wire        flush_D = m_kill | m_restart | x_flush_D;
   wire        flush_X = m_kill | m_restart | d_flush_X;

   stage_I stI(.clock(clock)
              ,.kill(~initialized[8] | m_kill)
              ,.restart(restart)
              ,.restart_pc(restart_pc)

              ,.imem_req(imem_req)
              ,.imem_res(imem_res)

              // Outputs
              ,.i1_valid(i1_valid)
              ,.i1_pc(i1_pc)
              ,.i2_valid(i2_valid)
              ,.i2_pc(i2_pc)

              ,.i_valid(i_valid)
              ,.i_instr(i_instr)
              ,.i_pc(i_pc)
              ,.i_npc(i_npc));

   stage_D stD(.clock(clock),
               .i_valid(i_valid & ~flush_I),
               .i_instr(i_instr),
               .i_pc(i_pc),
               .i_npc(i_npc),

               .x_valid(x_valid & ~flush_X),
               .x_wbr(x_wbr),
               .x_res(x_res),

               .m_valid(m_valid),
               .m_pc(m_pc),       // XXX for debugging only
               .m_wbr(m_wbr),
               .m_res(m_res),

               // Outputs, mostly derived from d_instr
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
               .d_target(d_target),
               .d_wbr(d_wbr),
               .d_has_delay_slot(d_has_delay_slot),

               // Register lookups
               .d_op1_val(d_op1_val),
               .d_op2_val(d_op2_val),
               .d_rt_val(d_rt_val),
               .d_simm(d_simm),
               .d_restart(d_restart),
               .d_restart_pc(d_restart_pc),
               .d_flush_X(d_flush_X),

               .flush_D(flush_D)
               );

   stage_X stX(.clock(clock),
               .d_valid(d_valid & ~flush_D),
               .d_instr(d_instr),
               .d_pc(d_pc),
               .d_npc(d_npc),
               .d_opcode(d_opcode),
               .d_fn(d_fn),
               .d_rd(d_rd),
               .d_rs(d_rs),
               .d_rt(d_rt),
               .d_sa(d_sa),
               .d_target(d_target),
               .d_wbr(d_wbr),
               .d_has_delay_slot(d_has_delay_slot),

               .d_op1_val(d_op1_val),
               .d_op2_val(d_op2_val),
               .d_rt_val(d_rt_val),
               .d_restart(d_restart),
               .d_restart_pc(d_restart_pc),

               .m_valid(m_valid),
               .m_wbr(m_wbr),

               // Results from this stage
               .x_valid(x_valid),
               .x_instr(x_instr), // XXX for debugging only
               .x_is_delay_slot(x_is_delay_slot),
               .x_pc(x_pc),
               .x_opcode(x_opcode),
               .x_op1_val(x_op1_val),
               .x_rt_val(x_rt_val),
               .x_wbr(x_wbr),
               .x_res(x_res),

               .x_restart(x_restart),
               .x_restart_pc(x_restart_pc),
               .x_flush_D(x_flush_D)
               );

   stage_M stM(.clock(clock),

               .d_simm(d_simm),
               .d_op1_val(d_op1_val),

               .x_valid(x_valid & ~flush_X),
               .x_instr(x_instr),
               .x_is_delay_slot(x_is_delay_slot),
               .x_pc(x_pc),
               .x_opcode(x_opcode),
               .x_op1_val(x_op1_val),
               .x_rt_val(x_rt_val),
               .x_wbr(x_wbr),
               .x_res(x_res),

               .dmem_req(dmem_req),
               .dmem_res(dmem_res),

               .peripherals_req(peripherals_req),
               .peripherals_res(peripherals_res),

               .m_valid(m_valid),
               .m_instr(m_instr),
               .m_pc(m_pc),
               .m_wbr(m_wbr),
               .m_res(m_res),

               .m_kill(m_kill),
               .m_restart(m_restart),
               .m_restart_pc(m_restart_pc)
               );


`ifdef SIMULATE_MAIN
   always @(posedge clock) if (debug) begin
      if (restart) begin
         $display("%05d  restart pipe at %x", $time, restart_pc);
         if (boot)
            $display("%05d        boot vector", $time);
         else if (m_restart)
            $display("%05d        from stage ME", $time);
         else if (x_restart)
            $display("%05d        from stage EX", $time);
      end

      if (m_kill)
         $display("%05d  flush pipe", $time);

      $display(
"%05dz %x %x/0 I %8x %8x D %8x:%8x X %8x:%8x:%8x>%2x M %8x:%8x>%2x W %8x:%8x>%2x",

               $time,
               0,

               {flush_X,flush_D,flush_I},

               // IF
               i1_valid ? i1_pc : 'hZ,
               i2_valid ? i2_pc : 'hZ,

               // DE
               i_pc, i_valid & ~flush_I ? i_instr : 'hZ,

               // EX
               d_pc, d_valid & ~flush_D ? d_op1_val : 'hZ,
                     d_valid & ~flush_D ? d_op2_val : 'hZ,
                     d_valid & ~flush_D ? d_wbr : 8'hZ,

               // ME
               x_pc, x_valid & ~flush_X ? x_res : 'hZ,
                     x_valid & ~flush_X ? x_wbr : 8'hZ,

               // WB
               m_pc, m_valid ? m_res : 'hZ,
                     m_valid ? m_wbr : 8'hZ);
   end
`endif
endmodule

// -----------------------------------------------------------------------
//
//   Copyright 2008 Tommy Thorn - All Rights Reserved
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

module yari(input  wire        clock
           ,input  wire        rst

            // Memory access
           ,input              mem_waitrequest
           ,output       [1:0] mem_id
           ,output      [29:0] mem_address
           ,output             mem_read
           ,output             mem_write
           ,output      [31:0] mem_writedata
           ,output       [3:0] mem_writedatamask
           ,input       [31:0] mem_readdata
           ,input        [1:0] mem_readdataid

           ,output wire `REQ   peripherals_req
           ,input  wire `RES   peripherals_res
           );

   parameter FREQ = 0; // Frequency in Hz, should be passed down from the top-level
   parameter debug = 1;

   wire          i1_valid, i2_valid;
   wire [31:0]   i1_pc, i2_pc;

   wire          i_valid;
   wire          i_valid_muxed;
   wire [31:0]   i_instr;
   wire [31:0]   i_instr_muxed;
   wire [31:0]   i_pc;
   wire [31:0]   i_pc_muxed;
   wire [31:0]   i_npc;

   wire          imem_waitrequest;
   wire [29:0]   imem_address;
   wire          imem_read;
   wire [31:0]   imem_readdata;
   wire          imem_readdatavalid;

   wire          d_valid;
   wire          d_illegal_instr;
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
   wire          d_load_use_hazard;

   wire          x_valid;
   wire [31:0]   x_instr;
   wire          x_is_delay_slot;
   wire [31:0]   x_pc;
   wire [ 5:0]   x_opcode;
   wire [31:0]   x_op1_val;
   wire [ 5:0]   x_rt;
   wire [31:0]   x_rt_val;
   wire [ 5:0]   x_wbr;
   wire [31:0]   x_res;

   wire          x_synci;
   wire [31:0]   x_synci_a;

   wire          x_restart;
   wire [31:0]   x_restart_pc;
   wire          x_flush_D;

   wire          m_valid;
   wire [31:0]   m_instr;
   wire [31:0]   m_pc;
   wire [ 5:0]   m_wbr;
   wire [31:0]   m_res;

   wire          m_restart;
   wire [31:0]   m_restart_pc;

   wire          dmem_waitrequest;
   wire [29:0]   dmem_address;
   wire          dmem_read;
   wire          dmem_write;
   wire [31:0]   dmem_writedata;
   wire [ 3:0]   dmem_writedatamask;
   wire [31:0]   dmem_readdata;
   wire          dmem_readdatavalid;

   wire [31:0]   perf_branch_hazard;
   wire [31:0]   perf_dcache_misses;
   wire [31:0]   perf_delay_slot_bubble;
   wire [31:0]   perf_div_hazard;
   wire [31:0]   perf_icache_misses;
   wire [31:0]   perf_io_load_busy;
   wire [31:0]   perf_io_store_busy;
   wire [31:0]   perf_load_hit_store_hazard;
   wire [31:0]   perf_load_use_hazard;
   wire [31:0]   perf_mult_hazard;
   wire [47:0]   perf_retired_inst;
   wire [31:0]   perf_sb_full;


   reg [9:0] initialized = 0;
   always @(posedge clock) initialized <= {initialized[8:0],~rst};

   // XXX It would be nice to make this a bit more general and merge
   // it with the interrupt mechanism (still to come)
   wire        boot = initialized[7] & ~initialized[8];

   wire        restart = d_restart | x_restart | m_restart;
   wire [31:0] restart_pc = (d_restart ? d_restart_pc :
                             m_restart ? m_restart_pc :
                             /*********/ x_restart_pc);
   wire        flush_I = restart;
   wire        flush_D = m_restart | x_flush_D;
   wire        flush_X = m_restart | d_flush_X;

   stage_I stI(.clock(clock)
              ,.kill(~initialized[8])
              ,.restart(restart)
              ,.restart_pc(restart_pc)
              ,.synci(x_synci)
              ,.synci_a(x_synci_a)

              ,.imem_waitrequest(imem_waitrequest)
              ,.imem_address(imem_address)
              ,.imem_read(imem_read)
              ,.imem_readdata(imem_readdata)
              ,.imem_readdatavalid(imem_readdatavalid)

              // Outputs
              ,.i1_valid(i1_valid)
              ,.i1_pc(i1_pc)
              ,.i2_valid(i2_valid)
              ,.i2_pc(i2_pc)

              ,.i_valid(i_valid)
              ,.i_instr(i_instr)
              ,.i_pc(i_pc)
              ,.i_npc(i_npc)
              ,.perf_icache_misses(perf_icache_misses));

   stage_D stD(.clock(clock),
               .i_valid(i_valid & ~flush_I),
               .i_instr(i_instr),
               .i_pc(i_pc),
               .i_npc(i_npc),

               .i_valid_muxed(i_valid_muxed),
               .i_pc_muxed(i_pc_muxed),
               .i_instr_muxed(i_instr_muxed),

               .x_valid(x_valid & ~flush_X),
               .x_wbr(x_wbr),
               .x_res(x_res),

               .m_valid(m_valid),
               .m_pc(m_pc),       // XXX for debugging only
               .m_wbr(m_wbr),
               .m_res(m_res),

               // Outputs, mostly derived from d_instr
               .d_valid(d_valid),
               .d_illegal_instr(d_illegal_instr),
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
               .d_load_use_hazard(d_load_use_hazard),

               .flush_D(flush_D),
               .perf_delay_slot_bubble(perf_delay_slot_bubble),
               .perf_retired_inst(perf_retired_inst)
               );

   stage_X stX(.clock(clock),

               .restart(restart),
               .restart_pc(restart_pc),

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
               .d_simm(d_simm),
               .d_restart(d_restart),
               .d_restart_pc(d_restart_pc),
               .d_load_use_hazard(d_load_use_hazard),

               .m_valid(m_valid),
               .m_wbr(m_wbr),

               // Results from this stage
               .x_valid(x_valid),
               .x_instr(x_instr), // XXX for debugging only
               .x_is_delay_slot(x_is_delay_slot),
               .x_pc(x_pc),
               .x_opcode(x_opcode),
               .x_op1_val(x_op1_val),
               .x_rt(x_rt),
               .x_rt_val(x_rt_val),
               .x_wbr(x_wbr),
               .x_res(x_res),

               .x_synci(x_synci),
               .x_synci_a(x_synci_a),

               .x_restart(x_restart),
               .x_restart_pc(x_restart_pc),
               .x_flush_D(x_flush_D),

               .perf_branch_hazard(perf_branch_hazard),
               .perf_dcache_misses(perf_dcache_misses),
               .perf_delay_slot_bubble(perf_delay_slot_bubble),
               .perf_div_hazard(perf_div_hazard),
               .perf_icache_misses(perf_icache_misses),
               .perf_io_load_busy(perf_io_load_busy),
               .perf_io_store_busy(perf_io_store_busy),
               .perf_load_hit_store_hazard(perf_load_hit_store_hazard),
               .perf_load_use_hazard(perf_load_use_hazard),
               .perf_mult_hazard(perf_mult_hazard),
               .perf_retired_inst(perf_retired_inst),
               .perf_sb_full(perf_sb_full)
               );
   defparam stX.FREQ = FREQ;

   stage_M stM(.clock(clock),

               .boot(boot),
               .boot_pc('hBFC00000),

               .d_simm(d_simm),
               .d_op1_val(d_op1_val),

               .x_valid(x_valid & ~flush_X),
               .x_instr(x_instr),
               .x_is_delay_slot(x_is_delay_slot),
               .x_pc(x_pc),
               .x_opcode(x_opcode),
               .x_op1_val(x_op1_val),
               .x_rt(x_rt),
               .x_rt_val(x_rt_val),
               .x_wbr(x_wbr),
               .x_res(x_res)

              ,.dmem_waitrequest(dmem_waitrequest)
              ,.dmem_address(dmem_address)
              ,.dmem_read(dmem_read)
              ,.dmem_write(dmem_write)
              ,.dmem_writedata(dmem_writedata)
              ,.dmem_writedatamask(dmem_writedatamask)
              ,.dmem_readdata(dmem_readdata)
              ,.dmem_readdatavalid(dmem_readdatavalid)

               ,
               .peripherals_req(peripherals_req),
               .peripherals_res(peripherals_res),

               .m_valid(m_valid),
               .m_instr(m_instr),
               .m_pc(m_pc),
               .m_wbr(m_wbr),
               .m_res(m_res),

               .m_restart(m_restart),
               .m_restart_pc(m_restart_pc),

               .perf_dcache_misses(perf_dcache_misses),
               .perf_io_load_busy(perf_io_load_busy),
               .perf_io_store_busy(perf_io_store_busy),
               .perf_load_hit_store_hazard(perf_load_hit_store_hazard),
               .perf_sb_full(perf_sb_full)
               );


   /*
    * Memory arbitration. "Hopefully so simple that I can do it all
    * right here".
    * Static priority - bad idea in general, but ok here.
    * Key decision: dmem port gets priority. Why? Imagine it was
    * the other way around and both miss in their caches. IF will
    * keep emitting bubbles while filling, but ME will repeatedly
    * restart the load and flush the pipe. At least with ME filling
    * first, we get to execute the few instructions already in the
    * pipe while waiting for IF. One of them could be a MUL!
    */
   parameter   ID_DC              = 2'd1;
   parameter   ID_IC              = 2'd2;
   wire        dmem_strobe        = dmem_read | dmem_write;

   assign      mem_id             = dmem_strobe ? ID_DC        : ID_IC;
   assign      mem_address        = dmem_strobe ? dmem_address : imem_address;
   assign      mem_read           = dmem_strobe ? dmem_read    : imem_read;
   assign      mem_write          = dmem_write;
   assign      mem_writedata      = dmem_writedata;
   assign      mem_writedatamask  = dmem_writedatamask;

   assign      dmem_waitrequest   = mem_waitrequest;
   assign      dmem_readdata      = mem_readdata;
   assign      dmem_readdatavalid = mem_readdataid == ID_DC;

   assign      imem_waitrequest   = mem_waitrequest | dmem_strobe;
   assign      imem_readdata      = mem_readdata;
   assign      imem_readdatavalid = mem_readdataid == ID_IC;

`ifdef SIMULATE_MAIN
   always @(posedge clock) if (debug) begin
      if (d_restart)
         $display("%05d  RESTART %x FROM DE", $time, d_restart_pc);
      else if (m_restart)
         $display("%05d  RESTART %x FROM ME", $time, m_restart_pc);
      else if (x_restart)
         $display("%05d  RESTART %x FROM EX", $time, x_restart_pc);

      $display(
"%05dz %x/0 I %8x D %8x:%8x X %8x:%8x:%8x>%2x M %8x:%8x>%2x W %8x:%8x>%2x",

               $time,

               {flush_X,flush_D,flush_I},

               // IF
               i1_valid ? i1_pc : 'hZ,

               // DE
               i_pc_muxed, i_valid_muxed ? i_instr_muxed : 'hZ,

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

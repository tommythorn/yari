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
//
// Memory stage
//

`timescale 1ns/10ps
`include "pipeconnect.h"

module stage_M(input  wire        clk
              ,input  wire        stall

              ,input  wire        x_valid
              ,input  wire [32:0] x_instr // XXX for debugging only
              ,input  wire [31:0] x_pc    // XXX for debugging only
              ,input  wire [ 6:0] x_opcode
              ,input  wire [31:0] x_op1_val // XXX
              ,input  wire [31:0] x_rt_val // for stores only
              ,input  wire [ 5:0] x_wbr
              ,input  wire [31:0] x_res

               // Connectionn to the dcache
              ,output wire `REQ   dmem_req
              ,input  wire `RES   dmem_res

               // Stage output
              ,output reg         m_valid = 0 // XXX for debugging only
              ,output reg  [32:0] m_instr = 0 // XXX for debugging only
              ,output reg  [31:0] m_pc    = 0 // XXX for debugging only
              ,output reg  [ 5:0] m_wbr   = 0
              ,output reg  [31:0] m_res   = 0
              ,output wire        m_hazzard // ASYNC
              );
   parameter debug = 0;
   pipechecker check("stage_M", clk, dmem_req, dmem_res);

   reg                m_load         = 0;
   reg  [31:0]        m_load_addr    = 0;
   reg  [ 6:0]        m_opcode       = 0;
   reg  [15:0]        q_half_shifted = 0;
   reg  [ 7:0]        q_byte_shifted = 0;
   reg  [31:0]        q_ext          = 0;

   // Shift and replicate the stored data
   wire [31:0] x_store_data =
         x_opcode[1:0] == 0 ? {x_rt_val[7:0], x_rt_val[7:0], x_rt_val[7:0], x_rt_val[7:0]} :
         x_opcode[1:0] == 1 ? {x_rt_val[15:0], x_rt_val[15:0]} :
         /* x_opcode==`SW ?*/ x_rt_val;

   // Generated the byte enables
   // XXX Trap on unaligned access!!
   wire [3:0] x_byteena =
        x_opcode[1:0] == 0 ? 4'h8 >> x_res[1:0] : // BE
        x_opcode[1:0] == 1 ? {~x_res[1],~x_res[1],x_res[1],x_res[1]} : // BE
        /* x_opcode==`SW ?*/ 4'hF;

   // XXX This depends on m_hazzard freezing the outputs of stage X!
   wire x_load  = {~x_valid,x_opcode[6:3]} == 4;
   wire x_store = {~x_valid,x_opcode[6:3]} == 5;

   wire [31:0] q;

   // Shifting and sign-extending the loaded result (if any).
   always @* begin
      case (m_load_addr[1]) // BE
        1: q_half_shifted = q[15: 0];
        0: q_half_shifted = q[31:16];
      endcase
      case (m_load_addr[0]) // BE
        1: q_byte_shifted = q_half_shifted[ 7: 0];
        0: q_byte_shifted = q_half_shifted[15: 8];
      endcase
      case (m_opcode[6:3])
         4: case (m_opcode[1:0])
              0: q_ext = {{24{q_byte_shifted[ 7] & ~m_opcode[2]}}, q_byte_shifted};
              1: q_ext = {{16{q_half_shifted[15] & ~m_opcode[2]}}, q_half_shifted};
              default: q_ext = q;
            endcase
         default: q_ext = q;
      endcase
      m_res = m_load ? q_ext : m_load_addr;
   end

   // Connect to the memory bus
   assign dmem_req`A   = x_res;
   assign dmem_req`R   = x_load;
   assign dmem_req`W   = x_store;
   assign dmem_req`WD  = x_store_data;
   assign dmem_req`WBE = x_byteena;
   assign q            = dmem_res`RD;
   assign m_hazzard    = dmem_res`HOLD;

   // Advance the pipeline
   always @(posedge clk) if (~stall) begin
     m_load        <= x_load;
     m_load_addr   <= x_res;
     m_wbr         <= x_valid ? x_wbr : 0;  // XXX This seems a bit inelegant.
     m_opcode      <= x_opcode;
     m_valid       <= x_valid;
     m_instr       <= x_instr;
     m_pc          <= x_pc;
   end

`ifdef SIMULATE_MAIN
   always @(posedge clk) if (debug) begin
      // $display("ME %8x", dmem_req);
      if (m_hazzard)
         $display("%5dd ME: memory busy!", $time);
      else begin
         if (m_load)
           $display("%5dd ME: ld [%x] -> %x", $time, m_load_addr, m_res);
         if (x_store)
           $display("%5dd ME: st %x -> [%x] & %x", $time,
                    x_store_data, x_res,
                    {{8{x_byteena[3]}},{8{x_byteena[2]}},{8{x_byteena[1]}},{8{x_byteena[0]}}});
      end
   end

initial if (debug) $monitor(
"%05dw ME: m_res %x = m_load %x ? q_ext %x : m_load_addr %x", $time,
m_res, m_load, q_ext, m_load_addr);
`endif
endmodule

`timescale 1ns/10ps
`include "pipeconnect.h"

module dmem(input  wire        clk,
            input  wire        rst,

            input  wire `REQ   dmem_req,
            output wire `RES   dmem_res,

            output wire `REQ   dc_ctrl_req,
            input  wire `RES   dc_ctrl_res,

            output wire `REQ   bus_ctrl_d_req,
            input  wire `RES   bus_ctrl_d_res);

   parameter       debug = 0;

   demux2 demux2(clk,
                 (dmem_req`A & 'hFFFF_E000) == 'h1000_0000, // XXX Get rid of this!
                 dmem_req,       dmem_res,
                 dc_ctrl_req,    dc_ctrl_res,
                 bus_ctrl_d_req, bus_ctrl_d_res);

`ifdef SIMULATE_MAIN
   pipechecker check1("dmem",            clk,       dmem_req,       dmem_res);
   pipechecker check2("dmem dc_ctrl_d",  clk,    dc_ctrl_req,    dc_ctrl_res);
   pipechecker check3("dmem bus_ctrl_d", clk, bus_ctrl_d_req, bus_ctrl_d_res);
`endif

   reg             r_ = 0;
   always @(posedge clk) if (debug) begin
      r_ <= dmem_req`R;
      if (dmem_res`HOLD)
        $display("%5d DMEM: Stall in data memory: %d %d %d", $time,
                 dmem_res`HOLD, dc_ctrl_res`HOLD, bus_ctrl_d_res`HOLD);
      else begin
         if (dmem_req`R)
           $display("%5d DMEM: load [%x]", $time, dmem_req`A);

         if (r_)
           $display("%5d DMEM: load -> %x", $time, dmem_res`RD);

         if (dmem_req`W)
           $display("%5d DMEM: store %x->[%x] (bytena %x)",
                    $time, dmem_req`WD, dmem_req`A, dmem_req`WBE);
      end
   end
endmodule // dmem

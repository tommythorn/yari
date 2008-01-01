`timescale 1ns/10ps
`include "pipeconnect.h"

module dc_ctrl(input  wire        clk,
               input  wire        rst,

               input  wire `REQ   dc_ctrl_req,
               output wire `RES   dc_ctrl_res);

   parameter       debug = 0;

   wire [31:0]     addr, q;
   reg             r_ = 0;
   always @(posedge clk) r_ <= rst ? 1'b0 : dc_ctrl_req`R;
   assign addr             = dc_ctrl_req`A;
   assign dc_ctrl_res`RD   = r_ ? q : 0;
   assign dc_ctrl_res`HOLD = 0;
    
   dcache dc(.clock  (clk),
             .clken  (1),
             .address(addr[12:2]),
             .wren   (dc_ctrl_req`W),
             .data   (dc_ctrl_req`WD),
             .byteena(dc_ctrl_req`WBE),
             .q      (q));

`ifdef SIMULATE_MAIN
   pipechecker check("dc_ctrl", clk, dc_ctrl_req, dc_ctrl_res);

   // Debugging
   always @(posedge clk) if (debug) begin
      if (dc_ctrl_res`HOLD)
        $display("%5d D$: Stall x_load %d x_store %d m_stall %d opcode %2x",
                 $time, dc_ctrl_req`R, dc_ctrl_req`W);
      else begin
         if (dc_ctrl_req`R)
           $display("%5d D$: load [%x]", $time, dc_ctrl_req`A);
         
         if (r_)
           $display("%5d D$: load -> %x", $time, dc_ctrl_res`RD);
         
         if (dc_ctrl_req`W)
           $display("%5d D$: store %x->[%x] (bytena %x)",
                    $time, dc_ctrl_req`WD, addr, dc_ctrl_req`WBE);
      end
   end
`endif
endmodule

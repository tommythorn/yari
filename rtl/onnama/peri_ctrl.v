`timescale 1ns/10ps
`include "pipeconnect.h"

// FF00_0000 - FF00_FFFF
module peri_ctrl(input  wire        clk,
                 input  wire        rst,

                 // Master connections
                 input  wire `REQ   peripheral_req,
                 output wire `RES   peripheral_res,

                 // Target connections
                 output wire `REQ   rs232_req,
                 input  wire `RES   rs232_res);

   // wire rs232en = (peripheral_req`A & 'hFFF0) == 'h0000;
   assign rs232_req     = peripheral_req;
   assign peripheral_res = rs232_res;

`ifdef SIMULATE_MAIN
   pipechecker check1("peri_ctrl",           clk, peripheral_req,  peripheral_res);
   pipechecker check2("peri_ctrl rs232_req", clk,     rs232_req,      rs232_res);
`endif
endmodule

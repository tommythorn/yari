`timescale 1ns/10ps
`include "pipeconnect.h"

module bus_ctrl(input  wire        clk,
                input  wire        rst,

                // Master connections
                input  wire `REQ   master1_req,
                output wire `RES   master1_res,

                input  wire `REQ   master2_req,
                output wire `RES   master2_res,

                input  wire `REQ   master3_req,
                output wire `RES   master3_res,

                // Target connections
                output wire `REQ   target1_req,
                input  wire `RES   target1_res,

                output wire `REQ   target2_req,
                input  wire `RES   target2_res
                );

   parameter debug = 0;

   wire master1_target1 = (master1_req`A & 'hFFF0_0000) == 'h4000_0000;
   wire master2_target1 = (master2_req`A & 'hFFF0_0000) == 'h4000_0000;
   wire master3_target1 = (master3_req`A & 'hFFF0_0000) == 'h4000_0000;

   wire master1_target2 = (master1_req`A & 'hFFF0_0000) == 'hFF00_0000;
   wire master2_target2 = (master2_req`A & 'hFFF0_0000) == 'hFF00_0000;
   wire master3_target2 = (master3_req`A & 'hFFF0_0000) == 'hFF00_0000;

   /*
    * Dummy target the returns all memory as 0. XXX xbar3x2 is cheaper
    * so we might drop this dummy target in future.
    */
   wire `REQ   target3_req;
   wire `RES   target3_res = 0;

   xbar3x3 xbar3x3(clk,
                   master1_target1, master1_target2, master1_req, master1_res,
                   master2_target1, master2_target2, master2_req, master2_res,
                   master3_target1, master3_target2, master3_req, master3_res,
                   target1_req, target1_res,
                   target2_req, target2_res,
                   target3_req, target3_res);

   pipechecker check1("bus_ctrl_i",    clk, master1_req, master1_res);
   pipechecker check2("master2",       clk, master2_req, master2_res);
   pipechecker check3("master3",       clk, master3_req, master3_res);
   pipechecker check4("bus_ctrl sram", clk, target1_req, target1_res);
   pipechecker check5("bus_ctrl peri", clk, target2_req, target2_res);
   pipechecker check6("target3",       clk, target3_req, target3_res);

   reg             r1_ = 0, r2_ = 0, r3_ = 0;
   always @(posedge clk) if (debug) begin
      r1_ <= master1_req`R;
      r2_ <= master2_req`R;
      r3_ <= master3_req`R;

      if (master1_req`R)    $display("%5d BUS_CTRL1: vga  [%x]",  $time, master1_req`A);
      if (master1_res`HOLD) $display("%5d BUS_CTRL1: Stall 1",    $time);
      if (r1_)              $display("%5d BUS_CTRL1: vga -> %x",  $time, master1_res`RD);
      if (master1_req`W)    $display("%5d BUS_CTRL1: store# %x->[%x] (bytena %x)",
                                     $time, master1_req`WD, master1_req`A, master1_req`WBE);

      if (master2_req`R)    $display("%5d BUS_CTRL2: fetc [%x]",  $time, master2_req`A);
      if (master2_res`HOLD) $display("%5d BUS_CTRL2: Stall 2",    $time);
      if (r2_)              $display("%5d BUS_CTRL2: fetc -> %x", $time, master2_res`RD);
      if (master2_req`W)    $display("%5d BUS_CTRL2: store? %x->[%x] (bytena %x)",
                                     $time, master2_req`WD, master2_req`A, master2_req`WBE);

      if (master3_req`R)    $display("%5d BUS_CTRL3: load [%x]",  $time, master3_req`A);
      if (master3_res`HOLD) $display("%5d BUS_CTRL3: Stall 3",    $time);
      if (r3_)              $display("%5d BUS_CTRL3: load -> %x", $time, master3_res`RD);
      if (master3_req`W)    $display("%5d BUS_CTRL3: store %x->[%x] (bytena %x)",
                                     $time, master3_req`WD, master3_req`A, master3_req`WBE);
   end
endmodule

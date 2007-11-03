`timescale 1ns/10ps
`include "pipeconnect.h"

/*
 Notation:
 _  low, 0
 ~  high, 1
 /  posedge
 \  negedge
 .  unknown,undetermined,unimportant
 #  valid data (held stable)
 <  changing
 >  --
 */


/*
 Fasttarget presents the request address as the result data after one
 cycle.  Wait is never asserted.

 WISHBONE - no wait states

 clock      _____/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______
 addr       ........<#### A1 ####><#### A2 ####>.........................
 read       ________/~~~~~~~~~~~~~~~~~~~~~~~~~~\_________________________
 wait       _____________________________________________________________
 readdata   _____________<#### D1 ####><#### D2 ####>____________________


 PIPECONNECT - no wait states
                               Request noticed by target
                               |             Response captured by initiator
                               v             v
 clock      _____/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______
 addr       ........<#### A1 ####><#### A2 ####>.........................
 read       ________/~~~~~~~~~~~~~~~~~~~~~~~~~~\_________________________
 wait       _____________________________________________________________
 readdata   ___________________________<#### D1 ####><#### D2 ####>______


 PIPECONNECT - some wait states
                               Request noticed by target
                               |             Response captured by initiator
                               v             v
 clock      _____/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______
 addr       ........<#### A1 ##################><#### A2 ####>.........................
 read       ________/~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________
 wait       _____________/~~~~~~~~~~~~\________________________________________________
 readdata   _________________________________________<#### D1 ####><#### D2 ####>______
 */
module fasttarget // PIPECONNECT, no wait
  (input  wire        clk,
   input  wire        rst,
   input  wire `REQ   req,
   output reg  `RES   res);

   always @(posedge clk) begin
      res`WAIT <= 0;
      res`RD <= ~rst && req`R ? req`A : 0;
   end
endmodule

/*
 PIPECONNECT - 1 wait state
                               Request noticed by target
                               |             Response captured by initiator
                               v             v
 clock      _____/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______
 addr       ........<#### A1 ##################><#### A2 ##################>...........
 read       ________/~~~~~~~~~~~~~~~~~~~~~~~~~~\/~~~~~~~~~~~~~~~~~~~~~~~~~~\___________
 wait       _____________/~~~~~~~~~~~~\______________/~~~~~~~~~~~~\____________________
 readdata   _________________________________________<#### D1 ####>______________<#### D2 ####>______

 _~_~_~_~_~_

 .AAAABBBB..
 _~~~~~~~~__
 _~~__~~____
 _____aa__bb

*/
module slowtarget // PIPECONNECT, 1 wait
  (input  wire        clk,
   input  wire        rst,
   input  wire `REQ   req,
   output wire `RES   res);

   reg [31:0] readData;
   reg        ready;

   assign     res`RD = readData;
   assign     res`WAIT = req`R & ~ready;

   always @(posedge clk)
     if (rst) begin
        readData <= 0;
        ready    <= 0;
        //$display("target in reset");
     end else begin
        readData <= ready ? req`A : 0;
        ready    <= req`R & ~ready;
        //$display("target %d %d", ready, res`WAIT);
     end
endmodule

/*
 Simple master waits for a result before issuing new request

 PIPECONNECT - no wait states
                               Request noticed by target
                               |             Response captured by initiator
                               v             v
 clock      /~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______
 addr       ...<#####req 1###>...........................<#####req 2
 read       ___/~~~~~~~~~~~~~\___________________________/~~~~~~~~~~
 wait       ________________________________________________________
 readdata   ______________________<#############>___________________
 */

/*
 Streaming master keeps one outstanding command

 PIPECONNECT - no wait states
                               Request noticed by target
                               |             Response captured by initiator
                               v             v
 clock      _____/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______
 addr       ........<#####req 1###>.............<#####req 2
 read       ________/~~~~~~~~~~~~~\___________________________/~~~~~~~~~~
 wait       _____________________________________________________________
 readdata   ___________________________<#############>___________________

*/

module initiator
  (input  wire        clk,
   input  wire        rst,
   output reg  `REQ   req,
   input  wire `RES   res);

   parameter  name = 1;

   reg [31:0] counter, data;
   reg [31:0] dataExpect;
   reg        dataValid;
   wire       pause = ^counter[1:0];

   always @(posedge clk)
     if (rst) begin
        counter <= name << 16;
        req     <= 0;
        dataValid <= 0;
        dataExpect <= 0;
     end else begin
        dataExpect <= data;
        dataValid <= req`R & ~res`WAIT;
        if (dataValid) begin
           if (dataExpect != res`RD)
             $display("%6d init%d got %x, expected %x !!! BAD!",
                      $time, name, res`RD, dataExpect);
           else
             $display("%6d init%d got %x as expected", $time, name, res`RD);
        end

        if (~res`WAIT) begin
           counter <= counter + 1;
           if (pause) begin
              req`R   <= 0;
           end else begin
              req`R   <= 1;
              req`A   <= counter;
              data    <= counter;
              $display("%6d init%d requests %x", $time, name, counter);
           end
        end
     end
endmodule

module main();
   reg rst, clk;
   wire `REQ req, req1, req2;
   wire `RES res, res1, res2;
   wire [31:0] addr = req`A;
   wire        read = req`R;
   wire        wai  = res`WAIT;
   wire [31:0] data = res`RD;

   initiator  #(1) initiator1(clk, rst, req1, res1);
   initiator  #(2) initiator2(clk, rst, req2, res2);
   mux2       mux_init(clk, req1, res1, req2, res2, req, res);
   slowtarget target(clk, rst, req, res);

   always # 5 clk = ~clk;
   initial begin
      $monitor("%d%d %4d %x %d %d %x", rst, clk, $time, addr, read, wai, data);
      clk = 1;
      rst = 1;
      #15 rst = 0;
      #200 $finish;
   end
endmodule

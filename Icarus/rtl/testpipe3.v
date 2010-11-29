`timescale 1ns/10ps
`include "pipeconnect.h"

`define TARGET_LOG 0
`define READER_LOG 1
`define WRITER_LOG 1

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

   parameter name = 1;

   always @(posedge clk) begin
      res`WAIT <= 0;
      res`RD <= ~rst && req`R ? req`A : 0;
      if (`TARGET_LOG & req`R)
        $display("Target%1d", name);
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

   parameter name = 1;

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
      if (`TARGET_LOG & req`R & ready)
        $display("Target%1d", name);
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

module reader
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
           if (dataExpect != res`RD) begin
             if (`READER_LOG)
               $display("%6d init%1d got %x, expected %x !!! BAD!",
                        $time, name, res`RD, dataExpect);
           end else begin
             if (`READER_LOG)
               $display("%6d init%1d got %x as expected", $time, name, res`RD);
           end
        end

        if (~res`WAIT) begin
           counter <= counter + name;
           if (pause) begin
              req`R   <= 0;
           end else begin
              req`R   <= 1;
              req`A   <= counter;
              data    <= counter;
              if (`READER_LOG)
                $display("%6d init%1d requests %x", $time, name, counter);
           end
        end else begin
           if (`READER_LOG)
             $display("%6d init%1d waits", $time, name);
        end
     end
endmodule

module writer
  (input  wire        clk,
   input  wire        rst,
   output reg  `REQ   req,
   input  wire `RES   res);

   parameter  name = 1;

   reg [31:0] counter, data;
   wire       pause = ^counter[2:1];

   always @(posedge clk)
     if (rst) begin
        counter <= name << 16;
        req     <= 0;
     end else begin
        if (~res`WAIT) begin
           counter <= counter + name;
           if (pause) begin
              req`W   <= 0;
           end else begin
              req`W   <= 1;
              req`A   <= counter;
              req`WD  <= counter;
              if (`WRITER_LOG)
                $display("%6d writer%1d requests %x", $time, name, counter);
           end
        end else begin
           if (`WRITER_LOG)
             $display("%6d writer%1d waits", $time, name);
        end
     end
endmodule

module main();
   reg rst, clk;
   wire `REQ req, req1, req2, reqA, reqB;
   wire `RES res, res1, res2, resA, resB;

   wire [31:0] addr1 = req1`A;
   wire        read1 = req1`R;
   wire        wai1  = res1`WAIT;
   wire [31:0] data1 = res1`RD;

   wire [31:0] addr2 = req2`A;
   wire        read2 = req2`R;
   wire        wai2  = res2`WAIT;
   wire [31:0] data2 = res2`RD;

   reader  #(1) reader1(clk, rst, req1, res1);
   writer  #(2) writer2(clk, rst, req2, res2);

   slowtarget #(1) target1(clk, rst, reqA, resA);
   slowtarget #(2) target2(clk, rst, reqB, resB);

   xbar2x2    xbar(clk,
                   addr1[2], req1, res1,
                   addr2[2], req2, res2,
                   reqA, resA,
                   reqB, resB);


   always # 5 clk = ~clk;
   initial begin
      $monitor(// "%d%d %4d 1: %x %d %d %x  2: %x %d %d %x",
               "%4d 1: %x %d %d %x  2: %x %d %d %x",
               // rst, clk,
               $time,
               addr1, read1, wai1, data1,
               addr2, read2, wai2, data2);
      clk = 1;
      rst = 1;
      #15 rst = 0;
      #20000 $finish;
   end
endmodule

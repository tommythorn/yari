`timescale 1ns/10ps
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

/*
  BETTER NAME WANTED!


                        P i p e  C o n n e c t

      A pipelined interconnect for inter-core master-target communication.

Rev: 0.1

Introduction:

- Inspired by WISHBONE(tm)
- Major improvement: enhanched for pipeline operation
- Simple operation

- Synchronous

Notation:
s' is the value of s in the previous clock cycle

Signals:
                Master          Target          Size
addr            O               I               m
rd_strobe       O               I               1
wr_strobe       O               I               1
wr_data         O               I               n (typical 16, 32)
wr_byteena      O               I               n/8

hold            I               O               1
rd_data         I               O               n


Operation:

  Asserting rd_strobe indicates that addr is to be latched and placed on rd_data
  in the next cycle.  If the target can't make that, then hold is asserted
  immediately and held until the cycle preceding the valid data on rd_data.

  Asserting wr_strobe indicates that addr, wr_data, and wr_byteena is to be
  latched and the bytes masked by wr_byteena be written at addr.  If target
  isn't ready to receive the command, the hold is asserted immediately and held
  until the inputs are latched.

  A fast target can accept a read or a write command each cycle and will never
  assert hold.


Invariants:
INV #1: [MASTER] No simultations reads and write (on the same port)

   rd_strobe /\ wr_strobe = 0

INV #2: [MASTER] Inputs must be held stable while hold is asserted

   hold' => addr = addr' /\ rd_strobe = rd_strobe' /\
            wr_strobe = wr_strobe' /\ wr_data = wr_data' /\ wr_byteena = wr_byteena'

INV #3: [TARGET] Hold can only be asserted when active

   ~rd_strobe /\ ~wr_strobe => hold = 0

(Maybe, still debated) rd_data must be lowered when idle
INV #4: [TARGET] ~rd_strobe' => rd_data = 0


 REWIRING:

 To ease the wiring of these signals, they are compounded into two
 structions: request and result.

  req={addr,rd_strobe,wr_strobe,wr_data,wr_byteena}
  res={rd_data,hold}


************************************************************************************/

`define AWM1 31
`define REQ     [`AWM1+38:0]
`define RES           [32:0]

// Request subfields
`define A      [`AWM1+38:38]
`define R               [37]
`define W               [36]
`define WD           [35: 4]
`define WBE          [ 3: 0]

// Result subfields
`define HOLD             [0]
`define RD            [32:1]

module pipechecker(where, clk, req, res);

   parameter m = 32;
   parameter n = 32;

   input wire      clk;
   input wire `REQ req;
   input wire `RES res;
   input wire [8*16:1] where;

   wire   [m-1:0] addr;
   wire           rd_strobe;
   wire           wr_strobe;
   wire   [n-1:0] wr_data;
   wire [n/8-1:0] wr_byteena;
   wire           hold;
   wire   [n-1:0] rd_data;

   reg            `REQ req_ = 0;
   reg            `RES res_ = 0;

   always @(posedge clk) begin
      req_       <= req;
      res_       <= res;
   end

`ifdef SIMULATE_MAIN
   // INV #1
   always @(posedge clk)
     if (req`R & req`W)
       $display("%5d PIPECHECKER: %s INV #1 violation, no simultaneous rd and wr", $time, where);

   // INV #2
   always @(posedge clk)
     if (res_`HOLD && req_ != req) begin
        $display("%5d PIPECHECKER: %s INV #2 violation, request changed while hold active", $time, where);
        $display("                 OLD: A %x R %d W %d RD %x WD %x", req_`A, req_`R, req_`W, req_`RD, req_`WD);
        $display("                 NEW: A %x R %d W %d RD %x WD %x", req`A, req`R, req`W, req`RD, req`WD);
     end

   // INV #3
   always @(posedge clk)
     if (~req`R & ~req`W & res`HOLD)
       $display("%5d PIPECHECKER: %s INV #3 violation, hold asserted without read or write strobe", $time, where);

   // INV #4
   always @(posedge clk)
     if (~req_`R & |res`RD)
       $display("%5d PIPECHECKER: %s INV #4 violation, data non-zero without a read in last cycle", $time, where);
`endif
endmodule


/*
Examples:

Trivial parallel IO port target:

m = 0, n = 8
module ioport(input  wire           clk,

              input  wire     [7:0] external_in,
              output reg      [7:0] external_out = 0,

              input  wire   [m-1:0] addr,
              input  wire           rd_strobe,
              input  wire           wr_strobe,
              input  wire   [n-1:0] wr_data,
              input  wire [n/8-1:0] wr_byteena,
              output wire           hold,
              output reg            rd_data = 0);

 assign hold = 0;

 always @(posedge clk) begin
    rd_data <= rd_strobe ? external_in : 0;
    if (wr_strobe & wr_byteena)
      external_out <= wr_data;
 end
endmodule
*/


// Demultiplexer: shares a master between two targets
module demux2(clk, // Unused
              selA,
              req, res,
              portA_req, portA_res,
              portB_req, portB_res);

   parameter m = 32;
   parameter n = 32;

   input  wire           clk; // Unused

   input  wire           selA;
   input  wire   `REQ    req;
   output wire   `RES    res;

   output wire   `REQ    portA_req;
   input  wire   `RES    portA_res;

   output wire   `REQ    portB_req;
   input  wire   `RES    portB_res;

 assign res`HOLD         = portA_res`HOLD | portB_res`HOLD; // Depends on INV #3
 assign res`RD           = portA_res`RD   | portB_res`RD;   // Depends on INV #4

 assign portA_req`A      = req`A;
 assign portA_req`R      = req`R & selA;
 assign portA_req`W      = req`W & selA;
 assign portA_req`WD     = req`WD;
 assign portA_req`WBE    = req`WBE;

 assign portB_req`A      = req`A;
 assign portB_req`R      = req`R & ~selA;
 assign portB_req`W      = req`W & ~selA;
 assign portB_req`WD     = req`WD;
 assign portB_req`WBE    = req`WBE;

`ifdef SIMULATE_MAIN
 pipechecker #(m,n) check("demux", clk, req, res);
 pipechecker #(m,n) checkA("demux A", clk, portA_req, portA_res);
 pipechecker #(m,n) checkB("demux B", clk, portB_req, portB_res);
`endif
endmodule

// Demultiplexer: shares a master between three targets
module demux3(input  wire clk
             ,input  wire           selA
             ,input  wire           selB
             ,input  wire   `REQ    req
             ,output wire   `RES    res

             ,output wire   `REQ    portA_req
             ,input  wire   `RES    portA_res

             ,output wire   `REQ    portB_req
             ,input  wire   `RES    portB_res

             ,output wire   `REQ    portC_req
             ,input  wire   `RES    portC_res
              );

 assign res`HOLD         =
        portA_res`HOLD | portB_res`HOLD | portC_res`HOLD; // Depends on INV #3
 assign res`RD           =
        portA_res`RD   | portB_res`RD   | portC_res`RD;   // Depends on INV #4

 assign portA_req`A      = req`A;
 assign portA_req`R      = req`R & selA;
 assign portA_req`W      = req`W & selA;
 assign portA_req`WD     = req`WD;
 assign portA_req`WBE    = req`WBE;

 assign portB_req`A      = req`A;
 assign portB_req`R      = req`R & selB;
 assign portB_req`W      = req`W & selB;
 assign portB_req`WD     = req`WD;
 assign portB_req`WBE    = req`WBE;

 assign portC_req`A      = req`A;
 assign portC_req`R      = req`R & ~(selA|selB);
 assign portC_req`W      = req`W & ~(selA|selB);
 assign portC_req`WD     = req`WD;
 assign portC_req`WBE    = req`WBE;

`ifdef SIMULATE_MAIN
 pipechecker check("demux", clk, req, res);
 pipechecker checkA("demux A", clk, portA_req, portA_res);
 pipechecker checkB("demux B", clk, portB_req, portB_res);
 pipechecker checkC("demux C", clk, portC_req, portC_res);
`endif
endmodule

// Prioritized multiplexer (port arbitration): shares a target between
// two masters
module mux2(
   input  wire            clk,
   input  wire   `REQ    portA_req,
   output wire   `RES    portA_res,

   input  wire   `REQ    portB_req,
   output wire   `RES    portB_res,

   output wire   `REQ    req,
   input  wire   `RES    res);

   parameter m = 32;
   parameter n = 32;


 // There are trivial for the two-port case, but to illustrate how to scale...
 wire portA_strobe = portA_req`R | portA_req`W;
 wire portB_strobe = portB_req`R | portB_req`W;

 // Prioritized arbitration

/*
 XXX Must be very careful when the target issues a hold (wait).
 If it previously was in a wait condition, then current input must be
 ignored until the hold clears.  Tricky.

 The following diagram illustrates how port B keeps the request while
 the target is in the hold condition even a request from (higher
 priority) port A comes in.  However, as soon as B's initial bus cycle
 is over, port A wins the next arbitration.  (Read requests used for
 this example).

                               Port B must keep the bus while hold is active
                               |             Request from port B accepted, port B loses the bus to A (thus B holds)
                               |             |             Data for port B from sampled, request from port B noted
                               |             |             |
                               v             v             v
 clock      _____/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______
 readA read ______________________/~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________
 portB read ________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________

 selected   ________<BBBBBBBBBBBBBBBBBBBBBBBBBB><AAAAAAAAAAAA><BBBBBBBBBBBB>_______________________
 addr       --------<###### Addr B1 ###########><## Addr A ##><## Addr B2##>-----------------------
 hold       _____________/~~~~~~~~~~~~\________________________________________________
 data       _______________________________________<## Data B1##><## Data A ##><## Data B2##>

 portA hold ______________________/~~~~~~~~~~~~\________________________________________________
 portB hold _____________/~~~~~~~~~~~~\_________/~~~~~~~~~~~~\_______________________________________



 */

 reg  hold_ = 0, selA_ = 0, selB_ =  0;
 wire selA = hold_ ? selA_ : portA_strobe; // INV #2
 wire selB = hold_ ? selB_ : ~selA & portB_strobe;  // INV #2

 always @(posedge clk) hold_ <= res`HOLD;
 always @(posedge clk) selA_ <= selA;
 always @(posedge clk) selB_ <= selB;

 assign req            = selA ? portA_req : portB_req;
 assign portA_res`RD   = selA_ ? res`RD : 0;           // INV #4
 assign portB_res`RD   = selB_ ? res`RD : 0;           // INV #4
 assign portA_res`HOLD = portA_strobe & (~selA | res`HOLD); // INV #3
 assign portB_res`HOLD = portB_strobe & (~selB | res`HOLD); // INV #3
endmodule

// Prioritized multiplexer (port arbitration): shares a target between
// three masters
module mux3
  (input  wire            clk,

   input  wire   `REQ    portA_req,
   output wire   `RES    portA_res,

   input  wire   `REQ    portB_req,
   output wire   `RES    portB_res,

   input  wire   `REQ    portC_req,
   output wire   `RES    portC_res,

   output wire   `REQ    req,
   input  wire   `RES    res);

   parameter y = 1;

   parameter m = 32;
   parameter n = 32;


 wire portA_strobe = portA_req`R | portA_req`W;
 wire portB_strobe = portB_req`R | portB_req`W;
 wire portC_strobe = portC_req`R | portC_req`W;

 // Prioritized arbitration
 reg hold_ = 0, selA_ = 0, selB_ = 0, selC_ = 0;
 wire selA = hold_ ? selA_ : portA_strobe;
 wire selB = hold_ ? selB_ : ~selA & portB_strobe;
 wire selC = hold_ ? selC_ : ~selA & ~selB & portC_strobe;

 always @(posedge clk) hold_ <= res`HOLD;
 always @(posedge clk) selA_ <= selA;
 always @(posedge clk) selB_ <= selB;
 always @(posedge clk) selC_ <= selC;

 assign req            = selA ? portA_req :
                         selB ? portB_req :
                         /*  */ portC_req ;

 assign portA_res`RD   = selA_ ? res`RD : 0;           // INV #4
 assign portB_res`RD   = selB_ ? res`RD : 0;           // INV #4
 assign portC_res`RD   = selC_ ? res`RD : 0;           // INV #4
 assign portA_res`HOLD = portA_strobe & (~selA | res`HOLD); // INV #3
 assign portB_res`HOLD = portB_strobe & (~selB | res`HOLD); // INV #3
 assign portC_res`HOLD = portC_strobe & (~selC | res`HOLD); // INV #3

/*
 always @(posedge clk)
    if (y == 1) begin
    $display("%5d T%1d: hold %1d  A%1d%1d%1d B%1d%1d%1d C%1d%1d%1d",
             $time, y,
             res`HOLD,
             portA_strobe, selA, portA_res`HOLD,
             portB_strobe, selB, portB_res`HOLD,
             portC_strobe, selC, portC_res`HOLD);
       if (portA_strobe + portB_strobe + portC_strobe > 1)
         $display("CONGESTION!!");
    end
*/
endmodule


// Full crossbar
module xbar2x2
  (input  wire            clk,

   input  wire           portM1selT1,
   input  wire   `REQ    portM1_req,
   output wire   `RES    portM1_res,

   input  wire           portM2selT1,
   input  wire   `REQ    portM2_req,
   output wire   `RES    portM2_res,

   output wire   `REQ    portT1_req,
   input  wire   `RES    portT1_res,

   output wire   `REQ    portT2_req,
   input  wire   `RES    portT2_res);

   parameter m = 32;
   parameter n = 32;

   wire   `REQ portM1T1_req, portM1T2_req, portM2T1_req, portM2T2_req;
   wire   `RES portM1T1_res, portM1T2_res, portM2T1_res, portM2T2_res;

   /* 1000 words:

        M1         M2
     ___|___    ___|___
    |       |  |       |
    | DEMUX |  | DEMUX |
    |_______|  |_______|
     |     \T2  /     |
     |    M1\  /      |
   M1|T1     \/       |
     |       /\     M2|T2
     |    M2/  \      |
     |____ /T1  \ ____|
    |       |  |       |
    |  MUX  |  |  MUX  |
    |_______|  |_______|
        |          |
        T1         T2
    */

   demux2 demux2_M1(clk,
                    portM1selT1, portM1_req, portM1_res,
                    portM1T1_req, portM1T1_res,
                    portM1T2_req, portM1T2_res);

   demux2 demux2_M2(clk,
                    portM2selT1, portM2_req, portM2_res,
                    portM2T1_req, portM2T1_res,
                    portM2T2_req, portM2T2_res);

   mux2   mux2_T1(clk,
                  portM1T1_req, portM1T1_res,
                  portM2T1_req, portM2T1_res,
                  portT1_req,   portT1_res);

   mux2   mux2_T2(clk,
                  portM1T2_req, portM1T2_res,
                  portM2T2_req, portM2T2_res,
                  portT2_req,   portT2_res);

`ifdef SIMULATE_MAIN
   pipechecker check1("xbar M1", clk, portM1_req, portM1_res);
   pipechecker check2("xbar M2", clk, portM2_req, portM2_res);
   pipechecker check3("xbar T1", clk, portT1_req, portT1_res);
   pipechecker check4("xbar T2", clk, portT2_req, portT2_res);
`endif
endmodule

// Full crossbar
module xbar3x2(clk,
               portM1selT1, portM1_req, portM1_res,
               portM2selT1, portM2_req, portM2_res,
               portM3selT1, portM3_req, portM3_res,
               portT1_req, portT1_res,
               portT2_req, portT2_res);

   parameter m = 32;
   parameter n = 32;

   input  wire            clk;

   input  wire           portM1selT1;
   input  wire   `REQ    portM1_req;
   output wire   `RES    portM1_res;

   input  wire           portM2selT1;
   input  wire   `REQ    portM2_req;
   output wire   `RES    portM2_res;

   input  wire           portM3selT1;
   input  wire   `REQ    portM3_req;
   output wire   `RES    portM3_res;

   output wire   `REQ    portT1_req;
   input  wire   `RES    portT1_res;

   output wire   `REQ    portT2_req;
   input  wire   `RES    portT2_res;

   wire   `REQ portM1T1_req;
   wire   `RES portM1T1_res;

   wire   `REQ portM1T2_req;
   wire   `RES portM1T2_res;

   wire   `REQ portM2T1_req;
   wire   `RES portM2T1_res;

   wire   `REQ portM2T2_req;
   wire   `RES portM2T2_res;

   wire   `REQ portM3T1_req;
   wire   `RES portM3T1_res;

   wire   `REQ portM3T2_req;
   wire   `RES portM3T2_res;

   /* 1000 words:

        M1         M2
     ___|___    ___|___
    |       |  |       |
    | DEMUX |  | DEMUX |
    |_______|  |_______|
     |     \T2  /     |
     |    M1\  /      |
   M1|T1     \/       |
     |       /\     M2|T2
     |    M2/  \      |
     |____ /T1  \ ____|
    |       |  |       |
    |  MUX  |  |  MUX  |
    |_______|  |_______|
        |          |
        T1         T2
    */

   demux2 demux2_M1(clk,
                    portM1selT1, portM1_req, portM1_res,
                    portM1T1_req, portM1T1_res,
                    portM1T2_req, portM1T2_res);

   demux2 demux2_M2(clk,
                    portM2selT1, portM2_req, portM2_res,
                    portM2T1_req, portM2T1_res,
                    portM2T2_req, portM2T2_res);

   demux2 demux2_M3(clk,
                    portM3selT1, portM3_req, portM3_res,
                    portM3T1_req, portM3T1_res,
                    portM3T2_req, portM3T2_res);

   mux3 #(1) mux3_T1(clk,
                  portM1T1_req, portM1T1_res,
                  portM2T1_req, portM2T1_res,
                  portM3T1_req, portM3T1_res,
                  portT1_req,   portT1_res);

   mux3 #(2) mux3_T2(clk,
                  portM1T2_req, portM1T2_res,
                  portM2T2_req, portM2T2_res,
                  portM3T2_req, portM3T2_res,
                  portT2_req,   portT2_res);

`ifdef SIMULATE_MAIN
   pipechecker check1("xbar M1", clk, portM1_req, portM1_res);
   pipechecker check2("xbar M2", clk, portM2_req, portM2_res);
   pipechecker check3("xbar M3", clk, portM3_req, portM3_res);
   pipechecker check4("xbar T1", clk, portT1_req, portT1_res);
   pipechecker check5("xbar T2", clk, portT2_req, portT2_res);
`endif
endmodule

/*
 * Scarily huge mostly combinatorial function
 */

/*
 * XXX This is most likely a very expensive and slow way to implement
 * to full crossbar.  I know this.  Before starting to micro-optimize
 * this, I'll first reevaluated pipeconnect as an interconnect
 * structure.  One interesting direction is to optimize for bursts and
 * multiplex the data and address bus.  Sort of like a dramatically
 * simplified Hypertransport.  Of course this increases latency (in
 * cycles), but should means lower logic and routing usage and a short
 * worst case path (that is, lower cycle time).  Increasing latency at
 * the expense of bandwidth is reasonable if and only if we have at
 * the minimum an instruction cache.  Now the major traffic becomes
 * multi-word cache refill, so burst traffic becomes important.
 *
 * In summery the goals are:
 * 0) optimize for burst traffic (amortizing the overhead)
 *    => multiplexing data, commands, and addresses
 * 1) lowering the overhead of the interconnect structure itself
 *    => VERY simple implementation
 *    => multiplexing data, commands, and addresses
 *    => registering paths => higher latency
 * 2) generalizing to multiple outstanding transactions
 *    => tagging the payload with the return address (Q: automatic?)
 *    => the receiver may cause the sender to wait
 *       Q: after the fact (hold the previous value) or before the
 *       fact (not-ready for input)?
 * 3) allowing for the receiver to be not ready
 *    already implied by 2)
 *
 * The basic structure is the peer-to-peer tunnel for sending data
 * from a sender to a receiver.  The sender sees a payload bus, a
 * strobe, and a hold (hello again).  The contract being that while
 * hold is active, the payload and the strobe must be held constant.
 * The payload arrives at the receiver after a tunnel specific delay,
 * which can be within the same cycle.
 *
 * The payload can contain anything, but for the routing elements to
 * ensure atomicity of burst transfers, we dedicate a bit to indicate
 * the first element of a burst.  It's easy to ... procrastinate.
 * Back to the Xbar.
 *
 */
module xbar3x3(input  wire clk

              ,input  wire           portM1selT1
              ,input  wire           portM1selT2
              ,input  wire   `REQ    portM1_req
              ,output wire   `RES    portM1_res

              ,input  wire           portM2selT1
              ,input  wire           portM2selT2
              ,input  wire   `REQ    portM2_req
              ,output wire   `RES    portM2_res

              ,input  wire           portM3selT1
              ,input  wire           portM3selT2
              ,input  wire   `REQ    portM3_req
              ,output wire   `RES    portM3_res

              ,output wire   `REQ    portT1_req
              ,input  wire   `RES    portT1_res

              ,output wire   `REQ    portT2_req
              ,input  wire   `RES    portT2_res

              ,output wire   `REQ    portT3_req
              ,input  wire   `RES    portT3_res
              );

   wire   `REQ portM1T1_req, portM1T2_req, portM1T3_req, portM2T1_req,
               portM2T2_req, portM2T3_req, portM3T1_req, portM3T2_req, portM3T3_req;
   wire   `RES portM1T1_res, portM1T2_res, portM1T3_res, portM2T1_res,
               portM2T2_res, portM2T3_res, portM3T1_res, portM3T2_res, portM3T3_res;

   demux3 demux3_M1(clk,
                    portM1selT1, portM1selT2, portM1_req, portM1_res,
                    portM1T1_req, portM1T1_res,
                    portM1T2_req, portM1T2_res,
                    portM1T3_req, portM1T3_res);

   demux3 demux3_M2(clk,
                    portM2selT1, portM2selT2, portM2_req, portM2_res,
                    portM2T1_req, portM2T1_res,
                    portM2T2_req, portM2T2_res,
                    portM2T3_req, portM2T3_res);

   demux3 demux3_M3(clk,
                    portM3selT1, portM3selT2, portM3_req, portM3_res,
                    portM3T1_req, portM3T1_res,
                    portM3T2_req, portM3T2_res,
                    portM3T3_req, portM3T3_res);

   mux3 #(1) mux3_T1(clk,
                  portM1T1_req, portM1T1_res,
                  portM2T1_req, portM2T1_res,
                  portM3T1_req, portM3T1_res,
                  portT1_req,   portT1_res);

   mux3 #(2) mux3_T2(clk,
                  portM1T2_req, portM1T2_res,
                  portM2T2_req, portM2T2_res,
                  portM3T2_req, portM3T2_res,
                  portT2_req,   portT2_res);

   mux3 #(3) mux3_T3(clk,
                  portM1T3_req, portM1T3_res,
                  portM2T3_req, portM2T3_res,
                  portM3T3_req, portM3T3_res,
                  portT3_req,   portT3_res);

`ifdef SIMULATE_MAIN
   pipechecker check1("xbar M1", clk, portM1_req, portM1_res);
   pipechecker check2("xbar M2", clk, portM2_req, portM2_res);
   pipechecker check3("xbar M3", clk, portM3_req, portM3_res);
   pipechecker check4("xbar T1", clk, portT1_req, portT1_res);
   pipechecker check5("xbar T2", clk, portT2_req, portT2_res);
   pipechecker check6("xbar T3", clk, portT3_req, portT3_res);
`endif
endmodule

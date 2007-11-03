/* -----------------------------------------------------------------------
 *
 *   Copyright 2004 Tommy Thorn - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * -----------------------------------------------------------------------
 *
 *
 * SRAM controller
 *
 * The is tailored for the Nios Dev. Kit, Cyclone edition which is
 * equipped with two IDT71V416 (S10PH Z0030P), a 256k x 16 SRAM.  The
 * SRAMs share all signals except the data bus and the bute enables
 * (however the data bus and byte enables are shared with the flash
 * ram and the ethernet adapter).
 *
 * The protocol is very simple: to perform a certain action, setup the
 * appropriate signals and raise the corresponding strobe signal.
 * Hold all signals while wait is active.  If the request was a read,
 * the data will be available in the first cycle after the wait.  If a
 * write isn't in progress and no higher priority reads are attempted
 * in the same cycle, the read will never see a wait and the data will
 * be available in the following cycle.
 *
 * TODO:
 * - Make it a real one-hot state machine (explicit idle state)
 * - Support the Flash RAM and the Ethernet device which are on the
 *   same bus.
 *
 * Some technical stuff about the SRAM protocol
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Reading is very easy; just write the address and pick up the data after
 * 10 ns (assuming OE is active and WE is inactive).
 *   1: [Optionally OE_n = 0, WE_n = 1, data_dir = IN]
 *      fse_addr = rd_addr;
 *      WAIT 10 ns (max{tRC,tAA,tOE})
 *
 *   2: data_o = fse_data
 *
 * Write cycle is trickier because we have to toggle WE:
 *
 *   1: fse_addr = wr_addr, OE_n = 1
 *      WAIT 5 ns (tOHZ)
 *
 *   2: WE_n = 0, fse_data_dir = OUT, fse_data = wr_data
 *      WAIT 8 ns (tWP)
 *
 *   3: WE_n = 1, OE_n = 0
 *      WAIT 5 ns (tOE)
 *
 *   4: data_dir = IN
 *
 * For writes in a row we can do
 *
 *   1: fse_addr = wr_addr, OE_n = 1
 *      WAIT 5 ns (tOHZ)
 *
 *   2: WE_n = 0, fse_data_dir = OUT, fse_data = wr_data
 *      WAIT 8 ns (tWP)
 *
 *      WHILE wr_req & ~rd_req DO
 *        XXX I'm banking on tWR=0 working!
 *
 *     3: WE_n = 1, fse_addr = wr_addr, fse_data = wr_data
 *        WAIT ??
 *
 *     4: WE_n = 0
 *        WAIT 5 ns (tOE)
 *
 *   5: data_dir = IN
 *
 * I probably could drive data out in step 1 but it
 * would be fighting the the output driver
 *
 *
 */

`timescale 1ns/10ps
`include "pipeconnect.h"
module sram_ctrl(
        input  wire        clk,

        // One read/write port (Latency 1, if not busy writing)
        input  wire `REQ   sram_req,
        output wire `RES   sram_res,

        // Flash-SRAM-Ethernet bus
        output wire [22:0] sram_a,
        inout  wire [31:0] sram_d,
        output wire        sram_cs_n,
        output wire  [3:0] sram_be_n,
        output wire        sram_oe_n,
        output wire        sram_we_n);

   parameter debug = 0;

   pipechecker pipechecker("sram_ctrl", clk, sram_req, sram_res);

   /*
    * OUT OF DATE COMMENT
    * Writing takes a state machine [, reading is immediate (pipelined).]
    * The write state machine is one-hot encoded.
    * XXX Make it truly one-hot and make a read state also.
    * State 0: loads the write address, marks it busy writing
    * State 1: asserts we, load data out
    * State 2: deassert we
    * State 3: turn the data bus back to input, acknowledge the write
    */

   /********************************************************************************
    *                           Port outputs
    *******************************************************************************/

/*

WISHFUL THINKING:

Two sequential writes + One read
                   Drive addr  Write data
clock      ~~~\______/~~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______

write      ______________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________
addr       <## Addr 0 ##><###### Addr1 #############><###### Addr 2 #############><## Addr 3 ##>------------------------
wr_data    --------------<###### Data1 #############><###### Data 2 #############>--------------------------------------
rd_data    ______________<## Data 0 ##>_________________________________________________________<## Data 3 ##>__________
hold       ______________/~~~~~~~~~~~~\______________/~~~~~~~~~~~~\_____________________________________________________

sram_addr  <## Addr 0 ##><###### Addr1 #############><###### Addr 2 #############><## Addr 3 ##>------------------------
sram_data* ZZZZZZZZZZZZZZZZZZZZZZZZ><## Data1 ###><ZZZZZZZZZZZZ><## Data2 ###><ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ
sram_we_n  ~~~~~~~~~~~~~~~~~~~~~~~~~\____________/~~~~~~~~~~~~~~\____________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
sram_be_n  _________________________<## BE1 #####>______________<## BE2 #####>__________________________________________

write_     _________________________/~~~~~~~~~~~~\

 (*) sram_data as driven, not actual line.

QUESTION: is it safe to switch address lines simultaneously with the we_n?  Probably not....

TURNS OUT TO BE SAFE I HAVE TO MAKE READ TAKE TWO CYCLES AND WRITE THREE :-(


Two sequential writes + One read
                   Drive addr  Write data
clock      ~~~\______/~~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______/~~~~~~\______

write      ______________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________
addr       <## Addr 0 ##><###### Addr1 #############><###### Addr 2 #############><## Addr 3 ##>------------------------
wr_data    --------------<###### Data1 #############><###### Data 2 #############>--------------------------------------
rd_data    ______________<## Data 0 ##>_________________________________________________________<## Data 3 ##>__________
hold       ______________/~~~~~~~~~~~~~~~~~~~~~~~~~~\______________/~~~~~~~~~~~~\_____________________________________________________

sram_addr  <## Addr 0 ##><###### Addr1 #############><###### Addr 2 #############><## Addr 3 ##>------------------------
sram_data* ZZZZZZZZZZZZZZZZZZZZZZZZ><## Data1 ###><ZZZZZZZZZZZZ><## Data2 ###><ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ
sram_we_n  ~~~~~~~~~~~~~~~~~~~~~~~~~\____________/~~~~~~~~~~~~~~\____________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
sram_be_n  _________________________<## BE1 #####>______________<## BE2 #####>__________________________________________
sram_oe_n  ______________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________

write      _________________________/~~~~~~~~~~~~\______________________
write_     _______________________________________/~~~~~~~~~~~~\______________________
*/

// `define FAST_READ 1

   reg         write = 0;
   reg         write_= 0;
   reg         read  = 0;
   reg         read_ = 0;

   /*
    * The SRAM is currently always selected, which is necessary to get
    * the low latency. Once the flash RAM and the ethernet port come
    * into play, we can no longer afford such luxury.
    */
   wire [31:0] addr   = sram_req`A;
   wire        reqR   = sram_req`R;
   wire        reqW   = sram_req`W;
   wire        hold   = sram_res`HOLD;

   reg         reqR_  = 0;
   reg  [31:0] sram_d_= 0;

   assign      sram_res`RD   = reqR_ ? sram_d_ : 0;
   assign      sram_res`HOLD = reqW & ~write_ | reqR & ~read;

   assign      sram_cs_n =  0;
   assign      sram_we_n = ~write;
   assign      sram_oe_n =  sram_req`W;
   assign      sram_be_n = (sram_req`W & ~write_) ? ~sram_req`WBE : 0;
   assign      sram_d    =  sram_req`W            ?  sram_req`WD  : 32'hZZZZZZZZ;
   assign      sram_a    =  sram_req`A;

   always @(posedge clk) begin
      if (debug)
        $display("%5d SRAM_CTRL: req`R %d  W %d R %d Hold %d SRAM A %8x D %8x BE %x OE %x WE %x",
                 $time, read, reqW, reqR, hold, addr, sram_d, sram_be_n, sram_oe_n, sram_we_n);

      sram_d_ <= sram_d;
      reqR_   <= reqR;
      read    <= reqR;
      read_   <= read;
      write   <= reqW & ~(write|write_);
      write_  <= write;

      if (debug & read)
        $display("%5d SRAM_CTRL: Read [%8x] -> %8x (sram_a %x)",
                 $time, sram_req`A, sram_d, sram_a);

      if (debug & write)
        $display("%5d SRAM_CTRL: Write %8x & %x -> [%8x] (sram_a %x)",
                 $time, sram_req`WD, sram_req`WBE, sram_req`A, sram_a);
     end

`ifdef SIMULATE_MAIN
   pipechecker check("sram_ctrl", clk, sram_req, sram_res);
   initial
     if (debug)
       $monitor("%5d SRAM_CTRL' W %d R %d Hold %d SRAM A %8x D %8x BE %x OE %x WE %x",
                $time, reqW, reqR, hold, sram_a, sram_d, sram_be_n, sram_oe_n, sram_we_n);
`endif
endmodule

`ifdef SIMULATE_SRAM_CTRL
module test();
   reg         clk = 1;
   reg         rst = 0;


   wire [22:0] sram_a;
   wire [31:0] sram_d;
   wire        sram_cs_n;
   wire [ 3:0] sram_be_n;
   wire        sram_oe_n;
   wire        sram_we_n;

   reg    `REQ sram_req;
   wire   `RES sram_res;

   idt71v416s10 u35(sram_d[15: 0], sram_a[19:2], sram_we_n, sram_oe_n, sram_cs_n,
                    sram_be_n[0], sram_be_n[1]); // Yep, strange order...
   idt71v416s10 u36(sram_d[31:16], sram_a[19:2], sram_we_n, sram_oe_n, sram_cs_n,
                    sram_be_n[2], sram_be_n[3]);

   sram_ctrl sram_ctrl(clk, rst,
                       sram_req, sram_res,
                       sram_a, sram_d, sram_cs_n, sram_be_n, sram_oe_n, sram_we_n);

   // XXX For some reason they don't seem to display correctly in $monitor
   // statements.  Icarus bug?
   wire        sram_req_r    = sram_req`R;
   wire        sram_req_w    = sram_req`W;
   wire        sram_res_hold = sram_res`HOLD;
   wire [31:0] sram_res_rd   = sram_res`RD;

 `define MAGIC_ADDR1 'h1730
 `define MAGIC_DATA1 'h87654321
 `define MAGIC_ADDR2 'h1729
 `define MAGIC_DATA2 'h98765432

   always #20 clk = ~clk;
   initial begin
      $display("Time Ck r w H data");
      $monitor("%4d %x  %x %x %x %x",
               $time, clk,
               sram_req_r, sram_req_w, sram_res_hold, sram_res_rd);

      #0 rst = 1; sram_req = 0;
      #120 rst = 0;

      $display("%4d write %8x -> [%4x]", $time, `MAGIC_DATA1, `MAGIC_ADDR1);
      sram_req`A   = `MAGIC_ADDR1 * 4;
      sram_req`WD  = `MAGIC_DATA1;
      sram_req`WBE = ~0;
      sram_req`W   = 1;
      #80
      wait (~sram_res_hold);
      sram_req`W = 0;
      $display("%4d write %8x -> [%4x]", $time, `MAGIC_DATA2, `MAGIC_ADDR2);
      sram_req`A   = `MAGIC_ADDR2 * 4;
      sram_req`WD  = `MAGIC_DATA2;
      sram_req`WBE = ~0;
      sram_req`W   = 1;
      #80
      wait (~sram_res_hold);
      sram_req`W = 0;
      $display("%4d read [%4x]", $time, `MAGIC_ADDR2);
      sram_req`A = `MAGIC_ADDR2 * 4;
      sram_req`R = 1;
      #40
        if (sram_res_rd != `MAGIC_DATA2)
          $display("FAILURE %8x ");
        sram_req`R = 0;
      $display("%4d read [1729]", $time);
      sram_req`A = 'h1729 * 4;
      sram_req`R = 1;
      #40 sram_req`R = 0;
      #320

      #2400 $finish;
   end
endmodule
`endif
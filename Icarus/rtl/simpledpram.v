// XXX Icarus can't handle parameters being use in the header

`timescale 1ns/10ps

module simpledpram(clock,
                   wraddress, wrdata, wren,
                   rdaddress, rddata);
   parameter DATA_WIDTH = 32;
   parameter ADDR_WIDTH = 7;
   parameter INIT_FILE = "somefile.mif";
   parameter OFFSET = 0;

   input                         clock;
   input      [(ADDR_WIDTH-1):0] rdaddress;
   output reg [(DATA_WIDTH-1):0] rddata;
   input      [(DATA_WIDTH-1):0] wrdata;
   input                         wren;
   input      [(ADDR_WIDTH-1):0] wraddress;

   // Declare the RAM variable
   (* ram_init_file = INIT_FILE *)
   reg [DATA_WIDTH-1:0] ram[(1 << ADDR_WIDTH)-1:0];

   parameter            debug = 0;

   always @ (posedge clock) begin
      // Write
      if (wren)
         ram[wraddress] <= wrdata;
      if (debug & wren)
         $display("%x: ram[%x] (was %x) <= %x", OFFSET, wraddress,
                  ram[wraddress],
                  wrdata);

      // Read (if rdaddress == wraddress, return OLD data).  To return
      // NEW data, use = (blocking write) rather than <= (non-blocking write)
      // in the write assignment.  NOTE: NEW data may require extra bypass
      // logic around the RAM.
      rddata <= ram[rdaddress];
    end

   initial #0 $readmemh({INIT_FILE,".data"}, ram);
endmodule

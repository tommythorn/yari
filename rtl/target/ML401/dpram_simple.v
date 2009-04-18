`timescale 1 ns / 10 ps

module dpram_simple(clock,
	            address_a, wrdata_a, wren_a, rddata_a,
                    address_b, wrdata_b, wren_b, rddata_b);

   parameter DATA_WIDTH =  8;
   parameter ADDR_WIDTH =  7;
   parameter INIT_FILE  = "somefile"; // No .mif!
   parameter DEBUG      = 0;

   input                         clock;

   input      [ADDR_WIDTH-1:0]   address_a;
   input      [DATA_WIDTH-1:0]   wrdata_a;
   input                         wren_a;
   output reg [DATA_WIDTH-1:0]   rddata_a;

   input      [ADDR_WIDTH-1:0]   address_b;
   input      [DATA_WIDTH-1:0]   wrdata_b;
   input                         wren_b;
   output reg [DATA_WIDTH-1:0]   rddata_b;

   // Declare the RAM variable
   reg [DATA_WIDTH-1:0] ram[(1 << ADDR_WIDTH)-1:0];

   always @(posedge clock) begin
      if (wren_a)
         ram[address_a] <= wrdata_a;

      /*
       * Read (if read_addr == write_addr, return OLD data).  To return
       * NEW data, use = (blocking write) rather than <= (non-blocking write)
       * in the write assignment.  NOTE: NEW data may require extra bypass
       * logic around the RAM.
       */

      rddata_a <= ram[address_a];
   end

   always @(posedge clock) begin
      if (wren_b)
         ram[address_b] <= wrdata_b;

      /*
       * Read (if read_addr == write_addr, return OLD data).  To return
       * NEW data, use = (blocking write) rather than <= (non-blocking write)
       * in the write assignment.  NOTE: NEW data may require extra bypass
       * logic around the RAM.
       */

      rddata_b <= ram[address_b];
   end
endmodule

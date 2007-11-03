`timescale 1 ns / 10 ps
module dpram (clock,
	      address_a, byteena_a, wrdata_a, wren_a, rddata_a,
              address_b, byteena_b, wrdata_b, wren_b, rddata_b);

   parameter DATA_WIDTH = 32;
   parameter ADDR_WIDTH =  7;
   parameter INIT_FILE  = "somefile"; // No .mif!
   parameter DEBUG      = 0;

   input                         clock;

   input      [ADDR_WIDTH-1:0]   address_a;
   input      [DATA_WIDTH/8-1:0] byteena_a;
   input      [DATA_WIDTH-1:0]   wrdata_a;
   input                         wren_a;
   output reg [DATA_WIDTH-1:0]   rddata_a;

   input      [ADDR_WIDTH-1:0]   address_b;
   input      [DATA_WIDTH-1:0]   wrdata_b;
   input      [DATA_WIDTH/8-1:0] byteena_b;
   input                         wren_b;
   output reg [DATA_WIDTH-1:0]   rddata_b;

   // Declare the RAM variable
   reg [DATA_WIDTH/4-1:0] ram0[(1 << ADDR_WIDTH)-1:0];
   reg [DATA_WIDTH/4-1:0] ram1[(1 << ADDR_WIDTH)-1:0];
   reg [DATA_WIDTH/4-1:0] ram2[(1 << ADDR_WIDTH)-1:0];
   reg [DATA_WIDTH/4-1:0] ram3[(1 << ADDR_WIDTH)-1:0];

   always @(posedge clock) begin
      if (wren_a) begin
         if (byteena_a[0]) ram0[address_a] <= wrdata_a[ 7: 0];
         if (byteena_a[1]) ram1[address_a] <= wrdata_a[15: 8];
         if (byteena_a[2]) ram2[address_a] <= wrdata_a[23:16];
         if (byteena_a[3]) ram3[address_a] <= wrdata_a[31:24];
      end

      if (wren_b) begin
         if (byteena_b[0]) ram0[address_b] <= wrdata_b[ 7: 0];
         if (byteena_b[1]) ram1[address_b] <= wrdata_b[15: 8];
         if (byteena_b[2]) ram2[address_b] <= wrdata_b[23:16];
         if (byteena_b[3]) ram3[address_b] <= wrdata_b[31:24];
      end

      // Read (if read_addr == write_addr, return OLD data).  To return
      // NEW data, use = (blocking write) rather than <= (non-blocking write)
      // in the write assignment.  NOTE: NEW data may require extra bypass
      // logic around the RAM.
      rddata_a <= {ram3[address_a],ram2[address_a],ram1[address_a],ram0[address_a]};
      rddata_b <= {ram3[address_b],ram2[address_b],ram1[address_b],ram0[address_b]};

      if (DEBUG) begin
         $display("%05d  dram[%d] = %d", $time,
                  address_a,
                  {ram3[address_a],ram2[address_a],ram1[address_a],ram0[address_a]});
         if (wren_a)
            $display("%05d  dram[%d] <-a %d/%d  [%d/%d/%d]", $time,
                     address_a, wrdata_a, byteena_a, address_b, wren_b, byteena_b);
         if (wren_b)
            $display("%05d  dram[%d] <-b %d/%d  [%d/%d/%d]", $time,
                     address_b, wrdata_b, byteena_b, address_a, wren_a, byteena_a);
      end
    end

   /*
    * This is so lame, but it looks like a Verilog limitation that we
    * can't write
    *
    *    if (byteena_b[3]) ram[address_b][31:24] <= wrdata_b[31:24];
    *
    * thus we have to manually split the ram into four blocks if we desire
    * byte enables. Meh.
    */
   reg [DATA_WIDTH-1:0] ram_initial[(1 << ADDR_WIDTH)-1:0];


   reg [31:0] i;
   initial begin
      #0 $readmemh({INIT_FILE,".data"}, ram_initial);
      for (i = 0; i < 1 << ADDR_WIDTH; i = i + 1)
         {ram3[i],ram2[i],ram1[i],ram0[i]} = ram_initial[i];
   end
endmodule

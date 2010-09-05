`timescale 1 ns / 10 ps

module dpram (clock,
	      address_a, byteena_a, wrdata_a, wren_a, rddata_a,
              address_b, byteena_b, wrdata_b, wren_b, rddata_b);

   parameter DATA_WIDTH = 32;
   parameter ADDR_WIDTH =  7;
   parameter INIT_FILE  = "dummy"; // This is ignored right now

   input                         clock;

   input      [ADDR_WIDTH-1:0]   address_a;
   input      [DATA_WIDTH/8-1:0] byteena_a;
   input      [DATA_WIDTH-1:0]   wrdata_a;
   input                         wren_a;
   output     [DATA_WIDTH-1:0]   rddata_a;

   input      [ADDR_WIDTH-1:0]   address_b;
   input      [DATA_WIDTH-1:0]   wrdata_b;
   input      [DATA_WIDTH/8-1:0] byteena_b;
   input                         wren_b;
   output     [DATA_WIDTH-1:0]   rddata_b;

   dpram_simple s0(clock,
	          address_a, wrdata_a[ 7: 0], byteena_a[0] & wren_a, rddata_a[ 7: 0],
                  address_b, wrdata_b[ 7: 0], byteena_b[0] & wren_b, rddata_b[ 7: 0]);
   defparam s0.DATA_WIDTH = DATA_WIDTH / 4,
            s0.DATA_WIDTH = DATA_WIDTH;

   dpram_simple s1(clock,
	          address_a, wrdata_a[15: 8], byteena_a[1] & wren_a, rddata_a[15: 8],
                  address_b, wrdata_b[15: 8], byteena_b[1] & wren_b, rddata_b[15: 8]);
   defparam s1.DATA_WIDTH = DATA_WIDTH / 4,
            s1.DATA_WIDTH = DATA_WIDTH;

   dpram_simple s2(clock,
	          address_a, wrdata_a[23:16], byteena_a[2] & wren_a, rddata_a[23:16],
                  address_b, wrdata_b[23:16], byteena_b[2] & wren_b, rddata_b[23:16]);
   defparam s2.DATA_WIDTH = DATA_WIDTH / 4,
            s2.DATA_WIDTH = DATA_WIDTH;

   dpram_simple s3(clock,
	          address_a, wrdata_a[31:24], byteena_a[3] & wren_a, rddata_a[31:24],
                  address_b, wrdata_b[31:24], byteena_b[3] & wren_b, rddata_b[31:24]);
   defparam s3.DATA_WIDTH = DATA_WIDTH / 4,
            s3.DATA_WIDTH = DATA_WIDTH;
endmodule

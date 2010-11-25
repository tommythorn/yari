`timescale 1ns/10ps
module dummy_master(
            input  wire           clock          // K5  PLL1 input clock (50 MHz)
           ,input  wire           reset

            // Memory access
           ,input                 mem_waitrequest
           ,output reg      [1:0] mem_id = 1
           ,output reg     [29:0] mem_address = 0
           ,output reg            mem_read = 0
           ,output reg            mem_write = 0
           ,output reg     [31:0] mem_writedata = 0
           ,output reg      [3:0] mem_writedatamask = 0
           ,input          [31:0] mem_readdata
           ,input           [1:0] mem_readdataid


           ,output reg     [31:0] errors = 0
           );

   reg [18:0] wp = 0, rp = 0, vp = 0;
   reg [31:0] data = 0;
   reg [32:0] lfsr = 0;
   reg mismatch_pre = 0, mismatch = 0;

   always @(posedge clock) if (reset) begin
      wp <= 0;
      rp <= 0;
      vp <= 0;
      lfsr <= 0;
      errors <= 0;
      mem_read <= 0;
      mem_write <= 0;
      mismatch_pre <= 0;
      mismatch <= 0;
   end else begin
      mem_id <= 1;

      lfsr <= {lfsr[31:0], ~lfsr[32] ^ lfsr[19]};

      if (mem_readdataid == 1) begin
         // Delay to help fMAX
         mismatch_pre <= mem_readdata != {vp,~vp[3:0]};
         mismatch <= mismatch_pre;
         if (mismatch)
            errors <= errors + 1;
         vp <= vp + 1;
      end

      if (~mem_waitrequest) begin
         mem_read          <= 0;
         mem_write         <= 0;

         if (~lfsr[4]) begin
            mem_writedata     <= {wp, ~wp[3:0]};

            mem_writedatamask <= /*lfsr[4:1]*/ ~0;
            mem_write        <= 1;
            mem_address      <= wp;
            wp               <= wp + 1;
         end else begin
            mem_read         <= 1;
            mem_address      <= rp;
            rp               <= rp + 4;
         end
      end
   end
endmodule

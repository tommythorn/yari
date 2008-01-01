module mult(input wire clk,
            input wire rst,

            input wire [31:0] op1,
            input wire [31:0] op2,
            input wire go,

            output reg [63:0] res,
            output wire hold);

   reg             valid, ready;
   reg [31:0]      a;
   reg [63:0]      b;
   assign #(2)     hold = go & ~valid;

/*
 res = 0;
 while (op1) {
   if (op1 & 1) res += op2;
   op1 >>= 1;
   op2 <<= 1;
 }

back to timing diagrams

             0      n
clk     _/~\_/~\...._/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/
a,b     #>.................<##>....
go      _/~~~~~~....~~~~~~~~~~~~~~~
hold    _/~~~~~~....~~~\__/~~~~~~~~
res     XXXXXXXXXXXXXXXXXX><R1><XXXXXX
valid   _______________/~~\__
ready*  ~\_______________

             0      n
clk     \_/~\_/~\...._/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/~\_/
a,b     0 #>.................<##>....
go      _/~~~~~~....~~~~~~~~~~~~~~~
hold    _/~\__/~~~~~~~~
res     XXXXXXXXXXXXXXXXXXX><R1><XXXXXX
valid   _______________/~~\__
ready   ~~_____/~~~~~~~~\




*/

   reg             hold_, go_;
   always @(posedge clk) begin
      hold_ <= hold;
      go_ <= go;
      if (go_ & ~hold_ & go & ~hold)
        $display("%5d HOLD WAS DOWN FOR TWO CONSECUTIVE CYCLES!", $time);
   end

   always @(posedge clk)
      if (rst) begin
         res       <= 0;
         valid     <= 0;
         ready     <= 1;
         a         <= 0;
      end else begin
         if (a) begin
            if (a[0]) res <= res + b;
            a <= {1'b0, a[30:1]}; // a >>= 1
            b <= {b[62:0], 1'b0}; // b <<= 1
            if (a[30:1] == 0) begin
               valid   <= 1;
               $display("Done1");
            end

         end else begin
            ready <= 1;
            valid <= 0;
            if (go & ready) begin
               $display("Started a new calculation of %2d * %2d",
                        op1, op2);
               ready   <= 0;
               b       <= {op2,1'b0};
               a       <= op1[31:1];
               res     <= op1[0] ? op2 : 0;
               if (op1[31:1] == 0) begin
                  valid   <= 1;
                  $display("Done2");
               end
            end
         end
      end
 initial
      $monitor("%5d %3d %5d %5d go %d valid %d ready %d %3d %3d",

               $time, a, b, res, go, valid, ready, op1, op2);
endmodule

module main();
   reg clk, rst;
   reg [31:0] a, b;
   reg        go;
   wire       hold;
   wire [63:0] res;
   reg [63:0] got;

   always @(posedge clk)
      if (rst) begin
         got <= 0;
         a <= 10;  // 4^2 + 3^2 + 2^2 + 1^2 = 16+9+4+1
         go <= 1;
      end else if (~hold) begin
         if (a > 0) begin
            a <= a-1;
            got <= got + res;
            $display("MULT result = %3d", res);
         end else begin
            go <= 0;
            $display("SUM = %3d", got);
         end
      end

   mult mult(clk, rst, a, a, go, res, hold);

   always # 5 clk = ~clk;

   initial begin
      $monitor("%5d %d a %3d res %3d hold %d", $time, rst, a, res, hold);
      #0 clk = 1; rst = 1;
      a = 5; go = 0;
      b = 7;
      #1 rst = 0;
      go = 1;
      // wait(~hold);
      // go = 0;
      #3000 $finish;
   end
endmodule

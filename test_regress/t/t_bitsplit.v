// DESCRIPTION: Verilator: Verilog Test module
//
// A test of the ability to find netlist subsets based on individual bits.
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2012 by Jeremy Bennett.

module t (clk);

   input  clk;

   integer      cnt = 0;

   reg [1:0]    res_in = 0;
   reg [1:0]    res_out = 0;

   // The test instantiates a connection which swaps bits from input to
   // output.
   test test_i (.clk (clk),
		.invec (res_in),
		.outvec (res_out));

   always @(posedge clk) begin
      res_in <= res_in + 1;
   end

   // Count clk edges to terminate.
   always @(posedge clk) begin
      cnt = cnt + 1;
      if (cnt == 5) begin
	 $write("*-* All Finished *-*\n");
	 $finish;
      end
   end

endmodule


module test (
  input        clk,
  input [1:0]  invec,
  output [1:0] outvec);

   always @(posedge clk) begin
      outvec [0] <= invec [1];
      outvec [1] <= invec [0];
   end
endmodule

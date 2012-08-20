// DESCRIPTION: Verilator: Verilog Test module
//
// A test of the ability to find netlist subsets based on individual bits.
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2012 by Jeremy Bennett.

module t (clk);

   input  clk;

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
      if (res_out == 2'b11) begin
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
      outvec <= {invec[0], invec[1]};
   end
endmodule

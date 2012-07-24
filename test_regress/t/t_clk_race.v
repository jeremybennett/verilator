// DESCRIPTION: Verilator: Verilog Test module
//
// Demonstrate a race between a clock and non-clock.
//
// Some other tools choose always to allow the clock to beat the non-clock.
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2012 by Jeremy Bennett.

 `timescale 1ns/1ns

module t (/*AUTOARG*/
   // Inputs
   clk
   );
   input clk;

   integer cnt;

   reg 	     sub_clk;
   reg [1:0] sub_comb;
   reg 	     res;

`ifndef VERILATOR
   initial begin
      $dumpfile("obj_dir/iv_t_clk_race/iv_t_clk_race.vcd");
      $dumpvars;
   end
`endif

   initial begin
      cnt      = 0;

      sub_clk  = 1'b0;
      sub_comb = 2'b0;
      res      = 1'b0;
   end

   // Derive our two derived signals in separate processes, so they are in a
   // race.

   // There are two potential races here, depending on whether blocking or
   // no-blocking assignments are used to assign sub_clk and sub_comb in the
   // first two always blocks.

   // 1. There is a race between sub_clk being written in the first always
   //    block and being read in the second, if it is assigned to using a
   //    blocking assignment.

   // 2. There is a race between sub_comb being updated in the second always
   //    block and sub_comb being read (i.e. the posedge of sub_clk event) in
   //    the third always block, if sub_comb is set using a non-blocking
   //    assignment.
   always @(posedge clk) begin
      sub_clk = ~sub_clk;
   end

   always @(posedge clk) begin
      if (sub_clk) begin
	sub_comb <= sub_comb + 1;
      end
   end

   always @(posedge sub_clk) begin
      if (&sub_comb[1:0] == 1'b1) begin
	 res <= 1'b1;
      end
   end

   always @(posedge clk) begin
      cnt = cnt + 1;
      if (cnt >= 10) begin
	 $write("*-* All Finished *-*\n");
	 $finish;
      end
   end

endmodule

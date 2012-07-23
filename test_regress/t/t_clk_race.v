// DESCRIPTION: Verilator: Verilog Test module
//
// Demonstrate a race between a clock and non-clock.
//
// Some other tools choose always to allow the clock to beat the non-clock.
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2012 by Jeremy Bennett.

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
   always @(posedge clk) begin
      sub_clk <= ~sub_clk;
   end

   always @(posedge clk) begin
      sub_comb <= sub_comb + 1;
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

// DESCRIPTION: Verilator: Verilog Test module
//
// Trigger the BLKLOOPINIT warning
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2013 by Jeremy Bennett, Embecosm.

`define LOOP_SIZE 10000
module t (/*AUTOARG*/
   // Inputs
   clk
   );
   input clk;

   integer 	i;
   reg [31:0] 	arr [0:`LOOP_SIZE - 1];

   initial begin
      arr[0]              = 32'ha5a5a5a5;
      arr[`LOOP_SIZE - 1] = 32'hdeadbeef;
   end

   always @(posedge clk) begin
      for (i = 1; i < `LOOP_SIZE; i++) begin
	 arr[i] <= arr[i - 1];
      end

      if (arr[`LOOP_SIZE - 1] == 32'hdeadbeef) begin
	 /* Delayed assignment means this should not yet be changed. */
	 $write("*-* All Finished *-*\n");
	 $finish;
      end
      else begin
	 $stop;
      end
   end

endmodule

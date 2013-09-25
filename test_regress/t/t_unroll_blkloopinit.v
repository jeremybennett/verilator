// DESCRIPTION: Verilator: Verilog Test module
//
// This files is used to generated the BLKLOOPINIT error which
// is actually caused by not being able to unroll the for loop.
//
// The reason for not being able to unroll is that the CONDITION
// in the for loop is not a "> >= < <=".
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2013 by Jie Xu.

module t (/*AUTOARG*/
   // Inputs
   clk
   );
   input clk;
   reg    [3:0] tmp [3:0];

   initial begin
       tmp[2] = 4'b0010;
       tmp[3] = 4'b0011;
   end

   // Test loop
   always @ (posedge clk) begin
       int i;
      // the following for-loop can't be UNROLLED
      for (i = 0; (i < 4) && (i > 1); i++) begin
         tmp[i] <= tmp[i-i];
      end

      if (tmp[3] != 4'b0011) begin
	 $stop;
      end
      else begin
	 $write("*-* All Finished *-*\n");
	 $finish;
      end
   end

endmodule

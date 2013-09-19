// DESCRIPTION: Verilator: Verilog Test module
//
// Use this file as a template for submitting bugs, etc.
// This module takes a single clock input, and should either
//	$write("*-* All Finished *-*\n");
//	$finish;
// on success, or $stop.
//
// The code as shown applies a random vector to the Test
// module, then calculates a CRC on the Test module's outputs.
//
// **If you do not wish for your code to be released to the public
// please note it here, otherwise:**
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2013 by ____YOUR_NAME_HERE____.

module t (/*AUTOARG*/
	  // Outputs
	  a,
	  b,
	  // Inputs
	  clk,
	  c
   );
   output a;
   output b;
   input clk;
   input c;

   always @(*) begin
      a = b;
      b = c;
   end

endmodule

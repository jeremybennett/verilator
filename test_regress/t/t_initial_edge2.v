// DESCRIPTION: Verilator: Verilog Test module
//
// Use this file as a template for submitting bugs, etc.
// This module takes a single clock input, and should either
//	$write("*-* All Finished *-*\n");
//	$finish;
// on success, or $stop.
//
// Demonstration of problem with --initial-edge being overridden by initial
// blocks.
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2013 by ____YOUR_NAME_HERE____.

module t (/*AUTOARG*/
   // Inputs
   clk
   );
   input clk;

   reg startpulse;    // Will be 0 at startup

   initial begin
      startpulse = 1;
   end

   always @ (posedge startpulse) begin
      $write("*-* All Finished *-*\n");
      $finish;
   end

endmodule

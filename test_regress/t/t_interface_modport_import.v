// DESCRIPTION: Verilator: Verilog Test module
//
// A test of the import parameter used with modport
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2013 by Jeremy Bennett.

interface test_if
  (
   // Inputs
   input logic  clk
   );

   // Modport
     modport mp(
                import myfunc
		);

   function automatic logic myfunc (input logic val);
      begin
	 myfunc = (val == 1'b0);
      end
   endfunction

endinterface // test_if


module t (/*AUTOARG*/
   // Inputs
   clk
   );
   input clk;

   test_if v (.clk (clk));

   testmod testmod_i (.v (v.mp));

endmodule


module testmod
  (
   test_if.mp  v
   );

   always_comb begin
      if (v.myfunc (1'b0)) begin
	 $write("*-* All Finished *-*\n");
	 $finish;
      end
      else begin
	 $stop;
      end
   end
endmodule

// DESCRIPTION: Verilator: Verilog Test module
//
// A test of the export parameter used with modport
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2013 by Jeremy Bennett.

interface test_if;

   // Interface variable
   logic 	data;

   // Modport
   modport mp(
              export  myfunc,
	      output  data
	      );

endinterface // test_if


module t (/*AUTOARG*/
   // Inputs
   clk
   );
   input clk;

   test_if i ();

   function automatic logic i.myfunc (input logic val);
      begin
	 myfunc = (val == 1'b0);
      end
   endfunction

   testmod testmod_i (.clk (clk),
		      .i (i.mp));

endmodule


module testmod
  (
   input clk,
   test_if.mp  i
   );

   always @(posedge clk) begin
      i.data = 1'b0;
      if (i.myfunc (1'b0)) begin
	 $write("*-* All Finished *-*\n");
	 $finish;
      end
      else begin
	 $stop;
      end
   end
endmodule

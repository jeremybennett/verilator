// DESCRIPTION: Verilator: Verilog 2001 test for short-circuiting
//
// The given generate loops should only access valid bits of mask, since that
// is defined by SIZE. However since the loop range is larger, this only works
// if short-circuited evaluation of the generate loop is in place.
//
// This test is only valid for Verilog, not SystemVerilog, since it uses
// short-circuting of bitwise AND and bitwise OR, which is described in the
// Verilog standard, but explicitly forbidden in the SystemVerilog standard.

// This file ONLY is placed into the Public Domain, for any use, without
// warranty, 2012 by Jeremy Bennett.


`define MAX_SIZE  4


module t (/*AUTOARG*/
   // Inputs
   clk
   );
   input clk;

   // Set the parameters, so that we use a size less than MAX_SIZE
   test_gen
     #(.SIZE (2),
       .MASK (2'b11))
     i_test_gen (.clk (clk));

   // This is only a compilation test, but for good measure we do one clock
   // cycle.
   integer count;

   initial begin
      count = 0;
   end

   always @(posedge clk) begin
      if (count == 1) begin
	 $write("*-* All Finished *-*\n");
	 $finish;
      end
      else begin
	 count = count + 1;
      end
   end

endmodule // t


module test_gen

  #( parameter
     SIZE = `MAX_SIZE,
     MASK = `MAX_SIZE'b0)

 (/*AUTOARG*/
   // Inputs
   clk
   );

   input clk;

   // Generate blocks that rely on short-circuiting of the logic to avoid
   // errors. We select those that are valid in Verilog 2001, but not
   // SystemVerilog.
   generate
      genvar g;

      for (g = 0; g < `MAX_SIZE; g = g + 1) begin
         if ((g < SIZE) & MASK[g]) begin
	    always @(posedge clk) begin
`ifdef TEST_VERBOSE
	       $write ("Bitwise AND generate if MASK [%1d] = %d\n", g, MASK[g]);
`endif
	       if (g >= SIZE) begin
		  $stop;
	       end
	    end
	 end
      end
   endgenerate

   generate
      for (g = 0; g < `MAX_SIZE; g = g + 1) begin
         if (!((g >= SIZE) | ~MASK[g])) begin
	    always @(posedge clk) begin
`ifdef TEST_VERBOSE
	       $write ("Bitwise OR generate if MASK [%1d] = %d\n", g, MASK[g]);
`endif
	       if (g >= SIZE) begin
		  $stop;
	       end
	    end
	 end
      end
   endgenerate

endmodule

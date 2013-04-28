// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2003 by Wilson Snyder.

// Also check that SystemC is ordering properly
// verilator lint_on IMPERFECTSCH

module t (/*AUTOARG*/
   // Outputs
   o1, o8, o16, o32, o64, o65, o128, o513, obv1, obv16,
   // Inputs
   clk, i1, i8, i16, i32, i64, i65, i128, i513, ibv1, ibv16
   );

   input clk;

   input 	 i1;
   input [7:0]	 i8;
   input [15:0]	 i16;
   input [31:0]	 i32;
   input [63:0]	 i64;
   input [64:0]	 i65;
   input [127:0] i128;
   input [512:0] i513;

   output 	  o1;
   output [7:0]   o8;
   output [15:0]  o16;
   output [31:0]  o32;
   output [63:0]  o64;
   output [64:0]  o65;
   output [127:0] o128;
   output [512:0] o513;

   input [0:0] 	 ibv1 /*verilator sc_bv*/;
   input [15:0]  ibv16 /*verilator sc_bv*/;

   output [0:0]   obv1 /*verilator sc_bv*/;
   output [15:0]  obv16 /*verilator sc_bv*/;

   always @ (posedge clk) begin
      o1 <= i1;
      o8 <= i8;
      o16 <= i16;
      o32 <= i32;
      o64 <= i64;
      o65 <= i65;
      o128 <= i128;
      o513 <= i513;
      obv1 <= ibv1;
      obv16 <= ibv16;
   end

endmodule

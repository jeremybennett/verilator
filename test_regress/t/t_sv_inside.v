// DESCRIPTION: Verilator: System Verilog test of case and if
//
// This code instantiates and runs a simple CPU written in System Verilog.
//
// This file ONLY is placed into the Public Domain, for any use, without
// warranty.

// Contributed 2012 by M W Lund, Atmel Corporation and Jeremy Bennett, Embecosm.


module t (/*AUTOARG*/
   // Inputs
   clk
   );

   input clk;

   /*AUTOWIRE*/

   // **************************************************************************
   // Regs and Wires
   // **************************************************************************

   reg 	   rst;
   integer rst_count;


   st3_testbench st3_testbench_i (/*AUTOINST*/
				  // Inputs
				  .clk			(clk),
				  .rst			(rst));


   // **************************************************************************
   // Reset Generation
   // **************************************************************************

   initial begin
      rst       = 1'b1;
      rst_count = 0;
   end

   always @( posedge clk ) begin
      if (rst_count < 2) begin
	 rst_count++;
      end
      else begin
	 rst = 1'b0;
      end
   end


   // **************************************************************************
   // Closing message
   // **************************************************************************

   final begin
      $write("*-* All Finished *-*\n");
   end


endmodule


module st3_testbench (/*AUTOARG*/
   // Inputs
   clk, rst
   );

   input  clk;
   input  rst;

   // Declarations
   logic            clk;
   logic            rst;
   logic [8*16-1:0] wide_input_bus;
   logic [47:0]     selected_out;
   logic [3:0] 	    cnt_reg;
   integer 	    i;

   initial begin
      wide_input_bus = {8'hf5, 8'hef, 8'hd5, 8'hc5,
                        8'hb5, 8'ha5, 8'h95, 8'h85,
                        8'ha7, 8'ha6, 8'ha5, 8'ha4,
                        8'ha3, 8'ha2, 8'ha1, 8'ha0};
      i = 0;
   end


   counter
     counter_i
       (/*AUTOINST*/
	// Outputs
	.cnt_reg                        (cnt_reg[3:0]),
	// Inputs
	.clk                             (clk),
	.rst                             (rst));

   simple_test_3f
     stf
       (.wide_input_bus                  (wide_input_bus[8*16-1:0]),
	.selector              (cnt_reg),
	.selected_out          (selected_out[47:40]));


   // Logic to print outputs and then finish.
   always @(posedge clk) begin
      if (i < 50) begin
`ifdef TEST_VERBOSE
	 $write("%x %x", simple_test_3_i.cnt_reg, selected_out);
`endif
	 i <= i + 1;
      end
      else begin
	 $finish();
      end
   end // always @ (posedge clk)

endmodule


// Module testing:
// - Unique case
// - Priority case
// - Unique if
// - ++, --, =- and =+ operands.

module simple_test_3
  (input logic [8*16-1:0] wide_input_bus,
   input logic 	       rst,
   input logic 	       clk,

   // Outputs
   output logic [47:0] selected_out);

endmodule // simple_test_3


module counter
  (output logic [3:0]          cnt_reg, // Registered version of cntA
   input logic clk,                      // Clock
   input logic rst);                     // Synchronous reset

   // Counter
   always_ff @(posedge clk) begin
      if (rst)
        cnt_reg <= 0;
      else
        cnt_reg <= cnt_reg + 1;
   end // always_ff @

endmodule // counter


// Test of "inside"
module simple_test_3f
  (input logic [8*16-1:0] wide_input_bus,
   input logic [3:0]  selector,
   output logic [7:0] selected_out);


   always_comb begin
      if ( selector[3:0] inside { 4'b?00?, 4'b1100})      // Matching 0000, 0001, 1000, 1100, 1001
	// if ( selector[3:2] inside { 2'b?0, selector[1:0]})
        selected_out = wide_input_bus[  7:  0];
      else
        priority casez (selector[3:0])
          4'b0?10: selected_out = wide_input_bus[ 15:  8]; // Matching 0010 and 0110
          4'b0??0: selected_out = wide_input_bus[ 23: 16]; // Overlap: only 0100 remains (0000 in "if" above)
          4'b0100: selected_out = wide_input_bus[ 31: 24]; // Overlap: Will never occur
          default: selected_out = wide_input_bus[127:120];   // Remaining 0011,0100,0101,0111,1010,1011,1101,1110,1111
	endcase // case (selector)
   end

endmodule // simple_test_3f

// Local Variables:
// verilog-library-directories:("." "t_sv_cpu_code")
// End:

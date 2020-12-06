`timescale 1ns / 1ps
module HLSM (Clk, Rst, Start, Done, c, b, a, x, z);
	input Clk, Rst, Start;
	output reg Done;
	reg[2:0] present_state, next_state;
	input [31:0] a, b, c;
	output reg [31:0] z, x;
	reg [31:0] d, e, f, g, h;
	reg [0:0] dLTe, dEQe;
	localparam Wait = 3'b000;
	localparam sig1 = 3'b001;
	localparam sig2 = 3'b010;
	localparam sig3 = 3'b011;
	localparam sig4 = 3'b100;
	localparam sig5 = 3'b101;
	localparam Final = 3'b110;
	always @(posedge Clk, posedge Rst)
	begin
		if(Rst == 1'b1)
			present_state = Wait;
		else
			present_state = next_state;
	end

	always @(posedge Clk)
	begin
		if(present_state == Final)
			Done = 1'b1;
		else
			Done = 1'b0;
	end

	always @(present_state)
	begin
		case(present_state)
		Wait:
		begin
			if(Start == 1'b1)
				next_state = sig1;
			else
				next_state = Wait;
		end

		sig1:
		begin
				d = a + b;
				e = a + c;
				next_state = sig2;
		end

		sig2:
		begin
				f = a - b  ;
				dEQe = d == e;
				dLTe = d < e;
				next_state = sig3;
		end

		sig3:
		begin
				g = dLTe ? d : e ;
				next_state = sig4;
		end

		sig4:
		begin
				h = dEQe ? g : f ;
				x = g << dLTe;
				next_state = sig5;
		end

		sig5:
		begin
				z = h >> dEQe;
				next_state = Final;
		end

		Final:
		begin
				next_state = Wait;
		end

		endcase
	end
endmodule

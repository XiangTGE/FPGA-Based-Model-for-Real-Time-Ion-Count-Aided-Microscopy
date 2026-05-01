`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: Xiang Jin
// 
// Create Date: 02/09/2026 10:35:25 AM 
// Design Name: 
// Module Name: peak_finder 
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision: 
// Revision 0.01 - File Created
// Additional Comments: 
// 
//////////////////////////////////////////////////////////////////////////////////


/*
* Detects peaks by checking if the sample is greater than those in the immediate vincinity of it
* */
module peak_finder(
    input clk,
    input rst,
    input signed [31:0] din,
    output reg valid_peak,
    output reg [31:0] dout
    );
    
    reg signed [31:0] previous; 
    reg signed [31:0] previous_2;
    reg signed [31:0] threshold = 75_000;   // Threshold away points that lie below the threshold
    
    
    // Logic
    always @(posedge clk) begin
    
        if (rst) begin
        
            // Reset registers to most negative value (2's complement)
            previous <= {1'b1, {31{1'b0}} };
            previous_2 <= { 1'b1, {31{1'b0}} };
            
            valid_peak <= 0;
        end 
        else begin
        
            // Peak finding logic
            if (previous > previous_2 && previous > din && previous > threshold) begin
            
                dout <= previous;
                valid_peak <= 1;
            end 
            else begin
            
                valid_peak <= 0;
            end
            
            // Update registers
            previous_2 <= previous;
            previous <= din;
        end
    end
    
endmodule

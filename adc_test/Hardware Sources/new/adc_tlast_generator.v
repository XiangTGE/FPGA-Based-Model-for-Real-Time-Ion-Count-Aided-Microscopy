`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: Xiang Jin
// 
// Create Date: 02/27/2026 11:12:14 AM
// Design Name: 
// Module Name: adc_tlast_generator
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Revision 1.00 - Made TLAST not depend on whether TREADY is high (TVALID always high) - 14 March 2026
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


// Generates a tlast signal when there is a set number of samples that have been transferred from
// ADC controller to AXI-FIFO
// NOTE: If FIFO is not ready to take data (tready low) then there will be lost data and 
// counter won't update
// NOTE: Made with help from ChatGPT on 13 March 2026
// NOTE: Changed how TLAST is outputted; uses reg instead of wire now (courtesy of Gemini) 15 March 2026
// NOTE: Turned rst pin into active low because system-wide reset system assumes active-low modules 
module adc_tlast_generator(
    input clk,
    input rst,
    input tvalid,
    input tready,
    output reg tlast
);

    // 8-bit counter is enough for 256 beats (0 to 255)
    reg [7:0] counter;

    always @(posedge clk) begin
        if (!rst) begin
            counter <= 8'd0;
            tlast   <= 1'b0;
        end 
        else if (tvalid && tready) begin
            // If we are currently on the 255th beat, the NEXT beat is the start of a new packet
            if (counter == 8'd254) begin
                counter <= 8'd255;
                tlast   <= 1'b1;
            end 
            else if (counter == 8'd255) begin
                counter <= 8'd0;
                tlast   <= 1'b0;
            end 
            else begin
                counter <= counter + 1'b1;
                tlast   <= 1'b0;
            end
        end
        // If no handshake, counter and tlast hold their current values automatically
    end

endmodule

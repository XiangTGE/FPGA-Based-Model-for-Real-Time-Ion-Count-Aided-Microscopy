`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: Xiang Jin
// 
// Create Date: 03/02/2026 10:17:30 AM
// Design Name: 
// Module Name: adc_tlast_generator_tb
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


// Testbench for ADC TLAST generator
// NOTE: Updated to explore different scenarios (courtesy of Gemini) 15 March 2026)
module adc_tlast_generator_tb();

    // Inputs
    reg clk;
    reg rst;
    reg tvalid;
    reg tready;

    // Output
    wire tlast;

    // Instantiate the Unit Under Test (UUT)
    adc_tlast_generator uut (
        .clk(clk),
        .rst(rst),
        .tvalid(tvalid),
        .tready(tready),
        .tlast(tlast)
    );

    // Clock generation (100MHz)
    always #5 clk = ~clk;

    initial begin
        // Initialize Inputs
        clk = 0;
        rst = 1;
        tvalid = 0;
        tready = 0;

        // Reset Pulse
        #20;
        rst = 0;
        #10;

        // --- Scenario 1: Continuous Data Flow ---
        tvalid = 1;
        tready = 1;
        
        // Wait for 256 beats (256 * 10ns = 2560ns)
        repeat (256) @(posedge clk);
        
        // --- Scenario 2: Data Gap (tvalid drops) ---
        tvalid = 0;
        #30;
        tvalid = 1;
        
        // --- Scenario 3: Backpressure (tready drops) ---
        // Let's wait until we are near the end of the next packet
        repeat (250) @(posedge clk);
        
        // Stall right before tlast should happen
        tready = 0; 
        #50;
        tready = 1; // Resume
        
        repeat (10) @(posedge clk);

        $display("Simulation Finished");
        $finish;
    end

endmodule

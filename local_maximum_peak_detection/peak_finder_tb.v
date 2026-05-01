`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: Xiang Jin
// 
// Create Date: 02/09/2026 10:46:31 AM
// Design Name: 
// Module Name: peak_finder_tb
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
* Testbench for peak_finder module
* 
*/
module peak_finder_tb();


    // I/O
    reg clk;
    reg rst;
    reg signed [31:0] din;
    wire signed [31:0] dout;
    wire valid_peak;
    
    
    // Device under test
    peak_finder DUT (clk, rst, din, valid_peak, dout);
    
    
    // Initial parameters
    initial begin
    
        clk <= 0;
        rst <= 0;
        din <= 0;
    end
    
    
    // Feed inputs from HIM? 
//    initial begin
    
//        #100 din = 1000;
//        #10 din = 1234;
//        #10 din = 900;
//        #50 $finish;
//    end

    
    // File variables
    integer file, times, r, Scan;
    reg signed [31:0] SE_Detect;
    
    
    initial begin
    
        // NOTE: SE voltage is actually scaled by 10e6 and truncated after decimal to avoid floating points
        file = $fopen("Input text file here", "r");
        
        // Read txt file line by line
        while (!$feof(file)) begin
        
            r = $fscanf(file, "%f,%f,%f\n", times, SE_Detect, Scan);
            if (times > $time)
                #(times - $time);
            
            din = SE_Detect;
        end
        
        $fclose(file);
    end
    
    
    // Write results into txt file
    integer fileWrite;
    reg [23:0] Pixel_ID_Prev = 100; // Random value that is not zero; want it to update to 1 right when start of frame begins
    reg writeEnable = 0;            // Determines whether to write to output file
    initial begin
        fileWrite = $fopen("File path to output file here", "w");
        if (fileWrite == 0) begin
        
            $display("Error: could not write to file.\n");
            $finish;
        end
        else
            $display("File writing initialized!\n");
    end
    
    
    // Tell when to write to output file
    always @ (posedge valid_peak) begin
    
        if (fileWrite != 0) begin
        
            // Write one line to file that corresponds to one pixel
            // Format: [Sample #], [Voltage at Peak]
            // NOTE: Sample # is just the simulation time scaled down by 10 because each 
            // sample is simulated to arrive in 10ns intervals. 
            $fdisplay(fileWrite, "%0d,%0d", $time/10, SE_Detect);
        end
    end
    
    
    // Close file when done
    initial begin
    
        #10000
        if (fileWrite != 0)
            $fclose(fileWrite);
        $finish;
    end
    
    
    // Clk (100 MHz)
    always #5 clk <= ~clk;


endmodule

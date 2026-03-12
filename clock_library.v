`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 12/17/2025 10:30:30 AM
// Design Name: 
// Module Name: clock_library
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


module clock_usec(
    input clk, reset_p,
    output reg clk_usec,
    output clk_usec_nedge, clk_usec_pedge);
    
    reg [5:0] cnt_sysclk;
    always @(posedge clk, posedge reset_p)begin
        if(reset_p)begin
            cnt_sysclk = 0;
            clk_usec = 0;
        end
        else begin
            if(cnt_sysclk >=49)begin
                cnt_sysclk = 0;
                clk_usec = ~clk_usec;
            end
            else cnt_sysclk = cnt_sysclk + 1;
        end
    end
    
    edge_detector_n ed(.clk(clk), .reset_p(reset_p),
                       .cp(clk_usec), .p_edge(clk_usec_pedge),
                       .n_edge(clk_usec_nedge));
endmodule

module FPGC (
    // Clock and reset
    input wire sys_clk_50,
    input wire sys_clk_27,
    input wire sys_rstn,
    
    // LEDs
    output wire        led1,
    output wire        led2
);

assign led1 = 1'b0;
assign led2 = 1'b1;

endmodule

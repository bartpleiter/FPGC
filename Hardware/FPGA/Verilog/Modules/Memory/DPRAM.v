/*
 * DPRAM
 * Dual port, dual clock RAM implementation (for L1 cache)
 * One port for CPU pipeline and one port for cache controller
 * Cache line format: {256bit_data, 14bit_tag, 1bit_valid} = 271 bits
 */
module DPRAM #(
    parameter WIDTH     = 271, // 8 words per cache line + 14 bit tag + 1 bit for valid
    parameter WORDS     = 128, // 128 cache lines
    parameter ADDR_BITS = 7    // 128 cache lines require 7 address bits
) (
    input  wire                     clk_pipe,
    input  wire [    WIDTH-1:0]     pipe_d,
    input  wire [ADDR_BITS-1:0]     pipe_addr,
    input  wire                     pipe_we,
    output reg  [    WIDTH-1:0]     pipe_q,

    input  wire                     clk_ctrl,
    input  wire [    WIDTH-1:0]     ctrl_d,
    input  wire [ADDR_BITS-1:0]     ctrl_addr,
    input  wire                     ctrl_we,
    output reg  [    WIDTH-1:0]     ctrl_q
);

reg [WIDTH-1:0] ram[0:WORDS-1];

// CPU pipeline port
always @(posedge clk_pipe)
begin
    pipe_q <= ram[pipe_addr];
    if (pipe_we)
    begin
        pipe_q        <= pipe_d;
        ram[pipe_addr] <= pipe_d;
        //$display("%d: DPRAM WRITE (pipe): addr=0x%h, data=0x%h, tag=0x%h, valid=%b, dirty=%b", 
        //         $time, pipe_addr, pipe_d[273:18], pipe_d[17:2], pipe_d[1], pipe_d[0]);
    end
end

// Cache controller port
always @(posedge clk_ctrl)
begin
    ctrl_q <= ram[ctrl_addr];
    if (ctrl_we)
    begin
        ctrl_q        <= ctrl_d;
        ram[ctrl_addr] <= ctrl_d;
        //$display("%d: DPRAM WRITE (ctrl): addr=0x%h, data=0x%h, tag=0x%h, valid=%b, dirty=%b", 
        //         $time, ctrl_addr, ctrl_d[273:18], ctrl_d[17:2], ctrl_d[1], ctrl_d[0]);
    end
end

// Initialize RAM to be zero
integer i;
initial
begin
    for (i = 0; i < WORDS; i = i + 1)
    begin
        ram[i] = {WIDTH{1'b0}};
    end
end

endmodule

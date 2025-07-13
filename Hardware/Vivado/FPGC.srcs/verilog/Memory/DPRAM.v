/*
* Dual port, single clock RAM implementation (for L1 cache)
* One port for CPU pipeline and one port for cache controller
*/
module DPRAM #(
    parameter WIDTH = 274, // 8 words per cache line + 16 bit tag + 2 bits for valid and dirty
    parameter WORDS = 128, // 128 cache lines
    parameter ADDR_BITS = 7, // 128 cache lines require 7 address bits
    parameter LIST = "memory/l1i.list"
) (
    input  wire                 clk,
    input  wire [    WIDTH-1:0] pipe_d,
    input  wire [ADDR_BITS-1:0] pipe_addr,
    input  wire                 pipe_we,
    output reg  [    WIDTH-1:0] pipe_q,

    input  wire [    WIDTH-1:0] ctrl_d,
    input  wire [ADDR_BITS-1:0] ctrl_addr,
    input  wire                 ctrl_we,
    output reg  [    WIDTH-1:0] ctrl_q
);

reg [WIDTH-1:0] ram[0:WORDS-1];

// CPU pipeline port
always @(posedge clk)
begin
    pipe_q <= ram[pipe_addr];
    if (pipe_we)
    begin
        pipe_q        <= pipe_d;
        ram[pipe_addr] <= pipe_d;
    end
end

// Cache controller port
always @(posedge clk)
begin
    ctrl_q <= ram[ctrl_addr];
    if (ctrl_we)
    begin
        ctrl_q        <= ctrl_d;
        ram[ctrl_addr] <= ctrl_d;
    end
end

// Initialize RAM
initial
begin
    $readmemb(LIST, ram);
end

endmodule

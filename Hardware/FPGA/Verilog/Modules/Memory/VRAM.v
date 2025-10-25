/*
* Dual port, dual clock VRAM implementation
* One port for CPU and one port for GPU
* Used indirectly as solution for clock domain crossing between CPU and GPU
*/
module VRAM #(
    parameter WIDTH = 32,
    parameter WORDS = 1056,
    parameter ADDR_BITS = 11,
    parameter LIST = "memory/vram.list"
) (
    input  wire                 cpu_clk,
    input  wire [    WIDTH-1:0] cpu_d,
    input  wire [ADDR_BITS-1:0] cpu_addr,
    input  wire                 cpu_we,
    output reg  [    WIDTH-1:0] cpu_q,

    input  wire                 gpu_clk,
    input  wire [    WIDTH-1:0] gpu_d,
    input  wire [ADDR_BITS-1:0] gpu_addr,
    input  wire                 gpu_we,
    output reg  [    WIDTH-1:0] gpu_q
);

reg [WIDTH-1:0] vram[0:WORDS-1];

// CPU port
always @(posedge cpu_clk)
begin
    cpu_q <= vram[cpu_addr];
    if (cpu_we)
    begin
        cpu_q          <= cpu_d;
        vram[cpu_addr] <= cpu_d;
    end
end

// GPU port
always @(posedge gpu_clk)
begin
    gpu_q <= vram[gpu_addr];
    if (gpu_we)
    begin
        gpu_q          <= gpu_d;
        vram[gpu_addr] <= gpu_d;
    end
end

// Initialize VRAM
initial
begin
    $readmemb(LIST, vram);
end

endmodule

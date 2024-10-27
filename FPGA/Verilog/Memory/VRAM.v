/*
* Dual port, Dual clock VRAM implementation
* One port is accessed by the CPU, the other is accessed by the GPU
*/
module VRAM
#(
    parameter WIDTH = 32,
    parameter WORDS = 1056,
    parameter ADDR_BITS = 11,
    parameter LIST  = "/home/bart/Documents/FPGA/FPGC6/Verilog/memory/vram32.list"
) 
(
  input                   cpu_clk,        
  input      [WIDTH-1:0]  cpu_d,
  input      [ADDR_BITS-1:0]       cpu_addr,
  input                   cpu_we,
  output reg [WIDTH-1:0]  cpu_q, 

  input                   gpu_clk,
  input      [WIDTH-1:0]  gpu_d,
  input      [ADDR_BITS-1:0]       gpu_addr,
  input                   gpu_we,
  output reg [WIDTH-1:0]  gpu_q 
);

reg [WIDTH-1:0] ram [0:WORDS-1]; //basically the memory cells

//cpu port
always @(posedge cpu_clk) 
begin
  cpu_q <= ram[cpu_addr];
  if (cpu_we)
  begin
    cpu_q         <= cpu_d;
    ram[cpu_addr] <= cpu_d;
  end
end

//gpu port
always @(posedge gpu_clk) 
begin
  gpu_q <= ram[gpu_addr];
  if (gpu_we)
  begin
    gpu_q         <= gpu_d;
    ram[gpu_addr] <= gpu_d;
  end
end

//initialize VRAM
initial 
begin
  $readmemb(LIST, ram);
end
    
endmodule
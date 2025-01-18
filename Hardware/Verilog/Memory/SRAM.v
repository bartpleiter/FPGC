/*
* SRAM implementation
*/
module SRAM
#(
    parameter WIDTH = 32,
    parameter WORDS = 4096,
    parameter ADDR_BITS = 12,
    parameter LIST  = "FPGA/Data/Simulation/sram.list"
) 
(
  input                   cpu_clk,        
  input      [WIDTH-1:0]  cpu_d,
  input      [ADDR_BITS-1:0]       cpu_addr,
  input                   cpu_we,
  output reg [WIDTH-1:0]  cpu_q
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

//initialize VRAM
initial 
begin
  $readmemb(LIST, ram);
end

endmodule

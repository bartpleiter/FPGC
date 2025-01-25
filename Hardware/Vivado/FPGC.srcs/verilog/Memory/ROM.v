/*
* Internal FPGA ROM
*/
module ROM #(
    parameter WIDTH = 32,
    parameter WORDS = 512,
    parameter ADDR_BITS = 9,
    parameter LIST = "memory/rom.list"
) (
    input  wire                 clk,
    input  wire [ADDR_BITS-1:0] addr,
    output reg  [    WIDTH-1:0] q
);

reg [WIDTH-1:0] rom[0:WORDS-1];

always @(posedge clk) 
begin
    q <= rom[addr];
end

initial
begin
    $readmemb(LIST, rom);
end

endmodule

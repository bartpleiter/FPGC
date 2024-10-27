/*
* Internal FPGA ROM
* 512x32bits
*/
module ROM(
    input clk,
    input reset,
    input [8:0] address,
    output reg [31:0] q
);

reg [31:0] rom [0:511];

always @(posedge clk) 
begin
    q <= rom[address];
end

initial
begin
    $readmemb("/home/bart/Documents/FPGA/FPGC6/Verilog/memory/rom.list", rom);
end

endmodule
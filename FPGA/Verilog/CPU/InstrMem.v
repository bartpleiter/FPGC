/*
* Instruction Memory
*/

module InstrMem(
    input clk, clk100,
    input reset,
    input [31:0] addr,
    output [31:0] q,
    output hit,

    output [26:0]   bus_l1i_addr,
    output          bus_l1i_start,
    input [31:0]    bus_l1i_q,
    input           bus_l1i_done,
    input           bus_l1i_ready,

    output [8:0]    bus_i_rom_addr,
    input [31:0]    bus_i_rom_q,

    input clear, hold
);

assign hit = 1'b1;
assign bus_i_rom_addr = addr;
assign q = bus_i_rom_q;

always @(posedge clk100)
begin
    if (reset)
    begin
    end
    else
    begin
    end
end

endmodule
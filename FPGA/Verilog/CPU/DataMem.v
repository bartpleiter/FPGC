/*
* Data Memory
*/

module DataMem(
    input wire          clk, clk100, reset,
    input wire  [31:0]  addr,
    input wire          we,
    input wire          re,
    input wire  [31:0]  data,
    output wire [31:0]  q,
    output              busy,

    output [26:0]   bus_l1d_addr,
    output          bus_l1d_start,
    input [31:0]    bus_l1d_data,
    input           bus_l1d_we,
    input [31:0]    bus_l1d_q,
    input           bus_l1d_done,
    input           bus_l1d_ready,

    // TODO: VRAM busses, SPIflash bus, MU bus

    input wire          clear, hold
);

assign busy = 1'b0;


endmodule
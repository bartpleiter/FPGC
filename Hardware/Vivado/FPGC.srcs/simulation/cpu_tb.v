/*
 * Testbench for the CPU (B32P2).
 * Designed to be used with the Icarus Verilog simulator for simplicity
 */
`timescale 1ns / 1ps

`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/B32P2.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/Regr.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/Regbank.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/InstructionDecoder.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/ALU.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/ControlUnit.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/Stack.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/BranchJumpUnit.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/AddressDecoder.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/ROM.v"

module cpu_tb ();

reg clk = 1'b0;
reg reset = 1'b0;

//-----------------------ROM-------------------------
wire [8:0] rom_addr;
wire [31:0] rom_q;
wire rom_oe;
ROM #(
    .WIDTH(32),
    .WORDS(512),
    .ADDR_BITS(9),
    .LIST("/home/bart/Documents/FPGA/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list")
) rom (
    .clk (clk),
    .addr(rom_addr),
    .oe  (rom_oe),
    .q   (rom_q)
);

//-----------------------CPU-------------------------
B32P2 cpu (
    // Clock and reset
    .clk(clk),
    .reset(reset),

    // L1i cache
    .icache_addr(rom_addr),
    .icache_oe(rom_oe),
    .icache_q(rom_q)
);

initial
begin
    $dumpfile("Hardware/Vivado/FPGC.srcs/simulation/output/cpu.vcd");
    $dumpvars;

    repeat(100)
    begin
        #10 clk = ~clk;
    end
end

endmodule

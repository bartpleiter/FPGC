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
wire [8:0] rom_fe_addr;
wire [8:0] rom_mem_addr;
wire rom_fe_oe;
wire rom_fe_hold;
wire rom_mem_oe;
wire [31:0] rom_fe_q;
wire [31:0] rom_mem_q;

ROM #(
    .WIDTH(32),
    .WORDS(512),
    .ADDR_BITS(9),
    .LIST("/home/bart/Documents/FPGA/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list")
) rom (
    .clk (clk),
    .reset(reset),

    .fe_addr(rom_fe_addr),
    .fe_oe(rom_fe_oe),
    .fe_q(rom_fe_q),
    .fe_hold(rom_fe_hold),

    .mem_addr(rom_mem_addr),
    .mem_oe(rom_mem_oe),
    .mem_q(rom_mem_q)
);

//-----------------------CPU-------------------------
B32P2 cpu (
    // Clock and reset
    .clk(clk),
    .reset(reset),

    // ROM (dual port)
    .rom_fe_addr(rom_fe_addr),
    .rom_fe_oe(rom_fe_oe),
    .rom_fe_q(rom_fe_q),
    .rom_fe_hold(rom_fe_hold),

    .rom_mem_addr(rom_mem_addr),
    .rom_mem_oe(rom_mem_oe),
    .rom_mem_q(rom_mem_q)
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

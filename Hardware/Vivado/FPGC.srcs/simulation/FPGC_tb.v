/*
 * Simulation testbench for FPGC
 */
`timescale 1ns / 1ps

`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/B32P2.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/Regr.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/Regbank.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/InstructionDecoder.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/ALU.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/MultiCycleALU.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/MultiCycleAluOps/MultuPipelined.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/MultiCycleAluOps/MultsPipelined.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/ControlUnit.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/Stack.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/BranchJumpUnit.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/AddressDecoder.v"

`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/ROM.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/VRAM.v"

`include "Hardware/Vivado/FPGC.srcs/verilog/GPU/FSX.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/GPU/BGWrenderer.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/GPU/PixelEngine.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/GPU/TimingGenerator.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/GPU/HDMI/RGB8toRGB24.v"

module FPGC_tb();

reg clkPixel = 1'b0;
reg clkTMDShalf = 1'b0;
wire clk;
assign clk = clkPixel; // For simulation purposes run the CPU at 25MHz

//-----------------------ROM-------------------------
wire [8:0] rom_fe_addr;
wire [8:0] rom_mem_addr;
wire rom_fe_oe;
wire rom_fe_hold;
wire [31:0] rom_fe_q;
wire [31:0] rom_mem_q;

ROM #(
    .WIDTH(32),
    .WORDS(512),
    .ADDR_BITS(9),
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/rom.list")
) rom (
    .clk (clk),

    .fe_addr(rom_fe_addr),
    .fe_oe(rom_fe_oe),
    .fe_q(rom_fe_q),
    .fe_hold(rom_fe_hold),

    .mem_addr(rom_mem_addr),
    .mem_q(rom_mem_q)
);


//---------------------------VRAM32---------------------------------
// VRAM32 I/O
wire [10:0] vram32_gpu_addr;
wire [31:0] vram32_gpu_d;
wire        vram32_gpu_we;
wire [31:0] vram32_gpu_q;

wire [10:0] vram32_cpu_addr;
wire [31:0] vram32_cpu_d;
wire        vram32_cpu_we; 
wire [31:0] vram32_cpu_q;

// GPU will not write to VRAM
assign vram32_gpu_we = 1'b0;
assign vram32_gpu_d  = 32'd0;

VRAM #(
    .WIDTH(32),
    .WORDS(1056),
    .ADDR_BITS(11),
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/memory/vram32.list")
) vram32 (
    //CPU port
    .cpu_clk (clk),
    .cpu_d   (vram32_cpu_d),
    .cpu_addr(vram32_cpu_addr),
    .cpu_we  (vram32_cpu_we),
    .cpu_q   (vram32_cpu_q),

    //GPU port
    .gpu_clk (clkPixel),
    .gpu_d   (vram32_gpu_d),
    .gpu_addr(vram32_gpu_addr),
    .gpu_we  (vram32_gpu_we),
    .gpu_q   (vram32_gpu_q)
);

//--------------------------VRAM8--------------------------------
// VRAM8 I/O
wire [13:0] vram8_gpu_addr;
wire [7:0]  vram8_gpu_d;
wire        vram8_gpu_we;
wire [7:0]  vram8_gpu_q;

wire [13:0] vram8_cpu_addr;
wire [7:0]  vram8_cpu_d;
wire        vram8_cpu_we;
wire [7:0]  vram8_cpu_q;

// GPU will not write to VRAM
assign vram8_gpu_we = 1'b0;
assign vram8_gpu_d  = 8'd0;

VRAM #(
    .WIDTH(8),
    .WORDS(8194),
    .ADDR_BITS(14),
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/memory/vram8.list")
) vram8 (
    // CPU port
    .cpu_clk (clk),
    .cpu_d   (vram8_cpu_d),
    .cpu_addr(vram8_cpu_addr),
    .cpu_we  (vram8_cpu_we),
    .cpu_q   (vram8_cpu_q),

    // GPU port
    .gpu_clk (clkPixel),
    .gpu_d   (vram8_gpu_d),
    .gpu_addr(vram8_gpu_addr),
    .gpu_we  (vram8_gpu_we),
    .gpu_q   (vram8_gpu_q)
);


//--------------------------VRAMPX--------------------------------
// VRAMPX I/O
wire [16:0] vramPX_gpu_addr;
wire [7:0]  vramPX_gpu_d;
wire        vramPX_gpu_we;
wire [7:0]  vramPX_gpu_q;

wire [16:0] vramPX_cpu_addr;
wire [7:0]  vramPX_cpu_d;
wire        vramPX_cpu_we;
wire [7:0]  vramPX_cpu_q;

// GPU will not write to VRAM
assign vramPX_gpu_we = 1'b0;
assign vramPX_gpu_d  = 8'd0;

VRAM #(
    .WIDTH(8),
    .WORDS(76800),
    .ADDR_BITS(17),
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/memory/vramPX.list")
) vramPX (
    // CPU port
    .cpu_clk (clk),
    .cpu_d   (vramPX_cpu_d),
    .cpu_addr(vramPX_cpu_addr),
    .cpu_we  (vramPX_cpu_we),
    .cpu_q   (vramPX_cpu_q),

    // GPU port
    .gpu_clk (clkPixel),
    .gpu_d   (vramPX_gpu_d),
    .gpu_addr(vramPX_gpu_addr),
    .gpu_we  (vramPX_gpu_we),
    .gpu_q   (vramPX_gpu_q)
);


//-----------------------FSX-------------------------
wire frameDrawn;
FSX fsx (
    // Clocks and reset
    .clkPixel(clkPixel),
    .clkTMDShalf(clkTMDShalf),
    .reset(1'b0),

    // HDMI
    .TMDS_clk_p(HDMI_CLK_P),
    .TMDS_clk_n(HDMI_CLK_N),
    .TMDS_d0_p (HDMI_D0_P),
    .TMDS_d0_n (HDMI_D0_N),
    .TMDS_d1_p (HDMI_D1_P),
    .TMDS_d1_n (HDMI_D1_N),
    .TMDS_d2_p (HDMI_D2_P),
    .TMDS_d2_n (HDMI_D2_N),

    // VRAM32
    .vram32_addr(vram32_gpu_addr),
    .vram32_q   (vram32_gpu_q),

    // VRAM8
    .vram8_addr(vram8_gpu_addr),
    .vram8_q   (vram8_gpu_q),

    // VRAMPX
    .vramPX_addr(vramPX_gpu_addr),
    .vramPX_q   (vramPX_gpu_q),
    
    // Parameters
    .halfRes(1'b0),

    // Interrupt signal
    .frameDrawn(frameDrawn)
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
    .rom_mem_q(rom_mem_q),

    // VRAM32
    .vram32_addr(vram32_cpu_addr),
    .vram32_d(vram32_cpu_d),
    .vram32_we(vram32_cpu_we),
    .vram32_q(vram32_cpu_q),

    // VRAM8
    .vram8_addr(vram8_cpu_addr),
    .vram8_d(vram8_cpu_d),
    .vram8_we(vram8_cpu_we),
    .vram8_q(vram8_cpu_q),

    // VRAMPX
    .vramPX_addr(vramPX_cpu_addr),
    .vramPX_d(vramPX_cpu_d),
    .vramPX_we(vramPX_cpu_we),
    .vramPX_q(vramPX_cpu_q)
);


initial
begin
    $dumpfile("Hardware/Vivado/FPGC.srcs/simulation/output/fpgc.vcd");
    $dumpvars;

    repeat(850000) // 850000 for exactly one frame at 640x480
    begin
        // clkTMDShalf: 125MHz, clkPixel: 25MHz
        #4 clkTMDShalf = ~clkTMDShalf;
        #4 clkTMDShalf = ~clkTMDShalf;
        #4 clkTMDShalf = ~clkTMDShalf;
        #4 clkTMDShalf = ~clkTMDShalf;
        #4 clkTMDShalf = ~clkTMDShalf; clkPixel = ~clkPixel;
    end
end


endmodule

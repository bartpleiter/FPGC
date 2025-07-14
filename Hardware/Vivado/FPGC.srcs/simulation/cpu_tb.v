/*
 * Testbench for the CPU (B32P2).
 * Designed to be used with the Icarus Verilog simulator
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
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/CacheControllerL1i.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/ROM.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/VRAM.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/DPRAM.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/MIG7Mock.v"

module cpu_tb ();

reg clk = 1'b0;
reg reset = 1'b0;

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
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/vram32.list")
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
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/vram8.list")
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
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/vramPX.list")
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

//-----------------------L1i RAM-------------------------

// DPRAM I/O signals
wire [273:0] l1i_pipe_d;
wire [6:0]   l1i_pipe_addr;
wire         l1i_pipe_we;
wire [273:0] l1i_pipe_q;

wire [273:0] l1i_ctrl_d;
wire [6:0]   l1i_ctrl_addr;
wire         l1i_ctrl_we;
wire [273:0] l1i_ctrl_q;

// CPU pipeline will not write to L1 RAM
assign l1i_pipe_we = 1'b0;
assign l1i_pipe_d  = 274'd0;

// DPRAM instance
DPRAM #(
    .WIDTH(274),
    .WORDS(128),
    .ADDR_BITS(7),
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/l1i.list")
) l1i_ram (
    .clk(clk),
    .pipe_d(l1i_pipe_d),
    .pipe_addr(l1i_pipe_addr),
    .pipe_we(l1i_pipe_we),
    .pipe_q(l1i_pipe_q),
    .ctrl_d(l1i_ctrl_d),
    .ctrl_addr(l1i_ctrl_addr),
    .ctrl_we(l1i_ctrl_we),
    .ctrl_q(l1i_ctrl_q)
);

//-----------------------L1d RAM-------------------------

// DPRAM I/O signals
wire [273:0] l1d_pipe_d;
wire [6:0]   l1d_pipe_addr;
wire         l1d_pipe_we;
wire [273:0] l1d_pipe_q;

wire [273:0] l1d_ctrl_d;
wire [6:0]   l1d_ctrl_addr;
wire         l1d_ctrl_we;
wire [273:0] l1d_ctrl_q;

// CPU pipeline will not write to L1 RAM
assign l1d_pipe_we = 1'b0;
assign l1d_pipe_d  = 274'd0;

// DPRAM instance
DPRAM #(
    .WIDTH(274),
    .WORDS(128),
    .ADDR_BITS(7),
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/l1d.list")
) l1d_ram (
    .clk(clk),
    .pipe_d(l1d_pipe_d),
    .pipe_addr(l1d_pipe_addr),
    .pipe_we(l1d_pipe_we),
    .pipe_q(l1d_pipe_q),
    .ctrl_d(l1d_ctrl_d),
    .ctrl_addr(l1d_ctrl_addr),
    .ctrl_we(l1d_ctrl_we),
    .ctrl_q(l1d_ctrl_q)
);

//-----------------------MIG7 Mock-------------------------

// MIG7Mock I/O signals
wire mig7_init_calib_complete;

wire [28:0] mig7_app_addr;
wire [2:0]  mig7_app_cmd;
wire        mig7_app_en;
wire        mig7_app_rdy;

wire [255:0] mig7_app_wdf_data;
wire         mig7_app_wdf_end;
wire [31:0]  mig7_app_wdf_mask;
wire         mig7_app_wdf_wren;
wire         mig7_app_wdf_rdy;

wire [255:0] mig7_app_rd_data;
wire         mig7_app_rd_data_end;
wire         mig7_app_rd_data_valid;

wire         mig7_app_sr_req = 1'b0;
wire         mig7_app_ref_req = 1'b0;
wire         mig7_app_zq_req = 1'b0;
wire         mig7_app_sr_active;
wire         mig7_app_ref_ack;
wire         mig7_app_zq_ack;

MIG7Mock #(
    .ADDR_WIDTH(29),
    .DATA_WIDTH(256),
    .MASK_WIDTH(32),
    .RAM_DEPTH(1024),
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/mig7mock.list")
) mig7mock (
    .sys_clk_i(clk),
    .sys_rst(reset),
    .ui_clk(), // Not used in simulation, all clocks are clk for now
    .ui_clk_sync_rst(), // Not used in simulation
    .init_calib_complete(mig7_init_calib_complete),

    .app_addr(mig7_app_addr),
    .app_cmd(mig7_app_cmd),
    .app_en(mig7_app_en),
    .app_rdy(mig7_app_rdy),

    .app_wdf_data(mig7_app_wdf_data),
    .app_wdf_end(mig7_app_wdf_end),
    .app_wdf_mask(mig7_app_wdf_mask),
    .app_wdf_wren(mig7_app_wdf_wren),
    .app_wdf_rdy(mig7_app_wdf_rdy),

    .app_rd_data(mig7_app_rd_data),
    .app_rd_data_end(mig7_app_rd_data_end),
    .app_rd_data_valid(mig7_app_rd_data_valid),

    .app_sr_req(mig7_app_sr_req),
    .app_ref_req(mig7_app_ref_req),
    .app_zq_req(mig7_app_zq_req),
    .app_sr_active(mig7_app_sr_active),
    .app_ref_ack(mig7_app_ref_ack),
    .app_zq_ack(mig7_app_zq_ack)
);

//-----------------------CacheControllerL1i-------------------------

// CacheControllerL1i control signals
wire        l1i_cache_controller_start;
wire [31:0] l1i_cache_controller_addr;
wire        l1i_cache_controller_done;
wire        l1i_cache_controller_ready;
wire [31:0] l1i_cache_controller_result;

// Instantiate CacheControllerL1i
CacheControllerL1i cache_controller_l1i (
    .clk(clk),
    .reset(reset),

    // CPU pipeline interface
    .cpu_start(l1i_cache_controller_start),
    .cpu_addr(l1i_cache_controller_addr),
    .cpu_done(l1i_cache_controller_done),
    .cpu_ready(l1i_cache_controller_ready),
    .cpu_result(l1i_cache_controller_result),

    // L1 instruction cache DPRAM interface
    .l1i_ctrl_d(l1i_ctrl_d),
    .l1i_ctrl_addr(l1i_ctrl_addr),
    .l1i_ctrl_we(l1i_ctrl_we),
    .l1i_ctrl_q(l1i_ctrl_q),

    // MIG7 interface
    .init_calib_complete(mig7_init_calib_complete),
    .app_addr(mig7_app_addr),
    .app_cmd(mig7_app_cmd),
    .app_en(mig7_app_en),
    .app_rdy(mig7_app_rdy),
    .app_wdf_data(mig7_app_wdf_data),
    .app_wdf_end(mig7_app_wdf_end),
    .app_wdf_mask(mig7_app_wdf_mask),
    .app_wdf_wren(mig7_app_wdf_wren),
    .app_wdf_rdy(mig7_app_wdf_rdy),
    .app_rd_data(mig7_app_rd_data),
    .app_rd_data_end(mig7_app_rd_data_end),
    .app_rd_data_valid(mig7_app_rd_data_valid)
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
    .vramPX_q(vramPX_cpu_q),

    // L1i cache (cpu pipeline port)
    .l1i_pipe_addr(l1i_pipe_addr),
    .l1i_pipe_q(l1i_pipe_q),

    // L1d cache (cpu pipeline port)
    .l1d_pipe_addr(l1d_pipe_addr),
    .l1d_pipe_q(l1d_pipe_q),

    // L1i cache controller
    .l1i_cache_controller_addr(l1i_cache_controller_addr),
    .l1i_cache_controller_start(l1i_cache_controller_start),
    .l1i_cache_controller_done(l1i_cache_controller_done),
    .l1i_cache_controller_result(l1i_cache_controller_result),
    .l1i_cache_controller_ready(l1i_cache_controller_ready)
);

initial
begin
    `ifndef testbench
    $dumpfile("Hardware/Vivado/FPGC.srcs/simulation/output/cpu.vcd");
    $dumpvars;
    `endif

    repeat(1000)
    begin
        #10 clk = ~clk;
    end
end

endmodule

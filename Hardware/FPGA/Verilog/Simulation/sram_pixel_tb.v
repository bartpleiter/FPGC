/*
 * Testbench for SRAM-based Pixel Framebuffer
 * Tests the external SRAM pixel framebuffer implementation with proper GPU clock
 * 
 * Designed to be used with the Icarus Verilog simulator
 */
`timescale 1ns / 1ps

`include "Hardware/FPGA/Verilog/Modules/CPU/B32P2.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/Regr.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/Regbank.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/InstructionDecoder.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/ALU.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleALU.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleAluOps/Multu.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleAluOps/Mults.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleAluOps/IDivider.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleAluOps/FPDivider.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/ControlUnit.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/Stack.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/BranchJumpUnit.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/InterruptController.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/AddressDecoder.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/CacheControllerSDRAM.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/ROM.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/VRAM.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/DPRAM.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SDRAMcontroller.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/mt48lc16m16a2.v"

// SRAM modules
`include "Hardware/FPGA/Verilog/Modules/Memory/IS61LV5128AL.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SRAMWriteFIFO.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SRAMReadFIFO.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SRAMArbiter.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/VRAMPXSram.v"

`include "Hardware/FPGA/Verilog/Modules/Memory/MemoryUnit.v"
`include "Hardware/FPGA/Verilog/Modules/IO/UARTrx.v"
`include "Hardware/FPGA/Verilog/Modules/IO/UARTtx.v"
`include "Hardware/FPGA/Verilog/Modules/IO/SimpleSPI.v"
`include "Hardware/FPGA/Verilog/Modules/IO/MicrosCounter.v"
`include "Hardware/FPGA/Verilog/Modules/IO/OStimer.v"
`include "Hardware/FPGA/Verilog/Modules/IO/UARTresetDetector.v"

// GPU modules
`include "Hardware/FPGA/Verilog/Modules/GPU/FSX_SRAM.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/BGWrenderer.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/PixelEngineSRAM.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/TimingGenerator.v"
`include "Hardware/FPGA/Verilog/Modules/GPU/HDMI/RGB8toRGB24.v"

module sram_pixel_tb ();

reg clk = 1'b0;          // 50MHz CPU clock
reg clk100 = 1'b1;       // 100MHz memory clock
reg clkPixel = 1'b0;     // 25MHz GPU clock
reg reset = 1'b0;
wire uart_reset;

wire clkTMDShalf = clk100;

// SDRAM clock phase shift configuration
parameter SDRAM_CLK_PHASE = 270;
localparam real PHASE_DELAY = (SDRAM_CLK_PHASE / 360.0) * 10.0;

//===========================================================================
// SDRAM
//===========================================================================
reg              SDRAM_CLK_internal = 1'b0;
wire             SDRAM_CLK;
wire    [31 : 0] SDRAM_DQ;
wire    [12 : 0] SDRAM_A;
wire    [1 : 0]  SDRAM_BA;
wire             SDRAM_CKE;
wire             SDRAM_CSn;
wire             SDRAM_RASn;
wire             SDRAM_CASn;
wire             SDRAM_WEn;
wire    [3 : 0]  SDRAM_DQM;

assign SDRAM_CLK = SDRAM_CLK_internal;

always @(clk100) begin
    SDRAM_CLK_internal <= #PHASE_DELAY clk100;
end

mt48lc16m16a2 #(
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/sdram.list"),
    .HIGH_HALF(0)
) sdram1 (
    .Dq     (SDRAM_DQ[15:0]), 
    .Addr   (SDRAM_A), 
    .Ba     (SDRAM_BA), 
    .Clk    (SDRAM_CLK), 
    .Cke    (SDRAM_CKE), 
    .Cs_n   (SDRAM_CSn), 
    .Ras_n  (SDRAM_RASn), 
    .Cas_n  (SDRAM_CASn), 
    .We_n   (SDRAM_WEn), 
    .Dqm    (SDRAM_DQM[1:0])
);

mt48lc16m16a2 #(
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/sdram.list"),
    .HIGH_HALF(1)
) sdram2 (
    .Dq     (SDRAM_DQ[31:16]), 
    .Addr   (SDRAM_A), 
    .Ba     (SDRAM_BA), 
    .Clk    (SDRAM_CLK), 
    .Cke    (SDRAM_CKE), 
    .Cs_n   (SDRAM_CSn), 
    .Ras_n  (SDRAM_RASn), 
    .Cas_n  (SDRAM_CASn), 
    .We_n   (SDRAM_WEn), 
    .Dqm    (SDRAM_DQM[3:2])
);

//===========================================================================
// SDRAM Controller (100MHz)
//===========================================================================
wire [20:0]     sdc_addr;
wire [255:0]    sdc_data;
wire            sdc_we;
wire            sdc_start;
wire            sdc_done;
wire [255:0]    sdc_q;

SDRAMcontroller sdc (
    .clk(clk100),
    .reset(1'b0),
    .cpu_addr(sdc_addr),
    .cpu_data(sdc_data),
    .cpu_we(sdc_we),
    .cpu_start(sdc_start),
    .cpu_done(sdc_done),
    .cpu_q(sdc_q),
    .SDRAM_CKE(SDRAM_CKE),
    .SDRAM_CSn(SDRAM_CSn),
    .SDRAM_WEn(SDRAM_WEn),
    .SDRAM_CASn(SDRAM_CASn),
    .SDRAM_RASn(SDRAM_RASn),
    .SDRAM_A(SDRAM_A),
    .SDRAM_BA(SDRAM_BA),
    .SDRAM_DQM(SDRAM_DQM),
    .SDRAM_DQ(SDRAM_DQ)
);

//===========================================================================
// ROM
//===========================================================================
wire [9:0] rom_fe_addr;
wire [9:0] rom_mem_addr;
wire rom_fe_oe;
wire rom_fe_hold;
wire [31:0] rom_fe_q;
wire [31:0] rom_mem_q;

ROM #(
    .WIDTH(32),
    .WORDS(1024),
    .ADDR_BITS(10),
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/rom.list")
) rom (
    .clk (clk),
    .fe_addr(rom_fe_addr),
    .fe_oe(rom_fe_oe),
    .fe_q(rom_fe_q),
    .fe_hold(rom_fe_hold),
    .mem_addr(rom_mem_addr),
    .mem_q(rom_mem_q)
);

//===========================================================================
// VRAM32 (for BGW renderer)
//===========================================================================
wire [10:0] vram32_gpu_addr;
wire [31:0] vram32_gpu_d;
wire        vram32_gpu_we;
wire [31:0] vram32_gpu_q;

wire [10:0] vram32_cpu_addr;
wire [31:0] vram32_cpu_d;
wire        vram32_cpu_we; 
wire [31:0] vram32_cpu_q;

assign vram32_gpu_we = 1'b0;
assign vram32_gpu_d  = 32'd0;

VRAM #(
    .WIDTH(32),
    .WORDS(1056),
    .ADDR_BITS(11),
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/vram32.list")
) vram32 (
    .cpu_clk (clk),
    .cpu_d   (vram32_cpu_d),
    .cpu_addr(vram32_cpu_addr),
    .cpu_we  (vram32_cpu_we),
    .cpu_q   (vram32_cpu_q),
    .gpu_clk (clkPixel),
    .gpu_d   (vram32_gpu_d),
    .gpu_addr(vram32_gpu_addr),
    .gpu_we  (vram32_gpu_we),
    .gpu_q   (vram32_gpu_q)
);

//===========================================================================
// VRAM8 (for BGW renderer)
//===========================================================================
wire [13:0] vram8_gpu_addr;
wire [7:0]  vram8_gpu_d;
wire        vram8_gpu_we;
wire [7:0]  vram8_gpu_q;

wire [13:0] vram8_cpu_addr;
wire [7:0]  vram8_cpu_d;
wire        vram8_cpu_we;
wire [7:0]  vram8_cpu_q;

assign vram8_gpu_we = 1'b0;
assign vram8_gpu_d  = 8'd0;

VRAM #(
    .WIDTH(8),
    .WORDS(8194),
    .ADDR_BITS(14),
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/vram8.list")
) vram8 (
    .cpu_clk (clk),
    .cpu_d   (vram8_cpu_d),
    .cpu_addr(vram8_cpu_addr),
    .cpu_we  (vram8_cpu_we),
    .cpu_q   (vram8_cpu_q),
    .gpu_clk (clkPixel),
    .gpu_d   (vram8_gpu_d),
    .gpu_addr(vram8_gpu_addr),
    .gpu_we  (vram8_gpu_we),
    .gpu_q   (vram8_gpu_q)
);

//===========================================================================
// External SRAM for Pixel Framebuffer
//===========================================================================
wire [18:0] SRAM_A;
wire [7:0]  SRAM_DQ;
wire        SRAM_CSn;
wire        SRAM_OEn;
wire        SRAM_WEn;

IS61LV5128AL sram (
    .A(SRAM_A),
    .DQ(SRAM_DQ),
    .CE_n(SRAM_CSn),
    .OE_n(SRAM_OEn),
    .WE_n(SRAM_WEn)
);

//===========================================================================
// VRAMPX SRAM Interface
// CPU writes go to SRAM via FIFO and arbiter
// GPU reads from FIFO filled by arbiter
//===========================================================================
wire [16:0] vramPX_cpu_addr;
wire [7:0]  vramPX_cpu_d;
wire        vramPX_cpu_we;
wire [7:0]  vramPX_cpu_q;  // Not used in this design (write-only from CPU)

// GPU pixel FIFO interface
wire [7:0]  gpu_pixel_data;
wire        gpu_fifo_empty;
wire        gpu_fifo_rd_en;

// Timing signals from FSX (25MHz domain)
wire [11:0] fsx_h_count;
wire [11:0] fsx_v_count;
wire        fsx_vsync;

VRAMPXSram vrampx_sram (
    // Clocks and reset
    .clk(clk),
    .clkPixel(clkPixel),
    .reset(reset),
    
    // CPU interface (50MHz)
    .cpu_addr(vramPX_cpu_addr),
    .cpu_data(vramPX_cpu_d),
    .cpu_we(vramPX_cpu_we),
    
    // GPU timing signals (from FSX, 25MHz domain)
    .vsync(fsx_vsync),
    .h_count(fsx_h_count),
    .v_count(fsx_v_count),
    .halfRes(1'b0),
    
    // GPU pixel output
    .gpu_pixel_data(gpu_pixel_data),
    .gpu_fifo_empty(gpu_fifo_empty),
    .gpu_fifo_rd_en(gpu_fifo_rd_en),
    
    // External SRAM
    .SRAM_A(SRAM_A),
    .SRAM_DQ(SRAM_DQ),
    .SRAM_CSn(SRAM_CSn),
    .SRAM_OEn(SRAM_OEn),
    .SRAM_WEn(SRAM_WEn)
);

// CPU read from vramPX returns 0 (write-only)
assign vramPX_cpu_q = 8'd0;

//===========================================================================
// L1i Cache RAM
//===========================================================================
wire [270:0] l1i_pipe_d;
wire [6:0]   l1i_pipe_addr;
wire         l1i_pipe_we;
wire [270:0] l1i_pipe_q;

wire [270:0] l1i_ctrl_d;
wire [6:0]   l1i_ctrl_addr;
wire         l1i_ctrl_we;
wire [270:0] l1i_ctrl_q;

assign l1i_pipe_we = 1'b0;
assign l1i_pipe_d  = 271'd0;

DPRAM #(
    .WIDTH(271),
    .WORDS(128),
    .ADDR_BITS(7)
) l1i_ram (
    .clk_pipe(clk),
    .pipe_d(l1i_pipe_d),
    .pipe_addr(l1i_pipe_addr),
    .pipe_we(l1i_pipe_we),
    .pipe_q(l1i_pipe_q),
    .clk_ctrl(clk100),
    .ctrl_d(l1i_ctrl_d),
    .ctrl_addr(l1i_ctrl_addr),
    .ctrl_we(l1i_ctrl_we),
    .ctrl_q(l1i_ctrl_q)
);

//===========================================================================
// L1d Cache RAM
//===========================================================================
wire [270:0] l1d_pipe_d;
wire [6:0]   l1d_pipe_addr;
wire         l1d_pipe_we;
wire [270:0] l1d_pipe_q;

wire [270:0] l1d_ctrl_d;
wire [6:0]   l1d_ctrl_addr;
wire         l1d_ctrl_we;
wire [270:0] l1d_ctrl_q;

assign l1d_pipe_we = 1'b0;
assign l1d_pipe_d  = 271'd0;

DPRAM #(
    .WIDTH(271),
    .WORDS(128),
    .ADDR_BITS(7)
) l1d_ram (
    .clk_pipe(clk),
    .pipe_d(l1d_pipe_d),
    .pipe_addr(l1d_pipe_addr),
    .pipe_we(l1d_pipe_we),
    .pipe_q(l1d_pipe_q),
    .clk_ctrl(clk100),
    .ctrl_d(l1d_ctrl_d),
    .ctrl_addr(l1d_ctrl_addr),
    .ctrl_we(l1d_ctrl_we),
    .ctrl_q(l1d_ctrl_q)
);

//===========================================================================
// Cache Controller
//===========================================================================
wire [31:0] l1i_cache_controller_addr;
wire        l1i_cache_controller_start;
wire        l1i_cache_controller_flush;
wire        l1i_cache_controller_done;
wire [31:0] l1i_cache_controller_result;

wire [31:0] l1d_cache_controller_addr;
wire [31:0] l1d_cache_controller_data;
wire        l1d_cache_controller_we;
wire        l1d_cache_controller_start;
wire        l1d_cache_controller_done;
wire [31:0] l1d_cache_controller_result;

wire l1_clear_cache;
wire l1_clear_cache_done;

CacheController cache_controller (
    .clk100(clk100),
    .reset(reset || uart_reset),

    .cpu_FE2_start(l1i_cache_controller_start),
    .cpu_FE2_addr(l1i_cache_controller_addr),
    .cpu_FE2_flush(l1i_cache_controller_flush),
    .cpu_FE2_done(l1i_cache_controller_done),
    .cpu_FE2_result(l1i_cache_controller_result),

    .cpu_EXMEM2_start(l1d_cache_controller_start),
    .cpu_EXMEM2_addr(l1d_cache_controller_addr),
    .cpu_EXMEM2_data(l1d_cache_controller_data),
    .cpu_EXMEM2_we(l1d_cache_controller_we),
    .cpu_EXMEM2_done(l1d_cache_controller_done),
    .cpu_EXMEM2_result(l1d_cache_controller_result),

    .cpu_clear_cache(l1_clear_cache),
    .cpu_clear_cache_done(l1_clear_cache_done),

    .l1i_ctrl_d(l1i_ctrl_d),
    .l1i_ctrl_addr(l1i_ctrl_addr),
    .l1i_ctrl_we(l1i_ctrl_we),
    .l1i_ctrl_q(l1i_ctrl_q),

    .l1d_ctrl_d(l1d_ctrl_d),
    .l1d_ctrl_addr(l1d_ctrl_addr),
    .l1d_ctrl_we(l1d_ctrl_we),
    .l1d_ctrl_q(l1d_ctrl_q),

    .sdc_addr(sdc_addr),
    .sdc_data(sdc_data),
    .sdc_we(sdc_we),
    .sdc_start(sdc_start),
    .sdc_done(sdc_done),
    .sdc_q(sdc_q)
);

//===========================================================================
// FSX_SRAM (GPU with SRAM-based pixel engine)
//===========================================================================
wire frameDrawn;

FSX_SRAM fsx (
    .clkPixel(clkPixel),
    .clkTMDShalf(clkTMDShalf),

    // HDMI outputs (unused in simulation)
    .TMDS_clk_p(),
    .TMDS_clk_n(),
    .TMDS_d0_p(),
    .TMDS_d0_n(),
    .TMDS_d1_p(),
    .TMDS_d1_n(),
    .TMDS_d2_p(),
    .TMDS_d2_n(),

    // VRAM32
    .vram32_addr(vram32_gpu_addr),
    .vram32_q   (vram32_gpu_q),

    // VRAM8
    .vram8_addr(vram8_gpu_addr),
    .vram8_q   (vram8_gpu_q),

    // Pixel FIFO interface
    .pixel_fifo_data(gpu_pixel_data),
    .pixel_fifo_empty(gpu_fifo_empty),
    .pixel_fifo_rd_en(gpu_fifo_rd_en),
    
    // Timing outputs
    .h_count_out(fsx_h_count),
    .v_count_out(fsx_v_count),
    .vsync_out(fsx_vsync),

    // Parameters
    .halfRes(1'b0),

    // Interrupt
    .frameDrawn(frameDrawn)
);

//===========================================================================
// SPI Flash (stubs)
//===========================================================================
wire SPI0_clk, SPI0_cs, SPI0_mosi, SPI0_miso;
wire SPI1_clk, SPI1_cs, SPI1_mosi, SPI1_miso;

//===========================================================================
// UART
//===========================================================================
wire uart_rx = 1'b1;  // Idle high
wire uart_tx;
wire uart_irq;

//===========================================================================
// Memory Unit
//===========================================================================
wire        mu_start;
wire [31:0] mu_addr;
wire [31:0] mu_data;
wire        mu_we;
wire [31:0] mu_q;
wire        mu_done;

wire        OST1_int;
wire        OST2_int;
wire        OST3_int;

reg         boot_mode = 1'b0;

wire        SPI2_clk, SPI2_mosi, SPI2_cs;
wire        SPI3_clk, SPI3_mosi, SPI3_cs;
wire        SPI4_clk, SPI4_mosi, SPI4_cs;
wire        SPI5_clk, SPI5_mosi, SPI5_cs;

MemoryUnit memory_unit (
    .clk(clk),
    .reset(reset),
    .uart_reset(uart_reset),

    .start(mu_start),
    .addr(mu_addr),
    .data(mu_data),
    .we(mu_we),
    .q(mu_q),
    .done(mu_done),

    .uart_rx(uart_rx),
    .uart_tx(uart_tx),
    .uart_irq(uart_irq),

    .OST1_int(OST1_int),
    .OST2_int(OST2_int),
    .OST3_int(OST3_int),

    .boot_mode(boot_mode),

    .SPI0_clk(SPI0_clk),
    .SPI0_cs(SPI0_cs),
    .SPI0_mosi(SPI0_mosi),
    .SPI0_miso(1'b0),

    .SPI1_clk(SPI1_clk),
    .SPI1_cs(SPI1_cs),
    .SPI1_mosi(SPI1_mosi),
    .SPI1_miso(1'b0),

    .SPI2_clk(SPI2_clk),
    .SPI2_cs(SPI2_cs),
    .SPI2_mosi(SPI2_mosi),
    .SPI2_miso(1'b0),

    .SPI3_clk(SPI3_clk),
    .SPI3_cs(SPI3_cs),
    .SPI3_mosi(SPI3_mosi),
    .SPI3_miso(1'b0),

    .SPI4_clk(SPI4_clk),
    .SPI4_cs(SPI4_cs),
    .SPI4_mosi(SPI4_mosi),
    .SPI4_miso(1'b0),

    .SPI5_clk(SPI5_clk),
    .SPI5_cs(SPI5_cs),
    .SPI5_mosi(SPI5_mosi),
    .SPI5_miso(1'b0)
);

//===========================================================================
// CPU
//===========================================================================
B32P2 cpu (
    .clk(clk),
    .reset(reset || uart_reset),

    // ROM
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

    // VRAMPX (via SRAM)
    .vramPX_addr(vramPX_cpu_addr),
    .vramPX_d(vramPX_cpu_d),
    .vramPX_we(vramPX_cpu_we),
    .vramPX_q(vramPX_cpu_q),

    // L1 Cache
    .l1i_pipe_addr(l1i_pipe_addr),
    .l1i_pipe_q(l1i_pipe_q),
    .l1d_pipe_addr(l1d_pipe_addr),
    .l1d_pipe_q(l1d_pipe_q),

    // Cache Controller
    .l1i_cache_controller_addr(l1i_cache_controller_addr),
    .l1i_cache_controller_start(l1i_cache_controller_start),
    .l1i_cache_controller_flush(l1i_cache_controller_flush),
    .l1i_cache_controller_done(l1i_cache_controller_done),
    .l1i_cache_controller_result(l1i_cache_controller_result),

    .l1d_cache_controller_addr(l1d_cache_controller_addr),
    .l1d_cache_controller_data(l1d_cache_controller_data),
    .l1d_cache_controller_we(l1d_cache_controller_we),
    .l1d_cache_controller_start(l1d_cache_controller_start),
    .l1d_cache_controller_done(l1d_cache_controller_done),
    .l1d_cache_controller_result(l1d_cache_controller_result),

    .l1_clear_cache(l1_clear_cache),
    .l1_clear_cache_done(l1_clear_cache_done),

    // Memory Unit
    .mu_start(mu_start),
    .mu_addr(mu_addr),
    .mu_data(mu_data),
    .mu_we(mu_we),
    .mu_q(mu_q),
    .mu_done(mu_done),

    // Interrupts
    .interrupts({3'd0, 1'b0, OST3_int, OST2_int, OST1_int, uart_irq})
);

//===========================================================================
// Clock Generation
//===========================================================================

// 100 MHz clock
always begin
    #5 clk100 = ~clk100;
end

// 50 MHz clock
always begin
    #10 clk = ~clk;
end

// 25 MHz GPU clock (proper half-speed)
always begin
    #20 clkPixel = ~clkPixel;
end

//===========================================================================
// Simulation Control
//===========================================================================
integer clk_counter = 0;
always @(posedge clk) begin
    clk_counter = clk_counter + 1;
    // With 25MHz GPU clock, we need approximately 2x the cycles for one frame
    // VGA 640x480 @ 25MHz = 800*525 = 420000 GPU clocks per frame
    // At 50MHz CPU clock, that's 840000 CPU clocks per frame
    // Run for 1.2 frames to capture at least one complete frame
    // and a bit of the second frame
    if (clk_counter == 1000000) begin
        $display("Simulation finished after %d CPU cycles.", clk_counter);
        $finish;
    end
end

initial begin
    $dumpfile("Hardware/FPGA/Verilog/Simulation/Output/sram_pixel.vcd");
    $dumpvars;
end

endmodule

/*
 * Testbench for the CPU (B32P3).
 * Designed to be used with the Icarus Verilog simulator
 */
`timescale 1ns / 1ps

`include "Hardware/FPGA/Verilog/Modules/CPU/B32P3.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/Regbank.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/InstructionDecoder.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/InstructionFetch.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MemoryStage.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/ALU.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleALU.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleAluOps/Multu.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleAluOps/Mults.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleAluOps/IDivider.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleAluOps/FPDivider.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/MultiCycleAluOps/Mults64.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/ControlUnit.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/PipelineController.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/Stack.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/BranchJumpUnit.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/InterruptController.v"
`include "Hardware/FPGA/Verilog/Modules/CPU/CacheControllerSDRAM.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/ROM.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/VRAM.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/DPRAM.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SDRAMcontroller.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/SDRAMarbiter.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/mt48lc16m16a2.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/W25Q128JV.v"

`include "Hardware/FPGA/Verilog/Modules/Memory/MemoryUnit.v"
`include "Hardware/FPGA/Verilog/Modules/IO/DMAengine.v"
`include "Hardware/FPGA/Verilog/Modules/IO/UARTrx.v"
`include "Hardware/FPGA/Verilog/Modules/IO/UARTtx.v"
`include "Hardware/FPGA/Verilog/Modules/IO/SimpleSPI.v"
`include "Hardware/FPGA/Verilog/Modules/IO/SimpleSPI2.v"
`include "Hardware/FPGA/Verilog/Modules/IO/QSPIflash.v"
`include "Hardware/FPGA/Verilog/Simulation/qspi_flash_sim.v"
`include "Hardware/FPGA/Verilog/Modules/IO/MicrosCounter.v"
`include "Hardware/FPGA/Verilog/Modules/IO/OStimer.v"
`include "Hardware/FPGA/Verilog/Modules/IO/CameraCapture.v"
`include "Hardware/FPGA/Verilog/Modules/IO/SCCB_master.v"
`include "Hardware/FPGA/Verilog/Modules/IO/OV7670_config_rom.v"
`include "Hardware/FPGA/Verilog/Modules/IO/CameraConfigure.v"
`include "Hardware/FPGA/Verilog/Modules/Memory/CameraSubArbiter.v"



module cpu_tb ();

reg clk = 1'b0;
reg reset = 1'b0;
wire uart_reset; // Always 1'b0, reset is handled via DTR in synthesis

// Inaccurate but good enough for simulation
wire clkPixel = clk;

// SDRAM clock phase shift configuration (in degrees)
parameter SDRAM_CLK_PHASE = 270;

// Calculate phase shift delay in nanoseconds (clock period is 10ns @ 100MHz)
localparam real PHASE_DELAY = (SDRAM_CLK_PHASE / 360.0) * 10.0;

//---------------------------SDRAM---------------------------------
// SDRAM signals
reg              SDRAM_CLK_internal = 1'b0;  // Internal SDRAM clock signal
wire             SDRAM_CLK;     // SDRAM clock
wire    [31 : 0] SDRAM_DQ;      // SDRAM I/O
wire    [12 : 0] SDRAM_A;       // SDRAM Address
wire    [1 : 0]  SDRAM_BA;      // Bank Address
wire             SDRAM_CKE;     // Synchronous Clock Enable
wire             SDRAM_CSn;     // CS#
wire             SDRAM_RASn;    // RAS#
wire             SDRAM_CASn;    // CAS#
wire             SDRAM_WEn;     // WE#
wire    [3 : 0]  SDRAM_DQM;     // Mask

// Apply phase shift to SDRAM clock
assign SDRAM_CLK = SDRAM_CLK_internal;

// Generate phase-shifted SDRAM clock
always @(clk) begin
    SDRAM_CLK_internal <= #PHASE_DELAY clk;
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

//-----------------------SDRAM Controller(100MHz)------------------------
wire [20:0]     sdc_addr;
wire [255:0]    sdc_data;
wire            sdc_we;
wire            sdc_start;
wire            sdc_done;
wire [255:0]    sdc_q;
SDRAMcontroller sdc (
    // Clock and reset
    .clk(clk),
    .reset(1'b0), // For now we do not want to reset the SDRAM controller

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


//-----------------------ROM-------------------------
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
assign vram32_gpu_addr = 11'd0;

VRAM #(
    .WIDTH(32),
    .WORDS(1056),
    .ADDR_BITS(11),
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/vram32.list")
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
assign vram8_gpu_addr = 14'd0;

VRAM #(
    .WIDTH(8),
    .WORDS(8194),
    .ADDR_BITS(14),
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/vram8.list")
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

// VRAMPX write-port mux: DMA wins over CPU writes when active.
wire [16:0] vramPX_w_addr = dma_vp_we ? dma_vp_addr : vramPX_cpu_addr;
wire [7:0]  vramPX_w_data = dma_vp_we ? dma_vp_data : vramPX_cpu_d;
wire        vramPX_w_we   = dma_vp_we | vramPX_cpu_we;

// GPU will not write to VRAM
assign vramPX_gpu_we = 1'b0;
assign vramPX_gpu_d  = 8'd0;
assign vramPX_gpu_addr = 17'd0;

VRAM #(
    .WIDTH(8),
    .WORDS(76800),
    .ADDR_BITS(17),
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/vramPX.list")
) vramPX (
    // CPU port
    .cpu_clk (clk),
    .cpu_d   (vramPX_w_data),
    .cpu_addr(vramPX_w_addr),
    .cpu_we  (vramPX_w_we),
    .cpu_q   (vramPX_cpu_q),

    // GPU port
    .gpu_clk (clkPixel),
    .gpu_d   (vramPX_gpu_d),
    .gpu_addr(vramPX_gpu_addr),
    .gpu_we  (vramPX_gpu_we),
    .gpu_q   (vramPX_gpu_q)
);

//-----------------------L1i RAM (100&50MHz)-------------------------

// DPRAM I/O signals
wire [270:0] l1i_pipe_d;
wire [6:0]   l1i_pipe_addr;
wire         l1i_pipe_we;
wire [270:0] l1i_pipe_q;

wire [270:0] l1i_ctrl_d;
wire [6:0]   l1i_ctrl_addr;
wire         l1i_ctrl_we;
wire [270:0] l1i_ctrl_q;

// CPU pipeline will not write to L1 RAM
assign l1i_pipe_we = 1'b0;
assign l1i_pipe_d  = 271'd0;

// DPRAM instance
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
    .clk_ctrl(clk),
    .ctrl_d(l1i_ctrl_d),
    .ctrl_addr(l1i_ctrl_addr),
    .ctrl_we(l1i_ctrl_we),
    .ctrl_q(l1i_ctrl_q)
);

//-----------------------L1d RAM (100&50MHz)------------------------

// DPRAM I/O signals
wire [270:0] l1d_pipe_d;
wire [6:0]   l1d_pipe_addr;
wire         l1d_pipe_we;
wire [270:0] l1d_pipe_q;

wire [270:0] l1d_ctrl_d;
wire [6:0]   l1d_ctrl_addr;
wire         l1d_ctrl_we;
wire [270:0] l1d_ctrl_q;

// CPU pipeline will not write to L1 RAM
assign l1d_pipe_we = 1'b0;
assign l1d_pipe_d  = 271'd0;

// DPRAM instance
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
    .clk_ctrl(clk),
    .ctrl_d(l1d_ctrl_d),
    .ctrl_addr(l1d_ctrl_addr),
    .ctrl_we(l1d_ctrl_we),
    .ctrl_q(l1d_ctrl_q)
);

//-----------------------CacheController (100MHz)-------------------------

// Cache controller <-> CPU pipeline interface signals
wire [31:0] l1i_cache_controller_addr;
wire        l1i_cache_controller_start;
wire        l1i_cache_controller_flush;
wire        l1i_cache_controller_done;
wire [31:0] l1i_cache_controller_result;

wire [31:0] l1d_cache_controller_addr;
wire [31:0] l1d_cache_controller_data;
wire        l1d_cache_controller_we;
wire        l1d_cache_controller_start;
wire [3:0]  l1d_cache_controller_byte_enable;
wire        l1d_cache_controller_done;
wire [31:0] l1d_cache_controller_result;

wire l1_clear_cache;
wire l1_clear_cache_data_only;
wire l1_clear_cache_done;

// Instantiate CacheController
CacheController cache_controller (
    .clk100(clk),
    .reset(reset || uart_reset),

    // CPU pipeline interface (50 MHz domain)
    .cpu_FE2_start(l1i_cache_controller_start),
    .cpu_FE2_addr(l1i_cache_controller_addr),
    .cpu_FE2_flush(l1i_cache_controller_flush),
    .cpu_FE2_done(l1i_cache_controller_done),
    .cpu_FE2_result(l1i_cache_controller_result),

    .cpu_EXMEM2_start(l1d_cache_controller_start),
    .cpu_EXMEM2_addr(l1d_cache_controller_addr),
    .cpu_EXMEM2_data(l1d_cache_controller_data),
    .cpu_EXMEM2_we(l1d_cache_controller_we),
    .cpu_EXMEM2_byte_enable(l1d_cache_controller_byte_enable),
    .cpu_EXMEM2_done(l1d_cache_controller_done),
    .cpu_EXMEM2_result(l1d_cache_controller_result),

    .cpu_clear_cache(l1_clear_cache),
    .cpu_clear_cache_data_only(l1_clear_cache_data_only),
    .cpu_clear_cache_done(l1_clear_cache_done),

    // L1i RAM ctrl port
    .l1i_ctrl_d(l1i_ctrl_d),
    .l1i_ctrl_addr(l1i_ctrl_addr),
    .l1i_ctrl_we(l1i_ctrl_we),
    .l1i_ctrl_q(l1i_ctrl_q),

    // L1d RAM ctrl port
    .l1d_ctrl_d(l1d_ctrl_d),
    .l1d_ctrl_addr(l1d_ctrl_addr),
    .l1d_ctrl_we(l1d_ctrl_we),
    .l1d_ctrl_q(l1d_ctrl_q),

    // SDRAM controller interface (via SDRAMarbiter)
    .sdc_addr(cpu_sdc_addr),
    .sdc_data(cpu_sdc_data),
    .sdc_we(cpu_sdc_we),
    .sdc_start(cpu_sdc_start),
    .sdc_done(cpu_sdc_done),
    .sdc_q(cpu_sdc_q)
);

//-----------------------SDRAM Arbiter-------------------------
wire [20:0]     cpu_sdc_addr;
wire [255:0]    cpu_sdc_data;
wire            cpu_sdc_we;
wire            cpu_sdc_start;
wire            cpu_sdc_done;
wire [255:0]    cpu_sdc_q;

// ---- DMA engine wires (step 8) ----
wire [20:0]     dma_raw_sd_addr;
wire [255:0]    dma_raw_sd_data;
wire            dma_raw_sd_we;
wire            dma_raw_sd_start;
wire            dma_raw_sd_done;
wire [255:0]    dma_raw_sd_q;

// ---- Camera sub-arbiter → SDRAM arbiter wires ----
wire [20:0]     dma_sd_addr;
wire [255:0]    dma_sd_data;
wire            dma_sd_we;
wire            dma_sd_start;
wire            dma_sd_done;
wire [255:0]    dma_sd_q;

// ---- Camera capture → sub-arbiter wires ----
wire [20:0]     cam_sd_addr;
wire [255:0]    cam_sd_data;
wire            cam_sd_we;
wire            cam_sd_start;
wire            cam_sd_done;

// ---- Camera control wires (from MemoryUnit) ----
wire            cam_ctrl_enable;
wire [20:0]     cam_ctrl_base_buf0;
wire [20:0]     cam_ctrl_base_buf1;
wire            cam_frame_done;
wire            cam_current_buf;

wire [2:0]      dma_reg_addr;
wire            dma_reg_we;
wire [31:0]     dma_reg_data;
wire [31:0]     dma_reg_q;

wire            dma_iop_start;
wire            dma_iop_we;
wire [31:0]     dma_iop_addr;
wire [31:0]     dma_iop_data;
wire            dma_iop_done;
wire [31:0]     dma_iop_q;

wire            dma_vp_we;
wire [16:0]     dma_vp_addr;
wire [7:0]      dma_vp_data;

// DMA SPI burst port (Phase B)
wire [2:0]      dma_burst_spi_id;
wire            dma_burst_select;
wire            dma_burst_we;
wire [7:0]      dma_burst_data;
wire            dma_burst_start;
wire [15:0]     dma_burst_len;
wire            dma_burst_dummy;
wire            dma_burst_re_rx;
wire            dma_burst_qspi_read;
wire [23:0]     dma_burst_qspi_addr;
wire            dma_burst_tx_full;
wire            dma_burst_rx_empty;
wire [7:0]      dma_burst_rx_data;
wire [7:0]      dma_burst_rx_count;
wire            dma_burst_busy;
wire            dma_burst_done;

wire            dma_irq;

SDRAMarbiter sdram_arb (
    .clk(clk),
    .reset(reset || uart_reset),

    .cpu_addr(cpu_sdc_addr),
    .cpu_data(cpu_sdc_data),
    .cpu_we(cpu_sdc_we),
    .cpu_start(cpu_sdc_start),
    .cpu_done(cpu_sdc_done),
    .cpu_q(cpu_sdc_q),

    .dma_addr(dma_sd_addr),
    .dma_data(dma_sd_data),
    .dma_we(dma_sd_we),
    .dma_start(dma_sd_start),
    .dma_done(dma_sd_done),
    .dma_q(dma_sd_q),

    .sdc_addr(sdc_addr),
    .sdc_data(sdc_data),
    .sdc_we(sdc_we),
    .sdc_start(sdc_start),
    .sdc_done(sdc_done),
    .sdc_q(sdc_q)
);

//-----------------------GPU (not simulated)-------------------------
wire frameDrawn = 1'b0;

//-----------------------SPI Flash (simulation models)-------------------------
// SPI0 Flash 1
wire SPI0_clk;
wire SPI0_cs; 
wire SPI0_mosi;
wire SPI0_miso;
wire SPI0_wp = 1'b1;
wire SPI0_hold = 1'b1;

W25Q128JV #(
    .LIST("Hardware/FPGA/Verilog/Simulation/MemoryLists/spiflash1.list")
) spiflash1 (
.CLK    (SPI0_clk),
.DIO    (SPI0_mosi),
.CSn    (SPI0_cs),
.WPn    (SPI0_wp),
.HOLDn  (SPI0_hold),
.DO     (SPI0_miso)
);

// SPI1 Flash 2 (QSPI-capable; 4-bit bidirectional bus)
wire SPI1_clk;
wire SPI1_cs;
wire [3:0] SPI1_io_out;
wire [3:0] SPI1_io_oe;
wire [3:0] SPI1_io_in;

// QSPI flash sim model on SPI1.
// It only drives IO[3:0] during the data phase of a Fast Read (opcode
// 0xEB); for any other transaction it stays tristated, so 1-bit-only
// tests behave exactly as before.
wire [3:0] qspisim_io_out;
wire [3:0] qspisim_io_oe;
qspi_flash_sim qspisim (
    .cs_n   (SPI1_cs),
    .spi_clk(SPI1_clk),
    .io_in  (SPI1_io_out),
    .io_out (qspisim_io_out),
    .io_oe  (qspisim_io_oe)
);

// Tristate: each IO line is driven by the controller when SPI1_io_oe[i]=1,
// or by the slave when qspisim_io_oe[i]=1, or floats high otherwise.
// (Both driving the same line at once is a protocol violation that won't
// happen as long as QSPIflash and qspi_flash_sim implement the protocol
// correctly -- in practice the slave only drives during ST_DATA, and the
// controller has IO[3:0] tristated then.)
assign SPI1_io_in[0] = SPI1_io_oe[0]    ? SPI1_io_out[0]    :
                       qspisim_io_oe[0] ? qspisim_io_out[0] : 1'b1;
assign SPI1_io_in[1] = SPI1_io_oe[1]    ? SPI1_io_out[1]    :
                       qspisim_io_oe[1] ? qspisim_io_out[1] : 1'b1;
assign SPI1_io_in[2] = SPI1_io_oe[2]    ? SPI1_io_out[2]    :
                       qspisim_io_oe[2] ? qspisim_io_out[2] : 1'b1;
assign SPI1_io_in[3] = SPI1_io_oe[3]    ? SPI1_io_out[3]    :
                       qspisim_io_oe[3] ? qspisim_io_out[3] : 1'b1;

//------------------Memory Unit (50MHz)----------------------
wire        mu_start;
wire [31:0] mu_addr;
wire [31:0] mu_data;
wire        mu_we;
wire [31:0] mu_q;
wire        mu_done;

wire        uart_tx;
// We ignore uart_rx in simulation, as we will connect uart_tx as rx for testing
wire        uart_irq;

wire        OST1_int;
wire        OST2_int;
wire        OST3_int;

reg         boot_mode = 1'b0; // In hardware this is read from a pin

wire        SPI2_clk;
wire        SPI2_mosi;
reg         SPI2_miso = 1'b0; // In hardware this is read from a pin
wire        SPI2_cs;

wire        SPI3_clk;
wire        SPI3_mosi;
reg         SPI3_miso = 1'b0; // In hardware this is read from a pin
wire        SPI3_cs;

wire        SPI4_clk;
wire        SPI4_mosi;
reg         SPI4_miso = 1'b0; // In hardware this is read from a pin
wire        SPI4_cs;

wire        SPI5_clk;
wire        SPI5_mosi;
reg         SPI5_miso = 1'b0; // In hardware this is read from a pin
wire        SPI5_cs;

MemoryUnit memory_unit (
    .clk(clk),
    .reset(reset || uart_reset),
    .uart_reset(uart_reset),

    .start(mu_start),
    .addr(mu_addr),
    .data(mu_data),
    .we(mu_we),
    .q(mu_q),
    .done(mu_done),

    .uart_rx(1'b1), // Ignored for now, use uart_tx for loopback testing
    .uart_tx(uart_tx),
    .uart_irq(uart_irq),

    .OST1_int(OST1_int),
    .OST2_int(OST2_int),
    .OST3_int(OST3_int),

    .boot_mode(boot_mode),

    .SPI0_clk(SPI0_clk),
    .SPI0_mosi(SPI0_mosi),
    .SPI0_miso(SPI0_miso),
    .SPI0_cs(SPI0_cs),

    .SPI1_clk(SPI1_clk),
    .SPI1_io_out(SPI1_io_out),
    .SPI1_io_oe(SPI1_io_oe),
    .SPI1_io_in(SPI1_io_in),
    .SPI1_cs(SPI1_cs),

    .SPI2_clk(SPI2_clk),
    .SPI2_mosi(SPI2_mosi),
    .SPI2_miso(SPI2_miso),
    .SPI2_cs(SPI2_cs),

    .SPI3_clk(SPI3_clk),
    .SPI3_mosi(SPI3_mosi),
    .SPI3_miso(SPI3_miso),
    .SPI3_cs(SPI3_cs),

    .SPI4_clk(SPI4_clk),
    .SPI4_mosi(SPI4_mosi),
    .SPI4_miso(SPI4_miso),
    .SPI4_cs(SPI4_cs),

    .SPI5_clk(SPI5_clk),
    .SPI5_mosi(SPI5_mosi),
    .SPI5_miso(SPI5_miso),
    .SPI5_cs(SPI5_cs),

    // DMA peer ports (engine wired in step 8)
    .iop_start(dma_iop_start),
    .iop_we(dma_iop_we),
    .iop_addr(dma_iop_addr),
    .iop_data(dma_iop_data),
    .iop_done(dma_iop_done),
    .iop_q(dma_iop_q),
    .vp_we(dma_vp_we),
    .vp_addr(dma_vp_addr),
    .vp_data(dma_vp_data),
    .vramPX_dma_we(),
    .vramPX_dma_addr(),
    .vramPX_dma_d(),

    // DMA SPI burst port (Phase B)
    .dma_burst_spi_id(dma_burst_spi_id),
    .dma_burst_select(dma_burst_select),
    .dma_burst_we(dma_burst_we),
    .dma_burst_data(dma_burst_data),
    .dma_burst_start(dma_burst_start),
    .dma_burst_len(dma_burst_len),
    .dma_burst_dummy(dma_burst_dummy),
    .dma_burst_re_rx(dma_burst_re_rx),
    .dma_burst_qspi_read(dma_burst_qspi_read),
    .dma_burst_qspi_addr(dma_burst_qspi_addr),
    .dma_burst_tx_full(dma_burst_tx_full),
    .dma_burst_rx_empty(dma_burst_rx_empty),
    .dma_burst_rx_data(dma_burst_rx_data),
    .dma_burst_rx_count(dma_burst_rx_count),
    .dma_burst_busy(dma_burst_busy),
    .dma_burst_done(dma_burst_done),

    .dma_reg_addr(dma_reg_addr),
    .dma_reg_we(dma_reg_we),
    .dma_reg_data(dma_reg_data),
    .dma_reg_q(dma_reg_q),

    // Camera control/status (tied off — no real sensor in sim)
    .cam_ctrl_enable(cam_ctrl_enable),
    .cam_ctrl_base_buf0(cam_ctrl_base_buf0),
    .cam_ctrl_base_buf1(cam_ctrl_base_buf1),
    .cam_frame_done(cam_frame_done),
    .cam_current_buf(cam_current_buf),
    .cam_sccb_addr(),
    .cam_sccb_data(),
    .cam_sccb_start(),
    .cam_sccb_ready(1'b1)
);

DMAengine dma_engine (
    .clk(clk),
    .reset(reset || uart_reset),

    .reg_addr(dma_reg_addr),
    .reg_we(dma_reg_we),
    .reg_data(dma_reg_data),
    .reg_q(dma_reg_q),

    .sd_addr(dma_raw_sd_addr),
    .sd_data(dma_raw_sd_data),
    .sd_we(dma_raw_sd_we),
    .sd_start(dma_raw_sd_start),
    .sd_done(dma_raw_sd_done),
    .sd_q(dma_raw_sd_q),

    .iop_start(dma_iop_start),
    .iop_we(dma_iop_we),
    .iop_addr(dma_iop_addr),
    .iop_data(dma_iop_data),
    .iop_done(dma_iop_done),
    .iop_q(dma_iop_q),

    .vp_we(dma_vp_we),
    .vp_addr(dma_vp_addr),
    .vp_data(dma_vp_data),
    .vp_full(1'b0),

    // DMA SPI burst port (Phase B)
    .dma_burst_spi_id(dma_burst_spi_id),
    .dma_burst_select(dma_burst_select),
    .dma_burst_we(dma_burst_we),
    .dma_burst_data(dma_burst_data),
    .dma_burst_start(dma_burst_start),
    .dma_burst_len(dma_burst_len),
    .dma_burst_dummy(dma_burst_dummy),
    .dma_burst_re_rx(dma_burst_re_rx),
    .dma_burst_qspi_read(dma_burst_qspi_read),
    .dma_burst_qspi_addr(dma_burst_qspi_addr),
    .dma_burst_tx_full(dma_burst_tx_full),
    .dma_burst_rx_empty(dma_burst_rx_empty),
    .dma_burst_rx_data(dma_burst_rx_data),
    .dma_burst_rx_count(dma_burst_rx_count),
    .dma_burst_busy(dma_burst_busy),
    .dma_burst_done(dma_burst_done),

    .irq(dma_irq)
);

//-----------------------Camera (no real sensor in sim)-------------------------
CameraCapture camera_capture (
    .clk(clk),
    .reset(reset || uart_reset),
    .ctrl_enable(cam_ctrl_enable),
    .ctrl_base_buf0(cam_ctrl_base_buf0),
    .ctrl_base_buf1(cam_ctrl_base_buf1),
    .cam_pclk(1'b0),
    .cam_vsync(1'b0),
    .cam_href(1'b0),
    .cam_data(8'd0),
    .sd_addr(cam_sd_addr),
    .sd_data(cam_sd_data),
    .sd_we(cam_sd_we),
    .sd_start(cam_sd_start),
    .sd_done(cam_sd_done),
    .frame_done(cam_frame_done),
    .current_buf(cam_current_buf)
);

CameraSubArbiter camera_sub_arbiter (
    .clk(clk),
    .reset(reset || uart_reset),
    .dma_addr(dma_raw_sd_addr),
    .dma_data(dma_raw_sd_data),
    .dma_we(dma_raw_sd_we),
    .dma_start(dma_raw_sd_start),
    .dma_done(dma_raw_sd_done),
    .dma_q(dma_raw_sd_q),
    .cam_addr(cam_sd_addr),
    .cam_data(cam_sd_data),
    .cam_we(cam_sd_we),
    .cam_start(cam_sd_start),
    .cam_done(cam_sd_done),
    .sd_addr(dma_sd_addr),
    .sd_data(dma_sd_data),
    .sd_we(dma_sd_we),
    .sd_start(dma_sd_start),
    .sd_done(dma_sd_done),
    .sd_q(dma_sd_q)
);

//-----------------------CPU-------------------------
B32P3 cpu (
    // Clock and reset
    .clk(clk),
    .reset(reset || uart_reset),

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
    .vramPX_fifo_full(1'b0),

    // L1i cache (cpu pipeline port)
    .l1i_pipe_addr(l1i_pipe_addr),
    .l1i_pipe_q(l1i_pipe_q),

    // L1d cache (cpu pipeline port)
    .l1d_pipe_addr(l1d_pipe_addr),
    .l1d_pipe_q(l1d_pipe_q),

    // cache controller connections
    .l1i_cache_controller_addr(l1i_cache_controller_addr),
    .l1i_cache_controller_start(l1i_cache_controller_start),
    .l1i_cache_controller_flush(l1i_cache_controller_flush),
    .l1i_cache_controller_done(l1i_cache_controller_done),
    .l1i_cache_controller_result(l1i_cache_controller_result),

    .l1d_cache_controller_addr(l1d_cache_controller_addr),
    .l1d_cache_controller_data(l1d_cache_controller_data),
    .l1d_cache_controller_we(l1d_cache_controller_we),
    .l1d_cache_controller_start(l1d_cache_controller_start),
    .l1d_cache_controller_byte_enable(l1d_cache_controller_byte_enable),
    .l1d_cache_controller_done(l1d_cache_controller_done),
    .l1d_cache_controller_result(l1d_cache_controller_result),

    .l1_clear_cache(l1_clear_cache),
    .l1_clear_cache_data_only(l1_clear_cache_data_only),
    .l1_clear_cache_done(l1_clear_cache_done),

    // Memory Unit connections
    .mu_start(mu_start),
    .mu_addr(mu_addr),
    .mu_data(mu_data),
    .mu_we(mu_we),
    .mu_q(mu_q),
    .mu_done(mu_done),

    // Interrupts, right is highest priority
    .interrupts({7'd0}) // We disable interrupts for the CPU tests, as otherwise they will mess up the tests run from RAM (until they have interrupt handling implemented)
);

// 100 MHz clock
always begin
    #5 clk = ~clk;
end

integer clk_counter = 0;
always @(posedge clk) begin
    clk_counter = clk_counter + 1;
    if (clk_counter == 30000) begin
        $display("Simulation finished.");
        $finish;
    end
end

endmodule

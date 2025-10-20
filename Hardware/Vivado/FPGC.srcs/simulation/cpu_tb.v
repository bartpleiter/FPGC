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
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/InterruptController.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/AddressDecoder.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/CPU/CacheController.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/ROM.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/VRAM.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/DPRAM.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/MIG7Mock.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/W25Q128JV.v"

`include "Hardware/Vivado/FPGC.srcs/verilog/Memory/MemoryUnit.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/IO/UARTrx.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/IO/UARTtx.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/IO/SimpleSPI.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/IO/MicrosCounter.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/IO/OStimer.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/IO/UARTresetDetector.v"

`include "Hardware/Vivado/FPGC.srcs/verilog/GPU/FSX.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/GPU/BGWrenderer.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/GPU/PixelEngine.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/GPU/TimingGenerator.v"
`include "Hardware/Vivado/FPGC.srcs/verilog/GPU/HDMI/RGB8toRGB24.v"

module cpu_tb ();

reg clk = 1'b0;
reg clk100 = 1'b1; // To align rising edge with clk
reg reset = 1'b0;
wire uart_reset; // Reset signal from UARTresetDetector

// Inaccurate but good enough for simulation
wire clkPixel = clk;
wire clkTMDShalf = clk100;


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

//-----------------------L1i RAM (100&50MHz)-------------------------

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

//-----------------------L1d RAM (100&50MHz)------------------------

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

//-----------------------MIG7 Mock (100MHz)-------------------------

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
    .RAM_DEPTH(33554432),
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/mig7mock.list")
) mig7mock (
    .sys_clk_i(clk100),
    .sys_rst(reset),
    .ui_clk(), // Not used in simulation
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
wire        l1d_cache_controller_done;
wire [31:0] l1d_cache_controller_result;

wire l1_clear_cache;
wire l1_clear_cache_done;

// Instantiate CacheController
CacheController #(
    .ADDR_WIDTH(29),
    .DATA_WIDTH(256),
    .MASK_WIDTH(32)
) cache_controller (
    .clk100(clk100),
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
    .cpu_EXMEM2_done(l1d_cache_controller_done),
    .cpu_EXMEM2_result(l1d_cache_controller_result),

    .cpu_clear_cache(l1_clear_cache),
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

    // MIG7 interface: use MIG7Mock signals directly
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

//-----------------------FSX-------------------------
wire frameDrawn;
FSX fsx (
    // Clocks and reset
    .clkPixel(clkPixel),
    .clkTMDShalf(clkTMDShalf),

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

//-----------------------SPI Flash (simulation models)-------------------------
// SPI0 Flash 1
wire SPI0_clk;
wire SPI0_cs; 
wire SPI0_mosi;
wire SPI0_miso;
wire SPI0_wp = 1'b1;
wire SPI0_hold = 1'b1;

W25Q128JV #(
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/spiflash1.list")
) spiflash1 (
.CLK    (SPI0_clk),
.DIO    (SPI0_mosi),
.CSn    (SPI0_cs),
.WPn    (SPI0_wp),
.HOLDn  (SPI0_hold),
.DO     (SPI0_miso)
);

// SPI1 Flash 2
wire SPI1_clk;
wire SPI1_cs;
wire SPI1_mosi;
wire SPI1_miso;
wire SPI1_wp = 1'b1;
wire SPI1_hold = 1'b1;

W25Q128JV #(
    .LIST("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/spiflash2.list")
) spiflash2 (
.CLK    (SPI1_clk),
.DIO    (SPI1_mosi),
.CSn    (SPI1_cs),
.WPn    (SPI1_wp),
.HOLDn  (SPI1_hold),
.DO     (SPI1_miso)
);

//------------------UART data sender (simulation only)----------------------
// We exclude the signal definitions from the ifdef, so they stay available in gtkwave
wire        uart_rx;
// UART test data transmission
reg [7:0] uart_test_data [0:65535]; // Buffer for test data (64KB max)
integer uart_test_data_size = 0;
integer uart_test_index = -1; // Start before first index (bit ugly solution)
reg uart_test_active = 1'b0;
reg uart_test_start = 1'b0;
reg uart_tx_trigger = 1'b0;
wire uart_tx_done;
wire uart_tx_active;

`ifdef uart_simulation
    // UART transmitter for testing
    UARTtx #(
        .ENABLE_DISPLAY(0)
    ) uart_transmitter (
        .i_Clock(clk),
        .reset(reset),
        .i_Tx_DV(uart_tx_trigger),
        .i_Tx_Byte(uart_test_data[uart_test_index]),
        .o_Tx_Active(uart_tx_active),
        .o_Tx_Serial(uart_rx), // Connect to CPU's uart_rx
        .o_Tx_Done(uart_tx_done)
    );

    always @(posedge clk)
    begin
        if (reset) begin
            uart_test_index <= -1;
            uart_test_active <= 1'b0;
            uart_tx_trigger <= 1'b0;
        end else begin
            // Start UART transmission after delay
            if (clk_counter > 3000 && !uart_test_start) begin
                uart_test_start <= 1'b1;
                uart_test_active <= 1'b1;
            end
            
            // UART transmission state machine
            if (uart_test_active && uart_test_start) begin
                if (!uart_tx_trigger && !uart_tx_active && uart_test_index < uart_test_data_size) begin
                    // Start next byte transmission
                    uart_tx_trigger <= 1'b1;
                    uart_test_index <= uart_test_index + 1;
                    if (uart_test_index >= uart_test_data_size -1) begin
                        uart_test_active <= 1'b0;
                        uart_tx_trigger <= 1'b0;
                        $display("UART bootloader test transmission complete at time %t", $time);
                    end
                    //$display("Sending UART byte %d: 0x%02x", uart_test_index, uart_test_data[uart_test_index]);
                end else if (uart_tx_trigger && uart_tx_active) begin
                    // Clear trigger once transmission starts
                    uart_tx_trigger <= 1'b0;
                end
            end
        end
    end
`endif

//------------------Memory Unit (50MHz)----------------------
wire        mu_start;
wire [31:0] mu_addr;
wire [31:0] mu_data;
wire        mu_we;
wire [31:0] mu_q;
wire        mu_done;

wire        uart_tx;
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
    .reset(reset || uart_reset), // Reset MemoryUnit on UART magic sequence
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
    .SPI0_mosi(SPI0_mosi),
    .SPI0_miso(SPI0_miso),
    .SPI0_cs(SPI0_cs),

    .SPI1_clk(SPI1_clk),
    .SPI1_mosi(SPI1_mosi),
    .SPI1_miso(SPI1_miso),
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
    .SPI5_cs(SPI5_cs)
);

//-----------------------CPU-------------------------
B32P2 cpu (
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
    .l1d_cache_controller_done(l1d_cache_controller_done),
    .l1d_cache_controller_result(l1d_cache_controller_result),

    .l1_clear_cache(l1_clear_cache),
    .l1_clear_cache_done(l1_clear_cache_done),

    // Memory Unit connections
    .mu_start(mu_start),
    .mu_addr(mu_addr),
    .mu_data(mu_data),
    .mu_we(mu_we),
    .mu_q(mu_q),
    .mu_done(mu_done),

    // Interrupts, right is highest priority
    .interrupts({3'd0, frameDrawn, OST3_int, OST2_int, OST1_int, uart_irq})
);

// 100 MHz clock
always begin
    #5 clk100 = ~clk100;
end

// 50 MHz clock
always begin
    #10 clk = ~clk;
end

integer clk_counter = 0;
always @(posedge clk) begin
    clk_counter = clk_counter + 1;
    if (clk_counter == 5500) begin // 55000
        $display("Simulation finished.");
        $finish;
    end
end

initial
begin
    $dumpfile("Hardware/Vivado/FPGC.srcs/simulation/output/cpu.vcd");
    $dumpvars;

    `ifdef uart_simulation
        // Initialize UART test data
        $readmemb("/home/bart/repos/FPGC/Hardware/Vivado/FPGC.srcs/simulation/memory/uartprog_8bit.list", uart_test_data);
        // Count actual data size (find first uninitialized location)
        uart_test_data_size = 0;
        for (integer i = 0; i < 262144; i = i + 1) begin
            if (uart_test_data[i] !== 8'hxx) begin
                uart_test_data_size = i + 1;
            end else begin
                i = 262144; // Break loop
            end
        end
    `endif
end

endmodule

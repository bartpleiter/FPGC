/*
 * FPGC Top-Level — Cyclone 10 CL120 (FPGC-Camera build)
 *
 * Same design as the Cyclone IV EP4CE40, except:
 *   - Two 16-bit SDRAM chips (separate data buses, shared control signals)
 *   - No external SRAM — pixel framebuffer uses internal BRAM (76,800 bytes)
 *   - GPU outputs to SPI display (ILI9341 320×240) instead of HDMI
 *   - OV7670 camera capture subsystem
 *   - Dedicated button input module
 *
 * Core module test configuration:
 *   Only SPI display, UART, single LED, and reset are connected.
 *   All other peripherals are tied off internally.
 *   Uncomment the port declarations below when full camera PCB is designed.
 */
module FPGC (
    // Clocks
    input wire          sys_clk_50,
    // input wire       sys_clk_header,  // Uncomment for camera PCB

    // SDRAM0
    output wire         SDRAM0_CLK,
    output wire         SDRAM0_CS_N,
    output wire         SDRAM0_WE_N,
    output wire         SDRAM0_CAS_N,
    output wire         SDRAM0_RAS_N,
    output wire         SDRAM0_CKE,
    output wire [12:0]  SDRAM0_ADDR,
    output wire [1:0]   SDRAM0_BA,
    output wire         SDRAM0_LDQM,
    output wire         SDRAM0_UDQM,
    inout wire  [15:0]  SDRAM0_DQ,

    // SDRAM1
    output wire         SDRAM1_CLK,
    output wire         SDRAM1_CS_N,
    output wire         SDRAM1_WE_N,
    output wire         SDRAM1_CAS_N,
    output wire         SDRAM1_RAS_N,
    output wire         SDRAM1_CKE,
    output wire [12:0]  SDRAM1_ADDR,
    output wire [1:0]   SDRAM1_BA,
    output wire         SDRAM1_LDQM,
    output wire         SDRAM1_UDQM,
    inout wire  [15:0]  SDRAM1_DQ,

    // Uncomment for camera PCB:
    // // USB Host 1
    // output wire      usb1_cs,
    // output wire      usb1_clk,
    // output wire      usb1_mosi,
    // input  wire      usb1_miso,
    // input  wire      usb1_nint,
    //
    // // USB Host 2
    // output wire      usb2_cs,
    // output wire      usb2_clk,
    // output wire      usb2_mosi,
    // input  wire      usb2_miso,
    // input  wire      usb2_nint,

    // SPI Display (ILI9341)
    output wire         spi_disp_clk,
    output wire         spi_disp_mosi,
    output wire         spi_disp_cs_n,
    output wire         spi_disp_dc,
    output wire         spi_disp_rst_n,
    output wire         spi_disp_bl,

    // Uncomment for camera PCB:
    // // Camera (OV7670)
    // input  wire [7:0] cam_data,
    // input  wire       cam_vsync,
    // input  wire       cam_href,
    // input  wire       cam_pclk,
    // output wire       cam_xclk,
    // output wire       cam_reset_n,
    // output wire       cam_pwdn,
    // output wire       cam_scl,
    // inout  wire       cam_sda,
    //
    // // SPI Flash 1
    // output wire       flash1_cs,
    // output wire       flash1_clk,
    // output wire       flash1_mosi,
    // input  wire       flash1_miso,
    // output wire       flash1_wp_n,
    // output wire       flash1_hold_n,
    //
    // // SPI Flash 2 (QSPI-capable)
    // output wire       flash2_cs,
    // output wire       flash2_clk,
    // inout  wire       flash2_mosi,
    // inout  wire       flash2_miso,
    // inout  wire       flash2_wp_n,
    // inout  wire       flash2_hold_n,

    // UART
    input wire          uart_rx,
    output wire         uart_tx,
    // input wire       uart_rts_n,      // Uncomment for camera PCB
    input wire          uart_dtr_n,

    // Uncomment for camera PCB:
    // // Micro SD Card
    // output wire       sd_cs,
    // output wire       sd_clk,
    // output wire       sd_mosi,
    // input  wire       sd_miso,
    // output wire       sd_data2_nc,
    // input wire        sd_data1_nc,
    //
    // // Dipswitch
    // input wire [3:0]  dipsw,
    //
    // // Audio DAC
    // output wire [7:0] audio_dac_data,
    //
    // // Ethernet
    // output wire       eth_cs,
    // output wire       eth_clk,
    // output wire       eth_mosi,
    // input  wire       eth_miso,
    // input  wire       eth_nint,
    //
    // // LEDs (accent LEDs on camera PCB)
    // output wire       led_gpu,
    // output wire       led_flash,
    // output wire       led_usb,
    // output wire       led_eth,
    // output wire       led_user,
    // output wire       led_uart_rx,
    // output wire       led_uart_tx,

    // LED (single LED on core module)
    output wire         led,

    // Reset
    input wire          reset_n
    // Uncomment for camera PCB:
    // input wire [7:0]  btn              // 8 active-low camera control buttons
);

/******************************************************************************
 * Tie-offs for peripherals not connected on core module
 *
 * When the full camera PCB is designed, uncomment the port declarations
 * in the module header and remove these internal wire declarations.
 ******************************************************************************/

// USB Host 1 — no device on core module
wire usb1_cs, usb1_clk, usb1_mosi;
wire usb1_miso = 1'b1;     // MISO idle high
wire usb1_nint = 1'b1;     // No interrupt (active-low)

// USB Host 2 — no device on core module
wire usb2_cs, usb2_clk, usb2_mosi;
wire usb2_miso = 1'b1;
wire usb2_nint = 1'b1;

// SPI Display backlight — directly from FSX (active on core module via AB19)
// (no internal tie-off needed, spi_disp_bl is a real port)

// Camera — no device on core module
wire [7:0] cam_data  = 8'd0;
wire       cam_vsync = 1'b0;
wire       cam_href  = 1'b0;
wire       cam_pclk  = 1'b0;
wire       cam_xclk;           // Driven by PLL output
wire       cam_reset_n;        // Driven by reset logic
wire       cam_pwdn;           // Driven by static assign
wire       cam_scl;            // Driven by I2C master
wire       cam_sda_in = 1'b1;  // SDA reads high (no device on bus)

// SPI Flash 1 — no device on core module
wire flash1_cs, flash1_clk, flash1_mosi;
wire flash1_miso = 1'b1;
wire flash1_wp_n, flash1_hold_n;

// SPI Flash 2 (QSPI) — no device on core module
wire flash2_cs, flash2_clk;

// UART RTS — not connected on core module
wire uart_rts_n = 1'b1;

// SD Card — no device on core module
wire sd_cs, sd_clk, sd_mosi;
wire sd_miso     = 1'b1;
wire sd_data2_nc;
wire sd_data1_nc = 1'b1;

// Dipswitches — boot via UART (boot_mode=dipsw[3]=0 → UART bootloader from ROM)
wire [3:0] dipsw = 4'b0000;

// Audio DAC — not connected
wire [7:0] audio_dac_data;

// Ethernet — no device on core module
wire eth_cs, eth_clk, eth_mosi;
wire eth_miso = 1'b1;
wire eth_nint = 1'b1;

// Individual LEDs — only one LED on core module
wire led_gpu, led_flash, led_usb, led_eth, led_uart_rx, led_uart_tx;
wire led_user;

// Buttons — all released (active-low), no button header on core module
wire [7:0] btn = 8'hFF;

/******************************************************************************
 * Static Assignments
 ******************************************************************************/
// SPI Flash 1 is in 1x mode
assign flash1_wp_n   = 1'b1;
assign flash1_hold_n = 1'b1;

// Flash 2 (BRFS) QSPI — no device, all inputs read high
// flash2_io_out and flash2_io_oe are driven by MemoryUnit but unused
wire [3:0] flash2_io_out;
wire [3:0] flash2_io_oe;
wire [3:0] flash2_io_in = 4'hF;

// SD Card is in SPI mode
assign sd_data2_nc = 1'b1;

// Audio DAC not used yet
assign audio_dac_data = 8'd0;

// Camera static assignments
assign cam_pwdn    = 1'b0;       // Power on
assign cam_reset_n = ~reset;     // Release reset when system is up

// Single LED output — directly from user LED state
assign led = led_user_state;

/******************************************************************************
 * Dip switch
 ******************************************************************************/
wire boot_mode = dipsw[3];

/******************************************************************************
 * Clocks
 ******************************************************************************/
wire clk100;        // CPU and main logic (100MHz)
wire clkSDRAM;      // 100MHz Phase-shifted for SDRAM
wire clk25;         // 25 MHz for camera XCLK

pll1 main_pll_inst (
    .inclk0(sys_clk_50),
    .c0(),              // 50MHz (unused)
    .c1(clk100),        // 100MHz
    .c2(clkSDRAM),      // 100MHz with phase shift
    .c3(clk25),         // 25MHz (camera XCLK)
    .c4()               // 125MHz (unused, was TMDS)
);

// Camera master clock
assign cam_xclk = clk25;

/******************************************************************************
 * SDRAM — two 16-bit chips, shared control signals
 ******************************************************************************/
wire          SDRAM_CSn;
wire          SDRAM_WEn;
wire          SDRAM_CASn;
wire          SDRAM_RASn;
wire          SDRAM_CKE;
wire [12:0]   SDRAM_A;
wire [1:0]    SDRAM_BA;

assign SDRAM0_CLK    = clkSDRAM;
assign SDRAM1_CLK    = clkSDRAM;
assign SDRAM0_CS_N   = SDRAM_CSn;
assign SDRAM1_CS_N   = SDRAM_CSn;
assign SDRAM0_WE_N   = SDRAM_WEn;
assign SDRAM1_WE_N   = SDRAM_WEn;
assign SDRAM0_CAS_N  = SDRAM_CASn;
assign SDRAM1_CAS_N  = SDRAM_CASn;
assign SDRAM0_RAS_N  = SDRAM_RASn;
assign SDRAM1_RAS_N  = SDRAM_RASn;
assign SDRAM0_CKE    = SDRAM_CKE;
assign SDRAM1_CKE    = SDRAM_CKE;
assign SDRAM0_ADDR   = SDRAM_A;
assign SDRAM1_ADDR   = SDRAM_A;
assign SDRAM0_BA     = SDRAM_BA;
assign SDRAM1_BA     = SDRAM_BA;

/******************************************************************************
 * Reset
 ******************************************************************************/
wire reset;
wire reset_dtr;

DtrReset dtr_reset (
    .clk(clk100),
    .dtr(uart_dtr_n),
    .reset_dtr(reset_dtr)
);

reg [1:0] reset_sync = 2'b11;
always @(posedge clk100)
begin
    reset_sync <= {reset_sync[0], ~reset_n | reset_dtr};
end
assign reset = reset_sync[1];

/******************************************************************************
 * LEDs
 ******************************************************************************/
wire led_flash_activity;
wire led_usb_activity;
wire led_eth_activity;
wire led_gpu_activity;
wire led_user_state;

localparam LED_ACTIVITY_HOLD_CYCLES = 24'd8000000;

ActivityLED #(
    .HOLD_CYCLES(LED_ACTIVITY_HOLD_CYCLES)
) led_flash_activity_driver (
    .clk(clk100),
    .reset(reset),
    .activity(led_flash_activity),
    .led(led_flash)
);

ActivityLED #(
    .HOLD_CYCLES(LED_ACTIVITY_HOLD_CYCLES)
) led_usb_activity_driver (
    .clk(clk100),
    .reset(reset),
    .activity(led_usb_activity),
    .led(led_usb)
);

ActivityLED #(
    .HOLD_CYCLES(LED_ACTIVITY_HOLD_CYCLES)
) led_eth_activity_driver (
    .clk(clk100),
    .reset(reset),
    .activity(led_eth_activity),
    .led(led_eth)
);

ActivityLED #(
    .HOLD_CYCLES(LED_ACTIVITY_HOLD_CYCLES)
) led_gpu_activity_driver (
    .clk(clk100),
    .reset(reset),
    .activity(led_gpu_activity),
    .led(led_gpu)
);

ActivityLED #(
    .HOLD_CYCLES(LED_ACTIVITY_HOLD_CYCLES)
) led_uart_rx_activity_driver (
    .clk(clk100),
    .reset(reset),
    .activity(uart_rx_line_activity),
    .led(led_uart_rx)
);

ActivityLED #(
    .HOLD_CYCLES(LED_ACTIVITY_HOLD_CYCLES)
) led_uart_tx_activity_driver (
    .clk(clk100),
    .reset(reset),
    .activity(uart_tx_line_activity),
    .led(led_uart_tx)
);

assign led_user = led_user_state;

assign led_gpu_activity = vramPX_w_we;

// UART line activity detection
reg uart_rx_prev = 1'b1;
reg uart_tx_prev = 1'b1;
wire uart_rx_line_activity = (uart_rx != uart_rx_prev);
wire uart_tx_line_activity = (uart_tx != uart_tx_prev);

always @(posedge clk100)
begin
    if (reset)
    begin
        uart_rx_prev <= 1'b1;
        uart_tx_prev <= 1'b1;
    end
    else
    begin
        uart_rx_prev <= uart_rx;
        uart_tx_prev <= uart_tx;
    end
end

/******************************************************************************
 * ROM
 ******************************************************************************/
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
    .LIST("/home/bart/repos/FPGC/Hardware/FPGA/Verilog/MemoryLists/rom_bootloader.list")
) rom (
    .clk (clk100),

    .fe_addr(rom_fe_addr),
    .fe_oe(rom_fe_oe),
    .fe_q(rom_fe_q),
    .fe_hold(rom_fe_hold),

    .mem_addr(rom_mem_addr),
    .mem_q(rom_mem_q)
);

/******************************************************************************
 * VRAM32
 ******************************************************************************/
wire [10:0] vram32_gpu_addr;
wire [31:0] vram32_gpu_d;
wire        vram32_gpu_we;
wire [31:0] vram32_gpu_q;

wire [10:0] vram32_cpu_addr;
wire [31:0] vram32_cpu_d;
wire        vram32_cpu_we;
wire [31:0] vram32_cpu_q;

// GPU will not write to VRAM32 (read-only for window tile layer)
assign vram32_gpu_we = 1'b0;
assign vram32_gpu_d  = 32'd0;
// GPU read address driven by FSX window tile compositing
wire [10:0] fsx_vram32_addr;
assign vram32_gpu_addr = fsx_vram32_addr;

VRAM #(
    .WIDTH(32),
    .WORDS(1056),
    .ADDR_BITS(11),
    .LIST("/home/bart/repos/FPGC/Hardware/FPGA/Verilog/MemoryLists/vram32.list")
) vram32 (
    //CPU port
    .cpu_clk (clk100),
    .cpu_d   (vram32_cpu_d),
    .cpu_addr(vram32_cpu_addr),
    .cpu_we  (vram32_cpu_we),
    .cpu_q   (vram32_cpu_q),

    //GPU port
    .gpu_clk (clk100),
    .gpu_d   (vram32_gpu_d),
    .gpu_addr(vram32_gpu_addr),
    .gpu_we  (vram32_gpu_we),
    .gpu_q   (vram32_gpu_q)
);

/******************************************************************************
 * VRAM8
 ******************************************************************************/
wire [13:0] vram8_gpu_addr;
wire [7:0]  vram8_gpu_d;
wire        vram8_gpu_we;
wire [7:0]  vram8_gpu_q;

wire [13:0] vram8_cpu_addr;
wire [7:0]  vram8_cpu_d;
wire        vram8_cpu_we;
wire [7:0]  vram8_cpu_q;

// GPU will not write to VRAM8 (read-only for window tile layer)
assign vram8_gpu_we = 1'b0;
assign vram8_gpu_d  = 8'd0;
// GPU read address driven by FSX window tile compositing
wire [13:0] fsx_vram8_addr;
assign vram8_gpu_addr = fsx_vram8_addr;

VRAM #(
    .WIDTH(8),
    .WORDS(8194),
    .ADDR_BITS(14),
    .LIST("/home/bart/repos/FPGC/Hardware/FPGA/Verilog/MemoryLists/vram8.list")
) vram8 (
    // CPU port
    .cpu_clk (clk100),
    .cpu_d   (vram8_cpu_d),
    .cpu_addr(vram8_cpu_addr),
    .cpu_we  (vram8_cpu_we),
    .cpu_q   (vram8_cpu_q),

    // GPU port
    .gpu_clk (clk100),
    .gpu_d   (vram8_gpu_d),
    .gpu_addr(vram8_gpu_addr),
    .gpu_we  (vram8_gpu_we),
    .gpu_q   (vram8_gpu_q)
);

/******************************************************************************
 * VRAMPX — Internal BRAM (no external SRAM needed)
 ******************************************************************************/
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

// DMA VRAMPX write signals
wire        dma_vp_we;
wire [16:0] dma_vp_addr;
wire [7:0]  dma_vp_data;

// VRAMPX write-port mux: DMA engine writes win over CPU writes when active.
// CPU is parked in dma_copy()'s busy-wait during DMA, so no real contention.
wire [16:0] vramPX_w_addr = dma_vp_we ? dma_vp_addr : vramPX_cpu_addr;
wire [7:0]  vramPX_w_data = dma_vp_we ? dma_vp_data : vramPX_cpu_d;
wire        vramPX_w_we   = dma_vp_we | vramPX_cpu_we;

VRAM #(
    .WIDTH(8),
    .WORDS(76800),
    .ADDR_BITS(17),
    .LIST("/home/bart/repos/FPGC/Hardware/FPGA/Verilog/MemoryLists/vramPX.list")
) vramPX (
    // CPU/DMA write port (muxed above)
    .cpu_clk (clk100),
    .cpu_d   (vramPX_w_data),
    .cpu_addr(vramPX_w_addr),
    .cpu_we  (vramPX_w_we),
    .cpu_q   (vramPX_cpu_q),

    // GPU read port
    .gpu_clk (clk100),
    .gpu_d   (vramPX_gpu_d),
    .gpu_addr(vramPX_gpu_addr),
    .gpu_we  (vramPX_gpu_we),
    .gpu_q   (vramPX_gpu_q)
);

/******************************************************************************
 * L1i RAM
 ******************************************************************************/
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

DPRAM #(
    .WIDTH(271),
    .WORDS(128),
    .ADDR_BITS(7)
) l1i_ram (
    .clk_pipe(clk100),
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

/******************************************************************************
 * L1d RAM
 ******************************************************************************/
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

DPRAM #(
    .WIDTH(271),
    .WORDS(128),
    .ADDR_BITS(7)
) l1d_ram (
    .clk_pipe(clk100),
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

/******************************************************************************
 * SDRAM Controller
 ******************************************************************************/
wire [20:0]     sdc_addr;
wire [255:0]    sdc_data;
wire            sdc_we;
wire            sdc_start;
wire            sdc_done;
wire [255:0]    sdc_q;
SDRAMcontroller sdc (
    .clk(clk100),
    .reset(1'b0), // Do not reset the SDRAM controller

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
    // Two 16-bit chips combined into one 32-bit bus
    .SDRAM_DQM({SDRAM1_UDQM, SDRAM1_LDQM, SDRAM0_UDQM, SDRAM0_LDQM}),
    .SDRAM_DQ({SDRAM1_DQ, SDRAM0_DQ})
);

/******************************************************************************
 * CacheController
 ******************************************************************************/
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

CacheController cache_controller (
    .clk100(clk100),
    .reset(reset),

    // CPU pipeline interface
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

/******************************************************************************
 * SDRAM Arbiter (CPU priority; DMA engine)
 ******************************************************************************/
wire [20:0]     cpu_sdc_addr;
wire [255:0]    cpu_sdc_data;
wire            cpu_sdc_we;
wire            cpu_sdc_start;
wire            cpu_sdc_done;
wire [255:0]    cpu_sdc_q;

// DMA engine wires
wire [20:0]     dma_sd_addr;
wire [255:0]    dma_sd_data;
wire            dma_sd_we;
wire            dma_sd_start;
wire            dma_sd_done;
wire [255:0]    dma_sd_q;

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

// DMA SPI burst port
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
    .clk(clk100),
    .reset(reset),

    // CPU port (from CacheController)
    .cpu_addr(cpu_sdc_addr),
    .cpu_data(cpu_sdc_data),
    .cpu_we(cpu_sdc_we),
    .cpu_start(cpu_sdc_start),
    .cpu_done(cpu_sdc_done),
    .cpu_q(cpu_sdc_q),

    // DMA port (engine)
    .dma_addr(dma_sd_addr),
    .dma_data(dma_sd_data),
    .dma_we(dma_sd_we),
    .dma_start(dma_sd_start),
    .dma_done(dma_sd_done),
    .dma_q(dma_sd_q),

    // SDRAM controller side
    .sdc_addr(sdc_addr),
    .sdc_data(sdc_data),
    .sdc_we(sdc_we),
    .sdc_start(sdc_start),
    .sdc_done(sdc_done),
    .sdc_q(sdc_q),

    // Debug
    .dbg_busy(sdram_arb_busy)
);

/******************************************************************************
 * FSX GPU — SPI Display (ILI9341 320×240)
 ******************************************************************************/
wire frameDrawn;

// Pixel palette CPU write signals
wire        palette_cpu_we;
wire [7:0]  palette_cpu_addr;
wire [23:0] palette_cpu_wdata;

FSX fsx (
    .clk(clk100),
    .reset(reset),

    // SPI display output
    .spi_clk       (spi_disp_clk),
    .spi_mosi      (spi_disp_mosi),
    .spi_cs_n      (spi_disp_cs_n),
    .spi_dc        (spi_disp_dc),
    .lcd_rst_n     (spi_disp_rst_n),
    .lcd_backlight (spi_disp_bl),

    // VRAMPX (BRAM)
    .vramPX_addr(vramPX_gpu_addr),
    .vramPX_q   (vramPX_gpu_q),

    // VRAM8/VRAM32 for window tile layer
    .vram8_addr (fsx_vram8_addr),
    .vram8_q    (vram8_gpu_q),
    .vram32_addr(fsx_vram32_addr),
    .vram32_q   (vram32_gpu_q),

    // Palette CPU write port
    .palette_we(palette_cpu_we),
    .palette_addr(palette_cpu_addr),
    .palette_wdata(palette_cpu_wdata),

    // Status
    .frame_drawn(frameDrawn)
);

/******************************************************************************
 * Memory Unit
 ******************************************************************************/
wire        mu_start;
wire [31:0] mu_addr;
wire [31:0] mu_data;
wire        mu_we;
wire [31:0] mu_q;

// Forward declarations for camera, I2C, and button wires
// (modules instantiated after DMA engine, but MemoryUnit needs the wires)
wire        cam_ctrl_enable;
wire        cam_ctrl_byte_phase;
wire        cam_frame_done;
wire        cam_current_buf;
wire [2:0]  cam_dbg_state;
wire [16:0] cam_dbg_frame_pixels;
wire [8:0]  cam_dbg_line_count;
wire [11:0] cam_dbg_cache_lines;
wire [7:0]  cam_dbg_partial_drops;
reg         cam_vsync_sync;
reg         cam_href_sync;

// I2C master signals (generic, sole bus master)
wire        i2c_start;
wire        i2c_rw;
wire [6:0]  i2c_dev_addr;
wire [7:0]  i2c_reg_addr;
wire [7:0]  i2c_wr_data;
wire        i2c_busy;
wire        i2c_ack_err;
wire [7:0]  i2c_rd_data;
wire [4:0]  i2c_dbg_state;
wire        i2c_scl_oe;
wire        i2c_sda_oe;

wire [31:0] btn_state;
wire        btn_irq;
wire [255:0] cam_line_data;
wire         cam_line_ready;
wire         cam_line_ack;
wire        mu_done;

// SDRAM arbiter busy flag (debug)
wire        sdram_arb_busy;

// Interrupt outputs
wire        uart_irq;
wire        OST1_int;
wire        OST2_int;
wire        OST3_int;

MemoryUnit memory_unit (
    .clk(clk100),
    .reset(reset),
    .uart_reset(), // DTR available, no need for UART magic reset

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

    .flash_spi_activity(led_flash_activity),
    .usb_spi_activity(led_usb_activity),
    .eth_spi_activity(led_eth_activity),
    .user_led_state(led_user_state),

    .boot_mode(boot_mode),

    // Flash 1
    .SPI0_clk(flash1_clk),
    .SPI0_mosi(flash1_mosi),
    .SPI0_miso(flash1_miso),
    .SPI0_cs(flash1_cs),

    // Flash 2 (QSPI)
    .SPI1_clk(flash2_clk),
    .SPI1_io_out(flash2_io_out),
    .SPI1_io_oe(flash2_io_oe),
    .SPI1_io_in(flash2_io_in),
    .SPI1_cs(flash2_cs),

    // USB 1
    .SPI2_clk(usb1_clk),
    .SPI2_mosi(usb1_mosi),
    .SPI2_miso(usb1_miso),
    .SPI2_cs(usb1_cs),
    .SPI2_nint(usb1_nint),

    // USB 2
    .SPI3_clk(usb2_clk),
    .SPI3_mosi(usb2_mosi),
    .SPI3_miso(usb2_miso),
    .SPI3_cs(usb2_cs),
    .SPI3_nint(usb2_nint),

    // Ethernet
    .SPI4_clk(eth_clk),
    .SPI4_mosi(eth_mosi),
    .SPI4_miso(eth_miso),
    .SPI4_cs(eth_cs),
    .SPI4_nint(eth_nint),

    // SD Card
    .SPI5_clk(sd_clk),
    .SPI5_mosi(sd_mosi),
    .SPI5_miso(sd_miso),
    .SPI5_cs(sd_cs),

    // DMA peer ports
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

    // DMA SPI burst port
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

    // Camera
    .cam_ctrl_enable(cam_ctrl_enable),
    .cam_ctrl_byte_phase(cam_ctrl_byte_phase),
    .cam_frame_done(cam_frame_done),
    .cam_current_buf(cam_current_buf),

    // I2C (generic)
    .i2c_start(i2c_start),
    .i2c_rw(i2c_rw),
    .i2c_dev_addr(i2c_dev_addr),
    .i2c_reg_addr(i2c_reg_addr),
    .i2c_wr_data(i2c_wr_data),
    .i2c_busy(i2c_busy),
    .i2c_ack_err(i2c_ack_err),
    .i2c_rd_data(i2c_rd_data),
    .i2c_dbg_state(i2c_dbg_state),
    .cam_vsync_raw(cam_vsync_sync),
    .cam_href_raw(cam_href_sync),
    .cam_dbg_state(cam_dbg_state),
    .cam_dbg_frame_pixels(cam_dbg_frame_pixels),
    .cam_dbg_line_count(cam_dbg_line_count),
    .cam_dbg_cache_lines(cam_dbg_cache_lines),
    .cam_dbg_partial_drops(cam_dbg_partial_drops),
    .sdram_arb_busy(sdram_arb_busy),

    // GPU timing (no raster scan on SPI display — tie off)
    .gpu_vblank(1'b0),
    .gpu_v_count(12'd0),

    // Buttons
    .btn_state(btn_state)
);

/******************************************************************************
 * DMA engine
 ******************************************************************************/
DMAengine dma_engine (
    .clk(clk100),
    .reset(reset),

    .reg_addr(dma_reg_addr),
    .reg_we(dma_reg_we),
    .reg_data(dma_reg_data),
    .reg_q(dma_reg_q),

    .sd_addr(dma_sd_addr),
    .sd_data(dma_sd_data),
    .sd_we(dma_sd_we),
    .sd_start(dma_sd_start),
    .sd_done(dma_sd_done),
    .sd_q(dma_sd_q),

    .iop_start(dma_iop_start),
    .iop_we(dma_iop_we),
    .iop_addr(dma_iop_addr),
    .iop_data(dma_iop_data),
    .iop_done(dma_iop_done),
    .iop_q(dma_iop_q),

    .vp_we(dma_vp_we),
    .vp_addr(dma_vp_addr),
    .vp_data(dma_vp_data),
    .vp_full(1'b0), // BRAM accepts writes every cycle — no backpressure

    // DMA SPI burst port
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

    .irq(dma_irq),

    // Camera cache-line handshake
    .cam_line_ready(cam_line_ready),
    .cam_line_data(cam_line_data),
    .cam_line_ack(cam_line_ack),
    .cam_frame_done(cam_frame_done)
);

/******************************************************************************
 * Camera Subsystem (OV7670)
 ******************************************************************************/

// Synchronize raw VSYNC and HREF pins into clk100 domain for diagnostics
reg cam_vsync_s1 = 1'b0;
reg cam_href_s1  = 1'b0;
always @(posedge clk100) begin
    cam_vsync_s1   <= cam_vsync;
    cam_vsync_sync <= cam_vsync_s1;
    cam_href_s1    <= cam_href;
    cam_href_sync  <= cam_href_s1;
end

// V2 CameraCapture does its own internal synchronization for cam_pclk,
// cam_vsync, cam_href, and cam_data. Pass raw pins directly.
CameraCapture camera_capture (
    .clk            (clk100),
    .reset          (reset),
    .cam_pclk       (cam_pclk),
    .cam_vsync      (cam_vsync),
    .cam_href       (cam_href),
    .cam_data       (cam_data),
    .line_data      (cam_line_data),
    .line_ready     (cam_line_ready),
    .line_ack       (cam_line_ack),
    .ctrl_enable    (cam_ctrl_enable),
    .ctrl_byte_phase(cam_ctrl_byte_phase),
    .frame_done     (cam_frame_done),
    .current_buf    (cam_current_buf),
    .dbg_state          (cam_dbg_state),
    .dbg_frame_pixels   (cam_dbg_frame_pixels),
    .dbg_line_count     (cam_dbg_line_count),
    .dbg_cache_lines    (cam_dbg_cache_lines),
    .dbg_partial_drops  (cam_dbg_partial_drops)
);

// ---- I2C Master (generic, sole bus master) ----
// All OV7670 configuration is done in software via I2C_CMD MMIO register.
// No hardware CameraConfigure — removed.

I2C_master #(
    .CLK_FREQ(100_000_000),
    .I2C_FREQ(100_000)
) i2c_master_inst (
    .clk      (clk100),
    .reset    (reset),
    .start    (i2c_start),
    .rw       (i2c_rw),
    .dev_addr (i2c_dev_addr),
    .reg_addr (i2c_reg_addr),
    .wr_data  (i2c_wr_data),
    .rd_data  (i2c_rd_data),
    .busy     (i2c_busy),
    .ack_err  (i2c_ack_err),
    .scl_oe   (i2c_scl_oe),
    .sda_oe   (i2c_sda_oe),
    .sda_in   (cam_sda_in),
    .dbg_state_out(i2c_dbg_state)
);

// SCL: push-pull (OV7670 doesn't clock-stretch)
// SDA: open-drain (bidirectional) — no device on core module, sda_in tied high above
assign cam_scl = i2c_scl_oe ? 1'b0 : 1'b1;
// cam_sda not driven (no bidir pin on core module)

/******************************************************************************
 * Button Input
 ******************************************************************************/
ButtonInput #(
    .NUM_BUTTONS(8),
    .CLK_FREQ(100_000_000),
    .DEBOUNCE_MS(20)
) button_input (
    .clk         (clk100),
    .reset       (reset),
    .btn_pins    (btn),
    .btn_state   (btn_state),
    .btn_changed (btn_irq)
);

/******************************************************************************
 * CPU
 ******************************************************************************/
// FSX runs on clk100 (same as CPU), no domain crossing needed
wire frameDrawn_CPU = frameDrawn;

B32P3 #(.NUM_INTERRUPTS(8)) cpu (
    // Clock and reset
    .clk(clk100),
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

    // VRAMPX (BRAM — reads work, no backpressure)
    .vramPX_addr(vramPX_cpu_addr),
    .vramPX_d(vramPX_cpu_d),
    .vramPX_we(vramPX_cpu_we),
    .vramPX_q(vramPX_cpu_q),
    .vramPX_fifo_full(1'b0), // BRAM never stalls

    // Pixel Palette
    .palette_we(palette_cpu_we),
    .palette_addr(palette_cpu_addr),
    .palette_wdata(palette_cpu_wdata),

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
    // bit0=UART, bit1=OST1, bit2=OST2, bit3=OST3, bit4=FrameDrawn, bit5=ENC28J60_RX, bit6=DMA, bit7=ButtonChange
    .interrupts({btn_irq, dma_irq, ~eth_nint, frameDrawn_CPU, OST3_int, OST2_int, OST1_int, uart_irq})
);

endmodule

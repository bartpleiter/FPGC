/*
 * FPGC Top-Level — Cyclone 10 CL120
 *
 * Same design as the Cyclone IV EP4CE40, except:
 *   - Two 16-bit SDRAM chips (separate data buses, shared control signals)
 *   - No external SRAM — pixel framebuffer uses internal BRAM (76,800 bytes)
 *   - GPU uses FSX (BRAM PixelEngine) instead of FSX_SRAM
 *   - No half_res mode
 */
module FPGC (
    // Clocks
    input wire          sys_clk_50,
    input wire          sys_clk_header,

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

    // USB Host 1
    output wire         usb1_cs,
    output wire         usb1_clk,
    output wire         usb1_mosi,
    input  wire         usb1_miso,
    input  wire         usb1_nint,

    // USB Host 2
    output wire         usb2_cs,
    output wire         usb2_clk,
    output wire         usb2_mosi,
    input  wire         usb2_miso,
    input  wire         usb2_nint,

    // HDMI
    output wire         HDMI_CLK_P,
    output wire         HDMI_CLK_N,
    output wire         HDMI_D0_P,
    output wire         HDMI_D0_N,
    output wire         HDMI_D1_P,
    output wire         HDMI_D1_N,
    output wire         HDMI_D2_P,
    output wire         HDMI_D2_N,

    // SPI Flash 1
    output wire         flash1_cs,
    output wire         flash1_clk,
    output wire         flash1_mosi,
    input  wire         flash1_miso,
    output wire         flash1_wp_n,
    output wire         flash1_hold_n,

    // SPI Flash 2 (QSPI-capable)
    output wire         flash2_cs,
    output wire         flash2_clk,
    inout  wire         flash2_mosi,
    inout  wire         flash2_miso,
    inout  wire         flash2_wp_n,
    inout  wire         flash2_hold_n,

    // UART
    input wire          uart_rx,
    output wire         uart_tx,
    input wire          uart_rts_n,
    input wire          uart_dtr_n,

    // Micro SD Card
    output wire         sd_cs,
    output wire         sd_clk,
    output wire         sd_mosi,
    input  wire         sd_miso,
    output wire         sd_data2_nc,
    input wire          sd_data1_nc,

    // Dipswitch
    input wire [3:0]    dipsw,

    // Audio DAC
    output wire [7:0]   audio_dac_data,

    // Ethernet
    output wire         eth_cs,
    output wire         eth_clk,
    output wire         eth_mosi,
    input  wire         eth_miso,
    input  wire         eth_nint,

    // LEDs
    output wire         led_gpu,
    output wire         led_flash,
    output wire         led_usb,
    output wire         led_eth,
    output wire         led_user,
    output wire         led_uart_rx,
    output wire         led_uart_tx,

    // Buttons
    input wire          reset_n,

    // Display header
    output wire         disp_1,
    output wire         disp_2,
    output wire         disp_3,
    output wire         disp_4,
    output wire         disp_5,
    output wire         disp_6,

    // GPIO Header (currently defined as inputs only until they are used)
    input wire          gpio_1,
    input wire          gpio_2,
    input wire          gpio_3,
    input wire          gpio_4,
    input wire          gpio_5,
    input wire          gpio_6,
    input wire          gpio_7,
    input wire          gpio_8,
    input wire          gpio_9
);

/******************************************************************************
 * Static Assignments
 ******************************************************************************/
// SPI Flash 1 is in 1x mode
assign flash1_wp_n   = 1'b1;
assign flash1_hold_n = 1'b1;

// Flash 2 (BRFS) QSPI bidirectional bus — same as Cyclone IV
wire [3:0] flash2_io_out;
wire [3:0] flash2_io_oe;
wire [3:0] flash2_io_in;
assign flash2_mosi   = flash2_io_oe[0] ? flash2_io_out[0] : 1'bz;
assign flash2_miso   = flash2_io_oe[1] ? flash2_io_out[1] : 1'bz;
assign flash2_wp_n   = flash2_io_oe[2] ? flash2_io_out[2] : 1'bz;
assign flash2_hold_n = flash2_io_oe[3] ? flash2_io_out[3] : 1'bz;
assign flash2_io_in  = { flash2_hold_n, flash2_wp_n, flash2_miso, flash2_mosi };

// SD Card is in SPI mode
assign sd_data2_nc = 1'b1;

// Audio DAC not used yet
assign audio_dac_data = 8'd0;

// Display header not used yet
assign disp_1 = 1'b0;
assign disp_2 = 1'b0;
assign disp_3 = 1'b0;
assign disp_4 = 1'b0;
assign disp_5 = 1'b0;
assign disp_6 = 1'b0;

/******************************************************************************
 * Dip switch
 ******************************************************************************/
wire boot_mode = dipsw[3];

/******************************************************************************
 * Clocks
 ******************************************************************************/
wire clk100;        // CPU and main logic (100MHz)
wire clkSDRAM;      // 100MHz Phase-shifted for SDRAM
wire clkGPU;        // GPU clock (25MHz)
wire clkTMDShalf;   // Half of TMDS clock for HDMI (125MHz)

pll1 main_pll_inst (
    .inclk0(sys_clk_50),
    .c0(),              // 50MHz (unused)
    .c1(clk100),        // 100MHz
    .c2(clkSDRAM),      // 100MHz with phase shift
    .c3(clkGPU),        // 25MHz
    .c4(clkTMDShalf)    // 125MHz
);

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

assign led_gpu_activity = vram32_cpu_we | vram8_cpu_we | vramPX_w_we;

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

// GPU will not write to VRAM
assign vram32_gpu_we = 1'b0;
assign vram32_gpu_d  = 32'd0;

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
    .gpu_clk (clkGPU),
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

// GPU will not write to VRAM
assign vram8_gpu_we = 1'b0;
assign vram8_gpu_d  = 8'd0;

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
    .gpu_clk (clkGPU),
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
    .gpu_clk (clkGPU),
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
    .sdc_q(sdc_q)
);

/******************************************************************************
 * FSX GPU — BRAM-based (no external SRAM)
 ******************************************************************************/
wire frameDrawn;

// Pixel palette CPU write signals
wire        palette_cpu_we;
wire [7:0]  palette_cpu_addr;
wire [23:0] palette_cpu_wdata;

FSX fsx (
    // Clocks
    .clk_pixel(clkGPU),
    .clk_tmds_half(clkTMDShalf),
    .clk_sys(clk100),

    // HDMI
    .tmds_clk_p(HDMI_CLK_P),
    .tmds_clk_n(HDMI_CLK_N),
    .tmds_d0_p (HDMI_D0_P),
    .tmds_d0_n (HDMI_D0_N),
    .tmds_d1_p (HDMI_D1_P),
    .tmds_d1_n (HDMI_D1_N),
    .tmds_d2_p (HDMI_D2_P),
    .tmds_d2_n (HDMI_D2_N),

    // VRAM32
    .vram32_addr(vram32_gpu_addr),
    .vram32_q   (vram32_gpu_q),

    // VRAM8
    .vram8_addr(vram8_gpu_addr),
    .vram8_q   (vram8_gpu_q),

    // VRAMPX (BRAM)
    .vramPX_addr(vramPX_gpu_addr),
    .vramPX_q   (vramPX_gpu_q),

    // Palette CPU write port
    .palette_we(palette_cpu_we),
    .palette_addr(palette_cpu_addr),
    .palette_wdata(palette_cpu_wdata),

    // Interrupt signal
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
wire        mu_done;

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
    .dma_reg_q(dma_reg_q)
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

    .irq(dma_irq)
);

/******************************************************************************
 * CPU
 ******************************************************************************/
// Convert frameDrawn to CPU clock domain
wire frameDrawn_CPU;
reg frameDrawn_ff1, frameDrawn_ff2;

always @(posedge clk100)
begin
    frameDrawn_ff1 <= frameDrawn;
    frameDrawn_ff2 <= frameDrawn_ff1;
end

assign frameDrawn_CPU = frameDrawn_ff2;

B32P3 cpu (
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
    // bit0=UART, bit1=OST1, bit2=OST2, bit3=OST3, bit4=FrameDrawn, bit5=ENC28J60_RX, bit6=DMA
    .interrupts({dma_irq, ~eth_nint, frameDrawn_CPU, OST3_int, OST2_int, OST1_int, uart_irq})
);

endmodule

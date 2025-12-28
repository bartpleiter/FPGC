module FPGC (
    // Clocks
    input wire          sys_clk_50,
    input wire          sys_clk_header,

    // SDRAM
    output wire         SDRAM_CLK,
    output wire         SDRAM_CSn,
    output wire         SDRAM_WEn,
    output wire         SDRAM_CASn,
    output wire         SDRAM_RASn,
    output wire         SDRAM_CKE,
    output wire [12:0]  SDRAM_A,
    output wire [1:0]   SDRAM_BA,
    output wire [3:0]   SDRAM_DQM,
    inout  wire [31:0]  SDRAM_DQ,

    // SRAM
    output wire         SRAM_CSn,
    output wire         SRAM_WEn,
    output wire         SRAM_OEn,
    output wire [18:0]  SRAM_A,
    inout  wire [7:0]   SRAM_DQ,

    // USB Host
    output wire         usb1_cs,
    output wire         usb1_clk,
    output wire         usb1_mosi,
    input  wire         usb1_miso,
    input  wire         usb1_nint,

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

    // SPI Flash
    output wire         flash1_cs,
    output wire         flash1_clk,
    output wire         flash1_mosi,
    input  wire         flash1_miso,
    output wire         flash1_wp_n,
    output wire         flash1_hold_n,

    output wire         flash2_cs,
    output wire         flash2_clk,
    output wire         flash2_mosi,
    input  wire         flash2_miso,
    output wire         flash2_wp_n,
    output wire         flash2_hold_n,

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
// SPI Flash is currently in 1x mode
assign flash1_wp_n   = 1'b1;
assign flash1_hold_n = 1'b1;
assign flash2_wp_n   = 1'b1;
assign flash2_hold_n = 1'b1;

// SD Card is currently in SPI mode
assign sd_data2_nc = 1'b1;

// Audio DAC is currently not used yet
assign audio_dac_data = 8'd0;

// Display header is currently not used yet
assign disp_1 = 1'b0;
assign disp_2 = 1'b0;
assign disp_3 = 1'b0;
assign disp_4 = 1'b0;
assign disp_5 = 1'b0;
assign disp_6 = 1'b0;

/******************************************************************************
 * Dip switch
 ******************************************************************************/
// Rightmost switch is dipsw[0]
wire boot_mode = dipsw[3];
wire half_res  = dipsw[2];

/******************************************************************************
 * Clocks
 ******************************************************************************/
wire clk50;         // CPU and main logic
wire clk100;        // Memory logic
wire clkSDRAM;      // 100MHz Phase-shifted for SDRAM
wire clkGPU;        // GPU clock
wire clkTMDShalf;   // Half of TMDS clock for HDMI

main_pll main_pll_inst (
    .inclk0(sys_clk_50),
    .c0(clk50),
    .c1(clk100),
    .c2(clkSDRAM),
    .c3(clkGPU),
    .c4(clkTMDShalf)
);

assign SDRAM_CLK = clkSDRAM;

/******************************************************************************
 * Reset
 ******************************************************************************/
wire reset;
wire reset_dtr;

DtrReset dtr_reset (
    .clk(clk50),
    .dtr(uart_dtr_n),
    .reset_dtr(reset_dtr)
);

reg [1:0] reset_sync = 2'b11;
always @(posedge clk50)
begin
    reset_sync <= {reset_sync[0], ~reset_n | reset_dtr};
end
assign reset = reset_sync[1];

/******************************************************************************
 * LEDs
 ******************************************************************************/
// Invert UART signals as they idle at high state
assign led_uart_rx = ~uart_rx;
assign led_uart_tx = ~uart_tx;

// TODO: integrate into memory unit:
// - led_flash
// - led_usb
// - led_eth
// - led_user
assign led_flash = 1'b0;
assign led_usb   = 1'b0;
assign led_eth   = 1'b0;
assign led_user  = 1'b0;

// TODO: integrate into GPU:
// - led_gpu
assign led_gpu = 1'b0;

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
    .clk (clk50),

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
    .cpu_clk (clk50),
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
    .cpu_clk (clk50),
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
 * VRAMPX SRAM Interface V2
 * CPU writes go to SRAM via block RAM FIFO during blanking
 * GPU reads directly from SRAM via arbiter during active video
 * Arbiter runs at 100MHz for proper SRAM timing
 ******************************************************************************/

wire [16:0] vramPX_cpu_addr;
wire [7:0]  vramPX_cpu_d;
wire        vramPX_cpu_we;
wire [7:0]  vramPX_cpu_q;  // Not used in this design (write-only from CPU)

// GPU pixel interface - direct SRAM read
wire [16:0] gpu_pixel_addr;
wire [7:0]  gpu_pixel_data;

// Timing signals from FSX (25MHz domain)
wire [11:0] fsx_h_count;
wire [11:0] fsx_v_count;
wire        fsx_vsync;
wire        fsx_blank;

VRAMPXSram vrampx_sram (
    // Clocks and reset
    .clk50(clk50),
    .clk100(clk100),
    .clkPixel(clkGPU),
    .reset(reset),
    
    // CPU interface (50MHz)
    .cpu_addr(vramPX_cpu_addr),
    .cpu_data(vramPX_cpu_d),
    .cpu_we(vramPX_cpu_we),
    
    // GPU interface - direct SRAM read
    .gpu_addr(gpu_pixel_addr),
    .gpu_data(gpu_pixel_data),
    
    // GPU timing
    .blank(fsx_blank),
    .vsync(fsx_vsync),
    
    // External SRAM
    .SRAM_A(SRAM_A),
    .SRAM_DQ(SRAM_DQ),
    .SRAM_CSn(SRAM_CSn),
    .SRAM_OEn(SRAM_OEn),
    .SRAM_WEn(SRAM_WEn)
);

// CPU read from vramPX returns 0 (write-only)
assign vramPX_cpu_q = 8'd0;

/******************************************************************************
 * L1i RAM (100&50MHz)
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

// DPRAM instance
DPRAM #(
    .WIDTH(271),
    .WORDS(128),
    .ADDR_BITS(7)
) l1i_ram (
    .clk_pipe(clk50),
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
 * L1d RAM (100&50MHz)
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

// DPRAM instance
DPRAM #(
    .WIDTH(271),
    .WORDS(128),
    .ADDR_BITS(7)
) l1d_ram (
    .clk_pipe(clk50),
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
 * SDRAM Controller (100MHz)
 ******************************************************************************/
wire [20:0]     sdc_addr;
wire [255:0]    sdc_data;
wire            sdc_we;
wire            sdc_start;
wire            sdc_done;
wire [255:0]    sdc_q;
SDRAMcontroller sdc (
    // Clock and reset
    .clk(clk100),
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

/******************************************************************************
 * CacheController (100MHz)
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
wire        l1d_cache_controller_done;
wire [31:0] l1d_cache_controller_result;

wire l1_clear_cache;
wire l1_clear_cache_done;

CacheController cache_controller (
    .clk100(clk100),
    .reset(reset),

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

    // SDRAM controller interface
    .sdc_addr(sdc_addr),
    .sdc_data(sdc_data),
    .sdc_we(sdc_we),
    .sdc_start(sdc_start),
    .sdc_done(sdc_done),
    .sdc_q(sdc_q)
);

/******************************************************************************
 * FSX GPU (SRAM V2) - Direct SRAM Read Design
 ******************************************************************************/
wire frameDrawn;
FSX_SRAM fsx (
    // Clocks
    .clkPixel(clkGPU),
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

    // Pixel SRAM interface
    .pixel_sram_addr(gpu_pixel_addr),
    .pixel_sram_data(gpu_pixel_data),

    // Timing outputs
    .h_count_out(fsx_h_count),
    .v_count_out(fsx_v_count),
    .vsync_out(fsx_vsync),
    .blank_out(fsx_blank),
    
    // Parameters
    .halfRes(half_res),

    // Interrupt signal
    .frameDrawn(frameDrawn)
);

/******************************************************************************
 * Memory Unit (50MHz)
 ******************************************************************************/
// Bus
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
    .clk(clk50),
    .reset(reset),
    .uart_reset(), // No need as we have DTR available

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

    // Flash 1
    .SPI0_clk(flash1_clk),
    .SPI0_mosi(flash1_mosi),
    .SPI0_miso(flash1_miso),
    .SPI0_cs(flash1_cs),

    // Flash 2
    .SPI1_clk(flash2_clk),
    .SPI1_mosi(flash2_mosi),
    .SPI1_miso(flash2_miso),
    .SPI1_cs(flash2_cs),

    // USB 1
    .SPI2_clk(usb1_clk),
    .SPI2_mosi(usb1_mosi),
    .SPI2_miso(usb1_miso),
    .SPI2_cs(usb1_cs),

    // USB 2
    .SPI3_clk(usb2_clk),
    .SPI3_mosi(usb2_mosi),
    .SPI3_miso(usb2_miso),
    .SPI3_cs(usb2_cs),

    // Ethernet
    .SPI4_clk(eth_clk),
    .SPI4_mosi(eth_mosi),
    .SPI4_miso(eth_miso),
    .SPI4_cs(eth_cs),

    // SD Card
    .SPI5_clk(sd_clk),
    .SPI5_mosi(sd_mosi),
    .SPI5_miso(sd_miso),
    .SPI5_cs(sd_cs)
);

/******************************************************************************
 * CPU (50MHz)
 ******************************************************************************/
// Convert frameDrawn to CPU clock domain
wire frameDrawn_CPU; // Interuupt synchronized to 50MHz clock
reg frameDrawn_ff1, frameDrawn_ff2;

always @(posedge clk50)
begin
    frameDrawn_ff1 <= frameDrawn;
    frameDrawn_ff2 <= frameDrawn_ff1;
end

assign frameDrawn_CPU = frameDrawn_ff2;

B32P3 cpu (
    // Clock and reset
    .clk(clk50),
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

    // VRAMPX (SRAM)
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
    .interrupts({3'd0, frameDrawn_CPU, OST3_int, OST2_int, OST1_int, uart_irq})
);

endmodule

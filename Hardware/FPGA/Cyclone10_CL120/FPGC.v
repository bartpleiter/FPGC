module FPGC (
    // Clocks
    input wire          sys_clk_50,
    input wire          sys_clk_27,

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

    // HDMI
    output wire         HDMI_CLK_P,
    output wire         HDMI_CLK_N,
    output wire         HDMI_D0_P,
    output wire         HDMI_D0_N,
    output wire         HDMI_D1_P,
    output wire         HDMI_D1_N,
    output wire         HDMI_D2_P,
    output wire         HDMI_D2_N,

    // UART
    input wire          uart_rx,
    output wire         uart_tx,

    // SPI flash 1
    output wire         SPI0_clk,
    output wire         SPI0_mosi,
    input wire          SPI0_miso,
    output wire         SPI0_cs,

    // SPI flash 2
    output wire         SPI1_clk,
    output wire         SPI1_mosi,
    input wire          SPI1_miso,
    output wire         SPI1_cs,

    // SPI USB Host
    output wire         SPI2_clk,
    output wire         SPI2_mosi,
    input wire          SPI2_miso,
    output wire         SPI2_cs,
    input wire          SPI2_nint,

    // SPI Ethernet
    output wire         SPI4_clk,
    output wire         SPI4_mosi,
    input wire          SPI4_miso,
    output wire         SPI4_cs,
    input wire          SPI4_nint,

    // Buttons
    input wire          key1,
    input wire          key2,
    input wire          sw1,
    input wire          sw2,

    // LEDs
    output wire         led1,
    output wire         led2
);

wire boot_mode = 1'b0 ; // TODO: connect to switch when available

// SPI signals that are not yet connected to pins
wire SPI3_clk;
wire SPI3_mosi;
wire SPI3_miso = 1'b1;
wire SPI3_cs;
wire SPI3_nint = 1'b1;

wire SPI5_clk;
wire SPI5_mosi;
wire SPI5_miso = 1'b1;
wire SPI5_cs;

assign led1 = key1;
assign led2 = key2;

//---------------------------Clocks and reset---------------------------------
wire clk100;        // Memory logic
wire SDRAM_CLK;     // 100MHz Phase-shifted for SDRAM
wire clkPixel;      // GPU clock
wire clkTMDShalf;   // Half of TMDS clock for HDMI

pll1 main_pll (
    .inclk0(sys_clk_50),
    .c0(),
    .c1(clk100),
    .c2(SDRAM_CLK),
    .c3(clkPixel),
    .c4(clkTMDShalf)
);

wire reset;
wire uart_reset; // Reset from UART magic sequence
assign reset = uart_reset; // For now only UART reset
wire reset100;
assign reset100 = reset; // TODO: add synchronizers when external pin is used

//--------------------------SDRAM----------------------------
wire          SDRAM_CSn;
wire          SDRAM_WEn;
wire          SDRAM_CASn;
wire          SDRAM_RASn;
wire          SDRAM_CKE;
wire [12:0]   SDRAM_A;
wire [1:0]    SDRAM_BA;

assign SDRAM0_CLK    = SDRAM_CLK;
assign SDRAM1_CLK    = SDRAM_CLK;
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


//-----------------------SDRAM Controller(100MHz)------------------------
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
    .SDRAM_DQM({SDRAM1_UDQM, SDRAM1_LDQM, SDRAM0_UDQM, SDRAM0_LDQM}),
    .SDRAM_DQ({SDRAM1_DQ, SDRAM0_DQ})
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
    .LIST("/home/bart/repos/FPGC/Hardware/FPGA/Verilog/MemoryLists/vram32.list")
) vram32 (
    //CPU port
    .cpu_clk (clk100),
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
    .LIST("/home/bart/repos/FPGC/Hardware/FPGA/Verilog/MemoryLists/vram8.list")
) vram8 (
    // CPU port
    .cpu_clk (clk100),
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
    .LIST("/home/bart/repos/FPGC/Hardware/FPGA/Verilog/MemoryLists/vramPX.list")
) vramPX (
    // CPU port
    .cpu_clk (clk100),
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
CacheController cache_controller (
    .clk100(clk100),
    .reset(reset100), // Testing 50MHz reset to rule out some reset issues

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

//-----------------------FSX-------------------------
wire frameDrawn;
FSX fsx (
    // Clocks
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

//------------------Memory Unit (50MHz)----------------------
wire        mu_start;
wire [31:0] mu_addr;
wire [31:0] mu_data;
wire        mu_we;
wire [31:0] mu_q;
wire        mu_done;

// HW pins are defined at the top of the module

wire        uart_irq;
wire        OST1_int;
wire        OST2_int;
wire        OST3_int;

// We need to synchronize the boot_mode signal to the 50MHz clock
reg [1:0] boot_mode_sync = 2'd0;
always @(posedge clk100)
begin
    boot_mode_sync <= {boot_mode_sync[0], boot_mode};
end
wire boot_mode_50mhz = boot_mode_sync[1];

MemoryUnit memory_unit (
    .clk(clk100),
    .reset(reset100),
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

    .boot_mode(boot_mode_50mhz),

    .SPI0_clk(SPI0_clk),
    .SPI0_mosi(SPI0_mosi),
    .SPI0_miso(SPI0_miso),
    .SPI0_cs(SPI0_cs),

    .SPI1_clk(SPI1_clk),
    .SPI1_mosi(SPI1_mosi),
    .SPI1_miso(SPI1_miso),
    .SPI1_cs(SPI1_cs),

    // .SPI2_clk(SPI2_clk),
    // .SPI2_mosi(SPI2_mosi),
    // .SPI2_miso(SPI2_miso),
    // .SPI2_cs(SPI2_cs),

    // .SPI3_clk(SPI3_clk),
    // .SPI3_mosi(SPI3_mosi),
    // .SPI3_miso(SPI3_miso),
    // .SPI3_cs(SPI3_cs),

    .SPI4_clk(SPI4_clk),
    .SPI4_mosi(SPI4_mosi),
    .SPI4_miso(SPI4_miso),
    .SPI4_cs(SPI4_cs)

    // .SPI5_clk(SPI5_clk),
    // .SPI5_mosi(SPI5_mosi),
    // .SPI5_miso(SPI5_miso),
    // .SPI5_cs(SPI5_cs)
);

//-----------------------CPU-------------------------
// Convert frameDrawn to CPU clock domain
wire frameDrawn_CPU; // Interuupt synchronized to 50MHz clock
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
    .reset(reset100),

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
    .interrupts({3'd0, frameDrawn_CPU, OST3_int, OST2_int, OST1_int, uart_irq})
);

endmodule

module FPGC (
    // Clock and reset
    input wire sys_clk_50,
    input wire sys_clk_27,
    input wire key1,
    input wire key2,

    // SDRAM0
    output wire        SDRAM0_CLK,
    output wire        SDRAM0_CS_N,
    output wire        SDRAM0_WE_N,
    output wire        SDRAM0_CAS_N,
    output wire        SDRAM0_RAS_N,
    output wire        SDRAM0_CKE,
    output wire [12:0] SDRAM0_ADDR,
    output wire [1:0]  SDRAM0_BA,
    output wire        SDRAM0_LDQM,
    output wire        SDRAM0_UDQM,
    inout wire  [15:0] SDRAM0_DQ,

    // SDRAM1
    output wire        SDRAM1_CLK,
    output wire        SDRAM1_CS_N,
    output wire        SDRAM1_WE_N,
    output wire        SDRAM1_CAS_N,
    output wire        SDRAM1_RAS_N,
    output wire        SDRAM1_CKE,
    output wire [12:0] SDRAM1_ADDR,
    output wire [1:0]  SDRAM1_BA,
    output wire        SDRAM1_LDQM,
    output wire        SDRAM1_UDQM,
    inout wire  [15:0] SDRAM1_DQ,
    
    // LEDs
    output wire        led1,
    output wire        led2
);

assign led1 = 1'b0;
assign led2 = 1'b1;

// Clocks and reset
wire clk50; // CPU and main logic
wire clk100; // Memory logic
wire SDRAM_CLK; // 100MHz Phase-shifted for SDRAM
wire clk25; // GPU clock
wire clk125; // TMDS clock for HDMI

pll1 main_pll (
    .inclk0(sys_clk_50),
    .c0(clk50),
    .c1(clk100),
    .c2(SDRAM_CLK),
    .c3(clk25),
    .c4(clk125)
);

// Combine SDRAM signals
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


// SDRAM controller
wire [20:0]     cpu_addr;
wire [255:0]    cpu_data;
wire            cpu_we;
wire            cpu_start;
wire            cpu_done;
wire [255:0]    cpu_q;
SDRAMcontroller sdc (
    // Clock and reset
    .clk(clk100),
    .reset(1'b0), // For now we do not want to reset the SDRAM controller

    .cpu_addr(cpu_addr),
    .cpu_data(cpu_data),
    .cpu_we(cpu_we),
    .cpu_start(cpu_start),
    .cpu_done(cpu_done),
    .cpu_q(cpu_q),

    .SDRAM_CKE(SDRAM_CKE),
    .SDRAM_CSn(SDRAM_CSn),
    .SDRAM_WEn(SDRAM_WEn),
    .SDRAM_CASn(SDRAM_CASn),
    .SDRAM_RASn(SDRAM_RASn),
    .SDRAM_A(SDRAM_A),
    .SDRAM_BA(SDRAM_BA),
    .SDRAM_DQM({SDRAM1_UDQM, SDRAM1_LDQM, SDRAM0_UDQM, SDRAM0_LDQM}),
    .SDRAM_DQ({SDRAM1_DQ, SDRAM_DQ})
);

endmodule

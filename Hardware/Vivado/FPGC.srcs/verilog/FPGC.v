module FPGC (
    // Clocks and reset
    input wire sys_clk_p,
    input wire sys_clk_n,
    input wire sys_rstn,

    // HDMI
    output wire HDMI_CLK_P,
    output wire HDMI_CLK_N,
    output wire HDMI_D0_P,
    output wire HDMI_D0_N,
    output wire HDMI_D1_P,
    output wire HDMI_D1_N,
    output wire HDMI_D2_P,
    output wire HDMI_D2_N,
    
    // DDR3
    inout  wire [31:0] ddr3_dq,        // ddr3 data
    inout  wire [3:0]  ddr3_dqs_n,     // ddr3 dqs negative
    inout  wire [3:0]  ddr3_dqs_p,     // ddr3 dqs positive
    output wire [14:0] ddr3_addr,      // ddr3 address
    output wire [2:0]  ddr3_ba,        // ddr3 bank
    output wire        ddr3_ras_n,     // ddr3 ras_n
    output wire        ddr3_cas_n,     // ddr3 cas_n
    output wire        ddr3_we_n,      // ddr3 write enable 
    output wire        ddr3_reset_n,   // ddr3 reset
    output wire [0:0]  ddr3_ck_p,      // ddr3 clock positive
    output wire [0:0]  ddr3_ck_n,      // ddr3 clock negative
    output wire [0:0]  ddr3_cke,       // ddr3_cke
    output wire [0:0]  ddr3_cs_n,      // ddr3 negated chip select
    output wire [3:0]  ddr3_dm,        // ddr3_dm
    output wire [0:0]  ddr3_odt        // ddr3_odt
);

//---------------------------Clocks and reset---------------------------------
// CPU/Memory clocks and reset
wire reset;  // From MIG7
wire clk50_unb;  // From user MMCM
wire clk50; // Buffered version of clk50_unb
wire clk100; // From MIG7
wire clk200; // To MIG7

IBUFDS ibufclk200 (
    .O  (clk200),
    .I  (sys_clk_p),
    .IB (sys_clk_n)
);

clk_gen_cpu cpu_clockgen (
    .clk_ui_in      (clk100),
    .resetn         (sys_rstn),
    .clk_out_50     (clk50_unb),
    .locked()
);

BUFG bufgclk50 (
    .I (clk50_unb),
    .O (clk50)
);

// GPU clocks
wire clkPixel;
wire clkTMDShalf;
wire clkPixel_unb;
wire clkTMDShalf_unb;

clk_generator clk_gen (
    .clk_sys_in     (clk200),
    .resetn         (sys_rstn),
    .clk_gpu        (clkPixel_unb),  // 25 MHz
    .clk_tmds_half  (clkTMDShalf_unb), // 125 MHz
    .locked()
);

BUFG bufgclkPixel (
    .I(clkPixel_unb),
    .O(clkPixel)
);

BUFG bufgclkTMDShalf (
    .I(clkTMDShalf_unb),
    .O(clkTMDShalf)
);

//-----------------------ROM-------------------------
// ROM I/O
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
    .clk (clk50),

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
    .cpu_clk (clk50),
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
    .cpu_clk (clk50),
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
    .cpu_clk (clk50),
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


//-----------------------MIG7 (200/100MHz)-------------------------

// MIG7 I/O signals
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

mig_7series_0 mig7_ddr3
(
// Memory interface ports
.ddr3_addr                      (ddr3_addr              ),
.ddr3_ba                        (ddr3_ba                ),
.ddr3_cas_n                     (ddr3_cas_n             ),
.ddr3_ck_n                      (ddr3_ck_n              ),
.ddr3_ck_p                      (ddr3_ck_p              ),
.ddr3_cke                       (ddr3_cke               ),
.ddr3_ras_n                     (ddr3_ras_n             ),
.ddr3_we_n                      (ddr3_we_n              ),
.ddr3_dq                        (ddr3_dq                ),
.ddr3_dqs_n                     (ddr3_dqs_n             ),
.ddr3_dqs_p                     (ddr3_dqs_p             ),
.ddr3_reset_n                   (ddr3_reset_n           ),
.ddr3_cs_n                      (ddr3_cs_n              ),
.ddr3_dm                        (ddr3_dm                ),
.ddr3_odt                       (ddr3_odt               ),

// Application interface ports
.app_addr                       (mig7_app_addr               ),
.app_cmd                        (mig7_app_cmd                ),
.app_en                         (mig7_app_en                 ),
.app_wdf_data                   (mig7_app_wdf_data           ),
.app_wdf_end                    (mig7_app_wdf_end            ),
.app_wdf_wren                   (mig7_app_wdf_wren           ),
.app_rd_data                    (mig7_app_rd_data            ),
.app_rd_data_end                (mig7_app_rd_data_end        ),
.app_rd_data_valid              (mig7_app_rd_data_valid      ),
.app_rdy                        (mig7_app_rdy                ),
.app_wdf_rdy                    (mig7_app_wdf_rdy            ),
.app_sr_req                     (mig7_app_sr_req             ),
.app_ref_req                    (mig7_app_ref_req            ),
.app_zq_req                     (mig7_app_zq_req             ),
.app_sr_active                  (mig7_app_sr_active          ),
.app_ref_ack                    (mig7_app_ref_ack            ),
.app_zq_ack                     (mig7_app_zq_ack             ),
.app_wdf_mask                   (mig7_app_wdf_mask           ),

.init_calib_complete            (mig7_init_calib_complete),

// Clock ports
.ui_clk                         (clk100),
.ui_clk_sync_rst                (reset),   // Active high reset

.sys_clk_i                      (clk200),  
.sys_rst                        (sys_rstn) // Active low reset
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

// Instantiate CacheController
CacheController #(
    .ADDR_WIDTH(29),
    .DATA_WIDTH(256),
    .MASK_WIDTH(32)
) cache_controller (
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

//-----------------------FSX-------------------------
wire frameDrawn;
FSX fsx (
    // Clocks
    .clkPixel(clkPixel),
    .clkTMDShalf(clkTMDShalf),
    .reset(reset),

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

    .l1_clear_cache(l1_clear_cache)
);

endmodule

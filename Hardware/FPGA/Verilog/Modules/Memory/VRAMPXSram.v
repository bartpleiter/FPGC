/*
 * VRAMPXSram
 * Top-level module that implements the pixel framebuffer using external SRAM
 * 
 * This module wraps:
 * - SRAMWriteFIFO: Buffers CPU writes
 * - SRAMReadFIFO: Dual-clock FIFO for GPU reads  
 * - SRAMArbiter: Manages SRAM access between CPU and GPU
 * 
 * Interfaces:
 * - CPU: Write-only access to pixel framebuffer (50MHz)
 * - GPU: Read-only access via modified PixelEngine (25MHz)
 * - SRAM: External IS61LV5128AL interface
 */
module VRAMPXSram (
    // Clocks and reset
    input  wire         clk,           // 50MHz system clock
    input  wire         clkPixel,      // 25MHz GPU clock
    input  wire         reset,
    
    // CPU interface (50MHz domain)
    input  wire [16:0]  cpu_addr,      // 17-bit pixel address (0-76799)
    input  wire [7:0]   cpu_data,      // 8-bit pixel data (R3G3B2)
    input  wire         cpu_we,        // Write enable
    
    // GPU timing signals (directly from TimingGenerator, 25MHz domain)
    input  wire         vsync,
    input  wire [11:0]  h_count,
    input  wire [11:0]  v_count,
    input  wire         halfRes,
    
    // GPU pixel output interface (25MHz domain)
    output wire [7:0]   gpu_pixel_data,
    output wire         gpu_fifo_empty,
    input  wire         gpu_fifo_rd_en,
    
    // External SRAM interface
    output wire [18:0]  SRAM_A,
    inout  wire [7:0]   SRAM_DQ,
    output wire         SRAM_CSn,
    output wire         SRAM_OEn,
    output wire         SRAM_WEn
);

//=============================================================================
// Clock Domain Crossing: vsync sync to 50MHz
//=============================================================================
reg vsync_sync1 = 1'b0;
reg vsync_sync2 = 1'b0;

always @(posedge clk) begin
    vsync_sync1 <= vsync;
    vsync_sync2 <= vsync_sync1;
end

wire vsync_50mhz = vsync_sync2;

//=============================================================================
// CPU Write FIFO (50MHz domain)
//=============================================================================
wire [16:0] cpu_fifo_addr;
wire [7:0]  cpu_fifo_data;
wire        cpu_fifo_empty;
wire        cpu_fifo_full;
wire        cpu_fifo_rd_en;

SRAMWriteFIFO #(
    .DEPTH(16),
    .ADDR_WIDTH(4)
) cpu_write_fifo (
    .clk(clk),
    .reset(reset),
    
    // CPU write port
    .wr_addr(cpu_addr),
    .wr_data(cpu_data),
    .wr_en(cpu_we && !cpu_fifo_full),
    .full(cpu_fifo_full),
    
    // Arbiter read port
    .rd_addr(cpu_fifo_addr),
    .rd_data(cpu_fifo_data),
    .empty(cpu_fifo_empty),
    .rd_en(cpu_fifo_rd_en)
);

//=============================================================================
// GPU Read FIFO (50MHz write, 25MHz read)
// Increased size to 128 entries for better buffering during CPU write contention
//=============================================================================
wire [7:0]  gpu_fifo_wr_data;
wire        gpu_fifo_wr_en;
wire        gpu_fifo_full;
wire [7:0]  gpu_fifo_wr_level;
wire [7:0]  gpu_fifo_rd_level;

SRAMReadFIFO #(
    .DEPTH(128),
    .ADDR_WIDTH(7)
) gpu_read_fifo (
    // Write side (50MHz)
    .wr_clk(clk),
    .wr_reset(reset),
    .wr_data(gpu_fifo_wr_data),
    .wr_en(gpu_fifo_wr_en),
    .wr_full(gpu_fifo_full),
    .wr_level(gpu_fifo_wr_level),
    
    // Read side (25MHz)
    .rd_clk(clkPixel),
    .rd_reset(reset),
    .rd_data(gpu_pixel_data),
    .rd_en(gpu_fifo_rd_en),
    .rd_empty(gpu_fifo_empty),
    .rd_level(gpu_fifo_rd_level)
);

//=============================================================================
// SRAM Arbiter (50MHz domain)
//=============================================================================
wire [18:0] sram_addr_int;
wire [7:0]  sram_dq_out;
wire [7:0]  sram_dq_in;
wire        sram_we_n_int;
wire        sram_oe_n_int;
wire        sram_cs_n_int;

SRAMArbiter arbiter (
    .clk(clk),
    .reset(reset),
    
    // GPU timing (50MHz synced)
    .vsync(vsync_50mhz),
    .h_count(h_count),
    .v_count(v_count),
    .halfRes(halfRes),
    
    // CPU Write FIFO interface
    .cpu_wr_addr(cpu_fifo_addr),
    .cpu_wr_data(cpu_fifo_data),
    .cpu_fifo_empty(cpu_fifo_empty),
    .cpu_fifo_rd_en(cpu_fifo_rd_en),
    
    // GPU Read FIFO interface
    .gpu_rd_data(gpu_fifo_wr_data),
    .gpu_fifo_wr_en(gpu_fifo_wr_en),
    .gpu_fifo_full(gpu_fifo_full),
    
    // SRAM interface
    .sram_addr(sram_addr_int),
    .sram_dq_out(sram_dq_out),
    .sram_dq_in(sram_dq_in),
    .sram_we_n(sram_we_n_int),
    .sram_oe_n(sram_oe_n_int),
    .sram_cs_n(sram_cs_n_int)
);

//=============================================================================
// SRAM I/O
//=============================================================================
assign SRAM_A = sram_addr_int;
assign SRAM_CSn = sram_cs_n_int;
assign SRAM_OEn = sram_oe_n_int;
assign SRAM_WEn = sram_we_n_int;

// Bidirectional data bus
assign SRAM_DQ = (~sram_we_n_int) ? sram_dq_out : 8'bz;
assign sram_dq_in = SRAM_DQ;

endmodule

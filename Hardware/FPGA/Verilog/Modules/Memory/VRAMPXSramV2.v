/*
 * VRAMPXSramV2
 * Simplified top-level module for pixel framebuffer using external SRAM
 * 
 * Interfaces:
 * - CPU: Write-only access to pixel framebuffer (50MHz)
 * - GPU: Direct read access through arbiter (25MHz, synced internally)
 * - SRAM: External IS61LV5128AL interface
 */
module VRAMPXSramV2 (
    // Clocks and reset
    input  wire         clk50,         // 50MHz CPU clock
    input  wire         clk100,        // 100MHz arbiter clock
    input  wire         clkPixel,      // 25MHz GPU clock
    input  wire         reset,
    
    // CPU interface (50MHz domain)
    input  wire [16:0]  cpu_addr,      // 17-bit pixel address (0-76799)
    input  wire [7:0]   cpu_data,      // 8-bit pixel data (R3G3B2)
    input  wire         cpu_we,        // Write enable
    
    // GPU interface (25MHz domain)
    input  wire [16:0]  gpu_addr,      // Requested pixel address
    output wire [7:0]   gpu_data,      // Pixel data output
    
    // GPU timing signals (25MHz domain)
    input  wire         blank,         // Active during blanking
    input  wire         vsync,         // For debug/monitoring
    
    // External SRAM interface
    output wire [18:0]  SRAM_A,
    inout  wire [7:0]   SRAM_DQ,
    output wire         SRAM_CSn,
    output wire         SRAM_OEn,
    output wire         SRAM_WEn
);

//=============================================================================
// GPU signals to 100MHz domain
// Since clocks are phase-aligned from same PLL, we can use direct connection
// for blank (which is stable for many cycles) and only register the address
//=============================================================================
reg [16:0] gpu_addr_sync = 17'd0;

always @(posedge clk100) begin
    gpu_addr_sync <= gpu_addr;
end

// Blank and vsync are stable for many cycles, direct connection is safe
wire blank_sync = blank;
wire vsync_sync = vsync;

//=============================================================================
// GPU data from arbiter to PixelEngine
// The arbiter already registers the SRAM data, so we can pass it directly
// The 25MHz GPU clock will sample stable data
//=============================================================================
wire [7:0] gpu_data_from_arbiter;

// Direct passthrough - arbiter output is already registered at 100MHz
// GPU will sample on its 25MHz clock edge (phase-aligned with 100MHz)
assign gpu_data = gpu_data_from_arbiter;

//=============================================================================
// CPU Write FIFO (50MHz write, 100MHz read)
// Using block RAM-based FIFO for larger capacity
//=============================================================================
wire [16:0] cpu_fifo_addr;
wire [7:0]  cpu_fifo_data;
wire        cpu_fifo_empty;
wire        cpu_fifo_full;
wire        cpu_fifo_rd_en;

SRAMWriteFIFOBlock #(
    .DEPTH(512),
    .ADDR_WIDTH(9)
) cpu_write_fifo (
    // Write side (50MHz)
    .wr_clk(clk50),
    .wr_reset(reset),
    .wr_addr(cpu_addr),
    .wr_data(cpu_data),
    .wr_en(cpu_we && !cpu_fifo_full),
    .wr_full(cpu_fifo_full),
    
    // Read side (100MHz)
    .rd_clk(clk100),
    .rd_reset(reset),
    .rd_addr(cpu_fifo_addr),
    .rd_data(cpu_fifo_data),
    .rd_empty(cpu_fifo_empty),
    .rd_en(cpu_fifo_rd_en)
);

//=============================================================================
// SRAM Arbiter (100MHz domain)
//=============================================================================
wire [18:0] sram_addr_int;
wire [7:0]  sram_dq_out;
wire [7:0]  sram_dq_in;
wire        sram_we_n_int;
wire        sram_oe_n_int;
wire        sram_cs_n_int;

SRAMArbiterV2 arbiter (
    .clk100(clk100),
    .reset(reset),
    
    // GPU interface
    .gpu_addr(gpu_addr_sync),
    .gpu_data(gpu_data_from_arbiter),
    
    // GPU timing
    .blank(blank_sync),
    .vsync(vsync_sync),
    
    // CPU Write FIFO interface
    .cpu_wr_addr(cpu_fifo_addr),
    .cpu_wr_data(cpu_fifo_data),
    .cpu_fifo_empty(cpu_fifo_empty),
    .cpu_fifo_rd_en(cpu_fifo_rd_en),
    
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

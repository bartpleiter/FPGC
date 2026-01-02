/*
 * VRAMPXSram
 * Top-level module for pixel framebuffer using external SRAM
 * 
 * Interfaces:
 * - CPU: Write-only access to pixel framebuffer (100MHz)
 * - GPU: Direct read access through arbiter (25MHz, synced internally)
 * - SRAM: External IS61LV5128AL interface
 */
module VRAMPXSram (
    // Clocks and reset
    input  wire         clk100,        // 100MHz CPU/arbiter clock
    input  wire         clkPixel,      // 25MHz GPU clock
    input  wire         reset,
    
    // CPU interface (50MHz domain)
    input  wire [16:0]  cpu_addr,      // 17-bit pixel address (0-76799)
    input  wire [7:0]   cpu_data,      // 8-bit pixel data (R3G3B2)
    input  wire         cpu_we,        // Write enable
    
    // GPU interface (25MHz domain)
    input  wire [16:0]  gpu_addr,      // Requested pixel address
    output wire [7:0]   gpu_data,      // Pixel data output
    input  wire         using_line_buffer, // High when GPU uses line buffer (SRAM free)
    
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

// Sync using_line_buffer to 100MHz domain
reg using_line_buffer_sync = 1'b0;
always @(posedge clk100) begin
    using_line_buffer_sync <= using_line_buffer;
end

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
// CPU Write FIFO (100MHz, synchronous)
// Buffers CPU writes for processing during blanking or line buffer periods
//=============================================================================
wire [24:0] cpu_fifo_data_out;
wire [16:0] cpu_fifo_addr = cpu_fifo_data_out[24:8];
wire [7:0]  cpu_fifo_data = cpu_fifo_data_out[7:0];
wire        cpu_fifo_empty;
wire        cpu_fifo_full;
wire        cpu_fifo_rd_en;

SyncFIFO #(
    .DATA_WIDTH(25),    // 17-bit address + 8-bit data
    .ADDR_WIDTH(9),     // 512 entries
    .DEPTH(512)
) cpu_write_fifo (
    .clk(clk100),
    .reset(reset),

    // Write side (100MHz)
    .wr_data({cpu_addr, cpu_data}),
    .wr_en(cpu_we && !cpu_fifo_full),
    .wr_full(cpu_fifo_full),
    
    // Read side (100MHz)
    .rd_data(cpu_fifo_data_out),
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

SRAMArbiter arbiter (
    .clk100(clk100),
    .reset(reset),
    
    // GPU interface
    .gpu_addr(gpu_addr_sync),
    .gpu_data(gpu_data_from_arbiter),
    
    // GPU timing
    .blank(blank_sync),
    .vsync(vsync_sync),
    .using_line_buffer(using_line_buffer_sync),
    
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

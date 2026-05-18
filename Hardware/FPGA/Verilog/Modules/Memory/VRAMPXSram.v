/*
 * VRAMPXSram
 * Top-level module for pixel framebuffer using external SRAM
 * 
 * Simplified for SPI display: single clock domain (100 MHz),
 * no line buffer, no VGA timing dependency.
 * 
 * Interfaces:
 * - CPU: Write-only access to pixel framebuffer (100MHz via FIFO)
 * - Display: Read access through arbiter (100MHz, on demand)
 * - SRAM: External IS61LV5128AL interface
 */
module VRAMPXSram (
    // Clock and reset
    input  wire         clk100,        // 100MHz system clock
    input  wire         reset,
    
    // CPU write interface (100MHz)
    input  wire [16:0]  cpu_addr,      // 17-bit pixel address (0-76799)
    input  wire [7:0]   cpu_data,      // 8-bit pixel data
    input  wire         cpu_we,        // Write enable
    
    // Display read interface (100MHz, from SPIDisplayController)
    input  wire [16:0]  gpu_addr,      // Requested pixel address
    output wire [7:0]   gpu_data,      // Pixel data output
    output wire         gpu_data_valid, // HIGH when gpu_data is valid for gpu_addr
    input  wire         display_read,  // Asserted when display needs a read
    
    // CPU backpressure
    output wire         cpu_fifo_full, // High when write FIFO is full (stall CPU)

    // External SRAM interface
    output wire [18:0]  SRAM_A,
    inout  wire [7:0]   SRAM_DQ,
    output wire         SRAM_CSn,
    output wire         SRAM_OEn,
    output wire         SRAM_WEn
);

// ---- CPU Write FIFO ----
wire [24:0] cpu_fifo_data_out;
wire [16:0] cpu_fifo_addr = cpu_fifo_data_out[24:8];
wire [7:0]  cpu_fifo_data = cpu_fifo_data_out[7:0];
wire        cpu_fifo_empty;
wire        cpu_fifo_full_int;
wire        cpu_fifo_rd_en;

assign cpu_fifo_full = cpu_fifo_full_int;

SyncFIFO #(
    .DATA_WIDTH(25),    // 17-bit address + 8-bit data
    .ADDR_WIDTH(10),    // 1024 entries
    .DEPTH(1024)
) cpu_write_fifo (
    .clk(clk100),
    .reset(reset),

    // Write side (100MHz)
    .wr_data({cpu_addr, cpu_data}),
    .wr_en(cpu_we && !cpu_fifo_full_int),
    .wr_full(cpu_fifo_full_int),
    
    // Read side (100MHz)
    .rd_data(cpu_fifo_data_out),
    .rd_empty(cpu_fifo_empty),
    .rd_en(cpu_fifo_rd_en)
);

// ---- SRAM Arbiter ----
wire [18:0] sram_addr_int;
wire [7:0]  sram_dq_out;
wire [7:0]  sram_dq_in;
wire        sram_we_n_int;
wire        sram_oe_n_int;
wire        sram_cs_n_int;

SRAMArbiter arbiter (
    .clk100(clk100),
    .reset(reset),
    
    // Display read interface
    .gpu_addr(gpu_addr),
    .gpu_data(gpu_data),
    .gpu_data_valid(gpu_data_valid),
    .display_read(display_read),
    
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

// ---- SRAM I/O ----
assign SRAM_A = sram_addr_int;
assign SRAM_CSn = sram_cs_n_int;
assign SRAM_OEn = sram_oe_n_int;
assign SRAM_WEn = sram_we_n_int;

// Bidirectional data bus
assign SRAM_DQ = (~sram_we_n_int) ? sram_dq_out : 8'bz;
assign sram_dq_in = SRAM_DQ;

endmodule

/*
 * SRAMWriteFIFOBlock
 * Synchronous FIFO using block RAM for buffering CPU writes to external SRAM
 * Designed to infer into Altera M9K block RAM
 * 
 * Key features:
 * - 512 entries deep (9-bit address = full M9K utilization)
 * - 25-bit wide (17-bit address + 8-bit data)
 * - Synchronous read and write for block RAM inference
 * - No asynchronous logic to ensure proper block RAM inference
 * 
 * Clock domains:
 * - Write side: 50MHz (CPU domain)
 * - Read side: 100MHz (Arbiter domain)
 * 
 * Since 100MHz is exactly 2× 50MHz and generated from the same PLL,
 * clock domain crossing is trivial without need for synchronizers.
 */
module SRAMWriteFIFOBlock #(
    parameter DEPTH = 512,
    parameter ADDR_WIDTH = 9   // log2(512) = 9
) (
    // Write port (50MHz CPU domain)
    input  wire         wr_clk,
    input  wire         wr_reset,
    input  wire [16:0]  wr_addr,     // 17-bit pixel address
    input  wire [7:0]   wr_data,     // 8-bit pixel data
    input  wire         wr_en,
    output wire         wr_full,
    
    // Read port (100MHz Arbiter domain)
    input  wire         rd_clk,
    input  wire         rd_reset,
    output wire [16:0]  rd_addr,
    output wire [7:0]   rd_data,
    output wire         rd_empty,
    input  wire         rd_en
);

// FIFO storage: 25 bits per entry (17-bit addr + 8-bit data)
// Using registered outputs for proper block RAM inference
(* ramstyle = "M9K" *) reg [24:0] fifo_mem [0:DEPTH-1];

// Pointers - extra bit for full/empty detection
// Write pointer in 50MHz domain
reg [ADDR_WIDTH:0] wr_ptr = 0;

// Read pointer in 100MHz domain
reg [ADDR_WIDTH:0] rd_ptr = 0;

// Synchronized read pointer to write domain (for full detection)
// Since clocks are phase-aligned, single register is sufficient
reg [ADDR_WIDTH:0] rd_ptr_sync = 0;

// Synchronized write pointer to read domain (for empty detection)
reg [ADDR_WIDTH:0] wr_ptr_sync = 0;

// Output registers for block RAM inference
reg [24:0] rd_data_reg = 25'd0;
assign rd_addr = rd_data_reg[24:8];
assign rd_data = rd_data_reg[7:0];

// Full flag: compare write pointer with synchronized read pointer
wire [ADDR_WIDTH:0] wr_level = wr_ptr - rd_ptr_sync;
assign wr_full = (wr_level >= DEPTH);

// Empty flag: compare synchronized write pointer with read pointer
assign rd_empty = (wr_ptr_sync == rd_ptr);

// Write logic (50MHz domain)
always @(posedge wr_clk) begin
    if (wr_reset) begin
        wr_ptr <= 0;
        rd_ptr_sync <= 0;
    end else begin
        // Synchronize read pointer (single register since clocks are aligned)
        rd_ptr_sync <= rd_ptr;
        
        // Write operation
        if (wr_en && !wr_full) begin
            fifo_mem[wr_ptr[ADDR_WIDTH-1:0]] <= {wr_addr, wr_data};
            wr_ptr <= wr_ptr + 1;
        end
    end
end

// Read logic (100MHz domain)
always @(posedge rd_clk) begin
    if (rd_reset) begin
        rd_ptr <= 0;
        wr_ptr_sync <= 0;
        rd_data_reg <= 25'd0;
    end else begin
        // Synchronize write pointer (single register since clocks are aligned)
        wr_ptr_sync <= wr_ptr;
        
        // Read operation - registered output for block RAM
        if (rd_en && !rd_empty) begin
            rd_ptr <= rd_ptr + 1;
        end
        
        // Always update output register from memory (for block RAM behavior)
        // Read the current location (combinatorial would break block RAM inference)
        rd_data_reg <= fifo_mem[rd_ptr[ADDR_WIDTH-1:0]];
    end
end

// Initialize memory for simulation
integer i;
initial begin
    for (i = 0; i < DEPTH; i = i + 1) begin
        fifo_mem[i] = 25'd0;
    end
end

endmodule

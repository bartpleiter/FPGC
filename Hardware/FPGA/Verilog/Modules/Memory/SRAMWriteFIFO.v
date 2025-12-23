/*
 * SRAMWriteFIFO
 * Simple synchronous FIFO for buffering CPU writes to external SRAM
 * Single clock domain (50MHz)
 */
module SRAMWriteFIFO #(
    parameter DEPTH = 16,
    parameter ADDR_WIDTH = 4   // log2(DEPTH)
) (
    input  wire         clk,
    input  wire         reset,
    
    // Write port (CPU side)
    input  wire [16:0]  wr_addr,     // 17-bit pixel address
    input  wire [7:0]   wr_data,     // 8-bit pixel data
    input  wire         wr_en,
    output wire         full,
    
    // Read port (Arbiter side)
    output wire [16:0]  rd_addr,
    output wire [7:0]   rd_data,
    output wire         empty,
    input  wire         rd_en
);

// FIFO storage: 25 bits per entry (17-bit addr + 8-bit data)
reg [24:0] fifo_mem [0:DEPTH-1];

// Pointers
reg [ADDR_WIDTH:0] wr_ptr = 0;  // Extra bit for full/empty detection
reg [ADDR_WIDTH:0] rd_ptr = 0;

// Full and empty flags
wire [ADDR_WIDTH:0] ptr_diff = wr_ptr - rd_ptr;
assign full  = (ptr_diff == DEPTH);
assign empty = (wr_ptr == rd_ptr);

// Read data (combinatorial)
wire [ADDR_WIDTH-1:0] rd_idx = rd_ptr[ADDR_WIDTH-1:0];
assign rd_addr = fifo_mem[rd_idx][24:8];
assign rd_data = fifo_mem[rd_idx][7:0];

// Write logic
always @(posedge clk) begin
    if (reset) begin
        wr_ptr <= 0;
    end else if (wr_en && !full) begin
        fifo_mem[wr_ptr[ADDR_WIDTH-1:0]] <= {wr_addr, wr_data};
        wr_ptr <= wr_ptr + 1;
    end
end

// Read logic
always @(posedge clk) begin
    if (reset) begin
        rd_ptr <= 0;
    end else if (rd_en && !empty) begin
        rd_ptr <= rd_ptr + 1;
    end
end

endmodule

/*
 * SRAMReadFIFO
 * Asynchronous (dual-clock) FIFO for transferring pixel data from SRAM to GPU
 * Write clock: 50MHz (SRAM/arbiter domain)
 * Read clock: 25MHz (GPU domain)
 * 
 * Uses gray code pointers for safe clock domain crossing
 */
module SRAMReadFIFO #(
    parameter DEPTH = 32,
    parameter ADDR_WIDTH = 5   // log2(DEPTH)
) (
    // Write port (50MHz domain)
    input  wire         wr_clk,
    input  wire         wr_reset,
    input  wire [7:0]   wr_data,
    input  wire         wr_en,
    output wire         wr_full,
    output wire [ADDR_WIDTH:0] wr_level,  // Fill level from write side perspective
    
    // Read port (25MHz domain)
    input  wire         rd_clk,
    input  wire         rd_reset,
    output wire [7:0]   rd_data,
    input  wire         rd_en,
    output wire         rd_empty,
    output wire [ADDR_WIDTH:0] rd_level   // Fill level from read side perspective
);

// FIFO memory - initialized to 0 for simulation
reg [7:0] fifo_mem [0:DEPTH-1];
integer i;
initial begin
    for (i = 0; i < DEPTH; i = i + 1) begin
        fifo_mem[i] = 8'd0;
    end
end

// Write domain signals
reg [ADDR_WIDTH:0] wr_ptr_bin = 0;          // Binary write pointer
reg [ADDR_WIDTH:0] wr_ptr_gray = 0;         // Gray code write pointer
reg [ADDR_WIDTH:0] rd_ptr_gray_sync1 = 0;   // Synced gray read pointer (stage 1)
reg [ADDR_WIDTH:0] rd_ptr_gray_sync2 = 0;   // Synced gray read pointer (stage 2)

// Read domain signals
reg [ADDR_WIDTH:0] rd_ptr_bin = 0;          // Binary read pointer
reg [ADDR_WIDTH:0] rd_ptr_gray = 0;         // Gray code read pointer
reg [ADDR_WIDTH:0] wr_ptr_gray_sync1 = 0;   // Synced gray write pointer (stage 1)
reg [ADDR_WIDTH:0] wr_ptr_gray_sync2 = 0;   // Synced gray write pointer (stage 2)

// Binary to gray conversion
function [ADDR_WIDTH:0] bin2gray;
    input [ADDR_WIDTH:0] bin;
    begin
        bin2gray = bin ^ (bin >> 1);
    end
endfunction

// Gray to binary conversion
function [ADDR_WIDTH:0] gray2bin;
    input [ADDR_WIDTH:0] gray;
    integer i;
    begin
        gray2bin[ADDR_WIDTH] = gray[ADDR_WIDTH];
        for (i = ADDR_WIDTH - 1; i >= 0; i = i - 1) begin
            gray2bin[i] = gray2bin[i+1] ^ gray[i];
        end
    end
endfunction

// Write domain logic
wire [ADDR_WIDTH:0] rd_ptr_bin_sync = gray2bin(rd_ptr_gray_sync2);
wire [ADDR_WIDTH:0] wr_level_calc = wr_ptr_bin - rd_ptr_bin_sync;
assign wr_full = (wr_level_calc == DEPTH);
assign wr_level = wr_level_calc;

always @(posedge wr_clk) begin
    if (wr_reset) begin
        wr_ptr_bin <= 0;
        wr_ptr_gray <= 0;
        rd_ptr_gray_sync1 <= 0;
        rd_ptr_gray_sync2 <= 0;
    end else begin
        // Synchronize read pointer to write domain
        rd_ptr_gray_sync1 <= rd_ptr_gray;
        rd_ptr_gray_sync2 <= rd_ptr_gray_sync1;
        
        // Write operation
        if (wr_en && !wr_full) begin
            fifo_mem[wr_ptr_bin[ADDR_WIDTH-1:0]] <= wr_data;
            wr_ptr_bin <= wr_ptr_bin + 1;
            wr_ptr_gray <= bin2gray(wr_ptr_bin + 1);
        end
    end
end

// Read domain logic
wire [ADDR_WIDTH:0] wr_ptr_bin_sync = gray2bin(wr_ptr_gray_sync2);
wire [ADDR_WIDTH:0] rd_level_calc = wr_ptr_bin_sync - rd_ptr_bin;
assign rd_empty = (rd_ptr_gray == wr_ptr_gray_sync2);
assign rd_level = rd_level_calc;

// Combinatorial read data
assign rd_data = fifo_mem[rd_ptr_bin[ADDR_WIDTH-1:0]];

always @(posedge rd_clk) begin
    if (rd_reset) begin
        rd_ptr_bin <= 0;
        rd_ptr_gray <= 0;
        wr_ptr_gray_sync1 <= 0;
        wr_ptr_gray_sync2 <= 0;
    end else begin
        // Synchronize write pointer to read domain
        wr_ptr_gray_sync1 <= wr_ptr_gray;
        wr_ptr_gray_sync2 <= wr_ptr_gray_sync1;
        
        // Read operation
        if (rd_en && !rd_empty) begin
            rd_ptr_bin <= rd_ptr_bin + 1;
            rd_ptr_gray <= bin2gray(rd_ptr_bin + 1);
        end
    end
end

endmodule

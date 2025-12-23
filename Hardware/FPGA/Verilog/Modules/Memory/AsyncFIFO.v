/*
 * AsyncFIFO
 * Proper asynchronous FIFO with Gray-coded pointers for robust CDC
 * 
 * Key features:
 * - Gray-coded pointers for safe CDC
 * - 2-FF synchronizers for metastability protection
 * - Designed for M9K block RAM inference
 * - Conservative full/empty detection (may indicate full/empty slightly early)
 */
module AsyncFIFO #(
    parameter DATA_WIDTH = 25,      // 17-bit address + 8-bit data
    parameter ADDR_WIDTH = 9,       // log2(512) = 9
    parameter DEPTH = 512
) (
    // Write port
    input  wire                   wr_clk,
    input  wire                   wr_reset,
    input  wire [DATA_WIDTH-1:0]  wr_data,
    input  wire                   wr_en,
    output wire                   wr_full,
    
    // Read port
    input  wire                   rd_clk,
    input  wire                   rd_reset,
    output wire [DATA_WIDTH-1:0]  rd_data,
    input  wire                   rd_en,
    output wire                   rd_empty
);

//=============================================================================
// Memory - using block RAM
//=============================================================================
(* ramstyle = "M9K" *) reg [DATA_WIDTH-1:0] mem [0:DEPTH-1];

//=============================================================================
// Write pointer (binary and Gray code)
//=============================================================================
reg [ADDR_WIDTH:0] wr_ptr_bin = 0;
reg [ADDR_WIDTH:0] wr_ptr_gray = 0;

// Binary to Gray conversion: gray = bin ^ (bin >> 1)
wire [ADDR_WIDTH:0] wr_ptr_bin_next = wr_ptr_bin + 1;
wire [ADDR_WIDTH:0] wr_ptr_gray_next = wr_ptr_bin_next ^ (wr_ptr_bin_next >> 1);

//=============================================================================
// Read pointer (binary and Gray code)
//=============================================================================
reg [ADDR_WIDTH:0] rd_ptr_bin = 0;
reg [ADDR_WIDTH:0] rd_ptr_gray = 0;

wire [ADDR_WIDTH:0] rd_ptr_bin_next = rd_ptr_bin + 1;
wire [ADDR_WIDTH:0] rd_ptr_gray_next = rd_ptr_bin_next ^ (rd_ptr_bin_next >> 1);

//=============================================================================
// Synchronizers (2-FF for metastability)
//=============================================================================
// Write pointer Gray code synchronized to read domain
reg [ADDR_WIDTH:0] wr_ptr_gray_rd1 = 0;
reg [ADDR_WIDTH:0] wr_ptr_gray_rd2 = 0;

// Read pointer Gray code synchronized to write domain
reg [ADDR_WIDTH:0] rd_ptr_gray_wr1 = 0;
reg [ADDR_WIDTH:0] rd_ptr_gray_wr2 = 0;

//=============================================================================
// Full detection (in write domain)
// Full when: wr_ptr_gray == {~rd_ptr_gray[N:N-1], rd_ptr_gray[N-2:0]}
// i.e., MSB and MSB-1 are inverted, rest are the same
//=============================================================================
wire wr_full_val = (wr_ptr_gray == {~rd_ptr_gray_wr2[ADDR_WIDTH:ADDR_WIDTH-1],
                                     rd_ptr_gray_wr2[ADDR_WIDTH-2:0]});
assign wr_full = wr_full_val;

//=============================================================================
// Empty detection (in read domain)
// Empty when: rd_ptr_gray == wr_ptr_gray_rd2
//=============================================================================
wire rd_empty_val = (rd_ptr_gray == wr_ptr_gray_rd2);
assign rd_empty = rd_empty_val;

//=============================================================================
// Write logic (wr_clk domain)
//=============================================================================
always @(posedge wr_clk) begin
    if (wr_reset) begin
        wr_ptr_bin <= 0;
        wr_ptr_gray <= 0;
        rd_ptr_gray_wr1 <= 0;
        rd_ptr_gray_wr2 <= 0;
    end else begin
        // 2-FF synchronizer for read pointer
        rd_ptr_gray_wr1 <= rd_ptr_gray;
        rd_ptr_gray_wr2 <= rd_ptr_gray_wr1;
        
        // Write operation
        if (wr_en && !wr_full_val) begin
            mem[wr_ptr_bin[ADDR_WIDTH-1:0]] <= wr_data;
            wr_ptr_bin <= wr_ptr_bin_next;
            wr_ptr_gray <= wr_ptr_gray_next;
        end
    end
end

//=============================================================================
// Read logic (rd_clk domain)
//=============================================================================
// Registered output for block RAM inference
reg [DATA_WIDTH-1:0] rd_data_reg = 0;
assign rd_data = rd_data_reg;

always @(posedge rd_clk) begin
    if (rd_reset) begin
        rd_ptr_bin <= 0;
        rd_ptr_gray <= 0;
        wr_ptr_gray_rd1 <= 0;
        wr_ptr_gray_rd2 <= 0;
        rd_data_reg <= 0;
    end else begin
        // 2-FF synchronizer for write pointer
        wr_ptr_gray_rd1 <= wr_ptr_gray;
        wr_ptr_gray_rd2 <= wr_ptr_gray_rd1;
        
        // Always read from memory (registered for block RAM)
        rd_data_reg <= mem[rd_ptr_bin[ADDR_WIDTH-1:0]];
        
        // Update read pointer after read
        if (rd_en && !rd_empty_val) begin
            rd_ptr_bin <= rd_ptr_bin_next;
            rd_ptr_gray <= rd_ptr_gray_next;
        end
    end
end

//=============================================================================
// Simulation initialization
//=============================================================================
integer i;
initial begin
    for (i = 0; i < DEPTH; i = i + 1) begin
        mem[i] = 0;
    end
end

endmodule

/*
 * SyncFIFO
 * Simple synchronous FIFO for same clock domain
 */
module SyncFIFO #(
    parameter DATA_WIDTH = 25,      // 17-bit address + 8-bit data
    parameter ADDR_WIDTH = 9,       // log2(512) = 9
    parameter DEPTH = 512
) (
    // Clock and reset
    input  wire                   clk,
    input  wire                   reset,

    // Write port
    input  wire [DATA_WIDTH-1:0]  wr_data,
    input  wire                   wr_en,
    output wire                   wr_full,
    
    // Read port
    output wire [DATA_WIDTH-1:0]  rd_data,
    input  wire                   rd_en,
    output wire                   rd_empty
);

//=============================================================================
// Memory - using block RAM
//=============================================================================
(* ramstyle = "M9K" *) reg [DATA_WIDTH-1:0] mem [0:DEPTH-1];

//=============================================================================
// Pointers (simple binary counters)
//=============================================================================
reg [ADDR_WIDTH:0] wr_ptr = 0;  // Extra bit for full/empty detection
reg [ADDR_WIDTH:0] rd_ptr = 0;

//=============================================================================
// Full and Empty flags
//=============================================================================
// Empty: pointers are equal
assign rd_empty = (wr_ptr == rd_ptr);

// Full: pointers equal except MSB
assign wr_full = (wr_ptr[ADDR_WIDTH-1:0] == rd_ptr[ADDR_WIDTH-1:0]) && 
                 (wr_ptr[ADDR_WIDTH] != rd_ptr[ADDR_WIDTH]);

//=============================================================================
// Read/Write logic
//=============================================================================
reg [DATA_WIDTH-1:0] rd_data_reg = 0;
assign rd_data = rd_data_reg;

always @(posedge clk) begin
    if (reset) begin
        wr_ptr <= 0;
        rd_ptr <= 0;
        rd_data_reg <= 0;
    end else begin
        // Write operation
        if (wr_en && !wr_full) begin
            mem[wr_ptr[ADDR_WIDTH-1:0]] <= wr_data;
            wr_ptr <= wr_ptr + 1;
        end
        
        // Read operation (always read for block RAM, update pointer conditionally)
        rd_data_reg <= mem[rd_ptr[ADDR_WIDTH-1:0]];
        if (rd_en && !rd_empty) begin
            rd_ptr <= rd_ptr + 1;
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

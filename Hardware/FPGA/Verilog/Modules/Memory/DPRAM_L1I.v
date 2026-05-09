/*
 * DPRAM_L1I
 * Dual port RAM for L1 Instruction Cache, optimized for M9K usage.
 *
 * Splits the cache line into separate tag and data RAMs:
 *   - Tag/Valid RAM (15 bits × 128): true dual-port → 1 M9K block
 *   - Data RAM (256 bits × 128): simple dual-port → 8 M9K blocks
 *   Total: 9 M9K blocks (vs 16 for a unified true dual-port DPRAM)
 *
 * This works because:
 *   - The pipe port never writes (pipe_we is hardwired to 0 in the design)
 *   - The ctrl port only reads tag/valid bits (CacheControllerSDRAM uses
 *     l1i_ctrl_q only for tag comparison, never reads the data portion)
 *
 * Cache line format: {256bit_data[270:15], 14bit_tag[14:1], 1bit_valid[0]}
 *
 * Interface is identical to DPRAM for drop-in replacement.
 */
module DPRAM_L1I #(
    parameter WIDTH     = 271,
    parameter WORDS     = 128,
    parameter ADDR_BITS = 7,
    parameter INSTANCE  = "L1I"
) (
    input  wire                     clk_pipe,
    input  wire [    WIDTH-1:0]     pipe_d,
    input  wire [ADDR_BITS-1:0]     pipe_addr,
    input  wire                     pipe_we,
    output wire [    WIDTH-1:0]     pipe_q,

    input  wire                     clk_ctrl,
    input  wire [    WIDTH-1:0]     ctrl_d,
    input  wire [ADDR_BITS-1:0]     ctrl_addr,
    input  wire                     ctrl_we,
    output wire [    WIDTH-1:0]     ctrl_q
);

// ---- TAG/VALID RAM: true dual-port (1 M9K block) ----
// 15 bits: {14-bit tag, 1-bit valid}
// Both ports need read access; ctrl port also writes.
localparam TAG_WIDTH = 15;

reg [TAG_WIDTH-1:0] tag_ram [0:WORDS-1];

reg [TAG_WIDTH-1:0] pipe_tag_q;
reg [TAG_WIDTH-1:0] ctrl_tag_q;

// Pipe port: read-only (pipe_we is always 0 for L1I)
always @(posedge clk_pipe)
begin
    pipe_tag_q <= tag_ram[pipe_addr];
end

// Ctrl port: read + write
always @(posedge clk_ctrl)
begin
    ctrl_tag_q <= tag_ram[ctrl_addr];
    if (ctrl_we)
    begin
        ctrl_tag_q          <= ctrl_d[TAG_WIDTH-1:0]; // Forwarding
        tag_ram[ctrl_addr]  <= ctrl_d[TAG_WIDTH-1:0];
    end
end

// ---- DATA RAM: simple dual-port (8 M9K blocks) ----
// 256 bits: instruction data
// Pipe port reads, ctrl port writes (ctrl never reads L1I data).
localparam DATA_WIDTH = WIDTH - TAG_WIDTH; // 256

reg [DATA_WIDTH-1:0] data_ram [0:WORDS-1];

reg [DATA_WIDTH-1:0] pipe_data_q;

// Pipe port: read-only
always @(posedge clk_pipe)
begin
    pipe_data_q <= data_ram[pipe_addr];
end

// Ctrl port: write-only
always @(posedge clk_ctrl)
begin
    if (ctrl_we)
    begin
        data_ram[ctrl_addr] <= ctrl_d[WIDTH-1:TAG_WIDTH];
    end
end

// ---- Output assembly ----
// pipe_q: full cache line (data + tag + valid)
assign pipe_q = {pipe_data_q, pipe_tag_q};

// ctrl_q: only tag/valid is meaningful (data reads return 0)
// CacheControllerSDRAM only uses l1i_ctrl_q[14:0] for tag checks.
assign ctrl_q = {{DATA_WIDTH{1'b0}}, ctrl_tag_q};

// Initialize RAM to zero
integer i;
`ifdef __ICARUS__
initial
begin
    for (i = 0; i < WORDS; i = i + 1)
    begin
        tag_ram[i]  = {TAG_WIDTH{1'b0}};
        data_ram[i] = {DATA_WIDTH{1'b0}};
    end
end
`endif

endmodule

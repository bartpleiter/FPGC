// L1 data cache arrays
// The data and tags of the direct-mapped L1D: LINES lines of 8 words, in four byte
// lanes so a write can write single bytes, and a tag entry {valid, dirty, tag} per
// line. The hit check is in MEM, so this module only holds the arrays. Both ports read
// at every clock edge, because Quartus can't build a true dual-port RAM with an enable.
// Port A belongs to MEM: it reads at a_addr, and writes there at a write hit. Port B
// belongs to the cache controller: it reads lines to write back and writes the lines it
// fills. A port that writes an address reads the new value. See l1-cache.md, "The
// arrays".
//
// The tag is bits [25:5 + log2(LINES)] of an address, so an entry is 23 - log2(LINES)
// bits wide.
module cpu_l1d #(
    parameter LINES = 128  // a power of two, at least 2
) (
    input  wire                           clk,

    // Port A, MEM. The address bits are the index of the line and the word in the line.
    // A write writes the bytes of a_write_data that a_byte_enable selects, and an entry
    // write the entry of the line of a_addr.
    input  wire [$clog2(LINES)+4:2]       a_addr,
    output wire [31:0]                    a_word,
    output reg  [22-$clog2(LINES):0]      a_entry,
    input  wire                           a_write,
    input  wire [3:0]                     a_byte_enable,
    input  wire [31:0]                    a_write_data,
    input  wire                           a_entry_write,
    input  wire [22-$clog2(LINES):0]      a_entry_write_value,

    // Port B, the cache controller. A write writes the whole word.
    input  wire [$clog2(LINES)+4:2]       b_addr,
    output wire [31:0]                    b_word,
    output reg  [22-$clog2(LINES):0]      b_entry,
    input  wire                           b_write,
    input  wire [31:0]                    b_write_data,
    input  wire                           b_entry_write,
    input  wire [22-$clog2(LINES):0]      b_entry_write_value
);

localparam INDEX_BITS = $clog2(LINES);

// The data, one array of bytes per lane, starting at 0 like the tags. Quartus only builds
// a true dual-port RAM from an array whose two ports are two always blocks of the module
// itself, not of a generate loop, so the four lanes are written out.
reg [7:0] lane0 [0:8*LINES-1];
reg [7:0] lane1 [0:8*LINES-1];
reg [7:0] lane2 [0:8*LINES-1];
reg [7:0] lane3 [0:8*LINES-1];
reg [7:0] a_q0, a_q1, a_q2, a_q3;
reg [7:0] b_q0, b_q1, b_q2, b_q3;

integer i;
initial begin
    for (i = 0; i < 8 * LINES; i = i + 1) begin
        lane0[i] = 8'd0;
        lane1[i] = 8'd0;
        lane2[i] = 8'd0;
        lane3[i] = 8'd0;
    end
end

always @(posedge clk) begin
    if (a_write && a_byte_enable[0]) begin
        lane0[a_addr] <= a_write_data[7:0];
        a_q0 <= a_write_data[7:0];
    end else begin
        a_q0 <= lane0[a_addr];
    end
end

always @(posedge clk) begin
    if (b_write) begin
        lane0[b_addr] <= b_write_data[7:0];
        b_q0 <= b_write_data[7:0];
    end else begin
        b_q0 <= lane0[b_addr];
    end
end

always @(posedge clk) begin
    if (a_write && a_byte_enable[1]) begin
        lane1[a_addr] <= a_write_data[15:8];
        a_q1 <= a_write_data[15:8];
    end else begin
        a_q1 <= lane1[a_addr];
    end
end

always @(posedge clk) begin
    if (b_write) begin
        lane1[b_addr] <= b_write_data[15:8];
        b_q1 <= b_write_data[15:8];
    end else begin
        b_q1 <= lane1[b_addr];
    end
end

always @(posedge clk) begin
    if (a_write && a_byte_enable[2]) begin
        lane2[a_addr] <= a_write_data[23:16];
        a_q2 <= a_write_data[23:16];
    end else begin
        a_q2 <= lane2[a_addr];
    end
end

always @(posedge clk) begin
    if (b_write) begin
        lane2[b_addr] <= b_write_data[23:16];
        b_q2 <= b_write_data[23:16];
    end else begin
        b_q2 <= lane2[b_addr];
    end
end

always @(posedge clk) begin
    if (a_write && a_byte_enable[3]) begin
        lane3[a_addr] <= a_write_data[31:24];
        a_q3 <= a_write_data[31:24];
    end else begin
        a_q3 <= lane3[a_addr];
    end
end

always @(posedge clk) begin
    if (b_write) begin
        lane3[b_addr] <= b_write_data[31:24];
        b_q3 <= b_write_data[31:24];
    end else begin
        b_q3 <= lane3[b_addr];
    end
end

assign a_word = {a_q3, a_q2, a_q1, a_q0};
assign b_word = {b_q3, b_q2, b_q1, b_q0};

// The tags. Every line starts invalid, which is also the content after the FPGA is
// configured.
reg [22-INDEX_BITS:0] tags [0:LINES-1];

initial for (i = 0; i < LINES; i = i + 1) tags[i] = {(23 - INDEX_BITS){1'b0}};

always @(posedge clk) begin
    if (a_entry_write) begin
        tags[a_addr[INDEX_BITS+4:5]] <= a_entry_write_value;
        a_entry <= a_entry_write_value;
    end else begin
        a_entry <= tags[a_addr[INDEX_BITS+4:5]];
    end
end

always @(posedge clk) begin
    if (b_entry_write) begin
        tags[b_addr[INDEX_BITS+4:5]] <= b_entry_write_value;
        b_entry <= b_entry_write_value;
    end else begin
        b_entry <= tags[b_addr[INDEX_BITS+4:5]];
    end
end

endmodule

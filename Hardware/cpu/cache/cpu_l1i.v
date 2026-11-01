// L1 instruction cache arrays
// The data and tags of the direct-mapped L1I: LINES lines of 8 words, and a tag entry
// {valid, tag} per line. The hit check is in IF, so this module only holds the arrays.
// Port A belongs to IF and reads, with the timing of the ROM's fetch port: at a clock
// edge when a_enable is 1 it reads at a_addr, and otherwise its outputs keep their
// value. Port B belongs to the cache controller and writes. See l1-cache.md, "The
// arrays".
//
// The tag is bits [25:5 + log2(LINES)] of an address, so an entry is 22 - log2(LINES)
// bits wide. The L1I is never written by the CPU, so its entries have no dirty bit.
module cpu_l1i #(
    parameter LINES = 128  // a power of two, at least 2
) (
    input  wire                           clk,

    // Port A, IF. The address bits are the index of the line and the word in the line.
    input  wire [$clog2(LINES)+4:2]       a_addr,
    input  wire                           a_enable,
    output reg  [31:0]                    a_word,
    output reg  [21-$clog2(LINES):0]      a_entry,

    // Port B, the cache controller. It writes a word at b_addr, and the entry of the
    // line of b_addr.
    input  wire [$clog2(LINES)+4:2]       b_addr,
    input  wire                           b_write,
    input  wire [31:0]                    b_write_data,
    input  wire                           b_entry_write,
    input  wire [21-$clog2(LINES):0]      b_entry_write_value
);

localparam INDEX_BITS = $clog2(LINES);

// Both arrays start at 0, so every line is invalid. That's also their content after
// the FPGA is configured.
//
// What port A reads in a cycle in which port B writes the same word or entry doesn't
// matter: the current rule never uses such a read (l1-cache.md, "The current rule").
// The attribute tells Quartus so. Without it, Quartus keeps Verilog's "old value" for
// that case, which an M9K can't give together with a read clock enable, so it adds a
// bypass of about 130 registers and 60 LUTs behind the arrays, in front of IF's
// instruction mux and tag compare.
(* ramstyle = "no_rw_check" *) reg [31:0]            data [0:8*LINES-1];
(* ramstyle = "no_rw_check" *) reg [21-INDEX_BITS:0] tags [0:LINES-1];

integer i;
initial begin
    for (i = 0; i < 8 * LINES; i = i + 1) data[i] = 32'd0;
    for (i = 0; i < LINES; i = i + 1) tags[i] = {(22 - INDEX_BITS){1'b0}};
end

always @(posedge clk) begin
    if (b_write)
        data[b_addr] <= b_write_data;
    if (a_enable)
        a_word <= data[a_addr];
end

always @(posedge clk) begin
    if (b_entry_write)
        tags[b_addr[INDEX_BITS+4:5]] <= b_entry_write_value;
    if (a_enable)
        a_entry <= tags[a_addr[INDEX_BITS+4:5]];
end

endmodule

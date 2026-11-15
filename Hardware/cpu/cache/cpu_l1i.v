// L1 instruction cache arrays
module cpu_l1i #(
    parameter LINES = 128  // a power of two, at least 2
) (
    input  wire                           clk,

    // Port A, IF.
    input  wire [$clog2(LINES)+4:2]       a_addr,
    input  wire                           a_enable,
    output reg  [31:0]                    a_word,
    output reg  [21-$clog2(LINES):0]      a_entry,

    // Port B, the cache controller.
    input  wire [$clog2(LINES)+4:2]       b_addr,
    input  wire                           b_write,
    input  wire [31:0]                    b_write_data,
    input  wire                           b_entry_write,
    input  wire [21-$clog2(LINES):0]      b_entry_write_value
);

localparam INDEX_BITS = $clog2(LINES);

// This helps forcing Quartus to implement the arrays as simple Block RAMs, read during write does not matter.
(* ramstyle = "no_rw_check" *) reg [31:0]            data [0:8*LINES-1];
(* ramstyle = "no_rw_check" *) reg [21-INDEX_BITS:0] tags [0:LINES-1];

// Only needed in simulation as Quartus initializes the RAMs to 0 by itself
integer i;

initial begin
`ifndef SYNTHESIS
    for (i = 0; i < 8 * LINES; i = i + 1) data[i] = 32'd0;
`endif
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

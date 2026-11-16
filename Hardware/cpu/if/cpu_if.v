// Instruction fetch stage (IF)
module cpu_if #(
    parameter L1I_LINES = 128  // the number of lines of the L1I, a power of two
) (
    input  wire                           clk,
    input  wire                           reset,
    input  wire                           stall_if,
    input  wire                           refetch_if,

    // Redirect from MEM
    input  wire                           redirect,
    input  wire [31:0]                    redirect_target,

    output wire [31:0]                    fetch_addr,
    output wire                           fetch_en,
    input  wire [31:0]                    rom_word,
    input  wire [31:0]                    l1i_word,
    input  wire [21-$clog2(L1I_LINES):0]  l1i_entry,

    output reg                            l1i_request,
    input  wire                           l1i_take,
    input  wire                           l1i_writing,

    // L1i fill status and address by the cache controller
    input  wire                           l1i_filling,
    input  wire [25:2]                    fill_addr,

    input  wire                           sdram_read_valid,
    input  wire [31:0]                    sdram_read_word,

    // To ID
    output reg  [31:0]                    pc,
    output wire [31:0]                    if_instr,
    output wire                           busy_if
);

localparam ROM_BASE = 32'h04000000;  // where the CPU starts after reset
localparam INDEX    = $clog2(L1I_LINES);

/* verilator lint_off PINCONNECTEMPTY */
wire in_rom;

cpu_memory_map pc_map (
    .addr(pc),
    .sdram(),
    .bus(),
    .rom(in_rom),
    .cpu_io(),
    .code(),
    .unmapped()
);
/* verilator lint_on PINCONNECTEMPTY */

reg l1i_current;

wire l1i_match = l1i_entry == {1'b1, pc[25:INDEX+5]};
wire l1i_hit   = l1i_current && l1i_match;

// Early restart
wire l1i_bypass = !in_rom && sdram_read_valid && l1i_filling && fill_addr == pc[25:2];

// Reread when the output isn't current
wire l1i_reread = !in_rom && !l1i_current && !l1i_writing && !l1i_request;

reg refetch;

// A stalled IF keeps its word, because the fetch memories keep their outputs.
assign fetch_addr = reset      ? ROM_BASE :
                    redirect   ? redirect_target :
                    refetch    ? pc - 32'd4 :
                    l1i_reread ? pc :
                                 pc + 32'd4;
assign fetch_en   = reset || redirect || refetch || l1i_reread || !stall_if;

always @(posedge clk) begin
    refetch <= refetch_if && !reset;

    if (fetch_en)
        pc <= fetch_addr;

    if (fetch_en)
        l1i_current <= !l1i_writing;
    else if (l1i_writing)
        l1i_current <= 1'b0;

    // When to take an l1i request
    if (reset || l1i_take || redirect || refetch)
        l1i_request <= 1'b0;
    else if (!in_rom && l1i_current && !l1i_match)
        l1i_request <= 1'b1;
end

assign if_instr = in_rom     ? rom_word        :
                  l1i_bypass ? sdram_read_word :
                               l1i_word;
assign busy_if  = !in_rom && !l1i_bypass && !l1i_hit;

`ifndef SYNTHESIS
// Simulation only check to catch implementation bugs
always @(posedge clk)
    if (!reset && !stall_if && !in_rom && !l1i_current && !l1i_bypass)
        $fatal(1, "cpu_if moved on with an L1I output that isn't current");
`endif

endmodule

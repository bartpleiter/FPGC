// Instruction fetch stage (IF)
// Holds the PC, the address of the instruction at the output of the fetch memories, and
// computes the next fetch address. The fetch memories are the ROM and port A of the L1I,
// both read at fetch_addr when fetch_en is 1. An instruction outside the ROM comes from
// the L1I, and IF only uses an L1I output that is current: read at the PC, and not
// written by the cache controller since. On a miss, IF requests the line from the cache
// controller, and reads again once the controller has stopped writing the L1I. While
// the controller fills a line of the L1I, IF also takes the word at the PC from the
// SDRAM port in the cycle it arrives (early restart). See l1-cache.md, "L1I: hits in
// IF", "The current rule", "Misses" and "Early restart".
module cpu_if #(
    parameter L1I_LINES = 128  // the number of lines of the L1I, a power of two
) (
    input  wire                           clk,
    input  wire                           reset,
    input  wire                           stall_if,
    input  wire                           refetch_if,  // the instruction in IF was lost

    // The redirect from MEM
    input  wire                           redirect,
    input  wire [31:0]                    redirect_target,

    // The ROM and the L1I read the word at fetch_addr at a clock edge when fetch_en is
    // 1, and keep their outputs otherwise. The L1I also gives the entry {valid, tag} of
    // the line.
    output wire [31:0]                    fetch_addr,
    output wire                           fetch_en,
    input  wire [31:0]                    rom_word,
    input  wire [31:0]                    l1i_word,
    input  wire [21-$clog2(L1I_LINES):0]  l1i_entry,

    // The cache controller takes the request for the line of the PC with l1i_take, and
    // l1i_writing is 1 from then until after its last write to the L1I.
    output reg                            l1i_request,
    input  wire                           l1i_take,
    input  wire                           l1i_writing,

    // While l1i_filling is 1, the controller fills a line of the L1I, and the word that
    // arrives on the SDRAM port with sdram_read_valid is the one at fill_addr.
    input  wire                           l1i_filling,
    input  wire [25:2]                    fill_addr,
    input  wire                           sdram_read_valid,
    input  wire [31:0]                    sdram_read_word,

    // To ID
    output reg  [31:0]                    pc,
    output wire [31:0]                    if_instr,
    output wire                           busy_if
);

localparam ROM_BASE = 32'h1E000000;  // where the CPU starts after reset
localparam INDEX    = $clog2(L1I_LINES);

/* verilator lint_off PINCONNECTEMPTY */
wire in_rom;

cpu_memory_map pc_map (
    .addr(pc),
    .sdram(),
    .io(),
    .rom(in_rom),
    .vram32(),
    .vram8(),
    .vrampx(),
    .palette(),
    .cpu_io(),
    .code(),
    .unmapped()
);
/* verilator lint_on PINCONNECTEMPTY */

// The L1I output is current when it was read while the controller wasn't writing the
// L1I, and the controller hasn't written it since. A read is always at the PC: it's the
// read of the address the PC takes at the same edge, or a re-read of the PC.
reg l1i_current;

wire l1i_match = l1i_entry == {1'b1, pc[25:INDEX+5]};
wire l1i_hit   = l1i_current && l1i_match;

// Early restart: the word at the PC arrives on the SDRAM port for a fill of the L1I.
// It comes from registers only, so it's known long before the tag compare. A word that
// IF can't take in the cycle it arrives isn't used, and IF reads the L1I again after
// the fill, as for any output that isn't current.
wire l1i_bypass = !in_rom && sdram_read_valid && l1i_filling && fill_addr == pc[25:2];

// When the output isn't current, IF reads the PC again once the controller has stopped
// writing the L1I, and doesn't wait for a request of its own.
wire l1i_reread = !in_rom && !l1i_current && !l1i_writing && !l1i_request;

// IF doesn't wait for the tag compare of an L1D miss. In the first cycle of a miss it
// can move on while ID keeps its instruction, and then the instruction that was in IF
// is lost. IF fetches it again in the next cycle, while MEM waits. It moved on by one
// instruction, because a redirect or a re-read can't happen in that cycle.
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

    // The request is set in a cycle in which IF sees a current miss, and the take
    // clears it, also in the cycle of the take, when IF still sees the same miss. A
    // redirect drops a request that the controller hasn't taken. If the controller
    // takes it at the same edge, that line is filled anyway. The output of the new PC
    // was read before the fill writes anything, so IF can use it if it hits. If it
    // misses, the take clears that request too, and IF reads again after the fill. A
    // refetch drops a request for the line after a lost instruction.
    if (reset || l1i_take || redirect || refetch)
        l1i_request <= 1'b0;
    else if (!in_rom && l1i_current && !l1i_match)
        l1i_request <= 1'b1;
end

// The tag compare comes late, so it's the last term.
assign if_instr = in_rom     ? rom_word        :
                  l1i_bypass ? sdram_read_word :
                               l1i_word;
assign busy_if  = !in_rom && !l1i_bypass && !l1i_hit;

`ifndef SYNTHESIS
// A check in simulation: IF never moves on with an L1I output that isn't current,
// unless it takes the word from the SDRAM port. The testbench of the ISS comparison also
// checks that the word is the SDRAM's.
always @(posedge clk)
    if (!reset && !stall_if && !in_rom && !l1i_current && !l1i_bypass)
        $fatal(1, "cpu_if moved on with an L1I output that isn't current");
`endif

endmodule

// Instruction fetch stage (IF)
// Holds the PC, the address of the instruction at the fetch memory's output, and
// computes the next fetch address.
module cpu_if (
    input  wire        clk,
    input  wire        reset,
    input  wire        stall_if,

    // The redirect from MEM
    input  wire        redirect,
    input  wire [31:0] redirect_target,

    // The fetch memory registers fetch_addr at the clock edge when fetch_en is 1, and
    // fetch_word is the word at that address from then on.
    output wire [31:0] fetch_addr,
    output wire        fetch_en,
    input  wire [31:0] fetch_word,

    // To ID
    output reg  [31:0] pc,
    output wire [31:0] if_instr,
    output wire        busy_if
);

localparam ROM_BASE = 32'h1E000000;  // where the CPU starts after reset

// A stalled IF keeps its word, because the fetch memory doesn't take a new address.
assign fetch_addr = reset    ? ROM_BASE :
                    redirect ? redirect_target :
                               pc + 32'd4;
assign fetch_en   = reset || redirect || !stall_if;

always @(posedge clk)
    if (fetch_en)
        pc <= fetch_addr;

assign if_instr = fetch_word;
assign busy_if  = 1'b0;  // the fetch memory always has the word, until there are caches

endmodule

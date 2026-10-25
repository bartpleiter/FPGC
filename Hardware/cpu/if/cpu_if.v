// Instruction fetch stage (IF)
// Holds the PC, the address of the instruction at the fetch memory's output, and
// computes the next fetch address. The fetch memory only gets a new request once it has
// answered the last one, also after a redirect.
module cpu_if (
    input  wire        clk,
    input  wire        reset,
    input  wire        stall_if,

    // The redirect from MEM
    input  wire        redirect,
    input  wire [31:0] redirect_target,

    // The fetch memory takes a request for the word at fetch_addr at a clock edge when
    // fetch_en is 1. It answers with fetch_ready, in the cycle after the request at the
    // earliest, and fetch_word is then the word until the next request. Apart from
    // reset, a new request only comes once the last one is answered.
    output wire [31:0] fetch_addr,
    output wire        fetch_en,
    input  wire [31:0] fetch_word,
    input  wire        fetch_ready,

    // To ID
    output reg  [31:0] pc,
    output wire [31:0] if_instr,
    output wire        busy_if
);

localparam ROM_BASE = 32'h1E000000;  // where the CPU starts after reset

// A redirect while the memory is still busy can't make its request yet. The PC already
// becomes the target, and old_request says that the word that comes is for the old
// address. IF then drops that word, and makes the request for the PC.
reg old_request;

// A stalled IF keeps its word, because the fetch memory doesn't get a new request.
assign fetch_addr = reset       ? ROM_BASE :
                    redirect    ? redirect_target :
                    old_request ? pc :
                                  pc + 32'd4;
assign fetch_en   = reset || (fetch_ready && (redirect || old_request)) || !stall_if;

always @(posedge clk) begin
    if (fetch_en || redirect)
        pc <= fetch_addr;
    if (reset)
        old_request <= 1'b0;
    else if (redirect && !fetch_ready)
        old_request <= 1'b1;
    else if (fetch_en)
        old_request <= 1'b0;
end

assign if_instr = fetch_word;
assign busy_if  = !fetch_ready || old_request;

endmodule

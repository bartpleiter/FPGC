// B32P4
// Top-level design of the CPU
// Currently a placeholder until the rest of the CPU design is finished.
module b32p4 (
    input  wire        clk,
    input  wire        reset,

    // Fetch: the instruction memory registers fetch_addr at the clock edge when
    // fetch_en is 1, and fetch_word is the word at that address from then on.
    output wire [31:0] fetch_addr,
    output wire        fetch_en,
    input  wire [31:0] fetch_word,

    // Retire: a debug port currently used in simulation only for observing finished (retired) instructions.
    output wire        retire_valid,
    output wire [31:0] retire_pc,
    output wire [31:0] retire_instr,
    output wire        retire_writes_dreg,
    output wire [3:0]  retire_dreg,
    output wire [31:0] retire_dreg_value,
    output wire        retire_write,
    output wire [31:0] retire_write_addr,
    output wire [31:0] retire_write_data,
    output wire [3:0]  retire_write_be,
    output wire        retire_push,
    output wire [31:0] retire_push_value,
    output wire        retire_pop,
    output wire [31:0] retire_pop_value,
    output wire        retire_redirect,
    output wire [31:0] retire_target,
    output wire        retire_irq,
    output wire [2:0]  retire_irq_id
);

localparam ROM_BASE = 32'h1E000000;
localparam HALT     = 32'hFFFFFFFF;

reg [31:0] pc;

assign fetch_addr = reset ? ROM_BASE : pc + 32'd4;
assign fetch_en   = 1'b1;

always @(posedge clk)
    pc <= fetch_addr;

assign retire_valid       = !reset;
assign retire_pc          = pc;
assign retire_instr       = fetch_word;
assign retire_writes_dreg = 1'b0;
assign retire_dreg        = 4'd0;
assign retire_dreg_value  = 32'd0;
assign retire_write       = 1'b0;
assign retire_write_addr  = 32'd0;
assign retire_write_data  = 32'd0;
assign retire_write_be    = 4'd0;
assign retire_push        = 1'b0;
assign retire_push_value  = 32'd0;
assign retire_pop         = 1'b0;
assign retire_pop_value   = 32'd0;
assign retire_redirect    = fetch_word == HALT;
assign retire_target      = pc;
assign retire_irq         = 1'b0;
assign retire_irq_id      = 3'd0;

endmodule

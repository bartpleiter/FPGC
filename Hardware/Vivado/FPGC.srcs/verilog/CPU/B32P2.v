/*
 * B32P2, a 32-bit pipelined CPU with 6-stage pipeline
 * Second iteration of B32P with focus on single cycle execution of all stages
 *
 * Features:
 * - 6 stage pipeline
 *   - FE1:     Instruction Cache Fetch
 *   - FE2:     Instruction Cache Miss Fetch
 *   - DE:      Decode
 *   - EXMEM1:  Execute and Data Cache Access
 *   - EXMEM2:  Multi-cycle Execute and Data Cache Miss Handling
 *   - WB:      Writeback
 * - Hazard detection and forwarding
 * - 27 bit address space for 0.5GiB of addressable memory
 */
module B32P2 #(
    parameter PC_START = 27'd0, // Initial PC value, so a bootloader can be placed at a later address
    parameter INTERRUPT_VALID_FROM = 27'd0, // Address from which interrupts are valid/enabled
    parameter INTERRUPT_JUMP_ADDR = 27'd1 // Address to jump to when an interrupt is triggered
) (
    // Clock and reset
    input wire clk,
    input wire reset,

    // L1i cache
    output wire [8:0] icache_addr,
    input wire [31:0] icache_q
);
    
reg [26:0] PC = PC_START; // Program Counter


/*
 * Stage 1: Instruction Cache Fetch
 */

// Instruction Cache
assign icache_addr = PC;


/*
 * Stage 2: Instruction Cache Miss Fetch
 */

// TODO: Skipped for now

// Forward FE1 to FE2
wire [31:0] icache_q_FE2;
Regr #(
    .N(32)
) regr_FE1_FE2 (
    .clk (clk),
    .in(icache_q),
    .out(icache_q_FE2),
    .hold(1'b0),
    .clear(1'b0)
);

/*
 * Stage 3: Decode
 */

endmodule

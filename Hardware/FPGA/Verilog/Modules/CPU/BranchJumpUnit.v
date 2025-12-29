/*
 * BranchJumpUnit
 * Handles branch and jump address calculations
 * 
 * Uses pre-computed jump and branch addresses to optimize timings
 * Uses parallel comparator for reduced critical path delay
 */
module BranchJumpUnit (
    input wire  [2:0]   branchOP,
    input wire  [31:0]  data_a,
    input wire  [31:0]  data_b,
    input wire  [31:0]  const16,
    input wire  [31:0]  pc,
    input wire          halt,
    input wire          branch,
    input wire          jumpc,
    input wire          jumpr,
    input wire          oe,
    input wire          sig,
    
    // Pre-computed addresses
    input wire  [31:0]  pre_jump_const_addr,  // pc + const27 or {5'b0, const27}
    input wire  [31:0]  pre_branch_addr,      // pc + const16

    output reg  [31:0]  jump_addr,
    output wire         jump_valid
);

// Branch opcodes
localparam 
    BRANCH_OP_BEQ   = 3'b000, // A == B
    BRANCH_OP_BGT   = 3'b001, // A >  B
    BRANCH_OP_BGE   = 3'b010, // A >= B
    // BRANCH_OP_XXX   = 3'b011, // Reserved
    BRANCH_OP_BNE   = 3'b100, // A != B
    BRANCH_OP_BLT   = 3'b101, // A <  B
    BRANCH_OP_BLE   = 3'b110; // A <= B
    // BRANCH_OP_XXX   = 3'b111; // Reserved

// =============================================================================
// PARALLEL COMPARATOR
// Uses tree structure instead of carry chain for faster comparison
// =============================================================================
wire cmp_eq;  // a == b
wire cmp_lt;  // a < b
wire cmp_gt;  // a > b

ParallelComparator comparator (
    .a   (data_a),
    .b   (data_b),
    .sig (sig),
    .eq  (cmp_eq),
    .lt  (cmp_lt),
    .gt  (cmp_gt)
);

// Branch pass detection using parallel comparator results
reg branch_passed;
always @(*) 
begin
    case (branchOP)
        BRANCH_OP_BEQ: branch_passed = cmp_eq;
        BRANCH_OP_BGT: branch_passed = cmp_gt;
        BRANCH_OP_BGE: branch_passed = cmp_gt | cmp_eq;
        BRANCH_OP_BNE: branch_passed = ~cmp_eq;
        BRANCH_OP_BLT: branch_passed = cmp_lt;
        BRANCH_OP_BLE: branch_passed = cmp_lt | cmp_eq;
        default:       branch_passed = 1'b0;
    endcase
end

// Jump address calculation
always @(*) 
begin
    if (jumpc)
    begin
        jump_addr <= pre_jump_const_addr;
    end

    else if (jumpr)
    begin
        if (oe)
        begin
            jump_addr <= pc + (data_b + const16);
        end
        else
        begin
            jump_addr <= data_b + const16;
        end
    end
    
    else if (branch)
    begin
        jump_addr <= pre_branch_addr;
    end

    else if (halt)
    begin
        // Jump to same address to halting
        jump_addr <= pc;
    end

    else
    begin
        jump_addr <= 32'd0;
    end
end

// Jump valid detection
assign jump_valid = (jumpc | jumpr | (branch & branch_passed) | halt);

endmodule

/*
 * BranchJumpUnit
 * Handles branch and jump address calculations.
 * Branch comparison is pre-computed in the EX stage (BranchCompare module)
 * and registered into ex_mem_branch_passed before reaching this module,
 * breaking the critical timing path through the MEM-stage comparator.
 */
module BranchJumpUnit (
    input wire  [31:0]  data_b,
    input wire  [31:0]  const16,
    input wire  [26:0]  const27,
    input wire  [31:0]  pc,
    input wire          halt,
    input wire          branch,
    input wire          jumpc,
    input wire          jumpr,
    input wire          oe,
    input wire          branch_passed,  // Pre-computed in EX, registered at EX/MEM boundary

    output reg  [31:0]  jump_addr,
    output wire         jump_valid
);

// Jump address calculation
always @(*)
begin
    if (jumpc)
    begin
        if (oe)
            jump_addr = pc + {{5{const27[26]}}, const27}; // Sign-extend const27
        else
            jump_addr = {5'b0, const27};
    end

    else if (jumpr)
    begin
        if (oe)
        begin
            jump_addr = pc + (data_b + const16);
        end
        else
        begin
            jump_addr = data_b + const16;
        end
    end

    else if (branch)
    begin
        jump_addr = pc + const16;
    end

    else if (halt)
    begin
        // Jump to same address to halting
        jump_addr = pc;
    end

    else
    begin
        jump_addr = 32'd0;
    end
end

// Jump valid detection
assign jump_valid = (jumpc | jumpr | (branch & branch_passed) | halt);

endmodule
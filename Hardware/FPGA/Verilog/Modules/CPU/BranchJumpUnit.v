/*
 * BranchJumpUnit
 * Handles branch and jump address calculations
 */
module BranchJumpUnit (
    input wire  [2:0]   branchOP,
    input wire  [31:0]  data_a,
    input wire  [31:0]  data_b,
    input wire  [31:0]  const16,
    input wire  [26:0]  const27,
    input wire  [31:0]  pc,
    input wire          halt,
    input wire          branch,
    input wire          jumpc,
    input wire          jumpr,
    input wire          oe,
    input wire          sig,

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

// Branch pass detection
reg branch_passed;
always @(*) 
begin
    case (branchOP)
        BRANCH_OP_BEQ:
        begin
            branch_passed <= (data_a == data_b);
        end
        BRANCH_OP_BGT:
        begin
            branch_passed <= (sig) ? ($signed(data_a) > $signed(data_b)) : (data_a > data_b);
        end
        BRANCH_OP_BGE:
        begin
            branch_passed <= (sig) ? ($signed(data_a) >= $signed(data_b)) : (data_a >= data_b);
        end
        BRANCH_OP_BNE:
        begin
            branch_passed <= (data_a != data_b);
        end
        BRANCH_OP_BLT:
        begin
            branch_passed <= (sig) ? ($signed(data_a) < $signed(data_b)) : (data_a < data_b);
        end
        BRANCH_OP_BLE:
        begin
            branch_passed <= (sig) ? ($signed(data_a) <= $signed(data_b)) : (data_a <= data_b);
        end
        default:
        begin
            branch_passed <= 1'b0;
        end
    endcase
end

// Jump address calculation
always @(*) 
begin
    if (jumpc)
    begin
        if (oe)
            jump_addr <= pc + {{5{const27[26]}}, const27}; // Sign-extend const27
        else
            jump_addr <= {5'b0, const27};
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
        jump_addr <= pc + const16;
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

/*
 * BranchCompare
 * Combinational branch condition evaluator.
 * Extracted from BranchJumpUnit to allow pre-computation in EX stage,
 * breaking the critical timing path through the MEM-stage ripple-carry
 * comparator chain.
 */
module BranchCompare (
    input wire [31:0] data_a,
    input wire [31:0] data_b,
    input wire [2:0]  branch_op,
    input wire        sig,
    output reg        passed
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

always @(*)
begin
    case (branch_op)
        BRANCH_OP_BEQ:
        begin
            passed = (data_a == data_b);
        end
        BRANCH_OP_BGT:
        begin
            passed = (sig) ? ($signed(data_a) > $signed(data_b)) : (data_a > data_b);
        end
        BRANCH_OP_BGE:
        begin
            passed = (sig) ? ($signed(data_a) >= $signed(data_b)) : (data_a >= data_b);
        end
        BRANCH_OP_BNE:
        begin
            passed = (data_a != data_b);
        end
        BRANCH_OP_BLT:
        begin
            passed = (sig) ? ($signed(data_a) < $signed(data_b)) : (data_a < data_b);
        end
        BRANCH_OP_BLE:
        begin
            passed = (sig) ? ($signed(data_a) <= $signed(data_b)) : (data_a <= data_b);
        end
        default:
        begin
            passed = 1'b0;
        end
    endcase
end

endmodule
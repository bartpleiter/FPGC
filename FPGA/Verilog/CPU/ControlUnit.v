/*
* Control Unit
*/

module ControlUnit(
    input       [3:0]   instrOP,
    input               he,

    output reg          alu_use_const,
    output reg          push, pop,
    output reg          dreg_we,
    output reg          mem_write, mem_read,
    output reg          jumpc, jumpr, branch, halt, reti,
    output reg          getIntID, getPC, clearCache
);

// Instruction Opcodes
localparam 
    OP_HALT     = 4'b1111,
    OP_READ     = 4'b1110,
    OP_WRITE    = 4'b1101,
    OP_INTID    = 4'b1100,
    OP_PUSH     = 4'b1011,
    OP_POP      = 4'b1010,
    OP_JUMP     = 4'b1001,
    OP_JUMPR    = 4'b1000,
    OP_CCACHE   = 4'b0111,
    OP_BRANCH   = 4'b0110,
    OP_SAVPC    = 4'b0101,
    OP_RETI     = 4'b0100,
    OP_UNDEF2   = 4'b0011, // undefined
    OP_UNDEF3   = 4'b0010, // undefined
    OP_ARITHC   = 4'b0001,
    OP_ARITH    = 4'b0000;


always @(*) begin
    // default
    alu_use_const   <= 1'b0;
    push            <= 1'b0;
    pop             <= 1'b0;
    dreg_we         <= 1'b0;
    mem_write       <= 1'b0;
    mem_read        <= 1'b0;
    jumpc           <= 1'b0;
    jumpr           <= 1'b0;
    getIntID        <= 1'b0;
    getPC           <= 1'b0;
    branch          <= 1'b0;
    halt            <= 1'b0;
    reti            <= 1'b0;
    clearCache      <= 1'b0;

    case (instrOP)
        OP_HALT:
        begin
            halt <= 1'b1;
        end

        OP_READ:
        begin
            mem_read <= 1'b1;
            dreg_we <= 1'b1;
        end

        OP_WRITE:
        begin
            mem_write <= 1'b1;
        end

        OP_INTID: // write interrupt ID to dreg
        begin
            getIntID <= 1'b1;
            dreg_we <= 1'b1;
        end

        OP_PUSH: // push reg to stack
        begin
            push <= 1'b1;
        end

        OP_POP: // pop stack tot reg
        begin
            dreg_we <= 1'b1;
            pop <= 1'b1;
        end

        OP_JUMP:
        begin
            jumpc <= 1'b1;
        end

        OP_JUMPR:
        begin
            jumpr <= 1'b1;
        end

        OP_BRANCH:
        begin
            branch <= 1'b1;
        end

        OP_SAVPC: // write PC to dreg
        begin
            getPC <= 1'b1;
            dreg_we <= 1'b1;
        end

        OP_RETI:
        begin
            reti <= 1'b1;
        end

        OP_CCACHE:
        begin
            clearCache <= 1'b1;
        end

        OP_ARITH:
        begin
            dreg_we <= 1'b1;
        end

        OP_ARITHC:
        begin
            alu_use_const <= 1'b1;
            dreg_we <= 1'b1;
        end

    endcase
end

endmodule
/*
 * Control Unit
 * Simple control unit that sets flags based on the instruction opcode and optional ALU opcode
 */
module ControlUnit(
    input wire [3:0]    instrOP,
    input wire [3:0]    aluOP,

    output reg          alu_use_const,
    output reg          alu_use_constu,
    output reg          push,
    output reg          pop,
    output reg          dreg_we,
    output reg          mem_write,
    output reg          mem_read,
    output reg          jumpc,
    output reg          jumpr,
    output reg          branch,
    output reg          halt,
    output reg          reti,
    output reg          getIntID,
    output reg          getPC,
    output reg          clearCache
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
    OP_ARITHMC  = 4'b0011,
    OP_ARITHM   = 4'b0010,
    OP_ARITHC   = 4'b0001,
    OP_ARITH    = 4'b0000;


always @(*)
begin
    // Default values
    alu_use_const   <= 1'b0;
    alu_use_constu  <= 1'b0;
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

    // Set values based on opcode
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

        // Write interrupt ID to dreg
        OP_INTID:
        begin
            getIntID <= 1'b1;
            dreg_we <= 1'b1;
        end

        // Push reg to stack
        OP_PUSH:
        begin
            push <= 1'b1;
        end

        // Pop stack to reg
        OP_POP:
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

        // Write PC to dreg
        OP_SAVPC:
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

        OP_ARITH, OP_ARITHM:
        begin
            dreg_we <= 1'b1;
        end

        OP_ARITHC, OP_ARITHMC:
        begin
            alu_use_const <= 1'b1;
            if (aluOP[3:1] == 3'b110)
            begin
                alu_use_constu <= 1'b1;
            end
            dreg_we <= 1'b1;
        end

    endcase
end

endmodule

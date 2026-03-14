/*
 * ControlUnit
 * Simple control unit that sets flags based on the instruction opcode and optional ALU opcode
 */
module ControlUnit (
    input wire  [3:0]   instr_op,
    input wire  [3:0]   alu_op,

    // Sub-opcode fields for byte-addressable memory operations
    input wire  [3:0]   read_subop,   // bits [7:4] of READ instruction
    input wire  [3:0]   write_subop,  // bits [3:0] of WRITE instruction

    output reg          alu_use_const,
    output reg          alu_use_constu,
    output reg          push,
    output reg          pop,
    output reg          dreg_we,
    output reg          mem_write,
    output reg          mem_read,
    output reg          arithm,
    output reg          jumpc,
    output reg          jumpr,
    output reg          branch,
    output reg          halt,
    output reg          reti,
    output reg          get_int_id,
    output reg          get_pc,
    output reg          clear_cache,

    // Memory size control for byte-addressable operations
    output reg  [1:0]   mem_size,         // 00=word, 01=byte, 10=halfword
    output reg          mem_sign_extend   // 1=sign-extend, 0=zero-extend (reads only)
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
    alu_use_const   = 1'b0;
    alu_use_constu  = 1'b0;
    push            = 1'b0;
    pop             = 1'b0;
    dreg_we         = 1'b0;
    mem_write       = 1'b0;
    mem_read        = 1'b0;
    arithm          = 1'b0;
    jumpc           = 1'b0;
    jumpr           = 1'b0;
    get_int_id        = 1'b0;
    get_pc           = 1'b0;
    branch          = 1'b0;
    halt            = 1'b0;
    reti            = 1'b0;
    clear_cache      = 1'b0;
    mem_size        = 2'b00;
    mem_sign_extend = 1'b0;

    // Set values based on opcode
    case (instr_op)
        OP_HALT:
        begin
            halt = 1'b1;
        end

        OP_READ:
        begin
            mem_read = 1'b1;
            dreg_we = 1'b1;
            // read_subop[1:0]: 00=word, 01=byte, 10=half
            // read_subop[2]: 0=signed, 1=unsigned
            mem_size = read_subop[1:0];
            mem_sign_extend = ~read_subop[2];
        end

        OP_WRITE:
        begin
            mem_write = 1'b1;
            // write_subop[1:0]: 00=word, 01=byte, 10=half
            mem_size = write_subop[1:0];
        end

        // Write interrupt ID to dreg
        OP_INTID:
        begin
            get_int_id = 1'b1;
            dreg_we = 1'b1;
        end

        // Push reg to stack
        OP_PUSH:
        begin
            push = 1'b1;
        end

        // Pop stack to reg
        OP_POP:
        begin
            dreg_we = 1'b1;
            pop = 1'b1;
        end

        OP_JUMP:
        begin
            jumpc = 1'b1;
        end

        OP_JUMPR:
        begin
            jumpr = 1'b1;
        end

        OP_BRANCH:
        begin
            branch = 1'b1;
        end

        // Write PC to dreg
        OP_SAVPC:
        begin
            get_pc = 1'b1;
            dreg_we = 1'b1;
        end

        OP_RETI:
        begin
            reti = 1'b1;
        end

        OP_CCACHE:
        begin
            clear_cache = 1'b1;
        end

        OP_ARITH:
        begin
            dreg_we = 1'b1;
        end

        OP_ARITHC:
        begin
            alu_use_const = 1'b1;
            if (alu_op[3:1] == 3'b110)
            begin
                alu_use_constu = 1'b1;
            end
            dreg_we = 1'b1;
        end

        OP_ARITHM:
        begin
            arithm = 1'b1;
            dreg_we = 1'b1;
        end

        OP_ARITHMC:
        begin
            arithm = 1'b1;
            alu_use_const = 1'b1;
            if (alu_op[3:1] == 3'b110)
            begin
                alu_use_constu = 1'b1;
            end
            dreg_we = 1'b1;
        end


    endcase
end

endmodule

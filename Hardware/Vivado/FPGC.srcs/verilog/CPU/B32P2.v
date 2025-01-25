/*
 * B32P2, a 32-bit pipelined CPU with 6-stage pipeline
 * Second iteration of B32P with focus on single cycle execution of all stages
 *
 * Features:
 * - 6 stage pipeline
 *   - FE1:     Instruction Cache Fetch
 *   - FE2:     Instruction Cache Miss Fetch
 *   - REG:     Register Read
 *   - EXMEM1:  Execute and Data Cache Access
 *   - EXMEM2:  Multi-cycle Execute and Data Cache Miss Handling
 *   - WB:      Writeback Register
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

 wire [31:0] icache_q_FE2 = icache_q;

// TODO: Skipped for now, should fetch from memory (via L1i cache) on cache miss
//  and forward either icache_q or the fetched instruction to the next stage

// Forward FE2 to REG
wire [31:0] instr_REG;
Regr #(
    .N(32)
) regr_FE2_REG (
    .clk (clk),
    .in(icache_q),
    .out(instr_REG),
    .hold(1'b0),
    .clear(1'b0)
);

/*
 * Stage 3: Register Read
 */

wire [3:0] addr_a_REG;
wire [3:0] addr_b_REG;

// Obtain register addresses from instruction
InstructionDecoder instrDec_REG (
    .instr(instr_REG),

    .instrOP(),
    .aluOP(),
    .branchOP(),

    .constAlu(),
    .constAluu(),
    .const16(),
    .const16u(),
    .const27(),

    .areg(addr_a_REG),
    .breg(addr_b_REG),
    .dreg(),

    .he(),
    .oe(),
    .sig()
);

// Since the regbank takes a cycle, the result will be in the next (EXMEM1) stage
wire [31:0] data_a_EXMEM1;
wire [31:0] data_b_EXMEM1;

// Signals already defined for WB stage
wire [3:0] addr_d_WB;
wire [31:0] data_d_WB;
wire we_WB;

Regbank regbank (
    .clk(clk),
    .reset(reset),

    .addr_a(addr_a_REG),
    .addr_b(addr_b_REG),
    .clear(1'b0),
    .hold(1'b0),
    .data_a(data_a_EXMEM1),
    .data_b(data_b_EXMEM1),

    .addr_d(addr_d_WB),
    .data_d(data_d_WB),
    .we(we_WB)
);

// Forward to next stage
wire [31:0] instr_EXMEM1;
Regr #(
    .N(32)
) regr_REG_EXMEM1 (
    .clk (clk),
    .in(instr_REG),
    .out(instr_EXMEM1),
    .hold(1'b0),
    .clear(1'b0)
);

/*
 * Stage 4: Execute and Data Cache Access
 */

// Single cycle ALU operations
wire [31:0] alu_a_EXMEM1;
wire [31:0] alu_b_EXMEM1;
wire [3:0]  aluOP_EXMEM1;
wire [31:0] constAlu_EXMEM1;
wire [31:0] constAluu_EXMEM1;
wire [3:0]  instrOP_EXMEM1;
wire        alu_use_const_EXMEM1;
wire [31:0] alu_y_EXMEM1;

InstructionDecoder instrDec_EXMEM1 (
    .instr(instr_EXMEM1),

    .instrOP(instrOP_EXMEM1),
    .aluOP(aluOP_EXMEM1),
    .branchOP(),

    .constAlu(constAlu_EXMEM1),
    .constAluu(constAluu_EXMEM1),
    .const16(),
    .const16u(),
    .const27(),

    .areg(),
    .breg(),
    .dreg(),

    .he(),
    .oe(),
    .sig()
);

ControlUnit constrolUnit_EXMEM1 (
    .instrOP(instrOP_EXMEM1),
    .aluOP(aluOP_EXMEM1),

    .alu_use_const(alu_use_const_EXMEM1),
    .alu_use_constu(alu_use_constu_EXMEM1),
    .push(),
    .pop(),
    .dreg_we(),
    .mem_write(),
    .mem_read(),
    .jumpc(),
    .jumpr(),
    .branch(),
    .halt(),
    .reti(),
    .clearCache()
);

assign alu_a_EXMEM1 = data_a_EXMEM1;

assign alu_b_EXMEM1 =   (alu_use_constu_EXMEM1) ? constAluu_EXMEM1:
                        (alu_use_const_EXMEM1)  ? constAlu_EXMEM1:
                        data_b_EXMEM1;

ALU alu_EXMEM1 (
    .a          (alu_a_EXMEM1),
    .b          (alu_b_EXMEM1),
    .opcode     (aluOP_EXMEM1),
    .y          (alu_y_EXMEM1)
);

// TODO:
// - data cache access
// - branch/jump handling
// - forwarding for ALU inputs

// Forward to next stage
wire [31:0] instr_EXMEM2;
Regr #(
    .N(32)
) regr_instr_EXMEM1_EXMEM2 (
    .clk (clk),
    .in(instr_EXMEM1),
    .out(instr_EXMEM2),
    .hold(1'b0),
    .clear(1'b0)
);

wire [31:0] alu_y_EXMEM2;
Regr #(
    .N(32)
) regr_ALU_EXMEM2 (
    .clk (clk),
    .in(alu_y_EXMEM1),
    .out(alu_y_EXMEM2),
    .hold(1'b0),
    .clear(1'b0)
);

wire [31:0] data_a_EXMEM2;
wire [31:0] data_b_EXMEM2;
Regr #(
    .N(64)
) regr_DATA_AB_EXMEM2 (
    .clk (clk),
    .in({data_a_EXMEM1, data_b_EXMEM1}),
    .out({data_a_EXMEM2, data_b_EXMEM2}),
    .hold(1'b0),
    .clear(1'b0)
);

/*
 * Stage 5: Multi-cycle Execute and Data Cache Miss Handling
 */

wire [3:0] instrOP_EXMEM2;
wire push_EXMEM2;
wire pop_EXMEM2;

InstructionDecoder instrDec_EXMEM2 (
    .instr(instr_EXMEM2),

    .instrOP(instrOP_EXMEM2),
    .aluOP(),
    .branchOP(),

    .constAlu(),
    .constAluu(),
    .const16(),
    .const16u(),
    .const27(),

    .areg(),
    .breg(),
    .dreg(),

    .he(),
    .oe(),
    .sig()
);

ControlUnit constrolUnit_EXMEM2 (
    .instrOP(instrOP_EXMEM2),
    .aluOP(),

    .alu_use_const(),
    .alu_use_constu(),
    .push(push_EXMEM2),
    .pop(pop_EXMEM2),
    .dreg_we(),
    .mem_write(),
    .mem_read(),
    .jumpc(),
    .jumpr(),
    .branch(),
    .halt(),
    .reti(),
    .clearCache()
);

// Since the stack takes a cycle, the result will be in the next (WB) stage
wire [31:0] stack_q_WB;

Stack stack_EXMEM2 (
    .clk(clk),
    .reset(reset),

    .d(data_b_EXMEM2),
    .push(push_EXMEM2),
    .pop(pop_EXMEM2),
    .clear(1'b0),
    .hold(1'b0),
    .q(stack_q_WB)
);

// TODO:
// - data mem access on cache miss
// - multi-cycle ALU operations

// Forward to next stage
wire [31:0] instr_WB;
Regr #(
    .N(32)
) regr_instr_EXMEM2_WB (
    .clk (clk),
    .in(instr_EXMEM2),
    .out(instr_WB),
    .hold(1'b0),
    .clear(1'b0)
);

wire [31:0] alu_y_WB;
Regr #(
    .N(32)
) regr_ALU_EXMEM2_WB (
    .clk (clk),
    .in(alu_y_EXMEM2),
    .out(alu_y_WB),
    .hold(1'b0),
    .clear(1'b0)
);

/*
 * Stage 6: Writeback Register
 */

wire [3:0] instrOP_WB;

InstructionDecoder instrDec_WB (
    .instr(instr_WB),

    .instrOP(instrOP_WB),
    .aluOP(),
    .branchOP(),

    .constAlu(),
    .constAluu(),
    .const16(),
    .const16u(),
    .const27(),

    .areg(),
    .breg(),
    .dreg(addr_d_WB),

    .he(),
    .oe(),
    .sig()
);

ControlUnit constrolUnit_WB (
    .instrOP(instrOP_WB),
    .aluOP(),

    .alu_use_const(),
    .alu_use_constu(),
    .push(),
    .pop(),
    .dreg_we(we_WB),
    .mem_write(),
    .mem_read(),
    .jumpc(),
    .jumpr(),
    .branch(),
    .halt(),
    .reti(),
    .clearCache()
);

endmodule

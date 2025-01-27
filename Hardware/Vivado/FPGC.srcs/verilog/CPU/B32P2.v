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
    output wire       icache_oe,
    input wire [31:0] icache_q
);
    
// Flush and Stall signals
wire flush_FE1;
wire flush_FE2;
wire flush_REG;

assign flush_FE1 = jump_valid_EXMEM1 || reti_EXMEM1 || interrupt_valid;
assign flush_FE2 = jump_valid_EXMEM1 || reti_EXMEM1 || interrupt_valid;
assign flush_REG = jump_valid_EXMEM1 || reti_EXMEM1 || interrupt_valid;

wire stall_FE1;
wire stall_FE2;
wire stall_REG;
wire stall_EXMEM1;

// TODO stall all previous stages on EXMEM2 busy or FE2 busy
assign stall_FE1 = 1'b0;
assign stall_FE2 = 1'b0;
assign stall_REG = 1'b0;
assign stall_EXMEM1 = 1'b0;

// Possible forwarding/hazard situations:
// Read after Write -> Hazard
// EXMEM2 -> EXMEM1 (single cycle ALU operation in EXMEM1)
// WB -> EXMEM1 (single cycle ALU operation in EXMEM1)
wire [1:0] forward_a; // From how many stages ahead to forward from towards ALU input A
wire [1:0] forward_b; // From how many stages ahead to forward from towards ALU input B

// Interrupt signals
wire interrupt_valid;
assign interrupt_valid = 1'b0; // TODO connect to interrupt controller
reg [26:0] PC_backup = 27'd0;

// Program counter updater
reg [26:0] PC = PC_START; 
always @(posedge clk)
begin
    if (reset)
    begin
        PC <= PC_START;
    end
    else
    begin
        if (interrupt_valid)
        begin
            PC <= INTERRUPT_JUMP_ADDR;
        end
        else if (reti_EXMEM1)
        begin
            PC <= PC_backup;
        end
        else if (jump_valid_EXMEM1)
        begin
            PC <= jump_addr_EXMEM1;
        end
        else if (stall_FE1)
        begin
            PC <= PC;
        end
        else
        begin
            PC <= PC + 1'b1;
        end
    end
end


/*
 * Stage 1: Instruction Cache Fetch
 */

// Instruction Cache
assign icache_addr = PC;

// Forward to next stage
wire [26:0] PC_FE2;
Regr #(
    .N(27)
) regr_PC_FE1_FE2 (
    .clk (clk),
    .in(PC),
    .out(PC_FE2),
    .hold(1'b0),
    .clear(flush_FE1)
);

assign icache_oe = !flush_FE1;

/*
 * Stage 2: Instruction Cache Miss Fetch
 */

 wire [31:0] icache_q_FE2 = icache_q;

// TODO: Skipped for now, should fetch from memory (via L1i cache) on cache miss
//  and forward either icache_q or the fetched instruction to the next stage

// Forward to next stage
wire [31:0] instr_REG;
Regr #(
    .N(32)
) regr_FE2_REG (
    .clk (clk),
    .in(icache_q),
    .out(instr_REG),
    .hold(1'b0),
    .clear(flush_FE2)
);

wire [26:0] PC_REG;
Regr #(
    .N(27)
) regr_PC_FE2_REG (
    .clk (clk),
    .in(PC_FE2),
    .out(PC_REG),
    .hold(1'b0),
    .clear(flush_FE2)
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
    .clear(flush_REG)
);

wire [26:0] PC_EXMEM1;
Regr #(
    .N(27)
) regr_PC_REG_EXMEM1 (
    .clk (clk),
    .in(PC_REG),
    .out(PC_EXMEM1),
    .hold(1'b0),
    .clear(flush_REG)
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

// Branch and jump operations
wire [2:0] branchOP_EXMEM1;
wire [31:0] const16_EXMEM1;
wire [26:0] const27_EXMEM1;
wire halt_EXMEM1;
wire reti_EXMEM1;
wire branch_EXMEM1;
wire jumpc_EXMEM1;
wire jumpr_EXMEM1;
wire oe_EXMEM1;
wire sig_EXMEM1;

wire [3:0] areg_EXMEM1;
wire [3:0] breg_EXMEM1;

InstructionDecoder instrDec_EXMEM1 (
    .instr(instr_EXMEM1),

    .instrOP(instrOP_EXMEM1),
    .aluOP(aluOP_EXMEM1),
    .branchOP(branchOP_EXMEM1),

    .constAlu(constAlu_EXMEM1),
    .constAluu(constAluu_EXMEM1),
    .const16(const16_EXMEM1),
    .const16u(),
    .const27(const27_EXMEM1),

    .areg(areg_EXMEM1),
    .breg(breg_EXMEM1),
    .dreg(),

    .he(),
    .oe(oe_EXMEM1),
    .sig(sig_EXMEM1)
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
    .jumpc(jumpc_EXMEM1),
    .jumpr(jumpr_EXMEM1),
    .branch(branch_EXMEM1),
    .halt(halt_EXMEM1),
    .reti(reti_EXMEM1),
    .clearCache()
);

assign forward_a =  (areg_EXMEM1 == dreg_EXMEM2 && we_EXMEM2 && areg_EXMEM1 != 4'd0) ? 2'd1 :
                    (areg_EXMEM1 == addr_d_WB && we_WB && areg_EXMEM1 != 4'd0) ? 2'd2 :
                    2'd0;
assign forward_b =  (breg_EXMEM1 == dreg_EXMEM2 && we_EXMEM2 && breg_EXMEM1 != 4'd0) ? 2'd1 :
                    (breg_EXMEM1 == addr_d_WB && we_WB && breg_EXMEM1 != 4'd0) ? 2'd2 :
                    2'd0;

assign alu_a_EXMEM1 =   (forward_a == 2'd1) ? alu_y_EXMEM2 :
                        (forward_a == 2'd2) ? data_d_WB :
                        data_a_EXMEM1;

assign alu_b_EXMEM1 =   (forward_b == 2'd1) ? alu_y_EXMEM2 :
                        (forward_b == 2'd2) ? data_d_WB :
                        (alu_use_constu_EXMEM1) ? constAluu_EXMEM1:
                        (alu_use_const_EXMEM1)  ? constAlu_EXMEM1:
                        data_b_EXMEM1;

ALU alu_EXMEM1 (
    .a          (alu_a_EXMEM1),
    .b          (alu_b_EXMEM1),
    .opcode     (aluOP_EXMEM1),
    .y          (alu_y_EXMEM1)
);

wire [26:0] jump_addr_EXMEM1;
wire jump_valid_EXMEM1;

BranchJumpUnit branchJumpUnit_EXMEM1 (
    .branchOP(branchOP_EXMEM1),
    .data_a (data_a_EXMEM1),
    .data_b (data_b_EXMEM1),
    .const16(const16_EXMEM1),
    .const27(const27_EXMEM1),
    .pc     (PC_EXMEM1),
    .halt   (halt_EXMEM1),
    .branch (branch_EXMEM1),
    .jumpc  (jumpc_EXMEM1),
    .jumpr  (jumpr_EXMEM1),
    .oe     (oe_EXMEM1),
    .sig    (sig_EXMEM1),
    
    .jump_addr(jump_addr_EXMEM1),
    .jump_valid(jump_valid_EXMEM1)
);

// TODO:
// - data cache access

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

wire [26:0] PC_EXMEM2;
Regr #(
    .N(27)
) regr_PC_EXMEM1_EXMEM2 (
    .clk (clk),
    .in(PC_EXMEM1),
    .out(PC_EXMEM2),
    .hold(1'b0),
    .clear(1'b0)
);

/*
 * Stage 5: Multi-cycle Execute and Data Cache Miss Handling
 */

wire [3:0] instrOP_EXMEM2;
wire push_EXMEM2;
wire pop_EXMEM2;

wire [3:0] dreg_EXMEM2;
wire dreg_we_EXMEM2;

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
    .dreg(dreg_EXMEM2),

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
    .dreg_we(we_EXMEM2),
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

wire [26:0] PC_WB;
Regr #(
    .N(27)
) regr_PC_EXMEM2_WB (
    .clk (clk),
    .in(PC_EXMEM2),
    .out(PC_WB),
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

wire pop_WB;
wire mem_read_WB;

ControlUnit constrolUnit_WB (
    .instrOP(instrOP_WB),
    .aluOP(),

    .alu_use_const(),
    .alu_use_constu(),
    .push(),
    .pop(pop_WB),
    .dreg_we(we_WB),
    .mem_write(),
    .mem_read(mem_read_WB),
    .jumpc(),
    .jumpr(),
    .branch(),
    .halt(),
    .reti(),
    .clearCache()
);

// TODO: add mem_q_WB
wire mem_q_WB;
assign data_d_WB =  (pop_WB) ? stack_q_WB :
                    (mem_read_WB) ? mem_q_WB :
                    alu_y_WB;

endmodule

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
 * - 32 bits, word-addressable only
 * - 32 bit address space for 16GiB of addressable memory
 *   - 27 bits jump constant for 512MiB of easily jumpable instruction memory
 *
 * Notes:
 * - ROM (where the CPU starts executing from) needs to be placed on an address after RAM
 * - Interrupts are only valid on an address before ROM
 */
module B32P2 #(
    parameter ROM_ADDRESS = 32'h7800000, // Initial PC value, so the CPU starts executing from ROM first
    parameter INTERRUPT_JUMP_ADDR = 32'd1 // Address to jump to when an interrupt is triggered
) (
    // Clock and reset
    input wire clk,
    input wire reset,

    // ROM (dual port)
    output wire [8:0] rom_fe_addr,
    output wire rom_fe_oe,
    input wire [31:0] rom_fe_q,
    output wire rom_fe_hold,

    output wire [8:0] rom_mem_addr,
    output wire rom_mem_oe,
    input wire [31:0] rom_mem_q
);
    
// Flush and Stall signals
wire flush_FE1;
wire flush_FE2;
wire flush_REG;
wire flush_EXMEM1;

assign flush_FE1 = jump_valid_EXMEM1 || reti_EXMEM1 || interrupt_valid;
assign flush_FE2 = jump_valid_EXMEM1 || reti_EXMEM1 || interrupt_valid;
assign flush_REG = jump_valid_EXMEM1 || reti_EXMEM1 || interrupt_valid;
assign flush_EXMEM1 = exmem1_uses_exmem2_result;

wire stall_FE1;
wire stall_FE2;
wire stall_REG;
wire stall_EXMEM1;

// TODO stall all previous stages on EXMEM2 busy or FE2 busy
assign stall_FE1 = exmem1_uses_exmem2_result;
assign stall_FE2 = exmem1_uses_exmem2_result;
assign stall_REG = exmem1_uses_exmem2_result;
assign stall_EXMEM1 = 1'b0;

// Possible hazard situations:
// - EXMEM1 uses result of non-ALU operation from EXMEM2 -> stall
wire exmem1_uses_exmem2_result;
assign exmem1_uses_exmem2_result = (pop_EXMEM2 || mem_read_EXMEM2) && (dreg_EXMEM2 == areg_EXMEM1 || dreg_EXMEM2 == breg_EXMEM1);


// Forwarding situations
// EXMEM2 -> EXMEM1 (single cycle ALU operations)
// WB -> EXMEM1 (single cycle ALU operations)
wire [1:0] forward_a; // From how many stages ahead to forward from towards ALU input A
wire [1:0] forward_b; // From how many stages ahead to forward from towards ALU input B

// Interrupt signals
wire interrupt_valid;
assign interrupt_valid = 1'b0; // TODO connect to interrupt controller
reg [31:0] PC_backup = 32'd0;

// Program counter updater
reg [31:0] PC_FE1 = ROM_ADDRESS; 
always @(posedge clk)
begin
    if (reset)
    begin
        PC_FE1 <= ROM_ADDRESS;
    end
    else
    begin
        if (interrupt_valid)
        begin
            PC_FE1 <= INTERRUPT_JUMP_ADDR;
        end
        else if (reti_EXMEM1)
        begin
            PC_FE1 <= PC_backup;
        end
        else if (jump_valid_EXMEM1)
        begin
            PC_FE1 <= jump_addr_EXMEM1;
        end
        else if (stall_FE1)
        begin
            PC_FE1 <= PC_FE1;
        end
        else
        begin
            PC_FE1 <= PC_FE1 + 1'b1;
        end
    end
end


/*
 * Stage 1: Instruction Cache Fetch (+ ROM access)
 */

// Fetch address decoding
wire mem_rom_FE1;
wire [8:0] mem_rom_addr_FE1;
assign mem_rom_FE1 = PC_FE1 >= ROM_ADDRESS;
assign mem_rom_addr_FE1 = PC_FE1 - ROM_ADDRESS;

// ROM access
assign rom_fe_addr = mem_rom_addr_FE1;
assign rom_fe_oe = mem_rom_FE1 && !flush_FE1;
assign rom_fe_hold = stall_FE1;

// Instruction Cache
wire [31:0] icache_addr;
assign icache_addr = PC_FE1;
wire icache_oe;
assign icache_oe = !mem_rom_FE1 && !flush_FE1;

// Forward to next stage
wire [31:0] PC_FE2;
Regr #(
    .N(32)
) regr_PC_FE1_FE2 (
    .clk (clk),
    .in(PC_FE1),
    .out(PC_FE2),
    .hold(stall_FE1),
    .clear(flush_FE1)
);

/*
 * Stage 2: Instruction Cache Miss Fetch
 */

// ROM access
wire mem_rom_FE2;
assign mem_rom_FE2 = PC_FE2 >= ROM_ADDRESS;
wire [31:0] rom_q_FE2 = rom_fe_q;

wire [31:0] icache_q_FE2 = 32'd0; // TODO: connect to instruction cache

// TODO: Skipped for now, should fetch from memory (via L1i cache) on cache miss
//  and forward either icache_q, the fetched instruction or rom_q to the next stage


wire [31:0] instr_result_FE2;
assign instr_result_FE2 = (mem_rom_FE2) ? rom_q_FE2 : icache_q_FE2;

// Forward to next stage
wire [31:0] instr_REG;
Regr #(
    .N(32)
) regr_instr_FE2_REG (
    .clk (clk),
    .in(instr_result_FE2),
    .out(instr_REG),
    .hold(stall_FE2),
    .clear(flush_FE2)
);

wire [31:0] PC_REG;
Regr #(
    .N(32)
) regr_PC_FE2_REG (
    .clk (clk),
    .in(PC_FE2),
    .out(PC_REG),
    .hold(stall_FE2),
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

// Clear on stall as well to create a bubble in the pipeline
Regbank regbank (
    .clk(clk),
    .reset(reset),

    .addr_a(addr_a_REG),
    .addr_b(addr_b_REG),
    .clear(flush_REG),
    .hold(stall_REG),
    .data_a(data_a_EXMEM1),
    .data_b(data_b_EXMEM1),

    .addr_d(addr_d_WB),
    .data_d(data_d_WB),
    .we(we_WB)
);

// Forward to next stage
// Clear on stall as well to create a bubble in the pipeline
wire [31:0] instr_EXMEM1;
Regr #(
    .N(32)
) regr_REG_EXMEM1 (
    .clk (clk),
    .in(instr_REG),
    .out(instr_EXMEM1),
    .hold(stall_REG),
    .clear(flush_REG)
);

wire [31:0] PC_EXMEM1;
Regr #(
    .N(32)
) regr_PC_REG_EXMEM1 (
    .clk (clk),
    .in(PC_REG),
    .out(PC_EXMEM1),
    .hold(stall_REG),
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

wire mem_read_EXMEM1;
wire mem_write_EXMEM1;

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
    .mem_write(mem_write_EXMEM1),
    .mem_read(mem_read_EXMEM1),
    .jumpc(jumpc_EXMEM1),
    .jumpr(jumpr_EXMEM1),
    .branch(branch_EXMEM1),
    .halt(halt_EXMEM1),
    .reti(reti_EXMEM1),
    .clearCache()
);

wire mem_sdram_EXMEM1;
wire [31:0] mem_local_address_EXMEM1;

// TODO optimization in case address calculations are slow:
// Calculate all signals in EXMEM1 and forward them to EXMEM2
// This way the address decoding does not need to be done in EXMEM2
AddressDecoder addressDecoder_EXMEM1 (
    .areg_value(alu_a_EXMEM1), // Use forwarded data
    .const16(const16_EXMEM1),
    .rw(mem_read_EXMEM1 || mem_write_EXMEM1),

    .mem_sdram(mem_sdram_EXMEM1),
    .mem_sdcard(),
    .mem_spiflash(),
    .mem_io(),
    .mem_rom(),
    .mem_vram32(),
    .mem_vram8(),
    .mem_vrampx(),

    .mem_multicycle(),
    .mem_local_address(mem_local_address_EXMEM1)
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

wire [31:0] jump_addr_EXMEM1;
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
// - in case of mem_read or mem_write, read from l1d cache using mem_local_address_EXMEM1

// Forward to next stage
wire [31:0] instr_EXMEM2;
Regr #(
    .N(32)
) regr_instr_EXMEM1_EXMEM2 (
    .clk (clk),
    .in(instr_EXMEM1),
    .out(instr_EXMEM2),
    .hold(stall_EXMEM1),
    .clear(flush_EXMEM1)
);

wire [31:0] alu_y_EXMEM2;
Regr #(
    .N(32)
) regr_ALU_EXMEM2 (
    .clk (clk),
    .in(alu_y_EXMEM1),
    .out(alu_y_EXMEM2),
    .hold(stall_EXMEM1),
    .clear(flush_EXMEM1)
);

// Here the ALU inputs are used to include forwarded data
wire [31:0] data_a_EXMEM2;
wire [31:0] data_b_EXMEM2;
Regr #(
    .N(64)
) regr_DATA_AB_EXMEM2 (
    .clk (clk),
    .in({alu_a_EXMEM1, alu_b_EXMEM1}),
    .out({data_a_EXMEM2, data_b_EXMEM2}),
    .hold(stall_EXMEM1),
    .clear(flush_EXMEM1)
);

wire [31:0] PC_EXMEM2;
Regr #(
    .N(32)
) regr_PC_EXMEM1_EXMEM2 (
    .clk (clk),
    .in(PC_EXMEM1),
    .out(PC_EXMEM2),
    .hold(stall_EXMEM1),
    .clear(flush_EXMEM1)
);

/*
 * Stage 5: Multi-cycle Execute and Data Cache Miss Handling
 */

wire [3:0] instrOP_EXMEM2;
wire push_EXMEM2;
wire pop_EXMEM2;

wire [3:0] dreg_EXMEM2;
wire dreg_we_EXMEM2;

wire mem_read_EXMEM2;
wire mem_write_EXMEM2;
wire [31:0] const16_EXMEM2;

InstructionDecoder instrDec_EXMEM2 (
    .instr(instr_EXMEM2),

    .instrOP(instrOP_EXMEM2),
    .aluOP(),
    .branchOP(),

    .constAlu(),
    .constAluu(),
    .const16(const16_EXMEM2),
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
    .mem_write(mem_write_EXMEM2),
    .mem_read(mem_read_EXMEM2),
    .jumpc(),
    .jumpr(),
    .branch(),
    .halt(),
    .reti(),
    .clearCache()
);

wire mem_multicycle_EXMEM2;
wire [31:0] mem_local_address_EXMEM2;

wire mem_sdram_EXMEM2;
wire mem_sdcard_EXMEM2;
wire mem_spiflash_EXMEM2;
wire mem_io_EXMEM2;
wire mem_rom_EXMEM2;
wire mem_vram32_EXMEM2;
wire mem_vram8_EXMEM2;
wire mem_vrampx_EXMEM2;

AddressDecoder addressDecoder_EXMEM2 (
    .areg_value(data_a_EXMEM2),
    .const16(const16_EXMEM2),
    .rw(mem_read_EXMEM2 || mem_write_EXMEM2),

    .mem_sdram(mem_sdram_EXMEM2),
    .mem_sdcard(mem_sdcard_EXMEM2),
    .mem_spiflash(mem_spiflash_EXMEM2),
    .mem_io(mem_io_EXMEM2),
    .mem_rom(mem_rom_EXMEM2),
    .mem_vram32(mem_vram32_EXMEM2),
    .mem_vram8(mem_vram8_EXMEM2),
    .mem_vrampx(mem_vrampx_EXMEM2),

    .mem_multicycle(mem_multicycle_EXMEM2),
    .mem_local_address(mem_local_address_EXMEM2)
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

// ROM
assign rom_mem_addr = mem_local_address_EXMEM2;
assign rom_mem_oe = mem_rom_EXMEM2;

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

wire [31:0] PC_WB;
Regr #(
    .N(32)
) regr_PC_EXMEM2_WB (
    .clk (clk),
    .in(PC_EXMEM2),
    .out(PC_WB),
    .hold(1'b0),
    .clear(1'b0)
);

wire [31:0] data_a_WB;
Regr #(
    .N(32)
) regr_DATA_A_EXMEM2_WB (
    .clk (clk),
    .in(data_a_EXMEM2),
    .out(data_a_WB),
    .hold(1'b0),
    .clear(1'b0)
);

/*
 * Stage 6: Writeback Register
 */

wire [3:0] instrOP_WB;
wire [31:0] const16_WB;

InstructionDecoder instrDec_WB (
    .instr(instr_WB),

    .instrOP(instrOP_WB),
    .aluOP(),
    .branchOP(),

    .constAlu(),
    .constAluu(),
    .const16(const16_WB),
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

// Re-use address decoder to determine from which memory result to use in data_d
wire mem_sdram_WB;
wire mem_sdcard_WB;
wire mem_spiflash_WB;
wire mem_io_WB;
wire mem_rom_WB;
wire mem_vram32_WB;
wire mem_vram8_WB;
wire mem_vrampx_WB;

AddressDecoder addressDecoder_WB (
    .areg_value(data_a_WB),
    .const16(const16_WB),
    .rw(mem_read_WB),

    .mem_sdram(mem_sdram_WB),
    .mem_sdcard(mem_sdcard_WB),
    .mem_spiflash(mem_spiflash_WB),
    .mem_io(mem_io_WB),
    .mem_rom(mem_rom_WB),
    .mem_vram32(mem_vram32_WB),
    .mem_vram8(mem_vram8_WB),
    .mem_vrampx(mem_vrampx_WB),

    .mem_multicycle(),
    .mem_local_address()
);

assign data_d_WB =  (pop_WB) ? stack_q_WB :
                    (mem_rom_WB) ? rom_mem_q :
                    alu_y_WB;

endmodule

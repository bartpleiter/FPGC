/*
 * B32P3
 * 32-bit pipelined CPU with classic 5-stage pipeline
 * Third iteration of B32P with focus on simplified hazard handling
 *
 * Features:
 * - 5 stage pipeline (Classic MIPS-style)
 *   - IF:  Instruction Fetch
 *   - ID:  Instruction Decode & Register Read
 *   - EX:  Execute (ALU operations, branch resolution)
 *   - MEM: Memory Access (Load/Store)
 *   - WB:  Write Back
 * - Simple hazard detection (load-use only)
 * - Data forwarding (EX→EX, MEM→EX)
 * - 32 bits, word-addressable only
 * - 32 bit address space for 16GiB of addressable memory
 *   - 27 bits jump constant for 512MiB of easily jumpable instruction memory
 *
 * Memory Map:
 * - SDRAM:  0x0000000 - 0x6FFFFFF (112MiW)
 * - I/O:    0x7000000 - 0x77FFFFF
 * - ROM:    0x7800000 - 0x78003FF (1KiW) - CPU starts here
 * - VRAM32: 0x7900000 - 0x790041F
 * - VRAM8:  0x7A00000 - 0x7A02001
 * - VRAMPX: 0x7B00000 - 0x7B12BFF
 *
 * Notes:
 * - ROM (where the CPU starts executing from) needs to be placed on an address after RAM
 * - Interrupts are only valid on an address before ROM
 */
module B32P3 #(
    parameter ROM_ADDRESS = 32'h7800000, // Initial PC value, so the CPU starts executing from ROM first
    parameter INTERRUPT_JUMP_ADDR = 32'd1, // Address to jump to when an interrupt is triggered
    parameter NUM_INTERRUPTS = 8 // Number of interrupt lines
) (
    //========================
    // System interface
    //========================
    input  wire         clk,
    input  wire         reset,

    //========================
    // ROM (dual port)
    //========================
    output wire [9:0]   rom_fe_addr,
    output wire         rom_fe_oe,
    input  wire [31:0]  rom_fe_q,
    output wire         rom_fe_hold,

    output wire [9:0]   rom_mem_addr,
    input  wire [31:0]  rom_mem_q,

    //========================
    // VRAM32
    //========================
    output wire [10:0]  vram32_addr,
    output wire [31:0]  vram32_d,
    output wire         vram32_we,
    input  wire [31:0]  vram32_q,

    //========================
    // VRAM8
    //========================
    output wire [13:0]  vram8_addr,
    output wire [7:0]   vram8_d,
    output wire         vram8_we,
    input  wire [7:0]   vram8_q,

    //========================
    // VRAMPX
    //========================
    output wire [16:0]  vramPX_addr,
    output wire [7:0]   vramPX_d,
    output wire         vramPX_we,
    input  wire [7:0]   vramPX_q,

    //========================
    // L1i cache (CPU pipeline port)
    // Cache line format: {256bit_data, 14bit_tag, 1bit_valid} = 271 bits
    //========================
    output wire [6:0]   l1i_pipe_addr,
    input  wire [270:0] l1i_pipe_q,

    //========================
    // L1d cache (CPU pipeline port)
    // Cache line format: {256bit_data, 14bit_tag, 1bit_valid} = 271 bits
    //========================
    output wire [6:0]   l1d_pipe_addr,
    input  wire [270:0] l1d_pipe_q,

    //========================
    // Cache controller
    //========================
    output wire [31:0]  l1i_cache_controller_addr,
    output wire         l1i_cache_controller_start,
    output wire         l1i_cache_controller_flush,
    input  wire         l1i_cache_controller_done,
    input  wire [31:0]  l1i_cache_controller_result,

    output wire [31:0]  l1d_cache_controller_addr,
    output wire [31:0]  l1d_cache_controller_data,
    output wire         l1d_cache_controller_we,
    output wire         l1d_cache_controller_start,
    input  wire         l1d_cache_controller_done,
    input  wire [31:0]  l1d_cache_controller_result, 

    output reg          l1_clear_cache = 1'b0,
    input wire          l1_clear_cache_done,

    //========================
    // Memory Unit
    //========================
    output reg          mu_start = 1'b0,
    output reg [31:0]   mu_addr = 32'd0,
    output reg [31:0]   mu_data = 32'd0,
    output reg          mu_we = 1'b0,
    input  wire [31:0]  mu_q,
    input  wire         mu_done,

    //========================
    // Interrupts
    //========================
    input wire [NUM_INTERRUPTS-1:0] interrupts
);

// =============================================================================
// INSTRUCTION OPCODES
// =============================================================================
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

// ALU Opcodes
localparam 
    ALU_OR      = 4'b0000,
    ALU_AND     = 4'b0001,
    ALU_XOR     = 4'b0010,
    ALU_ADD     = 4'b0011,
    ALU_SUB     = 4'b0100,
    ALU_SHIFTL  = 4'b0101,
    ALU_SHIFTR  = 4'b0110,
    ALU_NOTA    = 4'b0111,
    ALU_SLT     = 4'b1010,
    ALU_SLTU    = 4'b1011,
    ALU_LOAD    = 4'b1100,
    ALU_LOADHI  = 4'b1101,
    ALU_SHIFTRS = 4'b1110;

// =============================================================================
// INTERRUPT CONTROLLER
// =============================================================================
// Interrupt enable/disable flag (disabled during interrupt handling)
reg int_disabled = 1'b0;

// Interrupt controller signals
wire int_cpu;           // Interrupt request from controller
wire [7:0] int_id;      // Interrupt ID (1-8)

InterruptController interrupt_controller (
    .clk        (clk),
    .reset      (reset),
    .interrupts (interrupts),
    .intDisabled(int_disabled),
    .intCPU     (int_cpu),
    .intID      (int_id)
);

// Interrupt is valid only when:
// 1. Interrupt controller has a pending interrupt
// 2. Interrupts are not disabled
// 3. We're executing from SDRAM (PC < ROM_ADDRESS) - not from ROM
// 4. A jump is happening (simplifies hazard handling)
wire interrupt_valid = int_cpu && !int_disabled &&
                       (ex_mem_pc < ROM_ADDRESS) &&
                       (ex_mem_valid && jump_valid);

// PC backup for return from interrupt
reg [31:0] pc_backup = 32'd0;

// RETI signal from EX stage (return from interrupt)
wire reti_valid = id_ex_valid && id_ex_reti;

// =============================================================================
// PIPELINE CONTROL SIGNALS
// =============================================================================
wire stall_if;          // Stall IF stage
wire stall_id;          // Stall ID stage  
wire flush_if_id;       // Flush IF/ID register (insert bubble)
wire flush_id_ex;       // Flush ID/EX register (insert bubble)
wire flush_ex_mem;      // Flush EX/MEM register (for MEM-stage branch resolution)

// Pipeline stall sources
wire hazard_stall;      // Load-use hazard stall
wire cache_stall_if;    // L1I cache miss stall (combinatorial - from BRAM tag compare)
wire cache_stall_mem;   // L1D cache miss stall
wire multicycle_stall;  // Multi-cycle ALU stall
wire mu_stall;          // Memory unit stall
wire cc_stall;          // Cache clear stall

// TIMING OPTIMIZATION: Register cache_stall_if to break the critical path
// from L1I BRAM output -> l1i_hit -> cache_stall_if -> backend_stall -> PC
// The registered version is used for stall signals, while the combinatorial
// version is used for cache controller start (needs immediate response).
reg cache_stall_if_reg = 1'b0;
always @(posedge clk) begin
    if (reset)
        cache_stall_if_reg <= 1'b0;
    else
        cache_stall_if_reg <= cache_stall_if;
end

// Combined stall signal - stalls entire pipeline (for back-end stalls)
// Uses REGISTERED cache_stall_if to break critical timing path
wire backend_stall = cache_stall_if_reg || cache_stall_mem || multicycle_stall || mu_stall || cc_stall;

// Cache line hazard also needs to stall EX (declared in hazard section below, forward declare here)
wire cache_line_hazard;

// Pipeline stall for front-end (IF, ID) - includes all hazard stalls
wire pipeline_stall = hazard_stall || backend_stall;

// Pipeline stall for EX stage - includes cache_line_hazard and backend_stall
// Load/pop hazards don't stall EX because EX instruction needs to wait in ID for the result
wire ex_pipeline_stall = backend_stall || cache_line_hazard;

// Pipeline stall for MEM and WB stages - only backend_stall
// cache_line_hazard should NOT stall MEM/WB - we need MEM to complete so the hazard clears
wire backend_pipeline_stall = backend_stall;

assign stall_if = pipeline_stall;
assign stall_id = pipeline_stall;

// PC redirect signals (from MEM stage - branch resolution moved from EX)
wire        pc_redirect;
wire [31:0] pc_redirect_target;

// Flush on control hazard (branch/jump taken in MEM stage)
// Since branch resolution is now in MEM stage, we need to flush 3 stages:
// IF/ID, ID/EX, and EX/MEM (the 2 instructions after the branch in the pipeline)
// Also flush on interrupt_valid and reti_valid
assign flush_if_id = pc_redirect || interrupt_valid || reti_valid;
assign flush_id_ex = pc_redirect || interrupt_valid || reti_valid;
assign flush_ex_mem = pc_redirect || interrupt_valid || reti_valid;

// =============================================================================
// PROGRAM COUNTER
// =============================================================================
reg [31:0] pc = ROM_ADDRESS;

// Delayed PC to match instruction fetch latency
// ROM and cache have 1-cycle latency, so we need to track the PC
// that corresponds to the instruction data arriving this cycle
reg [31:0] pc_delayed = ROM_ADDRESS;

// Track when we've just redirected and need to discard the next ROM output
// When pc_redirect happens, the ROM was already fetching the old PC's instruction,
// so we need to wait one more cycle before the correct instruction arrives
reg redirect_pending = 1'b0;

always @(posedge clk) begin
    if (reset) begin
        pc <= ROM_ADDRESS;
        pc_delayed <= ROM_ADDRESS;
        redirect_pending <= 1'b0;
        int_disabled <= 1'b0;
        pc_backup <= 32'd0;
    end else if (interrupt_valid && !pipeline_stall) begin
        // Interrupt: save PC and jump to interrupt handler
        // Note: Interrupt should only be taken when not stalled
        int_disabled <= 1'b1;
        pc_backup <= ex_mem_pc;  // Save PC of instruction in MEM stage (matching jump_valid)
        $display("Interrupt taken, jumping to address %h, saving PC %h", INTERRUPT_JUMP_ADDR, ex_mem_pc);
        pc <= INTERRUPT_JUMP_ADDR;
        redirect_pending <= 1'b1;
    end else if (reti_valid && !pipeline_stall) begin
        // Return from interrupt: restore PC and re-enable interrupts
        // Note: RETI should only complete when not stalled
        int_disabled <= 1'b0;
        pc <= pc_backup;
        redirect_pending <= 1'b1;
    end else if (pc_redirect) begin
        // Jump/branch redirect - this takes priority over stalls!
        // When a jump executes, we MUST redirect the PC even if there's a cache stall
        // The cache stall was for a different (now stale) instruction
        pc <= pc_redirect_target;
        // Don't set pc_delayed yet - the memory is still outputting the old instruction
        redirect_pending <= 1'b1;
    end else if (!pipeline_stall) begin
        if (redirect_pending) begin
            // First cycle after redirect: update pc_delayed, check if we can proceed
            // The cache was queried with pc (the new target) in the previous cycle,
            // so on this cycle the cache output corresponds to pc, not pc_delayed.
            // Use l1i_hit_redirect which checks using pc instead of pc_delayed.
            pc_delayed <= pc;
            
            if (pc >= ROM_ADDRESS || l1i_hit_redirect) begin
                // ROM access or cache hit: proceed normally
                pc <= pc + 32'd1;
                redirect_pending <= 1'b0;
            end else begin
                // SDRAM access with cache miss: stay at current PC, wait for cache
                // Keep redirect_pending HIGH to continue flushing the pipeline
                redirect_pending <= 1'b1;
            end
        end else if (cache_stall_if) begin
            // Cache miss during normal operation - hold PC and wait
            // pc_delayed is already correct, just wait for cache fill
            // Don't advance pc, don't update pc_delayed
        end else begin
            pc_delayed <= pc;  // Save current PC before incrementing
            pc <= pc + 32'd1;
        end
    end
    // On stall: hold current PC
end

// =============================================================================
// IF/ID PIPELINE REGISTER
// =============================================================================
reg [31:0] if_id_pc = 32'd0;
reg [31:0] if_id_instr = 32'd0;
reg        if_id_valid = 1'b0;

// =============================================================================
// ID/EX PIPELINE REGISTER  
// =============================================================================
reg [31:0] id_ex_pc = 32'd0;
reg [31:0] id_ex_instr = 32'd0;
reg        id_ex_valid = 1'b0;

// NOTE: Register file data (areg_data, breg_data) comes directly from regbank
// with 1-cycle latency - we don't pipeline these values

// Decoded fields
reg [3:0]  id_ex_dreg = 4'd0;
reg [3:0]  id_ex_areg = 4'd0;
reg [3:0]  id_ex_breg = 4'd0;
reg [3:0]  id_ex_instr_op = 4'd0;
reg [3:0]  id_ex_alu_op = 4'd0;
reg [2:0]  id_ex_branch_op = 3'd0;
reg [31:0] id_ex_const_alu = 32'd0;
reg [26:0] id_ex_const27 = 27'd0;
reg [31:0] id_ex_const16 = 32'd0;
reg        id_ex_he = 1'b0;
reg        id_ex_oe = 1'b0;
reg        id_ex_sig = 1'b0;

// Control signals
reg        id_ex_alu_use_const = 1'b0;
reg        id_ex_dreg_we = 1'b0;
reg        id_ex_mem_read = 1'b0;
reg        id_ex_mem_write = 1'b0;
reg        id_ex_is_branch = 1'b0;
reg        id_ex_is_jump = 1'b0;
reg        id_ex_is_jumpr = 1'b0;
reg        id_ex_push = 1'b0;
reg        id_ex_pop = 1'b0;
reg        id_ex_halt = 1'b0;
reg        id_ex_reti = 1'b0;
reg        id_ex_getIntID = 1'b0;
reg        id_ex_getPC = 1'b0;
reg        id_ex_clearCache = 1'b0;
reg        id_ex_arithm = 1'b0;

// =============================================================================
// EX/MEM PIPELINE REGISTER
// =============================================================================
reg [31:0] ex_mem_pc = 32'd0;
reg [31:0] ex_mem_instr = 32'd0;
reg        ex_mem_valid = 1'b0;

// ALU result
reg [31:0] ex_mem_alu_result = 32'd0;
reg [31:0] ex_mem_breg_data = 32'd0; // For store operations

// Decoded fields
reg [3:0]  ex_mem_dreg = 4'd0;
reg [3:0]  ex_mem_instr_op = 4'd0;

// Memory address
reg [31:0] ex_mem_mem_addr = 32'd0;

// Control signals
reg        ex_mem_dreg_we = 1'b0;
reg        ex_mem_mem_read = 1'b0;
reg        ex_mem_mem_write = 1'b0;
reg        ex_mem_push = 1'b0;
reg        ex_mem_pop = 1'b0;
reg        ex_mem_halt = 1'b0;
reg        ex_mem_getIntID = 1'b0;
reg        ex_mem_getPC = 1'b0;
reg        ex_mem_clearCache = 1'b0;
reg        ex_mem_arithm = 1'b0;

// Branch/jump control signals (for MEM-stage branch resolution)
// This moves branch resolution from EX to MEM to break the critical path:
// forwarding_mux -> comparator -> jump_valid -> flush -> registers
reg        ex_mem_is_branch = 1'b0;
reg        ex_mem_is_jump = 1'b0;
reg        ex_mem_is_jumpr = 1'b0;
reg [2:0]  ex_mem_branch_op = 3'd0;
reg        ex_mem_sig = 1'b0;
reg        ex_mem_oe = 1'b0;
reg [31:0] ex_mem_const16 = 32'd0;
reg [26:0] ex_mem_const27 = 27'd0;
reg [31:0] ex_mem_areg_data = 32'd0;  // Forwarded A value for branch comparison

// =============================================================================
// MEM/WB PIPELINE REGISTER
// =============================================================================
reg [31:0] mem_wb_pc = 32'd0;
reg [31:0] mem_wb_instr = 32'd0;
reg        mem_wb_valid = 1'b0;

// Results
reg [31:0] mem_wb_alu_result = 32'd0;
reg [31:0] mem_wb_mem_data = 32'd0;
reg [31:0] mem_wb_stack_data = 32'd0;

// Pre-computed writeback data (registered at end of MEM stage)
// This breaks the critical path by pre-selecting the result source
reg [31:0] mem_wb_result = 32'd0;

// Decoded fields
reg [3:0]  mem_wb_dreg = 4'd0;
reg [3:0]  mem_wb_instr_op = 4'd0;

// Control signals
reg        mem_wb_dreg_we = 1'b0;
reg        mem_wb_mem_read = 1'b0;
reg        mem_wb_pop = 1'b0;
reg        mem_wb_halt = 1'b0;

// =============================================================================
// INSTRUCTION DECODER
// =============================================================================
wire [3:0]  id_instr_op;
wire [3:0]  id_alu_op;
wire [2:0]  id_branch_op;
wire [31:0] id_const_alu;
wire [31:0] id_const_aluu;
wire [31:0] id_const16;
wire [15:0] id_const16u;
wire [26:0] id_const27;
wire [3:0]  id_areg;
wire [3:0]  id_breg;
wire [3:0]  id_dreg;
wire        id_he;
wire        id_oe;
wire        id_sig;

InstructionDecoder instr_decoder (
    .instr      (if_id_instr),
    .instrOP    (id_instr_op),
    .aluOP      (id_alu_op),
    .branchOP   (id_branch_op),
    .constAlu   (id_const_alu),
    .constAluu  (id_const_aluu),
    .const16    (id_const16),
    .const16u   (id_const16u),
    .const27    (id_const27),
    .areg       (id_areg),
    .breg       (id_breg),
    .dreg       (id_dreg),
    .he         (id_he),
    .oe         (id_oe),
    .sig        (id_sig)
);

// =============================================================================
// CONTROL UNIT
// =============================================================================
wire id_alu_use_const;
wire id_alu_use_constu;
wire id_push;
wire id_pop;
wire id_dreg_we;
wire id_mem_write;
wire id_mem_read;
wire id_arithm;
wire id_jumpc;
wire id_jumpr;
wire id_branch;
wire id_halt;
wire id_reti;
wire id_getIntID;
wire id_getPC;
wire id_clearCache;

ControlUnit control_unit (
    .instrOP        (id_instr_op),
    .aluOP          (id_alu_op),
    .alu_use_const  (id_alu_use_const),
    .alu_use_constu (id_alu_use_constu),
    .push           (id_push),
    .pop            (id_pop),
    .dreg_we        (id_dreg_we),
    .mem_write      (id_mem_write),
    .mem_read       (id_mem_read),
    .arithm         (id_arithm),
    .jumpc          (id_jumpc),
    .jumpr          (id_jumpr),
    .branch         (id_branch),
    .halt           (id_halt),
    .reti           (id_reti),
    .getIntID       (id_getIntID),
    .getPC          (id_getPC),
    .clearCache     (id_clearCache)
);

// =============================================================================
// REGISTER FILE
// =============================================================================
// The regbank has 2-cycle read latency (fully registered for timing):
// - Cycle 1 (IF): Addresses are captured from if_instr
// - Cycle 2 (ID): Data is read from register array and registered
// - Cycle 3 (EX): Data is available at outputs
// We send addresses from IF stage using if_instr, and receive data in EX stage

// Extract register addresses from IF instruction (same logic as InstructionDecoder)
// These go directly to the regbank without any decoding delay
wire [3:0] if_instr_op = if_instr[31:28];
wire [3:0] if_areg = (if_instr_op == 4'b0001 || if_instr_op == 4'b0011) ? if_instr[7:4] : if_instr[11:8];
wire [3:0] if_breg = (if_instr_op == 4'b0001 || if_instr_op == 4'b0011) ? 4'd0 : if_instr[7:4];

wire [31:0] ex_areg_data;  // Data arrives in EX stage
wire [31:0] ex_breg_data;  // Data arrives in EX stage

// WB stage signals for register write
wire [3:0]  wb_dreg;
wire [31:0] wb_data;
wire        wb_dreg_we;

Regbank regbank (
    .clk        (clk),
    .reset      (reset),
    
    // Read ports - addresses from IF stage, data available in EX stage (2-cycle latency)
    .addr_a     (if_areg),
    .addr_b     (if_breg),
    .clear      (flush_if_id),
    .hold       (stall_if),
    .data_a     (ex_areg_data),  // Output arrives 2 cycles later (in EX stage)
    .data_b     (ex_breg_data),  // Output arrives 2 cycles later (in EX stage)
    
    // Write port (WB stage)
    .addr_d     (wb_dreg),
    .data_d     (wb_data),
    .we         (wb_dreg_we)
);

// =============================================================================
// ALU
// =============================================================================
wire [31:0] ex_alu_a;
wire [31:0] ex_alu_b;
wire [31:0] ex_alu_result;

// Forwarding mux for ALU input A
// Data comes directly from regbank (1-cycle latency handled by regbank)
wire [1:0] forward_a;
assign ex_alu_a = (forward_a == 2'b01) ? ex_mem_alu_result :
                  (forward_a == 2'b10) ? wb_data :
                  ex_areg_data;  // Use regbank output directly

// Forwarding mux for ALU input B (before const selection)
wire [1:0] forward_b;
wire [31:0] ex_breg_forwarded;
assign ex_breg_forwarded = (forward_b == 2'b01) ? ex_mem_alu_result :
                           (forward_b == 2'b10) ? wb_data :
                           ex_breg_data;  // Use regbank output directly

// ALU source B: register or constant
assign ex_alu_b = id_ex_alu_use_const ? id_ex_const_alu : ex_breg_forwarded;

ALU alu (
    .a      (ex_alu_a),
    .b      (ex_alu_b),
    .opcode (id_ex_alu_op),
    .y      (ex_alu_result)
);

// =============================================================================
// MULTI-CYCLE ALU (Division, Multiplication) - STATE MACHINE
// =============================================================================
// State machine to control multi-cycle ALU operations
// Ensures start signal is pulsed only once per operation
localparam MALU_IDLE    = 2'b00;
localparam MALU_STARTED = 2'b01;
localparam MALU_DONE    = 2'b10;

reg [1:0] malu_state = MALU_IDLE;
reg       malu_request_finished = 1'b0;
reg [31:0] malu_result_reg = 32'd0;

reg [31:0] malu_a_reg = 32'd0;
reg [31:0] malu_b_reg = 32'd0;
reg [3:0]  malu_opcode_reg = 4'd0;
reg        malu_start_reg = 1'b0;

wire [31:0] malu_result;
wire        malu_done;

// Stall pipeline while multi-cycle ALU is in progress
assign multicycle_stall = id_ex_valid && id_ex_arithm && !malu_request_finished;

MultiCycleALU multi_cycle_alu (
    .clk        (clk),
    .reset      (reset),
    .start      (malu_start_reg),
    .a          (malu_a_reg),
    .b          (malu_b_reg),
    .opcode     (malu_opcode_reg),
    .y          (malu_result),
    .done       (malu_done)
);

// Multi-cycle ALU state machine
always @(posedge clk) begin
    if (reset) begin
        malu_state <= MALU_IDLE;
        malu_start_reg <= 1'b0;
        malu_a_reg <= 32'd0;
        malu_b_reg <= 32'd0;
        malu_opcode_reg <= 4'd0;
        malu_request_finished <= 1'b0;
        malu_result_reg <= 32'd0;
    end else begin
        case (malu_state)
            MALU_IDLE: begin
                malu_request_finished <= 1'b0;
                malu_result_reg <= 32'd0;
                
                // Start when we have a valid arithm instruction that hasn't been processed
                if (id_ex_valid && id_ex_arithm && !malu_request_finished) begin
                    malu_start_reg <= 1'b1;
                    malu_a_reg <= ex_alu_a;
                    malu_b_reg <= ex_alu_b;
                    malu_opcode_reg <= id_ex_alu_op;
                    malu_state <= MALU_STARTED;
                end
            end
            
            MALU_STARTED: begin
                // Clear start signal after one cycle
                malu_start_reg <= 1'b0;
                malu_a_reg <= 32'd0;
                malu_b_reg <= 32'd0;
                malu_opcode_reg <= 4'd0;
                malu_state <= MALU_DONE;
            end
            
            MALU_DONE: begin
                // Wait for multi-cycle ALU to complete
                if (malu_done) begin
                    malu_result_reg <= malu_result;
                    malu_request_finished <= 1'b1;
                    malu_state <= MALU_IDLE;
                end
            end
        endcase
    end
end

// =============================================================================
// BRANCH/JUMP UNIT (MEM Stage)
// =============================================================================
// Branch resolution is done in MEM stage to break the critical path:
// forwarding_mux -> comparator -> jump_valid -> flush -> registers
// This increases branch penalty from 1 to 2 cycles but allows 100MHz timing
wire        jump_valid;
wire [31:0] jump_addr;

// Pre-computed addresses for branch/jump using MEM-stage values
wire [31:0] pre_jump_const_addr_mem = ex_mem_oe ? (ex_mem_pc + {5'b0, ex_mem_const27}) : {5'b0, ex_mem_const27};
wire [31:0] pre_branch_addr_mem = ex_mem_pc + ex_mem_const16;

BranchJumpUnit branch_jump_unit (
    .branchOP           (ex_mem_branch_op),
    .data_a             (ex_mem_areg_data),  // Registered forwarded A value
    .data_b             (ex_mem_breg_data),  // Registered forwarded B value
    .const16            (ex_mem_const16),
    .pc                 (ex_mem_pc),
    .halt               (ex_mem_halt),
    .branch             (ex_mem_is_branch),
    .jumpc              (ex_mem_is_jump),
    .jumpr              (ex_mem_is_jumpr),
    .oe                 (ex_mem_oe),
    .sig                (ex_mem_sig),
    .pre_jump_const_addr(pre_jump_const_addr_mem),
    .pre_branch_addr    (pre_branch_addr_mem),
    .jump_addr          (jump_addr),
    .jump_valid         (jump_valid)
);

// PC redirect logic (now from MEM stage)
assign pc_redirect = ex_mem_valid && jump_valid;
assign pc_redirect_target = jump_addr;

// =============================================================================
// STACK
// =============================================================================
wire [31:0] stack_q;
wire        stack_push;
wire        stack_pop;

assign stack_push = ex_mem_valid && ex_mem_push && !backend_pipeline_stall;
assign stack_pop = ex_mem_valid && ex_mem_pop && !backend_pipeline_stall;

Stack stack (
    .clk    (clk),
    .reset  (reset),
    .d      (ex_mem_breg_data),
    .q      (stack_q),
    .push   (stack_push),
    .pop    (stack_pop),
    .clear  (1'b0),
    .hold   (backend_pipeline_stall)
);

// =============================================================================
// FORWARDING UNIT
// =============================================================================
// Forward from EX/MEM (most recent) or MEM/WB (older)
// forward_a/b: 00=no forward, 01=from EX/MEM, 10=from MEM/WB
// IMPORTANT: Don't forward from EX/MEM for loads or pops - their data isn't ready yet!
// Load/pop data is only available in MEM/WB stage (handled by load_use_hazard stall)
wire ex_mem_can_forward = ex_mem_dreg_we && ex_mem_dreg != 4'd0 && !ex_mem_mem_read && !ex_mem_pop;

assign forward_a = (ex_mem_can_forward && ex_mem_dreg == id_ex_areg) ? 2'b01 :
                   (mem_wb_dreg_we && mem_wb_dreg != 4'd0 && mem_wb_dreg == id_ex_areg) ? 2'b10 :
                   2'b00;

assign forward_b = (ex_mem_can_forward && ex_mem_dreg == id_ex_breg) ? 2'b01 :
                   (mem_wb_dreg_we && mem_wb_dreg != 4'd0 && mem_wb_dreg == id_ex_breg) ? 2'b10 :
                   2'b00;

// =============================================================================
// HAZARD DETECTION UNIT
// =============================================================================
// Load-use hazard: instruction in EX needs data from load in MEM
// POP-use hazard: instruction in EX needs data from pop in MEM (pop result not available until WB)
// We need to stall when ID/EX has a memory read or pop and the next instruction uses that register
wire load_use_hazard = id_ex_valid && id_ex_mem_read &&
                       ((id_ex_dreg == id_areg && id_areg != 4'd0) ||
                        (id_ex_dreg == id_breg && id_breg != 4'd0)) &&
                       if_id_valid;

wire pop_use_hazard = id_ex_valid && id_ex_pop &&
                      ((id_ex_dreg == id_areg && id_areg != 4'd0) ||
                       (id_ex_dreg == id_breg && id_breg != 4'd0)) &&
                      if_id_valid;

// Cache line hazard: back-to-back SDRAM accesses to different cache lines
// The cache has 1-cycle read latency, so we need to stall when:
// - MEM stage has an SDRAM access (read or write)
// - EX stage also has an SDRAM access (read or write)
// - The cache lines are different
// This ensures the cache output is valid for the MEM stage instruction

// TIMING OPTIMIZATION: Use a 10-bit adder for cache line calculation instead of
// extracting from the full 32-bit ex_mem_addr_calc. This reduces the carry chain
// from 32 bits to 10 bits, saving ~1.5ns on the critical path.
// For cache line comparison, we only need bits[9:3], so a 10-bit adder suffices.
wire [9:0] ex_cache_addr_low = ex_alu_a[9:0] + id_ex_const16[9:0];
wire [6:0] ex_cache_line = ex_cache_addr_low[9:3];
wire [6:0] mem_cache_line = ex_mem_mem_addr[9:3];

// TIMING OPTIMIZATION: Pessimistic SDRAM check that doesn't require the full
// 32-bit address calculation. We check only if the BASE register's upper bits
// indicate a possible SDRAM address. This is safe (may cause extra stalls for
// high-address accesses that happen to have small base values, but such patterns
// are rare - typically high addresses use high base pointers).
// If ex_alu_a[31:27] == 0, the base is in range [0, 0x07FFFFFF], which includes
// all SDRAM addresses (0 to 0x6FFFFFF) plus some non-SDRAM. False positives
// are safe; they just cause unnecessary stalls.
wire ex_needs_sdram_pessimistic = id_ex_valid && (id_ex_mem_read || id_ex_mem_write) && 
                                   (ex_alu_a[31:27] == 5'd0);
wire mem_has_sdram = ex_mem_valid && (ex_mem_mem_read || ex_mem_mem_write) && mem_sel_sdram;
assign cache_line_hazard = ex_needs_sdram_pessimistic && mem_has_sdram && (ex_cache_line != mem_cache_line);

assign hazard_stall = load_use_hazard || pop_use_hazard || cache_line_hazard;

// =============================================================================
// INSTRUCTION FETCH (IF) STAGE
// =============================================================================

// ROM access (addresses >= ROM_ADDRESS)
wire if_use_rom = (pc >= ROM_ADDRESS);
wire [9:0] if_rom_addr = pc - ROM_ADDRESS;

assign rom_fe_addr = if_rom_addr;
assign rom_fe_oe = if_use_rom && !pipeline_stall;
assign rom_fe_hold = pipeline_stall;

// L1I cache access (addresses < ROM_ADDRESS for SDRAM)
wire if_use_cache = !if_use_rom;

// Use pc_delayed when stalled to keep reading the same cache line
// This ensures the cache output matches the address we're checking
assign l1i_pipe_addr = pipeline_stall ? pc_delayed[9:3] : pc[9:3];

// L1I cache hit detection
// Use pc_delayed for tag/offset because the cache output corresponds to the previous cycle's address
wire [13:0] l1i_tag = pc_delayed[23:10];
wire [2:0]  l1i_offset = pc_delayed[2:0];
wire l1i_cache_valid = l1i_pipe_q[0];
wire [13:0] l1i_cache_tag = l1i_pipe_q[14:1];
wire if_use_cache_delayed = (pc_delayed < ROM_ADDRESS);
wire l1i_hit = if_use_cache_delayed && l1i_cache_valid && (l1i_tag == l1i_cache_tag);
wire [31:0] l1i_cache_data = l1i_pipe_q[32 * l1i_offset + 15 +: 32];

// L1I cache hit detection for redirect target (uses pc instead of pc_delayed)
// During redirect_pending, the cache was read with pc[9:3] in the previous cycle,
// so the cache output corresponds to pc, not pc_delayed. We need to check using pc.
wire [13:0] l1i_tag_redirect = pc[23:10];
wire [2:0]  l1i_offset_redirect = pc[2:0];
wire if_use_cache_redirect = (pc < ROM_ADDRESS);
wire l1i_hit_redirect = if_use_cache_redirect && l1i_cache_valid && (l1i_tag_redirect == l1i_cache_tag);

// L1I cache miss handling
// Use if_use_cache_delayed to check if the PREVIOUS instruction (pc_delayed) needs the cache
// We stall when we're trying to execute from SDRAM and the cache doesn't have the data.
assign cache_stall_if = if_use_cache_delayed && !l1i_hit;

// Cache controller interface for instruction fetch misses
// Use pc_delayed since that's the address we're trying to fetch
assign l1i_cache_controller_addr = pc_delayed;
assign l1i_cache_controller_start = cache_stall_if && !l1i_cache_controller_done;
assign l1i_cache_controller_flush = 1'b0; // Not used in this implementation

// Instruction selection
// On cache HIT, always use cache data (even if cache controller says done for a different address)
// On cache MISS with controller done, use controller result (freshly fetched data)
// This prevents stale controller results from being used after a branch to a cached location
wire [31:0] if_instr = if_use_rom ? rom_fe_q : 
                       (l1i_hit ? l1i_cache_data : 
                        (l1i_cache_controller_done ? l1i_cache_controller_result : l1i_cache_data));

// =============================================================================
// IF/ID STAGE REGISTER UPDATE
// =============================================================================
always @(posedge clk) begin
    if (reset) begin
        if_id_pc <= 32'd0;
        if_id_instr <= 32'd0;
        if_id_valid <= 1'b0;
    end else if (flush_if_id || redirect_pending) begin
        // Flush on control hazard OR when we're waiting for the redirect to complete
        // When redirect_pending is set, the ROM output is stale (from the old PC)
        if_id_valid <= 1'b0;
        if_id_instr <= 32'd0;
    end else if (!pipeline_stall) begin
        // TIMING NOTE: pipeline_stall uses registered cache_stall_if for timing.
        // On the first cycle of cache miss, pipeline_stall may be 0 but cache_stall_if is 1.
        // We handle this by using cache_stall_if to gate the valid bit only (narrower path).
        if_id_pc <= pc_delayed;  // Use delayed PC to match instruction data
        if_id_instr <= if_instr;
        if_id_valid <= !cache_stall_if;  // Invalid if cache miss (first cycle)
    end
end

// =============================================================================
// ID/EX STAGE REGISTER UPDATE
// =============================================================================
always @(posedge clk) begin
    if (reset || flush_id_ex) begin
        id_ex_pc <= 32'd0;
        id_ex_instr <= 32'd0;
        id_ex_valid <= 1'b0;
        id_ex_dreg <= 4'd0;
        id_ex_areg <= 4'd0;
        id_ex_breg <= 4'd0;
        id_ex_instr_op <= 4'd0;
        id_ex_alu_op <= 4'd0;
        id_ex_branch_op <= 3'd0;
        id_ex_const_alu <= 32'd0;
        id_ex_const27 <= 27'd0;
        id_ex_const16 <= 32'd0;
        id_ex_he <= 1'b0;
        id_ex_oe <= 1'b0;
        id_ex_sig <= 1'b0;
        id_ex_alu_use_const <= 1'b0;
        id_ex_dreg_we <= 1'b0;
        id_ex_mem_read <= 1'b0;
        id_ex_mem_write <= 1'b0;
        id_ex_is_branch <= 1'b0;
        id_ex_is_jump <= 1'b0;
        id_ex_is_jumpr <= 1'b0;
        id_ex_push <= 1'b0;
        id_ex_pop <= 1'b0;
        id_ex_halt <= 1'b0;
        id_ex_reti <= 1'b0;
        id_ex_getIntID <= 1'b0;
        id_ex_getPC <= 1'b0;
        id_ex_clearCache <= 1'b0;
        id_ex_arithm <= 1'b0;
    end else if (!pipeline_stall) begin
        id_ex_pc <= if_id_pc;
        id_ex_instr <= if_id_instr;
        id_ex_valid <= if_id_valid;
        // NOTE: Register data comes from regbank with 1-cycle latency
        // No need to pipeline it here
        id_ex_dreg <= id_dreg;
        id_ex_areg <= id_areg;
        id_ex_breg <= id_breg;
        id_ex_instr_op <= id_instr_op;
        id_ex_alu_op <= id_alu_op;
        id_ex_branch_op <= id_branch_op;
        id_ex_const_alu <= id_alu_use_constu ? id_const_aluu : id_const_alu;
        id_ex_const27 <= id_const27;
        id_ex_const16 <= id_const16;
        id_ex_he <= id_he;
        id_ex_oe <= id_oe;
        id_ex_sig <= id_sig;
        id_ex_alu_use_const <= id_alu_use_const;
        id_ex_dreg_we <= id_dreg_we && if_id_valid;
        id_ex_mem_read <= id_mem_read && if_id_valid;
        id_ex_mem_write <= id_mem_write && if_id_valid;
        id_ex_is_branch <= id_branch && if_id_valid;
        id_ex_is_jump <= id_jumpc && if_id_valid;
        id_ex_is_jumpr <= id_jumpr && if_id_valid;
        id_ex_push <= id_push && if_id_valid;
        id_ex_pop <= id_pop && if_id_valid;
        id_ex_halt <= id_halt && if_id_valid;
        id_ex_reti <= id_reti && if_id_valid;
        id_ex_getIntID <= id_getIntID && if_id_valid;
        id_ex_getPC <= id_getPC && if_id_valid;
        id_ex_clearCache <= id_clearCache && if_id_valid;
        id_ex_arithm <= id_arithm && if_id_valid;
    end else if (!ex_pipeline_stall) begin
        // EX consumed the instruction but we're stalled (load-use or pop-use hazard)
        // Insert a bubble in ID/EX so the instruction doesn't execute twice
        id_ex_valid <= 1'b0;
        id_ex_dreg_we <= 1'b0;
        id_ex_mem_read <= 1'b0;
        id_ex_mem_write <= 1'b0;
        id_ex_is_branch <= 1'b0;
        id_ex_is_jump <= 1'b0;
        id_ex_is_jumpr <= 1'b0;
        id_ex_push <= 1'b0;
        id_ex_pop <= 1'b0;
        id_ex_halt <= 1'b0;
        id_ex_reti <= 1'b0;
        id_ex_getIntID <= 1'b0;
        id_ex_getPC <= 1'b0;
        id_ex_clearCache <= 1'b0;
        id_ex_arithm <= 1'b0;
    end
end

// =============================================================================
// MEMORY STAGE (MEM)
// =============================================================================

// Memory address calculation (done in EX, used in MEM)
// Use a registered address to handle forwarding correctly when stalled
// The issue: when EX is stalled, the forwarding source can move past WB,
// causing ex_alu_a to get a stale value. We need to capture the correct
// address when it's first calculated with valid forwarding.
wire [31:0] ex_mem_addr_calc_comb = ex_alu_a + id_ex_const16;
reg [31:0] ex_mem_addr_calc_reg = 32'd0;

// Track if the current EX instruction has a captured address
// Reset when a new instruction enters EX, set when we have valid forwarding
reg ex_addr_captured = 1'b0;

always @(posedge clk) begin
    if (reset) begin
        ex_mem_addr_calc_reg <= 32'd0;
        ex_addr_captured <= 1'b0;
    end else if (!ex_pipeline_stall) begin
        // Instruction leaving EX stage, reset capture for next instruction
        ex_addr_captured <= 1'b0;
    end else if (!ex_addr_captured && id_ex_valid && (id_ex_mem_read || id_ex_mem_write)) begin
        // First cycle with this instruction stalled in EX, capture the address
        // At this point forwarding should be valid
        ex_mem_addr_calc_reg <= ex_mem_addr_calc_comb;
        ex_addr_captured <= 1'b1;
    end
end

// Use the captured address if available, otherwise use combinational
wire [31:0] ex_mem_addr_calc = ex_addr_captured ? ex_mem_addr_calc_reg : ex_mem_addr_calc_comb;

// EX-stage address decoding for BRAM address setup (data available next cycle in MEM)
// VRAM and ROM addresses must be set in EX stage because BRAM has 1-cycle read latency
wire ex_sel_rom    = ex_mem_addr_calc >= 32'h7800000 && ex_mem_addr_calc < 32'h7900000;
wire ex_sel_vram32 = ex_mem_addr_calc >= 32'h7900000 && ex_mem_addr_calc < 32'h7A00000;
wire ex_sel_vram8  = ex_mem_addr_calc >= 32'h7A00000 && ex_mem_addr_calc < 32'h7B00000;
wire ex_sel_vrampx = ex_mem_addr_calc >= 32'h7B00000 && ex_mem_addr_calc < 32'h7C00000;

// EX-stage local address calculations for BRAM (ROM and VRAM)
wire [31:0] ex_local_addr_rom    = ex_mem_addr_calc - 32'h7800000;
wire [31:0] ex_local_addr_vram32 = ex_mem_addr_calc - 32'h7900000;
wire [31:0] ex_local_addr_vram8  = ex_mem_addr_calc - 32'h7A00000;
wire [31:0] ex_local_addr_vrampx = ex_mem_addr_calc - 32'h7B00000;

// Address decoding (MEM stage - for data selection and write control)
wire mem_sel_sdram  = ex_mem_mem_addr >= 32'h0000000 && ex_mem_mem_addr < 32'h7000000;
wire mem_sel_io     = ex_mem_mem_addr >= 32'h7000000 && ex_mem_mem_addr < 32'h7800000;
wire mem_sel_rom    = ex_mem_mem_addr >= 32'h7800000 && ex_mem_mem_addr < 32'h7900000;
wire mem_sel_vram32 = ex_mem_mem_addr >= 32'h7900000 && ex_mem_mem_addr < 32'h7A00000;
wire mem_sel_vram8  = ex_mem_mem_addr >= 32'h7A00000 && ex_mem_mem_addr < 32'h7B00000;
wire mem_sel_vrampx = ex_mem_mem_addr >= 32'h7B00000 && ex_mem_mem_addr < 32'h7C00000;

// Local address calculation (MEM stage - for ROM and I/O, not VRAM)
wire [31:0] mem_local_addr = mem_sel_rom    ? ex_mem_mem_addr - 32'h7800000 :
                             mem_sel_vram32 ? ex_mem_mem_addr - 32'h7900000 :
                             mem_sel_vram8  ? ex_mem_mem_addr - 32'h7A00000 :
                             mem_sel_vrampx ? ex_mem_mem_addr - 32'h7B00000 :
                             ex_mem_mem_addr; // SDRAM and I/O use full address

// ROM data port
// ROM data port - address from EX stage for reads (BRAM latency)
// ROM is read-only, no write support needed
assign rom_mem_addr = ex_local_addr_rom[9:0];
wire [31:0] rom_mem_data = rom_mem_q;

// VRAM32 interface - address from EX stage for reads (BRAM latency), MEM stage for writes
// When writing, we need the address to match the write_enable timing (MEM stage)
// When reading, we need address in EX so data is ready in MEM
assign vram32_addr = (ex_mem_valid && ex_mem_mem_write && mem_sel_vram32) ? 
                     mem_local_addr[10:0] : ex_local_addr_vram32[10:0];
assign vram32_d = ex_mem_breg_data;
assign vram32_we = ex_mem_valid && ex_mem_mem_write && mem_sel_vram32 && !backend_pipeline_stall;

// VRAM8 interface - address from EX stage for reads, MEM stage for writes
assign vram8_addr = (ex_mem_valid && ex_mem_mem_write && mem_sel_vram8) ?
                    mem_local_addr[13:0] : ex_local_addr_vram8[13:0];
assign vram8_d = ex_mem_breg_data[7:0];
assign vram8_we = ex_mem_valid && ex_mem_mem_write && mem_sel_vram8 && !backend_pipeline_stall;

// VRAMPX interface - address from EX stage for reads, MEM stage for writes
assign vramPX_addr = (ex_mem_valid && ex_mem_mem_write && mem_sel_vrampx) ?
                     mem_local_addr[16:0] : ex_local_addr_vrampx[16:0];
assign vramPX_d = ex_mem_breg_data[7:0];
assign vramPX_we = ex_mem_valid && ex_mem_mem_write && mem_sel_vrampx && !backend_pipeline_stall;

// L1D cache interface
// Use MEM stage address for cache lookup to ensure correctness
// The EX stage pre-fetch optimization was incorrect - when an instruction moves
// from EX to MEM, l1d_pipe_addr would already point to the NEXT instruction's address
assign l1d_pipe_addr = ex_mem_mem_addr[9:3];

// L1D cache hit detection
wire [13:0] l1d_tag = ex_mem_mem_addr[23:10];
wire [2:0]  l1d_offset = ex_mem_mem_addr[2:0];
wire l1d_cache_valid = l1d_pipe_q[0];
wire [13:0] l1d_cache_tag = l1d_pipe_q[14:1];
wire l1d_hit = mem_sel_sdram && l1d_cache_valid && (l1d_tag == l1d_cache_tag);
wire [31:0] l1d_cache_data = l1d_pipe_q[32 * l1d_offset + 15 +: 32];

// L1D cache read delay tracking
// Since the cache DPRAM has 1-cycle read latency, when an instruction first
// enters MEM stage, we need to wait 1 cycle for l1d_pipe_q to be valid
// Track if we've waited for the cache read for the current instruction
reg l1d_cache_read_done = 1'b0;

// Detect when a NEW instruction enters MEM (based on PC change)
reg [31:0] l1d_prev_pc = 32'hFFFFFFFF;
wire l1d_is_sdram_op = ex_mem_valid && (ex_mem_mem_read || ex_mem_mem_write) && mem_sel_sdram;
wire l1d_new_instr = (ex_mem_pc != l1d_prev_pc);

// Need to wait for cache read on first cycle of new SDRAM operation
// Use l1d_cache_read_done to track if we've completed the wait
wire l1d_need_cache_wait = l1d_is_sdram_op && !l1d_cache_read_done;

always @(posedge clk) begin
    if (reset) begin
        l1d_cache_read_done <= 1'b0;
        l1d_prev_pc <= 32'hFFFFFFFF;
    end else begin
        // When a NEW instruction enters MEM, reset the cache_read_done flag
        if (l1d_new_instr) begin
            l1d_prev_pc <= ex_mem_pc;
            l1d_cache_read_done <= 1'b0;
        end
        // After waiting one cycle, mark cache read as done
        else if (l1d_need_cache_wait) begin
            l1d_cache_read_done <= 1'b1;
        end
        // When no longer an SDRAM op (or invalid), reset for next instruction
        else if (!l1d_is_sdram_op) begin
            l1d_cache_read_done <= 1'b0;
        end
    end
end

// =============================================================================
// L1D CACHE CONTROLLER STATE MACHINE
// =============================================================================
// Manages cache miss handling to ensure proper request/done handshaking
localparam L1D_STATE_IDLE    = 2'b00;
localparam L1D_STATE_STARTED = 2'b01;
localparam L1D_STATE_WAIT    = 2'b10;

reg [1:0] l1d_state = L1D_STATE_IDLE;
reg       l1d_request_finished = 1'b0;
reg       l1d_start_reg = 1'b0;
reg [31:0] l1d_result_reg = 32'd0;

// Cache read wait signal - stall while waiting for cache DPRAM to read
wire l1d_cache_read_wait = l1d_need_cache_wait;

// Cache miss/write detection (only triggers new request if not already finished)
// Wait for cache read delay to complete
wire l1d_miss = ex_mem_valid && ex_mem_mem_read && mem_sel_sdram && !l1d_hit && !l1d_request_finished && !l1d_cache_read_wait;
wire l1d_write = ex_mem_valid && ex_mem_mem_write && mem_sel_sdram && !l1d_request_finished && !l1d_cache_read_wait;

// Cache stall: waiting for cache read, or operation to complete
assign cache_stall_mem = l1d_cache_read_wait || l1d_miss || l1d_write;

// State machine for cache controller requests
always @(posedge clk) begin
    if (reset) begin
        l1d_state <= L1D_STATE_IDLE;
        l1d_start_reg <= 1'b0;
        l1d_request_finished <= 1'b0;
        l1d_result_reg <= 32'd0;
    end else begin
        case (l1d_state)
            L1D_STATE_IDLE: begin
                l1d_start_reg <= 1'b0;
                l1d_request_finished <= 1'b0;
                l1d_result_reg <= 32'd0;
                
                // Start new request on cache miss or SDRAM write (after cache read wait clears)
                if (!l1d_cache_read_wait &&
                    ((ex_mem_valid && ex_mem_mem_read && mem_sel_sdram && !l1d_hit && !l1d_request_finished) ||
                     (ex_mem_valid && ex_mem_mem_write && mem_sel_sdram && !l1d_request_finished))) begin
                    l1d_start_reg <= 1'b1;
                    l1d_state <= L1D_STATE_STARTED;
                end
            end
            
            L1D_STATE_STARTED: begin
                // Clear start signal after one cycle
                l1d_start_reg <= 1'b0;
                l1d_state <= L1D_STATE_WAIT;
            end
            
            L1D_STATE_WAIT: begin
                // Wait for cache controller to complete
                if (l1d_cache_controller_done) begin
                    l1d_result_reg <= l1d_cache_controller_result;
                    l1d_request_finished <= 1'b1;
                    l1d_state <= L1D_STATE_IDLE;
                end
            end
        endcase
    end
end

// L1D cache controller interface
assign l1d_cache_controller_addr = ex_mem_mem_addr;
assign l1d_cache_controller_data = ex_mem_breg_data;
assign l1d_cache_controller_we = ex_mem_mem_write && mem_sel_sdram;
assign l1d_cache_controller_start = l1d_start_reg;

// Memory Unit interface (I/O)
// Use a state machine to ensure we only start the MU once per operation
// and wait for completion before allowing a new operation
reg mu_io_started = 1'b0;  // Track if we've already started this I/O operation

always @(posedge clk) begin
    if (reset) begin
        mu_start <= 1'b0;
        mu_addr <= 32'd0;
        mu_data <= 32'd0;
        mu_we <= 1'b0;
        mu_io_started <= 1'b0;
    end else if (mu_done) begin
        // MU operation completed, clear the started flag
        mu_start <= 1'b0;
        mu_io_started <= 1'b0;
    end else if (ex_mem_valid && mem_sel_io && (ex_mem_mem_read || ex_mem_mem_write) && !mu_io_started) begin
        // New I/O operation, start it
        mu_start <= 1'b1;
        mu_addr <= ex_mem_mem_addr;
        mu_data <= ex_mem_breg_data;
        mu_we <= ex_mem_mem_write;
        mu_io_started <= 1'b1;
    end else begin
        mu_start <= 1'b0;
    end
end

assign mu_stall = ex_mem_valid && mem_sel_io && (ex_mem_mem_read || ex_mem_mem_write) && !mu_done;

// Cache clear
always @(posedge clk) begin
    if (reset) begin
        l1_clear_cache <= 1'b0;
    end else if (ex_mem_valid && ex_mem_clearCache && !l1_clear_cache_done) begin
        l1_clear_cache <= 1'b1;
    end else begin
        l1_clear_cache <= 1'b0;
    end
end

assign cc_stall = ex_mem_valid && ex_mem_clearCache && !l1_clear_cache_done;

// Memory read data multiplexer
// For SDRAM: use l1d_result_reg if we had a cache miss (request_finished),
// otherwise use direct cache data for hits
wire [31:0] mem_read_data = mem_sel_rom    ? rom_mem_data :
                            mem_sel_vram32 ? vram32_q :
                            mem_sel_vram8  ? {24'd0, vram8_q} :
                            mem_sel_vrampx ? {24'd0, vramPX_q} :
                            mem_sel_io     ? mu_q :
                            mem_sel_sdram  ? (l1d_request_finished ? l1d_result_reg : l1d_cache_data) :
                            32'hDEADBEEF;

// =============================================================================
// EX/MEM STAGE REGISTER UPDATE
// =============================================================================

// Result selection for EX stage (before going to MEM)
// SAVPC uses PC, INTID uses interrupt ID, otherwise ALU result
// For multi-cycle ALU: use malu_result directly when done (not the registered value,
// which hasn't been updated yet on the same clock edge)
wire [31:0] ex_result = id_ex_getPC    ? id_ex_pc :
                        id_ex_getIntID ? {24'd0, int_id} :
                        (id_ex_arithm && malu_done) ? malu_result : 
                        id_ex_arithm   ? malu_result_reg : 
                        ex_alu_result;

always @(posedge clk) begin
    if (reset || flush_ex_mem) begin
        // Reset or flush on branch/jump taken in MEM stage
        ex_mem_pc <= 32'd0;
        ex_mem_instr <= 32'd0;
        ex_mem_valid <= 1'b0;
        ex_mem_alu_result <= 32'd0;
        ex_mem_breg_data <= 32'd0;
        ex_mem_dreg <= 4'd0;
        ex_mem_instr_op <= 4'd0;
        ex_mem_mem_addr <= 32'd0;
        ex_mem_dreg_we <= 1'b0;
        ex_mem_mem_read <= 1'b0;
        ex_mem_mem_write <= 1'b0;
        ex_mem_push <= 1'b0;
        ex_mem_pop <= 1'b0;
        ex_mem_halt <= 1'b0;
        ex_mem_getIntID <= 1'b0;
        ex_mem_getPC <= 1'b0;
        ex_mem_clearCache <= 1'b0;
        ex_mem_arithm <= 1'b0;
        // Branch control signals
        ex_mem_is_branch <= 1'b0;
        ex_mem_is_jump <= 1'b0;
        ex_mem_is_jumpr <= 1'b0;
        ex_mem_branch_op <= 3'd0;
        ex_mem_sig <= 1'b0;
        ex_mem_oe <= 1'b0;
        ex_mem_const16 <= 32'd0;
        ex_mem_const27 <= 27'd0;
        ex_mem_areg_data <= 32'd0;
    end else if (!ex_pipeline_stall) begin
        ex_mem_pc <= id_ex_pc;
        ex_mem_instr <= id_ex_instr;
        ex_mem_valid <= id_ex_valid;
        ex_mem_alu_result <= ex_result;  // Use selected result
        ex_mem_breg_data <= ex_breg_forwarded;
        ex_mem_dreg <= id_ex_dreg;
        ex_mem_instr_op <= id_ex_instr_op;
        ex_mem_mem_addr <= ex_mem_addr_calc;
        ex_mem_dreg_we <= id_ex_dreg_we;
        ex_mem_mem_read <= id_ex_mem_read;
        ex_mem_mem_write <= id_ex_mem_write;
        ex_mem_push <= id_ex_push;
        ex_mem_pop <= id_ex_pop;
        ex_mem_halt <= id_ex_halt;
        ex_mem_getIntID <= id_ex_getIntID;
        ex_mem_getPC <= id_ex_getPC;
        ex_mem_clearCache <= id_ex_clearCache;
        ex_mem_arithm <= id_ex_arithm;
        // Branch control signals for MEM-stage resolution
        ex_mem_is_branch <= id_ex_is_branch;
        ex_mem_is_jump <= id_ex_is_jump;
        ex_mem_is_jumpr <= id_ex_is_jumpr;
        ex_mem_branch_op <= id_ex_branch_op;
        ex_mem_sig <= id_ex_sig;
        ex_mem_oe <= id_ex_oe;
        ex_mem_const16 <= id_ex_const16;
        ex_mem_const27 <= id_ex_const27;
        ex_mem_areg_data <= ex_alu_a;  // Forwarded A value
    end else if (cache_line_hazard && !backend_stall) begin
        // Cache line hazard: MEM should advance, insert bubble into MEM stage
        // The instruction in EX (second SDRAM access) stays in ID/EX (pipeline_stall holds it)
        // We need to let the first instruction complete in MEM and go to WB
        ex_mem_valid <= 1'b0;  // Bubble in MEM stage
        ex_mem_dreg_we <= 1'b0;
        ex_mem_mem_read <= 1'b0;
        ex_mem_mem_write <= 1'b0;
        ex_mem_push <= 1'b0;
        ex_mem_pop <= 1'b0;
        ex_mem_halt <= 1'b0;
        // Clear branch signals during hazard
        ex_mem_is_branch <= 1'b0;
        ex_mem_is_jump <= 1'b0;
        ex_mem_is_jumpr <= 1'b0;
    end
end

// =============================================================================
// MEM/WB STAGE REGISTER UPDATE
// =============================================================================

// Pre-compute writeback data in MEM stage (for non-pop instructions)
// This breaks the critical path by registering the result mux
// For pop: we still need stack_q which arrives in WB cycle
wire [31:0] mem_wb_result_precompute = ex_mem_mem_read ? mem_read_data : ex_mem_alu_result;

always @(posedge clk) begin
    if (reset) begin
        mem_wb_pc <= 32'd0;
        mem_wb_instr <= 32'd0;
        mem_wb_valid <= 1'b0;
        mem_wb_alu_result <= 32'd0;
        mem_wb_mem_data <= 32'd0;
        mem_wb_stack_data <= 32'd0;
        mem_wb_result <= 32'd0;
        mem_wb_dreg <= 4'd0;
        mem_wb_instr_op <= 4'd0;
        mem_wb_dreg_we <= 1'b0;
        mem_wb_mem_read <= 1'b0;
        mem_wb_pop <= 1'b0;
        mem_wb_halt <= 1'b0;
    end else if (!backend_pipeline_stall) begin
        mem_wb_pc <= ex_mem_pc;
        mem_wb_instr <= ex_mem_instr;
        mem_wb_valid <= ex_mem_valid;
        mem_wb_alu_result <= ex_mem_alu_result;
        mem_wb_mem_data <= mem_read_data;
        mem_wb_stack_data <= stack_q;
        mem_wb_result <= mem_wb_result_precompute;  // Pre-computed result (excluding pop)
        mem_wb_dreg <= ex_mem_dreg;
        mem_wb_instr_op <= ex_mem_instr_op;
        mem_wb_dreg_we <= ex_mem_dreg_we;
        mem_wb_mem_read <= ex_mem_mem_read;
        mem_wb_pop <= ex_mem_pop;
        mem_wb_halt <= ex_mem_halt;
    end
end

// =============================================================================
// WRITEBACK STAGE (WB)
// =============================================================================

// Result selection - use pre-computed result for non-pop, stack_q for pop
// Note: stack_q is used directly because the stack has 1-cycle read latency
// the result appears on stack_q in the WB cycle when the pop instruction 
// that was in MEM stage last cycle advances to WB stage
assign wb_data = mem_wb_pop ? stack_q : mem_wb_result;

assign wb_dreg = mem_wb_dreg;
assign wb_dreg_we = mem_wb_valid && mem_wb_dreg_we && (mem_wb_dreg != 4'd0) && !backend_pipeline_stall;

// =============================================================================
// DEBUG OUTPUT
// =============================================================================
// The test framework looks for "reg rXX:" in the output

// This debug output is already in Regbank.v, but we add explicit r15 tracking here for tests
// The Regbank.v already has: $display("%0t reg r%02d: %d", $time, addr_d, data_d);

// SIMULATION DEBUG: Track PC through pipeline when register write occurs
// synthesis translate_off
always @(posedge clk) begin
    if (wb_dreg_we && wb_dreg != 4'd0 && !reset)
        $display("%0t WB pc=%0d instr=%08h dreg=r%0d data=%0d", $time, mem_wb_pc, mem_wb_instr, wb_dreg, wb_data);
end

// Debug L1I cache access
always @(posedge clk) begin
    if (!reset && if_id_valid && !pipeline_stall)
        $display("%0t IF: pc=%0d pc_del=%0d l1i_addr=%0d l1i_offset=%0d hit=%b instr=%08h", 
                 $time, pc, pc_delayed, l1i_pipe_addr, l1i_offset, l1i_hit, if_instr);
end
// synthesis translate_on

endmodule

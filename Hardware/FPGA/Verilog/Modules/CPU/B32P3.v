/*
 * B32P3
 * 32-bit pipelined CPU with classic 5-stage pipeline
 * Third iteration of B32P with focus on optimized timings to run at 100MHz
 *
 * Features:
 * - 5 stage pipeline (Classic MIPS-style)
 *   - IF:  Instruction Fetch
 *   - ID:  Instruction Decode & Register Read
 *   - EX:  Execute (ALU operations, branch resolution)
 *   - MEM: Memory Access (Load/Store)
 *   - WB:  Write Back
 * - Simple hazard detection (load-use only)
 * - Data forwarding
 * - 32 bits, word-addressable only
 * - 32 bit address space for 16GiB of addressable memory
 *   - 27 bits jump constant for 512MiB of easily jumpable instruction memory
 *
 * Parameters:
 * - ROM_ADDRESS:            Address where ROM is mapped (also initial PC value)
 * - INTERRUPT_JUMP_ADDR:    Address to jump to when an interrupt is triggered
 * - NUM_INTERRUPTS:         Number of interrupt lines
 *
 * Notes:
 * - ROM needs to be placed on an address after RAM (RAM is best to be placed at 0x0000000)
 * - Interrupts are only valid on an address before ROM
 */
module B32P3 #(
    parameter ROM_ADDRESS = 32'h1E000000,
    parameter INTERRUPT_JUMP_ADDR = 32'd4,
    parameter NUM_INTERRUPTS = 8
) (
    // ---- System interface ----
    input  wire         clk,
    input  wire         reset,

    // ---- ROM (dual port) ----
    output wire [9:0]   rom_fe_addr,
    output wire         rom_fe_oe,
    input  wire [31:0]  rom_fe_q,
    output wire         rom_fe_hold,

    output wire [9:0]   rom_mem_addr,
    input  wire [31:0]  rom_mem_q,

    // ---- VRAM32 ----
    output wire [10:0]  vram32_addr,
    output wire [31:0]  vram32_d,
    output wire         vram32_we,
    input  wire [31:0]  vram32_q,

    // ---- VRAM8 ----
    output wire [13:0]  vram8_addr,
    output wire [7:0]   vram8_d,
    output wire         vram8_we,
    input  wire [7:0]   vram8_q,

    // ---- VRAMPX ----
    output wire [16:0]  vramPX_addr,
    output wire [7:0]   vramPX_d,
    output wire         vramPX_we,
    input  wire [7:0]   vramPX_q,
    input  wire         vramPX_fifo_full,

    // ---- Pixel Palette ----
    output wire         palette_we,
    output wire [7:0]   palette_addr,
    output wire [23:0]  palette_wdata,

    // ---- L1i cache (CPU pipeline port) ----
    output wire [6:0]   l1i_pipe_addr,
    input  wire [270:0] l1i_pipe_q,

    // ---- L1d cache (CPU pipeline port) ----
    output wire [6:0]   l1d_pipe_addr,
    input  wire [270:0] l1d_pipe_q,

    // ---- Cache controller ----
    output wire [31:0]  l1i_cache_controller_addr,
    output wire         l1i_cache_controller_start,
    output wire         l1i_cache_controller_flush,
    input  wire         l1i_cache_controller_done,
    input  wire [31:0]  l1i_cache_controller_result,

    output wire [31:0]  l1d_cache_controller_addr,
    output wire [31:0]  l1d_cache_controller_data,
    output wire         l1d_cache_controller_we,
    output wire         l1d_cache_controller_start,
    output wire [3:0]   l1d_cache_controller_byte_enable,
    input  wire         l1d_cache_controller_done,
    input  wire [31:0]  l1d_cache_controller_result,

    output wire         l1_clear_cache,
    input wire          l1_clear_cache_done,

    // ---- Memory Unit ----
    output wire         mu_start,
    output wire [31:0]  mu_addr,
    output wire [31:0]  mu_data,
    output wire         mu_we,
    input  wire [31:0]  mu_q,
    input  wire         mu_done,

    // ---- Interrupts ----
    input wire [NUM_INTERRUPTS-1:0] interrupts
);

// CPU-internal I/O register addresses (byte-addressed)
localparam CPU_IO_PC_BACKUP    = 32'h1F000000;  // Read/write interrupt return PC
localparam CPU_IO_HW_STACK_PTR = 32'h1F000004;  // Read/write hardware stack pointer

// ---- INTERRUPT CONTROLLER ----
// Interrupt enable/disable flag (disabled during interrupt handling)
wire int_disabled;

// Interrupt controller signals
wire int_cpu;           // Interrupt request from controller
wire [7:0] int_id;      // Interrupt ID value from controller

InterruptController interrupt_controller (
    .clk          (clk),
    .reset        (reset),
    .interrupts   (interrupts),
    .int_disabled (int_disabled),
    .int_cpu      (int_cpu),
    .int_id       (int_id)
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
wire [31:0] pc_backup;

// RETI signal from EX stage (return from interrupt)
// IMPORTANT: RETI should NOT execute if pc_redirect is active, because the branch
// in MEM stage will flush the RETI instruction. Without this check, a branch that
// skips a RETI would still execute the RETI due to pipeline timing.
wire reti_valid = id_ex_valid && id_ex_reti && !pc_redirect;

// ---- PIPELINE CONTROL SIGNALS ----
wire        flush_if_id;            // Flush IF/ID register
wire        flush_id_ex;            // Flush ID/EX register
wire        flush_ex_mem;           // Flush EX/MEM register

// Pipeline stall sources (driven by cache, ALU, MU, and cache-clear subsystems)
wire cache_stall_if;    // L1I cache miss stall
wire cache_stall_mem;   // L1D cache miss stall
wire multicycle_stall;  // Multi-cycle ALU stall
wire mu_stall;          // Memory unit stall
wire cc_stall;          // Cache clear stall
wire vrampx_stall;      // VRAMPX FIFO full stall

// Pipeline control outputs
wire        pipeline_stall;
wire        ex_pipeline_stall;
wire        backend_pipeline_stall;
wire [1:0]  forward_a;
wire [1:0]  forward_b;
wire        cache_line_hazard;

// PC redirect signals
wire        pc_redirect;
wire [31:0] pc_redirect_target;

// ---- INSTRUCTION FETCH (IF) STAGE ----
wire [31:0] if_id_pc;
wire [31:0] if_id_instr;
wire        if_id_valid;
wire [31:0] if_instr;

InstructionFetch #(
    .ROM_ADDRESS         (ROM_ADDRESS),
    .INTERRUPT_JUMP_ADDR (INTERRUPT_JUMP_ADDR),
    .CPU_IO_PC_BACKUP    (CPU_IO_PC_BACKUP)
) instr_fetch (
    .clk                        (clk),
    .reset                      (reset),
    // Pipeline control
    .pipeline_stall             (pipeline_stall),
    .flush_if_id                (flush_if_id),
    .backend_pipeline_stall     (backend_pipeline_stall),
    // PC redirect
    .pc_redirect                (pc_redirect),
    .pc_redirect_target         (pc_redirect_target),
    // Interrupt/RETI
    .interrupt_valid            (interrupt_valid),
    .reti_valid                 (reti_valid),
    .ex_mem_pc                  (ex_mem_pc),
    // CPU I/O: pc_backup write
    .ex_mem_valid               (ex_mem_valid),
    .ex_mem_mem_write           (ex_mem_mem_write),
    .ex_mem_mem_addr            (ex_mem_mem_addr),
    .ex_mem_breg_data           (ex_mem_breg_data),
    // ROM interface
    .rom_fe_q                   (rom_fe_q),
    .rom_fe_addr                (rom_fe_addr),
    .rom_fe_oe                  (rom_fe_oe),
    .rom_fe_hold                (rom_fe_hold),
    // L1I cache pipeline port
    .l1i_pipe_q                 (l1i_pipe_q),
    .l1i_pipe_addr              (l1i_pipe_addr),
    // L1I cache controller
    .l1i_cache_controller_done  (l1i_cache_controller_done),
    .l1i_cache_controller_result(l1i_cache_controller_result),
    .l1i_cache_controller_addr  (l1i_cache_controller_addr),
    .l1i_cache_controller_start (l1i_cache_controller_start),
    .l1i_cache_controller_flush (l1i_cache_controller_flush),
    // Outputs
    .int_disabled               (int_disabled),
    .pc_backup                  (pc_backup),
    .cache_stall_if             (cache_stall_if),
    .if_id_pc                   (if_id_pc),
    .if_id_instr                (if_id_instr),
    .if_id_valid                (if_id_valid),
    .if_instr                   (if_instr),
    // Debug
    .id_ex_pc                   (id_ex_pc)
);

// ---- ID/EX PIPELINE REGISTER ----
reg [31:0] id_ex_pc = 32'd0;
reg        id_ex_valid = 1'b0;

// NOTE: Register file data (areg_data, breg_data) comes directly from regbank
// with 1-cycle latency - we don't pipeline these values

// Decoded fields
reg [3:0]  id_ex_dreg = 4'd0;
reg [3:0]  id_ex_areg = 4'd0;
reg [3:0]  id_ex_breg = 4'd0;
reg [3:0]  id_ex_alu_op = 4'd0;
reg [2:0]  id_ex_branch_op = 3'd0;
reg [31:0] id_ex_const_alu = 32'd0;
reg [26:0] id_ex_const27 = 27'd0;
reg [31:0] id_ex_const16 = 32'd0;
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
reg        id_ex_get_int_id = 1'b0;
reg        id_ex_get_pc = 1'b0;
reg        id_ex_clear_cache = 1'b0;
reg        id_ex_arithm = 1'b0;
reg [1:0]  id_ex_mem_size = 2'b00;
reg        id_ex_mem_sign_extend = 1'b0;

// ---- EX/MEM PIPELINE REGISTER ----
reg [31:0] ex_mem_pc = 32'd0;
reg        ex_mem_valid = 1'b0;

// ALU result
reg [31:0] ex_mem_alu_result = 32'd0;
reg [31:0] ex_mem_breg_data = 32'd0; // For store operations

// Decoded fields
reg [3:0]  ex_mem_dreg = 4'd0;

// Memory address
reg [31:0] ex_mem_mem_addr = 32'd0;

// Control signals
reg        ex_mem_dreg_we = 1'b0;
reg        ex_mem_mem_read = 1'b0;
reg        ex_mem_mem_write = 1'b0;
reg        ex_mem_push = 1'b0;
reg        ex_mem_pop = 1'b0;
reg        ex_mem_halt = 1'b0;
reg        ex_mem_clear_cache = 1'b0;

// Branch/jump control signals for MEM-stage branch resolution
reg        ex_mem_is_branch = 1'b0;
reg        ex_mem_is_jump = 1'b0;
reg        ex_mem_is_jumpr = 1'b0;
reg [2:0]  ex_mem_branch_op = 3'd0;
reg        ex_mem_sig = 1'b0;
reg        ex_mem_oe = 1'b0;
reg [31:0] ex_mem_const16 = 32'd0;
reg [26:0] ex_mem_const27 = 27'd0;
reg [31:0] ex_mem_areg_data = 32'd0;

// Memory size control for byte-addressable operations
reg [1:0]  ex_mem_mem_size = 2'b00;
reg        ex_mem_mem_sign_extend = 1'b0;

// ---- MEM/WB PIPELINE REGISTER ----
reg        mem_wb_valid = 1'b0;

// Results
reg [31:0] mem_wb_alu_result = 32'd0;
reg [31:0] mem_wb_mem_data = 32'd0;
reg [31:0] mem_wb_stack_data = 32'd0;

// Decoded fields
reg [3:0]  mem_wb_dreg = 4'd0;

// Control signals
reg        mem_wb_dreg_we = 1'b0;
reg        mem_wb_mem_read = 1'b0;
reg        mem_wb_pop = 1'b0;
reg        mem_wb_halt = 1'b0;

// ---- INSTRUCTION DECODER ----
wire [3:0]  id_instr_op;
wire [3:0]  id_alu_op;
wire [2:0]  id_branch_op;
wire [31:0] id_const_alu;
wire [31:0] id_const_aluu;
wire [31:0] id_const16;
wire [26:0] id_const27;
wire [3:0]  id_areg;
wire [3:0]  id_breg;
wire [3:0]  id_dreg;
wire        id_oe;
wire        id_sig;
wire [3:0]  id_read_subop;
wire [3:0]  id_write_subop;

InstructionDecoder instr_decoder (
    .instr       (if_id_instr),
    .instr_op    (id_instr_op),
    .alu_op      (id_alu_op),
    .branch_op   (id_branch_op),
    .const_alu   (id_const_alu),
    .const_aluu  (id_const_aluu),
    .const16     (id_const16),
    .const27     (id_const27),
    .areg        (id_areg),
    .breg        (id_breg),
    .dreg        (id_dreg),
    .oe          (id_oe),
    .sig         (id_sig),
    .read_subop  (id_read_subop),
    .write_subop (id_write_subop)
);

// ---- CONTROL UNIT ----
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
wire id_get_int_id;
wire id_get_pc;
wire id_clear_cache;
wire [1:0] id_mem_size;
wire       id_mem_sign_extend;

ControlUnit control_unit (
    .instr_op       (id_instr_op),
    .alu_op         (id_alu_op),
    .read_subop     (id_read_subop),
    .write_subop    (id_write_subop),
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
    .get_int_id     (id_get_int_id),
    .get_pc         (id_get_pc),
    .clear_cache    (id_clear_cache),
    .mem_size       (id_mem_size),
    .mem_sign_extend(id_mem_sign_extend)
);

// ---- REGISTER FILE ----
// The regbank is fully registered:
// - Cycle 1 (IF): Addresses are captured from if_instr
// - Cycle 2 (ID): Data is read from register array and registered
// - Cycle 3 (EX): Data is available at outputs
// We send addresses from IF stage using if_instr, and receive data in EX stage

// Extract register addresses from IF instruction
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

    // Read ports - addresses from IF stage, data available in EX stage
    .addr_a     (if_areg),
    .addr_b     (if_breg),
    .clear      (flush_if_id),
    .hold       (pipeline_stall),
    .data_a     (ex_areg_data),  // Output arrives in EX stage
    .data_b     (ex_breg_data),  // Output arrives in EX stage

    // Write port (WB stage)
    .addr_d     (wb_dreg),
    .data_d     (wb_data),
    .we         (wb_dreg_we)
);

// ---- ALU ----
wire [31:0] ex_alu_a;
wire [31:0] ex_alu_b;
wire [31:0] ex_alu_result;

// ---- Forwarded-value capture for EX stalls ----
// When EX is stalled (e.g., cache_line_hazard), the forwarding sources may
// advance out of the pipeline before EX can latch. This creates a gap where
// forward_a/b fall to 00 (no forward) but the register file output is stale.
// Shadow registers capture the forwarded value during the stall so it persists.
reg [31:0] ex_stall_saved_a = 32'd0;
reg [31:0] ex_stall_saved_b = 32'd0;
reg        ex_stall_saved_a_valid = 1'b0;
reg        ex_stall_saved_b_valid = 1'b0;

always @(posedge clk) begin
    if (reset) begin
        ex_stall_saved_a_valid <= 1'b0;
        ex_stall_saved_b_valid <= 1'b0;
    end else if (!ex_pipeline_stall) begin
        // EX advancing: clear saved values
        ex_stall_saved_a_valid <= 1'b0;
        ex_stall_saved_b_valid <= 1'b0;
    end else begin
        // EX stalled: capture forwarded values when available
        if (forward_a != 2'b00) begin
            ex_stall_saved_a <= (forward_a == 2'b01) ? ex_mem_alu_result : wb_data;
            ex_stall_saved_a_valid <= 1'b1;
        end
        if (forward_b != 2'b00) begin
            ex_stall_saved_b <= (forward_b == 2'b01) ? ex_mem_alu_result : wb_data;
            ex_stall_saved_b_valid <= 1'b1;
        end
    end
end

// Forwarding mux for ALU input A, with stall-capture fallback
assign ex_alu_a = (forward_a == 2'b01) ? ex_mem_alu_result :
                  (forward_a == 2'b10) ? wb_data :
                  ex_stall_saved_a_valid ? ex_stall_saved_a :
                  ex_areg_data;  // Use regbank output directly

// Forwarding mux for ALU input B (before const selection), with stall-capture fallback
wire [31:0] ex_breg_forwarded;
assign ex_breg_forwarded = (forward_b == 2'b01) ? ex_mem_alu_result :
                           (forward_b == 2'b10) ? wb_data :
                           ex_stall_saved_b_valid ? ex_stall_saved_b :
                           ex_breg_data;  // Use regbank output directly

// ALU source B: register or constant
assign ex_alu_b = id_ex_alu_use_const ? id_ex_const_alu : ex_breg_forwarded;

ALU alu (
    .a      (ex_alu_a),
    .b      (ex_alu_b),
    .opcode (id_ex_alu_op),
    .y      (ex_alu_result)
);

// ---- MULTI-CYCLE ALU (Division, Multiplication) - STATE MACHINE ----
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
wire [63:0] malu_fp_result;  // 64-bit result from FMUL

// ---- FP64 COPROCESSOR ----
// 8 x 64-bit register file for 64-bit fixed-point operations
// Combinational reads (data available same cycle), synchronous writes

(* ramstyle = "logic" *) reg [63:0] fp_regs [0:7];

// FP register addresses extracted from instruction fields (lower 3 bits)
wire [2:0] fp_areg = id_ex_areg[2:0];
wire [2:0] fp_breg = id_ex_breg[2:0];
wire [2:0] fp_dreg = id_ex_dreg[2:0];

// Combinational read ports
wire [63:0] fp_a_data = fp_regs[fp_areg];
wire [63:0] fp_b_data = fp_regs[fp_breg];

// Identify single-cycle FP operations that should NOT stall the pipeline
wire is_fp_singlecycle = id_ex_arithm && (
    id_ex_alu_op == 4'b1001 ||  // FADD
    id_ex_alu_op == 4'b1010 ||  // FSUB
    id_ex_alu_op == 4'b1011 ||  // FLD
    id_ex_alu_op == 4'b1100 ||  // FSTHI
    id_ex_alu_op == 4'b1101     // FSTLO
);

// Identify FP operations that write to FP register file (not CPU register file)
wire fp_writes_to_fpregs = id_ex_arithm && (
    id_ex_alu_op == 4'b1000 ||  // FMUL
    id_ex_alu_op == 4'b1001 ||  // FADD
    id_ex_alu_op == 4'b1010 ||  // FSUB
    id_ex_alu_op == 4'b1011     // FLD
);

// 64-bit adder/subtractor (combinational)
wire [63:0] fp_add_result = fp_a_data + fp_b_data;
wire [63:0] fp_sub_result = fp_a_data - fp_b_data;

// FP register write data selection
wire [63:0] fp_write_data =
    (id_ex_alu_op == 4'b1001)  ? fp_add_result :          // FADD
    (id_ex_alu_op == 4'b1010)  ? fp_sub_result :          // FSUB
    (id_ex_alu_op == 4'b1011)  ? {ex_alu_a, ex_alu_b} :   // FLD: {rA, rB}
    64'd0;

// FP register write enable for single-cycle operations
// Uses a "write-once" flag to avoid repeated writes during pipeline stalls.
// This removes ex_pipeline_stall from the combinational write-enable path,
// breaking the critical timing path through cache_line_hazard → pipeline_stall.
reg fp_write_pending = 1'b0;
wire fp_singlecycle_we = id_ex_valid && is_fp_singlecycle && fp_writes_to_fpregs
                         && !fp_write_pending;

// fp_write_pending: set after writing, cleared when stall ends (instruction advances)
always @(posedge clk) begin
    if (reset)
        fp_write_pending <= 1'b0;
    else if (!ex_pipeline_stall)
        fp_write_pending <= 1'b0;   // Instruction advancing, reset for next
    else if (fp_singlecycle_we)
        fp_write_pending <= 1'b1;   // Written, block further writes during stall
end

// FP register file write (synchronous)
always @(posedge clk) begin
    if (reset) begin
        // Initialize FP registers to 0
        fp_regs[0] <= 64'd0;
        fp_regs[1] <= 64'd0;
        fp_regs[2] <= 64'd0;
        fp_regs[3] <= 64'd0;
        fp_regs[4] <= 64'd0;
        fp_regs[5] <= 64'd0;
        fp_regs[6] <= 64'd0;
        fp_regs[7] <= 64'd0;
    end else if (fp_singlecycle_we) begin
        fp_regs[fp_dreg] <= fp_write_data;
    end else if (malu_done && id_ex_alu_op == 4'b1000) begin
        // FMUL writeback: pipeline is stalled so fp_dreg is still valid
        fp_regs[fp_dreg] <= malu_fp_result;
    end
end

// Stall pipeline while multi-cycle ALU is in progress
// Single-cycle FP operations (fadd, fsub, fld, fsthi, fstlo) do NOT stall
// Must also check !pc_redirect: if MEM stage resolved a branch/jump, the
// instruction in EX is speculative and must NOT start the multi-cycle ALU.
// (Using pc_redirect instead of flush_id_ex avoids a combinational loop through
// pipeline_stall → interrupt_executes → flush_id_ex → multicycle_stall.)
assign multicycle_stall = id_ex_valid && id_ex_arithm && !is_fp_singlecycle && !malu_request_finished && !pc_redirect;


MultiCycleALU multi_cycle_alu (
    .clk        (clk),
    .reset      (reset),
    .start      (malu_start_reg),
    .a          (malu_a_reg),
    .b          (malu_b_reg),
    .opcode     (malu_opcode_reg),
    .y          (malu_result),
    .done       (malu_done),
    // FP64 coprocessor ports
    .fp_a       (fp_a_data),
    .fp_b       (fp_b_data),
    .fp_result  (malu_fp_result)
);

// Multi-cycle ALU state machine
always @(posedge clk)
begin
    if (reset)
    begin
        malu_state <= MALU_IDLE;
        malu_start_reg <= 1'b0;
        malu_a_reg <= 32'd0;
        malu_b_reg <= 32'd0;
        malu_opcode_reg <= 4'd0;
        malu_request_finished <= 1'b0;
        malu_result_reg <= 32'd0;
    end else
    begin
        case (malu_state)
            MALU_IDLE:
            begin
                malu_request_finished <= 1'b0;
                malu_result_reg <= 32'd0;

                // Start when we have a valid multi-cycle arithm instruction
                // Single-cycle FP ops (fadd, fsub, fld, fsthi, fstlo) must NOT start the MALU
                if (id_ex_valid && id_ex_arithm && !malu_request_finished && !is_fp_singlecycle && !pc_redirect)
                begin
                    malu_start_reg <= 1'b1;
                    malu_a_reg <= ex_alu_a;
                    malu_b_reg <= ex_alu_b;
                    malu_opcode_reg <= id_ex_alu_op;
                    malu_state <= MALU_STARTED;
                end
            end

            MALU_STARTED:
            begin
                // Clear start signal after one cycle
                malu_start_reg <= 1'b0;
                malu_a_reg <= 32'd0;
                malu_b_reg <= 32'd0;
                malu_opcode_reg <= 4'd0;
                malu_state <= MALU_DONE;
            end

            MALU_DONE:
            begin
                // Wait for multi-cycle ALU to complete
                if (malu_done)
                begin
                    malu_result_reg <= malu_result;
                    malu_request_finished <= 1'b1;
                    malu_state <= MALU_IDLE;
                end
            end
        endcase
    end
end

// ---- BRANCH/JUMP UNIT (MEM Stage) ----
// Branch resolution is done in MEM stage to improve timings
wire        jump_valid;
wire [31:0] jump_addr;

BranchJumpUnit branch_jump_unit (
    .branch_op  (ex_mem_branch_op),
    .data_a     (ex_mem_areg_data),
    .data_b     (ex_mem_breg_data),
    .const16    (ex_mem_const16),
    .const27    (ex_mem_const27),
    .pc         (ex_mem_pc),
    .halt       (ex_mem_halt),
    .branch     (ex_mem_is_branch),
    .jumpc      (ex_mem_is_jump),
    .jumpr      (ex_mem_is_jumpr),
    .oe         (ex_mem_oe),
    .sig        (ex_mem_sig),
    .jump_addr  (jump_addr),
    .jump_valid (jump_valid)
);

// PC redirect logic
assign pc_redirect = ex_mem_valid && jump_valid;
assign pc_redirect_target = jump_addr;

// ---- STACK ----
wire [31:0] stack_q;
wire        stack_push;
wire        stack_pop;

assign stack_push = ex_mem_valid && ex_mem_push && !backend_pipeline_stall;
assign stack_pop = ex_mem_valid && ex_mem_pop && !backend_pipeline_stall;

// Stack pointer access
wire [7:0]  stack_ptr_out;
wire [7:0]  stack_ptr_in;
wire        stack_ptr_we;

Stack stack (
    .clk     (clk),
    .reset   (reset),
    .d       (ex_mem_breg_data),
    .q       (stack_q),
    .push    (stack_push),
    .pop     (stack_pop),
    .clear   (1'b0),
    .hold    (backend_pipeline_stall),
    .ptr_out (stack_ptr_out),
    .ptr_in  (stack_ptr_in),
    .ptr_we  (stack_ptr_we)
);

// CPU-internal I/O: stack pointer write via store instruction
assign stack_ptr_we = ex_mem_valid && ex_mem_mem_write &&
                      (ex_mem_mem_addr == CPU_IO_HW_STACK_PTR) && !backend_pipeline_stall;
assign stack_ptr_in = ex_mem_breg_data[7:0];

// ---- PIPELINE CONTROLLER ----

// ---- Fast cache-line hazard inputs (uses registered base, no forwarding mux) ----
// The cache_line_hazard detection previously used ex_alu_a (which depends on the
// forwarding mux) to compute ex_full_addr via a 27-bit adder. This created a
// critical timing path: id_ex_areg → forwarding mux → adder → hazard → stall → regbank.
//
// Fix: use the registered base address (Regbank output or stall shadow register)
// for cache line computation. When forwarding is active (forward_a != 00), we
// conservatively assert the hazard — this is safe (at most 1 extra stall cycle)
// since cache_line_hazard only lasts 1 cycle anyway.
wire [31:0] ex_hazard_base = ex_stall_saved_a_valid ? ex_stall_saved_a : ex_areg_data;
wire [11:0] ex_hazard_addr_low = ex_hazard_base[11:0] + id_ex_const16[11:0];
wire [6:0]  ex_hazard_cache_line = ex_hazard_addr_low[11:5];
wire        ex_hazard_is_sdram = (ex_hazard_base < 32'h1C000000);
wire        ex_hazard_forward_active = (forward_a != 2'b00);

PipelineController pipeline_controller (
    // Forwarding inputs
    .ex_mem_dreg_we     (ex_mem_dreg_we),
    .ex_mem_dreg        (ex_mem_dreg),
    .ex_mem_mem_read    (ex_mem_mem_read),
    .ex_mem_pop         (ex_mem_pop),
    .id_ex_areg         (id_ex_areg),
    .id_ex_breg         (id_ex_breg),
    .mem_wb_dreg_we     (mem_wb_dreg_we),
    .mem_wb_dreg        (mem_wb_dreg),
    // Hazard detection inputs
    .id_ex_valid        (id_ex_valid),
    .id_ex_mem_read     (id_ex_mem_read),
    .id_ex_dreg         (id_ex_dreg),
    .id_ex_pop          (id_ex_pop),
    .id_areg            (id_areg),
    .id_breg            (id_breg),
    .if_id_valid        (if_id_valid),
    .id_ex_mem_write    (id_ex_mem_write),
    .ex_mem_valid       (ex_mem_valid),
    .ex_mem_mem_write   (ex_mem_mem_write),
    .mem_sel_sdram      (mem_sel_sdram),
    .ex_mem_mem_addr    (ex_mem_mem_addr),
    // Pre-computed cache-line hazard inputs (registered base, no forwarding mux)
    .ex_hazard_cache_line     (ex_hazard_cache_line),
    .ex_hazard_is_sdram       (ex_hazard_is_sdram),
    .ex_hazard_forward_active (ex_hazard_forward_active),
    // Stall source inputs
    .cache_stall_if     (cache_stall_if),
    .cache_stall_mem    (cache_stall_mem),
    .multicycle_stall   (multicycle_stall),
    .mu_stall           (mu_stall),
    .cc_stall           (cc_stall),
    .vrampx_stall       (vrampx_stall),
    // Flush source inputs
    .pc_redirect        (pc_redirect),
    .reti_valid         (reti_valid),
    .interrupt_valid    (interrupt_valid),
    // Outputs
    .forward_a              (forward_a),
    .forward_b              (forward_b),
    .pipeline_stall         (pipeline_stall),
    .ex_pipeline_stall      (ex_pipeline_stall),
    .backend_pipeline_stall (backend_pipeline_stall),
    .flush_if_id            (flush_if_id),
    .flush_id_ex            (flush_id_ex),
    .flush_ex_mem           (flush_ex_mem),
    .cache_line_hazard      (cache_line_hazard)
);

// ---- ID/EX STAGE REGISTER UPDATE ----
always @(posedge clk)
begin
    if (reset || flush_id_ex)
    begin
        id_ex_pc <= 32'd0;
        id_ex_valid <= 1'b0;
        id_ex_dreg <= 4'd0;
        id_ex_areg <= 4'd0;
        id_ex_breg <= 4'd0;
        id_ex_alu_op <= 4'd0;
        id_ex_branch_op <= 3'd0;
        id_ex_const_alu <= 32'd0;
        id_ex_const27 <= 27'd0;
        id_ex_const16 <= 32'd0;
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
        id_ex_get_int_id <= 1'b0;
        id_ex_get_pc <= 1'b0;
        id_ex_clear_cache <= 1'b0;
        id_ex_arithm <= 1'b0;
        id_ex_mem_size <= 2'b00;
        id_ex_mem_sign_extend <= 1'b0;
    end else if (!pipeline_stall)
    begin
        id_ex_pc <= if_id_pc;
        id_ex_valid <= if_id_valid;
        // NOTE: Register data comes from regbank with 1-cycle latency
        // No need to pipeline it here
        id_ex_dreg <= id_dreg;
        id_ex_areg <= id_areg;
        id_ex_breg <= id_breg;
        id_ex_alu_op <= id_alu_op;
        id_ex_branch_op <= id_branch_op;
        id_ex_const_alu <= id_alu_use_constu ? id_const_aluu : id_const_alu;
        id_ex_const27 <= id_const27;
        id_ex_const16 <= id_const16;
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
        id_ex_get_int_id <= id_get_int_id && if_id_valid;
        id_ex_get_pc <= id_get_pc && if_id_valid;
        id_ex_clear_cache <= id_clear_cache && if_id_valid;
        id_ex_arithm <= id_arithm && if_id_valid;
        id_ex_mem_size <= id_mem_size;
        id_ex_mem_sign_extend <= id_mem_sign_extend;
    end else if (!ex_pipeline_stall)
    begin
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
        id_ex_get_int_id <= 1'b0;
        id_ex_get_pc <= 1'b0;
        id_ex_clear_cache <= 1'b0;
        id_ex_arithm <= 1'b0;
    end
end

// ---- MEMORY STAGE ----
wire [31:0] ex_mem_addr_calc;
wire        mem_sel_sdram;
wire [31:0] mem_read_data;

MemoryStage #(
    .CPU_IO_PC_BACKUP    (CPU_IO_PC_BACKUP),
    .CPU_IO_HW_STACK_PTR (CPU_IO_HW_STACK_PTR)
) mem_stage (
    .clk                        (clk),
    .reset                      (reset),
    // EX stage inputs for address calculation
    .ex_alu_a                   (ex_alu_a),
    .id_ex_const16              (id_ex_const16),
    .id_ex_valid                (id_ex_valid),
    .id_ex_mem_read             (id_ex_mem_read),
    .id_ex_mem_write            (id_ex_mem_write),
    .ex_pipeline_stall          (ex_pipeline_stall),
    // Pipeline control
    .backend_pipeline_stall     (backend_pipeline_stall),
    // EX/MEM register inputs
    .ex_mem_valid               (ex_mem_valid),
    .ex_mem_mem_read            (ex_mem_mem_read),
    .ex_mem_mem_write           (ex_mem_mem_write),
    .ex_mem_mem_addr            (ex_mem_mem_addr),
    .ex_mem_breg_data           (ex_mem_breg_data),
    .ex_mem_pc                  (ex_mem_pc),
    .ex_mem_clear_cache         (ex_mem_clear_cache),
    // Memory size control
    .ex_mem_mem_size            (ex_mem_mem_size),
    .ex_mem_mem_sign_extend     (ex_mem_mem_sign_extend),
    // CPU-internal I/O read sources
    .pc_backup                  (pc_backup),
    .stack_ptr_out              (stack_ptr_out),
    // ROM data port
    .rom_mem_q                  (rom_mem_q),
    .rom_mem_addr               (rom_mem_addr),
    // VRAM32
    .vram32_q                   (vram32_q),
    .vram32_addr                (vram32_addr),
    .vram32_d                   (vram32_d),
    .vram32_we                  (vram32_we),
    // VRAM8
    .vram8_q                    (vram8_q),
    .vram8_addr                 (vram8_addr),
    .vram8_d                    (vram8_d),
    .vram8_we                   (vram8_we),
    // VRAMPX
    .vramPX_q                   (vramPX_q),
    .vramPX_addr                (vramPX_addr),
    .vramPX_d                   (vramPX_d),
    .vramPX_we                  (vramPX_we),
    .vramPX_fifo_full           (vramPX_fifo_full),
    // Pixel Palette
    .palette_we                 (palette_we),
    .palette_addr               (palette_addr),
    .palette_wdata              (palette_wdata),
    // L1D cache pipeline port
    .l1d_pipe_q                 (l1d_pipe_q),
    .l1d_pipe_addr              (l1d_pipe_addr),
    // L1D cache controller
    .l1d_cache_controller_done  (l1d_cache_controller_done),
    .l1d_cache_controller_result(l1d_cache_controller_result),
    .l1d_cache_controller_addr  (l1d_cache_controller_addr),
    .l1d_cache_controller_data  (l1d_cache_controller_data),
    .l1d_cache_controller_we    (l1d_cache_controller_we),
    .l1d_cache_controller_start (l1d_cache_controller_start),
    .l1d_cache_controller_byte_enable(l1d_cache_controller_byte_enable),
    // Memory Unit (I/O)
    .mu_done                    (mu_done),
    .mu_q                       (mu_q),
    .mu_start                   (mu_start),
    .mu_addr                    (mu_addr),
    .mu_data                    (mu_data),
    .mu_we                      (mu_we),
    // Cache clear
    .l1_clear_cache_done        (l1_clear_cache_done),
    .l1_clear_cache             (l1_clear_cache),
    // Outputs
    .ex_mem_addr_calc           (ex_mem_addr_calc),
    .mem_sel_sdram              (mem_sel_sdram),
    .cache_stall_mem            (cache_stall_mem),
    .mu_stall                   (mu_stall),
    .cc_stall                   (cc_stall),
    .vrampx_stall               (vrampx_stall),
    .mem_read_data              (mem_read_data)
);

// ---- EX/MEM STAGE REGISTER UPDATE ----

// Result selection for EX stage (before going to MEM)
// SAVPC uses PC, INTID uses interrupt ID, otherwise ALU result
// For multi-cycle ALU: use malu_result directly when done (not the registered value,
// which hasn't been updated yet on the same clock edge)
// For FP store: extract 32-bit half from FP register
wire [31:0] ex_result = id_ex_get_pc    ? id_ex_pc :
                        id_ex_get_int_id ? {24'd0, int_id} :
                        (is_fp_singlecycle && id_ex_alu_op == 4'b1100) ? fp_a_data[63:32] :  // FSTHI
                        (is_fp_singlecycle && id_ex_alu_op == 4'b1101) ? fp_a_data[31:0]  :  // FSTLO
                        (id_ex_arithm && malu_done) ? malu_result :
                        id_ex_arithm   ? malu_result_reg :
                        ex_alu_result;

always @(posedge clk)
begin
    if (reset || flush_ex_mem)
    begin
        // Reset or flush on branch/jump taken in MEM stage
        ex_mem_pc <= 32'd0;
        ex_mem_valid <= 1'b0;
        ex_mem_alu_result <= 32'd0;
        ex_mem_breg_data <= 32'd0;
        ex_mem_dreg <= 4'd0;
        ex_mem_mem_addr <= 32'd0;
        ex_mem_dreg_we <= 1'b0;
        ex_mem_mem_read <= 1'b0;
        ex_mem_mem_write <= 1'b0;
        ex_mem_push <= 1'b0;
        ex_mem_pop <= 1'b0;
        ex_mem_halt <= 1'b0;
        ex_mem_clear_cache <= 1'b0;
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
        ex_mem_mem_size <= 2'b00;
        ex_mem_mem_sign_extend <= 1'b0;
    end else if (!ex_pipeline_stall)
    begin
        ex_mem_pc <= id_ex_pc;
        ex_mem_valid <= id_ex_valid;
        ex_mem_alu_result <= ex_result;
        ex_mem_breg_data <= ex_breg_forwarded;
        ex_mem_dreg <= id_ex_dreg;
        ex_mem_mem_addr <= ex_mem_addr_calc;
        ex_mem_dreg_we <= id_ex_dreg_we && !fp_writes_to_fpregs;
        ex_mem_mem_read <= id_ex_mem_read;
        ex_mem_mem_write <= id_ex_mem_write;
        ex_mem_push <= id_ex_push;
        ex_mem_pop <= id_ex_pop;
        ex_mem_halt <= id_ex_halt;
        ex_mem_clear_cache <= id_ex_clear_cache;
        // Branch control signals for MEM-stage resolution
        ex_mem_is_branch <= id_ex_is_branch;
        ex_mem_is_jump <= id_ex_is_jump;
        ex_mem_is_jumpr <= id_ex_is_jumpr;
        ex_mem_branch_op <= id_ex_branch_op;
        ex_mem_sig <= id_ex_sig;
        ex_mem_oe <= id_ex_oe;
        ex_mem_const16 <= id_ex_const16;
        ex_mem_const27 <= id_ex_const27;
        ex_mem_areg_data <= ex_alu_a;
        // Memory size control
        ex_mem_mem_size <= id_ex_mem_size;
        ex_mem_mem_sign_extend <= id_ex_mem_sign_extend;
    end else if (cache_line_hazard && !backend_pipeline_stall)
    begin
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

// ---- MEM/WB STAGE REGISTER UPDATE ----

always @(posedge clk)
begin
    if (reset)
    begin
        mem_wb_valid <= 1'b0;
        mem_wb_alu_result <= 32'd0;
        mem_wb_mem_data <= 32'd0;
        mem_wb_stack_data <= 32'd0;
        mem_wb_dreg <= 4'd0;
        mem_wb_dreg_we <= 1'b0;
        mem_wb_mem_read <= 1'b0;
        mem_wb_pop <= 1'b0;
        mem_wb_halt <= 1'b0;
    end else if (!backend_pipeline_stall)
    begin
        mem_wb_valid <= ex_mem_valid;
        mem_wb_alu_result <= ex_mem_alu_result;
        mem_wb_mem_data <= mem_read_data;
        mem_wb_stack_data <= stack_q;
        mem_wb_dreg <= ex_mem_dreg;
        mem_wb_dreg_we <= ex_mem_dreg_we;
        mem_wb_mem_read <= ex_mem_mem_read;
        mem_wb_pop <= ex_mem_pop;
        mem_wb_halt <= ex_mem_halt;
    end
end

// ---- WRITEBACK STAGE (WB) ----

// Result selection - compute in WB stage based on instruction type
// For pop: stack_q arrives in WB cycle
// For memory read: use mem_wb_mem_data (captured in MEM stage)
// Otherwise: use ALU result
assign wb_data = mem_wb_pop ? stack_q :
                 mem_wb_mem_read ? mem_wb_mem_data :
                 mem_wb_alu_result;

assign wb_dreg = mem_wb_dreg;
assign wb_dreg_we = mem_wb_valid && mem_wb_dreg_we && (mem_wb_dreg != 4'd0) && !backend_pipeline_stall;

endmodule

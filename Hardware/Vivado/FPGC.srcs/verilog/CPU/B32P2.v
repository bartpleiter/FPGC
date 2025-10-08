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
    parameter INTERRUPT_JUMP_ADDR = 32'd1, // Address to jump to when an interrupt is triggered
    parameter NUM_INTERRUPTS = 8 // Number of interrupt lines (currently requires changing InterruptController.v as well)
) (
    //========================
    // System interface
    //========================
    input  wire         clk,
    input  wire         reset,

    //========================
    // ROM (dual port)
    //========================
    output wire [8:0]   rom_fe_addr,
    output wire         rom_fe_oe,
    input  wire [31:0]  rom_fe_q,
    output wire         rom_fe_hold,

    output wire [8:0]   rom_mem_addr,
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
    //========================
    output wire [6:0]   l1i_pipe_addr,
    input  wire [273:0] l1i_pipe_q,

    //========================
    // L1d cache (CPU pipeline port)
    //========================
    output wire [6:0]   l1d_pipe_addr,
    input  wire [273:0] l1d_pipe_q,

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

// Flush and Stall signals
wire flush_FE1;
wire flush_FE2;
wire flush_REG;
wire flush_EXMEM1;

assign flush_FE1 = jump_valid_EXMEM1 || hazard_pc1_pc2 || hazard_pc2_pc1 || reti_EXMEM1 || interrupt_valid;
assign flush_FE2 = jump_valid_EXMEM1 || hazard_pc1_pc2 || hazard_pc2_pc1 || reti_EXMEM1 || interrupt_valid;
assign flush_REG = jump_valid_EXMEM1 || hazard_pc1_pc2 || hazard_pc2_pc1 || reti_EXMEM1 || interrupt_valid;
assign flush_EXMEM1 = exmem1_uses_exmem2_result;

assign l1i_cache_controller_flush = l1i_cache_controller_flush_reg;

wire stall_FE1;
wire stall_FE2;
wire stall_REG;
wire stall_EXMEM1;

// Stall all previous stages on EXMEM2 busy
assign stall_FE1 = exmem1_uses_exmem2_result || multicycle_alu_stall || l1i_cache_miss_FE2 || l1d_cache_wait_EXMEM2 || mu_stall || cc_stall;
assign stall_FE2 = exmem1_uses_exmem2_result || multicycle_alu_stall || l1d_cache_wait_EXMEM2 || mu_stall || cc_stall;
assign stall_REG = exmem1_uses_exmem2_result || multicycle_alu_stall || l1d_cache_wait_EXMEM2 || mu_stall || cc_stall;
assign stall_EXMEM1 = multicycle_alu_stall || l1d_cache_wait_EXMEM2 || mu_stall || cc_stall;

wire multicycle_alu_stall;
assign multicycle_alu_stall = arithm_EXMEM2 && !multicycle_alu_done_EXMEM2;

wire mu_stall;
assign mu_stall = mem_io_EXMEM2 && !mu_request_finished_EXMEM2;
wire cc_stall;
assign cc_stall = clearCache_EXMEM2 && !cc_request_finished_EXMEM2;

// Possible hazard situations:

// - EXMEM1 uses result of non-ALU operation from EXMEM2 -> stall
// Note: in case of multi cycle operation (cache miss or ALU), only set this signal high on the last cycle!
// TODO: when adding support for more multicycle memory, add here as well!
wire exmem1_uses_exmem2_result;
assign exmem1_uses_exmem2_result = 
    (pop_EXMEM2 || (mem_read_EXMEM2 && (l1d_cache_hit_EXMEM2 || !mem_multicycle_EXMEM2)) || was_cache_miss_EXMEM2 || mu_request_finished_EXMEM2 ||
    (arithm_EXMEM2 && multicycle_alu_done_EXMEM2)) && 
    (dreg_EXMEM2 == areg_EXMEM1 || dreg_EXMEM2 == breg_EXMEM1);

// - EXMEM1 uses result of multicycle EXMEM2 at PC-1 and dreg of PC-2 -> jump to same address to resolve
// Note: because this is hard to describe as a variable name, we will call this situation hazard_pc1_pc2
wire hazard_pc1_pc2;
assign hazard_pc1_pc2 = 
    ( (mem_read_EXMEM2 && mem_multicycle_EXMEM2) || (mem_write_EXMEM2 && mem_io_EXMEM2) || arithm_EXMEM2 || l1d_cache_wait_EXMEM2 ) &&
    ( ( (areg_EXMEM1 == addr_d_WB) && areg_EXMEM1 != 4'd0) || ( (breg_EXMEM1 == addr_d_WB) && breg_EXMEM1 != 4'd0) );

// - EXMEM1 uses result of multicycle EXMEM2 at PC-2 and dreg of PC-1 -> jump to same address to resolve
// Note: because this is hard to describe as a variable name, we will call this situation hazard_pc2_pc1
wire hazard_pc2_pc1;
assign hazard_pc2_pc1 = 
    ( (mem_read_WB && mem_multicycle_WB) || (mem_write_WB && mem_io_WB) || arithm_WB || l1d_cache_wait_EXMEM2 ) &&
    ( ( (areg_EXMEM1 == dreg_EXMEM2) && areg_EXMEM1 != 4'd0) || ( (breg_EXMEM1 == dreg_EXMEM2) && breg_EXMEM1 != 4'd0) );

// Forwarding situations
// EXMEM2 -> EXMEM1 (single cycle ALU operations)
// WB -> EXMEM1 (single cycle ALU operations)
wire [1:0] forward_a; // From how many stages ahead to forward from towards ALU input A
wire [1:0] forward_b; // From how many stages ahead to forward from towards ALU input B

// Interrupts
reg intDisabled_FE1 = 1'b0;
wire intCPU;
wire [7:0] intID;

InterruptController interruptController(
.clk(clk),
.reset(reset),

.interrupts(interrupts),

.intDisabled(intDisabled_FE1),
.intCPU(intCPU),
.intID(intID)
);

wire interrupt_valid;

assign interrupt_valid = (
    intCPU && !intDisabled_FE1 &&
    PC_EXMEM1 < ROM_ADDRESS && // We ignore interrupts when executing from ROM (this assumes SDRAM is placed before ROM in the memory map)
    jump_valid_EXMEM1 // We only allow interrupts during jumps to limit possible hazard situations
);

// Program counter backup for interrupts
reg [31:0] PC_backup = 32'd0;


// Program counter update logic
reg [31:0] PC_FE1 = ROM_ADDRESS; 
always @(posedge clk)
begin
    if (reset)
    begin
        PC_FE1 <= ROM_ADDRESS;
        intDisabled_FE1 <= 1'b0;
        PC_backup <= 32'd0;
    end
    else
    begin
        if (interrupt_valid)
        begin
            intDisabled_FE1 <= 1'b1;
            PC_backup <= PC_EXMEM1; // Backup current PC (in EXMEM1 stage)
            PC_FE1 <= INTERRUPT_JUMP_ADDR;
        end
        else if (reti_EXMEM1)
        begin
            intDisabled_FE1 <= 1'b0;
            PC_FE1 <= PC_backup;
        end
        else if (jump_valid_EXMEM1)
        begin
            PC_FE1 <= jump_addr_EXMEM1;
        end
        else if (hazard_pc1_pc2 || hazard_pc2_pc1)
        begin
            PC_FE1 <= PC_EXMEM1;
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
assign l1i_pipe_addr = PC_FE1[9:3]; // Address of the cache line

// Forward to next stage
wire [31:0] PC_FE2;
Regr #(
    .N(32)
) regr_PC_FE1_FE2 (
    .clk (clk),
    .in(PC_FE1),
    .out(PC_FE2),
    .hold(stall_FE1),
    .clear(flush_FE1 || reset)
);

// Signal to make sure we do not detect cache misses on invalid memory accesses (e.g. during pipeline bubbles for address 0)
wire valid_mem_access_FE2;
Regr #(
    .N(1)
) regr_valid_mem_access_FE1_FE2 (
    .clk (clk),
    .in(!mem_rom_FE1 && !flush_FE1),
    .out(valid_mem_access_FE2),
    .hold(1'b0),
    .clear(reset)
);

// Signal to tag bubbles for FE2 to ignore
wire n_bubble_FE2;
Regr #(
    .N(1)
) regr_bubble_FE1_FE2 (
    .clk (clk),
    .in(1'b1),
    .out(n_bubble_FE2),
    .hold(stall_FE1),
    .clear(flush_FE1 || reset)
);
wire bubble_FE2 = !n_bubble_FE2;

/*
 * Stage 2: Instruction Cache Miss Fetch
 */

// ROM access
wire mem_rom_FE2;
assign mem_rom_FE2 = PC_FE2 >= ROM_ADDRESS;
wire [31:0] rom_q_FE2 = rom_fe_q;

// L1i cache hit decoding

wire [15:0] l1i_tag_FE2 = PC_FE2[25:10]; // Tag for the cache line
wire [2:0] l1i_offset_FE2 = PC_FE2[2:0]; // Offset within the cache line

wire l1i_cache_hit_FE2 = 
    valid_mem_access_FE2 &&
    !bubble_FE2 &&
    (l1i_tag_FE2 == l1i_pipe_q[17:2]) &&
    l1i_pipe_q[1]; // Valid bit

wire [31:0] l1i_cache_hit_q_FE2 = l1i_pipe_q[32 * l1i_offset_FE2 + 18 +: 32]; // Note that the +: is used to select base +: width

// L1i cache miss handling
wire l1i_cache_miss_FE2;
assign l1i_cache_miss_FE2 = l1i_cache_miss_state_FE2 != L1I_CACHE_RESULT_READY && !bubble_FE2 && valid_mem_access_FE2 && !(l1i_tag_FE2 == l1i_pipe_q[17:2] && l1i_pipe_q[1]);

// Cache miss state machine
reg [1:0] l1i_cache_miss_state_FE2 = 2'b00;
reg l1i_cache_controller_start_reg = 1'b0;
reg l1i_cache_controller_flush_reg = 1'b0;

localparam L1I_CACHE_IDLE = 2'b00;
localparam L1I_CACHE_STARTED = 2'b01;
localparam L1I_CACHE_WAIT_DONE = 2'b10;
localparam L1I_CACHE_RESULT_READY = 2'b11;

reg [31:0] saved_cache_result_FE2 = 32'd0; // To save the result from the cache controller on cache miss

always @(posedge clk) begin
    if (reset) begin
        l1i_cache_miss_state_FE2 <= L1I_CACHE_IDLE;
        l1i_cache_controller_start_reg <= 1'b0;
        l1i_cache_controller_flush_reg <= 1'b0;
        saved_cache_result_FE2 <= 32'd0;
    end else begin
        case (l1i_cache_miss_state_FE2)
            L1I_CACHE_IDLE: begin
                l1i_cache_controller_start_reg <= 1'b0;
                l1i_cache_controller_flush_reg <= 1'b0;
                // Do not start on flush
                if (l1i_cache_miss_FE2 && !flush_FE2) begin
                    l1i_cache_controller_start_reg <= 1'b1;
                    l1i_cache_miss_state_FE2 <= L1I_CACHE_STARTED;
                end
            end
            
            L1I_CACHE_STARTED: begin
                // Request has been started, so if there is a flush we need to abort it
                // If we suddenly get a cache hit after starting (because of ignore signal), we can abort this request too
                if (flush_FE2 || (l1i_tag_FE2 == l1i_pipe_q[17:2] && l1i_pipe_q[1])) begin
                    l1i_cache_miss_state_FE2 <= L1I_CACHE_IDLE;
                    l1i_cache_controller_start_reg <= 1'b0;
                    l1i_cache_controller_flush_reg <= 1'b1;
                end
                else begin
                    l1i_cache_controller_start_reg <= 1'b0;
                    l1i_cache_miss_state_FE2 <= L1I_CACHE_WAIT_DONE;
                end
            end
            
            L1I_CACHE_WAIT_DONE: begin
                // Request has been started, so if there is a flush we need to abort it
                // If we suddenly get a cache hit after starting (because of ignore signal), we can abort this request too
                if (flush_FE2 || (l1i_tag_FE2 == l1i_pipe_q[17:2] && l1i_pipe_q[1])) begin
                    l1i_cache_miss_state_FE2 <= L1I_CACHE_IDLE;
                    l1i_cache_controller_flush_reg <= 1'b1;
                end
                else if (l1i_cache_controller_done) begin
                    saved_cache_result_FE2 <= l1i_cache_controller_result;
                    l1i_cache_miss_state_FE2 <= L1I_CACHE_RESULT_READY;
                end
            end

            L1I_CACHE_RESULT_READY: begin
                // If there is a stall active, wait until it is gone before going back to idle to prevent double requests
                // On flush we always go back to idle
                if (!stall_FE2 || flush_FE2)
                begin
                    l1i_cache_miss_state_FE2 <= L1I_CACHE_IDLE;
                    saved_cache_result_FE2 <= 32'd0;
                end
            end
        endcase
    end
end

// Cache controller interface
assign l1i_cache_controller_addr = PC_FE2;
assign l1i_cache_controller_start = l1i_cache_controller_start_reg;


// Result selection for next stage
wire [31:0] instr_result_FE2;
assign instr_result_FE2 =   (mem_rom_FE2) ? rom_q_FE2 :
                            (l1i_cache_miss_state_FE2 == L1I_CACHE_RESULT_READY) ? saved_cache_result_FE2 : // Cache miss result is ready
                            (l1i_cache_miss_FE2) ? 32'b0 : // Forward bubbles when still handling cache miss
                            (l1i_cache_hit_FE2 && !stall_FE2 && !flush_FE2) ? l1i_cache_hit_q_FE2 : // Must be below cache miss checks
                            32'b0;

// Forward to next stage
wire [31:0] instr_REG;
Regr #(
    .N(32)
) regr_instr_FE2_REG (
    .clk (clk),
    .in(instr_result_FE2),
    .out(instr_REG),
    .hold(stall_FE2),
    .clear(flush_FE2 || reset)
);

wire [31:0] PC_REG;
Regr #(
    .N(32)
) regr_PC_FE2_REG (
    .clk (clk),
    .in(PC_FE2),
    .out(PC_REG),
    .hold(stall_FE2),
    .clear(flush_FE2 || reset)
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
    .clear(flush_REG || reset),
    .hold(stall_REG),
    .data_a(data_a_EXMEM1),
    .data_b(data_b_EXMEM1),

    .addr_d(addr_d_WB),
    .data_d(data_d_WB),
    // When EXMEM2 is stalling, we keep the data in the pipeline in case of forwarding
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
    .clear(flush_REG || reset)
);

wire [31:0] PC_EXMEM1;
Regr #(
    .N(32)
) regr_PC_REG_EXMEM1 (
    .clk (clk),
    .in(PC_REG),
    .out(PC_EXMEM1),
    .hold(stall_REG),
    .clear(flush_REG || reset)
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
wire arithm_EXMEM1;

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

wire getIntID_EXMEM1;
wire getPC_EXMEM1;

ControlUnit controlUnit_EXMEM1 (
    .instrOP(instrOP_EXMEM1),
    .aluOP(aluOP_EXMEM1),

    .alu_use_const(alu_use_const_EXMEM1),
    .alu_use_constu(alu_use_constu_EXMEM1),
    .push(),
    .pop(),
    .dreg_we(),
    .mem_write(mem_write_EXMEM1),
    .mem_read(mem_read_EXMEM1),
    .arithm(arithm_EXMEM1),
    .jumpc(jumpc_EXMEM1),
    .jumpr(jumpr_EXMEM1),
    .branch(branch_EXMEM1),
    .halt(halt_EXMEM1),
    .reti(reti_EXMEM1),
    .getIntID(getIntID_EXMEM1),
    .getPC(getPC_EXMEM1),
    .clearCache() // We do not set clearCache here, as we want to stall the pipeline when clearing the cache
);

wire mem_sdram_EXMEM1;
wire mem_multicycle_EXMEM1;
wire [31:0] mem_local_address_EXMEM1;

// TODO optimization in case address calculations are slow:
// Calculate all signals in EXMEM1 and forward them to EXMEM2
// This way the address decoding does not need to be done in EXMEM2
AddressDecoder addressDecoder_EXMEM1 (
    .areg_value(alu_a_EXMEM1), // Use forwarded data
    .const16(const16_EXMEM1),
    .rw(mem_read_EXMEM1 || mem_write_EXMEM1),

    .mem_sdram(mem_sdram_EXMEM1),
    .mem_io(),
    .mem_rom(),
    .mem_vram32(),
    .mem_vram8(),
    .mem_vrampx(),

    .mem_multicycle(mem_multicycle_EXMEM1),
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
    .data_a (alu_a_EXMEM1), // Use forwarded data
    .data_b (alu_b_EXMEM1), // Use forwarded data
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

assign l1d_pipe_addr = mem_local_address_EXMEM1[9:3]; // Address of the cache line

// Result based on operation
wire [31:0] result_EXMEM1;
assign result_EXMEM1 =  (getPC_EXMEM1) ? PC_EXMEM1 :
                        (getIntID_EXMEM1) ? intID :
                        alu_y_EXMEM1;

// Forward to next stage
wire [31:0] instr_EXMEM2;
Regr #(
    .N(32)
) regr_instr_EXMEM1_EXMEM2 (
    .clk (clk),
    .in(instr_EXMEM1),
    .out(instr_EXMEM2),
    .hold(stall_EXMEM1),
    .clear(flush_EXMEM1 || reset)
);

wire [31:0] alu_y_EXMEM2;
Regr #(
    .N(32)
) regr_ALU_EXMEM2 (
    .clk (clk),
    .in(result_EXMEM1), // Use result based on operation
    .out(alu_y_EXMEM2),
    .hold(stall_EXMEM1),
    .clear(flush_EXMEM1 || reset)
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
    .clear(flush_EXMEM1 || reset)
);

wire [31:0] PC_EXMEM2;
Regr #(
    .N(32)
) regr_PC_EXMEM1_EXMEM2 (
    .clk (clk),
    .in(PC_EXMEM1),
    .out(PC_EXMEM2),
    .hold(stall_EXMEM1),
    .clear(flush_EXMEM1 || reset)
);

/*
 * Stage 5: Multi-cycle Execute and Data Cache Miss Handling
 */

wire [3:0] instrOP_EXMEM2;
wire push_EXMEM2;
wire pop_EXMEM2;

wire [3:0] dreg_EXMEM2;

wire mem_read_EXMEM2;
wire mem_write_EXMEM2;
wire [31:0] const16_EXMEM2;

wire arithm_EXMEM2;
wire [3:0] aluOP_EXMEM2;

InstructionDecoder instrDec_EXMEM2 (
    .instr(instr_EXMEM2),

    .instrOP(instrOP_EXMEM2),
    .aluOP(aluOP_EXMEM2),
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

wire clearCache_EXMEM2;

ControlUnit controlUnit_EXMEM2 (
    .instrOP(instrOP_EXMEM2),
    .aluOP(4'd0),

    .alu_use_const(),
    .alu_use_constu(),
    .push(push_EXMEM2),
    .pop(pop_EXMEM2),
    .dreg_we(we_EXMEM2),
    .mem_write(mem_write_EXMEM2),
    .mem_read(mem_read_EXMEM2),
    .arithm(arithm_EXMEM2),
    .jumpc(),
    .jumpr(),
    .branch(),
    .halt(),
    .reti(),
    .getIntID(),
    .getPC(),
    .clearCache(clearCache_EXMEM2)
);

wire [31:0] multicycle_alu_y_EXMEM2;
wire multicycle_alu_done_EXMEM2;
MultiCycleALU multiCycleALU_EXMEM2 (
    .clk(clk),
    .reset(reset),

    .start(arithm_EXMEM2),
    .done(multicycle_alu_done_EXMEM2),
    .a(data_a_EXMEM2),
    .b(data_b_EXMEM2),
    .opcode(aluOP_EXMEM2),
    .y(multicycle_alu_y_EXMEM2)
);

wire mem_multicycle_EXMEM2;
wire [31:0] mem_local_address_EXMEM2;

wire mem_sdram_EXMEM2;
wire mem_io_EXMEM2;
wire mem_vram32_EXMEM2;
wire mem_vram8_EXMEM2;
wire mem_vrampx_EXMEM2;

AddressDecoder addressDecoder_EXMEM2 (
    .areg_value(data_a_EXMEM2),
    .const16(const16_EXMEM2),
    .rw(mem_read_EXMEM2 || mem_write_EXMEM2),

    .mem_sdram(mem_sdram_EXMEM2),
    .mem_io(mem_io_EXMEM2),
    .mem_rom(),
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
    .clear(reset),
    .hold(1'b0),
    .q(stack_q_WB)
);

// ROM
assign rom_mem_addr = mem_local_address_EXMEM2;

// VRAM32
assign vram32_addr = mem_local_address_EXMEM2;
assign vram32_we = mem_vram32_EXMEM2 && mem_write_EXMEM2;
assign vram32_d = data_b_EXMEM2;

// VRAM8
assign vram8_addr = mem_local_address_EXMEM2;
assign vram8_we = mem_vram8_EXMEM2 && mem_write_EXMEM2;
assign vram8_d = data_b_EXMEM2;

// VRAMPX
assign vramPX_addr = mem_local_address_EXMEM2;
assign vramPX_we = mem_vrampx_EXMEM2 && mem_write_EXMEM2;
assign vramPX_d = data_b_EXMEM2;



// L1d cache hit decoding

wire [15:0] l1d_tag_EXMEM2 = mem_local_address_EXMEM2[25:10]; // Tag for the cache line
wire [2:0] l1d_offset_EXMEM2 = mem_local_address_EXMEM2[2:0]; // Offset within the cache line

wire l1d_cache_hit_EXMEM2 = 
    (mem_read_EXMEM2) &&
    (mem_sdram_EXMEM2) &&
    (l1d_tag_EXMEM2 == l1d_pipe_q[17:2]) &&
    l1d_pipe_q[1] && // Valid bit
    l1d_cache_miss_state_EXMEM2 == L1D_CACHE_IDLE; // Make sure we are not currently handling a cache miss

wire [31:0] l1d_cache_hit_q_EXMEM2 = l1d_pipe_q[32 * l1d_offset_EXMEM2 + 18 +: 32]; // Note that the +: is used to select base +: width

// L1d cache miss handling
wire l1d_cache_miss_EXMEM2;
assign l1d_cache_miss_EXMEM2 = mem_read_EXMEM2 && mem_sdram_EXMEM2 && !l1d_cache_hit_EXMEM2;

// Cache miss state machine
reg [1:0] l1d_cache_miss_state_EXMEM2 = 2'b00;
reg l1d_cache_controller_start_reg = 1'b0;

reg was_cache_miss_EXMEM2 = 1'b0;

localparam L1D_CACHE_IDLE = 2'b00;
localparam L1D_CACHE_STARTED = 2'b01;
localparam L1D_CACHE_WAIT_DONE = 2'b10;

always @(posedge clk) begin
    if (reset) begin
        l1d_cache_miss_state_EXMEM2 <= L1D_CACHE_IDLE;
        l1d_cache_controller_start_reg <= 1'b0;
        was_cache_miss_EXMEM2 <= 1'b0;
    end else begin
        case (l1d_cache_miss_state_EXMEM2)
            L1D_CACHE_IDLE: begin
                l1d_cache_controller_start_reg <= 1'b0;
                was_cache_miss_EXMEM2 <= 1'b0;
                if (l1d_cache_miss_EXMEM2 || (mem_write_EXMEM2 && mem_sdram_EXMEM2 && !was_cache_miss_EXMEM2)) begin
                    l1d_cache_controller_start_reg <= 1'b1;
                    l1d_cache_miss_state_EXMEM2 <= L1D_CACHE_STARTED;
                end
            end
            
            L1D_CACHE_STARTED: begin
                l1d_cache_controller_start_reg <= 1'b0;
                l1d_cache_miss_state_EXMEM2 <= L1D_CACHE_WAIT_DONE;
            end
            
            L1D_CACHE_WAIT_DONE: begin
                if (l1d_cache_controller_done) begin
                    l1d_cache_miss_state_EXMEM2 <= L1D_CACHE_IDLE;
                    was_cache_miss_EXMEM2 <= 1'b1;
                end
            end
        endcase
    end
end

// Cache controller interface
assign l1d_cache_controller_addr = mem_local_address_EXMEM2;
assign l1d_cache_controller_data = data_b_EXMEM2;
assign l1d_cache_controller_start = l1d_cache_controller_start_reg;
assign l1d_cache_controller_we = mem_write_EXMEM2;

// Wire to check if the current instruction was a cache miss
wire l1d_cache_wait_EXMEM2 = (l1d_cache_miss_EXMEM2 || (mem_write_EXMEM2 && mem_sdram_EXMEM2)) && !was_cache_miss_EXMEM2;

// State machine for Memory Unit/IO access
localparam MU_IDLE = 2'b00;
localparam MU_STARTED = 2'b01;
localparam MU_DONE = 2'b10;
reg [1:0] mu_state_EXMEM2 = 2'b00;
reg mu_request_finished_EXMEM2 = 1'b0;
reg [31:0] mu_q_EXMEM2 = 32'd0;

always @(posedge clk) begin
    if (reset) begin
        mu_state_EXMEM2 <= MU_IDLE;
        mu_start <= 1'b0;
        mu_addr <= 32'd0;
        mu_data <= 32'd0;
        mu_we <= 1'b0;

        mu_request_finished_EXMEM2 <= 1'b0;
        mu_q_EXMEM2 <= 32'd0;
    end else begin
        case (mu_state_EXMEM2)
            MU_IDLE: begin
                mu_request_finished_EXMEM2 <= 1'b0;
                mu_q_EXMEM2 <= 32'd0;
                // Checking for mem_io_EXMEM2 is enough, as it is only high when read or write is high too (see AddressDecoder)
                if (mem_io_EXMEM2 && !mu_request_finished_EXMEM2) begin
                    mu_start <= 1'b1;
                    mu_addr <= mem_local_address_EXMEM2;
                    mu_data <= data_b_EXMEM2;
                    mu_we <= mem_write_EXMEM2;
                    mu_state_EXMEM2 <= MU_STARTED;
                end
            end
            
            MU_STARTED: begin
                mu_start <= 1'b0;
                mu_addr <= 32'd0;
                mu_data <= 32'd0;
                mu_we <= 1'b0;
                mu_state_EXMEM2 <= MU_DONE;

            end
            
            MU_DONE: begin
                if (mu_done) begin
                    mu_q_EXMEM2 <= mu_q;
                    mu_request_finished_EXMEM2 <= 1'b1;
                    mu_state_EXMEM2 <= MU_IDLE;
                end
            end
        endcase
    end
end

// Clear cache state machine
reg [2:0] clearcache_state_EXMEM2 = MU_IDLE;
localparam CC_IDLE = 2'b00;
localparam CC_STARTED = 2'b01;
localparam CC_DONE = 2'b10;

reg cc_request_finished_EXMEM2 = 1'b0;

always @(posedge clk) begin
    if (reset) begin
        clearcache_state_EXMEM2 <= MU_IDLE;
        l1_clear_cache <= 1'b0;
        cc_request_finished_EXMEM2 <= 1'b0;
        
    end else begin
        case (clearcache_state_EXMEM2)
            CC_IDLE: begin
                l1_clear_cache <= 1'b0;
                cc_request_finished_EXMEM2 <= 1'b0;
                if (clearCache_EXMEM2 && !cc_request_finished_EXMEM2) begin
                    l1_clear_cache <= 1'b1;
                    clearcache_state_EXMEM2 <= CC_STARTED;
                end
            end
            
            CC_STARTED: begin
                l1_clear_cache <= 1'b0;
                clearcache_state_EXMEM2 <= CC_DONE;
            end
            
            CC_DONE: begin
                if (l1_clear_cache_done) begin
                    cc_request_finished_EXMEM2 <= 1'b1;
                    clearcache_state_EXMEM2 <= CC_IDLE;
                end
                
            end
        endcase
    end
end

// Forward to next stage
wire [31:0] instr_WB;
Regr #(
    .N(32)
) regr_instr_EXMEM2_WB (
    .clk (clk),
    .in(instr_EXMEM2),
    .out(instr_WB),
    .hold(1'b0),
    .clear(l1d_cache_wait_EXMEM2 || multicycle_alu_stall || mu_stall || cc_stall || reset) // Insert bubble if EXMEM2 is stalling
);

wire [31:0] alu_y_WB;
Regr #(
    .N(32)
) regr_ALU_EXMEM2_WB (
    .clk (clk),
    .in(alu_y_EXMEM2),
    .out(alu_y_WB),
    .hold(1'b0),
    .clear(reset)
);

wire [31:0] PC_WB;
Regr #(
    .N(32)
) regr_PC_EXMEM2_WB (
    .clk (clk),
    .in(PC_EXMEM2),
    .out(PC_WB),
    .hold(1'b0),
    .clear(reset)
);

wire [31:0] data_a_WB;
Regr #(
    .N(32)
) regr_DATA_A_EXMEM2_WB (
    .clk (clk),
    .in(data_a_EXMEM2),
    .out(data_a_WB),
    .hold(1'b0),
    .clear(reset)
);

wire [31:0] multicycle_alu_y_WB;
Regr #(
    .N(32)
) regr_MULTICYCLE_ALU_EXMEM2_WB (
    .clk (clk),
    .in(multicycle_alu_y_EXMEM2),
    .out(multicycle_alu_y_WB),
    .hold(1'b0),
    .clear(reset)
);

wire [31:0] l1d_cache_hit_q_WB;
Regr #(
    .N(32)
) regr_L1D_CACHE_HIT_EXMEM2_WB (
    .clk (clk),
    .in(l1d_cache_hit_q_EXMEM2),
    .out(l1d_cache_hit_q_WB),
    .hold(1'b0),
    .clear(reset)
);

wire was_cache_miss_WB;
Regr #(
    .N(1)
) regr_L1D_CACHE_MISS_EXMEM2_WB (
    .clk (clk),
    .in(was_cache_miss_EXMEM2),
    .out(was_cache_miss_WB),
    .hold(1'b0),
    .clear(reset)
);

wire [31:0] mu_q_WB;
Regr #(
    .N(32)
) regr_MU_Q_WB (
    .clk (clk),
    .in(mu_q_EXMEM2),
    .out(mu_q_WB),
    .hold(1'b0),
    .clear(reset)
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
wire mem_write_WB;
wire arithm_WB;

ControlUnit controlUnit_WB (
    .instrOP(instrOP_WB),
    .aluOP(4'd0),

    .alu_use_const(),
    .alu_use_constu(),
    .push(),
    .pop(pop_WB),
    .dreg_we(we_WB),
    .mem_write(mem_write_WB),
    .mem_read(mem_read_WB),
    .arithm(arithm_WB),
    .jumpc(),
    .jumpr(),
    .branch(),
    .halt(),
    .reti(),
    .getIntID(),
    .getPC(),
    .clearCache()
);

// Re-use address decoder to determine from which memory result to use in data_d
wire mem_sdram_WB;
wire mem_io_WB;
wire mem_rom_WB;
wire mem_vram32_WB;
wire mem_vram8_WB;
wire mem_vrampx_WB;

wire mem_multicycle_WB;

AddressDecoder addressDecoder_WB (
    .areg_value(data_a_WB),
    .const16(const16_WB),
    .rw(mem_read_WB),

    .mem_sdram(mem_sdram_WB),
    .mem_io(mem_io_WB),
    .mem_rom(mem_rom_WB),
    .mem_vram32(mem_vram32_WB),
    .mem_vram8(mem_vram8_WB),
    .mem_vrampx(mem_vrampx_WB),

    .mem_multicycle(mem_multicycle_WB),
    .mem_local_address()
);

assign data_d_WB =  (pop_WB) ? stack_q_WB :
                    (mem_sdram_WB && was_cache_miss_WB) ? l1d_cache_controller_result :
                    (mem_sdram_WB && !was_cache_miss_WB) ? l1d_cache_hit_q_WB :
                    (mem_rom_WB) ? rom_mem_q :
                    (mem_vram32_WB) ? vram32_q :
                    (mem_vram8_WB) ? vram8_q :
                    (mem_vrampx_WB) ? vramPX_q :
                    (arithm_WB) ? multicycle_alu_y_WB :
                    (mem_io_WB) ? mu_q_WB :
                    alu_y_WB;

endmodule

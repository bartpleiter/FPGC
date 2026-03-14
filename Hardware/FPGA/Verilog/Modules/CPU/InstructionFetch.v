/*
 * InstructionFetch
 * Handles the IF stage of the pipeline:
 * - PC management (increment, redirect, interrupt, RETI)
 * - ROM fetch interface
 * - L1I cache hit detection and miss handling
 * - Instruction selection mux
 * - IF/ID pipeline register update
 */
module InstructionFetch #(
    parameter ROM_ADDRESS         = 32'h1E000000,
    parameter INTERRUPT_JUMP_ADDR = 32'd4,
    parameter CPU_IO_PC_BACKUP    = 32'h1F000000
) (
    input  wire         clk,
    input  wire         reset,

    // ---- Pipeline control ----
    input  wire         pipeline_stall,
    input  wire         flush_if_id,
    input  wire         backend_pipeline_stall,

    // ---- PC redirect (from branch/jump unit in MEM stage) ----
    input  wire         pc_redirect,
    input  wire [31:0]  pc_redirect_target,

    // ---- Interrupt/RETI ----
    input  wire         interrupt_valid,
    input  wire         reti_valid,
    input  wire [31:0]  ex_mem_pc,

    // ---- CPU I/O: pc_backup write (from MEM stage store) ----
    input  wire         ex_mem_valid,
    input  wire         ex_mem_mem_write,
    input  wire [31:0]  ex_mem_mem_addr,
    input  wire [31:0]  ex_mem_breg_data,

    // ---- ROM interface ----
    input  wire [31:0]  rom_fe_q,
    output wire [9:0]   rom_fe_addr,
    output wire         rom_fe_oe,
    output wire         rom_fe_hold,

    // ---- L1I cache pipeline port ----
    input  wire [270:0] l1i_pipe_q,
    output wire [6:0]   l1i_pipe_addr,

    // ---- L1I cache controller ----
    input  wire         l1i_cache_controller_done,
    input  wire [31:0]  l1i_cache_controller_result,
    output wire [31:0]  l1i_cache_controller_addr,
    output wire         l1i_cache_controller_start,
    output wire         l1i_cache_controller_flush,

    // ---- Outputs ----
    output reg          int_disabled = 1'b0,
    output reg  [31:0]  pc_backup = 32'd0,
    output wire         cache_stall_if,

    // ---- IF/ID pipeline register outputs ----
    output reg  [31:0]  if_id_pc = 32'd0,
    output reg  [31:0]  if_id_instr = 32'd0,
    output reg          if_id_valid = 1'b0,

    // ---- Pre-pipeline instruction (for register file address decode) ----
    output wire [31:0]  if_instr,

    // ---- Debug ----
    input  wire [31:0]  id_ex_pc   // Only used in $display for RETI
);

// ---- PROGRAM COUNTER ----
reg [31:0] pc = ROM_ADDRESS;

// Delayed PC to match instruction fetch latency
// ROM and cache have 1-cycle latency, so we need to track the PC
// that corresponds to the instruction data arriving this cycle
reg [31:0] pc_delayed = ROM_ADDRESS;

// Track when we've just redirected and need to discard the next ROM output
// When pc_redirect happens, the ROM was already fetching the old PC's instruction,
// so we need to wait one more cycle before the correct instruction arrives
reg redirect_pending = 1'b0;

always @(posedge clk)
begin
    if (reset)
    begin
        pc <= ROM_ADDRESS;
        pc_delayed <= ROM_ADDRESS;
        redirect_pending <= 1'b0;
        int_disabled <= 1'b0;
        pc_backup <= 32'd0;
    end else if (interrupt_valid && !pipeline_stall)
    begin
        // Interrupt: save PC and jump to interrupt handler
        int_disabled <= 1'b1;
        pc_backup <= ex_mem_pc;
        $display("Interrupt taken, jumping to address %h, saving PC %h", INTERRUPT_JUMP_ADDR, ex_mem_pc);
        pc <= INTERRUPT_JUMP_ADDR;
        redirect_pending <= 1'b1;
    end else if (reti_valid && !pipeline_stall)
    begin
        // Return from interrupt: restore PC and re-enable interrupts
        int_disabled <= 1'b0;
        pc <= pc_backup;
        redirect_pending <= 1'b1;
        $display("%0t RETI executed, restoring PC to %h, id_ex_pc=%h, pc_redirect=%b", $time, pc_backup, id_ex_pc, pc_redirect);
    end else if (pc_redirect)
    begin
        // Jump/branch redirect - takes priority over stalls
        pc <= pc_redirect_target;
        redirect_pending <= 1'b1;
    end else if (!pipeline_stall)
    begin
        if (redirect_pending)
        begin
            // First cycle after redirect: update pc_delayed
            pc_delayed <= pc;

            if (pc >= ROM_ADDRESS || l1i_hit_redirect)
            begin
                // ROM access or cache hit: proceed normally
                pc <= pc + 32'd4;
                redirect_pending <= 1'b0;
            end else
            begin
                // SDRAM access with cache miss: stay at current PC
                redirect_pending <= 1'b1;
            end
        end else if (cache_stall_if)
        begin
            // Cache miss during normal operation, hold PC
        end else
        begin
            pc_delayed <= pc;
            pc <= pc + 32'd4;
        end
    end

    // CPU-internal I/O: write pc_backup via store instruction
    // Separate from the PC management if-else chain above.
    // Safe because this only executes during interrupt handler (int_disabled=1),
    // so it never conflicts with the interrupt branch that also writes pc_backup.
    if (ex_mem_valid && ex_mem_mem_write &&
        (ex_mem_mem_addr == CPU_IO_PC_BACKUP) && !backend_pipeline_stall)
    begin
        pc_backup <= ex_mem_breg_data;
    end

end

// ---- ROM FETCH ----
wire if_use_rom = (pc >= ROM_ADDRESS);
wire [9:0] if_rom_addr = (pc - ROM_ADDRESS) >> 2;  // Byte address to word address

assign rom_fe_addr = if_rom_addr;
assign rom_fe_oe = if_use_rom && !pipeline_stall;
assign rom_fe_hold = pipeline_stall;

// ---- L1I CACHE HIT DETECTION ----
// Byte-addressable: bits [11:5] = cache index, bits [25:12] = tag, bits [4:2] = word offset
// Use pc_delayed when stalled to keep reading the same cache line
assign l1i_pipe_addr = pipeline_stall ? pc_delayed[11:5] : pc[11:5];

// Normal path: use pc_delayed because cache output corresponds to previous cycle's address
wire [13:0] l1i_tag = pc_delayed[25:12];
wire [2:0]  l1i_offset = pc_delayed[4:2];
wire l1i_cache_valid = l1i_pipe_q[0];
wire [13:0] l1i_cache_tag = l1i_pipe_q[14:1];
wire if_use_cache_delayed = (pc_delayed < ROM_ADDRESS);
wire l1i_hit = if_use_cache_delayed && l1i_cache_valid && (l1i_tag == l1i_cache_tag);
wire [31:0] l1i_cache_data = l1i_pipe_q[32 * l1i_offset + 15 +: 32];

// Redirect path: uses pc instead of pc_delayed
// During redirect_pending, cache was read with pc[11:5], so output corresponds to pc
wire [13:0] l1i_tag_redirect = pc[25:12];
wire if_use_cache_redirect = (pc < ROM_ADDRESS);
wire l1i_hit_redirect = if_use_cache_redirect && l1i_cache_valid && (l1i_tag_redirect == l1i_cache_tag);

// ---- L1I CACHE MISS HANDLING ----
assign cache_stall_if = if_use_cache_delayed && !l1i_hit;
assign l1i_cache_controller_addr = pc_delayed;
assign l1i_cache_controller_start = cache_stall_if && !l1i_cache_controller_done;
assign l1i_cache_controller_flush = 1'b0;

// ---- INSTRUCTION SELECTION ----
// On cache HIT: always use cache data
// On cache MISS with controller done: use controller result (freshly fetched data)
assign if_instr = if_use_rom ? rom_fe_q :
                  (l1i_hit ? l1i_cache_data :
                   (l1i_cache_controller_done ? l1i_cache_controller_result : l1i_cache_data));

// ---- IF/ID STAGE REGISTER UPDATE ----
always @(posedge clk)
begin
    if (reset)
    begin
        if_id_pc <= 32'd0;
        if_id_instr <= 32'd0;
        if_id_valid <= 1'b0;
    end else if (flush_if_id || redirect_pending)
    begin
        // Flush on control hazard OR when waiting for redirect to complete
        if_id_valid <= 1'b0;
        if_id_instr <= 32'd0;
    end else if (!pipeline_stall)
    begin
        // Normal operation: capture instruction from IF stage
        if_id_pc <= pc_delayed;
        if_id_instr <= if_instr;
        if_id_valid <= 1'b1;
    end
end

endmodule

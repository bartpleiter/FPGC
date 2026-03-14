/*
 * MemoryStage
 * Handles the MEM stage of the pipeline:
 * - Memory address calculation (with forwarding capture for stall safety)
 * - Address decoding (ROM, VRAM32, VRAM8, VRAMPX, SDRAM, I/O, CPU-internal I/O)
 * - VRAM and ROM data port interfaces
 * - L1D cache hit detection and miss handling
 * - Memory Unit (I/O) interface
 * - Cache clear state machine
 * - Memory read data multiplexer
 *
 * Important timing note:
 * BRAM-based memories (ROM, VRAM) have 1-cycle read latency.
 * Read addresses must be set in EX stage so data is ready in MEM stage.
 * Write addresses are set in MEM stage to match write-enable timing.
 */
module MemoryStage #(
    parameter CPU_IO_PC_BACKUP    = 32'h1F000000,
    parameter CPU_IO_HW_STACK_PTR = 32'h1F000004
) (
    input  wire         clk,
    input  wire         reset,

    // ---- EX stage inputs for address calculation ----
    input  wire [31:0]  ex_alu_a,
    input  wire [31:0]  id_ex_const16,
    input  wire         id_ex_valid,
    input  wire         id_ex_mem_read,
    input  wire         id_ex_mem_write,
    input  wire         ex_pipeline_stall,

    // ---- Pipeline control ----
    input  wire         backend_pipeline_stall,

    // ---- EX/MEM register inputs (MEM stage data) ----
    input  wire         ex_mem_valid,
    input  wire         ex_mem_mem_read,
    input  wire         ex_mem_mem_write,
    input  wire [31:0]  ex_mem_mem_addr,
    input  wire [31:0]  ex_mem_breg_data,
    input  wire [31:0]  ex_mem_pc,
    input  wire         ex_mem_clear_cache,

    // ---- Memory size control for byte-addressable operations ----
    input  wire [1:0]   ex_mem_mem_size,        // 00=word, 01=byte, 10=halfword
    input  wire         ex_mem_mem_sign_extend,  // 1=sign-extend, 0=zero-extend

    // ---- CPU-internal I/O read sources ----
    input  wire [31:0]  pc_backup,
    input  wire [7:0]   stack_ptr_out,

    // ---- ROM data port ----
    input  wire [31:0]  rom_mem_q,
    output wire [9:0]   rom_mem_addr,

    // ---- VRAM32 ----
    input  wire [31:0]  vram32_q,
    output wire [10:0]  vram32_addr,
    output wire [31:0]  vram32_d,
    output wire         vram32_we,

    // ---- VRAM8 ----
    input  wire [7:0]   vram8_q,
    output wire [13:0]  vram8_addr,
    output wire [7:0]   vram8_d,
    output wire         vram8_we,

    // ---- VRAMPX ----
    input  wire [7:0]   vramPX_q,
    output wire [16:0]  vramPX_addr,
    output wire [7:0]   vramPX_d,
    output wire         vramPX_we,
    input  wire         vramPX_fifo_full,

    // ---- Pixel Palette ----
    output wire         palette_we,
    output wire [7:0]   palette_addr,
    output wire [23:0]  palette_wdata,

    // ---- L1D cache pipeline port ----
    input  wire [270:0] l1d_pipe_q,
    output wire [6:0]   l1d_pipe_addr,

    // ---- L1D cache controller ----
    input  wire         l1d_cache_controller_done,
    input  wire [31:0]  l1d_cache_controller_result,
    output wire [31:0]  l1d_cache_controller_addr,
    output wire [31:0]  l1d_cache_controller_data,
    output wire         l1d_cache_controller_we,
    output wire         l1d_cache_controller_start,
    output wire [3:0]   l1d_cache_controller_byte_enable,

    // ---- Memory Unit (I/O) ----
    input  wire         mu_done,
    input  wire [31:0]  mu_q,
    output reg          mu_start = 1'b0,
    output reg  [31:0]  mu_addr = 32'd0,
    output reg  [31:0]  mu_data = 32'd0,
    output reg          mu_we = 1'b0,

    // ---- Cache clear ----
    input  wire         l1_clear_cache_done,
    output reg          l1_clear_cache = 1'b0,

    // ---- Outputs ----
    output wire [31:0]  ex_mem_addr_calc,
    output wire         mem_sel_sdram,
    output wire         cache_stall_mem,
    output wire         mu_stall,
    output wire         cc_stall,
    output wire         vrampx_stall,
    output wire [31:0]  mem_read_data
);

// ---- ADDRESS CALCULATION ----
// Use a registered address to handle forwarding correctly when stalled.
// When EX is stalled, the forwarding source can move past WB, causing
// ex_alu_a to get a stale value. We capture the correct address when
// it's first calculated with valid forwarding.
wire [31:0] ex_mem_addr_calc_comb = ex_alu_a + id_ex_const16;
reg  [31:0] ex_mem_addr_calc_reg = 32'd0;

// Track if the current EX instruction has a captured address.
// Reset when a new instruction enters EX, set when we have valid forwarding.
reg ex_addr_captured = 1'b0;

always @(posedge clk)
begin
    if (reset)
    begin
        ex_mem_addr_calc_reg <= 32'd0;
        ex_addr_captured <= 1'b0;
    end else if (!ex_pipeline_stall)
    begin
        // Instruction leaving EX stage, reset capture for next instruction
        ex_addr_captured <= 1'b0;
    end else if (!ex_addr_captured && id_ex_valid && (id_ex_mem_read || id_ex_mem_write))
    begin
        // First cycle with this instruction stalled in EX, capture the address.
        // At this point forwarding should be valid.
        ex_mem_addr_calc_reg <= ex_mem_addr_calc_comb;
        ex_addr_captured <= 1'b1;
    end
end

// Use the captured address if available, otherwise use combinational
assign ex_mem_addr_calc = ex_addr_captured ? ex_mem_addr_calc_reg : ex_mem_addr_calc_comb;

// ---- EX-STAGE ADDRESS DECODE ----
// BRAM addresses must be set in EX stage because BRAM has 1-cycle read latency.
wire ex_sel_rom    = ex_mem_addr_calc >= 32'h1E000000 && ex_mem_addr_calc < 32'h1E400000;
wire ex_sel_vram32 = ex_mem_addr_calc >= 32'h1E400000 && ex_mem_addr_calc < 32'h1E800000;
wire ex_sel_vram8  = ex_mem_addr_calc >= 32'h1E800000 && ex_mem_addr_calc < 32'h1EC00000;
wire ex_sel_vrampx_pixel  = ex_mem_addr_calc >= 32'h1EC00000 && ex_mem_addr_calc < 32'h1EC80000;
wire ex_sel_palette       = ex_mem_addr_calc >= 32'h1EC80000 && ex_mem_addr_calc < 32'h1EC80400;
wire ex_sel_vrampx        = ex_sel_vrampx_pixel || ex_sel_palette;

// EX-stage local address calculations for BRAM (ROM and VRAM)
// Byte address → word address for word-addressed BRAM ports
wire [31:0] ex_local_addr_rom    = (ex_mem_addr_calc - 32'h1E000000) >> 2;
wire [31:0] ex_local_addr_vram32 = (ex_mem_addr_calc - 32'h1E400000) >> 2;
wire [31:0] ex_local_addr_vram8  = (ex_mem_addr_calc - 32'h1E800000) >> 2;
wire [31:0] ex_local_addr_vrampx = (ex_mem_addr_calc - 32'h1EC00000) >> 2;

// ---- MEM-STAGE ADDRESS DECODE ----
assign mem_sel_sdram = ex_mem_mem_addr >= 32'h00000000 && ex_mem_mem_addr < 32'h1C000000;
wire   mem_sel_io     = ex_mem_mem_addr >= 32'h1C000000 && ex_mem_mem_addr < 32'h1E000000;
wire   mem_sel_rom    = ex_mem_mem_addr >= 32'h1E000000 && ex_mem_mem_addr < 32'h1E400000;
wire   mem_sel_vram32 = ex_mem_mem_addr >= 32'h1E400000 && ex_mem_mem_addr < 32'h1E800000;
wire   mem_sel_vram8  = ex_mem_mem_addr >= 32'h1E800000 && ex_mem_mem_addr < 32'h1EC00000;
wire   mem_sel_vrampx_pixel = ex_mem_mem_addr >= 32'h1EC00000 && ex_mem_mem_addr < 32'h1EC80000;
wire   mem_sel_palette      = ex_mem_mem_addr >= 32'h1EC80000 && ex_mem_mem_addr < 32'h1EC80400;
wire   mem_sel_vrampx       = mem_sel_vrampx_pixel || mem_sel_palette;

// ---- CPU-INTERNAL I/O ----
wire mem_sel_cpu_io = (ex_mem_mem_addr == CPU_IO_PC_BACKUP) ||
                      (ex_mem_mem_addr == CPU_IO_HW_STACK_PTR);
wire [31:0] cpu_io_read_data = (ex_mem_mem_addr == CPU_IO_PC_BACKUP) ? pc_backup :
                               {24'd0, stack_ptr_out};

// ---- LOCAL ADDRESS (MEM stage) ----
// For word-addressed BRAM ports, convert byte address to word address
wire [31:0] mem_local_addr = mem_sel_rom    ? (ex_mem_mem_addr - 32'h1E000000) >> 2 :
                             mem_sel_vram32 ? (ex_mem_mem_addr - 32'h1E400000) >> 2 :
                             mem_sel_vram8  ? (ex_mem_mem_addr - 32'h1E800000) >> 2 :
                             mem_sel_vrampx ? (ex_mem_mem_addr - 32'h1EC00000) >> 2 :
                             ex_mem_mem_addr;

// ---- ROM DATA PORT ----
// Address from EX stage for reads (BRAM latency)
assign rom_mem_addr = ex_local_addr_rom[9:0];

// ---- VRAM32 INTERFACE ----
// Address from EX stage for reads (BRAM latency), MEM stage for writes
assign vram32_addr = (ex_mem_valid && ex_mem_mem_write && mem_sel_vram32) ?
                     mem_local_addr[10:0] : ex_local_addr_vram32[10:0];
assign vram32_d  = ex_mem_breg_data;
assign vram32_we = ex_mem_valid && ex_mem_mem_write && mem_sel_vram32 && !backend_pipeline_stall;

// ---- VRAM8 INTERFACE ----
assign vram8_addr = (ex_mem_valid && ex_mem_mem_write && mem_sel_vram8) ?
                    mem_local_addr[13:0] : ex_local_addr_vram8[13:0];
assign vram8_d  = ex_mem_breg_data[7:0];
assign vram8_we = ex_mem_valid && ex_mem_mem_write && mem_sel_vram8 && !backend_pipeline_stall;

// ---- VRAMPX INTERFACE (pixel framebuffer only) ----
assign vramPX_addr = (ex_mem_valid && ex_mem_mem_write && mem_sel_vrampx_pixel) ?
                     mem_local_addr[16:0] : ex_local_addr_vrampx[16:0];
assign vramPX_d  = ex_mem_breg_data[7:0];
assign vramPX_we = ex_mem_valid && ex_mem_mem_write && mem_sel_vrampx_pixel && !backend_pipeline_stall;

// ---- PIXEL PALETTE INTERFACE ----
assign palette_we    = ex_mem_valid && ex_mem_mem_write && mem_sel_palette && !backend_pipeline_stall;
assign palette_addr  = mem_local_addr[7:0];
assign palette_wdata = ex_mem_breg_data[23:0];

// ---- VRAMPX FIFO BACKPRESSURE ----
// Stall the pipeline when MEM stage wants to write to VRAMPX pixel framebuffer but FIFO is full
// Palette writes go to BRAM (instant) and don't need backpressure
assign vrampx_stall = ex_mem_valid && ex_mem_mem_write && mem_sel_vrampx_pixel && vramPX_fifo_full;

// ---- L1D CACHE INTERFACE ----
// Byte-addressable: bits [11:5] = cache index (7 bits), bits [25:12] = tag (14 bits), bits [4:2] = word offset
assign l1d_pipe_addr = ex_mem_mem_addr[11:5];

// L1D cache hit detection
wire [13:0] l1d_tag = ex_mem_mem_addr[25:12];
wire [2:0]  l1d_offset = ex_mem_mem_addr[4:2];
wire l1d_cache_valid = l1d_pipe_q[0];
wire [13:0] l1d_cache_tag = l1d_pipe_q[14:1];
wire l1d_hit = mem_sel_sdram && l1d_cache_valid && (l1d_tag == l1d_cache_tag);
wire [31:0] l1d_cache_data = l1d_pipe_q[32 * l1d_offset + 15 +: 32];

// ---- L1D CACHE READ DELAY TRACKING ----
// Since the cache DPRAM has 1-cycle read latency, when an instruction first
// enters MEM stage, we need to wait 1 cycle for l1d_pipe_q to be valid.
reg l1d_cache_read_done = 1'b0;

// Detect when a NEW instruction enters MEM (based on PC change)
reg [31:0] l1d_prev_pc = 32'hFFFFFFFF;
wire l1d_is_sdram_op = ex_mem_valid && (ex_mem_mem_read || ex_mem_mem_write) && mem_sel_sdram;
wire l1d_new_instr = (ex_mem_pc != l1d_prev_pc);

// Need to wait for cache read on first cycle of new SDRAM operation
wire l1d_need_cache_wait = l1d_is_sdram_op && !l1d_cache_read_done;

always @(posedge clk)
begin
    if (reset)
    begin
        l1d_cache_read_done <= 1'b0;
        l1d_prev_pc <= 32'hFFFFFFFF;
    end else
    begin
        if (l1d_new_instr)
        begin
            l1d_prev_pc <= ex_mem_pc;
            l1d_cache_read_done <= 1'b0;
        end
        else if (l1d_need_cache_wait)
        begin
            l1d_cache_read_done <= 1'b1;
        end
        else if (!l1d_is_sdram_op)
        begin
            l1d_cache_read_done <= 1'b0;
        end
    end
end

// ---- L1D CACHE CONTROLLER STATE MACHINE ----
// Manages cache miss handling to ensure proper request/done handshaking
localparam L1D_STATE_IDLE    = 2'b00;
localparam L1D_STATE_STARTED = 2'b01;
localparam L1D_STATE_WAIT    = 2'b10;

reg [1:0]  l1d_state = L1D_STATE_IDLE;
reg        l1d_request_finished = 1'b0;
reg        l1d_start_reg = 1'b0;
reg [31:0] l1d_result_reg = 32'd0;

// Cache read wait signal
wire l1d_cache_read_wait = l1d_need_cache_wait;

// Cache miss/write detection (only triggers new request if not already finished)
wire l1d_miss  = ex_mem_valid && ex_mem_mem_read  && mem_sel_sdram && !l1d_hit && !l1d_request_finished && !l1d_cache_read_wait;
wire l1d_write = ex_mem_valid && ex_mem_mem_write && mem_sel_sdram && !l1d_request_finished && !l1d_cache_read_wait;

// Cache stall: waiting for cache read, or operation to complete
assign cache_stall_mem = l1d_cache_read_wait || l1d_miss || l1d_write;

always @(posedge clk)
begin
    if (reset)
    begin
        l1d_state <= L1D_STATE_IDLE;
        l1d_start_reg <= 1'b0;
        l1d_request_finished <= 1'b0;
        l1d_result_reg <= 32'd0;
    end else
    begin
        case (l1d_state)
            L1D_STATE_IDLE:
            begin
                l1d_start_reg <= 1'b0;
                l1d_request_finished <= 1'b0;
                l1d_result_reg <= 32'd0;

                if (!l1d_cache_read_wait &&
                    ((ex_mem_valid && ex_mem_mem_read && mem_sel_sdram && !l1d_hit && !l1d_request_finished) ||
                     (ex_mem_valid && ex_mem_mem_write && mem_sel_sdram && !l1d_request_finished)))
                begin
                    l1d_start_reg <= 1'b1;
                    l1d_state <= L1D_STATE_STARTED;
                end
            end

            L1D_STATE_STARTED:
            begin
                l1d_start_reg <= 1'b0;
                l1d_state <= L1D_STATE_WAIT;
            end

            L1D_STATE_WAIT:
            begin
                if (l1d_cache_controller_done)
                begin
                    l1d_result_reg <= l1d_cache_controller_result;
                    l1d_request_finished <= 1'b1;
                    l1d_state <= L1D_STATE_IDLE;
                end
            end
        endcase
    end
end

// ---- SUB-WORD WRITE: BYTE-ENABLE AND DATA SHIFTING ----
// Generate byte_enable mask from mem_size and byte_offset (for writes)
wire [1:0] write_byte_offset = ex_mem_mem_addr[1:0];
wire [3:0] byte_enable = (ex_mem_mem_size == 2'b00) ? 4'b1111 :                           // write word
                          (ex_mem_mem_size == 2'b01) ? (4'b0001 << write_byte_offset) :    // writeb
                          (ex_mem_mem_size == 2'b10) ? (write_byte_offset[1] ? 4'b1100 : 4'b0011) : // writeh
                          4'b1111;

// Replicate sub-word data to all byte lanes (byte_enable selects which actually written)
wire [31:0] write_data_shifted = (ex_mem_mem_size == 2'b01) ? {4{ex_mem_breg_data[7:0]}} :   // Replicate byte
                                 (ex_mem_mem_size == 2'b10) ? {2{ex_mem_breg_data[15:0]}} :  // Replicate halfword
                                 ex_mem_breg_data;                                            // Full word

// L1D cache controller port assignments
assign l1d_cache_controller_addr  = ex_mem_mem_addr;
assign l1d_cache_controller_data  = write_data_shifted;
assign l1d_cache_controller_we    = ex_mem_mem_write && mem_sel_sdram;
assign l1d_cache_controller_start = l1d_start_reg;
assign l1d_cache_controller_byte_enable = byte_enable;

// ---- MEMORY UNIT (I/O) STATE MACHINE ----
// Ensures we only start the MU once per operation and wait for completion
reg mu_io_started = 1'b0;

always @(posedge clk)
begin
    if (reset)
    begin
        mu_start <= 1'b0;
        mu_addr <= 32'd0;
        mu_data <= 32'd0;
        mu_we <= 1'b0;
        mu_io_started <= 1'b0;
    end else if (mu_done)
    begin
        mu_start <= 1'b0;
        mu_io_started <= 1'b0;
    end else if (ex_mem_valid && mem_sel_io && (ex_mem_mem_read || ex_mem_mem_write) && !mu_io_started)
    begin
        mu_start <= 1'b1;
        mu_addr <= ex_mem_mem_addr;
        mu_data <= ex_mem_breg_data;
        mu_we <= ex_mem_mem_write;
        mu_io_started <= 1'b1;
    end else
    begin
        mu_start <= 1'b0;
    end
end

assign mu_stall = ex_mem_valid && mem_sel_io && (ex_mem_mem_read || ex_mem_mem_write) && !mu_done;

// ---- CACHE CLEAR STATE MACHINE ----
reg clear_cache_in_progress = 1'b0;
reg clear_cache_finished = 1'b0;

always @(posedge clk)
begin
    if (reset)
    begin
        l1_clear_cache <= 1'b0;
        clear_cache_in_progress <= 1'b0;
        clear_cache_finished <= 1'b0;
    end else if (!ex_mem_valid || !ex_mem_clear_cache)
    begin
        l1_clear_cache <= 1'b0;
        clear_cache_in_progress <= 1'b0;
        clear_cache_finished <= 1'b0;
    end else if (clear_cache_finished)
    begin
        l1_clear_cache <= 1'b0;
    end else if (l1_clear_cache_done)
    begin
        l1_clear_cache <= 1'b0;
        clear_cache_in_progress <= 1'b0;
        clear_cache_finished <= 1'b1;
        $display("%0t Clear Cache completed", $time);
    end else if (!clear_cache_in_progress)
    begin
        l1_clear_cache <= 1'b1;
        clear_cache_in_progress <= 1'b1;
        $display("%0t Clear Cache triggered", $time);
    end else
    begin
        l1_clear_cache <= 1'b0;
    end
end

assign cc_stall = ex_mem_valid && ex_mem_clear_cache && !clear_cache_finished;

// ---- MEMORY READ DATA MUX ----
// For SDRAM: use l1d_result_reg if we had a cache miss (request_finished),
// otherwise use direct cache data for hits.
wire [31:0] raw_read_data = mem_sel_cpu_io ? cpu_io_read_data :
                            mem_sel_rom    ? rom_mem_q :
                            mem_sel_vram32 ? vram32_q :
                            mem_sel_vram8  ? {24'd0, vram8_q} :
                            mem_sel_vrampx ? {24'd0, vramPX_q} :
                            mem_sel_io     ? mu_q :
                            mem_sel_sdram  ? (l1d_request_finished ? l1d_result_reg : l1d_cache_data) :
                            32'hDEADBEEF;

// ---- SUB-WORD READ EXTRACTION ----
// Extract byte or halfword from the raw 32-bit word based on byte offset
wire [1:0] byte_offset = ex_mem_mem_addr[1:0];

wire [7:0] selected_byte = (byte_offset == 2'd0) ? raw_read_data[7:0] :
                            (byte_offset == 2'd1) ? raw_read_data[15:8] :
                            (byte_offset == 2'd2) ? raw_read_data[23:16] :
                                                    raw_read_data[31:24];

wire [15:0] selected_half = byte_offset[1] ? raw_read_data[31:16] : raw_read_data[15:0];

// Final read data with sign/zero extension based on mem_size
assign mem_read_data = (ex_mem_mem_size == 2'b00) ? raw_read_data :
                       (ex_mem_mem_size == 2'b01 &&  ex_mem_mem_sign_extend) ? {{24{selected_byte[7]}}, selected_byte} :
                       (ex_mem_mem_size == 2'b01 && !ex_mem_mem_sign_extend) ? {24'd0, selected_byte} :
                       (ex_mem_mem_size == 2'b10 &&  ex_mem_mem_sign_extend) ? {{16{selected_half[15]}}, selected_half} :
                       (ex_mem_mem_size == 2'b10 && !ex_mem_mem_sign_extend) ? {16'd0, selected_half} :
                       raw_read_data;

endmodule

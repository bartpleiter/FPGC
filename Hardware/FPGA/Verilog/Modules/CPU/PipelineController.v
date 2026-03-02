/*
 * PipelineController
 * Controls pipeline stall, flush, forwarding, and hazard detection.
 * Extracted from B32P3.v to centralise all pipeline control logic.
 */
module PipelineController (
    // ---- Forwarding inputs ----
    input wire        ex_mem_dreg_we,
    input wire [3:0]  ex_mem_dreg,
    input wire        ex_mem_mem_read,
    input wire        ex_mem_pop,
    input wire [3:0]  id_ex_areg,
    input wire [3:0]  id_ex_breg,
    input wire        mem_wb_dreg_we,
    input wire [3:0]  mem_wb_dreg,

    // ---- Hazard detection inputs ----
    input wire        id_ex_valid,
    input wire        id_ex_mem_read,
    input wire [3:0]  id_ex_dreg,
    input wire        id_ex_pop,
    input wire [3:0]  id_areg,
    input wire [3:0]  id_breg,
    input wire        if_id_valid,
    input wire        id_ex_mem_write,
    input wire        ex_mem_valid,
    input wire        ex_mem_mem_write,
    input wire        mem_sel_sdram,
    input wire [31:0] ex_mem_mem_addr,

    // Pre-computed cache-line hazard inputs (from B32P3.v)
    // Uses registered base address to avoid forwarding-mux → adder critical path
    input wire [6:0]  ex_hazard_cache_line,      // Cache line from registered base + const16
    input wire        ex_hazard_is_sdram,         // SDRAM check from registered base upper bits
    input wire        ex_hazard_forward_active,   // Forwarding active on base register

    // ---- Stall source inputs ----
    input wire        cache_stall_if,
    input wire        cache_stall_mem,
    input wire        multicycle_stall,
    input wire        mu_stall,
    input wire        cc_stall,
    input wire        vrampx_stall,

    // ---- Flush source inputs ----
    input wire        pc_redirect,
    input wire        reti_valid,
    input wire        interrupt_valid,

    // ---- Forwarding outputs ----
    output wire [1:0] forward_a,
    output wire [1:0] forward_b,

    // ---- Stall outputs ----
    output wire       pipeline_stall,
    output wire       ex_pipeline_stall,
    output wire       backend_pipeline_stall,

    // ---- Flush outputs ----
    output wire       flush_if_id,
    output wire       flush_id_ex,
    output wire       flush_ex_mem,

    // ---- Hazard outputs ----
    output wire       cache_line_hazard
);

// ---- Forwarding unit ----
// Forward from EX/MEM (most recent) or MEM/WB (older)
// forward_a/b: 00=no forward, 01=from EX/MEM, 10=from MEM/WB
// Don't forward from EX/MEM for reads or pops as their data isn't ready yet!
wire ex_mem_can_forward = ex_mem_dreg_we && ex_mem_dreg != 4'd0 && !ex_mem_mem_read && !ex_mem_pop;

assign forward_a = (ex_mem_can_forward && ex_mem_dreg == id_ex_areg) ? 2'b01 :
                   (mem_wb_dreg_we && mem_wb_dreg != 4'd0 && mem_wb_dreg == id_ex_areg) ? 2'b10 :
                   2'b00;

assign forward_b = (ex_mem_can_forward && ex_mem_dreg == id_ex_breg) ? 2'b01 :
                   (mem_wb_dreg_we && mem_wb_dreg != 4'd0 && mem_wb_dreg == id_ex_breg) ? 2'b10 :
                   2'b00;

// ---- Hazard detection unit ----
// Load-use hazard: instruction in ID needs data from read in EX
wire load_use_hazard = id_ex_valid && id_ex_mem_read &&
                       ((id_ex_dreg == id_areg && id_areg != 4'd0) ||
                        (id_ex_dreg == id_breg && id_breg != 4'd0)) &&
                       if_id_valid;

// Pop-use hazard: instruction in ID needs data from pop in EX
wire pop_use_hazard = id_ex_valid && id_ex_pop &&
                      ((id_ex_dreg == id_areg && id_areg != 4'd0) ||
                       (id_ex_dreg == id_breg && id_breg != 4'd0)) &&
                      if_id_valid;

// Cache line hazard: back-to-back SDRAM accesses to different cache lines
// Uses pre-computed inputs from registered base (no forwarding mux in path).
// When forwarding is active, conservatively assumes hazard (safe: 1 extra stall cycle).
wire [6:0] mem_cache_line = ex_mem_mem_addr[9:3];

wire ex_needs_sdram = id_ex_valid && (id_ex_mem_read || id_ex_mem_write) &&
                      (ex_hazard_forward_active || ex_hazard_is_sdram);
wire mem_has_sdram = ex_mem_valid && (ex_mem_mem_read || ex_mem_mem_write) && mem_sel_sdram;
assign cache_line_hazard = ex_needs_sdram && mem_has_sdram &&
    (ex_hazard_forward_active || (ex_hazard_cache_line != mem_cache_line));

wire hazard_stall = load_use_hazard || pop_use_hazard || cache_line_hazard;

// ---- Stall computation ----
// Combined backend stall - stalls entire pipeline
wire backend_stall = cache_stall_if || cache_stall_mem || multicycle_stall || mu_stall || cc_stall || vrampx_stall;

// Front-end stall (IF, ID) - includes hazard stalls
assign pipeline_stall = hazard_stall || backend_stall;

// EX stage stall - includes cache_line_hazard and backend_stall
// Load/pop hazards don't stall EX because EX instruction needs to wait in ID
assign ex_pipeline_stall = backend_stall || cache_line_hazard;

// MEM and WB stall - only backend_stall
// cache_line_hazard should NOT stall MEM/WB - MEM must complete so hazard clears
assign backend_pipeline_stall = backend_stall;

// ---- Flush computation ----
// Flushes should only happen when the triggering event actually executes (not stalled)
wire reti_executes = reti_valid && !pipeline_stall;
wire interrupt_executes = interrupt_valid && !pipeline_stall;

assign flush_if_id = pc_redirect || interrupt_executes || reti_executes;
assign flush_id_ex = pc_redirect || interrupt_executes || reti_executes;
assign flush_ex_mem = pc_redirect || interrupt_executes;

endmodule

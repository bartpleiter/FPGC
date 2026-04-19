/*
 * DMAengine
 * ---------
 * Single-channel DMA controller for the FPGC. Phase 1 implements MEM2MEM
 * fully; the other modes from dma-implementation-plan.md (MEM2SPI, SPI2MEM,
 * MEM2VRAM, MEM2IO, IO2MEM) are accepted by the register interface but the
 * engine completes them immediately with error=1 set in DMA_STATUS.
 * They will be filled in later commits when the iop_X / vp_X peer ports get
 * wired into MemoryUnits IDLE-state arbiter.
 *
 * Restrictions for MEM2MEM in this commit:
 *   - DMA_SRC, DMA_DST, DMA_COUNT must all be 32-byte (cache-line) aligned.
 *     Unaligned values result in error=1 and an immediate completion.
 *   - DMA_COUNT must be > 0.
 *
 * Register interface (1-cycle write, combinatorial read):
 *
 *   reg_addr  Register
 *     0       DMA_SRC      (RW, 32-bit byte source address)
 *     1       DMA_DST      (RW, 32-bit byte destination address)
 *     2       DMA_COUNT    (RW, 32-bit byte count)
 *     3       DMA_CTRL     (RW; bit [31] is W1S "start", self-clears)
 *     4       DMA_STATUS   (R; reads return {..., err, done, busy};
 *                          reading the register clears the sticky done
 *                          and error bits.)
 *
 * SDRAM master port mirrors the SDRAMcontrollers CPU-side protocol:
 * pulse sd_start with sd_addr (line address, 21 bits, byte_addr >> 5),
 * sd_we, and (for writes) sd_data; await sd_done; for reads, latch
 * sd_q on the cycle sd_done is asserted.
 */
module DMAengine (
    input  wire         clk,
    input  wire         reset,

    // ---- Register-bus side (driven by MemoryUnit) ----
    input  wire [2:0]   reg_addr,
    input  wire         reg_we,
    input  wire [31:0]  reg_data,
    output reg  [31:0]  reg_q,

    // ---- SDRAM master port (to SDRAMarbiter dma_*) ----
    output reg  [20:0]  sd_addr   = 21'd0,
    output reg [255:0]  sd_data   = 256'd0,
    output reg          sd_we     = 1'b0,
    output reg          sd_start  = 1'b0,
    input  wire         sd_done,
    input  wire [255:0] sd_q,

    // ---- I/O peer port (to MemoryUnit iop_*; unused this commit) ----
    output wire         iop_start,
    output wire         iop_we,
    output wire [31:0]  iop_addr,
    output wire [31:0]  iop_data,
    input  wire         iop_done,
    input  wire [31:0]  iop_q,

    // ---- VRAMPX peer port (to MemoryUnit vp_*; unused this commit) ----
    output wire         vp_we,
    output wire [16:0]  vp_addr,
    output wire [7:0]   vp_data,

    // ---- Interrupt to InterruptController bit 8 (INT_ID_DMA = 9) ----
    output reg          irq       = 1'b0
);

// MEM2MEM-only commit: peer ports inactive
assign iop_start = 1'b0;
assign iop_we    = 1'b0;
assign iop_addr  = 32'd0;
assign iop_data  = 32'd0;
assign vp_we     = 1'b0;
assign vp_addr   = 17'd0;
assign vp_data   = 8'd0;

// ---- Mode constants (mirror libfpgc/io/dma.h) ----
localparam MODE_MEM2MEM  = 4'd0;
localparam MODE_MEM2SPI  = 4'd1;
localparam MODE_SPI2MEM  = 4'd2;
localparam MODE_MEM2VRAM = 4'd3;
localparam MODE_MEM2IO   = 4'd4;
localparam MODE_IO2MEM   = 4'd5;

// ---- Register backing storage ----
reg [31:0] dma_src   = 32'd0;
reg [31:0] dma_dst   = 32'd0;
reg [31:0] dma_count = 32'd0;
reg [31:0] dma_ctrl  = 32'd0;

reg        sticky_done  = 1'b0;
reg        sticky_error = 1'b0;
reg        busy         = 1'b0;

// ---- MEM2MEM transfer state ----
reg [31:0] src_cur          = 32'd0;
reg [31:0] dst_cur          = 32'd0;
reg [31:0] bytes_remaining  = 32'd0;
reg [255:0] line_buf        = 256'd0;

localparam
    ST_IDLE     = 3'd0,
    ST_RD_REQ   = 3'd1,
    ST_RD_WAIT  = 3'd2,
    ST_WR_REQ   = 3'd3,
    ST_WR_WAIT  = 3'd4,
    ST_DONE     = 3'd5,
    ST_ERROR    = 3'd6;

reg [2:0] state = ST_IDLE;

// ---- Combinatorial read mux ----
always @(*)
begin
    case (reg_addr)
        3'd0:    reg_q = dma_src;
        3'd1:    reg_q = dma_dst;
        3'd2:    reg_q = dma_count;
        3'd3:    reg_q = dma_ctrl;
        3'd4:    reg_q = {29'd0, sticky_error, sticky_done, busy};
        default: reg_q = 32'd0;
    endcase
end

// ---- Decode helpers ----
wire        ctrl_start_bit  = dma_ctrl[31];
wire [3:0]  ctrl_mode       = dma_ctrl[3:0];
wire        ctrl_irq_en     = dma_ctrl[4];

// Aligned-to-32-bytes test (low 5 bits zero)
wire mem2mem_args_aligned =
    (dma_src[4:0] == 5'd0) &&
    (dma_dst[4:0] == 5'd0) &&
    (dma_count[4:0] == 5'd0) &&
    (dma_count != 32'd0);

// Reading STATUS clears the sticky bits next cycle
wire status_read = reg_we == 1'b0 && reg_addr == 3'd4;

always @(posedge clk)
begin
    if (reset)
    begin
        dma_src         <= 32'd0;
        dma_dst         <= 32'd0;
        dma_count       <= 32'd0;
        dma_ctrl        <= 32'd0;
        sticky_done     <= 1'b0;
        sticky_error    <= 1'b0;
        busy            <= 1'b0;
        sd_addr         <= 21'd0;
        sd_data         <= 256'd0;
        sd_we           <= 1'b0;
        sd_start        <= 1'b0;
        irq             <= 1'b0;
        src_cur         <= 32'd0;
        dst_cur         <= 32'd0;
        bytes_remaining <= 32'd0;
        line_buf        <= 256'd0;
        state           <= ST_IDLE;
    end
    else
    begin
        // Defaults (single-cycle pulses)
        sd_start <= 1'b0;
        irq      <= 1'b0;

        // ---- Register writes from MemoryUnit ----
        // Note: while busy, software is not supposed to scribble on
        // SRC/DST/COUNT/CTRL. Allow it anyway -- only the start bit is
        // gated below.
        if (reg_we)
        begin
            case (reg_addr)
                3'd0: dma_src   <= reg_data;
                3'd1: dma_dst   <= reg_data;
                3'd2: dma_count <= reg_data;
                3'd3: dma_ctrl  <= reg_data;
                default: ; // STATUS is read-only
            endcase
        end

        // ---- Status sticky-clear on read ----
        if (status_read)
        begin
            sticky_done  <= 1'b0;
            sticky_error <= 1'b0;
        end

        // ---- State machine ----
        case (state)
            ST_IDLE:
            begin
                if (ctrl_start_bit && !busy)
                begin
                    // Self-clear the start bit (W1S)
                    dma_ctrl[31] <= 1'b0;
                    busy <= 1'b1;

                    if (ctrl_mode == MODE_MEM2MEM)
                    begin
                        if (mem2mem_args_aligned)
                        begin
                            src_cur         <= dma_src;
                            dst_cur         <= dma_dst;
                            bytes_remaining <= dma_count;
                            state           <= ST_RD_REQ;
                        end
                        else
                        begin
                            state <= ST_ERROR;
                        end
                    end
                    else
                    begin
                        // MEM2SPI / SPI2MEM / MEM2VRAM / MEM2IO / IO2MEM
                        // not implemented in this commit -- fail clean.
                        state <= ST_ERROR;
                    end
                end
            end

            ST_RD_REQ:
            begin
                sd_addr  <= src_cur[25:5]; // 21-bit line address
                sd_we    <= 1'b0;
                sd_start <= 1'b1;
                state    <= ST_RD_WAIT;
            end

            ST_RD_WAIT:
            begin
                // Hold sd_start asserted until the arbiter grants us (sd_done).
                // Without this, a CPU request that wins arbitration on the
                // grant cycle would cause our pulse to be dropped.
                sd_start <= 1'b1;
                if (sd_done)
                begin
                    sd_start <= 1'b0;
                    line_buf <= sd_q;
                    state    <= ST_WR_REQ;
                end
            end

            ST_WR_REQ:
            begin
                sd_addr  <= dst_cur[25:5];
                sd_data  <= line_buf;
                sd_we    <= 1'b1;
                sd_start <= 1'b1;
                state    <= ST_WR_WAIT;
            end

            ST_WR_WAIT:
            begin
                sd_start <= 1'b1;
                if (sd_done)
                begin
                    sd_start        <= 1'b0;
                    src_cur         <= src_cur + 32'd32;
                    dst_cur         <= dst_cur + 32'd32;
                    bytes_remaining <= bytes_remaining - 32'd32;
                    if (bytes_remaining == 32'd32)
                        state <= ST_DONE;
                    else
                        state <= ST_RD_REQ;
                end
            end

            ST_DONE:
            begin
                busy        <= 1'b0;
                sticky_done <= 1'b1;
                if (ctrl_irq_en)
                    irq <= 1'b1;
                state <= ST_IDLE;
            end

            ST_ERROR:
            begin
                busy         <= 1'b0;
                sticky_error <= 1'b1;
                if (ctrl_irq_en)
                    irq <= 1'b1;
                state <= ST_IDLE;
            end

            default: state <= ST_IDLE;
        endcase
    end
end

endmodule

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
 *     0       DMA_SRC       (RW, 32-bit byte source address)
 *     1       DMA_DST       (RW, 32-bit byte destination address)
 *     2       DMA_COUNT     (RW, 32-bit byte count)
 *     3       DMA_CTRL      (RW; bit [31] is W1S "start", self-clears)
 *     4       DMA_STATUS    (R; reads return {..., err, done, busy};
 *                           reading the register clears the sticky done
 *                           and error bits.)
 *     5       DMA_QSPI_ADDR (RW, 24-bit byte address into the QSPI flash;
 *                           used as the start address for the QSPI Fast
 *                           Read issued by mode SPI2MEM_QSPI.)
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

    // ---- I/O peer port (to MemoryUnit iop_*; legacy, unused since the
    //      Phase-B SPI burst port replaced the per-byte iop_* loop) ----
    output wire         iop_start,
    output wire         iop_we,
    output wire [31:0]  iop_addr,
    output wire [31:0]  iop_data,
    input  wire         iop_done,
    input  wire [31:0]  iop_q,

    // ---- DMA SPI burst port (drives SimpleSPI2 directly via MemoryUnit) ----
    output reg  [2:0]   dma_burst_spi_id = 3'd0,
    output reg          dma_burst_select = 1'b0,
    output reg          dma_burst_we     = 1'b0,
    output reg  [7:0]   dma_burst_data   = 8'd0,
    output reg          dma_burst_start  = 1'b0,
    output reg  [15:0]  dma_burst_len    = 16'd0,
    output reg          dma_burst_dummy  = 1'b0,
    output reg          dma_burst_re_rx  = 1'b0,
    // QSPI Fast Read controls (only meaningful when dma_burst_select=1
    // and the SPI port wired through is QSPIflash, i.e. SPI1).
    output reg          dma_burst_qspi_read = 1'b0,
    output reg  [23:0]  dma_burst_qspi_addr = 24'd0,
    input  wire         dma_burst_tx_full,
    input  wire         dma_burst_rx_empty,
    input  wire [7:0]   dma_burst_rx_data,
    input  wire [7:0]   dma_burst_rx_count,
    input  wire         dma_burst_busy,
    input  wire         dma_burst_done,

    // ---- VRAMPX peer port (to MemoryUnit vp_*) ----
    // vp_full is the VRAMPXSram CPU-write-FIFO full flag, routed back through
    // the top-level mux. The engine pauses byte emission while it is high.
    // vp_we / vp_addr / vp_data are driven combinationally so the FIFO
    // wr_en (gated by !full inside the FIFO) is consistent with the
    // !vp_full check the engine uses to advance state -- a registered
    // vp_we would let one byte through after the FIFO transitioned to
    // full and silently drop it.
    output wire         vp_we,
    output wire [16:0]  vp_addr,
    output wire [7:0]   vp_data,
    input  wire         vp_full,

    // ---- Interrupt to InterruptController bit 6 (INT_ID_DMA = 7) ----
    output reg          irq       = 1'b0
);

// Legacy iop_* port is no longer driven by the engine -- the Phase-B SPI
// burst port (dma_burst_*) replaces it. Tie off so MemoryUnit's iop_*
// arbiter sits idle and CPU MMIO never has to compete with the engine.
assign iop_start = 1'b0;
assign iop_we    = 1'b0;
assign iop_addr  = 32'd0;
assign iop_data  = 32'd0;

// ---- Mode constants (mirror libfpgc/io/dma.h) ----
localparam MODE_MEM2MEM       = 4'd0;
localparam MODE_MEM2SPI       = 4'd1;
localparam MODE_SPI2MEM       = 4'd2;
localparam MODE_MEM2VRAM      = 4'd3;
localparam MODE_MEM2IO        = 4'd4;
localparam MODE_IO2MEM        = 4'd5;
localparam MODE_SPI2MEM_QSPI  = 4'd6;   // SPI1 only (QSPIflash)

// ---- Register backing storage ----
reg [31:0] dma_src       = 32'd0;
reg [31:0] dma_dst       = 32'd0;
reg [31:0] dma_count     = 32'd0;
reg [31:0] dma_ctrl      = 32'd0;
reg [31:0] dma_qspi_addr = 32'd0;   // only [23:0] is meaningful

reg        sticky_done  = 1'b0;
reg        sticky_error = 1'b0;
reg        busy         = 1'b0;

// ---- MEM2MEM transfer state ----
reg [31:0] src_cur          = 32'd0;
reg [31:0] dst_cur          = 32'd0;
reg [31:0] bytes_remaining  = 32'd0;
reg [255:0] line_buf        = 256'd0;

// ---- SPI byte cursor (used by MEM2SPI / SPI2MEM / SPI2MEM_QSPI) ----
// Counts bytes within the current 32-byte cache line, 0..31.
reg [5:0]  spi_byte_idx = 6'd0;  // 6 bits so we can hold 32 (== done) without wrap
// Latched at start: which SPI bus is the target.
reg [2:0]  spi_id_sel   = 3'd0;  // 0 = SPI0 (Flash), 1 = SPI1 (Flash 2 / BRFS), 4 = SPI4 (Eth), 5 = SPI5 (SD)
// Per-burst cursor for SPI2MEM_QSPI: starts at dma_qspi_addr, advances by 32
// after every committed cache line. Tracked separately from src_cur (SDRAM)
// because for QSPI the "source address" is a flash byte offset, not SDRAM.
reg [23:0] qspi_addr_cur = 24'd0;

// MODE_SPI2MEM_QSPI: kick off ONE QSPIflash burst of the full transfer
// length, then drain the rolling RX FIFO line-by-line. This avoids the
// per-32B opcode + addr + M + dummy prologue (~24% of each chunk's SCK
// time). qspi_burst_open is set on the first ST_S2M_BURST entry of a
// transfer and stays high until ST_DONE / ST_ERROR.
reg        qspi_burst_open = 1'b0;

localparam
    ST_IDLE          = 4'd0,
    ST_RD_REQ        = 4'd1,   // MEM2MEM / MEM2SPI: SDRAM read request
    ST_RD_WAIT       = 4'd2,
    ST_WR_REQ        = 4'd3,   // MEM2MEM / SPI2MEM: SDRAM write request
    ST_WR_WAIT       = 4'd4,
    ST_DONE          = 4'd5,
    ST_ERROR         = 4'd6,
    ST_S2M_BURST     = 4'd7,   // SPI2MEM: kick off 32-byte SPI burst (dummy)
    ST_S2M_BWAIT     = 4'd8,   // SPI2MEM: wait for burst done
    ST_S2M_DRAIN     = 4'd9,   // SPI2MEM: drain 32 RX bytes into line_buf
    ST_M2S_FILL      = 4'd10,  // MEM2SPI: push 32 line_buf bytes into TX FIFO
    ST_M2S_BURST     = 4'd11,  // MEM2SPI: kick off 32-byte SPI burst
    ST_M2S_BWAIT     = 4'd12,  // MEM2SPI: wait for burst done
    ST_M2S_DRAIN     = 4'd13,  // MEM2SPI: drain 32 RX bytes (discarded)
    ST_M2V_DRAIN     = 4'd14;  // MEM2VRAM: emit 32 bytes from line_buf to vp_*

reg [3:0] state = ST_IDLE;

// ---- Combinatorial read mux ----
always @(*)
begin
    case (reg_addr)
        3'd0:    reg_q = dma_src;
        3'd1:    reg_q = dma_dst;
        3'd2:    reg_q = dma_count;
        3'd3:    reg_q = dma_ctrl;
        3'd4:    reg_q = {29'd0, sticky_error, sticky_done, busy};
        3'd5:    reg_q = dma_qspi_addr;
        default: reg_q = 32'd0;
    endcase
end

// ---- Decode helpers ----
wire        ctrl_start_bit  = dma_ctrl[31];
wire [3:0]  ctrl_mode       = dma_ctrl[3:0];
wire        ctrl_irq_en     = dma_ctrl[4];
wire [2:0]  ctrl_spi_id     = dma_ctrl[7:5];

// Aligned-to-32-bytes test (low 5 bits zero); applies to both SDRAM endpoints
// in MEM2MEM and to the SDRAM endpoint in MEM2SPI / SPI2MEM.
wire mem2mem_args_aligned =
    (dma_src[4:0] == 5'd0) &&
    (dma_dst[4:0] == 5'd0) &&
    (dma_count[4:0] == 5'd0) &&
    (dma_count != 32'd0);

// For MEM2SPI/SPI2MEM only the SDRAM-side address needs to be 32-byte aligned.
// dma_src is the SDRAM read address for MEM2SPI; dma_dst is the SDRAM write
// address for SPI2MEM. The SPI-side address (the other one) carries the
// flash byte address and has no alignment constraint as far as the engine is
// concerned -- software handles the flash command/address phase before
// kicking off DMA.
wire mem2spi_args_aligned =
    (dma_src[4:0] == 5'd0) &&
    (dma_count[4:0] == 5'd0) &&
    (dma_count != 32'd0);

wire spi2mem_args_aligned =
    (dma_dst[4:0] == 5'd0) &&
    (dma_count[4:0] == 5'd0) &&
    (dma_count != 32'd0);

// QSPI Fast Read: same alignment as plain SPI2MEM. Multi-line transfers
// are now handled by issuing a single big QSPIflash burst per DMA call
// and draining 32 bytes per SDRAM line. CS is held low for the whole
// transfer (the engine doesn't toggle CS), and the W25Q is happy with
// that as long as we don't try to issue a second opcode mid-transaction.
wire qspi_args_aligned =
    (dma_dst[4:0] == 5'd0) &&
    (dma_count[4:0] == 5'd0) &&
    (dma_count != 32'd0);

// MEM2VRAM: SDRAM source line-aligned, byte count line-aligned, and the
// destination must lie entirely within the VRAMPX byte range
// (0x1EC00000 .. 0x1EC20000).
wire mem2vram_args_aligned =
    (dma_src[4:0]   == 5'd0)         &&
    (dma_dst[4:0]   == 5'd0)         &&
    (dma_count[4:0] == 5'd0)         &&
    (dma_count      != 32'd0)        &&
    (dma_dst        >= 32'h1EC00000) &&
    ((dma_dst + dma_count) <= 32'h1EC20000);

// SPI0 (Flash 1), SPI1 (Flash 2 / BRFS via QSPI), SPI4 (Ethernet) and SPI5
// (SD card) are the four controllers wired through MemoryUnit's DMA burst
// port. Reject other SPI ids cleanly.
wire ctrl_spi_id_valid = (ctrl_spi_id == 3'd0) || (ctrl_spi_id == 3'd1) ||
                         (ctrl_spi_id == 3'd4) || (ctrl_spi_id == 3'd5);

// Reading STATUS clears the sticky bits, but the MemoryUnit holds reg_addr=4
// across an entire poll loop, so status_read sits high for many cycles in a
// row. We rising-edge-detect it so a multi-cycle hold counts as a single
// read; otherwise the engine's done/error bits would be cleared the cycle
// after they were set and the CPU would never see them.
wire status_read = (reg_we == 1'b0) && (reg_addr == 3'd4);
reg  status_read_d = 1'b0;
wire status_read_pulse = status_read && !status_read_d;

always @(posedge clk)
begin
    if (reset)
    begin
        dma_src         <= 32'd0;
        dma_dst         <= 32'd0;
        dma_count       <= 32'd0;
        dma_ctrl        <= 32'd0;
        dma_qspi_addr   <= 32'd0;
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
        spi_byte_idx    <= 6'd0;
        spi_id_sel      <= 3'd0;
        status_read_d   <= 1'b0;
        dma_burst_spi_id <= 3'd0;
        dma_burst_select <= 1'b0;
        dma_burst_we     <= 1'b0;
        dma_burst_data   <= 8'd0;
        dma_burst_start  <= 1'b0;
        dma_burst_len    <= 16'd0;
        dma_burst_dummy  <= 1'b0;
        dma_burst_re_rx  <= 1'b0;
        dma_burst_qspi_read <= 1'b0;
        dma_burst_qspi_addr <= 24'd0;
        qspi_addr_cur    <= 24'd0;
        qspi_burst_open  <= 1'b0;
        state           <= ST_IDLE;
    end
    else
    begin
        // Defaults (single-cycle pulses)
        sd_start         <= 1'b0;
        irq              <= 1'b0;
        dma_burst_start  <= 1'b0;
        dma_burst_we     <= 1'b0;
        dma_burst_re_rx  <= 1'b0;

        // ---- Register writes from MemoryUnit ----
        // Note: while busy, software is not supposed to scribble on
        // SRC/DST/COUNT/CTRL. Allow it anyway -- only the start bit is
        // gated below.
        if (reg_we)
        begin
            case (reg_addr)
                3'd0: dma_src       <= reg_data;
                3'd1: dma_dst       <= reg_data;
                3'd2: dma_count     <= reg_data;
                3'd3: dma_ctrl      <= reg_data;
                3'd5: dma_qspi_addr <= reg_data;
                default: ; // STATUS is read-only
            endcase
        end

        // ---- Status sticky-clear on read ----
        status_read_d <= status_read;
        if (status_read_pulse)
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
                    // Clear leftover sticky bits from the previous transfer
                    // so the new transfer's status reads see only its own
                    // outcome.
                    sticky_done  <= 1'b0;
                    sticky_error <= 1'b0;

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
                    else if (ctrl_mode == MODE_MEM2SPI)
                    begin
                        if (mem2spi_args_aligned && ctrl_spi_id_valid)
                        begin
                            src_cur         <= dma_src;
                            bytes_remaining <= dma_count;
                            spi_id_sel      <= ctrl_spi_id;
                            // Read the first SDRAM line, then walk it out
                            // byte-by-byte to the SPI TX register.
                            state           <= ST_RD_REQ;
                        end
                        else
                        begin
                            state <= ST_ERROR;
                        end
                    end
                    else if (ctrl_mode == MODE_SPI2MEM)
                    begin
                        if (spi2mem_args_aligned && ctrl_spi_id_valid)
                        begin
                            dst_cur          <= dma_dst;
                            bytes_remaining  <= dma_count;
                            spi_id_sel       <= ctrl_spi_id;
                            dma_burst_spi_id <= ctrl_spi_id;
                            spi_byte_idx     <= 6'd0;
                            line_buf         <= 256'd0;
                            // Kick off a 32-byte SPI dummy burst; the
                            // controller will accumulate 32 RX bytes
                            // which we drain into line_buf, then commit
                            // the line to SDRAM.
                            state            <= ST_S2M_BURST;
                        end
                        else
                        begin
                            state <= ST_ERROR;
                        end
                    end
                    else if (ctrl_mode == MODE_SPI2MEM_QSPI)
                    begin
                        // QSPI Fast Read is only wired through QSPIflash
                        // on SPI1 (BRFS flash). Other SPI ids -> error.
                        // Constrained to a single 32-byte line per call;
                        // software loops with CS toggle for larger reads.
                        if (qspi_args_aligned && (ctrl_spi_id == 3'd1))
                        begin
                            dst_cur          <= dma_dst;
                            bytes_remaining  <= dma_count;
                            spi_id_sel       <= 3'd1;
                            dma_burst_spi_id <= 3'd1;
                            spi_byte_idx     <= 6'd0;
                            line_buf         <= 256'd0;
                            qspi_addr_cur    <= dma_qspi_addr[23:0];
                            qspi_burst_open  <= 1'b0;
                            state            <= ST_S2M_BURST;
                        end
                        else
                        begin
                            state <= ST_ERROR;
                        end
                    end
                    else if (ctrl_mode == MODE_MEM2VRAM)
                    begin
                        if (mem2vram_args_aligned)
                        begin
                            src_cur         <= dma_src;
                            dst_cur         <= dma_dst;
                            bytes_remaining <= dma_count;
                            // First step: read the SDRAM line, then drain
                            // it into VRAMPX byte-by-byte.
                            state           <= ST_RD_REQ;
                        end
                        else
                        begin
                            state <= ST_ERROR;
                        end
                    end
                    else
                    begin
                        // MEM2IO / IO2MEM not implemented yet.
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
                    if (ctrl_mode == MODE_MEM2MEM)
                        state <= ST_WR_REQ;
                    else if (ctrl_mode == MODE_MEM2VRAM)
                    begin
                        // Drain line_buf into VRAMPX one byte per cycle.
                        spi_byte_idx <= 6'd0;
                        state        <= ST_M2V_DRAIN;
                    end
                    else
                    begin
                        // MEM2SPI: push the line into the SPI TX FIFO,
                        // then trigger a 32-byte burst.
                        dma_burst_spi_id <= spi_id_sel;
                        spi_byte_idx     <= 6'd0;
                        state            <= ST_M2S_FILL;
                    end
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
                    sd_start <= 1'b0;
                    if (ctrl_mode == MODE_MEM2MEM)
                    begin
                        src_cur         <= src_cur + 32'd32;
                        dst_cur         <= dst_cur + 32'd32;
                        bytes_remaining <= bytes_remaining - 32'd32;
                        if (bytes_remaining == 32'd32)
                            state <= ST_DONE;
                        else
                            state <= ST_RD_REQ;
                    end
                    else
                    begin
                        // SPI2MEM (and SPI2MEM_QSPI): this line is committed
                        // to SDRAM, advance and either start the next line
                        // or finish.
                        dst_cur         <= dst_cur + 32'd32;
                        bytes_remaining <= bytes_remaining - 32'd32;
                        // QSPI: advance the per-burst flash address by 32
                        // bytes. Harmless for plain SPI2MEM (qspi_addr_cur
                        // isn't read by ST_S2M_BURST in that mode).
                        qspi_addr_cur   <= qspi_addr_cur + 24'd32;
                        if (bytes_remaining == 32'd32)
                            state <= ST_DONE;
                        else
                        begin
                            spi_byte_idx <= 6'd0;
                            line_buf     <= 256'd0;
                            state        <= ST_S2M_BURST;
                        end
                    end
                end
            end

            // ---- SPI2MEM burst: kick off, wait, drain RX into line_buf ----
            ST_S2M_BURST:
            begin
                // Hold dma_burst_select high so SimpleSPI2's command-port
                // mux routes our signals. Pulse start_burst for one cycle
                // with dummy=1 so the controller sends 32 zero bytes and
                // captures 32 RX bytes into its RX FIFO.
                //
                // For MODE_SPI2MEM_QSPI we drive the QSPI Fast Read
                // controls (cmd_qspi_read + cmd_qspi_addr) so QSPIflash
                // issues opcode 0xEB + addr + M + dummy + data instead
                // of 32 1-bit dummy SPI bytes. We issue a SINGLE big
                // burst of the full transfer length so the W25Q stays
                // in one CS-low transaction; subsequent cache lines in
                // the same DMA call skip the kickoff and just drain
                // another 32 bytes from the rolling FIFO. qspi_burst_open
                // tracks whether the kickoff has happened.
                dma_burst_select <= 1'b1;
                dma_burst_dummy  <= 1'b1;
                spi_byte_idx     <= 6'd0;
                if (ctrl_mode == MODE_SPI2MEM_QSPI)
                begin
                    if (!qspi_burst_open)
                    begin
                        // First line of this DMA call: kick off one big
                        // QSPI burst that covers all bytes_remaining.
                        dma_burst_qspi_read <= 1'b1;
                        dma_burst_qspi_addr <= qspi_addr_cur;
                        dma_burst_len       <= bytes_remaining[15:0];
                        dma_burst_start     <= 1'b1;
                        qspi_burst_open     <= 1'b1;
                    end
                    // Either way, go to BWAIT which for QSPI waits for a
                    // full 32-byte line in the FIFO (dma_burst_rx_count
                    // >= 32) before transitioning to DRAIN.
                end
                else
                begin
                    // Plain SPI2MEM: per-line 32-byte burst as before.
                    dma_burst_qspi_read <= 1'b0;
                    dma_burst_len       <= 16'd32;
                    dma_burst_start     <= 1'b1;
                end
                state            <= ST_S2M_BWAIT;
            end

            ST_S2M_BWAIT:
            begin
                dma_burst_select <= 1'b1;
                if (ctrl_mode == MODE_SPI2MEM_QSPI)
                begin
                    // Wait until at least one full 32-byte cache line is
                    // sitting in the RX FIFO. The big burst keeps pushing
                    // bytes in the background; the FIFO is sized to 32 so
                    // it never overflows (drain rate 1 byte/cycle is ~8x
                    // the 4-bit-per-SCK fill rate).
                    if (dma_burst_rx_count >= 8'd32)
                    begin
                        dma_burst_re_rx <= 1'b1;
                        state           <= ST_S2M_DRAIN;
                    end
                end
                else if (dma_burst_done)
                begin
                    // Plain SPI2MEM: wait for the 32-byte burst to finish.
                    // Pre-assert re_rx so the controller pops byte 0 on
                    // the first DRAIN cycle (re_rx is an output reg with
                    // a one-cycle propagation delay; capturing rx_data
                    // and asserting re_rx in the same cycle would
                    // otherwise double-read byte 0 and skip byte 31).
                    dma_burst_re_rx <= 1'b1;
                    state           <= ST_S2M_DRAIN;
                end
            end

            ST_S2M_DRAIN:
            begin
                // Pop one byte per cycle from the RX FIFO. rx_data is
                // combinational from the head of the FIFO, so we capture
                // it the same cycle we assert re_rx (which advances the
                // pointer next cycle).
                dma_burst_select <= 1'b1;
                if (!dma_burst_rx_empty)
                begin
                    dma_burst_re_rx <= 1'b1;
                    case (spi_byte_idx)
                        6'd0:  line_buf[  7:  0] <= dma_burst_rx_data;
                        6'd1:  line_buf[ 15:  8] <= dma_burst_rx_data;
                        6'd2:  line_buf[ 23: 16] <= dma_burst_rx_data;
                        6'd3:  line_buf[ 31: 24] <= dma_burst_rx_data;
                        6'd4:  line_buf[ 39: 32] <= dma_burst_rx_data;
                        6'd5:  line_buf[ 47: 40] <= dma_burst_rx_data;
                        6'd6:  line_buf[ 55: 48] <= dma_burst_rx_data;
                        6'd7:  line_buf[ 63: 56] <= dma_burst_rx_data;
                        6'd8:  line_buf[ 71: 64] <= dma_burst_rx_data;
                        6'd9:  line_buf[ 79: 72] <= dma_burst_rx_data;
                        6'd10: line_buf[ 87: 80] <= dma_burst_rx_data;
                        6'd11: line_buf[ 95: 88] <= dma_burst_rx_data;
                        6'd12: line_buf[103: 96] <= dma_burst_rx_data;
                        6'd13: line_buf[111:104] <= dma_burst_rx_data;
                        6'd14: line_buf[119:112] <= dma_burst_rx_data;
                        6'd15: line_buf[127:120] <= dma_burst_rx_data;
                        6'd16: line_buf[135:128] <= dma_burst_rx_data;
                        6'd17: line_buf[143:136] <= dma_burst_rx_data;
                        6'd18: line_buf[151:144] <= dma_burst_rx_data;
                        6'd19: line_buf[159:152] <= dma_burst_rx_data;
                        6'd20: line_buf[167:160] <= dma_burst_rx_data;
                        6'd21: line_buf[175:168] <= dma_burst_rx_data;
                        6'd22: line_buf[183:176] <= dma_burst_rx_data;
                        6'd23: line_buf[191:184] <= dma_burst_rx_data;
                        6'd24: line_buf[199:192] <= dma_burst_rx_data;
                        6'd25: line_buf[207:200] <= dma_burst_rx_data;
                        6'd26: line_buf[215:208] <= dma_burst_rx_data;
                        6'd27: line_buf[223:216] <= dma_burst_rx_data;
                        6'd28: line_buf[231:224] <= dma_burst_rx_data;
                        6'd29: line_buf[239:232] <= dma_burst_rx_data;
                        6'd30: line_buf[247:240] <= dma_burst_rx_data;
                        6'd31: line_buf[255:248] <= dma_burst_rx_data;
                        default: ;
                    endcase
                    if (spi_byte_idx == 6'd31)
                    begin
                        // Done draining; release the SPI controller and
                        // commit the line to SDRAM.
                        dma_burst_select <= 1'b0;
                        state            <= ST_WR_REQ;
                    end
                    else
                    begin
                        spi_byte_idx <= spi_byte_idx + 6'd1;
                    end
                end
            end

            // ---- MEM2SPI burst: fill TX FIFO, kick off, wait, drain RX ----
            ST_M2S_FILL:
            begin
                dma_burst_select <= 1'b1;
                if (!dma_burst_tx_full)
                begin
                    dma_burst_we <= 1'b1;
                    case (spi_byte_idx)
                        6'd0:  dma_burst_data <= line_buf[  7:  0];
                        6'd1:  dma_burst_data <= line_buf[ 15:  8];
                        6'd2:  dma_burst_data <= line_buf[ 23: 16];
                        6'd3:  dma_burst_data <= line_buf[ 31: 24];
                        6'd4:  dma_burst_data <= line_buf[ 39: 32];
                        6'd5:  dma_burst_data <= line_buf[ 47: 40];
                        6'd6:  dma_burst_data <= line_buf[ 55: 48];
                        6'd7:  dma_burst_data <= line_buf[ 63: 56];
                        6'd8:  dma_burst_data <= line_buf[ 71: 64];
                        6'd9:  dma_burst_data <= line_buf[ 79: 72];
                        6'd10: dma_burst_data <= line_buf[ 87: 80];
                        6'd11: dma_burst_data <= line_buf[ 95: 88];
                        6'd12: dma_burst_data <= line_buf[103: 96];
                        6'd13: dma_burst_data <= line_buf[111:104];
                        6'd14: dma_burst_data <= line_buf[119:112];
                        6'd15: dma_burst_data <= line_buf[127:120];
                        6'd16: dma_burst_data <= line_buf[135:128];
                        6'd17: dma_burst_data <= line_buf[143:136];
                        6'd18: dma_burst_data <= line_buf[151:144];
                        6'd19: dma_burst_data <= line_buf[159:152];
                        6'd20: dma_burst_data <= line_buf[167:160];
                        6'd21: dma_burst_data <= line_buf[175:168];
                        6'd22: dma_burst_data <= line_buf[183:176];
                        6'd23: dma_burst_data <= line_buf[191:184];
                        6'd24: dma_burst_data <= line_buf[199:192];
                        6'd25: dma_burst_data <= line_buf[207:200];
                        6'd26: dma_burst_data <= line_buf[215:208];
                        6'd27: dma_burst_data <= line_buf[223:216];
                        6'd28: dma_burst_data <= line_buf[231:224];
                        6'd29: dma_burst_data <= line_buf[239:232];
                        6'd30: dma_burst_data <= line_buf[247:240];
                        6'd31: dma_burst_data <= line_buf[255:248];
                        default: ;
                    endcase
                    if (spi_byte_idx == 6'd31)
                    begin
                        spi_byte_idx <= 6'd0;
                        state        <= ST_M2S_BURST;
                    end
                    else
                    begin
                        spi_byte_idx <= spi_byte_idx + 6'd1;
                    end
                end
            end

            ST_M2S_BURST:
            begin
                dma_burst_select <= 1'b1;
                dma_burst_dummy  <= 1'b0;
                dma_burst_len    <= 16'd32;
                dma_burst_start  <= 1'b1;
                state            <= ST_M2S_BWAIT;
            end

            ST_M2S_BWAIT:
            begin
                dma_burst_select <= 1'b1;
                if (dma_burst_done)
                begin
                    // Pre-assert re_rx (same reasoning as ST_S2M_BWAIT).
                    dma_burst_re_rx <= 1'b1;
                    spi_byte_idx    <= 6'd0;
                    state           <= ST_M2S_DRAIN;
                end
            end

            ST_M2S_DRAIN:
            begin
                // Drain the 32 RX bytes pushed by SimpleSPI2 during the
                // write burst (we don't care about the values, but the
                // FIFO has to be empty for the next burst).
                dma_burst_select <= 1'b1;
                if (!dma_burst_rx_empty)
                begin
                    dma_burst_re_rx <= 1'b1;
                    if (spi_byte_idx == 6'd31)
                    begin
                        // Done with this line.
                        dma_burst_select <= 1'b0;
                        src_cur         <= src_cur + 32'd32;
                        bytes_remaining <= bytes_remaining - 32'd32;
                        if (bytes_remaining == 32'd32)
                            state <= ST_DONE;
                        else
                            state <= ST_RD_REQ;
                    end
                    else
                    begin
                        spi_byte_idx <= spi_byte_idx + 6'd1;
                    end
                end
            end

            // ---- MEM2VRAM drain: emit 32 line_buf bytes to VRAMPX ----
            // vp_we / vp_addr / vp_data are combinational (see assigns at
            // the bottom of the module). This block only advances the
            // engine's bookkeeping when the FIFO actually accepts the
            // byte, i.e. when !vp_full this same cycle.
            ST_M2V_DRAIN:
            begin
                if (!vp_full)
                begin
                    dst_cur <= dst_cur + 32'd1;
                    if (spi_byte_idx == 6'd31)
                    begin
                        // Whole line emitted. Advance and either fetch
                        // the next line or finish.
                        src_cur         <= src_cur + 32'd32;
                        bytes_remaining <= bytes_remaining - 32'd32;
                        if (bytes_remaining == 32'd32)
                            state <= ST_DONE;
                        else
                            state <= ST_RD_REQ;
                    end
                    else
                    begin
                        spi_byte_idx <= spi_byte_idx + 6'd1;
                    end
                end
            end

            ST_DONE:
            begin
                dma_burst_select <= 1'b0;
                busy        <= 1'b0;
                sticky_done <= 1'b1;
                qspi_burst_open <= 1'b0;
                if (ctrl_irq_en)
                    irq <= 1'b1;
                state <= ST_IDLE;
            end

            ST_ERROR:
            begin
                dma_burst_select <= 1'b0;
                busy         <= 1'b0;
                sticky_error <= 1'b1;
                qspi_burst_open <= 1'b0;
                if (ctrl_irq_en)
                    irq <= 1'b1;
                state <= ST_IDLE;
            end

            default: state <= ST_IDLE;
        endcase
    end
end

// ---- VRAMPX peer-port combinational drives ----
// Drive vp_we / vp_addr / vp_data combinationally from the engine state so
// the FIFO write_enable lines up with the FIFO's own !full check the same
// cycle. (See the long comment on the vp_we port declaration.)
assign vp_we   = (state == ST_M2V_DRAIN) && !vp_full;
assign vp_addr = dst_cur[16:0];
assign vp_data =
    (spi_byte_idx == 6'd0)  ? line_buf[  7:  0] :
    (spi_byte_idx == 6'd1)  ? line_buf[ 15:  8] :
    (spi_byte_idx == 6'd2)  ? line_buf[ 23: 16] :
    (spi_byte_idx == 6'd3)  ? line_buf[ 31: 24] :
    (spi_byte_idx == 6'd4)  ? line_buf[ 39: 32] :
    (spi_byte_idx == 6'd5)  ? line_buf[ 47: 40] :
    (spi_byte_idx == 6'd6)  ? line_buf[ 55: 48] :
    (spi_byte_idx == 6'd7)  ? line_buf[ 63: 56] :
    (spi_byte_idx == 6'd8)  ? line_buf[ 71: 64] :
    (spi_byte_idx == 6'd9)  ? line_buf[ 79: 72] :
    (spi_byte_idx == 6'd10) ? line_buf[ 87: 80] :
    (spi_byte_idx == 6'd11) ? line_buf[ 95: 88] :
    (spi_byte_idx == 6'd12) ? line_buf[103: 96] :
    (spi_byte_idx == 6'd13) ? line_buf[111:104] :
    (spi_byte_idx == 6'd14) ? line_buf[119:112] :
    (spi_byte_idx == 6'd15) ? line_buf[127:120] :
    (spi_byte_idx == 6'd16) ? line_buf[135:128] :
    (spi_byte_idx == 6'd17) ? line_buf[143:136] :
    (spi_byte_idx == 6'd18) ? line_buf[151:144] :
    (spi_byte_idx == 6'd19) ? line_buf[159:152] :
    (spi_byte_idx == 6'd20) ? line_buf[167:160] :
    (spi_byte_idx == 6'd21) ? line_buf[175:168] :
    (spi_byte_idx == 6'd22) ? line_buf[183:176] :
    (spi_byte_idx == 6'd23) ? line_buf[191:184] :
    (spi_byte_idx == 6'd24) ? line_buf[199:192] :
    (spi_byte_idx == 6'd25) ? line_buf[207:200] :
    (spi_byte_idx == 6'd26) ? line_buf[215:208] :
    (spi_byte_idx == 6'd27) ? line_buf[223:216] :
    (spi_byte_idx == 6'd28) ? line_buf[231:224] :
    (spi_byte_idx == 6'd29) ? line_buf[239:232] :
    (spi_byte_idx == 6'd30) ? line_buf[247:240] :
                              line_buf[255:248];

endmodule

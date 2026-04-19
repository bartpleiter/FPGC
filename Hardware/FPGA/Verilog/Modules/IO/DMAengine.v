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

    // ---- I/O peer port (to MemoryUnit iop_*) ----
    output reg          iop_start = 1'b0,
    output reg          iop_we    = 1'b0,
    output reg  [31:0]  iop_addr  = 32'd0,
    output reg  [31:0]  iop_data  = 32'd0,
    input  wire         iop_done,
    input  wire [31:0]  iop_q,

    // ---- VRAMPX peer port (to MemoryUnit vp_*; unused this commit) ----
    output wire         vp_we,
    output wire [16:0]  vp_addr,
    output wire [7:0]   vp_data,

    // ---- Interrupt to InterruptController bit 8 (INT_ID_DMA = 9) ----
    output reg          irq       = 1'b0
);

// MEM2MEM + MEM2SPI + SPI2MEM commit: VRAMPX peer port still inactive
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

// ---- SPI byte cursor (used by MEM2SPI / SPI2MEM) ----
// Counts bytes within the current 32-byte cache line, 0..31.
reg [4:0]  spi_byte_idx = 5'd0;
// Latched at start: which SPI bus + the corresponding MMIO data address.
reg        spi_is_eth   = 1'b0;       // 0 = SPI0 (Flash), 1 = SPI4 (Eth)

// SPI MMIO data register addresses (mirror MemoryUnit.v ADDR_SPI*_DATA)
localparam [31:0] ADDR_SPI0_DATA = 32'h1C000020;
localparam [31:0] ADDR_SPI4_DATA = 32'h1C000048;
wire [31:0] spi_data_addr = spi_is_eth ? ADDR_SPI4_DATA : ADDR_SPI0_DATA;

localparam
    ST_IDLE          = 4'd0,
    ST_RD_REQ        = 4'd1,   // MEM2MEM / MEM2SPI: SDRAM read request
    ST_RD_WAIT       = 4'd2,
    ST_WR_REQ        = 4'd3,   // MEM2MEM / SPI2MEM: SDRAM write request
    ST_WR_WAIT       = 4'd4,
    ST_DONE          = 4'd5,
    ST_ERROR         = 4'd6,
    ST_S2M_BYTE_REQ  = 4'd7,   // SPI2MEM: issue iop transaction (TX 0x00)
    ST_S2M_BYTE_WAIT = 4'd8,   // SPI2MEM: wait for iop_done, capture RX byte
    ST_M2S_BYTE_REQ  = 4'd9,   // MEM2SPI: issue iop transaction (TX line byte)
    ST_M2S_BYTE_WAIT = 4'd10;  // MEM2SPI: wait for iop_done, ignore RX byte

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

// Only SPI0 (id=0, Flash) and SPI4 (id=4, Ethernet) are wired through
// MemoryUnit's iop_* port today. Reject other SPI ids cleanly.
wire ctrl_spi_id_valid = (ctrl_spi_id == 3'd0) || (ctrl_spi_id == 3'd4);

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
        spi_byte_idx    <= 5'd0;
        spi_is_eth      <= 1'b0;
        iop_start       <= 1'b0;
        iop_we          <= 1'b0;
        iop_addr        <= 32'd0;
        iop_data        <= 32'd0;
        state           <= ST_IDLE;
    end
    else
    begin
        // Defaults (single-cycle pulses)
        sd_start <= 1'b0;
        iop_start <= 1'b0;
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
                    else if (ctrl_mode == MODE_MEM2SPI)
                    begin
                        if (mem2spi_args_aligned && ctrl_spi_id_valid)
                        begin
                            src_cur         <= dma_src;
                            bytes_remaining <= dma_count;
                            spi_is_eth      <= (ctrl_spi_id == 3'd4);
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
                            dst_cur         <= dma_dst;
                            bytes_remaining <= dma_count;
                            spi_is_eth      <= (ctrl_spi_id == 3'd4);
                            spi_byte_idx    <= 5'd0;
                            line_buf        <= 256'd0;
                            // Start fetching bytes from SPI; once 32 are in,
                            // ST_S2M_BYTE_WAIT transitions to ST_WR_REQ.
                            state           <= ST_S2M_BYTE_REQ;
                        end
                        else
                        begin
                            state <= ST_ERROR;
                        end
                    end
                    else
                    begin
                        // MEM2VRAM / MEM2IO / IO2MEM not implemented yet.
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
                    else
                    begin
                        // MEM2SPI: walk the freshly-fetched line out byte by byte.
                        spi_byte_idx <= 5'd0;
                        state        <= ST_M2S_BYTE_REQ;
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
                        // SPI2MEM: this line is committed to SDRAM, advance
                        // and either start the next line or finish.
                        dst_cur         <= dst_cur + 32'd32;
                        bytes_remaining <= bytes_remaining - 32'd32;
                        if (bytes_remaining == 32'd32)
                            state <= ST_DONE;
                        else
                        begin
                            spi_byte_idx <= 5'd0;
                            line_buf     <= 256'd0;
                            state        <= ST_S2M_BYTE_REQ;
                        end
                    end
                end
            end

            // ---- SPI2MEM byte loop ----
            ST_S2M_BYTE_REQ:
            begin
                // Issue a dummy-write byte transaction. MemoryUnit's
                // STATE_WAIT_SPI*_DATA returns the RX byte regardless of we,
                // so this both clocks the SPI MISO and yields the byte we
                // need.
                iop_addr  <= spi_data_addr;
                iop_data  <= 32'd0;
                iop_we    <= 1'b1;
                iop_start <= 1'b1;
                state     <= ST_S2M_BYTE_WAIT;
            end

            ST_S2M_BYTE_WAIT:
            begin
                iop_start <= 1'b1; // hold until iop_done
                if (iop_done)
                begin
                    iop_start <= 1'b0;
                    // Place RX byte at the right position in line_buf
                    case (spi_byte_idx)
                        5'd0:  line_buf[  7:  0] <= iop_q[7:0];
                        5'd1:  line_buf[ 15:  8] <= iop_q[7:0];
                        5'd2:  line_buf[ 23: 16] <= iop_q[7:0];
                        5'd3:  line_buf[ 31: 24] <= iop_q[7:0];
                        5'd4:  line_buf[ 39: 32] <= iop_q[7:0];
                        5'd5:  line_buf[ 47: 40] <= iop_q[7:0];
                        5'd6:  line_buf[ 55: 48] <= iop_q[7:0];
                        5'd7:  line_buf[ 63: 56] <= iop_q[7:0];
                        5'd8:  line_buf[ 71: 64] <= iop_q[7:0];
                        5'd9:  line_buf[ 79: 72] <= iop_q[7:0];
                        5'd10: line_buf[ 87: 80] <= iop_q[7:0];
                        5'd11: line_buf[ 95: 88] <= iop_q[7:0];
                        5'd12: line_buf[103: 96] <= iop_q[7:0];
                        5'd13: line_buf[111:104] <= iop_q[7:0];
                        5'd14: line_buf[119:112] <= iop_q[7:0];
                        5'd15: line_buf[127:120] <= iop_q[7:0];
                        5'd16: line_buf[135:128] <= iop_q[7:0];
                        5'd17: line_buf[143:136] <= iop_q[7:0];
                        5'd18: line_buf[151:144] <= iop_q[7:0];
                        5'd19: line_buf[159:152] <= iop_q[7:0];
                        5'd20: line_buf[167:160] <= iop_q[7:0];
                        5'd21: line_buf[175:168] <= iop_q[7:0];
                        5'd22: line_buf[183:176] <= iop_q[7:0];
                        5'd23: line_buf[191:184] <= iop_q[7:0];
                        5'd24: line_buf[199:192] <= iop_q[7:0];
                        5'd25: line_buf[207:200] <= iop_q[7:0];
                        5'd26: line_buf[215:208] <= iop_q[7:0];
                        5'd27: line_buf[223:216] <= iop_q[7:0];
                        5'd28: line_buf[231:224] <= iop_q[7:0];
                        5'd29: line_buf[239:232] <= iop_q[7:0];
                        5'd30: line_buf[247:240] <= iop_q[7:0];
                        5'd31: line_buf[255:248] <= iop_q[7:0];
                    endcase

                    if (spi_byte_idx == 5'd31)
                        state <= ST_WR_REQ;
                    else
                    begin
                        spi_byte_idx <= spi_byte_idx + 5'd1;
                        state        <= ST_S2M_BYTE_REQ;
                    end
                end
            end

            // ---- MEM2SPI byte loop ----
            ST_M2S_BYTE_REQ:
            begin
                iop_addr  <= spi_data_addr;
                case (spi_byte_idx)
                    5'd0:  iop_data <= {24'd0, line_buf[  7:  0]};
                    5'd1:  iop_data <= {24'd0, line_buf[ 15:  8]};
                    5'd2:  iop_data <= {24'd0, line_buf[ 23: 16]};
                    5'd3:  iop_data <= {24'd0, line_buf[ 31: 24]};
                    5'd4:  iop_data <= {24'd0, line_buf[ 39: 32]};
                    5'd5:  iop_data <= {24'd0, line_buf[ 47: 40]};
                    5'd6:  iop_data <= {24'd0, line_buf[ 55: 48]};
                    5'd7:  iop_data <= {24'd0, line_buf[ 63: 56]};
                    5'd8:  iop_data <= {24'd0, line_buf[ 71: 64]};
                    5'd9:  iop_data <= {24'd0, line_buf[ 79: 72]};
                    5'd10: iop_data <= {24'd0, line_buf[ 87: 80]};
                    5'd11: iop_data <= {24'd0, line_buf[ 95: 88]};
                    5'd12: iop_data <= {24'd0, line_buf[103: 96]};
                    5'd13: iop_data <= {24'd0, line_buf[111:104]};
                    5'd14: iop_data <= {24'd0, line_buf[119:112]};
                    5'd15: iop_data <= {24'd0, line_buf[127:120]};
                    5'd16: iop_data <= {24'd0, line_buf[135:128]};
                    5'd17: iop_data <= {24'd0, line_buf[143:136]};
                    5'd18: iop_data <= {24'd0, line_buf[151:144]};
                    5'd19: iop_data <= {24'd0, line_buf[159:152]};
                    5'd20: iop_data <= {24'd0, line_buf[167:160]};
                    5'd21: iop_data <= {24'd0, line_buf[175:168]};
                    5'd22: iop_data <= {24'd0, line_buf[183:176]};
                    5'd23: iop_data <= {24'd0, line_buf[191:184]};
                    5'd24: iop_data <= {24'd0, line_buf[199:192]};
                    5'd25: iop_data <= {24'd0, line_buf[207:200]};
                    5'd26: iop_data <= {24'd0, line_buf[215:208]};
                    5'd27: iop_data <= {24'd0, line_buf[223:216]};
                    5'd28: iop_data <= {24'd0, line_buf[231:224]};
                    5'd29: iop_data <= {24'd0, line_buf[239:232]};
                    5'd30: iop_data <= {24'd0, line_buf[247:240]};
                    5'd31: iop_data <= {24'd0, line_buf[255:248]};
                endcase
                iop_we    <= 1'b1;
                iop_start <= 1'b1;
                state     <= ST_M2S_BYTE_WAIT;
            end

            ST_M2S_BYTE_WAIT:
            begin
                iop_start <= 1'b1;
                if (iop_done)
                begin
                    iop_start <= 1'b0;
                    // RX byte (iop_q) is ignored on writes.
                    if (spi_byte_idx == 5'd31)
                    begin
                        src_cur         <= src_cur + 32'd32;
                        bytes_remaining <= bytes_remaining - 32'd32;
                        if (bytes_remaining == 32'd32)
                            state <= ST_DONE;
                        else
                        begin
                            spi_byte_idx <= 5'd0;
                            state        <= ST_RD_REQ;
                        end
                    end
                    else
                    begin
                        spi_byte_idx <= spi_byte_idx + 5'd1;
                        state        <= ST_M2S_BYTE_REQ;
                    end
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

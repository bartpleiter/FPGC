/*
 * QSPIflash
 * ---------
 * Quad-IO-capable SPI flash controller for the FPGC. Drop-in replacement
 * for SimpleSPI2 on the BRFS flash port (SPI1 / `flash2`).
 *
 * Two operating modes, selected per burst:
 *
 *   1-bit SPI Mode 0 (default):
 *     Same FIFO-/skip-FIFO-/dummy-byte protocol as SimpleSPI2.
 *     IO0 = MOSI (driven), IO1 = MISO (sampled), IO2/IO3 driven high
 *     to mimic the existing flash2_wp_n / flash2_hold_n tie-offs.
 *     Used for every flash op except fast reads (RDID, RDSR, WREN,
 *     WRDI, WRSR, PP page program, SE sector erase, BE block erase,
 *     CE chip erase, slow read).
 *
 *   QSPI Fast Read with continuous mode (cmd_qspi_read=1):
 *     Issues opcode 0xEB on IO0 (or skips it if continuous mode is
 *     already latched in the chip), sends the 24-bit address +
 *     M7..M0 mode bits on all four IO lines, drives 4 dummy SCK
 *     cycles with IOs tri-stated, then receives `cmd_burst_len`
 *     data bytes 4 bits per SCK cycle. After the burst the chip
 *     stays in continuous read mode (M[7:4]=0xA) so subsequent
 *     bursts can skip the opcode -- saving 8 SCK cycles per access.
 *     The host can issue a 1-bit transaction (any non-QSPI burst)
 *     to drop continuous mode; the controller automatically
 *     re-emits the opcode on the next QSPI burst.
 *
 * Chip-select is left to software, exactly like SimpleSPI2.
 *
 * Pin interface:
 *   spi_clk          SPI clock (Mode 0 -- low when idle).
 *   spi_io_out[3:0]  Data driven onto IO0..IO3 when OE bit is 1.
 *   spi_io_oe[3:0]   1 = drive, 0 = high-Z (tristate at top level).
 *   spi_io_in[3:0]   Sampled values of IO0..IO3.
 *
 * The bit-shuffle for the 4-bit phases follows tmp/qspi_reader.v's
 * scheme: each cycle drives/captures 4 bits, with bit 0 mapped to
 * IO0, bit 1 to IO1, etc. Two cycles per data byte (high nibble first).
 */
module QSPIflash #(
    parameter CLKS_PER_HALF_BIT = 2,
    parameter FIFO_DEPTH        = 16
) (
    // ---- System ----
    input  wire        clk,
    input  wire        reset,

    // ---- Command / data (CPU MMIO master) ----
    input  wire        cmd_we,          // pulse: push cmd_data into TX FIFO
    input  wire [7:0]  cmd_data,
    input  wire        cmd_start_burst,
    input  wire [15:0] cmd_burst_len,
    input  wire        cmd_dummy,
    input  wire        cmd_skip_fifos,
    input  wire        cmd_qspi_read,         // latched at start: this burst is a QSPI Fast Read
    input  wire [23:0] cmd_qspi_addr,         // 24-bit byte address for QSPI read

    output wire        tx_full,
    output wire        rx_empty,
    output wire [7:0]  rx_data,
    output wire [7:0]  rx_count_out, // current bytes-in-FIFO (zero-extended)
    input  wire        cmd_re_rx,
    output reg  [7:0]  last_rx_byte = 8'd0,

    output wire        busy,
    output reg         done = 1'b0,

    // ---- DMA engine master ----
    input  wire        dma_select,
    input  wire        dma_we,
    input  wire [7:0]  dma_data,
    input  wire        dma_start_burst,
    input  wire [15:0] dma_burst_len,
    input  wire        dma_dummy,
    input  wire        dma_qspi_read,
    input  wire [23:0] dma_qspi_addr,
    input  wire        dma_re_rx,

    // ---- Pins (bidirectional quad bus, tristated externally) ----
    output reg         spi_clk    = 1'b0,
    output reg  [3:0]  spi_io_out = 4'b1100,
    output reg  [3:0]  spi_io_oe  = 4'b1101,
    input  wire [3:0]  spi_io_in
);

// ============================================================================
// FIFO infrastructure (TX and RX)
// ============================================================================
localparam FIFO_AW = (FIFO_DEPTH > 1) ? $clog2(FIFO_DEPTH) : 1;

wire        muxed_we          = dma_select ? dma_we          : cmd_we;
wire [7:0]  muxed_data        = dma_select ? dma_data        : cmd_data;
wire        muxed_start_burst = dma_select ? dma_start_burst : cmd_start_burst;
wire [15:0] muxed_burst_len   = dma_select ? dma_burst_len   : cmd_burst_len;
wire        muxed_dummy       = dma_select ? dma_dummy       : cmd_dummy;
wire        muxed_skip_fifos  = dma_select ? 1'b0            : cmd_skip_fifos;
wire        muxed_qspi_read   = dma_select ? dma_qspi_read   : cmd_qspi_read;
wire [23:0] muxed_qspi_addr   = dma_select ? dma_qspi_addr   : cmd_qspi_addr;
wire        muxed_re_rx       = dma_select ? dma_re_rx       : cmd_re_rx;

reg [7:0]         tx_mem [0:FIFO_DEPTH-1];
reg [FIFO_AW:0]   tx_count = 0;
reg [FIFO_AW-1:0] tx_rd_ptr = 0;
reg [FIFO_AW-1:0] tx_wr_ptr = 0;
wire              tx_empty_w = (tx_count == 0);
assign            tx_full    = (tx_count == FIFO_DEPTH);

reg [7:0]         rx_mem [0:FIFO_DEPTH-1];
reg [FIFO_AW:0]   rx_count = 0;
reg [FIFO_AW-1:0] rx_rd_ptr = 0;
reg [FIFO_AW-1:0] rx_wr_ptr = 0;
assign            rx_empty   = (rx_count == 0);
wire              rx_full_w  = (rx_count == FIFO_DEPTH);
assign            rx_data    = rx_mem[rx_rd_ptr];
assign            rx_count_out = {{(8-FIFO_AW-1){1'b0}}, rx_count};

reg               tx_pop  = 1'b0;
reg               rx_push = 1'b0;
reg  [7:0]        rx_push_data = 8'd0;

integer fi;

// ============================================================================
// State machine
// ============================================================================
localparam
    // Shared
    STATE_IDLE         = 4'd0,
    STATE_DONE         = 4'd1,
    // 1-bit SPI bursts (mirror SimpleSPI2)
    STATE_LOAD_BYTE    = 4'd2,
    STATE_TRANSFER     = 4'd3,
    // QSPI Fast Read phases
    STATE_Q_OPCODE     = 4'd4,   // 8 cycles, 0xEB on IO0 only
    STATE_Q_ADDR_MODE  = 4'd5,   // 6 cycles addr + 2 cycles M, all IOs out
    STATE_Q_DUMMY      = 4'd6,   // 4 cycles, IOs tristated
    STATE_Q_DATA       = 4'd7;   // 2 cycles per byte, IOs in

reg [3:0] state = STATE_IDLE;

localparam CLK_COUNT_WIDTH = (CLKS_PER_HALF_BIT > 1) ? $clog2(CLKS_PER_HALF_BIT*2) : 1;
reg [CLK_COUNT_WIDTH-1:0] clk_count = 0;

reg        spi_clk_internal = 1'b0;
reg [4:0]  edge_count = 5'd0;

// 1-bit shift state
reg [7:0]  tx_shift   = 8'd0;
reg [2:0]  tx_bit_idx = 3'd7;
reg [2:0]  rx_bit_idx = 3'd7;
reg [7:0]  rx_shift   = 8'd0;

reg        leading_edge  = 1'b0;
reg        trailing_edge = 1'b0;

reg [15:0] burst_remaining = 16'd0;
reg        dummy_latched      = 1'b0;
reg        skip_fifos_latched = 1'b0;
reg [7:0]  skip_data_latched  = 8'd0;

// QSPI shift state
//   q_phase_count counts SCK cycles within the current QSPI phase.
//   q_addr_mode   = {addr[23:0], 8'hA5} = 32-bit shift register, MSB-first
//                   in 4-bit groups. We use M[7:4]=0xA so the chip latches
//                   continuous mode; M[3:0] is don't-care, kept as 5 to
//                   match tmp/qspi_reader.v.
//   q_byte_phase  = 0 -> high nibble next, 1 -> low nibble next.
//   q_byte_acc    = current byte being assembled.
reg [4:0]  q_phase_count = 5'd0;
reg [31:0] q_addr_mode   = 32'd0;
reg        q_byte_phase  = 1'b0;
reg [7:0]  q_byte_acc    = 8'd0;

// Tracks whether continuous mode is currently latched in the flash chip.
// Set after a successful QSPI read; cleared whenever a 1-bit burst runs
// (which ends with CS rising in software, dropping the chip out of
// continuous mode for any new sequence that starts with an opcode).
reg        q_continuous = 1'b0;

assign busy = (state != STATE_IDLE);

// MISO (1-bit) is on IO1
wire spi_miso_w = spi_io_in[1];

// ----- Per-state OE / IO drive computation -----
// We default to the iter-1 1-bit pattern (drive IO0/IO2/IO3, sample IO1)
// and override per QSPI state.
reg [3:0]  io_oe_next  = 4'b1101;
reg [3:0]  io_out_next = 4'b1100;

reg        spi_mosi_r  = 1'b0;     // 1-bit MOSI bit, mapped onto IO0

always @(*) begin
    // Default: 1-bit mode pin layout
    io_oe_next  = 4'b1101;
    io_out_next = { 1'b1, 1'b1, 1'b0, spi_mosi_r };

    case (state)
        STATE_Q_OPCODE: begin
            // 1-bit on IO0; IO1 sampled (don't care here); IO2/IO3 high.
            io_oe_next  = 4'b1101;
            io_out_next = { 1'b1, 1'b1, 1'b0, spi_mosi_r };
        end
        STATE_Q_ADDR_MODE: begin
            // 4-bit out on all IOs.
            io_oe_next  = 4'b1111;
            io_out_next = q_addr_mode[31:28];
        end
        STATE_Q_DUMMY: begin
            // All IOs tri-stated.
            io_oe_next  = 4'b0000;
            io_out_next = 4'b0000;
        end
        STATE_Q_DATA: begin
            // 4-bit in on all IOs.
            io_oe_next  = 4'b0000;
            io_out_next = 4'b0000;
        end
        default: ;
    endcase
end

always @(posedge clk)
begin
    if (reset)
    begin
        // FIFO pointers
        tx_count  <= 0;
        tx_rd_ptr <= 0;
        tx_wr_ptr <= 0;
        rx_count  <= 0;
        rx_rd_ptr <= 0;
        rx_wr_ptr <= 0;

        // Engine
        state            <= STATE_IDLE;
        clk_count        <= 0;
        edge_count       <= 5'd0;
        spi_clk_internal <= 1'b0;
        spi_clk          <= 1'b0;
        spi_mosi_r       <= 1'b0;
        spi_io_out       <= 4'b1100;   // IO2/IO3 high (wp_n/hold_n), IO0/IO1 low
        spi_io_oe        <= 4'b1101;
        tx_shift         <= 8'd0;
        tx_bit_idx       <= 3'd7;
        rx_bit_idx       <= 3'd7;
        rx_shift         <= 8'd0;
        leading_edge     <= 1'b0;
        trailing_edge    <= 1'b0;
        burst_remaining  <= 16'd0;
        dummy_latched      <= 1'b0;
        skip_fifos_latched <= 1'b0;
        skip_data_latched  <= 8'd0;
        last_rx_byte       <= 8'd0;
        done             <= 1'b0;
        tx_pop           <= 1'b0;
        rx_push          <= 1'b0;
        q_phase_count    <= 5'd0;
        q_addr_mode      <= 32'd0;
        q_byte_phase     <= 1'b0;
        q_byte_acc       <= 8'd0;
        q_continuous     <= 1'b0;
    end
    else
    begin
        done          <= 1'b0;
        leading_edge  <= 1'b0;
        trailing_edge <= 1'b0;
        tx_pop        <= 1'b0;
        rx_push       <= 1'b0;

        // Pin drive register update (combinational source, registered output)
        spi_io_oe  <= io_oe_next;
        spi_io_out <= io_out_next;

        // ----- TX FIFO write port -----
        if (muxed_we && !tx_full)
        begin
            tx_mem[tx_wr_ptr] <= muxed_data;
            tx_wr_ptr <= tx_wr_ptr + 1'b1;
        end

        // ----- RX FIFO read port -----
        if (muxed_re_rx && !rx_empty)
        begin
            rx_rd_ptr <= rx_rd_ptr + 1'b1;
        end

        // ----- Shift engine -----
        case (state)
            STATE_IDLE:
            begin
                spi_clk_internal <= 1'b0;
                spi_clk          <= 1'b0;
                clk_count        <= 0;
                edge_count       <= 5'd0;
                tx_bit_idx       <= 3'd7;
                rx_bit_idx       <= 3'd7;
                q_phase_count    <= 5'd0;
                q_byte_phase     <= 1'b0;

                if (muxed_start_burst && muxed_burst_len != 16'd0)
                begin
                    burst_remaining    <= muxed_burst_len;
                    dummy_latched      <= muxed_dummy;
                    skip_fifos_latched <= muxed_skip_fifos;
                    skip_data_latched  <= muxed_data;

                    if (muxed_qspi_read)
                    begin
                        // Pre-load opcode (0xEB) MSB into spi_mosi_r so the
                        // first SCK rising edge in the opcode phase samples
                        // the right value; q_addr_mode for the address+M
                        // phase. M[7:4] = 0xA, M[3:0] = 5 (don't care).
                        spi_mosi_r       <= 1'b1;       // 0xEB MSB = 1
                        tx_shift         <= 8'hEB;
                        tx_bit_idx       <= 3'd6;
                        q_addr_mode      <= { muxed_qspi_addr, 8'h00 };

                        if (q_continuous)
                        begin
                            // Skip the opcode phase entirely.
                            q_phase_count <= 5'd8;      // 6 addr + 2 mode
                            edge_count    <= q_continuous ? 5'd16 : 5'd16; // unused in QSPI
                            state         <= STATE_Q_ADDR_MODE;
                        end
                        else
                        begin
                            q_phase_count <= 5'd8;      // 8 cycles of opcode
                            state         <= STATE_Q_OPCODE;
                        end
                    end
                    else
                    begin
                        state <= STATE_LOAD_BYTE;
                    end
                end
            end

            // ============================================================
            // 1-bit SPI path (identical to SimpleSPI2)
            // ============================================================
            STATE_LOAD_BYTE:
            begin
                if (skip_fifos_latched)
                begin
                    tx_shift   <= skip_data_latched;
                    spi_mosi_r <= skip_data_latched[7];
                end
                else if (dummy_latched)
                begin
                    tx_shift   <= 8'd0;
                    spi_mosi_r <= 1'b0;
                end
                else if (!tx_empty_w)
                begin
                    tx_shift   <= tx_mem[tx_rd_ptr];
                    spi_mosi_r <= tx_mem[tx_rd_ptr][7];
                    tx_pop     <= 1'b1;
                end

                if (skip_fifos_latched || dummy_latched || !tx_empty_w)
                begin
                    tx_bit_idx <= 3'd6;
                    rx_bit_idx <= 3'd7;
                    rx_shift   <= 8'd0;
                    if (CLKS_PER_HALF_BIT > 1)
                        edge_count <= 5'd16;
                    else
                        edge_count <= 5'd15;
                    clk_count  <= 0;
                    state      <= STATE_TRANSFER;
                end

                // Any 1-bit burst drops the chip out of continuous mode for
                // the next QSPI burst (the chip is left at CS-high by the
                // driver, which loses the M latch on the next opcode).
                q_continuous <= 1'b0;
            end

            STATE_TRANSFER:
            begin
                if (clk_count == CLKS_PER_HALF_BIT * 2 - 1)
                begin
                    clk_count        <= 0;
                    spi_clk_internal <= ~spi_clk_internal;
                    trailing_edge    <= 1'b1;
                    edge_count       <= edge_count - 1'b1;
                end
                else if (clk_count == CLKS_PER_HALF_BIT - 1)
                begin
                    clk_count        <= clk_count + 1'b1;
                    spi_clk_internal <= ~spi_clk_internal;
                    leading_edge     <= 1'b1;
                    edge_count       <= edge_count - 1'b1;
                end
                else
                begin
                    clk_count <= clk_count + 1'b1;
                end

                if (edge_count == 0)
                begin
                    last_rx_byte <= rx_shift;

                    if (!skip_fifos_latched)
                    begin
                        rx_push      <= 1'b1;
                        rx_push_data <= rx_shift;
                    end

                    burst_remaining <= burst_remaining - 1'b1;
                    if (burst_remaining == 16'd1)
                        state <= STATE_DONE;
                    else
                        state <= STATE_LOAD_BYTE;
                end
            end

            // ============================================================
            // QSPI Fast Read path
            // ============================================================
            // For all four QSPI states we generate a half-rate SCK by
            // toggling spi_clk_internal every CLKS_PER_HALF_BIT system
            // cycles. q_phase_count counts the remaining SCK cycles in
            // the current phase (decremented on the trailing edge).
            STATE_Q_OPCODE:
            begin
                // Run the same SCK generator as STATE_TRANSFER, but
                // shift out the 0xEB byte 1 bit/cycle on IO0.
                if (clk_count == CLKS_PER_HALF_BIT * 2 - 1)
                begin
                    clk_count        <= 0;
                    spi_clk_internal <= ~spi_clk_internal;
                    trailing_edge    <= 1'b1;
                end
                else if (clk_count == CLKS_PER_HALF_BIT - 1)
                begin
                    clk_count        <= clk_count + 1'b1;
                    spi_clk_internal <= ~spi_clk_internal;
                    leading_edge     <= 1'b1;
                end
                else
                begin
                    clk_count <= clk_count + 1'b1;
                end

                if (trailing_edge)
                begin
                    spi_mosi_r    <= tx_shift[tx_bit_idx];
                    tx_bit_idx    <= tx_bit_idx - 1'b1;
                    q_phase_count <= q_phase_count - 1'b1;
                    if (q_phase_count == 5'd1)
                    begin
                        // After the 8th opcode bit's trailing edge, switch
                        // to the address+mode phase.
                        q_phase_count <= 5'd8;   // 6 addr nibbles + 2 mode nibbles
                        state         <= STATE_Q_ADDR_MODE;
                    end
                end
            end

            STATE_Q_ADDR_MODE:
            begin
                if (clk_count == CLKS_PER_HALF_BIT * 2 - 1)
                begin
                    clk_count        <= 0;
                    spi_clk_internal <= ~spi_clk_internal;
                    trailing_edge    <= 1'b1;
                end
                else if (clk_count == CLKS_PER_HALF_BIT - 1)
                begin
                    clk_count        <= clk_count + 1'b1;
                    spi_clk_internal <= ~spi_clk_internal;
                    leading_edge     <= 1'b1;
                end
                else
                begin
                    clk_count <= clk_count + 1'b1;
                end

                if (trailing_edge)
                begin
                    // Shift out next nibble onto IO[3:0].
                    q_addr_mode   <= { q_addr_mode[27:0], 4'd0 };
                    q_phase_count <= q_phase_count - 1'b1;
                    if (q_phase_count == 5'd1)
                    begin
                        q_phase_count <= 5'd4;  // 4 dummy SCK cycles
                        state         <= STATE_Q_DUMMY;
                    end
                end
            end

            STATE_Q_DUMMY:
            begin
                if (clk_count == CLKS_PER_HALF_BIT * 2 - 1)
                begin
                    clk_count        <= 0;
                    spi_clk_internal <= ~spi_clk_internal;
                    trailing_edge    <= 1'b1;
                end
                else if (clk_count == CLKS_PER_HALF_BIT - 1)
                begin
                    clk_count        <= clk_count + 1'b1;
                    spi_clk_internal <= ~spi_clk_internal;
                    leading_edge     <= 1'b1;
                end
                else
                begin
                    clk_count <= clk_count + 1'b1;
                end

                if (trailing_edge)
                begin
                    q_phase_count <= q_phase_count - 1'b1;
                    if (q_phase_count == 5'd1)
                    begin
                        q_byte_phase <= 1'b0;
                        q_byte_acc   <= 8'd0;
                        state        <= STATE_Q_DATA;
                    end
                end
            end

            STATE_Q_DATA:
            begin
                if (clk_count == CLKS_PER_HALF_BIT * 2 - 1)
                begin
                    clk_count        <= 0;
                    spi_clk_internal <= ~spi_clk_internal;
                    trailing_edge    <= 1'b1;
                end
                else if (clk_count == CLKS_PER_HALF_BIT - 1)
                begin
                    clk_count        <= clk_count + 1'b1;
                    spi_clk_internal <= ~spi_clk_internal;
                    leading_edge     <= 1'b1;
                end
                else
                begin
                    clk_count <= clk_count + 1'b1;
                end

                // Sample IO[3:0] on the leading edge (rising SCK).
                if (leading_edge)
                begin
                    if (q_byte_phase == 1'b0)
                    begin
                        // High nibble first
                        q_byte_acc[7:4] <= spi_io_in;
                        q_byte_phase    <= 1'b1;
                    end
                    else
                    begin
                        q_byte_acc[3:0] <= spi_io_in;
                        q_byte_phase    <= 1'b0;

                        // Byte complete: push to RX FIFO (or last_rx_byte).
                        last_rx_byte <= { q_byte_acc[7:4], spi_io_in };
                        if (!skip_fifos_latched)
                        begin
                            rx_push      <= 1'b1;
                            rx_push_data <= { q_byte_acc[7:4], spi_io_in };
                        end

                        burst_remaining <= burst_remaining - 1'b1;
                        if (burst_remaining == 16'd1)
                        begin
                            // This was the last byte. Do NOT latch
                            // continuous-mode: M was driven as 0x00 (top
                            // nibble != 0xA), so the flash will require a
                            // fresh opcode 0xEB next burst. Keeping
                            // q_continuous=0 also ensures any subsequent
                            // 1-bit READ_DATA on this controller is not
                            // mis-interpreted as a quad-mode address.
                            q_continuous <= 1'b0;
                            state        <= STATE_DONE;
                        end
                    end
                end
            end

            STATE_DONE:
            begin
                spi_clk_internal <= 1'b0;
                spi_clk          <= 1'b0;
                done             <= 1'b1;
                state            <= STATE_IDLE;
            end

            default:
            begin
                state <= STATE_IDLE;
            end
        endcase

        // ----- TX FIFO state updates -----
        if (muxed_we && !tx_full && !tx_pop)
            tx_count <= tx_count + 1'b1;
        else if (tx_pop && !(muxed_we && !tx_full))
        begin
            tx_count  <= tx_count - 1'b1;
            tx_rd_ptr <= tx_rd_ptr + 1'b1;
        end
        else if (tx_pop && (muxed_we && !tx_full))
        begin
            tx_rd_ptr <= tx_rd_ptr + 1'b1;
        end

        // ----- RX FIFO state updates -----
        if (rx_push && !rx_full_w)
        begin
            rx_mem[rx_wr_ptr] <= rx_push_data;
            rx_wr_ptr         <= rx_wr_ptr + 1'b1;
        end

        if (rx_push && !rx_full_w && !(muxed_re_rx && !rx_empty))
            rx_count <= rx_count + 1'b1;
        else if ((muxed_re_rx && !rx_empty) && !(rx_push && !rx_full_w))
            rx_count <= rx_count - 1'b1;

        // ----- 1-bit bit-level TX/RX (CPHA=0) -----
        // Only meaningful in STATE_TRANSFER, but harmless elsewhere because
        // leading_edge / trailing_edge only fire inside that state for the
        // 1-bit path. The QSPI states drive their own TX/RX above and use
        // their own leading/trailing pulses, which is why we keep the
        // 1-bit shift logic guarded by `state == STATE_TRANSFER`.
        if (state == STATE_TRANSFER)
        begin
            if (trailing_edge)
            begin
                spi_mosi_r <= tx_shift[tx_bit_idx];
                tx_bit_idx <= tx_bit_idx - 1'b1;
            end
            if (leading_edge)
            begin
                rx_shift[rx_bit_idx] <= spi_miso_w;
                rx_bit_idx           <= rx_bit_idx - 1'b1;
            end
        end

        // Delay SPI clock output by one cycle for pin alignment
        spi_clk <= spi_clk_internal;
    end
end

// Initialise FIFO RAMs to zero (for simulation cleanliness)
initial
begin
    for (fi = 0; fi < FIFO_DEPTH; fi = fi + 1)
    begin
        tx_mem[fi] = 8'd0;
        rx_mem[fi] = 8'd0;
    end
end

endmodule

/*
 * QSPIflash
 * ---------
 * Quad-IO-capable SPI flash controller for the FPGC. Drop-in replacement
 * for SimpleSPI2 on the BRFS flash port (SPI1 / `flash2`).
 *
 * This is iteration 1: it preserves the entire SimpleSPI2 control
 * surface (CPU MMIO command port, DMA burst peer port, RX FIFO etc.)
 * and behaves bit-for-bit like SimpleSPI2 in 1-bit SPI Mode 0.  The
 * only externally-visible difference is the pin shape:
 *
 *   SimpleSPI2:   spi_clk, spi_mosi, spi_miso
 *   QSPIflash:    spi_clk,
 *                 spi_io_out[3:0], spi_io_oe[3:0], spi_io_in[3:0]
 *
 * In this iteration the OE pattern is a constant `4'b1101`:
 *   IO0 = MOSI (driven by us)
 *   IO1 = MISO (tri-stated, sampled from spi_io_in[1])
 *   IO2 = WP#  driven high (mimics the previous flash2_wp_n tied to 1)
 *   IO3 = HOLD# driven high (mimics the previous flash2_hold_n tied to 1)
 *
 * The next iteration will add a Quad I/O Fast Read state machine
 * (opcode 0xEB with continuous-mode bits) that flips OE per phase.
 *
 * Chip-select is left to software, exactly like SimpleSPI2.
 */
module QSPIflash #(
    parameter CLKS_PER_HALF_BIT = 2,
    parameter FIFO_DEPTH        = 16
) (
    // ---- System ----
    input  wire        clk,
    input  wire        reset,

    // ---- Command / data (CPU MMIO master, via MemoryUnit) ----
    input  wire        cmd_we,          // pulse: push cmd_data into TX FIFO
    input  wire [7:0]  cmd_data,
    input  wire        cmd_start_burst, // pulse: begin burst of cmd_burst_len bytes
    input  wire [15:0] cmd_burst_len,
    input  wire        cmd_dummy,       // latched at start: send 0x00 instead of TX FIFO
    input  wire        cmd_skip_fifos,  // latched at start: bypass both FIFOs
                                        // (TX from cmd_data, RX into last_rx_byte only)

    output wire        tx_full,
    output wire        rx_empty,
    output wire [7:0]  rx_data,
    input  wire        cmd_re_rx,       // pulse: pop one byte from RX FIFO
    output reg  [7:0]  last_rx_byte = 8'd0, // persistent most-recent received byte

    output wire        busy,
    output reg         done = 1'b0,     // 1-cycle pulse on burst completion

    // ---- DMA engine master (direct, bypasses MemoryUnit) ----
    input  wire        dma_select,
    input  wire        dma_we,
    input  wire [7:0]  dma_data,
    input  wire        dma_start_burst,
    input  wire [15:0] dma_burst_len,
    input  wire        dma_dummy,
    input  wire        dma_re_rx,

    // ---- Pins (bidirectional quad bus, tristated externally) ----
    output reg         spi_clk    = 1'b0,
    output wire [3:0]  spi_io_out,
    output wire [3:0]  spi_io_oe,
    input  wire [3:0]  spi_io_in
);

// ============================================================================
// FIFO infrastructure (TX and RX, identical structure)
// ============================================================================
localparam FIFO_AW = (FIFO_DEPTH > 1) ? $clog2(FIFO_DEPTH) : 1;

// ----- Command-port master mux (CPU MMIO vs DMA engine) -----
wire        muxed_we          = dma_select ? dma_we          : cmd_we;
wire [7:0]  muxed_data        = dma_select ? dma_data        : cmd_data;
wire        muxed_start_burst = dma_select ? dma_start_burst : cmd_start_burst;
wire [15:0] muxed_burst_len   = dma_select ? dma_burst_len   : cmd_burst_len;
wire        muxed_dummy       = dma_select ? dma_dummy       : cmd_dummy;
wire        muxed_skip_fifos  = dma_select ? 1'b0            : cmd_skip_fifos;
wire        muxed_re_rx       = dma_select ? dma_re_rx       : cmd_re_rx;

// --- TX FIFO ---
reg [7:0]         tx_mem [0:FIFO_DEPTH-1];
reg [FIFO_AW:0]   tx_count = 0;
reg [FIFO_AW-1:0] tx_rd_ptr = 0;
reg [FIFO_AW-1:0] tx_wr_ptr = 0;
wire              tx_empty_w = (tx_count == 0);
assign            tx_full    = (tx_count == FIFO_DEPTH);

// --- RX FIFO ---
reg [7:0]         rx_mem [0:FIFO_DEPTH-1];
reg [FIFO_AW:0]   rx_count = 0;
reg [FIFO_AW-1:0] rx_rd_ptr = 0;
reg [FIFO_AW-1:0] rx_wr_ptr = 0;
assign            rx_empty   = (rx_count == 0);
wire              rx_full_w  = (rx_count == FIFO_DEPTH);
assign            rx_data    = rx_mem[rx_rd_ptr];

reg               tx_pop  = 1'b0;
reg               rx_push = 1'b0;
reg  [7:0]        rx_push_data = 8'd0;

integer fi;

// ============================================================================
// Bit-level shift state machine (mirrors SimpleSPI2 / SimpleSPI structure)
// ============================================================================
localparam
    STATE_IDLE      = 2'd0,
    STATE_LOAD_BYTE = 2'd1,
    STATE_TRANSFER  = 2'd2,
    STATE_DONE      = 2'd3;

reg [1:0] state = STATE_IDLE;

localparam CLK_COUNT_WIDTH = (CLKS_PER_HALF_BIT > 1) ? $clog2(CLKS_PER_HALF_BIT*2) : 1;
reg [CLK_COUNT_WIDTH-1:0] clk_count = 0;

reg [4:0]  edge_count = 5'd0; // 16 SPI clock edges per byte
reg        spi_clk_internal = 1'b0;

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

reg        spi_mosi_r = 1'b0;     // MOSI bit, driven onto IO0

assign busy = (state != STATE_IDLE);

// ----- Pin direction (iteration 1: constant 1-bit SPI mode) -----
//   IO0 = MOSI       (output, OE=1)
//   IO1 = MISO       (input,  OE=0)
//   IO2 = WP#  high  (output, OE=1)
//   IO3 = HOLD# high (output, OE=1)
assign spi_io_oe  = 4'b1101;
assign spi_io_out = { 1'b1,         // IO3 = HOLD# = 1
                      1'b1,         // IO2 = WP#   = 1
                      1'b0,         // IO1 = MISO (don't-care, OE=0)
                      spi_mosi_r }; // IO0 = MOSI

wire spi_miso_w = spi_io_in[1];

always @(posedge clk)
begin
    if (reset)
    begin
        // FIFO pointers / counters
        tx_count  <= 0;
        tx_rd_ptr <= 0;
        tx_wr_ptr <= 0;
        rx_count  <= 0;
        rx_rd_ptr <= 0;
        rx_wr_ptr <= 0;

        // Shift engine
        state            <= STATE_IDLE;
        clk_count        <= 0;
        edge_count       <= 5'd0;
        spi_clk_internal <= 1'b0;
        spi_clk          <= 1'b0;
        spi_mosi_r       <= 1'b0;
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
    end
    else
    begin
        // Defaults (single-cycle pulses)
        done          <= 1'b0;
        leading_edge  <= 1'b0;
        trailing_edge <= 1'b0;
        tx_pop        <= 1'b0;
        rx_push       <= 1'b0;

        // ----- TX FIFO write port (host-side push) -----
        if (muxed_we && !tx_full)
        begin
            tx_mem[tx_wr_ptr] <= muxed_data;
            tx_wr_ptr <= tx_wr_ptr + 1'b1;
        end

        // ----- RX FIFO read port (host-side pop) -----
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
                tx_bit_idx       <= 3'd7;
                rx_bit_idx       <= 3'd7;
                edge_count       <= 5'd0;

                if (muxed_start_burst && muxed_burst_len != 16'd0)
                begin
                    burst_remaining    <= muxed_burst_len;
                    dummy_latched      <= muxed_dummy;
                    skip_fifos_latched <= muxed_skip_fifos;
                    skip_data_latched  <= muxed_data;
                    state              <= STATE_LOAD_BYTE;
                end
            end

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
                else
                begin
                    tx_shift   <= tx_shift;
                    spi_mosi_r <= spi_mosi_r;
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

        // ----- TX FIFO state updates from this cycle's events -----
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

        // ----- Bit-level TX/RX (CPHA=0) -----
        // TX: shift out on trailing edge (data changes on falling edge for Mode 0)
        if (trailing_edge)
        begin
            spi_mosi_r <= tx_shift[tx_bit_idx];
            tx_bit_idx <= tx_bit_idx - 1'b1;
        end

        // RX: sample on leading edge (rising edge for Mode 0)
        if (leading_edge)
        begin
            rx_shift[rx_bit_idx] <= spi_miso_w;
            rx_bit_idx           <= rx_bit_idx - 1'b1;
        end

        // Delay SPI clock output by one cycle for pin alignment
        spi_clk <= spi_clk_internal;
    end
end

// Initialise FIFO RAMs to zero (for simulation cleanliness; iverilog only)
initial
begin
    for (fi = 0; fi < FIFO_DEPTH; fi = fi + 1)
    begin
        tx_mem[fi] = 8'd0;
        rx_mem[fi] = 8'd0;
    end
end

endmodule

/*
 * SimpleSPI2
 * ----------
 * SPI master (Mode 0: CPOL=0, CPHA=0) with TX/RX FIFOs and burst command
 * support. Designed for use as the SPI back-end for the DMA engine and for
 * software bursts longer than one byte (Flash, ENC28J60).
 *
 * Burst protocol:
 *   1. Software (or DMA) pushes bytes into the TX FIFO via cmd_we/cmd_data.
 *      For a read-only burst, software may skip filling the TX FIFO and
 *      assert cmd_dummy together with cmd_start_burst; the controller will
 *      send 0x00 for every byte instead.
 *   2. Software writes cmd_burst_len, then pulses cmd_start_burst.
 *   3. The controller shifts out cmd_burst_len bytes (popping the TX FIFO
 *      one byte at a time, or sending 0x00 if cmd_dummy was set), and
 *      captures every received byte into the RX FIFO.
 *   4. When the burst finishes, `done` pulses for one cycle and `busy`
 *      drops. Software can pop the RX FIFO with cmd_re_rx/rx_data.
 *
 * Single-byte fast-path (`cmd_skip_fifos`):
 *   When `cmd_skip_fifos` is asserted together with `cmd_start_burst`, the
 *   controller bypasses both FIFOs for the duration of that burst. The
 *   outgoing byte is taken directly from `cmd_data` (sampled in the same
 *   cycle as `cmd_start_burst`), and the captured byte is written only to
 *   `last_rx_byte` -- the RX FIFO is left untouched. This preserves the
 *   semantics of the original SimpleSPI module: `last_rx_byte` is a
 *   persistent register that holds the most recently received byte until
 *   the next transfer overwrites it, regardless of how many times software
 *   reads it. Intended for the CPU's single-byte SPI MMIO path; DMA
 *   bursts keep using the FIFO interface and `cmd_re_rx`/`rx_data`.
 *
 * Chip-select is left to software (write ADDR_SPIx_CS first, run the
 * burst, then deassert CS). This lets multiple bursts run inside one CS
 * hold (e.g. command + address + payload for Flash).
 *
 * SPI_CLK_FREQ = clk_freq / (2 * CLKS_PER_HALF_BIT)
 *   100 MHz / (2 * 2) = 25 MHz with CLKS_PER_HALF_BIT=2.
 */
module SimpleSPI2 #(
    parameter CLKS_PER_HALF_BIT = 2,
    parameter FIFO_DEPTH        = 16
) (
    // ---- System ----
    input  wire        clk,
    input  wire        reset,

    // ---- Command / data ----
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

    // ---- Pins ----
    output reg         spi_clk  = 1'b0,
    output reg         spi_mosi = 1'b0,
    input  wire        spi_miso
);

// ============================================================================
// FIFO infrastructure (TX and RX, identical structure)
// ============================================================================
localparam FIFO_AW = (FIFO_DEPTH > 1) ? $clog2(FIFO_DEPTH) : 1;

// --- TX FIFO ---
reg [7:0]         tx_mem [0:FIFO_DEPTH-1];
reg [FIFO_AW:0]   tx_count = 0;   // one extra bit so full/empty are unambiguous
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

// FIFO write/read enables resolved per-cycle below
reg               tx_pop  = 1'b0;
reg               rx_push = 1'b0;
reg  [7:0]        rx_push_data = 8'd0;

integer fi;

// ============================================================================
// Bit-level shift state machine (mirrors SimpleSPI.v structure)
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

assign busy = (state != STATE_IDLE);

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
        spi_mosi         <= 1'b0;
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
        if (cmd_we && !tx_full)
        begin
            tx_mem[tx_wr_ptr] <= cmd_data;
            tx_wr_ptr <= tx_wr_ptr + 1'b1;
        end

        // ----- RX FIFO read port (host-side pop) -----
        if (cmd_re_rx && !rx_empty)
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

                if (cmd_start_burst && cmd_burst_len != 16'd0)
                begin
                    burst_remaining    <= cmd_burst_len;
                    dummy_latched      <= cmd_dummy;
                    skip_fifos_latched <= cmd_skip_fifos;
                    skip_data_latched  <= cmd_data;
                    state              <= STATE_LOAD_BYTE;
                end
            end

            STATE_LOAD_BYTE:
            begin
                // Pick up next outgoing byte. In skip-FIFO mode the byte was
                // latched from cmd_data in IDLE; in dummy mode we send 0x00;
                // otherwise pop the TX FIFO front.
                if (skip_fifos_latched)
                begin
                    tx_shift   <= skip_data_latched;
                    spi_mosi   <= skip_data_latched[7];
                end
                else if (dummy_latched)
                begin
                    tx_shift   <= 8'd0;
                    spi_mosi   <= 1'b0;
                end
                else if (!tx_empty_w)
                begin
                    tx_shift   <= tx_mem[tx_rd_ptr];
                    spi_mosi   <= tx_mem[tx_rd_ptr][7];
                    tx_pop     <= 1'b1; // consume one byte
                end
                else
                begin
                    // TX FIFO underrun -- stall here until software supplies a byte.
                    // (Should not happen if software fills TX before starting.)
                    tx_shift   <= tx_shift; // no-op, hold
                    spi_mosi   <= spi_mosi;
                end

                // Only advance to TRANSFER if we got a byte (or are in dummy / skip mode)
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
                    // Always latch the captured byte into last_rx_byte so the
                    // single-byte CPU MMIO path has a persistent register to read.
                    last_rx_byte <= rx_shift;

                    // Push into RX FIFO unless this burst is in skip-FIFO mode.
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
        // tx_pop fires in STATE_LOAD_BYTE when we consume one byte
        if (cmd_we && !tx_full && !tx_pop)
            tx_count <= tx_count + 1'b1;
        else if (tx_pop && !(cmd_we && !tx_full))
        begin
            tx_count  <= tx_count - 1'b1;
            tx_rd_ptr <= tx_rd_ptr + 1'b1;
        end
        else if (tx_pop && (cmd_we && !tx_full))
        begin
            // Simultaneous push and pop: count unchanged
            tx_rd_ptr <= tx_rd_ptr + 1'b1;
        end

        // ----- RX FIFO state updates -----
        if (rx_push && !rx_full_w)
        begin
            rx_mem[rx_wr_ptr] <= rx_push_data;
            rx_wr_ptr         <= rx_wr_ptr + 1'b1;
        end

        if (rx_push && !rx_full_w && !(cmd_re_rx && !rx_empty))
            rx_count <= rx_count + 1'b1;
        else if ((cmd_re_rx && !rx_empty) && !(rx_push && !rx_full_w))
            rx_count <= rx_count - 1'b1;
        // simultaneous push+pop: count unchanged

        // ----- Bit-level TX/RX (CPHA=0) -----
        // TX: shift out on trailing edge (data changes on falling edge for Mode 0)
        if (trailing_edge)
        begin
            spi_mosi   <= tx_shift[tx_bit_idx];
            tx_bit_idx <= tx_bit_idx - 1'b1;
        end

        // RX: sample on leading edge (rising edge for Mode 0)
        if (leading_edge)
        begin
            rx_shift[rx_bit_idx] <= spi_miso;
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

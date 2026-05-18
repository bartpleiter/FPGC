/*
 * FrameScanEngine
 * Scans the 320×240 pixel framebuffer and streams pixels over SPI
 *
 * Continuously streams pixel data after init sends RAMWR.
 * The ILI9341 address counter auto-wraps at the window boundary
 * (set once during init), so no per-frame CASET/PASET/RAMWR is needed.
 *
 * Pixel pipeline:
 *   SRAM read (1 cycle) → wait for SRAM settle (8 cycles) →
 *   palette lookup (1 cycle) → RGB565 convert →
 *   SPI byte 1 (high) → SPI byte 2 (low)
 *
 * The 8-cycle wait in S_PIXEL_PAL covers the worst case where a write
 * races with S_PIXEL_READ: 3 cycles write completion + 2 cycles bus
 * settling + 2 cycles for SRAM access time and PCB routing delays.
 *
 * Clocked at 100 MHz.
 */
module FrameScanEngine (
    input  wire        clk,        // 100 MHz
    input  wire        reset,
    input  wire        enable,     // Start scanning (asserted after init)

    // SRAM read interface
    output reg  [16:0] sram_addr,
    input  wire [7:0]  sram_data,  // Pixel data from SRAM arbiter
    input  wire        sram_data_valid, // HIGH when sram_data is correct for sram_addr
    output reg         sram_read,  // Asserted when reading from SRAM

    // Palette read interface
    output reg  [7:0]  palette_idx,
    input  wire [23:0] palette_rgb, // Valid 1 cycle after idx presented

    // SPI master interface
    output reg  [7:0]  spi_tx_data,
    output reg         spi_tx_valid,
    input  wire        spi_tx_ready,
    output reg         spi_dc,     // 0=command, 1=data

    // SPI CS control
    output reg         spi_cs_n,

    // Status
    output reg         frame_done  // Pulse at end of each frame
);

    // ---- Pixel coordinate counters ----
    reg [8:0] pixel_x = 9'd0;  // 0–319
    reg [7:0] pixel_y = 8'd0;  // 0–239

    // ---- RGB565 conversion (combinational from palette output) ----
    wire [7:0] pixel_hi = {palette_rgb[23:19], palette_rgb[15:13]};
    wire [7:0] pixel_lo = {palette_rgb[12:10], palette_rgb[7:3]};

    // ---- Main state machine ----
    //
    // After every tx_valid assertion, the FSM transitions to S_WAIT_SPI
    // which waits for the SPIMaster to accept the byte (tx_ready drops)
    // and then finish it (tx_ready rises again), avoiding a 1-cycle race
    // where tx_ready is still high from the previous IDLE.
    localparam
        S_IDLE         = 3'd0,
        // Pixel streaming
        S_PIXEL_READ   = 3'd1,  // Issue SRAM read
        S_PIXEL_PAL    = 3'd2,  // Wait for palette data
        S_PIXEL_HI     = 3'd3,  // Send RGB565 high byte
        S_PIXEL_LO     = 3'd4,  // Send RGB565 low byte
        S_WAIT_SPI     = 3'd5;  // Wait for SPI byte to complete

    reg [2:0] state = S_IDLE;
    reg [2:0] return_state = S_IDLE; // Where to go after SPI byte done
    reg       spi_accepted = 1'b0;  // Tracks that tx_ready went low
    reg [4:0] pal_wait = 5'd0;     // Wait counter for SRAM settle

    always @(posedge clk) begin
        if (reset) begin
            state <= S_IDLE;
            return_state <= S_IDLE;
            spi_accepted <= 1'b0;
            spi_tx_valid <= 1'b0;
            spi_tx_data <= 8'd0;
            spi_dc <= 1'b1;
            spi_cs_n <= 1'b0;
            sram_read <= 1'b0;
            sram_addr <= 17'd0;
            palette_idx <= 8'd0;
            pixel_x <= 9'd0;
            pixel_y <= 8'd0;
            frame_done <= 1'b0;
        end else begin
            // Defaults
            spi_tx_valid <= 1'b0;
            sram_read <= 1'b0;
            frame_done <= 1'b0;

            case (state)
                S_IDLE: begin
                    // CS stays low (init left it asserted)
                    spi_cs_n <= 1'b0;
                    spi_dc <= 1'b1; // All data from here on
                    if (enable) begin
                        pixel_x <= 9'd1; // Compensate for ILI9341 2-pixel column offset
                        pixel_y <= 8'd0;
                        state <= S_PIXEL_READ;
                    end
                end

                // ---- Pixel streaming ----
                S_PIXEL_READ: begin
                    // Present SRAM address and request read
                    // Address = y * 320 + x = (y << 8) + (y << 6) + x
                    sram_addr <= ({9'd0, pixel_y} << 8)
                               + ({9'd0, pixel_y} << 6)
                               + {8'd0, pixel_x};
                    sram_read <= 1'b1;
                    pal_wait <= 5'd0;
                    state <= S_PIXEL_PAL;
                end

                S_PIXEL_PAL: begin
                    // Keep blocking new writes while waiting for data
                    sram_read <= 1'b1;
                    // Wait 8 cycles for SRAM data to settle after write→read transition
                    if (pal_wait == 5'd8) begin
                        palette_idx <= sram_data;
                        state <= S_PIXEL_HI;
                    end else begin
                        pal_wait <= pal_wait + 5'd1;
                    end
                end

                S_PIXEL_HI: begin
                    // Palette RGB is now valid, send high byte of RGB565
                    if (spi_tx_ready) begin
                        spi_tx_data <= pixel_hi;
                        spi_dc <= 1'b1; // Data
                        spi_tx_valid <= 1'b1;
                        return_state <= S_PIXEL_LO;
                        spi_accepted <= 1'b0;
                        state <= S_WAIT_SPI;
                    end
                end

                S_PIXEL_LO: begin
                    // Send low byte of RGB565
                    if (spi_tx_ready) begin
                        spi_tx_data <= pixel_lo;
                        spi_dc <= 1'b1; // Data
                        spi_tx_valid <= 1'b1;
                        spi_accepted <= 1'b0;

                        // Advance to next pixel
                        if (pixel_x == 9'd319) begin
                            pixel_x <= 9'd0;
                            if (pixel_y == 8'd239) begin
                                // Frame complete — wrap to (0,0)
                                // ILI9341 auto-wraps, just keep streaming
                                pixel_y <= 8'd0;
                                frame_done <= 1'b1;
                            end else begin
                                pixel_y <= pixel_y + 8'd1;
                            end
                        end else begin
                            pixel_x <= pixel_x + 9'd1;
                        end
                        return_state <= S_PIXEL_READ;
                        state <= S_WAIT_SPI;
                    end
                end

                // ---- Wait for SPI byte to complete ----
                // First wait for tx_ready to drop (byte accepted),
                // then wait for it to rise (byte transmitted).
                S_WAIT_SPI: begin
                    if (!spi_accepted) begin
                        if (!spi_tx_ready)
                            spi_accepted <= 1'b1;
                    end else begin
                        if (spi_tx_ready)
                            state <= return_state;
                    end
                end

                default: state <= S_IDLE;
            endcase
        end
    end

endmodule

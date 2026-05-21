/*
 * FrameScanEngine
 * Scans the 320×240 pixel framebuffer and streams pixels over SPI
 *
 * For each frame:
 *   1. Send CASET command (column address: 0–319)
 *   2. Send PASET command (page address: 0–239)
 *   3. Send RAMWR command
 *   4. Stream 76,800 pixels as RGB565 (2 bytes each, 153,600 bytes total)
 *   5. Pulse frame_done, return to step 1
 *
 * Pixel pipeline:
 *   SRAM read (1 cycle) → palette lookup (1 cycle) → RGB565 convert
 *   → SPI byte 1 (high) → SPI byte 2 (low)
 *
 * Since SPI at 50 MHz takes 16 system clocks per byte, we have
 * plenty of time to pipeline the SRAM read and palette lookup.
 *
 * Clocked at 100 MHz.
 */
module FrameScanEngine (
    input  wire        clk,        // 100 MHz
    input  wire        reset,
    input  wire        enable,     // Start scanning (asserted after init)

    // SRAM read interface
    output reg  [16:0] sram_addr,
    input  wire [7:0]  sram_data,  // Valid 1 cycle after addr presented
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
    wire [7:0] rgb565_hi = {palette_rgb[23:19], palette_rgb[15:13]};
    wire [7:0] rgb565_lo = {palette_rgb[12:10], palette_rgb[7:3]};

    // ---- Main state machine ----
    //
    // After every tx_valid assertion, the FSM transitions to S_WAIT_SPI
    // which waits for the SPIMaster to accept the byte (tx_ready drops)
    // and then finish it (tx_ready rises again), avoiding a 1-cycle race
    // where tx_ready is still high from the previous IDLE.
    localparam
        S_IDLE         = 4'd0,
        S_CS_ASSERT    = 4'd1,
        // CASET (0x2A): cmd + 4 data bytes
        S_CASET_CMD    = 4'd2,
        S_CASET_DATA   = 4'd3,
        // PASET (0x2B): cmd + 4 data bytes
        S_PASET_CMD    = 4'd4,
        S_PASET_DATA   = 4'd5,
        // RAMWR (0x2C): cmd then pixel data
        S_RAMWR_CMD    = 4'd6,
        // Pixel streaming
        S_PIXEL_READ   = 4'd7,  // Issue SRAM read
        S_PIXEL_PAL    = 4'd8,  // Wait for palette data
        S_PIXEL_HI     = 4'd9,  // Send RGB565 high byte
        S_PIXEL_LO     = 4'd10, // Send RGB565 low byte
        S_FRAME_DONE   = 4'd11,
        S_WAIT_SPI     = 4'd12; // Wait for SPI byte to complete

    reg [3:0] state = S_IDLE;
    reg [3:0] return_state = S_IDLE; // Where to go after SPI byte done
    reg       spi_accepted = 1'b0;  // Tracks that tx_ready went low
    reg [2:0] byte_idx = 3'd0;      // Sub-index for multi-byte commands

    // CASET/PASET data bytes (landscape 320×240)
    // CASET: start_col=0x0000, end_col=0x013F (319)
    // PASET: start_row=0x0000, end_row=0x00EF (239)
    reg [7:0] caset_data [0:3];
    reg [7:0] paset_data [0:3];

    initial begin
        caset_data[0] = 8'h00; // Start column high
        caset_data[1] = 8'h00; // Start column low
        caset_data[2] = 8'h01; // End column high (319 = 0x013F)
        caset_data[3] = 8'h3F; // End column low

        paset_data[0] = 8'h00; // Start row high
        paset_data[1] = 8'h00; // Start row low
        paset_data[2] = 8'h00; // End row high (239 = 0x00EF)
        paset_data[3] = 8'hEF; // End row low
    end

    always @(posedge clk) begin
        if (reset) begin
            state <= S_IDLE;
            return_state <= S_IDLE;
            spi_accepted <= 1'b0;
            spi_tx_valid <= 1'b0;
            spi_tx_data <= 8'd0;
            spi_dc <= 1'b0;
            spi_cs_n <= 1'b1;
            sram_read <= 1'b0;
            sram_addr <= 17'd0;
            palette_idx <= 8'd0;
            pixel_x <= 9'd0;
            pixel_y <= 8'd0;
            byte_idx <= 3'd0;
            frame_done <= 1'b0;
        end else begin
            // Defaults
            spi_tx_valid <= 1'b0;
            sram_read <= 1'b0;
            frame_done <= 1'b0;

            case (state)
                S_IDLE: begin
                    spi_cs_n <= 1'b1;
                    if (enable) begin
                        state <= S_CS_ASSERT;
                    end
                end

                S_CS_ASSERT: begin
                    spi_cs_n <= 1'b0;
                    pixel_x <= 9'd0;
                    pixel_y <= 8'd0;
                    state <= S_CASET_CMD;
                end

                // ---- CASET command ----
                S_CASET_CMD: begin
                    if (spi_tx_ready) begin
                        spi_tx_data <= 8'h2A; // CASET
                        spi_dc <= 1'b0;       // Command
                        spi_tx_valid <= 1'b1;
                        byte_idx <= 3'd0;
                        return_state <= S_CASET_DATA;
                        spi_accepted <= 1'b0;
                        state <= S_WAIT_SPI;
                    end
                end

                S_CASET_DATA: begin
                    if (spi_tx_ready) begin
                        spi_tx_data <= caset_data[byte_idx];
                        spi_dc <= 1'b1; // Data
                        spi_tx_valid <= 1'b1;
                        if (byte_idx == 3'd3) begin
                            return_state <= S_PASET_CMD;
                        end else begin
                            byte_idx <= byte_idx + 3'd1;
                            return_state <= S_CASET_DATA;
                        end
                        spi_accepted <= 1'b0;
                        state <= S_WAIT_SPI;
                    end
                end

                // ---- PASET command ----
                S_PASET_CMD: begin
                    if (spi_tx_ready) begin
                        spi_tx_data <= 8'h2B; // PASET
                        spi_dc <= 1'b0;       // Command
                        spi_tx_valid <= 1'b1;
                        byte_idx <= 3'd0;
                        return_state <= S_PASET_DATA;
                        spi_accepted <= 1'b0;
                        state <= S_WAIT_SPI;
                    end
                end

                S_PASET_DATA: begin
                    if (spi_tx_ready) begin
                        spi_tx_data <= paset_data[byte_idx];
                        spi_dc <= 1'b1; // Data
                        spi_tx_valid <= 1'b1;
                        if (byte_idx == 3'd3) begin
                            return_state <= S_RAMWR_CMD;
                        end else begin
                            byte_idx <= byte_idx + 3'd1;
                            return_state <= S_PASET_DATA;
                        end
                        spi_accepted <= 1'b0;
                        state <= S_WAIT_SPI;
                    end
                end

                // ---- RAMWR command ----
                S_RAMWR_CMD: begin
                    if (spi_tx_ready) begin
                        spi_tx_data <= 8'h2C; // RAMWR
                        spi_dc <= 1'b0;       // Command
                        spi_tx_valid <= 1'b1;
                        return_state <= S_PIXEL_READ;
                        spi_accepted <= 1'b0;
                        state <= S_WAIT_SPI;
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
                    state <= S_PIXEL_PAL;
                end

                S_PIXEL_PAL: begin
                    // SRAM data is now valid, feed to palette
                    palette_idx <= sram_data;
                    state <= S_PIXEL_HI;
                end

                S_PIXEL_HI: begin
                    // Palette RGB is now valid, send high byte of RGB565
                    if (spi_tx_ready) begin
                        spi_tx_data <= rgb565_hi;
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
                        spi_tx_data <= rgb565_lo;
                        spi_dc <= 1'b1; // Data
                        spi_tx_valid <= 1'b1;
                        spi_accepted <= 1'b0;

                        // Advance to next pixel
                        if (pixel_x == 9'd319) begin
                            pixel_x <= 9'd0;
                            if (pixel_y == 8'd239) begin
                                // Frame complete
                                return_state <= S_FRAME_DONE;
                            end else begin
                                pixel_y <= pixel_y + 8'd1;
                                return_state <= S_PIXEL_READ;
                            end
                        end else begin
                            pixel_x <= pixel_x + 9'd1;
                            return_state <= S_PIXEL_READ;
                        end
                        state <= S_WAIT_SPI;
                    end
                end

                S_FRAME_DONE: begin
                    frame_done <= 1'b1;
                    // Start next frame immediately
                    pixel_x <= 9'd0;
                    pixel_y <= 8'd0;
                    state <= S_CASET_CMD;
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

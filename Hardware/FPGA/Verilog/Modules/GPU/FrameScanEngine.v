/*
 * FrameScanEngine
 * Scans the 320×240 pixel framebuffer and streams pixels over SPI
 * with window tile layer compositing.
 *
 * Continuously streams pixel data after init sends RAMWR.
 * The ILI9341 address counter auto-wraps at the window boundary
 * (set once during init), so no per-frame CASET/PASET/RAMWR is needed.
 *
 * Pixel pipeline:
 *   SRAM read (1 cycle) → wait for SRAM settle + tile fetch (8 cycles) →
 *   composite + palette lookup (1 cycle) → RGB565 convert →
 *   SPI byte 1 (high) → SPI byte 2 (low)
 *
 * During the 8-cycle wait, the engine fetches window tile data from
 * VRAM8 (tile/color maps) and VRAM32 (patterns/palettes) to composite
 * a text overlay on top of the pixel framebuffer. Transparent window
 * tiles (pattern==0 and palette color 0==0) show the pixel layer through.
 *
 * Clocked at 100 MHz.
 */
module FrameScanEngine (
    input  wire        clk,        // 100 MHz
    input  wire        reset,
    input  wire        enable,     // Start scanning (asserted after init)

    // SRAM read interface (pixel framebuffer)
    output reg  [16:0] sram_addr,
    input  wire [7:0]  sram_data,  // Pixel data from SRAM arbiter
    input  wire        sram_data_valid, // HIGH when sram_data is correct for sram_addr
    output reg         sram_read,  // Asserted when reading from SRAM

    // VRAM8 read interface (tile/color maps)
    output reg  [13:0] vram8_addr,
    input  wire [7:0]  vram8_q,

    // VRAM32 read interface (patterns/palettes)
    output reg  [10:0] vram32_addr,
    input  wire [31:0] vram32_q,

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

    // ---- Window tile coordinate derivation ----
    wire [5:0] tile_x = pixel_x[8:3];      // pixel_x / 8 (0-39)
    wire [4:0] tile_y = pixel_y[7:3];      // pixel_y / 8 (0-29)
    wire [2:0] col_in_tile = pixel_x[2:0]; // pixel_x % 8
    wire [2:0] line_in_tile = pixel_y[2:0]; // pixel_y % 8

    // Window tile address: tile_y * 40 + tile_x
    // 40 = 32 + 8, so tile_y * 40 = (tile_y << 5) + (tile_y << 3)
    wire [10:0] win_tile_linear = ({6'd0, tile_y} << 5) + ({6'd0, tile_y} << 3) + {5'd0, tile_x};

    // ---- Window tile layer state ----
    reg [7:0]  win_tile_index    = 8'd0;   // Tile index from VRAM8
    reg [7:0]  win_color_index   = 8'd0;   // Color/palette index from VRAM8
    reg [15:0] win_pattern_half  = 16'd0;  // 16-bit pattern (1 row of 8 pixels × 2 bits)
    reg [31:0] win_palette_word  = 32'd0;  // 4-color palette (4 × 8-bit RRRGGGBB)
    reg        win_transparent   = 1'b1;   // Window pixel is transparent
    reg [23:0] win_rgb24         = 24'd0;  // Window pixel RGB24 (from tile palette)

    // ---- Combinational pattern extraction ----
    // Pattern layout: pixel 0 at [15:14], pixel 7 at [1:0]
    // For col_in_tile=0 we want bits [15:14], for col_in_tile=7 we want [1:0]
    // shift_amount = (7 - col_in_tile) * 2
    wire [3:0] pattern_shift = ({1'b0, (3'd7 - col_in_tile)} << 1);
    wire [1:0] pattern_bits  = win_pattern_half[pattern_shift +: 2];

    // ---- Combinational palette color extraction ----
    // Palette word: color 0 at [31:24], color 1 at [23:16], etc.
    // Each color is 8-bit RRRGGGBB
    reg [7:0] win_color_byte;
    always @(*) begin
        case (pattern_bits)
            2'b00: win_color_byte = win_palette_word[31:24];
            2'b01: win_color_byte = win_palette_word[23:16];
            2'b10: win_color_byte = win_palette_word[15:8];
            2'b11: win_color_byte = win_palette_word[7:0];
        endcase
    end

    // Expand RRRGGGBB to RGB24 (replicate MSBs into lower bits)
    wire [23:0] win_color_rgb24 = {
        win_color_byte[7:5], win_color_byte[7:5], win_color_byte[7:6],  // R: 3→8 bits
        win_color_byte[4:2], win_color_byte[4:2], win_color_byte[4:3],  // G: 3→8 bits
        win_color_byte[1:0], win_color_byte[1:0], win_color_byte[1:0], win_color_byte[1:0]  // B: 2→8 bits
    };

    // ---- Composited RGB565 (muxes pixel palette RGB or window tile RGB) ----
    wire [23:0] final_rgb24     = win_transparent ? palette_rgb : win_rgb24;
    wire [7:0]  pixel_hi        = {final_rgb24[23:19], final_rgb24[15:13]};
    wire [7:0]  pixel_lo        = {final_rgb24[12:10], final_rgb24[7:3]};

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
        S_PIXEL_PAL    = 3'd2,  // Wait for SRAM settle + fetch window tile data
        S_PIXEL_HI     = 3'd3,  // Send RGB565 high byte
        S_PIXEL_LO     = 3'd4,  // Send RGB565 low byte
        S_WAIT_SPI     = 3'd5;  // Wait for SPI byte to complete

    reg [2:0] state = S_IDLE;
    reg [2:0] return_state = S_IDLE; // Where to go after SPI byte done
    reg       spi_accepted = 1'b0;  // Tracks that tx_ready went low
    reg [3:0] pal_wait = 4'd0;      // Wait counter for SRAM settle + tile fetch

    // ---- Latched SRAM pixel data ----
    // (reserved for future use; palette_idx captures sram_data directly)

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
            vram8_addr <= 14'd0;
            vram32_addr <= 11'd0;
            palette_idx <= 8'd0;
            pixel_x <= 9'd0;
            pixel_y <= 8'd0;
            frame_done <= 1'b0;
            win_tile_index <= 8'd0;
            win_color_index <= 8'd0;
            win_pattern_half <= 16'd0;
            win_palette_word <= 32'd0;
            win_transparent <= 1'b1;
            win_rgb24 <= 24'd0;
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
                    pal_wait <= 4'd0;
                    state <= S_PIXEL_PAL;
                end

                S_PIXEL_PAL: begin
                    // Keep blocking new writes while waiting for data
                    sram_read <= 1'b1;

                    // 10-cycle window: SRAM settle (cycles 0-7) + tile fetch (cycles 0-7 overlapped)
                    //
                    // Cycle 0: Present VRAM8 tile index address
                    // Cycle 1: Latch VRAM8 tile index; present VRAM32 pattern address
                    // Cycle 2: Latch VRAM32 pattern; present VRAM8 color index address
                    // Cycle 3: Latch VRAM8 color index; present VRAM32 palette address
                    // Cycle 4: Latch VRAM32 palette
                    // Cycle 5: Extract pattern bits, determine transparency
                    // Cycle 6: Compute window pixel RGB24
                    // Cycle 7: Latch SRAM data + set palette_idx
                    // Cycle 8: Palette lookup latency (palette_rgb valid next cycle)
                    // Cycle 9: Done — composited pixel_hi/pixel_lo ready

                    case (pal_wait)
                        4'd0: begin
                            // Request window tile index from VRAM8
                            vram8_addr <= 14'd4096 + {3'd0, win_tile_linear};
                        end

                        4'd1: begin
                            // Latch tile index from VRAM8
                            win_tile_index <= vram8_q;
                            // Request pattern data from VRAM32
                            // Address = tile_index * 4 + line_in_tile / 2
                            vram32_addr <= ({3'd0, vram8_q} << 2) + {8'd0, line_in_tile[2:1]};
                        end

                        4'd2: begin
                            // Latch pattern data from VRAM32 (select correct 16-bit half)
                            if (line_in_tile[0])
                                win_pattern_half <= vram32_q[15:0];
                            else
                                win_pattern_half <= vram32_q[31:16];
                            // Request color index from VRAM8
                            vram8_addr <= 14'd6144 + {3'd0, win_tile_linear};
                        end

                        4'd3: begin
                            // Latch color index from VRAM8
                            win_color_index <= vram8_q;
                            // Request palette from VRAM32
                            vram32_addr <= 11'd1024 + {3'd0, vram8_q};
                        end

                        4'd4: begin
                            // Latch palette word from VRAM32
                            win_palette_word <= vram32_q;
                        end

                        4'd5: begin
                            // Pattern bits and palette color are available combinationally.
                            // Register transparency and window RGB24.
                            win_transparent <= (pattern_bits == 2'b00) && (win_palette_word[31:24] == 8'd0);
                            win_rgb24 <= win_color_rgb24;
                        end

                        4'd6: begin
                            // Latch SRAM pixel data (now settled) and present to pixel palette
                            palette_idx <= sram_data;
                        end

                        4'd7: begin
                            // Palette lookup latency cycle — palette_rgb becomes valid
                        end

                        4'd8: begin
                            // Composited pixel_hi/pixel_lo now valid via final_rgb24 mux
                            state <= S_PIXEL_HI;
                        end
                    endcase

                    if (pal_wait != 4'd8)
                        pal_wait <= pal_wait + 4'd1;
                end

                S_PIXEL_HI: begin
                    // Composited RGB is now valid, send high byte of RGB565
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

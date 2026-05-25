/*
 * FrameScanEngine
 * Scans the 320×240 pixel framebuffer and streams pixels over SPI
 * with window tile layer compositing.
 *
 * Continuously streams pixel data after init sends RAMWR.
 * The ILI9341 address counter auto-wraps at the window boundary
 * (set once during init), so no per-frame CASET/PASET/RAMWR is needed.
 *
 * Pixel pipeline (all memories are synchronous BRAMs, 1-cycle latency):
 *
 *   S_PIXEL_READ:  Present VRAMPX addr + VRAM8 tile addr
 *   pal_wait=0:    Latch pixel index + tile index; present pattern + color addrs
 *   pal_wait=1:    Latch pattern + color index; present palette addr; start pixel palette
 *   pal_wait=2:    Latch tile palette word; pixel palette RGB valid
 *   pal_wait=3:    Extract pattern bits, compute transparency + composite → S_PIXEL_HI
 *
 * Pattern/palette extraction matches BGWrenderer.v (proven HDMI implementation).
 *
 * Clocked at 100 MHz.
 */
module FrameScanEngine (
    input  wire        clk,        // 100 MHz
    input  wire        reset,
    input  wire        enable,     // Start scanning (asserted after init)

    // SRAM read interface (pixel framebuffer — actually BRAM on Cyclone 10)
    output reg  [16:0] sram_addr,
    input  wire [7:0]  sram_data,  // Pixel data (valid 1 cycle after sram_addr)
    input  wire        sram_data_valid, // (unused — BRAM always valid after 1 clk)
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

    // ---- Window tile layer state (registered) ----
    reg [15:0] win_pattern_half  = 16'd0;  // 16-bit pattern (1 row of 8 pixels × 2 bits)
    reg [31:0] win_palette_word  = 32'd0;  // 4-color palette (4 × 8-bit RRRGGGBB)
    reg        win_transparent   = 1'b1;   // Window pixel is transparent
    reg [23:0] win_rgb24         = 24'd0;  // Window pixel RGB24 (from tile palette)

    // ---- Combinational pattern bit extraction (BGWrenderer style) ----
    // Pattern layout: pixel 0 at [15:14], pixel 7 at [1:0]
    reg [1:0] pattern_bits;
    always @(*) begin
        case (col_in_tile)
            3'd0: pattern_bits = win_pattern_half[15:14];
            3'd1: pattern_bits = win_pattern_half[13:12];
            3'd2: pattern_bits = win_pattern_half[11:10];
            3'd3: pattern_bits = win_pattern_half[9:8];
            3'd4: pattern_bits = win_pattern_half[7:6];
            3'd5: pattern_bits = win_pattern_half[5:4];
            3'd6: pattern_bits = win_pattern_half[3:2];
            3'd7: pattern_bits = win_pattern_half[1:0];
        endcase
    end

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
    localparam
        S_IDLE         = 3'd0,
        S_PIXEL_READ   = 3'd1,  // Present VRAMPX + VRAM8 tile addresses
        S_PIXEL_PAL    = 3'd2,  // 6-cycle tile fetch + palette pipeline
        S_PIXEL_HI     = 3'd3,  // Send RGB565 high byte
        S_PIXEL_LO     = 3'd4,  // Send RGB565 low byte
        S_WAIT_SPI     = 3'd5;  // Wait for SPI byte to complete

    reg [2:0] state = S_IDLE;
    reg [2:0] return_state = S_IDLE; // Where to go after SPI byte done
    reg       spi_accepted = 1'b0;  // Tracks that tx_ready went low
    reg [2:0] pal_wait = 3'd0;      // Pipeline cycle counter

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
                    spi_cs_n <= 1'b0;
                    spi_dc <= 1'b1; // All data from here on
                    if (enable) begin
                        pixel_x <= 9'd0;
                        pixel_y <= 8'd0;
                        state <= S_PIXEL_READ;
                    end
                end

                // ---- Pixel pipeline stage 0: present addresses ----
                S_PIXEL_READ: begin
                    // Present VRAMPX address: y * 320 + x = (y << 8) + (y << 6) + x
                    sram_addr <= ({9'd0, pixel_y} << 8)
                               + ({9'd0, pixel_y} << 6)
                               + {8'd0, pixel_x};
                    sram_read <= 1'b1;

                    // Simultaneously present VRAM8 address for window tile index
                    vram8_addr <= 14'd4096 + {3'd0, win_tile_linear};

                    pal_wait <= 3'd0;
                    state <= S_PIXEL_PAL;
                end

                // ---- Pixel pipeline: 6-cycle tile fetch + palette lookup ----
                //
                // BRAMs have 2-cycle read latency (address via NBA at cycle T,
                // data valid at cycle T+2). Each read needs a settle cycle.
                //
                // Cycle 0: BRAM settle — VRAMPX + VRAM8 tile addr registered
                // Cycle 1: sram_data + vram8_q valid — latch pixel + tile index
                //          Present VRAM32 pattern addr + VRAM8 color addr
                // Cycle 2: BRAM settle — VRAM32 + VRAM8 registered
                // Cycle 3: vram32_q + vram8_q valid — latch pattern + color
                //          Present VRAM32 palette addr
                //          palette_rgb also valid (from palette_idx at cycle 1)
                // Cycle 4: BRAM settle — VRAM32 palette registered
                // Cycle 5: vram32_q valid (palette) — latch + composite
                //          → S_PIXEL_HI
                S_PIXEL_PAL: begin
                    sram_read <= 1'b1; // Hold SRAM read active

                    case (pal_wait)
                        3'd0: begin
                            // BRAM settle cycle — data not yet valid
                        end

                        3'd1: begin
                            // VRAMPX data valid — start pixel palette lookup
                            palette_idx <= sram_data;

                            // VRAM8 data valid — tile index
                            // Present VRAM32 pattern address: tile_index * 4 + line_pair
                            vram32_addr <= ({3'd0, vram8_q} << 2) + {8'd0, line_in_tile[2:1]};

                            // Present VRAM8 color index address
                            vram8_addr <= 14'd6144 + {3'd0, win_tile_linear};
                        end

                        3'd2: begin
                            // BRAM settle cycle — data not yet valid
                        end

                        3'd3: begin
                            // VRAM32 data valid — latch pattern half
                            if (line_in_tile[0])
                                win_pattern_half <= vram32_q[15:0];   // Odd line
                            else
                                win_pattern_half <= vram32_q[31:16];  // Even line

                            // VRAM8 data valid — color index
                            // Present VRAM32 palette address: 1024 + color_index
                            vram32_addr <= 11'd1024 + {3'd0, vram8_q};
                        end

                        3'd4: begin
                            // BRAM settle cycle — data not yet valid
                            // palette_rgb is also settling (palette_idx set at cycle 1)
                        end

                        3'd5: begin
                            // VRAM32 data valid — latch palette word
                            win_palette_word <= vram32_q;
                        end

                        3'd6: begin
                            // win_palette_word now valid (NBA from cycle 5 applied)
                            // pattern_bits valid (win_pattern_half set at cycle 3)
                            // palette_rgb valid (palette_idx set at cycle 1)
                            // win_color_byte / win_color_rgb24 valid (combinational from above)
                            win_transparent <= (pattern_bits == 2'b00) && (win_palette_word[31:24] == 8'd0);
                            win_rgb24 <= win_color_rgb24;

                            state <= S_PIXEL_HI;
                        end
                    endcase

                    if (pal_wait != 3'd6)
                        pal_wait <= pal_wait + 3'd1;
                end

                // ---- Send RGB565 high byte ----
                S_PIXEL_HI: begin
                    if (spi_tx_ready) begin
                        spi_tx_data <= pixel_hi;
                        spi_dc <= 1'b1;
                        spi_tx_valid <= 1'b1;
                        return_state <= S_PIXEL_LO;
                        spi_accepted <= 1'b0;
                        state <= S_WAIT_SPI;
                    end
                end

                // ---- Send RGB565 low byte + advance pixel ----
                S_PIXEL_LO: begin
                    if (spi_tx_ready) begin
                        spi_tx_data <= pixel_lo;
                        spi_dc <= 1'b1;
                        spi_tx_valid <= 1'b1;
                        spi_accepted <= 1'b0;

                        // Advance to next pixel
                        if (pixel_x == 9'd319) begin
                            pixel_x <= 9'd0;
                            if (pixel_y == 8'd239) begin
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

/*
 * FSX
 * Frame Synthesizer using internal BRAM for pixel framebuffer
 * with SPI display output (ILI9341 320×240 TFT)
 *
 * Used on Cyclone 10 for the FPGC-Camera build.
 * All logic runs in the 100 MHz system clock domain.
 */
module FSX (
    input wire          clk,          // 100 MHz system clock

    // SPI display output
    output wire         spi_clk,
    output wire         spi_mosi,
    output wire         spi_cs_n,
    output wire         spi_dc,
    output wire         lcd_rst_n,
    output wire         lcd_backlight,

    // VRAMpixel read port (BRAM GPU side)
    output wire [16:0]  vramPX_addr,
    input wire  [7:0]   vramPX_q,

    // Palette CPU write port
    input wire          palette_we,
    input wire  [7:0]   palette_addr,
    input wire  [23:0]  palette_wdata,

    // Status
    output wire         frame_drawn,

    // Active reset
    input wire          reset
);

    SPIDisplayController spi_display (
        .clk            (clk),
        .reset          (reset),

        // SPI pins
        .spi_clk        (spi_clk),
        .spi_mosi       (spi_mosi),
        .spi_cs_n       (spi_cs_n),
        .spi_dc         (spi_dc),
        .lcd_rst_n      (lcd_rst_n),
        .lcd_backlight  (lcd_backlight),

        // VRAM read interface
        .pixel_sram_addr (vramPX_addr),
        .pixel_sram_data (vramPX_q),
        .pixel_reading   (),

        // Palette CPU write port
        .palette_we     (palette_we),
        .palette_addr   (palette_addr),
        .palette_wdata  (palette_wdata),

        // Status
        .frame_drawn    (frame_drawn),
        .busy           ()
    );

endmodule

/*
 * SPIDisplayController
 * Top-level display pipeline for ILI9341 320×240 SPI TFT
 *
 * Replaces the old FSX_SRAM (HDMI) display module. All logic runs
 * in the 100 MHz system clock domain — no clock domain crossings.
 *
 * Pipeline:
 *   ILI9341_Init (one-shot) → FrameScanEngine → PixelPalette → RGB565 → SPIMaster
 *
 * The FrameScanEngine reads 8-bit indexed pixels from external SRAM,
 * looks them up in the 256-entry PixelPalette to get 24-bit RGB,
 * converts to 16-bit RGB565, and streams the result over SPI.
 */
module SPIDisplayController (
    input  wire        clk,        // 100 MHz system clock
    input  wire        reset,

    // SPI display output pins
    output wire        spi_clk,
    output wire        spi_mosi,
    output wire        spi_cs_n,
    output wire        spi_dc,
    output wire        lcd_rst_n,
    output wire        lcd_backlight,

    // Pixel SRAM read interface (directly to SRAMArbiter/VRAMPXSram)
    output wire [16:0] pixel_sram_addr,
    input  wire [7:0]  pixel_sram_data,
    input  wire        pixel_sram_data_valid,
    output wire        pixel_reading,

    // VRAM8 read interface (tile/color maps for window layer)
    output wire [13:0] vram8_addr,
    input  wire [7:0]  vram8_q,

    // VRAM32 read interface (patterns/palettes for window layer)
    output wire [10:0] vram32_addr,
    input  wire [31:0] vram32_q,

    // Palette CPU write port (directly from MemoryUnit)
    input  wire        palette_we,
    input  wire [7:0]  palette_addr,
    input  wire [23:0] palette_wdata,

    // Status
    output wire        frame_drawn,
    output wire        busy
);

    // Backlight always on
    assign lcd_backlight = 1'b1;

    // ---- Internal signals ----

    // Init module outputs
    wire        init_done;
    wire [7:0]  init_spi_tx_data;
    wire        init_spi_tx_valid;
    wire        init_spi_dc;
    wire        init_spi_cs_n;
    wire        init_lcd_rst_n;

    // Frame scan engine outputs
    wire [7:0]  scan_spi_tx_data;
    wire        scan_spi_tx_valid;
    wire        scan_spi_dc;
    wire        scan_spi_cs_n;
    wire        scan_frame_done;

    // SRAM interface from scan engine
    wire [16:0] scan_sram_addr;
    wire        scan_sram_read;

    // VRAM interface from scan engine
    wire [13:0] scan_vram8_addr;
    wire [10:0] scan_vram32_addr;

    // Palette interface from scan engine
    wire [7:0]  scan_palette_idx;
    wire [23:0] palette_rgb;

    // SPI master signals
    wire        spi_tx_ready;

    // ---- Mux: init vs scan engine control of SPI ----
    wire [7:0]  mux_spi_tx_data  = init_done ? scan_spi_tx_data  : init_spi_tx_data;
    wire        mux_spi_tx_valid = init_done ? scan_spi_tx_valid : init_spi_tx_valid;
    wire        mux_spi_dc       = init_done ? scan_spi_dc       : init_spi_dc;
    wire        mux_spi_cs_n     = init_done ? scan_spi_cs_n     : init_spi_cs_n;

    // CS and RST outputs
    assign spi_cs_n = mux_spi_cs_n;
    assign lcd_rst_n = init_lcd_rst_n;

    // SRAM interface
    assign pixel_sram_addr = scan_sram_addr;
    assign pixel_reading = scan_sram_read;

    // VRAM interfaces (pass through to scan engine)
    assign vram8_addr = scan_vram8_addr;
    assign vram32_addr = scan_vram32_addr;

    // Status
    assign frame_drawn = scan_frame_done;
    assign busy = !init_done || scan_sram_read;

    // ---- SPIMaster ----
    SPIMaster spi_master (
        .clk(clk),
        .reset(reset),
        .tx_data(mux_spi_tx_data),
        .tx_valid(mux_spi_tx_valid),
        .tx_ready(spi_tx_ready),
        .dc_value(mux_spi_dc),
        .spi_clk(spi_clk),
        .spi_mosi(spi_mosi),
        .spi_dc(spi_dc)
    );

    // ---- ILI9341 Init Sequencer ----
    ILI9341_Init init_seq (
        .clk(clk),
        .reset(reset),
        .spi_tx_data(init_spi_tx_data),
        .spi_tx_valid(init_spi_tx_valid),
        .spi_tx_ready(spi_tx_ready),
        .spi_dc(init_spi_dc),
        .spi_cs_n(init_spi_cs_n),
        .lcd_rst_n(init_lcd_rst_n),
        .init_done(init_done)
    );

    // ---- PixelPalette (reused from existing design) ----
    // GPU read port now uses clk (100 MHz) instead of clk_pixel (25 MHz)
    PixelPalette palette (
        .clk_pixel(clk),          // Read port at 100 MHz
        .gpu_index(scan_palette_idx),
        .gpu_rgb24(palette_rgb),
        .clk_sys(clk),            // Write port at 100 MHz (same clock)
        .cpu_we(palette_we),
        .cpu_addr(palette_addr),
        .cpu_wdata(palette_wdata)
    );

    // ---- Frame Scan Engine ----
    FrameScanEngine scan_engine (
        .clk(clk),
        .reset(reset),
        .enable(init_done),
        .sram_addr(scan_sram_addr),
        .sram_data(pixel_sram_data),
        .sram_data_valid(pixel_sram_data_valid),
        .sram_read(scan_sram_read),
        .vram8_addr(scan_vram8_addr),
        .vram8_q(vram8_q),
        .vram32_addr(scan_vram32_addr),
        .vram32_q(vram32_q),
        .palette_idx(scan_palette_idx),
        .palette_rgb(palette_rgb),
        .spi_tx_data(scan_spi_tx_data),
        .spi_tx_valid(scan_spi_tx_valid),
        .spi_tx_ready(spi_tx_ready),
        .spi_dc(scan_spi_dc),
        .spi_cs_n(scan_spi_cs_n),
        .frame_done(scan_frame_done)
    );

endmodule

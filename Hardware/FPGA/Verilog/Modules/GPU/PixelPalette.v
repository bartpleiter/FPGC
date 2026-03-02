/*
 * PixelPalette
 * 256-entry programmable color palette (8-bit index → 24-bit RGB)
 *
 * Dual-port BRAM:
 *   - GPU read port (25 MHz pixel clock, registered output)
 *   - CPU write port (100 MHz system clock)
 *
 * Initialized with the default RRRGGGBB bit-replication mapping
 * for backward compatibility. The CPU can overwrite entries at runtime
 * to define custom palettes.
 *
 * Resource cost: 1 M9K block (of 126 available on EP4CE40).
 */
module PixelPalette (
    // GPU read port (25 MHz pixel clock)
    input  wire        clk_pixel,
    input  wire [7:0]  gpu_index,      // 8-bit pixel value (palette index)
    output reg  [23:0] gpu_rgb24,      // 24-bit color output (registered)

    // CPU write port (100 MHz system clock)
    input  wire        clk_sys,
    input  wire        cpu_we,         // Write enable
    input  wire [7:0]  cpu_addr,       // Palette entry index (0–255)
    input  wire [23:0] cpu_wdata       // 24-bit color to write
);

    // 256 × 24-bit dual-port BRAM
    reg [23:0] palette [0:255];

    // Initialize with default RRRGGGBB bit-replication (backward compatible)
    integer i;
    initial begin
        for (i = 0; i < 256; i = i + 1) begin
            palette[i] = {
                i[7:5], i[7:5], i[7:6],    // R: 3→8 bit replication
                i[4:2], i[4:2], i[4:3],    // G: 3→8 bit replication
                i[1:0], i[1:0], i[1:0], i[1:0]  // B: 2→8 bit replication
            };
        end
    end

    // GPU read (registered for timing — adds 1 pixel clock latency)
    always @(posedge clk_pixel)
        gpu_rgb24 <= palette[gpu_index];

    // CPU write
    always @(posedge clk_sys)
        if (cpu_we)
            palette[cpu_addr] <= cpu_wdata;

endmodule

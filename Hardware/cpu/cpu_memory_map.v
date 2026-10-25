// Memory map
// Tells which region an address is in, as one-hot bits.
module cpu_memory_map (
    input  wire [31:0] addr,

    // Regions, one-hot. All 0 for an unmapped address.
    output wire        sdram,
    output wire        io,        // Memory Unit
    output wire        rom,
    output wire        vram32,
    output wire        vram8,
    output wire        vrampx,
    output wire        palette,
    output wire        cpu_io,    // CPU-internal I/O registers

    output wire        code,      // Region can hold code: SDRAM or ROM
    output wire        unmapped
);

localparam                              SDRAM_SIZE   = 32'h04000000;  // from address 0
localparam IO_BASE      = 32'h1C000000, IO_SIZE      = 32'h02000000;
localparam ROM_BASE     = 32'h1E000000, ROM_SIZE     = 32'h00001000;
localparam VRAM32_BASE  = 32'h1E400000, VRAM32_SIZE  = 32'h00001080;
localparam VRAM8_BASE   = 32'h1E800000, VRAM8_SIZE   = 32'h00008008;
localparam VRAMPX_BASE  = 32'h1EC00000, VRAMPX_SIZE  = 32'h00020000;
localparam PALETTE_BASE = 32'h1EC80000, PALETTE_SIZE = 32'h00000400;
localparam CPU_IO_BASE  = 32'h1F000000, CPU_IO_SIZE  = 32'h00000010;

assign sdram   = addr < SDRAM_SIZE;
assign io      = addr >= IO_BASE      && addr < IO_BASE      + IO_SIZE;
assign rom     = addr >= ROM_BASE     && addr < ROM_BASE     + ROM_SIZE;
assign vram32  = addr >= VRAM32_BASE  && addr < VRAM32_BASE  + VRAM32_SIZE;
assign vram8   = addr >= VRAM8_BASE   && addr < VRAM8_BASE   + VRAM8_SIZE;
assign vrampx  = addr >= VRAMPX_BASE  && addr < VRAMPX_BASE  + VRAMPX_SIZE;
assign palette = addr >= PALETTE_BASE && addr < PALETTE_BASE + PALETTE_SIZE;
assign cpu_io  = addr >= CPU_IO_BASE  && addr < CPU_IO_BASE  + CPU_IO_SIZE;

assign code     = sdram || rom;
assign unmapped = !(sdram || io || rom || vram32 || vram8 || vrampx || palette || cpu_io);

endmodule

// Memory map
// Tells which region an address is in as one-hot bits.
module cpu_memory_map (
    input  wire [31:0] addr,

    // Regions. All 0 for an unmapped address.
    output wire        sdram,
    output wire        io,        // Memory Unit
    output wire        rom,
    output wire        cpu_io,    // CPU-internal I/O registers

    output wire        code,      // Region can hold code: SDRAM or ROM
    output wire        unmapped
);

localparam                              SDRAM_SIZE   = 32'h04000000;
localparam IO_BASE      = 32'h1C000000, IO_SIZE      = 32'h02000000;
localparam ROM_BASE     = 32'h1E000000, ROM_SIZE     = 32'h00001000;
localparam CPU_IO_BASE  = 32'h1F000000, CPU_IO_SIZE  = 32'h00000014;

assign sdram   = addr < SDRAM_SIZE;
assign io      = addr >= IO_BASE      && addr < IO_BASE      + IO_SIZE;
assign rom     = addr >= ROM_BASE     && addr < ROM_BASE     + ROM_SIZE;
assign cpu_io  = addr >= CPU_IO_BASE  && addr < CPU_IO_BASE  + CPU_IO_SIZE;

assign code     = sdram || rom;
assign unmapped = !(sdram || io || rom || cpu_io);

endmodule

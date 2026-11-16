// Memory map
// Tells which region an address is in.
module cpu_memory_map (
    input  wire [31:0] addr,

    // Regions. All 0 for an unmapped address.
    output wire        sdram,
    output wire        rom,
    output wire        bus,
    output wire        cpu_io,

    output wire        code,      // Region can hold code
    output wire        unmapped
);

// SDRAM:     0x00000000, via L1d
// ROM:       0x04000000, via the bus for data reads
// Memories:  0x04000000 to 0x07FFFFFF
// Devices:   0xFFFF8000 to 0xFFFFFEFF
// CPU I/O:   0xFFFFFF00, via the CPU's internal registers
localparam CPU_IO_SIZE = 8'h14;

wire top = addr[31:15] == 17'h1FFFF;

assign sdram   = addr[31:26] == 6'b000000;
assign rom     = addr[31:12] == 20'h04000;
assign bus     = addr[31:26] == 6'b000001 || (top && addr[14:8] != 7'h7F);
assign cpu_io  = top && addr[14:8] == 7'h7F && addr[7:0] < CPU_IO_SIZE;

assign code     = sdram || rom;
assign unmapped = !(sdram || bus || cpu_io);

endmodule

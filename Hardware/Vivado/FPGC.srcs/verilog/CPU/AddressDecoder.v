/*
 * AddressDecoder
 * Given the address of a memory location, determines which memory type is accessed
 * It also determines if the memory access is multi-cycle, and return the
 * address of the memory location
 * Basically it acts as the memory map for data memory
 */
module AddressDecoder(
    input wire [31:0]   areg_value,
    input wire [31:0]   const16,

    output wire         mem_sdram,
    output wire         mem_sdcard,
    output wire         mem_spiflash,
    output wire         mem_io,
    output wire         mem_rom,
    output wire         mem_vram32,
    output wire         mem_vram8,
    output wire         mem_vrampx,

    output wire         mem_multicycle,

    output wire [31:0]  mem_local_address
);

wire [31:0] mem_address = areg_value + const16;

assign mem_multicycle = mem_address < 32'h7800000;

assign mem_sdram    = (mem_address >= 32'h0000000) && (mem_address < 32'h4000000);
assign mem_sdcard   = (mem_address >= 32'h4000000) && (mem_address < 32'h6000000);
assign mem_spiflash = (mem_address >= 32'h6000000) && (mem_address < 32'h7000000);
assign mem_io       = (mem_address >= 32'h7000000) && (mem_address < 32'h7800000);

assign mem_rom      = (mem_address >= 32'h7800000) && (mem_address < 32'h7900000);
assign mem_vram32   = (mem_address >= 32'h7900000) && (mem_address < 32'h7A00000);
assign mem_vram8    = (mem_address >= 32'h7A00000) && (mem_address < 32'h7B00000);
assign mem_vrampx   = (mem_address >= 32'h7B00000) && (mem_address < 32'h7C00000);

endmodule

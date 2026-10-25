// Testbench of cpu_memory_map. See tb_unit.vh for how the vectors are applied.
`timescale 1ns / 1ps
module tb_cpu_memory_map;

`include "tb_unit.vh"

reg  [31:0] addr;
wire        sdram;
wire        io;
wire        rom;
wire        vram32;
wire        vram8;
wire        vrampx;
wire        palette;
wire        cpu_io;
wire        code;
wire        unmapped;

cpu_memory_map dut (
    .addr(addr),
    .sdram(sdram),
    .io(io),
    .rom(rom),
    .vram32(vram32),
    .vram8(vram8),
    .vrampx(vrampx),
    .palette(palette),
    .cpu_io(cpu_io),
    .code(code),
    .unmapped(unmapped)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h\n", label, addr) == 2)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h %h %h %h %h %h %h %h\n",
                               sdram, io, rom, vram32, vram8, vrampx, palette, cpu_io,
                               code, unmapped);
    tb_close;
end

endmodule

// Testbench of cpu_memory_map. See tb_unit.vh for how the vectors are applied.
`timescale 1ns / 1ps
module tb_cpu_memory_map;

`include "tb_unit.vh"

reg  [31:0] addr;
wire        sdram;
wire        rom;
wire        bus;
wire        cpu_io;
wire        code;
wire        unmapped;

cpu_memory_map dut (
    .addr(addr),
    .sdram(sdram),
    .rom(rom),
    .bus(bus),
    .cpu_io(cpu_io),
    .code(code),
    .unmapped(unmapped)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h\n", label, addr) == 2)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h %h %h %h\n",
                               sdram, rom, bus, cpu_io, code, unmapped);
    tb_close;
end

endmodule

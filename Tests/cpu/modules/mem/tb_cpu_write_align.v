// Testbench of cpu_write_align. See tb_unit.vh for how the vectors are applied.
`timescale 1ns / 1ps
module tb_cpu_write_align;

`include "tb_unit.vh"

reg  [31:0] data;
reg  [1:0]  addr;
reg         mem_byte;
reg         mem_half;
wire [31:0] write_data;
wire [3:0]  byte_enable;

cpu_write_align dut (
    .data(data),
    .addr(addr),
    .mem_byte(mem_byte),
    .mem_half(mem_half),
    .write_data(write_data),
    .byte_enable(byte_enable)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h\n", label, data, addr, mem_byte, mem_half) == 5)
        @(negedge clk) $fwrite(outputs_fd, "%h %h\n", write_data, byte_enable);
    tb_close;
end

endmodule

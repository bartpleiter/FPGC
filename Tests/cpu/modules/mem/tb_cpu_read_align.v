// Testbench of cpu_read_align. See tb_unit.vh for how the vectors are applied.
`timescale 1ns / 1ps
module tb_cpu_read_align;

`include "tb_unit.vh"

reg  [31:0] word;
reg  [1:0]  addr;
reg         mem_byte;
reg         mem_half;
reg         sign_extend;
wire [31:0] value;

cpu_read_align dut (
    .word(word),
    .addr(addr),
    .mem_byte(mem_byte),
    .mem_half(mem_half),
    .sign_extend(sign_extend),
    .value(value)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h\n",
                   label, word, addr, mem_byte, mem_half, sign_extend) == 6)
        @(negedge clk) $fwrite(outputs_fd, "%h\n", value);
    tb_close;
end

endmodule

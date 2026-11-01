// Testbench of cpu_l1d. See tb_unit.vh for how the vectors are applied.
//
// One cycle in which both ports read address 0 comes before the vectors, so the outputs
// start at the empty line 0 instead of X. The test sets the number of lines with the
// parameter.
`timescale 1ns / 1ps
module tb_cpu_l1d #(
    parameter LINES = 128
);

`include "tb_unit.vh"

localparam INDEX_BITS = $clog2(LINES);

reg  [INDEX_BITS+4:2]  a_addr;
wire [31:0]            a_word;
wire [22-INDEX_BITS:0] a_entry;
reg                    a_write;
reg  [3:0]             a_byte_enable;
reg  [31:0]            a_write_data;
reg                    a_entry_write;
reg  [22-INDEX_BITS:0] a_entry_write_value;
reg  [INDEX_BITS+4:2]  b_addr;
wire [31:0]            b_word;
wire [22-INDEX_BITS:0] b_entry;
reg                    b_write;
reg  [31:0]            b_write_data;
reg                    b_entry_write;
reg  [22-INDEX_BITS:0] b_entry_write_value;

cpu_l1d #(
    .LINES(LINES)
) dut (
    .clk(clk),
    .a_addr(a_addr),
    .a_word(a_word),
    .a_entry(a_entry),
    .a_write(a_write),
    .a_byte_enable(a_byte_enable),
    .a_write_data(a_write_data),
    .a_entry_write(a_entry_write),
    .a_entry_write_value(a_entry_write_value),
    .b_addr(b_addr),
    .b_word(b_word),
    .b_entry(b_entry),
    .b_write(b_write),
    .b_write_data(b_write_data),
    .b_entry_write(b_entry_write),
    .b_entry_write_value(b_entry_write_value)
);

initial begin
    tb_open;
    {a_addr, a_write, a_byte_enable, a_write_data, a_entry_write, a_entry_write_value} = 0;
    {b_addr, b_write, b_write_data, b_entry_write, b_entry_write_value} = 0;
    @(negedge clk);
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h %h %h %h %h %h\n", label,
                   a_addr, a_write, a_byte_enable, a_write_data, a_entry_write,
                   a_entry_write_value, b_addr, b_write, b_write_data, b_entry_write,
                   b_entry_write_value) == 12) begin
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h %h\n", a_word, a_entry, b_word, b_entry);
    end
    tb_close;
end

endmodule

// Testbench of cpu_l1i. See tb_unit.vh for how the vectors are applied.
//
// One cycle that reads address 0 comes before the vectors, so the outputs start at the
// empty line 0 instead of X. The test sets the number of lines with the parameter.
`timescale 1ns / 1ps
module tb_cpu_l1i #(
    parameter LINES = 128
);

`include "tb_unit.vh"

localparam INDEX_BITS = $clog2(LINES);

reg  [INDEX_BITS+4:2]  a_addr;
reg                    a_enable;
wire [31:0]            a_word;
wire [21-INDEX_BITS:0] a_entry;
reg  [INDEX_BITS+4:2]  b_addr;
reg                    b_write;
reg  [31:0]            b_write_data;
reg                    b_entry_write;
reg  [21-INDEX_BITS:0] b_entry_write_value;

cpu_l1i #(
    .LINES(LINES)
) dut (
    .clk(clk),
    .a_addr(a_addr),
    .a_enable(a_enable),
    .a_word(a_word),
    .a_entry(a_entry),
    .b_addr(b_addr),
    .b_write(b_write),
    .b_write_data(b_write_data),
    .b_entry_write(b_entry_write),
    .b_entry_write_value(b_entry_write_value)
);

initial begin
    tb_open;
    a_addr        = 0;
    a_enable      = 1'b1;
    b_addr        = 0;
    b_write       = 1'b0;
    b_write_data  = 32'd0;
    b_entry_write = 1'b0;
    b_entry_write_value = 0;
    @(negedge clk);
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h %h\n", label, a_addr, a_enable,
                   b_addr, b_write, b_write_data, b_entry_write,
                   b_entry_write_value) == 8) begin
        @(negedge clk) $fwrite(outputs_fd, "%h %h\n", a_word, a_entry);
    end
    tb_close;
end

endmodule

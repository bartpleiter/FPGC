// Testbench of cpu_alu. See tb_unit.vh for how the vectors are applied.
`timescale 1ns / 1ps
module tb_cpu_alu;

`include "tb_unit.vh"

reg  [3:0]  op;
reg  [31:0] a;
reg  [31:0] b;
wire [31:0] y;

cpu_alu dut (
    .op(op),
    .a(a),
    .b(b),
    .y(y)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h\n", label, op, a, b) == 4)
        @(negedge clk) $fwrite(outputs_fd, "%h\n", y);
    tb_close;
end

endmodule
